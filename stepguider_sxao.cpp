/*
 *  stepguider_sxao.cpp
 *  PHD Guiding
 *
 *  Created by Bret McKee
 *  Copyright (c) 2013 Bret McKee
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
 *    Neither the name of Bret McKee, Dad Dog Development, nor the names of its
 *     Craig Stark, Stark Labs nor the names of its
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

#ifdef STEPGUIDER_SXAO

class StepGuiderSxAO : public StepGuider
{
    static const int DefaultMaxSteps = 45;
    static const int DefaultTimeout =  1 * 1000;
    static const int CenterTimeout  = 45 * 1000;

    wxString m_serialPortName;
    SerialPort *m_pSerialPort;
    int m_maxSteps;

  public:
    StepGuiderSxAO();
    virtual ~StepGuiderSxAO();

    bool Connect() override;
    bool Disconnect() override;

    bool HasNonGuiMove() override;

  private:
    bool Center() override;
    STEP_RESULT Step(GUIDE_DIRECTION direction, int steps) override;
    int MaxPosition(GUIDE_DIRECTION direction) const override;
    bool SetMaxPosition(int steps) override;
    bool IsAtLimit(GUIDE_DIRECTION direction, bool *isAtLimit) override;

    void ShowPropertyDialog() override;

    bool SendThenReceive(unsigned char sendChar, unsigned char *receivedChar);
    bool SendThenReceive(const unsigned char *pBuffer, unsigned int bufferSize, unsigned char *receivedChar);

    bool SendShortCommand(unsigned char command, unsigned char *response);
    bool SendLongCommand(unsigned char command, unsigned char parameter, unsigned count, unsigned char *response);

    bool FirmwareVersion(unsigned int *version);
    bool Center(unsigned char cmd);

    bool ST4HasGuideOutput() override;
    bool ST4HostConnected() override;
    bool ST4HasNonGuiMove() override;
    bool ST4PulseGuideScope(int direction, int duration) override;
};

StepGuiderSxAO::StepGuiderSxAO()
{
    m_Name = "SXV-AO";

#ifdef USE_LOOPBACK_SERIAL
    m_pSerialPort = new SerialPortLoopback();
#else
    m_pSerialPort = SerialPort::SerialPortFactory();
#endif

    m_serialPortName = pConfig->Profile.GetString("/stepguider/sxao/serialport", wxEmptyString);
    m_maxSteps = pConfig->Profile.GetInt("/stepguider/sxao/MaxSteps", DefaultMaxSteps);
}

StepGuiderSxAO::~StepGuiderSxAO()
{
    delete m_pSerialPort;
}

bool StepGuiderSxAO::Connect()
{
    bool bError = false;

    try
    {
        if (!m_pSerialPort)
        {
            throw ERROR_INFO("StepGuiderSxAO::Connect: serial port is NULL");
        }

        if (m_serialPortName.IsEmpty())
        {
            ShowPropertyDialog();
            if (m_serialPortName.IsEmpty())
                throw ERROR_INFO("StepGuiderSxAO::Connect: no serial port selected");
        }

        Debug.Write(wxString::Format("Connecting to SX AO on port %s\n", m_serialPortName));

        if (m_pSerialPort->Connect(m_serialPortName, 9600, 8, 1, SerialPort::ParityNone, false, false))
        {
            throw ERROR_INFO("StepGuiderSxAO::Connect: serial port connect failed");
        }

        wxYield();

        pConfig->Profile.SetString("/stepguider/sxao/serialport", m_serialPortName);

        if (m_pSerialPort->SetReceiveTimeout(DefaultTimeout))
        {
            throw ERROR_INFO("StepGuiderSxAO::Connect: SetReceiveTimeout failed");
        }

        wxYield();

        unsigned int version;

        if (FirmwareVersion(&version))
        {
            throw ERROR_INFO("StepGuiderSxAO::Connect: unable to get firmware version");
        }

        if (version == 0)
        {
            wxMessageBox(wxString::Format(
                _("This AO device has firmware version %03u which means it needs to be flashed.\n"
                  "It is recommended to load firmware version 101 or earlier.\n"
                  "The SXV-AO Utility v104 or newer, available at http://www.sxccd.com/drivers-downloads,\n"
                  "contains the v101 firmware."), version),
                  _("Error"));
            throw ERROR_INFO("StepGuiderSxAO::Connect: V000 means AO device needs a flash");
        }
        else if (version >= 102 && version <= 107)
        {
            bool confirmed = ConfirmDialog::Confirm(wxString::Format(
                _("This version of AO firmware (%03u) limits the travel range of the AO, and may cause\n"
                "calibration to fail. It is recommended to load firmware version 101 or earlier.\n"
                "The SXV-AO Utility v104 or newer, available at http://www.sxccd.com/drivers-downloads,\n"
                "contains the v101 firmware."), version) + "\n\n" + _("Would you like to proceed anyway?"),
                "/sx_ao_bad_firmware_ok");

            if (!confirmed)
                throw ERROR_INFO("StepGuiderSxAO::Connect: user cancelled after firmware version warning");
        }

        StepGuider::Connect();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void StepGuiderSxAO::ShowPropertyDialog()
{
    try
    {
        wxArrayString serialPorts;
        if (m_pSerialPort)
        {
            serialPorts = m_pSerialPort->GetSerialPortList();
        }

        if (serialPorts.IsEmpty())
        {
            wxMessageBox(_("No serial ports found"),_("Error"), wxOK | wxICON_ERROR);
            throw ERROR_INFO("No Serial ports found");
        }

        int resp = serialPorts.Index(m_serialPortName);

        resp = wxGetSingleChoiceIndex(_("Select serial port"),_("Serial Port"), serialPorts,
                NULL, wxDefaultCoord, wxDefaultCoord, true, wxCHOICE_WIDTH, wxCHOICE_HEIGHT,
                resp);
        if (resp == -1)
        {
            Debug.Write("Serial port selection canceled\n");
            return;
        }

        m_serialPortName = serialPorts[resp];
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        m_serialPortName = wxEmptyString;
    }
}

bool StepGuiderSxAO::Disconnect()
{
    if (StepGuider::Disconnect())
    {
        return true;
    }

    bool bError = false;

    try
    {
        if (m_pSerialPort && m_pSerialPort->Disconnect())
        {
            throw ERROR_INFO("StepGuiderSxAO::serial port disconnect failed");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAO::SendThenReceive(unsigned char sendChar, unsigned char *receivedChar)
{
    bool bError = false;

    try
    {
        if (m_pSerialPort->Send(&sendChar, 1))
        {
            throw ERROR_INFO("StepGuiderSxAO::SendThenReceive serial send failed");
        }

        if (m_pSerialPort->Receive(receivedChar, 1))
        {
            throw ERROR_INFO("StepGuiderSxAO::SendThenReceive serial receive failed");
        }
        Debug.Write(wxString::Format("StepGuiderSxAO::SendThenReceive sent %c received %c\n", sendChar, receivedChar));
    }
    catch (const wxString& Msg)
    {
        Debug.Write(wxString::Format("StepGuiderSxAO::SendThenReceive send unsigned char %c\n", sendChar));
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAO::SendThenReceive(const unsigned char *pBuffer, unsigned int bufferSize, unsigned char *receivedChar)
{
    bool bError = false;

    try
    {
        if (m_pSerialPort->Send(pBuffer, bufferSize))
        {
            throw ERROR_INFO("StepGuiderSxAO::SendThenReceive serial send failed");
        }

        if (m_pSerialPort->Receive(receivedChar, 1))
        {
            throw ERROR_INFO("StepGuiderSxAO::SendThenReceive serial receive failed");
        }
        Debug.AddBytes(wxString::Format("StepGuiderSxAO::SendThenReceive received %c, sent", *receivedChar), pBuffer, bufferSize);

        if (*receivedChar == 'W') // TODO: meaning
        {
            if (m_pSerialPort->Receive(receivedChar, 1))
            {
                throw ERROR_INFO("StepGuiderSxAO::step: Error reading another character after 'W'");
            }
        }
    }
    catch (const wxString& Msg)
    {
        Debug.AddBytes("StepGuiderSxAO::SendThenReceive send", pBuffer, bufferSize);
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAO::SendShortCommand(unsigned char command, unsigned char *response)
{
    bool bError = false;

    try
    {
        bError = SendThenReceive(command, response);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

/*
 * Long commands send 7 bytes to the AO. The first char
 * is the command, the second is the direction and the remaining
 * 5 characters are a count.
 */
