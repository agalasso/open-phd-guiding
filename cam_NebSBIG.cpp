/*
 *  cam_NebSBIG.cpp
 *  PHD
 *
 *  Created by Craig Stark on 4/2/08.
 *  Copyright (c) 2006-2009 Craig Stark.
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

#ifdef NEB_SBIG
#include "camera.h"
#include "time.h"
#include "image_math.h"
#include "cam_NebSBIG.h"
#include "socket_server.h"

CameraNebSBIG::CameraNebSBIG()
{
    Connected = false;
    Name=_T("Nebulosity SBIG Guide chip");
}

wxByte CameraNebSBIG::BitsPerPixel()
{
    return 16;
}

bool CameraNebSBIG::Connect(const wxString& camId)
{
    int xsize, ysize;
    bool retval = ServerSendCamConnect(xsize, ysize);
    if (retval) return true;
    FullSize = wxSize(xsize,ysize);
    Connected = true;
    return false;
}

bool CameraNebSBIG::Disconnect()
{
    Connected = false;
    ServerSendCamDisconnect();
    return false;
}

bool CameraNebSBIG::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    if (img.Init(FullSize))
    {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }
    bool retval = ServerReqFrame(duration, img);
    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);

    return retval;

}

bool CameraNebSBIG::ST4PulseGuideScope(int direction, int duration)
{
    bool retval = ServerSendGuideCommand(direction, duration);
    return retval;
}

bool CameraNebSBIG::HasNonGuiCapture(void)
{
    return true;
}

#endif
