/*
*  rotator_indi.cpp
*  PHD Guiding
*
*  Created by Philipp Weber and Kirill M. Skorobogatov
*  Copyright (c) 2024 openphdguiding.org
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
*    Neither the name of openphdguiding.org nor the names of its
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

#ifdef ROTATOR_INDI

#include "rotator_indi.h"
#include "rotator.h"
#include "config_indi.h"
#include "indi_gui.h"
#include "libindi/baseclient.h"

#include <libindi/basedevice.h>
#include <libindi/indiproperty.h>

class RotatorINDI : public Rotator, public INDI::BaseClient {
    public:
        RotatorINDI();
        ~RotatorINDI();
        bool Connect() override;
        bool Disconnect() override;
        wxString Name() const override;
        float Position() const override;

    private:
        long          INDIport;
        wxString      INDIhost;
        volatile bool modal;
        bool          m_ready = false;
        float         m_angle = POSITION_UNKNOWN;

        INumberVectorProperty *angle_prop;
        ISwitchVectorProperty *connection_prop;

        IndiGui  *m_gui;

        wxString INDIRotatorName;
        wxString m_Name;

        void ClearStatus();
        void CheckState();
        void RotatorDialog();
        void RotatorSetup();
        void updateAngle();

    protected:
        void serverConnected() override;
        void serverDisconnected(int exit_code) override;
        void newDevice(INDI::BaseDevice dp) override;
        void removeDevice(INDI::BaseDevice dp) override;
        void newProperty(INDI::Property property) override;
        void updateProperty(INDI::Property property) override;
        void newMessage(INDI::BaseDevice dp, int messageID) override;

    private:
        void ShowPropertyDialog() override;
};

void RotatorINDI::ClearStatus() {
    angle_prop = nullptr;
    connection_prop = nullptr;
    m_ready = false;
    m_angle = POSITION_UNKNOWN;
}

void RotatorINDI::CheckState() {
    if ( ! IsConnected() ) {
        return;
    }
    if ( m_ready ) {
        return;
    }
    if ( ! angle_prop ) {
        return;
    }

    Debug.Write(wxString(_("INDI Rotator is ready\n")));
    m_ready = true;
    if ( modal ) {
        modal = false;
    }
}

RotatorINDI::RotatorINDI() : m_gui(nullptr) {
    ClearStatus();
    INDIhost = pConfig->Profile.GetString("/indi/INDIhost", _T("localhost"));
    INDIport = pConfig->Profile.GetLong("/indi/INDIport", 7624);
    INDIRotatorName = pConfig->Profile.GetString("/indi/INDIrotator", _T("INDI Rotator"));
    m_Name = wxString::Format("INDI Rotator [%s]", INDIRotatorName);
}

RotatorINDI::~RotatorINDI() {
    if ( m_gui ) {
        IndiGui::DestroyIndiGui(&m_gui);
    }
    disconnectServer();
}

bool RotatorINDI::Connect() {
    if ( INDIRotatorName == wxT("INDI Rotator") ) {
        RotatorSetup();
    }

    if(isServerConnected()) {
        return false;
    }

    setServer(INDIhost.mb_str(wxConvUTF8), INDIport);
    watchDevice(INDIRotatorName.mb_str(wxConvUTF8));

    Debug.Write(wxString::Format("Waiting for 30s for [%s] to connect...\n", INDIRotatorName));

    /* Wait in background for driver to establish a device connection */
    struct ConnectInBg : public ConnectRotatorInBg {
        RotatorINDI* rotator {nullptr};
        ConnectInBg(RotatorINDI* rotator_) : rotator(rotator_) {}

        bool Entry() {
            // Wait for driver to establish a device connection
            if(rotator->connectServer()) {
                int i = 0;
                while(!rotator->IsConnected() && i++ < 300) {
                    if(IsCanceled()) {
                        break;
                    }
                    wxMilliSleep(100);
                }
            }

            // We need to return FALSE if we are successful
            return !rotator->IsConnected();
        }
    };

    return ConnectInBg(this).Run();
}

void RotatorINDI::serverConnected() {
    Debug.Write("INDI Rotator connection succeeded\n");
    Rotator::Connect();
}

bool RotatorINDI::Disconnect() {
    disconnectServer();
    return false;
}

void RotatorINDI::serverDisconnected(int exit_code) {
    Rotator::Disconnect();
    ClearStatus();

    if(pFrame) {
        pFrame->UpdateStatsWindowScopePointing();
    }

    if ( exit_code == -1 ) {
        if(pFrame) {
            pFrame->Alert(wxString(_("INDI server disconnected")));
        }
        Disconnect();
    }
}

void RotatorINDI::removeDevice(INDI::BaseDevice dp) {
    ClearStatus();
    Disconnect();
}

void RotatorINDI::ShowPropertyDialog() {
    if ( IsConnected() ) {
        RotatorDialog();
    } else {
        RotatorSetup();
    }
}

