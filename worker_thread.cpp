/*
 *  worker_thread.cpp
 *  PHD Guiding
 *
 *  Created by Bret McKee
 *  Copyright (c) 2012 Bret McKee
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
 *    Neither the name of Bret McKee, Dad Dog Development,
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

WorkerThread::WorkerThread(MyFrame *pFrame)
    : wxThread(wxTHREAD_JOINABLE),
      m_interruptRequested(0),
      m_killable(true),
      m_skipSendExposeComplete(false)
{
    m_pFrame = pFrame;
    Debug.Write("WorkerThread constructor called\n");
}

WorkerThread::~WorkerThread(void)
{
    Debug.Write("WorkerThread destructor called\n");
}

void WorkerThread::EnqueueMessage(const WORKER_THREAD_REQUEST& message)
{
    wxMessageQueueError queueError;

    if (message.request == REQUEST_EXPOSE)
    {
        queueError = m_lowPriorityQueue.Post(message);
    }
    else
    {
        queueError = m_highPriorityQueue.Post(message);
    }

    assert(queueError == wxMSGQUEUE_NO_ERROR);

    queueError = m_wakeupQueue.Post(true);

    assert(queueError == wxMSGQUEUE_NO_ERROR);
}

/*************      Terminate      **************************/

void WorkerThread::EnqueueWorkerThreadTerminateRequest(void)
{
    m_interruptRequested = INT_STOP | INT_TERMINATE;

    WORKER_THREAD_REQUEST message;
    memset(&message, 0, sizeof(message));

    message.request = REQUEST_TERMINATE;
    EnqueueMessage(message);
}

/*************      Expose      **************************/

void WorkerThread::EnqueueWorkerThreadExposeRequest(usImage *pImage, int exposureDuration, int exposureOptions, const wxRect& subframe)
{
    m_interruptRequested &= ~INT_STOP;

    WORKER_THREAD_REQUEST message;
    memset(&message, 0, sizeof(message));

    Debug.Write("Enqueuing Expose request\n");

    message.request                      = REQUEST_EXPOSE;
    message.args.expose.pImage           = pImage;
    message.args.expose.exposureDuration = exposureDuration;
    message.args.expose.options          = exposureOptions;
    message.args.expose.subframe         = subframe;
    message.args.expose.pSemaphore       = 0;

    EnqueueMessage(message);
}

unsigned int WorkerThread::MilliSleep(int ms, unsigned int checkInterrupts)
{
    enum { MAX_SLEEP = 100 };

    if (ms <= MAX_SLEEP)
    {
        if (ms > 0)
            wxMilliSleep(ms);
        return WorkerThread::InterruptRequested() & checkInterrupts;
    }

    WorkerThread *thr = WorkerThread::This();
    wxStopWatch swatch;

    long elapsed = 0;
    do {
        wxMilliSleep(wxMin((long) ms - elapsed, (long) MAX_SLEEP));
        unsigned int val = thr ? (thr->m_interruptRequested & checkInterrupts) : 0;
        if (val)
            return val;
        elapsed = swatch.Time();
    } while (elapsed < ms);

    return 0;
}

void WorkerThread::SetSkipExposeComplete()
{
    Debug.Write("worker thread setting skip send exposure complete\n");
    m_skipSendExposeComplete = true;
}

//#define ENABLE_CAMERA_TEST
#ifdef ENABLE_CAMERA_TEST
static void
CameraROITest(usImage *img)
{
    // Overlay a simulated star that wanders around and periodically disappears.
    // This is used for testing new cameras to ensure that they deal properly with
    // dynamically changing subframes.
    static int ddx = 1, ddy = 1;
    static int dx, dy;
    int X = 250 + dx;
    int Y = 150 + dy;
    unsigned short base = (img->BitsPerPixel == 8 ? 255 : 60000);
    int scale = img->BitsPerPixel == 8 ? 5 : 5 * 256;
    if ((double)rand() / RAND_MAX > 0.05)
    {
        for (int x = -4; x <= 4; x++)
            for (int y = -4; y <= 4; y++)
                img->ImageData[X + x + (Y + y)*img->Size.x] = base - (x * x + y * y) * scale;
    }
    dx += ddx;
    if (dx < 0 || dx >= 48)
    {
        ddx = -ddx;
        dy += ddy;
        if (dy < 0 || dy >= 48)
            ddy = -ddy;
    }
}
#else
# define CameraROITest(img) do { } while (false)
#endif

