/*
 *  stepguider_sxao_indi.cpp
 *  PHD Guiding
 *
 *  Created by Hans Lambermont
 *  Copyright (c) 2016 Hans Lambermont
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
 *    Neither the name Craig Stark, Stark Labs nor the names of its
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

#ifdef STEPGUIDER_SXAO_INDI

#include "stepguider_sxao_indi.h"
#include <libindi/baseclient.h>
#include "config_indi.h"

#include <libindi/basedevice.h>
#include <libindi/indiproperty.h>

class StepGuiderSxAoINDI : public StepGuider, public INDI::BaseClient
{
    private:
        // INDI parts
        static const int MaxDeviceInitWaitMilliSeconds = 2000;
        static const int MaxDevicePropertiesWaitMilliSeconds = 5000;
        long     INDIport;
        wxString INDIhost;
        wxString INDIaoDeviceName;
        bool     modal;
        bool     ready;
        void     ClearStatus();
        void     CheckState();

        INumberVectorProperty *pulseGuideNS_prop;
        INumber               *pulseN_prop;
        INumber               *pulseS_prop;
        INumberVectorProperty *pulseGuideWE_prop;
        INumber               *pulseW_prop;
        INumber               *pulseE_prop;
        INumberVectorProperty *aoNS_prop;
        INumber               *aoN_prop;
        INumber               *aoS_prop;
        INumberVectorProperty *aoWE_prop;
        INumber               *aoW_prop;
        INumber               *aoE_prop;
        ISwitchVectorProperty *aoCenterUnjam_prop;
        ISwitch               *aoCenter_prop;
        ISwitch               *aoUnjam_prop;
        INDI::BaseDevice      ao_device;
        ITextVectorProperty   *ao_port;
        ITextVectorProperty   *ao_driverInfo;
        IText                 *ao_driverName;
        IText                 *ao_driverExec;
        IText                 *ao_driverVersion;
        IText                 *ao_driverInterface;
        ITextVectorProperty   *ao_info;
        IText                 *ao_firmware;
        ILightVectorProperty  *ao_limit;
        ILight                *ao_limit_north;
        ILight                *ao_limit_south;
        ILight                *ao_limit_east;
        ILight                *ao_limit_west;

        // StepGuider parts
        static const int DefaultMaxSteps = 45;
        wxString m_Name;
        int m_maxSteps;
        int SxAoVersion;
        bool Connected {false};

        bool Center() override;
        STEP_RESULT Step(GUIDE_DIRECTION direction, int steps) override;
        int MaxPosition(GUIDE_DIRECTION direction) const override;
        bool SetMaxPosition(int steps) override;
        bool IsAtLimit(GUIDE_DIRECTION direction, bool *isAtLimit) override;

        bool FirmwareVersion(int *version);

        bool    ST4HasGuideOutput() override;
        bool    ST4HostConnected() override;
        bool    ST4HasNonGuiMove() override;
        bool    ST4PulseGuideScope(int direction, int duration) override;

    protected:
        // INDI parts
        void newDevice(INDI::BaseDevice dp) override;
        void removeDevice(INDI::BaseDevice dp) override;
        void newProperty(INDI::Property property) override;
        void removeProperty(INDI::Property property) override {};
        //void updateProperty(INDI::Property property) override;
        //void newMessage(INDI::BaseDevice dp, int messageID) override;
        void serverConnected() override;
        void serverDisconnected(int exit_code) override;

    public:
        StepGuiderSxAoINDI();
        ~StepGuiderSxAoINDI();

        // StepGuider parts
        bool Connect() override;
        bool Disconnect() override;
        bool HasNonGuiMove() override;
        bool HasSetupDialog() const override;
        void SetupDialog() override;

    private:
        void ShowPropertyDialog() override;
};

StepGuiderSxAoINDI::StepGuiderSxAoINDI()
{
    ClearStatus();

    // load the values from the current profile
    INDIhost   = pConfig->Profile.GetString("/indi/INDIhost", _T("localhost"));
    INDIport   = pConfig->Profile.GetLong("/indi/INDIport", 7624);
    INDIaoDeviceName = pConfig->Profile.GetString("/indi/INDIao", _T("INDI SXV-AO-LF"));

    m_Name = INDIaoDeviceName;
    m_maxSteps = pConfig->Profile.GetInt("/stepguider/sxao/MaxSteps", DefaultMaxSteps);
}

StepGuiderSxAoINDI::~StepGuiderSxAoINDI()
{
    disconnectServer();
}

void StepGuiderSxAoINDI::ClearStatus()
{
    // reset properties
    pulseGuideNS_prop = nullptr;
    pulseN_prop = nullptr;
    pulseS_prop = nullptr;
    pulseGuideWE_prop = nullptr;
    pulseW_prop = nullptr;
    pulseE_prop = nullptr;
    aoNS_prop = nullptr;
    aoN_prop = nullptr;
    aoS_prop = nullptr;
    aoWE_prop = nullptr;
    aoW_prop = nullptr;
    aoE_prop = nullptr;
    aoCenterUnjam_prop = nullptr;
    aoCenter_prop = nullptr;
    aoUnjam_prop = nullptr;
    ao_port = nullptr;
    ao_driverInfo = nullptr;
    ao_driverName = nullptr;
    ao_driverExec = nullptr;
    ao_driverVersion = nullptr;
    ao_driverInterface = nullptr;
    ao_info = nullptr;
    ao_firmware = nullptr;
    ao_limit = nullptr;
    ao_limit_north = nullptr;
    ao_limit_south = nullptr;
    ao_limit_east = nullptr;
    ao_limit_west = nullptr;
    SxAoVersion = -1;

    // reset connection status
    ready = false;
}

void StepGuiderSxAoINDI::CheckState()
{
    // Check if the device has all the required properties for our usage.
    if (IsConnected() && (ao_driverVersion && aoN_prop && aoS_prop && aoW_prop && aoE_prop && aoCenter_prop))
    {
        if (atof(ao_driverVersion->text) < 1.12)
        {
            wxMessageBox(wxString::Format(
                             _("We need at least INDI driver %s version 1.12 to get the Firmware version and the Limit switch states."),
                             ao_driverExec->text), _("Error"));
        }
        else if (ao_firmware)
        {
            if (! ready)
            {
                if (FirmwareVersion(&SxAoVersion))
                {
                    throw ERROR_INFO("StepGuiderSxAoINDI::CheckState: unable to get firmware version");
                }
                if (SxAoVersion == 0)
                {
                    wxMessageBox(wxString::Format(
                                     _("This AO device has firmware version %03u which means it needs to be flashed.\n"
                                       "The SXV-AO Utility v104 or newer, available at http://www.sxccd.com/drivers-downloads,\n"
                                       "contains the firmware."), SxAoVersion),
                                 _("Error"));
                    throw ERROR_INFO("StepGuiderSxAoINDI::CheckState: V000 means AO device needs a flash");
                }
                Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::CheckState is ready, firmware %03u", SxAoVersion));
                ready = true;
                if (modal)
                {
                    modal = false;
                }
            }
        }
    }
}

void StepGuiderSxAoINDI::newDevice(INDI::BaseDevice dp)
{
    if (strcmp(dp.getDeviceName(), INDIaoDeviceName.mb_str(wxConvUTF8)) == 0)
    {
        ao_device = dp;
    }
}

void StepGuiderSxAoINDI::newProperty(INDI::Property property)
{
    auto PropName = property.getName();
    auto Proptype = property.getType();

    /*
    printf("SXAO PropName: %s Proptype: %d\n", PropName, Proptype);
    fflush(stdout);
     * 0 INDI_NUMBER, < INumberVectorProperty.
     * 1 INDI_SWITCH, < ISwitchVectorProperty.
     * 2 INDI_TEXT,   < ITextVectorProperty.
     * 3 INDI_LIGHT,  < ILightVectorProperty.
     * 4 INDI_BLOB,   < IBLOBVectorProperty.
     * 5 INDI_UNKNOWN
    SXAO PropName: CONNECTION Proptype: 1
    SXAO PropName: DRIVER_INFO Proptype: 2
    SXAO PropName: DEBUG Proptype: 1
    SXAO PropName: SIMULATION Proptype: 1
    SXAO PropName: DEVICE_PORT Proptype: 2
    SXAO PropName: CONFIG_PROCESS Proptype: 1
    SXAO PropName: TELESCOPE_TIMED_GUIDE_NS Proptype: 0
    SXAO PropName: TELESCOPE_TIMED_GUIDE_WE Proptype: 0
    SXAO PropName: AO_NS Proptype: 0
    SXAO PropName: AO_WE Proptype: 0
    SXAO PropName: AO_CENTER Proptype: 1
    SXAO PropName: INFO Proptype: 2
    SXAO PropName: AT_LIMIT Proptype: 3
    */

    if (strcmp(PropName, "CONNECTION") == 0 && Proptype == INDI_SWITCH)
    {
        ISwitch *connectswitch = IUFindSwitch(property.getSwitch(), "CONNECT");
        Connected = connectswitch->s == ISS_ON;
        if (Connected)
        {
            StepGuider::Connect();
        }
    }
    else if (strcmp(PropName, "DRIVER_INFO") == 0 && Proptype == INDI_TEXT)
    {
        ao_driverInfo = property.getText();
        ao_driverName = IUFindText(ao_driverInfo, "DRIVER_NAME");
        ao_driverExec = IUFindText(ao_driverInfo, "DRIVER_EXEC");
        ao_driverVersion = IUFindText(ao_driverInfo, "DRIVER_VERSION"); // we need >=1.12 for Firmware version and AT_LIMIT
        ao_driverInterface = IUFindText(ao_driverInfo, "DRIVER_INTERFACE");

        if (!Connected)
            connectDevice(property.getDeviceName());
    }
    else if (strcmp(PropName, "DEVICE_PORT") == 0 && Proptype == INDI_TEXT)
    {
        ao_port = property.getText();
    }
    else if ((strcmp(PropName, "TELESCOPE_TIMED_GUIDE_NS") == 0) && Proptype == INDI_NUMBER)
    {
        pulseGuideNS_prop = property.getNumber();
        pulseN_prop = IUFindNumber(pulseGuideNS_prop, "TIMED_GUIDE_N");
        pulseS_prop = IUFindNumber(pulseGuideNS_prop, "TIMED_GUIDE_S");
    }
    else if ((strcmp(PropName, "TELESCOPE_TIMED_GUIDE_WE") == 0) && Proptype == INDI_NUMBER)
    {
        pulseGuideWE_prop = property.getNumber();
        pulseW_prop = IUFindNumber(pulseGuideWE_prop, "TIMED_GUIDE_W");
        pulseE_prop = IUFindNumber(pulseGuideWE_prop, "TIMED_GUIDE_E");
    }
    else if ((strcmp(PropName, "AO_NS") == 0) && Proptype == INDI_NUMBER)
    {
        aoNS_prop = property.getNumber();
        aoN_prop = IUFindNumber(aoNS_prop, "AO_N");
        aoS_prop = IUFindNumber(aoNS_prop, "AO_S");
    }
    else if ((strcmp(PropName, "AO_WE") == 0) && Proptype == INDI_NUMBER)
    {
        aoWE_prop = property.getNumber();
        aoW_prop = IUFindNumber(aoWE_prop, "AO_W");
        aoE_prop = IUFindNumber(aoWE_prop, "AO_E");
    }
    else if (strcmp(PropName, "AO_CENTER") == 0 && Proptype == INDI_SWITCH)
    {
        aoCenterUnjam_prop = property.getSwitch();
        aoCenter_prop = IUFindSwitch(aoCenterUnjam_prop, "CENTER");
        aoUnjam_prop  = IUFindSwitch(aoCenterUnjam_prop, "UNJAM");
    }
    else if (strcmp(PropName, "INFO") == 0 && Proptype == INDI_TEXT)
    {
        ao_info = property.getText();
        ao_firmware = IUFindText(ao_info, "FIRMWARE");
    }
    else if (strcmp(PropName, "AT_LIMIT") == 0 && Proptype == INDI_LIGHT)
    {
        ao_limit = property.getLight();
        ao_limit_north = IUFindLight(ao_limit, "AT_LIMIT_N");
        ao_limit_south = IUFindLight(ao_limit, "AT_LIMIT_S");
        ao_limit_east  = IUFindLight(ao_limit, "AT_LIMIT_E");
        ao_limit_west  = IUFindLight(ao_limit, "AT_LIMIT_W");
    }

    CheckState();
}