void RotatorINDI::RotatorDialog() {
    if ( m_gui ) {
        m_gui->Show();
    } else {
        IndiGui::ShowIndiGui(&m_gui, INDIhost, INDIport, false, false);
    }
}

void RotatorINDI::RotatorSetup() {
    INDIConfig indiDlg(wxGetApp().GetTopWindow(), _("INDI Rotator Selection"), INDI_TYPE_ROTATOR);
    indiDlg.INDIhost = INDIhost;
    indiDlg.INDIport = INDIport;
    indiDlg.INDIDevName = INDIRotatorName;
    indiDlg.SetSettings();
    indiDlg.Connect();
    if ( indiDlg.ShowModal() == wxID_OK ) {
        indiDlg.SaveSettings();
        INDIhost = indiDlg.INDIhost;
        INDIport = indiDlg.INDIport;
        INDIRotatorName = indiDlg.INDIDevName;
        pConfig->Profile.SetString("/indi/INDIhost", INDIhost);
        pConfig->Profile.SetLong("/indi/INDIport", INDIport);
        pConfig->Profile.SetString("/indi/INDIrotator", INDIRotatorName);
        m_Name = INDIRotatorName;
    }
    indiDlg.Disconnect();
}

wxString RotatorINDI::Name() const {
    return m_Name;
}

float RotatorINDI::Position() const {
    return m_angle;
}

void RotatorINDI::updateAngle() {
    if (!IsConnected()) {
        m_angle = POSITION_UNKNOWN;
        return;
    }
    if(!angle_prop) {
        return;
    }

    m_angle = IUFindNumber(angle_prop, "ANGLE")->value;
}

void RotatorINDI::newProperty(INDI::Property property) {
    wxString PropName(property.getName());
    INDI_PROPERTY_TYPE Proptype = property.getType();

    Debug.Write(wxString::Format("INDI Rotator: Received property: %s\n", PropName));

    if ( Proptype == INDI_NUMBER && PropName == "ABS_ROTATOR_ANGLE" ) {
        if ( INDIConfig::Verbose() ) {
            Debug.Write(wxString::Format(_("INDI Rotator found ABS_ROTATOR_ANGLE for %s %s\n"),
                            property.getDeviceName(), PropName));
        }
        angle_prop = property.getNumber();
        updateAngle();

        if(pFrame) {
            pFrame->UpdateStatsWindowScopePointing();
        }
    }
    if ( Proptype == INDI_SWITCH && PropName == "CONNECTION" ) {
        if ( INDIConfig::Verbose() ) {
            Debug.Write(wxString::Format(_("INDI Rotator found CONNECTION for %s %s\n"),
                        property.getDeviceName(), PropName));
        }
        connection_prop = property.getSwitch();
        ISwitch *connectswitch = IUFindSwitch(connection_prop, "CONNECT");
        if ( connectswitch->s == ISS_ON ) {
            Rotator::Connect();
        }
    }
    CheckState();
}

void RotatorINDI::newMessage(INDI::BaseDevice dp, int messageID) {
    if ( INDIConfig::Verbose() ) {
        Debug.Write(wxString::Format(_("INDI Rotator received message: %s\n"), dp.messageQueue(messageID)));
    }
}

void RotatorINDI::newDevice(INDI::BaseDevice dp) {
    if ( INDIConfig::Verbose() ) {
        Debug.Write(wxString::Format("INDI Rotator new device %s\n", dp.getDeviceName()));
    }
}

void RotatorINDI::updateProperty(INDI::Property property) {
    switch(property.getType()) {
        case INDI_SWITCH:
        {
            auto* svp = property.getSwitch();
            if ( INDIConfig::Verbose() ) {
                Debug.Write(wxString::Format("INDI Rotator: Receiving Switch: %s = %i\n",
                            svp->name, svp->sp->s));
            }
            if ( strcmp(svp->name, "CONNECTION") == 0 ) {
                ISwitch *connectswitch = IUFindSwitch(svp, "CONNECT");
                if ( connectswitch->s == ISS_ON ) {
                    Rotator::Connect();
                    CheckState();
                } else {
                    if ( m_ready ) {
                        ClearStatus();
                        PhdApp::ExecInMainThread(
                                [this]() {
                                pFrame->Alert(_("INDI rotator was disconnected"));
                                Disconnect();
                                });
                    }
                }
            }
        } break;
        case INDI_NUMBER:
        {
            auto* nvp = property.getNumber();
            if ( INDIConfig::Verbose() ) {
                Debug.Write(wxString::Format(_("INDI Rotator: New number: %s\n"), nvp->name));
            }
            if ( strcmp(nvp->name, "ABS_ROTATOR_ANGLE") == 0 ) {
                updateAngle();

                if(pFrame) {
                    pFrame->UpdateStatsWindowScopePointing();
                }
            }
        } break;
        default:
            break;
    }
}

Rotator *INDIRotatorFactory::MakeINDIRotator() {
    return new RotatorINDI();
}

#endif
