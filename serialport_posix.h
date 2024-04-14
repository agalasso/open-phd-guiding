/*
 *  serialport_posix.h
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

#if !defined(SERIALPORT_POSIX_H_INCLUDED)
#define SERIALPORT_POSIX_H_INCLUDED

#if defined (__linux__) || defined (__APPLE__) || defined (__FreeBSD__)

#include <termios.h>

class SerialPortPosix : public SerialPort
{
    int m_fd;
#if defined (__APPLE__)
    struct termios m_originalAttrs;
#endif

public:

    wxArrayString GetSerialPortList() override;

    SerialPortPosix();
    virtual ~SerialPortPosix();

    bool Connect(const wxString& portName, int baud, int dataBits, int stopBits, PARITY Parity, bool useRTS, bool useDTR) override;
    bool Disconnect() override;

    bool Send(const unsigned char *pData, unsigned count) override;

    bool SetReceiveTimeout(int timeoutMilliSeconds) override;
    bool Receive(unsigned char *pData, unsigned count) override;

    bool SetRTS(bool asserted) override;
    bool SetDTR(bool asserted) override;
};

#endif // __linux__ || __APPLE__

#endif // SERIALPORT_POSIX_H_INCLUDED
