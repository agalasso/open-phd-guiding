/*
 *  config_indi.h
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

#ifndef _CONFIG_INDI_H_
#define _CONFIG_INDI_H_

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>
#include <libindi/indiproperty.h>
#include <libindi/indibasetypes.h>

#include "indi_gui.h"

enum IndiDevType
{
    INDI_TYPE_CAMERA,
    INDI_TYPE_MOUNT,
    INDI_TYPE_AUX_MOUNT,
    INDI_TYPE_AO,
    INDI_TYPE_ROTATOR,
};

class INDIConfig : public wxDialog, public INDI::BaseClient
{
        static bool s_verbose;

        wxTextCtrl *host;
        wxTextCtrl *port;
        wxButton *connect;
        wxStaticText *connect_status;
        wxStaticText *devlabel;
        wxComboBox *dev;
        wxComboBox *ccd;
        wxCheckBox *forcevideo;
        wxCheckBox *forceexposure;
        wxButton *guiBtn;
        wxButton *okBtn;

        IndiGui *m_gui;
        IndiDevType dev_type;

    public:

        INDIConfig(wxWindow *parent, const wxString& title, IndiDevType devtype);
        ~INDIConfig();

        long     INDIport;
        wxString INDIhost;
        wxString INDIDevName;
        long     INDIDevCCD;
        bool     INDIForceVideo;
        bool     INDIForceExposure;

        void Connect();
        void Disconnect();
        void SetSettings();
        void SaveSettings();

        static void LoadProfileSettings();
        static bool Verbose();
        static void SetVerbose(bool val);

        void OnUpdateFromThread(wxThreadEvent& event);

    protected:

        void newDevice(INDI::BaseDevice dp) override;
        void removeDevice(INDI::BaseDevice dp) override {};
        void newProperty(INDI::Property property) override;
        void removeProperty(INDI::Property property) override {}
        void serverConnected() override;
        void serverDisconnected(int exit_code) override;

    private:

        void OnConnectButton(wxCommandEvent& evt);
        void OnIndiGui(wxCommandEvent& evt);
        void OnDevSelected(wxCommandEvent& evt);
        void OnVerboseChecked(wxCommandEvent& evt);
        void OnForceVideoChecked(wxCommandEvent& evt);
        void UpdateControlStates();

        wxDECLARE_EVENT_TABLE();
};

inline bool INDIConfig::Verbose()
{
    return INDIConfig::s_verbose;
}

#endif