bool StepGuiderSxAO::SendLongCommand(unsigned char command, unsigned char parameter, unsigned int count, unsigned char *response)
{
    bool bError = false;

    try
    {
        unsigned char cmdBuf[8]; // 7 chars + NULL

        if (count > 99999)
        {
            throw ERROR_INFO("StepGuiderSxAO::SendLongCommand invalid count");
        }
        int bufsize = sizeof(cmdBuf);
#if defined (__WINDOWS__)
        // MSVC-ism _snprintf returns a negative number if there is not enough space in the buffer
        int ret = _snprintf((char *)&cmdBuf[0], bufsize, "%c%c%5.5d", command, parameter, count);
#else
        // C99 snprintf returns the number of characters that the formatted string takes whether there was enough space in the buffer or not
        int ret = snprintf((char *)&cmdBuf[0], bufsize, "%c%c%5.5d", command, parameter, count);
#endif

        if (ret < 0)
        {
            throw ERROR_INFO("StepGuiderSxAO::SendLongCommand snprintf failed");
        }

        if (ret >= bufsize)
        {
            throw ERROR_INFO("StepGuiderSxAO::SendLongCommand snprintf buffer to small");
        }

        if (SendThenReceive(&cmdBuf[0], 7, response))
        {
            throw ERROR_INFO("StepGuiderSxAO::SendLongCommand SendThenReceive failed");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

/*
 * the firmwareVersion command is unique.  It sends 1 byte, and receives 3 digits
 * in response.
 */
bool StepGuiderSxAO::FirmwareVersion(unsigned int *version)
{
    bool bError = false;

    Debug.Write("StepGuiderSxAO::Firmwareversion\n");
    try
    {
        *version = 0;
        unsigned char cmd = 'V';
        unsigned char response;

        if (SendThenReceive(cmd, &response))
        {
            throw ERROR_INFO("StepGuiderSxAO::firmwareVersion: SendThenReceive failed");
        }

        if (response != cmd)
        {
            throw ERROR_INFO("StepGuiderSxAO::firmwareVersion: response != cmd");
        }

        wxMilliSleep(200); // often read just V instead of V123 without this sleep

        unsigned char buf[3];

        if (m_pSerialPort->Receive(&buf[0], sizeof(buf)))
        {
            throw ERROR_INFO("StepGuiderSxAO::firmwareVersion: Receive failed");
        }

        for (int i=0; i<3; i++)
        {
            unsigned char ch = buf[i];

            if (ch < '0' || ch > '9')
            {
                throw ERROR_INFO("StepGuiderSxAO::firmwareVersion: invalid character");
            }
            *version *= 10;
            *version += ch - '0';
        }
        Debug.Write(wxString::Format("StepGuiderSxAO::Firmwareversion %u\n", *version));
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAO::Center(unsigned char cmd)
{
    bool bError = false;

    Debug.Write(wxString::Format("StepGuiderSxAO::Center using command %c\n", cmd));
    try
    {
        unsigned char response;

        if (m_pSerialPort->SetReceiveTimeout(CenterTimeout))
        {
            throw ERROR_INFO("StepGuiderSxAO::Center: SetReceiveTimeout failed");
        }

        if (SendShortCommand(cmd, &response))
        {
            throw ERROR_INFO("StepGuiderSxAO::center SendShortCommand failed");
        }

        // there are two center commands and both return 'K'
        if (response != 'K')
        {
            throw ERROR_INFO("StepGuiderSxAO::center response != cmd");
        }

        if (m_pSerialPort->SetReceiveTimeout(DefaultTimeout))
        {
            throw ERROR_INFO("StepGuiderSxAO::Center: SetReceiveTimeout failed");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAO::Center()
{
    Debug.Write("StepGuiderSxAO::Center (K)\n");

    bool err = Center('K');

    if (err)
    {
        Debug.Write("StepGuiderSxAO::Center (R)\n");

        err = Center('R');
        if (err)
            Debug.Write("StepGuiderSxAO: Unable to center or Unjam\n");
    }

    if (!err)
        ZeroCurrentPosition();

    return err;
}

StepGuider::STEP_RESULT StepGuiderSxAO::Step(GUIDE_DIRECTION direction, int steps)
{
    STEP_RESULT result = STEP_OK;

    try
    {
        unsigned char cmd = 'G';
        unsigned char parameter;
        unsigned char response;

        switch (direction)
        {
            case NORTH:
                parameter = 'N';
                break;
            case SOUTH:
                parameter = 'S';
                break;
            case EAST:
                parameter = 'T';
                break;
            case WEST:
                parameter = 'W';
                break;
            default:
                throw ERROR_INFO("StepGuiderSxAO::step: invalid direction");
                break;
        }

        if (SendLongCommand(cmd, parameter, steps, &response))
        {
            throw ERROR_INFO("StepGuiderSxAO::step: SendLongCommand failed");
        }

        if (response == 'L')
        {
            result = STEP_LIMIT_REACHED;
            throw ERROR_INFO("StepGuiderSxAO::step: step: at limit");
        }

        if (response != cmd)
        {
            throw ERROR_INFO("StepGuiderSxAO::step: response != cmd");
        }

    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        if (result == STEP_OK)
            result = STEP_ERROR;
    }

    return result;
}

int StepGuiderSxAO::MaxPosition(GUIDE_DIRECTION direction) const
{
    return m_maxSteps;
}

bool StepGuiderSxAO::SetMaxPosition(int steps)
{
    Debug.Write(wxString::Format("SX-AO: setting max steps = %d\n", steps));
    m_maxSteps = steps;
    pConfig->Profile.SetInt("/stepguider/sxao/MaxSteps", m_maxSteps);
    return false;
}

bool StepGuiderSxAO::IsAtLimit(GUIDE_DIRECTION direction, bool *isAtLimit)
{
    bool bError = false;

    try
    {
        unsigned char cmd = 'L';
        unsigned char response;

        if (SendThenReceive(cmd, &response))
        {
            throw ERROR_INFO("StepGuiderSxAO::IsAtLimit: SendThenReceive failed");
        }

        if ((response & 0xf0) != 0x30)
        {
            throw ERROR_INFO("StepGuiderSxAO::IsAtLimit: invalid response ");
        }

        switch (direction)
        {
            case NORTH:
                *isAtLimit = (response & 0x1) == 0x1;
                break;
            case SOUTH:
                *isAtLimit = (response & 0x2) == 0x2;
                break;
            case EAST:
                *isAtLimit = (response & 0x4) == 0x4;
                break;
            case WEST:
                *isAtLimit = (response & 0x8) == 0x8;
                break;
            default:
                throw ERROR_INFO("StepGuiderSxAO::step: invalid direction");
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAO::ST4HasGuideOutput()
{
    return true;
}

bool StepGuiderSxAO::ST4HostConnected()
{
    return IsConnected();
}

bool StepGuiderSxAO::ST4HasNonGuiMove()
{
    return true;
}

bool StepGuiderSxAO::ST4PulseGuideScope(int direction, int duration)
{
    bool bError = false;

    try
    {
        char cmd = 'M';
        char parameter = 0;
        unsigned char response;

        switch (direction)
        {
            case NORTH:
                parameter = 'N';
                break;
            case SOUTH:
                parameter = 'S';
                break;
            case EAST:
                parameter = 'T';
                break;
            case WEST:
                parameter = 'W';
                break;
            default:
                throw ERROR_INFO("StepGuiderSxAO::ST4PulseGuideScope(): invalid direction");
                break;
        }

        if (SendLongCommand(cmd, parameter, duration, &response))
        {
            throw ERROR_INFO("StepGuiderSxAO::ST4PulseGuideScope(): SendLongCommand failed");
        }

        if (response != cmd)
        {
            throw ERROR_INFO("StepGuiderSxAO::ST4PulseGuideScope(): response != cmd");
        }

        // The Step function is asynchronous, and there is no way to wait for it, so we just
        // wait
        WorkerThread::MilliSleep(duration);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool StepGuiderSxAO::HasNonGuiMove()
{
    return true;
}

StepGuider *StepGuiderSxAoFactory::MakeStepGuiderSxAo()
{
    return new StepGuiderSxAO();
}

#endif // STEPGUIDER_SXAO