bool StepGuiderSxAoINDI::Connect()
{
    if (INDIaoDeviceName == wxT("INDI SXV-AO-LF"))
    {
        SetupDialog(); // If not configured open the setup dialog
    }

    if (isServerConnected())
        return false;

    setServer(INDIhost.mb_str(wxConvUTF8), INDIport); // define server to connect to.
    watchDevice(INDIaoDeviceName.mb_str(wxConvUTF8)); // Receive messages only for our device.
    Debug.AddLine(wxString::Format("Connecting to INDI server %s on port %d, device %s", INDIhost.mb_str(wxConvUTF8), INDIport,
                                   INDIaoDeviceName.mb_str(wxConvUTF8)));
    Debug.Write(wxString::Format("Waiting for 30s for [%s] to connect...\n", INDIaoDeviceName));

    /* Wait in background for driver to establish a device connection */
    struct ConnectInBg : public ConnectAoInBg
    {
        StepGuiderSxAoINDI *ao;
        ConnectInBg(StepGuiderSxAoINDI *ao_) : ao(ao_) { }
        bool Entry()
        {
            // Wait for driver to establish a device connection
            if (ao->connectServer())
            {
                int i = 0;
                while (!ao->Connected && i++ < 300)
                {
                    if (IsCanceled())
                        break;

                    wxMilliSleep(100);
                }
            }

            // We need to return FALSE if we are successful???
            return !ao->Connected;
        }
    };

    return ConnectInBg(this).Run();
}