bool WorkerThread::HandleExpose(EXPOSE_REQUEST *req)
{
    bool bError = false;

    try
    {
        if (WorkerThread::MilliSleep(m_pFrame->GetExposureDelay(), INT_ANY))
        {
            throw ERROR_INFO("Time lapse interrupted");
        }

        if (pCamera->HasNonGuiCapture())
        {
            Debug.Write(wxString::Format("Handling exposure in thread, d=%d o=%x r=(%d,%d,%d,%d)\n", req->exposureDuration,
                                         req->options, req->subframe.x, req->subframe.y, req->subframe.width, req->subframe.height));

            if (GuideCamera::Capture(pCamera, req->exposureDuration, *req->pImage, req->options, req->subframe))
            {
                throw ERROR_INFO("Capture failed");
            }
        }
        else
        {
            Debug.Write(wxString::Format("Handling exposure in myFrame, d=%d o=%x r=(%d,%d,%d,%d)\n", req->exposureDuration,
                                         req->options, req->subframe.x, req->subframe.y, req->subframe.width, req->subframe.height));

            wxSemaphore semaphore;
            req->pSemaphore = &semaphore;

            wxCommandEvent evt(REQUEST_EXPOSURE_EVENT, GetId());
            evt.SetClientData(req);
            wxQueueEvent(m_pFrame, evt.Clone());

            // wait for the request to complete
            req->pSemaphore->Wait();

            bError = req->error;
            req->pSemaphore = NULL;
        }

        Debug.Write("Exposure complete\n");

        if (!bError)
        {
            CameraROITest(req->pImage);

            switch (m_pFrame->GetNoiseReductionMethod())
            {
                case NR_NONE:
                    break;
                case NR_2x2MEAN:
                    QuickLRecon(*req->pImage);
                    break;
                case NR_3x3MEDIAN:
                    Median3(*req->pImage);
                    break;
            }

            req->pImage->CalcStats();
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void WorkerThread::SendWorkerThreadExposeComplete(usImage *pImage, bool bError)
{
    wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, MYFRAME_WORKER_THREAD_EXPOSE_COMPLETE);
    event->SetPayload<usImage *>(pImage);
    event->SetInt(bError);
    wxQueueEvent(m_pFrame, event);
}

/*************      Move       **************************/

void WorkerThread::EnqueueWorkerThreadMoveRequest(Mount *mount, const GuiderOffset& ofs, unsigned int moveOptions)
{
    m_interruptRequested &= ~INT_STOP;

    WORKER_THREAD_REQUEST message;
    memset(&message, 0, sizeof(message));

    Debug.Write(wxString::Format("Enqueuing Move request for %s (%.2f, %.2f)\n", mount->GetMountClassName(), ofs.cameraOfs.X, ofs.cameraOfs.Y));

    message.request                   = REQUEST_MOVE;
    message.args.move.mount           = mount;
    message.args.move.axisMove        = false;
    message.args.move.ofs             = ofs;
    message.args.move.moveOptions     = moveOptions;
    message.args.move.semaphore       = nullptr;

    EnqueueMessage(message);
}

void WorkerThread::EnqueueWorkerThreadAxisMove(Mount *mount, const GUIDE_DIRECTION direction, int duration, unsigned int moveOptions)
{
    m_interruptRequested &= ~INT_STOP;

    WORKER_THREAD_REQUEST message;
    memset(&message, 0, sizeof(message));

    Debug.Write(wxString::Format("Enqueuing Calibration Move request for direction %d\n", direction));

    message.request                   = REQUEST_MOVE;
    message.args.move.mount           = mount;
    message.args.move.axisMove        = true;
    message.args.move.direction       = direction;
    message.args.move.duration        = duration;
    message.args.move.moveOptions     = moveOptions;
    message.args.move.semaphore       = nullptr;

    EnqueueMessage(message);
}

void WorkerThread::HandleMove(MOVE_REQUEST *req)
{
    Mount::MOVE_RESULT result = Mount::MOVE_OK;

    try
    {
        if (req->mount->HasNonGuiMove())
        {
            if (req->axisMove)
            {
                Debug.Write(wxString::Format("Handling axis move in thread for %s dir=%d dur=%d\n",
                                             req->mount->GetMountClassName(),
                                             req->direction, req->duration));

                result = req->mount->MoveAxis(req->direction, req->duration, req->moveOptions);

                if (result != Mount::MOVE_OK)
                {
                    throw ERROR_INFO("MoveAxis failed");
                }
            }
            else
            {
                Debug.Write(wxString::Format("Handling offset move in thread for %s, endpoint = (%.2f, %.2f)\n",
                                             req->mount->GetMountClassName(),
                                             req->ofs.cameraOfs.X, req->ofs.cameraOfs.Y));

                result = req->mount->MoveOffset(&req->ofs, req->moveOptions);

                if (result != Mount::MOVE_OK)
                {
                    throw ERROR_INFO("Move failed");
                }
            }
        }
        else
        {
            // we don't have a non-gui guide function, so we send this to the
            // main frame routine that handles guides requests

            Debug.Write("Sending move to myFrame\n");

            wxSemaphore semaphore;
            req->semaphore = &semaphore;

            wxCommandEvent evt(REQUEST_MOUNT_MOVE_EVENT, GetId());
            evt.SetClientData(req);
            wxQueueEvent(m_pFrame, evt.Clone());

            // wait for the request to complete
            req->semaphore->Wait();

            req->semaphore = nullptr;
            result = req->moveResult;

            if (result != Mount::MOVE_OK)
            {
                throw ERROR_INFO("myFrame handled move failed");
            }
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        if (result == Mount::MOVE_OK)
            result = Mount::MOVE_ERROR;
    }

    Debug.Write(wxString::Format("move complete, result=%d\n", result));

    req->moveResult = result;
}

MoveCompleteEvent::MoveCompleteEvent(const MOVE_REQUEST& move)
    :
    wxThreadEvent(wxEVT_THREAD, MYFRAME_WORKER_THREAD_MOVE_COMPLETE),
    moveOptions(move.moveOptions),
    result(move.moveResult),
    mount(move.mount)
{
}

void WorkerThread::SendWorkerThreadMoveComplete(const MOVE_REQUEST& move)
{
    wxQueueEvent(m_pFrame, new MoveCompleteEvent(move));
}

/*
 * entry point for the background thread
 */
wxThread::ExitCode WorkerThread::Entry()
{
    bool bDone = TestDestroy();

    Debug.Write("WorkerThread::Entry() begins\n");

#if defined(__WINDOWS__)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    Debug.Write(wxString::Format("worker thread CoInitializeEx returns %x\n", hr));
#endif

    while (!bDone)
    {
        bool dummy;
        wxMessageQueueError queueError = m_wakeupQueue.Receive(dummy);

        Debug.Write("Worker thread wakes up\n");

        assert(queueError == wxMSGQUEUE_NO_ERROR);

        WORKER_THREAD_REQUEST message;
        queueError = m_highPriorityQueue.ReceiveTimeout(0, message);

        if (queueError == wxMSGQUEUE_TIMEOUT)
        {
            queueError = m_lowPriorityQueue.ReceiveTimeout(0, message);
            assert(queueError != wxMSGQUEUE_TIMEOUT);
        }

        assert(queueError == wxMSGQUEUE_NO_ERROR);

        switch (message.request)
        {
            bool bError;

            case REQUEST_TERMINATE:
                Debug.Write("worker thread servicing REQUEST_TERMINATE\n");
                bDone = true;
                break;

            case REQUEST_EXPOSE:
                Debug.Write(wxString::Format("worker thread servicing REQUEST_EXPOSE %d\n",
                    message.args.expose.exposureDuration));
                bError = HandleExpose(&message.args.expose);
                if (m_skipSendExposeComplete)
                {
                    Debug.Write("worker thread skipping SendWorkerThreadExposeComplete\n");
                    delete message.args.expose.pImage; // should be null though
                    message.args.expose.pImage = 0;
                    m_skipSendExposeComplete = false;
                }
                else
                    SendWorkerThreadExposeComplete(message.args.expose.pImage, bError);
                break;

            case REQUEST_MOVE: {
                if (message.args.move.axisMove)
                    Debug.Write(wxString::Format("worker thread servicing REQUEST_MOVE %s dir %s(%d) %d opts 0x%x\n",
                                                 message.args.move.mount->GetMountClassName(),
                                                 message.args.move.mount->DirectionChar(message.args.move.direction),
                                                 message.args.move.direction,
                                                 message.args.move.duration, message.args.move.moveOptions));
                else
                    Debug.Write(wxString::Format("worker thread servicing REQUEST_MOVE %s ofs (%.2f, %.2f) opts 0x%x\n",
                                                 message.args.move.mount->GetMountClassName(),
                                                 message.args.move.ofs.cameraOfs.X, message.args.move.ofs.cameraOfs.Y,
                                                 message.args.move.moveOptions));

                HandleMove(&message.args.move);
                SendWorkerThreadMoveComplete(message.args.move);
                break;
            }

            default:
                Debug.Write(wxString::Format("worker thread servicing unknown request %d\n", message.request));
                break;
        }

        Debug.Write("worker thread done servicing request\n");
        bDone |= TestDestroy();
    }

    Debug.Write("WorkerThread::Entry() ends\n");
    Debug.Flush();

    return (wxThread::ExitCode)0;
}
