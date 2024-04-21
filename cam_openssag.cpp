/*
 *  cam_openssag.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2009 Craig Stark.
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

#ifdef OPENSSAG_CAMERA

#include "camera.h"
#include "image_math.h"
#include "cam_openssag.h"

#include <openssag.h>
#include <libusb.h>

using namespace OpenSSAG;

static bool s_libusb_init_done;

static bool init_libusb()
{
    if (s_libusb_init_done)
        return false;
    int ret = libusb_init(0);
    if (ret != 0)
        return true;
    s_libusb_init_done = true;
    return false;
}

static void uninit_libusb()
{
    if (s_libusb_init_done)
    {
        libusb_exit(0);
        s_libusb_init_done = false;
    }
}

CameraOpenSSAG::CameraOpenSSAG()
{
    Connected = false;
    Name = _T("StarShoot Autoguider (OpenSSAG)");
    FullSize = wxSize(1280,1024);  // Current size of a full frame
    m_hasGuideOutput = true;  // Do we have an ST4 port?
    HasGainControl = true;  // Can we adjust gain?
    PropertyDialogType = PROPDLG_WHEN_DISCONNECTED;

    ssag = new SSAG();
}

CameraOpenSSAG::~CameraOpenSSAG()
{
    uninit_libusb();
}

wxByte CameraOpenSSAG::BitsPerPixel()
{
    return 8;
}

static void GetLoaderVidPid(int *vid, int *pid)
{
    int default_loader_vid, default_loader_pid;
    SSAG::GetDefaultLoaderUSBIds(&default_loader_vid, &default_loader_pid);

    *vid = pConfig->Profile.GetInt("/camera/openssag/loader_vid", default_loader_vid);
    *pid = pConfig->Profile.GetInt("/camera/openssag/loader_pid", default_loader_pid);
}

static void SetLoaderVidPid(int vid, int pid)
{
    pConfig->Profile.SetInt("/camera/openssag/loader_vid", vid);
    pConfig->Profile.SetInt("/camera/openssag/loader_pid", pid);
}

bool CameraOpenSSAG::Connect(const wxString& camId)
{
    if (init_libusb())
    {
        return CamConnectFailed(_("Could not initialize USB library"));
    }

    struct ConnectInBg : public ConnectCameraInBg
    {
        SSAG *ssag;
        int vid, pid;
        ConnectInBg(SSAG *ssag_, int vid_, int pid_) : ssag(ssag_), vid(vid_), pid(pid_) { }
        bool Entry()
        {
            bool err = !ssag->Connect(true, vid, pid);
            return err;
        }
    };

    int vid, pid;
    GetLoaderVidPid(&vid, &pid);

    if (ConnectInBg(ssag, vid, pid).Run())
    {
        return CamConnectFailed(_("Could not connect to StarShoot Autoguider"));
    }

    Connected = true;  // Set global flag for being connected

    return false;
}

bool CameraOpenSSAG::ST4PulseGuideScope(int direction, int duration)
{
    switch (direction) {
        case WEST:
            ssag->Guide(guide_west, duration);
            break;
        case NORTH:
            ssag->Guide(guide_north, duration);
            break;
        case SOUTH:
            ssag->Guide(guide_south, duration);
            break;
        case EAST:
            ssag->Guide(guide_east, duration);
            break;
        default: return true; // bad direction passed in
    }

    wxMilliSleep(duration + 10);

    return false;
}

bool CameraOpenSSAG::Disconnect()
{
    Connected = false;
    ssag->Disconnect();
    return false;
}

bool CameraOpenSSAG::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    if (img.Init(FullSize))
    {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }

    ssag->SetGain((int)(GuideCameraGain / 24));
    struct raw_image *raw = ssag->Expose(duration);

    if (!raw)
    {
        Debug.Write("ssag Expose returned null!\n");
        return true;
    }

    for (unsigned int i = 0; i < raw->width * raw->height; i++) {
        img.ImageData[i] = (unsigned short) raw->data[i];
    }

    ssag->FreeRawImage(raw);

    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);

    return false;
}

bool CameraOpenSSAG::GetDevicePixelSize(double *devPixelSize)
{
    *devPixelSize = 5.2;
    return false;
}

class PropertiesDlg : public wxDialog
{
public:
    wxTextCtrl *m_vid;
    wxTextCtrl *m_pid;

    PropertiesDlg(wxWindow *parent);
    ~PropertiesDlg() { }
};

static int TextWidth(wxWindow *win, const wxString& s)
{
    int w, h;
    win->GetTextExtent(s, &w, &h);
    return w;
}

PropertiesDlg::PropertiesDlg(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _("SSAG Camera Settings"))
{
    int vid, pid;
    GetLoaderVidPid(&vid, &pid);

    int default_loader_vid, default_loader_pid;
    SSAG::GetDefaultLoaderUSBIds(&default_loader_vid, &default_loader_pid);

    SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer *sz0 = new wxBoxSizer(wxVERTICAL);

    wxWindow *label = new wxStaticText(this, wxID_ANY, _("Loader VID:"));
    m_vid = new wxTextCtrl(this, wxID_ANY, wxString::Format("0x%04x", vid), wxDefaultPosition, wxSize(TextWidth(this, "0x88888"), -1));
    m_vid->SetToolTip(wxString::Format(_("SSAG Loader USB Vendor ID. Default = 0x%04x"), default_loader_vid));

    wxSizer *sz1 = new wxBoxSizer(wxHORIZONTAL);
    sz1->Add(label, wxSizerFlags(0).Border(wxALL, 10));
    sz1->Add(m_vid, wxSizerFlags(0).Border(wxALL, 10));

    sz0->Add(sz1, 1, wxEXPAND, 5);

    label = new wxStaticText(this, wxID_ANY, _("Loader PID:"));
    m_pid = new wxTextCtrl(this, wxID_ANY, wxString::Format("0x%04x", pid), wxDefaultPosition, wxSize(TextWidth(this, "0x88888"), -1));
    m_pid->SetToolTip(wxString::Format(_("SSAG Loader USB Product ID. Default = 0x%04x"), default_loader_pid));

    sz1 = new wxBoxSizer(wxHORIZONTAL);
    sz1->Add(label, wxSizerFlags(0).Border(wxALL, 10));
    sz1->Add(m_pid, wxSizerFlags(0).Border(wxALL, 10));

    sz0->Add(sz1, 1, wxEXPAND, 5);

    wxStdDialogButtonSizer *bs = new wxStdDialogButtonSizer();
    bs->AddButton(new wxButton(this, wxID_OK));
    bs->AddButton(new wxButton(this, wxID_CANCEL));
    bs->Realize();
    sz0->Add(bs, 0, wxALL | wxEXPAND, 5);

    SetSizer(sz0);
    Layout();
    Fit();

    Centre(wxBOTH);
}

void CameraOpenSSAG::ShowPropertyDialog()
{
    PropertiesDlg dlg(wxGetApp().GetTopWindow());

    if (dlg.ShowModal() == wxID_OK)
    {
        long vid, pid;
        if (dlg.m_vid->GetValue().ToLong(&vid, 0) &&
            dlg.m_pid->GetValue().ToLong(&pid, 0))
        {
            SetLoaderVidPid(vid, pid);
        }
    }
}

#endif // Apple-only
