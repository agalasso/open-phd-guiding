/*
 *  config_indi.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark
 *  Copyright (c) 2006, 2007, 2008, 2009 Craig Stark.
 *  All rights reserved.
 *
 *  Redraw for libindi/baseclient by Patrick Chevalley
 *  Copyright (c) 2014 Patrick Chevalley
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
#include "camera.h"
#include "scope.h"

#if defined (INDI_CAMERA) || defined (GUIDE_INDI)

#include "config_indi.h"

#include <wx/sizer.h>
#include <wx/gbsizer.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/dialog.h>

enum
{
    MCONNECT = 101,
    MINDIGUI = 102,
    MDEV = 103,
    VERBOSE = 104,
    FORCEVIDEO = 105,
};

#define POS(r, c)        wxGBPosition(r,c)
#define SPAN(r, c)       wxGBSpan(r,c)

bool INDIConfig::s_verbose;

INDIConfig::INDIConfig(wxWindow *parent, const wxString& title, IndiDevType devtype)
    :
    wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    m_gui(nullptr),
    dev_type(devtype)
{
    auto sizerLabelFlags  = wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL;
    auto sizerButtonFlags = wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL;
    auto sizerSectionFlags = wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL;
    auto sizerTextFlags = wxALIGN_LEFT | wxALL | wxEXPAND;
    int border = 2;

    int pos;
    wxGridBagSizer *gbs = new wxGridBagSizer(0, 20);
    wxBoxSizer *sizer;

    pos = 0;
    gbs->Add(new wxStaticText(this, wxID_ANY, _("INDI Server")),
             POS(pos, 0), SPAN(1, 1), sizerSectionFlags, border);

    ++pos;
    gbs->Add(new wxStaticText(this, wxID_ANY, _("Hostname")),
             POS(pos, 0), SPAN(1, 1), sizerLabelFlags, border);
    host = new wxTextCtrl(this, wxID_ANY);
    gbs->Add(host, POS(pos, 1), SPAN(1, 1), sizerTextFlags, border);

    ++pos;
    gbs->Add(new wxStaticText(this, wxID_ANY, _("Port")),
             POS(pos, 0), SPAN(1, 1), sizerLabelFlags, border);
    port = new wxTextCtrl(this, wxID_ANY);
    gbs->Add(port, POS(pos, 1), SPAN(1, 1), sizerTextFlags, border);

    ++pos;
    connect_status = new wxStaticText(this, wxID_ANY, _("Disconnected"));
    gbs->Add(connect_status, POS(pos, 0), SPAN(1, 1), wxALIGN_RIGHT | wxALL | wxALIGN_CENTER_VERTICAL, border);
    connect = new wxButton(this, MCONNECT, _("Connect"));
    gbs->Add(connect, POS(pos, 1), SPAN(1, 1), sizerButtonFlags, border);

    ++pos;
    gbs->Add(new wxStaticText(this, wxID_ANY, _T("========")),
             POS(pos, 0), SPAN(1, 1), wxALIGN_LEFT | wxALL, border);
    devlabel = new wxStaticText(this, wxID_ANY, _("Device"));

    if (dev_type == INDI_TYPE_CAMERA)
        devlabel->SetLabel(_("Camera"));
    else if (dev_type == INDI_TYPE_MOUNT)
        devlabel->SetLabel(_("Mount"));
    else if (dev_type == INDI_TYPE_AUX_MOUNT)
        devlabel->SetLabel(_("Aux Mount"));
    else if (dev_type == INDI_TYPE_AO)
        devlabel->SetLabel(_("AO"));
    else if (dev_type == INDI_TYPE_ROTATOR)
        devlabel->SetLabel(_("Rotator"));

    gbs->Add(devlabel, POS(pos, 1), SPAN(1, 1), wxALIGN_LEFT | wxALL, border);

    ++pos;
    gbs->Add(new wxStaticText(this, wxID_ANY, _("Driver")),
             POS(pos, 0), SPAN(1, 1), sizerLabelFlags, border);
    dev = new wxComboBox(this, MDEV, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
    gbs->Add(dev, POS(pos, 1), SPAN(1, 1), sizerTextFlags, border);

    ccd = nullptr;
    if (dev_type == INDI_TYPE_CAMERA)
    {
        ++pos;
        gbs->Add(new wxStaticText(this, wxID_ANY, _("Dual CCD")),
                 POS(pos, 0), SPAN(1, 1), sizerLabelFlags, border);
        ccd = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        gbs->Add(ccd, POS(pos, 1), SPAN(1, 1), sizerTextFlags, border);
    }

    forcevideo = nullptr;
    forceexposure = nullptr;
    if (dev_type == INDI_TYPE_CAMERA)
    {
        ++pos;
        forcevideo = new wxCheckBox(this, FORCEVIDEO, _("Camera does not support exposure time"));
        forcevideo->SetToolTip(
            _("Force the use of streaming and frame stacking for cameras that do not support setting an absolute exposure time."));
        gbs->Add(forcevideo,  POS(pos, 0), SPAN(1, 2), sizerTextFlags, border);

        ++pos;
        forceexposure = new wxCheckBox(this, wxID_ANY, _("Camera does not support streaming"));
        forceexposure->SetToolTip(_("Force the use of exposure time for cameras that do not support streaming."));
        gbs->Add(forceexposure,  POS(pos, 0), SPAN(1, 2), sizerTextFlags, border);
    }

    ++pos;
    gbs->Add(new wxStaticText(this, wxID_ANY, _("Other options")),
             POS(pos, 0), SPAN(1, 1), sizerLabelFlags, border);
    guiBtn = new wxButton(this, MINDIGUI, _("INDI"));
    gbs->Add(guiBtn, POS(pos, 1), SPAN(1, 1), sizerButtonFlags, border);

    ++pos;
    wxCheckBox *cb = new wxCheckBox(this, VERBOSE, _("Verbose logging"));
    cb->SetToolTip(_("Enable more detailed INDI information in the PHD2 Debug Log."));
    cb->SetValue(INDIConfig::Verbose());
    gbs->Add(cb, POS(pos, 0), SPAN(1, 2), sizerTextFlags, border);

    sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(gbs);
    sizer->AddSpacer(10);
    sizer->Add(CreateButtonSizer(wxOK | wxCANCEL));
    okBtn = static_cast<wxButton *>(this->FindWindow(wxID_OK));
    sizer->AddSpacer(10);
    SetSizer(sizer);
    sizer->SetSizeHints(this);
    sizer->Fit(this);

    UpdateControlStates();
}

void INDIConfig::OnUpdateFromThread(wxThreadEvent& event)
{
    UpdateControlStates();
}

void INDIConfig::UpdateControlStates()
{
    if (isServerConnected())
    {
        host->Enable(false);
        port->Enable(false);
        connect_status->SetLabel(_("Connected"));
        connect->SetLabel(_("Disconnect"));

        // device gets selected when driver property arrives
        dev->Enable(true);

        if (dev_type == INDI_TYPE_CAMERA)
        {
            ccd->Append(_("Main"));
            ccd->Append(_("Secondary"));
            ccd->SetSelection(INDIDevCCD);
            ccd->Enable(true);
            forcevideo->SetValue(INDIForceVideo);
            forcevideo->Enable(true);
            forceexposure->SetValue(INDIForceExposure);
            forceexposure->Enable(!INDIForceVideo);
        }
        guiBtn->Enable(true);

        // okBtn remains disabled until a device is selected
    }
    else
    {
        host->Enable(true);
        port->Enable(true);
        connect_status->SetLabel(_("Disconnected"));
        connect->SetLabel(_("Connect"));

        dev->Clear();
        dev->Enable(false);

        if (dev_type == INDI_TYPE_CAMERA)
        {
            ccd->Clear();
            ccd->Enable(false);
            forcevideo->SetValue(false);
            forcevideo->Enable(false);
            forceexposure->SetValue(false);
            forceexposure->Enable(false);
        }
        guiBtn->Enable(false);
        okBtn->Enable(false);
    }
}

wxDEFINE_EVENT(THREAD_UPDATE_EVENT, wxThreadEvent);

wxBEGIN_EVENT_TABLE(INDIConfig, wxDialog)
    EVT_BUTTON(MCONNECT, INDIConfig::OnConnectButton)
    EVT_BUTTON(MINDIGUI, INDIConfig::OnIndiGui)
    EVT_COMBOBOX(MDEV, INDIConfig::OnDevSelected)
    EVT_CHECKBOX(VERBOSE, INDIConfig::OnVerboseChecked)
    EVT_CHECKBOX(FORCEVIDEO, INDIConfig::OnForceVideoChecked)
    EVT_THREAD(THREAD_UPDATE_EVENT, INDIConfig::OnUpdateFromThread)
wxEND_EVENT_TABLE()

INDIConfig::~INDIConfig()
{
    if (m_gui)
        IndiGui::DestroyIndiGui(&m_gui);

    disconnectServer();
}

void INDIConfig::OnIndiGui(wxCommandEvent& WXUNUSED(event))
{
    if (m_gui)
    {
        m_gui->ShowModal();
    }
    else
    {
        IndiGui::ShowIndiGui(&m_gui, INDIhost, INDIport, true, true);
    }
}

void INDIConfig::OnConnectButton(wxCommandEvent& WXUNUSED(event))
{
    if (isServerConnected())
        Disconnect();
    else
        Connect();
}

void INDIConfig::OnDevSelected(wxCommandEvent& WXUNUSED(event))
{
    okBtn->Enable(true);
}

void INDIConfig::LoadProfileSettings()
{
    s_verbose = pConfig->Profile.GetBoolean("/indi/VerboseLogging", false);
}

void INDIConfig::SetVerbose(bool val)
{
    if (s_verbose != val)
    {
        Debug.Write(wxString::Format("INDI Verbose Logging %s\n", val ? "enabled" : "disabled"));
        INDIConfig::s_verbose = val;
        pConfig->Profile.SetBoolean("/indi/VerboseLogging", val);
    }
}

void INDIConfig::OnVerboseChecked(wxCommandEvent& evt)
{
    INDIConfig::SetVerbose(evt.IsChecked());
}

void INDIConfig::OnForceVideoChecked(wxCommandEvent& evt)
{
    forceexposure->Enable(!evt.IsChecked());
    if (evt.IsChecked())
    {
        forceexposure->SetValue(false);
    }
}

void INDIConfig::Connect()
{
    wxASSERT(!isServerConnected());

    INDIhost = host->GetLineText(0);
    port->GetLineText(0).ToLong(&INDIport);
    setServer(INDIhost.mb_str(wxConvUTF8), INDIport);

    connectServer();
}

void INDIConfig::Disconnect()
{
    disconnectServer();
}

void INDIConfig::serverConnected()
{
    if (wxThread::IsMain())
    {
        UpdateControlStates();
    }
    else
    {
        wxQueueEvent(this, new wxThreadEvent(wxEVT_THREAD, THREAD_UPDATE_EVENT));
    }
}

void INDIConfig::serverDisconnected(int exit_code)
{
    if (wxThread::IsMain())
    {
        UpdateControlStates();
    }
    else
    {
        wxQueueEvent(this, new wxThreadEvent(wxEVT_THREAD, THREAD_UPDATE_EVENT));
    }
}

void INDIConfig::newDevice(INDI::BaseDevice dp)
{
    auto devname = dp.getDeviceName();

    Debug.Write(wxString::Format("INDIConfig: newDevice %s\n", devname));

    dev->Append(devname);
    if (devname == INDIDevName)
    {
        dev->SetValue(INDIDevName);
        okBtn->Enable(true);
    }
}

inline static void _append(wxString& s, const wxString& ap)
{
    if (s.Length())
        s += "|";
    s += ap;
}

static wxString formatInterface(unsigned int ifs)
{
    if (ifs == INDI::BaseDevice::GENERAL_INTERFACE)
        return "GENERAL";

    wxString s;

#define F(t) \
    if (ifs & INDI::BaseDevice:: t ## _INTERFACE) \
        _append(s, #t)

    F(TELESCOPE);
    F(CCD);
    F(GUIDER);
    F(FOCUSER);
    F(FILTER);
    F(DOME);
    F(GPS);
    F(WEATHER);
    F(AO);
    F(DUSTCAP);
    F(LIGHTBOX);
    F(AUX);

#undef F

    return s;
}

void INDIConfig::newProperty(INDI::Property property)
{
    if (strcmp(property.getName(), "DRIVER_INFO") == 0)
    {
        wxString devname(property.getDeviceName());
        uint16_t di = property.getBaseDevice().getDriverInterface();

        Debug.Write(wxString::Format("device %s interface(s) %s\n", property.getDeviceName(), formatInterface(di)));

        bool include = false;

        if (dev_type == INDI_TYPE_CAMERA)
            include = (di & INDI::BaseDevice::CCD_INTERFACE) != 0;
        else if (dev_type == INDI_TYPE_MOUNT)
            include = (di & INDI::BaseDevice::CCD_INTERFACE) == 0 && (di & INDI::BaseDevice::GUIDER_INTERFACE) != 0;
        else if (dev_type == INDI_TYPE_AUX_MOUNT)
            include = (di & INDI::BaseDevice::TELESCOPE_INTERFACE) != 0;
        else if (dev_type == INDI_TYPE_AO)
            include = (di & INDI::BaseDevice::AO_INTERFACE) != 0;
        else if (dev_type == INDI_TYPE_ROTATOR)
            include = (di & INDI::BaseDevice::ROTATOR_INTERFACE) != 0;

        if (!include)
        {
            Debug.Write(wxString::Format("exclude device %s not a valid %s\n", devname,
                dev_type == INDI_TYPE_CAMERA ? "camera" :
                dev_type == INDI_TYPE_MOUNT ? "mount" :
                dev_type == INDI_TYPE_AUX_MOUNT ? "aux mount" :
                dev_type == INDI_TYPE_AO ? "AO" :
                "rotator"));

            int n = dev->FindString(devname, true);
            if (n != wxNOT_FOUND)
            {
                dev->Delete(n);
                // re-select
                int pos = dev->FindString(INDIDevName, true);
                dev->SetSelection(pos);
                if (pos == wxNOT_FOUND)
                    okBtn->Enable(false);
            }
        }
    }
}

void INDIConfig::SetSettings()
{
    host->WriteText(INDIhost);
    port->WriteText(wxString::Format("%ld", INDIport));
}

void INDIConfig::SaveSettings()
{
    INDIhost = host->GetLineText(0);
    port->GetLineText(0).ToLong(&INDIport);
    INDIDevName = dev->GetValue();
    if (dev_type == INDI_TYPE_CAMERA)
    {
        INDIForceVideo = forcevideo->GetValue();
        INDIForceExposure = forceexposure->GetValue();
        INDIDevCCD = ccd->GetSelection();
    }
}

#endif