bool StepGuiderSxAoINDI::Disconnect()
{
    Debug.Write("StepGuiderSxAoINDI::Disconnect\n");
    disconnectServer();
    ClearStatus();
    StepGuider::Disconnect();
    return false;
}

bool StepGuiderSxAoINDI::HasSetupDialog() const
{
    return true;
}

void StepGuiderSxAoINDI::SetupDialog()
{
    // show the server and device configuration
    INDIConfig indiDlg(wxGetApp().GetTopWindow(), _("INDI AO Selection"), INDI_TYPE_AO);
    indiDlg.INDIhost = INDIhost;
    indiDlg.INDIport = INDIport;
    indiDlg.INDIDevName = INDIaoDeviceName;
    // initialize with actual values
    indiDlg.SetSettings();
    // try to connect to server
    indiDlg.Connect();
    if (indiDlg.ShowModal() == wxID_OK)
    {
        // if OK save the values to the current profile
        indiDlg.SaveSettings();
        INDIhost = indiDlg.INDIhost;
        INDIport = indiDlg.INDIport;
        INDIaoDeviceName = indiDlg.INDIDevName;
        pConfig->Profile.SetString("/indi/INDIhost", INDIhost);
        pConfig->Profile.SetLong("/indi/INDIport", INDIport);
        pConfig->Profile.SetString("/indi/INDIao", INDIaoDeviceName);
        m_Name = INDIaoDeviceName;
    }

    indiDlg.Disconnect();
}

