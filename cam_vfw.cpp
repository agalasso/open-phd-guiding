/*
 *  cam_vfw.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
 *  All rights reserved.
 *
 *  This source code is distributed under the following "BSD" license
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *    Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *    Neither the name of Craig Stark, Stark Labs nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "phd.h"

#ifdef VFW_CAMERA

#include "camera.h"
#include "time.h"
#include "image_math.h"
#include <wx/timer.h>
//#include "vcapwin.h"
#include "cam_vfw.h"

CameraVFW::CameraVFW()
{
    Connected = false;
    Name = _T("Windows VFW");
    FullSize = wxSize(640,480);  // should be overwritten
    VFW_Window = NULL; Extra_Window=NULL;
    PropertyDialogType = PROPDLG_WHEN_CONNECTED;
    HasDelayParam = false;
    HasPortNum = false;
}

wxByte CameraVFW::BitsPerPixel()
{
    return 8;
}

bool CameraVFW::Connect(const wxString& camId)
{
// returns true on error
//  bool retval;
    int ndevices, i, devicenum;
    wxSplitterWindow *dispwin;
    wxVideoCaptureWindow* capwin;

    if (!Extra_Window) {
        dispwin = new wxSplitterWindow(pFrame->guider,-1);
        Extra_Window = dispwin;
    }
    else dispwin = Extra_Window;

    if (!VFW_Window) {
        capwin = new wxVideoCaptureWindow(dispwin,WIN_VFW,wxPoint(0,0),wxSize(640,480));
        VFW_Window = capwin;
    }
    else capwin = VFW_Window;

    dispwin->Show(false);
    //capwin->Create(frame);
    ndevices = capwin->GetDeviceCount();
    if (ndevices == 0) return true;
    devicenum = 1;
    if (ndevices > 1) { // multiple found -- get one from user
        wxArrayString devnames;
        for (i=0; i<ndevices; i++)
            devnames.Add(capwin->GetDeviceName(i));
        devicenum = wxGetSingleChoiceIndex(_("Select capture device"),_("Camera choice"),devnames);
        if (devicenum == -1)
            return true;
        else devicenum = devicenum + 1;
    }
    if (capwin->DeviceConnect(devicenum-1) == false)  // try to connect
        return true;

    if (VFW_Window->HasVideoFormatDialog()) {
        VFW_Window->VideoFormatDialog();
//      int w,h,bpp;
//      FOURCC fourcc;
//      VFW_Window->GetVideoFormat( &w,&h, &bpp, &fourcc );
//      FullSize = wxSize(w,h);
    }

    int w,h,bpp;
    FOURCC fourcc;
    capwin->GetVideoFormat( &w,&h, &bpp, &fourcc );
//  capwin->SetVideoFormat(640,480,-1,-1);
    FullSize=wxSize(w,h);
    pFrame->StatusMsg(wxString::Format("%d x %d mode activated",w,h));
    Connected = true;
    return false;
}

bool CameraVFW::Disconnect() {
    if (VFW_Window->IsDeviceConnected()) {
        VFW_Window->DeviceDisconnect();
    }
    Connected = false;
    VFW_Window = NULL;
//  Extra_Window = NULL;
    return false;
}

bool CameraVFW::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    int xsize, ysize;
    int NFrames = 0;
    xsize = FullSize.GetWidth();
    ysize = FullSize.GetHeight();
    unsigned short *dptr;
    unsigned char *imgdata;
    bool still_going = true;
    wxImage cap_img;

    wxStopWatch swatch;

    //gNumFrames = 0;
    if (img.Init(FullSize)) {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }

    img.Clear();

    swatch.Start(); //wxStartTimer();
    while (still_going) {
        VFW_Window->SnapshotTowxImage();
        cap_img = VFW_Window->GetwxImage();
        imgdata = cap_img.GetData();
        dptr = img.ImageData;
        for (unsigned int i = 0; i < img.NPixels; i++, dptr++, imgdata += 3) {
            *dptr = *dptr + (unsigned short) (*imgdata + *(imgdata+1) + *(imgdata+2));
        }
        NFrames++;
        if ((swatch.Time() >= duration) && (NFrames > 2))
            still_going = false;
    }
    pFrame->StatusMsg(wxString::Format("%d frames",NFrames));
    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);
    return false;
}

void CameraVFW::ShowPropertyDialog() {
//      if (event.GetId() == ADV_BUTTON1) {
/*  if (VFW_Window->HasVideoFormatDialog()) {
        VFW_Window->VideoFormatDialog();
        int w,h,bpp;
        FOURCC fourcc;
        VFW_Window->GetVideoFormat( &w,&h, &bpp, &fourcc );
        FullSize = wxSize(w,h);
    }*/
//  else {
        if (VFW_Window->HasVideoSourceDialog()) VFW_Window->VideoSourceDialog();
//  }

}

#endif
