/*
 *  socket_server.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2008-2010 Craig Stark.
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

#include "socket_server.h"
#include "gear_simulator.h"

#include <algorithm>
#include <functional>

static std::set<wxSocketBase *> s_clients;

enum {
    MSG_PAUSE = 1,
    MSG_RESUME,
    MSG_MOVE1,
    MSG_MOVE2,
    MSG_MOVE3,
    MSG_IMAGE,
    MSG_GUIDE,
    MSG_CAMCONNECT,
    MSG_CAMDISCONNECT,
    MSG_REQDIST,
    MSG_REQFRAME,
    MSG_MOVE4,
    MSG_MOVE5,
    MSG_AUTOFINDSTAR,
    MSG_SETLOCKPOSITION,    //15
    MSG_FLIPRACAL,          //16
    MSG_GETSTATUS,          //17
    MSG_STOP,               //18
    MSG_LOOP,               //19
    MSG_STARTGUIDING,       //20
    MSG_LOOPFRAMECOUNT,     //21
    MSG_CLEARCAL,           //22
    MSG_FLIP_SIM_CAMERA,    //23
    MSG_DESELECT,           //24
};

void MyFrame::OnServerMenu(wxCommandEvent &evt)
{
    SetServerMode(evt.IsChecked());
    StartServer(GetServerMode());
}

bool MyFrame::StartServer(bool state)
{
    if (state)
    {
        if (SocketServer)
        {
            Debug.AddLine("start server, server already running");
            return false;
        }

        Debug.AddLine("starting server");

        int instanceNumber = wxGetApp().GetInstanceNumber();

        // Create the SocketServer socket
        unsigned int port = 4300 + instanceNumber - 1;
        wxIPV4address sockServerAddr;
        sockServerAddr.Service(port);
        SocketServer = new wxSocketServer(sockServerAddr, wxSOCKET_REUSEADDR);

        // We use Ok() here to see if the server is really listening
        if (!SocketServer->Ok())
        {
            Debug.AddLine(wxString::Format("Socket server failed to start - Could not listen at port %u", port));
            delete SocketServer;
            SocketServer = nullptr;
            StatusMsg(_("Server start failed"));
            return true;
        }
        SocketServer->SetEventHandler(*this, SOCK_SERVER_ID);
        SocketServer->SetNotify(wxSOCKET_CONNECTION_FLAG);
        SocketServer->Notify(true);

        // start the event server
        if (EvtServer.EventServerStart(instanceNumber))
        {
            delete SocketServer;
            SocketServer = nullptr;
            StatusMsg(_("Server start failed"));
            return true;
        }

        Debug.AddLine(wxString::Format("Server started, listening on port %u", port));
        StatusMsg(_("Server started"));
    }
    else {
        if (!SocketServer)
        {
            Debug.AddLine("stop server, server already stopped");
            return false;
        }

        Debug.AddLine("stopping server");
        std::for_each(s_clients.begin(), s_clients.end(), std::mem_fn(&wxSocketBase::Destroy));
        s_clients.clear();
        EvtServer.EventServerStop();
        delete SocketServer;
        SocketServer = nullptr;
        StatusMsg(_("Server stopped"));
    }

    return false;
}

void MyFrame::OnSockServerEvent(wxSocketEvent& event)
{
    wxSocketServer *server = static_cast<wxSocketServer *>(event.GetSocket());

    if (!server)
        return;

    if (event.GetSocketEvent() != wxSOCKET_CONNECTION)
    {
        Debug.AddLine(wxString::Format("socket server event expected %d, got %d. ignoring it.",
            wxSOCKET_CONNECTION, event.GetSocketEvent()));
        return;
    }

    wxSocketBase *client = server->Accept(false);

    if (client)
    {
        pFrame->StatusMsg(_("New server connection"));
        Debug.AddLine("SOCKSVR: New connection");
    }
    else
    {
        Debug.AddLine("SOCKSVR: connection error");
        return;
    }

    client->SetEventHandler(*this, SOCK_SERVER_CLIENT_ID);
    client->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
    client->Notify(true);

    s_clients.insert(client);
}

double MyFrame::GetDitherAmount(int ditherType)
{
    switch (ditherType) {
    case 1: return 0.5;
    case 2: return 1.0;
    case 3: return 2.0;
    case 4: return 3.0;
    case 5: return 5.0;
    }
    return 1.0;
}

void MyFrame::HandleSockServerInput(wxSocketBase *sock)
{
    unsigned char rval = 0;

    try
    {
        // We disable input events, so that the test doesn't trigger
        // wxSocketEvent again.
        sock->SetNotify(wxSOCKET_LOST_FLAG);

        // Which command is coming in?
        unsigned char c;

        sock->Read(&c, 1);

        Debug.Write(wxString::Format("read socket command %d\n", c));

        switch (c)
        {
            case MSG_PAUSE:
            case 'p':
                Debug.AddLine("processing socket request PAUSE");
                SetPaused(PAUSE_GUIDING);
                break;

            case MSG_RESUME:
            case 'r':
                Debug.AddLine("processing socket request RESUME");
                SetPaused(PAUSE_NONE);
                break;

            case MSG_MOVE1:  // +/- 0.5
            case MSG_MOVE2:  // +/- 1.0
            case MSG_MOVE3:  // +/- 2.0
            case MSG_MOVE4:  // +/- 3.0
            case MSG_MOVE5:  // +/- 5.0
            {
                Debug.AddLine("processing socket request MOVEn");

                int ditherType = 3;
                switch (c)
                {
                    case MSG_MOVE1: ditherType = 1; break;
                    case MSG_MOVE2: ditherType = 2; break;
                    case MSG_MOVE3: ditherType = 3; break;
                    case MSG_MOVE4: ditherType = 4; break;
                    case MSG_MOVE5: ditherType = 5; break;
                }

                double size = GetDitherAmount(ditherType);

                wxString errMsg;
                bool ok = PhdController::DitherCompat(size, &errMsg);
                if (!ok)
                {
                    throw ERROR_INFO(+errMsg);
                }

                rval = RequestedExposureDuration() / 1000;
                if (rval < 1)
                {
                    rval = 1;
                }
                break;
            }

            case MSG_REQDIST:
            {
                Debug.AddLine("processing socket request REQDIST");
                if (!pGuider->IsGuiding())
                {
                    throw ERROR_INFO("cannot request distance if not guiding");
                }

                double currentError = CurrentGuideError();

                if (currentError > 2.55)
                {
                    rval = 255;
                }
                else
                {
                    rval = (unsigned char) (currentError * 100);
                }

                Debug.AddLine(wxString::Format("SOCKSVR: Sending pixel error of %.2f", (float) rval / 100.0));
                break;
            }

            case MSG_AUTOFINDSTAR:
                Debug.AddLine("processing socket request AUTOFINDSTAR");
                bool error;
                error = pFrame->AutoSelectStar();
                rval = error ? 0 : 1;
                if (!error)
                {
                    StartLooping();
                }
                GuideLog.ServerCommand(pGuider, "AUTO FIND STAR");
                break;

            case MSG_SETLOCKPOSITION:
            {
                // Sets LockX and LockY to be user-specified
                unsigned short x,y;
                sock->Read(&x, 2);
                sock->Read(&y, 2);
                sock->Discard();  // Clean out anything else

                if (!pFrame->pGuider->SetLockPosToStarAtPosition(PHD_Point(x,y)))
                {
                    Debug.Write(wxString::Format("processing socket request SETLOCKPOSITION for (%d, %d) succeeded\n", x, y));
                    pFrame->StatusMsg(wxString::Format(_("Lock set to %d,%d"), x, y));
                    GuideLog.NotifySetLockPosition(pGuider);
                }
                else
                {
                    Debug.Write(wxString::Format("processing socket request SETLOCKPOSITION for (%d, %d) failed\n", x, y));
                }
                break;
            }

            case MSG_FLIPRACAL:
            {
                Debug.AddLine("processing socket request FLIPRACAL");
                PauseType prev = pGuider->SetPaused(PAUSE_GUIDING);
                // return 1 for success, 0 for failure
                rval = 1;
                if (FlipCalibrationData())
                {
                    rval = 0;
                }
                pGuider->SetPaused(prev);
                GuideLog.ServerCommand(pGuider, "FLIP RA CAL");
                break;
            }

            case MSG_GETSTATUS:
                Debug.AddLine("processing socket request GETSTATUS");
                rval = Guider::GetExposedState();
                break;

            case MSG_LOOP:
            {
                Debug.AddLine("processing socket request LOOP");
                StartLooping();
                GuideLog.ServerCommand(pGuider, "LOOP");
                break;
            }

            case MSG_STOP:
            {
                Debug.AddLine("processing socket request STOP");
                StopCapturing();
                GuideLog.ServerCommand(pGuider, "STOP");
                break;
            }

            case MSG_STARTGUIDING:
            {
                Debug.AddLine("processing socket request STARTGUIDING");
                bool err = StartGuiding();
                Debug.AddLine(wxString::Format("StartGuiding returned %d, guider state is %d", err, pFrame->pGuider->GetState()));
                GuideLog.ServerCommand(pGuider, "START GUIDING");
                break;
            }

            case MSG_LOOPFRAMECOUNT:
                Debug.AddLine("processing socket request LOOPFRAMECOUNT");
                if (!CaptureActive)
                {
                    rval = 0;
                }
                else if (m_frameCounter > UCHAR_MAX)
                {
                    rval = UCHAR_MAX;
                }
                else
                {
                    rval = m_frameCounter;
                }
                break;

            case MSG_CLEARCAL:
                Debug.AddLine("processing socket request CLEARCAL");

                if (pMount)
                    pMount->ClearCalibration();
                if (pSecondaryMount)
                    pSecondaryMount->ClearCalibration();

                GuideLog.ServerCommand(pGuider, "CLEAR CAL");
                break;

            case MSG_FLIP_SIM_CAMERA:
                Debug.AddLine("processing socket request flip camera simulator");
                GearSimulator::FlipPierSide(pCamera);
                break;

            case MSG_DESELECT:
                Debug.AddLine("processing socket request deselect");
                pFrame->pGuider->Reset(true);
                break;

            default:
                Debug.AddLine(wxString::Format("SOCKSVR: Unknown command char received from client: %d\n", (int) c));
                rval = 1;
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    Debug.Write(wxString::Format("Sending socket response %d (0x%x)\n", rval, rval));

    // Send response
    sock->Write(&rval,1);

    // Enable input events again.
    sock->SetNotify(wxSOCKET_LOST_FLAG | wxSOCKET_INPUT_FLAG);
}

void MyFrame::OnSockServerClientEvent(wxSocketEvent& event)
{
    try
    {
        wxSocketBase *sock = event.GetSocket();

        if (!SocketServer)
        {
            throw ERROR_INFO("socket command when SocketServer == NULL");
        }

        // Now we process the event
        switch (event.GetSocketEvent())
        {
            case wxSOCKET_INPUT:
                HandleSockServerInput(sock);
                break;
            case wxSOCKET_LOST:
                Debug.AddLine("SOCKSVR: Client disconnected, deleting socket");
                size_t n;
                n = s_clients.erase(sock);
                assert(n > 0);
                sock->Destroy();
                break;
            default:
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }
}

#ifdef NEB_SBIG

// this code only works when there is a single socket connection from Nebulosity

static int SocketConnections;
static wxSocketBase *ServerEndpoint;

bool ServerSendGuideCommand (int direction, int duration) {
    // Sends a guide command to Nebulosity
    if (!pFrame->SocketServer || !SocketConnections)
        return true;

    unsigned char cmd = MSG_GUIDE;
    unsigned char rval = 0;
    Debug.AddLine(wxString::Format("Sending guide: %d %d", direction, duration));
//  cmd = 'Z';
    ServerEndpoint->Write(&cmd, 1);
    if (pFrame->SocketServer->Error())
        Debug.AddLine(_T("Error sending Neb command"));
    else {
        Debug.AddLine(_T("Cmd done - sending data"));
        ServerEndpoint->Write(&direction, sizeof(int));
        ServerEndpoint->Write(&duration, sizeof(int));
        ServerEndpoint->Read(&rval,1);
        Debug.Write(wxString::Format("Sent guide command - returned %d\n", (int) rval));
    }
    return false;
}

bool ServerSendCamConnect(int& xsize, int& ysize) {
    if (!pFrame->SocketServer || !SocketConnections)
        return true;
    Debug.AddLine(_T("Sending cam connect request"));
    unsigned char cmd = MSG_CAMCONNECT;
    unsigned char rval = 0;

    ServerEndpoint->Write(&cmd, 1);
    if (pFrame->SocketServer->Error()) {
        Debug.AddLine(_T("Error sending Neb command"));
        return true;
    }
    else {
//      unsigned char c;
        ServerEndpoint->Read(&rval, 1);
        Debug.Write(wxString::Format("Cmd done - returned %d\n", (int) rval));
    }
    if (rval)
        return true;
    else {  // cam connected OK
        // Should get x and y size back
        ServerEndpoint->Read(&xsize,sizeof(int));
        ServerEndpoint->Read(&ysize,sizeof(int));
        Debug.Write(wxString::Format("Guide chip reported as %d x %d\n", xsize, ysize));
        return false;
    }
}

bool ServerSendCamDisconnect() {
    if (!pFrame->SocketServer || !SocketConnections)
        return true;

    Debug.AddLine(_T("Sending cam disconnect request"));
    unsigned char cmd = MSG_CAMDISCONNECT;
    unsigned char rval = 0;

    ServerEndpoint->Write(&cmd, 1);
    if (pFrame->SocketServer->Error()) {
        Debug.AddLine(_T("Error sending Neb command"));
        return true;
    }
    else {
//      unsigned char c;
        ServerEndpoint->Read(&rval, 1);
        Debug.Write(wxString::Format("Cmd done - returned %d\n", (int) rval));
    }
    if (rval)
        return true;
    else
        return false;  // cam disconnected OK
}

bool ServerReqFrame(int duration, usImage& img) {
    if (!pFrame->SocketServer || !SocketConnections)
        return true;
    Debug.AddLine(_T("Sending guide frame request"));
    unsigned char cmd = MSG_REQFRAME;
    unsigned char rval = 0;

    ServerEndpoint->Write(&cmd, 1);
    if (pFrame->SocketServer->Error()) {
        Debug.AddLine(_T("Error sending Neb command"));
        return true;
    }
    else {
//      unsigned char c;
        ServerEndpoint->Read(&rval, 1);
        Debug.Write(wxString::Format("Cmd done - returned %d\n", (int) rval));
    }
    if (rval)
        return true;
    else { // grab frame data
        // Send duration request
        ServerEndpoint->Write(&duration,sizeof(int));
        Debug.Write(wxString::Format("Starting %d ms frame\n", duration));
        wxMilliSleep(duration); // might as well wait here nicely at least this long
        Debug.Write(wxString::Format("Reading frame - looking for %u pixels (%u bytes)\n", img.NPixels, img.NPixels * 2));
        unsigned short *dataptr;
        dataptr = img.ImageData;
        unsigned short buffer[512];
        int i, xsize, ysize, pixels_left, packet_size;
        xsize = img.Size.GetWidth();
        ysize = img.Size.GetHeight();
        pixels_left = img.NPixels;
        packet_size = 256;  // # pixels to get
        int j=0;
        while (pixels_left > 0) {
            ServerEndpoint->Read(&buffer,packet_size * 2);
            pixels_left -= packet_size;
            if ((j % 100) == 0)
                Debug.Write(wxString::Format("%d left\n", pixels_left));
            for (i=0; i<packet_size; i++, dataptr++)
                *dataptr = buffer[i];
            if (pixels_left < 256)
                packet_size = 256;
            ServerEndpoint->Write(&cmd,1);
            j++;
        }
        int min = 9999999;
        int max = -999999;
        dataptr = img.ImageData;
        for (i=0; i<(xsize * ysize); i++, dataptr++) {
            if (*dataptr > max) max = (int) *dataptr;
            else if (*dataptr < min) min = (int) *dataptr;
        }
        Debug.Write(wxString::Format("Frame received min=%d max=%d\n", min, max));

//      ServerEndpoint->ReadMsg(img.ImageData,(xsize * ysize * 2));
        Debug.AddLine("Frame read");
    }

    return false;
}

#endif // NEB_SBIG