void StepGuiderSxAoINDI::ShowPropertyDialog()
{
    SetupDialog();
}

void StepGuiderSxAoINDI::serverConnected()
{
    // wait for the DEVICE_PORT property
    //    modal = true;
    //    wxLongLong msec;
    //    msec = wxGetUTCTimeMillis();
    //    while (!ao_port && wxGetUTCTimeMillis() - msec < MaxDeviceInitWaitMilliSeconds)
    //    {
    //        wxMilliSleep(20);
    //        ::wxSafeYield();
    //    }
    //    // connect to the device
    //    connectDevice(INDIaoDeviceName.mb_str(wxConvUTF8));

    //    // wait for all defined properties in CheckState
    //    msec = wxGetUTCTimeMillis();
    //    while (modal && wxGetUTCTimeMillis() - msec < MaxDevicePropertiesWaitMilliSeconds)
    //    {
    //        wxMilliSleep(20);
    //        ::wxSafeYield();
    //    }
    //    modal = false; // even if CheckState still says no

    //    if (ready)
    //    {
    //        try
    //        {
    //            Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::serverConnected connecting StepGuider"));
    //            StepGuider::Connect();
    //        }
    //        catch (const wxString &Msg)
    //        {
    //            POSSIBLY_UNUSED(Msg);
    //        }
    //    }
    //    else
    //    {
    //        Disconnect();
    //    }
}

