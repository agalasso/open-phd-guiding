/*
 *  cam_qguide.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2007-2010 Craig Stark.
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

#if defined (QGUIDE)

#include "camera.h"
#include "time.h"
#include "image_math.h"

#include <wx/stdpaths.h>
#include <wx/textfile.h>
//wxTextFile *qglogfile;

int ushort_compare (const void * a, const void * b) {
  if ( *(unsigned short *)a > *(unsigned short *)b ) return 1;
  if ( *(unsigned short *)a < *(unsigned short *)b ) return -1;
  return 0;
}

#define QGDEBUG 0
#include "cam_qguide.h"
// QHY CMOS guide camera version
// Tom's driver

CameraQGuider::CameraQGuider()
{
    Connected = false;
    Name = _T("Q-Guider");
    FullSize = wxSize(1280,1024);
    m_hasGuideOutput = true;
    HasGainControl = true;
}

wxByte CameraQGuider::BitsPerPixel()
{
    return 16;
}

bool CameraQGuider::Connect(const wxString& camId)
{
// returns true on error
//  CameraReset();
    if (!openUSB(0))
        return CamConnectFailed(_("No camera"));

//  ClearGuidePort();
//  GuideCommand(0x0F,10);
//  buffer = new unsigned char[1311744];
    SETBUFFERMODE(0);
    Connected = true;
//  qglogfile = new wxTextFile(Debug.GetLogDir() + PATHSEPSTR + _T("PHD_QGuide_log.txt"));
    //qglogfile->AddLine(wxNow() + ": QGuide connected"); //qglogfile->Write();
    return false;
}

bool CameraQGuider::ST4PulseGuideScope(int direction, int duration)
{
    int reg = 0;
    int dur = duration / 10;

    //qglogfile->AddLine(wxString::Format("Sending guide dur %d",dur)); //qglogfile->Write();
    if (dur >= 255) dur = 254; // Max guide pulse is 2.54s -- 255 keeps it on always
    // Output pins are NC, Com, RA+(W), Dec+(N), Dec-(S), RA-(E) ??  http://www.starlight-xpress.co.uk/faq.htm
    switch (direction) {
        case WEST: reg = 0x80; break;   // 0111 0000
        case NORTH: reg = 0x40; break;  // 1011 0000
        case SOUTH: reg = 0x20; break;  // 1101 0000
        case EAST: reg = 0x10;  break;  // 1110 0000
        default: return true; // bad direction passed in
    }
    GuideCommand(reg,dur);
    //if (duration > 50) wxMilliSleep(duration - 50);  // wait until it's mostly done
    WorkerThread::MilliSleep(duration + 10);
    //qglogfile->AddLine("Done"); //qglogfile->Write();
    return false;
}

void CameraQGuider::ClearGuidePort()
{
//  SendGuideCommand(DevName,0,0);
}

void CameraQGuider::InitCapture()
{
//  CameraReset();
    ProgramCamera(0,0,1280,1024, (GuideCameraGain * 63 / 100) );
//  SetNoiseReduction(0);
}

bool CameraQGuider::Disconnect()
{
    closeUSB();
//  delete [] buffer;
    Connected = false;
    //qglogfile->AddLine(wxNow() + ": Disconnecting"); //qglogfile->Write(); //qglogfile->Close();
    return false;
}

static bool StopExposure()
{
    Debug.AddLine("QGuide: stop exposure");
    CancelExposure();
    return true;
}

bool CameraQGuider::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
// Only does full frames still

    unsigned short *dptr;
    bool firstimg = true;

    //qglogfile->AddLine(wxString::Format("Capturing dur %d",duration)); //qglogfile->Write();
//  ThreadedExposure(10, buffer);
    ProgramCamera(0,0,1280,1024, (GuideCameraGain * 63 / 100) );

/*  ThreadedExposure(10, NULL);
    while (isExposing())
        wxMilliSleep(10);
*/
    if (img.Init(FullSize)) {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }

    //  ThreadedExposure(duration, buffer);
    ThreadedExposure(duration, NULL);
    //qglogfile->AddLine("Exposure programmed"); //qglogfile->Write();

    CameraWatchdog watchdog(duration, GetTimeoutMs() + 1000); // typically 6 second timeout

    if (duration > 100)
    {
        // Shift to > duration
        if (WorkerThread::MilliSleep(duration + 100) &&
            (WorkerThread::TerminateRequested() || StopExposure()))
        {
            return true;
        }
    }

    while (isExposing())
    {
        wxMilliSleep(200);
        if (WorkerThread::InterruptRequested() &&
            (WorkerThread::TerminateRequested() || StopExposure()))
        {
            return true;
        }
        if (watchdog.Expired())
        {
            DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
            return true;
        }
    }

    //qglogfile->AddLine("Exposure done"); //qglogfile->Write();

/*  dptr = img.ImageData;
    for (unsigned int i=0; i<img.NPixels; i++,dptr++) {
        *dptr = 0;
    }
*/
    dptr = img.ImageData;
    GETBUFFER(dptr, img.NPixels * 2);
    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);

    return false;
}

void CameraQGuider::RemoveLines(usImage& img)
{
    int i, j, val;
    unsigned short data[21];
    unsigned short *ptr1, *ptr2;
    unsigned short med[1024];
    int offset;
    double mean;
    int h = img.Size.GetHeight();
    int w = img.Size.GetWidth();
    size_t sz = sizeof(unsigned short);
    mean = 0.0;

    for (i=0; i<h; i++) {
        ptr1 = data;
        ptr2 = img.ImageData + i*w;
        for (j=0; j<21; j++, ptr1++, ptr2++)
            *ptr1 = *ptr2;
        qsort(data,21,sz,ushort_compare);
        med[i] = data[10];
        mean = mean + (double) (med[i]);
    }
    mean = mean / (double) h;
    for (i=0; i<h; i++) {
        offset = (int) mean - (int) med[i];
        ptr2 = img.ImageData + i*w;
        for (j=0; j<w; j++, ptr2++) {
            val = (int) *ptr2 + offset;
            if (val < 0) val = 0;
            else if (val > 65535) val = 65535;
            *ptr2 = (unsigned short) val;
        }

    }
}
#endif