void StepGuiderSxAoINDI::serverDisconnected(int exit_code)
{
    // after disconnection we reset the connection status and the properties pointers
    ClearStatus();
    if (exit_code == -1)
    {
        // in case the connection is lost we must reset the client socket
        Disconnect();
        Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::serverDisconnected disconnecting StepGuider"));
        StepGuider::Disconnect();
    }
}

void StepGuiderSxAoINDI::removeDevice(INDI::BaseDevice dp)
{
    ClearStatus();
    Disconnect();
    StepGuider::Disconnect();
}

StepGuider::STEP_RESULT StepGuiderSxAoINDI::Step(GUIDE_DIRECTION direction, int steps)
{
    STEP_RESULT result = STEP_OK;

    try
    {
        if (!aoNS_prop || !aoWE_prop)
            throw ERROR_INFO("StepGuiderSxAO::step: missing ns or we property");

        switch (direction)
        {
            case NORTH:
                aoN_prop->value = steps;
                aoS_prop->value = 0;
                sendNewNumber(aoNS_prop);
                break;
            case SOUTH:
                aoN_prop->value = 0;
                aoS_prop->value = steps;
                sendNewNumber(aoNS_prop);
                break;
            case EAST:
                aoW_prop->value = 0;
                aoE_prop->value = steps;
                sendNewNumber(aoWE_prop);
                break;
            case WEST:
                aoW_prop->value = steps;
                aoE_prop->value = 0;
                sendNewNumber(aoWE_prop);
                break;
            default:
                throw ERROR_INFO("StepGuiderSxAO::step: invalid direction");
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        result = STEP_ERROR;
    }

    return result;
}

int StepGuiderSxAoINDI::MaxPosition(GUIDE_DIRECTION direction) const
{
    return m_maxSteps;
}

bool StepGuiderSxAoINDI::SetMaxPosition(int steps)
{
    Debug.Write(wxString::Format("StepGuiderSxAoINDI: setting max steps = %d\n", steps));
    m_maxSteps = steps;
    pConfig->Profile.SetInt("/stepguider/sxao/MaxSteps", m_maxSteps);
    return false;
}

bool StepGuiderSxAoINDI::IsAtLimit(GUIDE_DIRECTION direction, bool *isAtLimit)
{
    bool bError = false;
    if (ao_limit)
    {
        try
        {
            switch (direction)
            {
                case NORTH:
                    *isAtLimit = ao_limit_north->s == IPS_ALERT;
                    Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::IsAtLimit North"));
                    break;
                case SOUTH:
                    *isAtLimit = ao_limit_south->s == IPS_ALERT;
                    Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::IsAtLimit South"));
                    break;
                case EAST:
                    *isAtLimit = ao_limit_east->s == IPS_ALERT;
                    Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::IsAtLimit East"));
                    break;
                case WEST:
                    *isAtLimit = ao_limit_west->s == IPS_ALERT;
                    Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::IsAtLimit West"));
                    break;
                default:
                    throw ERROR_INFO("StepGuiderSxAoINDI::IsAtLimit: invalid direction");
                    break;
            }
        }
        catch (const wxString& Msg)
        {
            POSSIBLY_UNUSED(Msg);
            bError = true;
        }
    }
    else
    {
        Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::IsAtLimit called before we received any ao_limit"));
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAoINDI::FirmwareVersion(int *version)
{
    bool bError = false;
    if (ao_firmware)
    {
        try
        {
            *version = 0;
            for (int i = 0; i < 3; i++)
            {
                unsigned char ch = (ao_firmware->text)[1 + i];
                if (ch < '0' || ch > '9')
                {
                    throw ERROR_INFO("StepGuiderSxAO::firmwareVersion: invalid character");
                }
                *version *= 10;
                *version += ch - '0';
            }
            Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::Firmwareversion %u", *version));
        }
        catch (const wxString& Msg)
        {
            *version = -1;
            POSSIBLY_UNUSED(Msg);
            bError = true;
        }
    }
    else
    {
        bError = true;
    }
    return bError;
}

bool StepGuiderSxAoINDI::Center()
{
    Debug.AddLine(wxString::Format("StepGuiderSxAoINDI::Center"));
    if (aoCenterUnjam_prop)
    {
        aoCenter_prop->s = ISS_ON;
        aoUnjam_prop->s = ISS_OFF;
        sendNewSwitch(aoCenterUnjam_prop);
        // TODO: detect failure of center and try Unjam
        ZeroCurrentPosition();
        return false;
    }
    return true;
}

bool StepGuiderSxAoINDI::ST4HasGuideOutput()
{
    return true;
}

bool StepGuiderSxAoINDI::ST4HostConnected()
{
    return IsConnected();
}

bool StepGuiderSxAoINDI::ST4HasNonGuiMove()
{
    return true;
}

bool StepGuiderSxAoINDI::ST4PulseGuideScope(int direction, int duration)
{
    bool bError = true;
    if (pulseGuideNS_prop && pulseGuideWE_prop)
    {
        bError = false;
        try
        {
            switch (direction)
            {
                case NORTH:
                    pulseN_prop->value = duration;
                    pulseS_prop->value = 0;
                    sendNewNumber(pulseGuideNS_prop);
                    break;
                case SOUTH:
                    pulseN_prop->value = 0;
                    pulseS_prop->value = duration;
                    sendNewNumber(pulseGuideNS_prop);
                    break;
                case EAST:
                    pulseW_prop->value = 0;
                    pulseE_prop->value = duration;
                    sendNewNumber(pulseGuideWE_prop);
                    break;
                case WEST:
                    pulseW_prop->value = duration;
                    pulseE_prop->value = 0;
                    sendNewNumber(pulseGuideWE_prop);
                    break;
                default:
                    throw ERROR_INFO("StepGuiderSxAO::ST4PulseGuideScope: invalid direction");
                    break;
            }
        }
        catch (const wxString& Msg)
        {
            POSSIBLY_UNUSED(Msg);
            bError = true;
        }
    }

    return bError;
}

bool StepGuiderSxAoINDI::HasNonGuiMove()
{
    return true;
}

StepGuider *StepGuiderSxAoIndiFactory::MakeStepGuiderSxAoIndi()
{
    return new StepGuiderSxAoINDI();
}

#endif // #ifdef STEPGUIDER_SXAO_INDI
