/*
 *  cam_indi.cpp
 *  PHD Guiding
 *
 *  Created by Geoffrey Hausheer.
 *  Copyright (c) 2009 Geoffrey Hausheer
 *  Copyright (c) 2014 Patrick Chevalley
 *  Copyright (c) 2020 Andy Galasso
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

#ifdef INDI_CAMERA

#include "cam_indi.h"
#include "camera.h"
#include "config_indi.h"
#include "image_math.h"
#include "indi_gui.h"
#include <libindi/baseclient.h>

#include <libindi/basedevice.h>
#include <libindi/indiproperty.h>

class CapturedFrame
{
    public:
        void     *m_data;
        size_t   m_size;
        char     m_format[MAXINDIBLOBFMT];

        CapturedFrame()
        {
            m_data = nullptr;
            m_size = 0;
            m_format[0] = 0;
        }

        ~CapturedFrame()
        {
#ifdef INDI_SHARED_BLOB_SUPPORT
            IDSharedBlobFree(m_data);
#else
            free(m_data);
#endif
        }

        // Take ownership of this blob's data, so INDI won't overwrite/free the memory
        void steal(IBLOB *bp)
        {
            m_data = bp->blob;
            m_size = bp->size;
            strncpy(m_format, bp->format, MAXINDIBLOBFMT);

            bp->blob = nullptr;
            bp->size = 0;
        }
};

class CameraINDI : public GuideCamera, public INDI::BaseClient
{
    private:
        ISwitchVectorProperty *connection_prop;
        INumberVectorProperty *expose_prop;
        INumberVectorProperty *frame_prop;
        INumber               *frame_x;
        INumber               *frame_y;
        INumber               *frame_width;
        INumber               *frame_height;
        ISwitchVectorProperty *frame_type_prop;
        INumberVectorProperty *ccdinfo_prop;
        INumberVectorProperty *binning_prop;
        INumber               *binning_x;
        INumber               *binning_y;
        ISwitchVectorProperty *video_prop;
        ITextVectorProperty   *camera_port;
        INDI::BaseDevice      camera_device;
        INumberVectorProperty *pulseGuideNS_prop;
        INumber               *pulseN_prop;
        INumber               *pulseS_prop;
        INumberVectorProperty *pulseGuideEW_prop;
        INumber               *pulseE_prop;
        INumber               *pulseW_prop;

        wxMutex sync_lock;
        wxCondition sync_cond;
        bool guide_active;
        GuideAxis guide_active_axis;

        IndiGui  *m_gui;

        wxMutex  m_lastFrame_lock;
        wxCondition m_lastFrame_cond;
        CapturedFrame *m_lastFrame;

        usImage  *StackImg;
        int      StackFrames;
        volatile bool stacking; // TODO: use a wxCondition to signal completion
        bool     has_blob;
        bool     has_old_videoprop;
        bool     first_frame;
        volatile bool modal;
        bool     ready;
        wxByte   m_bitsPerPixel;
        double   PixSize;
        double   PixSizeX;
        double   PixSizeY;
        wxRect   m_maxSize;
        wxByte   m_curBinning;
        bool     HasBayer;
        long     INDIport;
        wxString INDIhost;
        wxString INDICameraName;
        long     INDICameraCCD;
        wxString INDICameraCCDCmd;
        wxString INDICameraBlobName;
        bool     INDICameraForceVideo;
        bool     INDICameraForceExposure;
        wxRect   m_roi;

        bool     ConnectToDriver(RunInBg *ctx);
        void     SetCCDdevice();
        void     ClearStatus();
        void     CheckState();
        void     CameraDialog();
        void     CameraSetup();
        bool     ReadFITS(CapturedFrame *cf, usImage& img, bool takeSubframe, const wxRect& subframe);
        bool     StackStream(CapturedFrame *cf);
        void     SendBinning();

        // Update the last frame, discarding any missed frame
        void updateLastFrame(IBLOB *bp);

        // Wait until a frame was acquired, for limited amout of time.
        // If non null is returned, caller is responsible for deletion
        CapturedFrame *waitFrame(unsigned long waitTime);

    protected:
        void newDevice(INDI::BaseDevice dp) override;
        void removeDevice(INDI::BaseDevice dp) override;
        void newProperty(INDI::Property property) override;
        void updateProperty(INDI::Property property) override;
        void removeProperty(INDI::Property property) override {}
        void newMessage(INDI::BaseDevice dp, int messageID) override;
        void serverConnected() override;
        void serverDisconnected(int exit_code) override;

    public:
        CameraINDI();
        ~CameraINDI();
        bool    Connect(const wxString& camId) override;
        bool    Disconnect() override;
        bool    HasNonGuiCapture() override;
        wxByte  BitsPerPixel() override;
        bool    GetDevicePixelSize(double *pixSize) override;
        void    ShowPropertyDialog() override;

        bool    Capture(int duration, usImage& img, int options, const wxRect& subframe) override;

        bool    ST4PulseGuideScope(int direction, int duration) override;
        bool    ST4HasNonGuiMove() override;
};

CameraINDI::CameraINDI()
    :
    sync_cond(sync_lock),
    m_lastFrame_cond(m_lastFrame_lock),
    m_gui(nullptr)
{
    m_lastFrame = nullptr;
    ClearStatus();
    // load the values from the current profile
    INDIhost = pConfig->Profile.GetString("/indi/INDIhost", _T("localhost"));
    INDIport = pConfig->Profile.GetLong("/indi/INDIport", 7624);
    INDICameraName = pConfig->Profile.GetString("/indi/INDIcam", _T("INDI Camera"));
    INDICameraCCD = pConfig->Profile.GetLong("/indi/INDIcam_ccd", 0);
    INDICameraForceVideo = pConfig->Profile.GetBoolean("/indi/INDIcam_forcevideo", false);
    INDICameraForceExposure = pConfig->Profile.GetBoolean("/indi/INDIcam_forceexposure", false);
    Name = wxString::Format("INDI Camera [%s]", INDICameraName);
    SetCCDdevice();
    PropertyDialogType = PROPDLG_ANY;
    HasSubframes = true;
    m_bitsPerPixel = 0;
    HasBayer = false;
}

CameraINDI::~CameraINDI()
{
    if (m_gui)
        IndiGui::DestroyIndiGui(&m_gui);

    disconnectServer();
}

void CameraINDI::ClearStatus()
{
    // reset properties pointer
    connection_prop = nullptr;
    expose_prop = nullptr;
    frame_prop = nullptr;
    frame_type_prop = nullptr;
    ccdinfo_prop = nullptr;
    binning_prop = nullptr;
    video_prop = nullptr;
    camera_port = nullptr;
    pulseGuideNS_prop = nullptr;
    pulseGuideEW_prop = nullptr;
    // reset connection status
    has_blob = false;
    Connected = false;
    ready = false;
    m_hasGuideOutput = false;
    PixSize = PixSizeX = PixSizeY = 0.0;

    updateLastFrame(nullptr);

    guide_active = false;
    sync_cond.Broadcast(); // just in case worker thread was blocked waiting for guide pulse to complete
}

CapturedFrame *CameraINDI::waitFrame(unsigned long waitTime)
{
    wxMutexLocker lck(m_lastFrame_lock);
    if (!m_lastFrame)
    {
        m_lastFrame_cond.WaitTimeout(waitTime);
    }

    if (m_lastFrame)
    {
        auto ret = m_lastFrame;
        m_lastFrame = nullptr;
        return ret;
    }

    return nullptr;
}

void CameraINDI::updateLastFrame(IBLOB *blob)
{
    bool notify = false;
    {
        wxMutexLocker lck(m_lastFrame_lock);
        if (m_lastFrame != nullptr)
        {
            delete m_lastFrame;
            m_lastFrame = nullptr;
        }
        if (blob)
        {
            m_lastFrame = new CapturedFrame();
            m_lastFrame->steal(blob);
            notify = true;
        }
    }
    if (notify)
    {
        Debug.Write(wxString::Format("lastFrame signaled Camera is ready\n"));
        m_lastFrame_cond.Broadcast();
    }
}

void CameraINDI::CheckState()
{
    // Check if the device has all the required properties for our usage.
    if (has_blob && camera_device && Connected && (expose_prop || video_prop))
    {
        if (!ready)
        {
            Debug.Write(wxString::Format("INDI Camera is ready\n"));
            ready = true;
            first_frame = true;
            if (modal)
                modal = false;
        }
    }
}

void CameraINDI::newDevice(INDI::BaseDevice dp)
{
    if (strcmp(dp.getDeviceName(), INDICameraName.mb_str(wxConvUTF8)) == 0)
    {
        // The camera object
        camera_device = dp;
    }
}

void CameraINDI::updateProperty(INDI::Property property)
{
    // we go here every time a Switch state change

    switch (property.getType())
    {
        case INDI_SWITCH:
        {
            auto svp = property.getSwitch();

            if (INDIConfig::Verbose())
                Debug.Write(wxString::Format("INDI Camera Received Switch: %s = %i\n", svp->name, svp->sp->s));

            if (strcmp(svp->name, "CONNECTION") == 0)
            {
                ISwitch *connectswitch = IUFindSwitch(svp, "CONNECT");
                if (connectswitch->s == ISS_ON)
                {
                    Connected = true;
                    CheckState();
                }
                else
                {
                    if (ready)
                    {
                        ClearStatus();

                        // call Disconnect in the main thread since that will
                        // want to join the INDI worker thread which is
                        // probably the current thread

                        PhdApp::ExecInMainThread(
                            [this]()
                        {
                            DisconnectWithAlert(_("INDI camera disconnected"), NO_RECONNECT);
                        });
                    }
                }
            }

        }
        break;
        case INDI_NUMBER:
        {
            auto nvp = property.getNumber();

            if (INDIConfig::Verbose())
            {
                if (strcmp(nvp->name, "CCD_EXPOSURE") == 0 )
                {
                    // rate limit this one, it's too noisy
                    static double s_lastval;
                    if (nvp->np->value > 0.0 && fabs(nvp->np->value - s_lastval) < 0.5)
                        return;
                    s_lastval = nvp->np->value;
                }
                std::ostringstream os;
                for (int i = 0; i < nvp->nnp; i++)
                {
                    if (i) os << ',';
                    os << nvp->np[i].name << ':' << nvp->np[i].value;
                }
                Debug.Write(wxString::Format("INDI Camera Received Number: %s = %s state = %s\n", nvp->name, os.str().c_str(),
                                             property.getStateAsString()));
            }

            if (nvp == ccdinfo_prop)
            {
                PixSize = IUFindNumber(ccdinfo_prop, "CCD_PIXEL_SIZE")->value;
                PixSizeX = IUFindNumber(ccdinfo_prop, "CCD_PIXEL_SIZE_X")->value;
                PixSizeY = IUFindNumber(ccdinfo_prop, "CCD_PIXEL_SIZE_Y")->value;
                m_maxSize.x = IUFindNumber(ccdinfo_prop, "CCD_MAX_X")->value;
                m_maxSize.y = IUFindNumber(ccdinfo_prop, "CCD_MAX_Y")->value;
                // defer defining FullSize since it is not simply derivable from max size and binning
                // no: FullSize = wxSize(m_maxSize.x / Binning, m_maxSize.y / Binning);
                m_bitsPerPixel = IUFindNumber(ccdinfo_prop, "CCD_BITSPERPIXEL")->value;
            }
            else if (nvp == binning_prop)
            {
                MaxBinning = wxMin(binning_x->max, binning_y->max);
                m_curBinning = wxMin(binning_x->value, binning_y->value);
                if (Binning > MaxBinning)
                    Binning = MaxBinning;
                // defer defining FullSize since it is not simply derivable from max size and binning
                // no: FullSize = wxSize(m_maxSize.x / Binning, m_maxSize.y / Binning);
            }
            else if (nvp == pulseGuideEW_prop || nvp == pulseGuideNS_prop)
            {
                bool notify = false;
                {
                    wxMutexLocker lck(sync_lock);
                    if (guide_active && nvp->s != IPS_BUSY &&
                            ((guide_active_axis == GUIDE_RA && nvp == pulseGuideEW_prop) ||
                             (guide_active_axis == GUIDE_DEC && nvp == pulseGuideNS_prop)))
                    {
                        guide_active = false;
                        notify = true;
                    }
                    else if (!guide_active && nvp->s == IPS_BUSY)
                    {
                        guide_active = true;
                        guide_active_axis = nvp == pulseGuideEW_prop ? GUIDE_RA : GUIDE_DEC;
                    }
                }
                if (notify)
                    sync_cond.Broadcast();
            }
        }
        break;
        case INDI_TEXT:
        {
            auto tvp = property.getText();

            if (INDIConfig::Verbose())
                Debug.Write(wxString::Format("INDI Camera Received Text: %s = %s\n", tvp->name, tvp->tp->text));

        }
        break;
        case INDI_BLOB:
        {
            // we go here every time a new blob is available
            // this is normally the image from the camera

            auto bvp = property.getBLOB();
            auto bp = bvp->at(0);

            if (INDIConfig::Verbose())
                Debug.Write(wxString::Format("INDI Camera Received BLOB %s len=%d size=%d\n", bp->name, bp->bloblen, bp->size));

            if (expose_prop && !INDICameraForceVideo)
            {
                if (bp->name == INDICameraBlobName)
                {
                    updateLastFrame(bp);
                }
            }
            else if (video_prop)
            {
                if (modal && !stacking)
                {
                    CapturedFrame cf;
                    cf.steal(bp);
                    StackStream(&cf);
                }
            }
        }
        break;
        default:
            break;
    }
}

void CameraINDI::newMessage(INDI::BaseDevice dp, int messageID)
{
    // we go here every time the camera driver send a message

    if (INDIConfig::Verbose())
        Debug.Write(wxString::Format("INDI Camera Received message: %s\n", dp.messageQueue(messageID)));
}

void CameraINDI::newProperty(INDI::Property property)
{
    // Here we receive a list of all the properties after the connection
    // Updated values are not received here but in the newTYPE() functions above.
    // We keep the vector for each interesting property to send some data later.

    wxString PropName(property.getName());
    auto Proptype = property.getType();

    if (INDIConfig::Verbose())
        Debug.Write(wxString::Format("INDI Camera Property: %s\n", property.getName()));

    if (Proptype == INDI_BLOB)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found BLOB property for %s %s\n", property.getDeviceName(), PropName));

        if (PropName == INDICameraBlobName)
        {
            has_blob = 1;
            // set option to receive blob and messages for the selected CCD
            setBLOBMode(B_ALSO, INDICameraName.mb_str(wxConvUTF8), INDICameraBlobName.mb_str(wxConvUTF8));

#ifdef INDI_SHARED_BLOB_SUPPORT
            // Allow faster mode provided we don't modify the blob content or free/realloc it
            enableDirectBlobAccess(INDICameraName.mb_str(wxConvUTF8), INDICameraBlobName.mb_str(wxConvUTF8));
#endif
        }
    }
    else if (PropName == INDICameraCCDCmd + "EXPOSURE" && Proptype == INDI_NUMBER)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found CCD_EXPOSURE for %s %s\n", property.getDeviceName(), PropName));

        expose_prop = property.getNumber();
    }
    else if (PropName == INDICameraCCDCmd + "FRAME" && Proptype == INDI_NUMBER)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found CCD_FRAME for %s %s\n", property.getDeviceName(), PropName));

        frame_prop = property.getNumber();
        frame_x = IUFindNumber(frame_prop, "X");
        frame_y = IUFindNumber(frame_prop, "Y");
        frame_width = IUFindNumber(frame_prop, "WIDTH");
        frame_height = IUFindNumber(frame_prop, "HEIGHT");
    }
    else if (PropName == INDICameraCCDCmd + "FRAME_TYPE" && Proptype == INDI_SWITCH)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found CCD_FRAME_TYPE for %s %s\n", property.getDeviceName(), PropName));

        frame_type_prop = property.getSwitch();
    }
    else if (PropName == INDICameraCCDCmd + "BINNING" && Proptype == INDI_NUMBER)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found CCD_BINNING for %s %s\n", property.getDeviceName(), PropName));

        binning_prop = property.getNumber();
        binning_x = IUFindNumber(binning_prop, "HOR_BIN");
        binning_y = IUFindNumber(binning_prop, "VER_BIN");
        updateProperty(property);
    }
    else if (PropName == INDICameraCCDCmd + "CFA" && Proptype == INDI_TEXT)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found CCD_CFA for %s %s\n", property.getDeviceName(), PropName));

        ITextVectorProperty *cfa_prop = property.getText();
        IText *cfa_type = IUFindText(cfa_prop, "CFA_TYPE");
        if (cfa_type && cfa_type->text && *cfa_type->text)
        {
            if (INDIConfig::Verbose())
                Debug.Write(wxString::Format("INDI Camera CFA_TYPE is %s\n", cfa_type->text));

            HasBayer = true;
        }
    }
    else if (PropName == INDICameraCCDCmd + "VIDEO_STREAM" && Proptype == INDI_SWITCH)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found Video %s %s\n", property.getDeviceName(), PropName));

        video_prop = property.getSwitch();
        has_old_videoprop = false;
    }
    else if (PropName == "VIDEO_STREAM" && Proptype == INDI_SWITCH)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found Video %s %s\n", property.getDeviceName(), PropName));

        video_prop = property.getSwitch();
        has_old_videoprop = true;
    }
    else if (PropName == "DEVICE_PORT" && Proptype == INDI_TEXT)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found device port for %s \n", property.getDeviceName()));

        camera_port = property.getText();
    }
    else if (PropName == "CONNECTION" && Proptype == INDI_SWITCH)
    {
        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Found CONNECTION for %s %s\n", property.getDeviceName(), PropName));

        // Check the value here in case the device is already connected
        connection_prop = property.getSwitch();
        ISwitch *connectswitch = IUFindSwitch(connection_prop, "CONNECT");
        Connected = (connectswitch->s == ISS_ON);
    }
    else if (PropName == "DRIVER_INFO" && Proptype == INDI_TEXT)
    {
        if (camera_device && (camera_device.getDriverInterface() & INDI::BaseDevice::GUIDER_INTERFACE))
            m_hasGuideOutput = true; // Device supports guiding

        if (!Connected)
            connectDevice(camera_device.getDeviceName());
    }
    else if (PropName == "TELESCOPE_TIMED_GUIDE_NS" && Proptype == INDI_NUMBER)
    {
        pulseGuideNS_prop = property.getNumber();
        pulseN_prop = IUFindNumber(pulseGuideNS_prop, "TIMED_GUIDE_N");
        pulseS_prop = IUFindNumber(pulseGuideNS_prop, "TIMED_GUIDE_S");
    }
    else if (PropName == "TELESCOPE_TIMED_GUIDE_WE" && Proptype == INDI_NUMBER)
    {
        pulseGuideEW_prop = property.getNumber();
        pulseW_prop = IUFindNumber(pulseGuideEW_prop, "TIMED_GUIDE_W");
        pulseE_prop = IUFindNumber(pulseGuideEW_prop, "TIMED_GUIDE_E");
    }
    else if (PropName == INDICameraCCDCmd + "INFO" && Proptype == INDI_NUMBER)
    {
        ccdinfo_prop = property.getNumber();
        updateProperty(property);
    }

    CheckState();
}

bool CameraINDI::Connect(const wxString& camId)
{
    // If not configured open the setup dialog
    if (INDICameraName == wxT("INDI Camera"))
    {
        CameraSetup();
    }

    if (isServerConnected())
        return false;

    Debug.Write(wxString::Format("INDI Camera connecting to device [%s]\n", INDICameraName));

    // define server to connect to.
    setServer(INDIhost.mb_str(wxConvUTF8), INDIport);

    // Receive messages only for our camera.
    watchDevice(INDICameraName.mb_str(wxConvUTF8));

    Debug.Write(wxString::Format("Waiting for 30s for [%s] to connect...\n", INDICameraName));

    /* Wait in background for driver to establish a device connection */
    struct ConnectInBg : public ConnectCameraInBg
    {
        CameraINDI *cam;
        ConnectInBg(CameraINDI *cam_) : cam(cam_) { }
        bool Entry()
        {
            // Wait for driver to establish a device connection
            if (cam->connectServer())
            {
                int i = 0;
                while (!cam->Connected && i++ < 300)
                {
                    if (IsCanceled())
                        break;

                    wxMilliSleep(100);
                }
            }

            // We need to return FALSE if we are successful
            return !cam->Connected;

        }
    };

    return ConnectInBg(this).Run();
}

wxByte CameraINDI::BitsPerPixel()
{
    return m_bitsPerPixel;
}

bool CameraINDI::Disconnect()
{
    // Disconnect from server (no-op if not connected)
    disconnectServer();
    return false;
}

bool CameraINDI::ConnectToDriver(RunInBg *r)
{
    modal = true;

    // wait for the device port property

    wxLongLong msec = wxGetUTCTimeMillis();

    // Connect the camera device
    while (!connection_prop && wxGetUTCTimeMillis() - msec < 15 * 1000)
    {
        if (r->IsCanceled())
        {
            modal = false;
            return false;
        }

        wxMilliSleep(20);
    }
    if (!connection_prop)
    {
        r->SetErrorMsg(_("connection timed-out"));
        modal = false;
        return false;
    }

    connectDevice(INDICameraName.mb_str(wxConvUTF8));

    msec = wxGetUTCTimeMillis();
    while (modal && wxGetUTCTimeMillis() - msec < 30 * 1000)
    {
        if (r->IsCanceled())
        {
            modal = false;
            return false;
        }

        wxMilliSleep(20);
    }
    if (!ready)
    {
        r->SetErrorMsg(_("Connection timed-out"));
    }

    modal = false;
    return ready;
}

void CameraINDI::serverConnected()
{
    // After connection to the server

    //    struct ConnectInBg : public ConnectCameraInBg
    //    {
    //        CameraINDI *cam;
    //        ConnectInBg(CameraINDI *cam_) : cam(cam_) { }
    //        bool Entry()
    //        {
    //            return !cam->ConnectToDriver(this);
    //        }
    //    };
    //    ConnectInBg bg(this);

    //    if (bg.Run())
    //    {
    //        Debug.Write(wxString::Format("INDI Camera bg connection failed canceled=%d\n", bg.IsCanceled()));
    //        CamConnectFailed(wxString::Format(_("Cannot connect to camera %s: %s"), INDICameraName, bg.GetErrorMsg()));
    //        Connected = false;
    //        Disconnect();
    //    }
    //    else
    //    {
    //        Debug.Write("INDI Camera bg connection succeeded\n");
    //        Connected = true;
    //    }
}

void CameraINDI::serverDisconnected(int exit_code)
{
    Debug.Write("INDI Camera: serverDisconnected\n");

    // after disconnection we reset the connection status and the properties pointers
    ClearStatus();

    // in case the connection lost we must reset the client socket
    if (exit_code == -1)
        DisconnectWithAlert(_("INDI server disconnected"), NO_RECONNECT);
}

void CameraINDI::removeDevice(INDI::BaseDevice dp)
{
    ClearStatus();
    DisconnectWithAlert(_("INDI camera disconnected"), NO_RECONNECT);
}

void CameraINDI::ShowPropertyDialog()
{
    if (Connected)
    {
        // show the devices INDI dialog
        CameraDialog();
    }
    else
    {
        // show the server and device configuration
        CameraSetup();
    }
}

void CameraINDI::CameraDialog()
{
    if (m_gui)
    {
        m_gui->Show();
    }
    else
    {
        IndiGui::ShowIndiGui(&m_gui, INDIhost, INDIport, false, false);
    }
}

void CameraINDI::CameraSetup()
{
    // show the server and device configuration
    INDIConfig indiDlg(wxGetApp().GetTopWindow(), _("INDI Camera Selection"), INDI_TYPE_CAMERA);
    indiDlg.INDIhost = INDIhost;
    indiDlg.INDIport = INDIport;
    indiDlg.INDIDevName = INDICameraName;
    indiDlg.INDIDevCCD = INDICameraCCD;
    indiDlg.INDIForceVideo = INDICameraForceVideo;
    indiDlg.INDIForceExposure = INDICameraForceExposure;
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
        INDICameraName = indiDlg.INDIDevName;
        INDICameraCCD = indiDlg.INDIDevCCD;
        INDICameraForceVideo = indiDlg.INDIForceVideo;
        INDICameraForceExposure = indiDlg.INDIForceExposure;
        pConfig->Profile.SetString("/indi/INDIhost", INDIhost);
        pConfig->Profile.SetLong("/indi/INDIport", INDIport);
        pConfig->Profile.SetString("/indi/INDIcam", INDICameraName);
        pConfig->Profile.SetLong("/indi/INDIcam_ccd", INDICameraCCD);
        pConfig->Profile.SetBoolean("/indi/INDIcam_forcevideo", INDICameraForceVideo);
        pConfig->Profile.SetBoolean("/indi/INDIcam_forceexposure", INDICameraForceExposure);
        Name = INDICameraName;
        SetCCDdevice();
    }
    indiDlg.Disconnect();
}

void CameraINDI::SetCCDdevice()
{
    if (INDICameraCCD == 0)
    {
        INDICameraBlobName = "CCD1";
        INDICameraCCDCmd = "CCD_";
    }
    else
    {
        INDICameraBlobName = "CCD2";
        INDICameraCCDCmd = "GUIDER_";
    }
}

bool CameraINDI::ReadFITS(CapturedFrame *frame, usImage& img, bool takeSubframe, const wxRect& subframe)
{
    fitsfile *fptr;  // FITS file pointer
    int status = 0;  // CFITSIO status value MUST be initialized to zero!

    // load blob to CFITSIO
    if (fits_open_memfile(&fptr,
                          "",
                          READONLY,
                          &frame->m_data,
                          &frame->m_size,
                          0,
                          nullptr,
                          &status))
    {
        pFrame->Alert(_("Unsupported type or read error loading FITS file"));
        return true;
    }

    int hdutype;
    if (fits_get_hdu_type(fptr, &hdutype, &status) || hdutype != IMAGE_HDU)
    {
        pFrame->Alert(_("FITS file is not of an image"));
        PHD_fits_close_file(fptr);
        return true;
    }

    // Get HDUs and size
    int naxis;
    fits_get_img_dim(fptr, &naxis, &status);

    long fits_size[2];
    fits_get_img_size(fptr, 2, fits_size, &status);
    int xsize = (int) fits_size[0];
    int ysize = (int) fits_size[1];

    int nhdus = 0;
    fits_get_num_hdus(fptr, &nhdus, &status);

    if (naxis == 3)
    {
        pFrame->Alert(_("RGB images are not supported, please switch the INDI driver to Mono"));
        PHD_fits_close_file(fptr);
        return true;
    }
    if (nhdus != 1 || naxis != 2)
    {
        pFrame->Alert(_("Unsupported FITS file"));
        PHD_fits_close_file(fptr);
        return true;
    }

    long fpixel[3] = {1, 1, 1};

    if (takeSubframe)
    {
        if (FullSize == UNDEFINED_FRAME_SIZE)
        {
            // should never happen since we arranged not to take a subframe
            // unless full frame size is known
            Debug.Write("internal error: taking subframe before full frame\n");
            PHD_fits_close_file(fptr);
            return true;
        }
        if (img.Init(FullSize))
        {
            pFrame->Alert(_("Memory allocation error"));
            PHD_fits_close_file(fptr);
            return true;
        }

        img.Clear();
        img.Subframe = subframe;

        unsigned short *rawdata = new unsigned short[xsize * ysize];

        if (fits_read_pix(fptr, TUSHORT, fpixel, xsize * ysize, nullptr, rawdata, nullptr, &status))
        {
            pFrame->Alert(_("Error reading data"));
            PHD_fits_close_file(fptr);
            delete[] rawdata;
            return true;
        }

        int i = 0;
        for (int y = 0; y < subframe.height; y++)
        {
            unsigned short *dataptr = img.ImageData + (y + subframe.y) * img.Size.GetWidth() + subframe.x;
            memcpy(dataptr, &rawdata[i], subframe.width * sizeof(unsigned short));
            i += subframe.width;
        }

        delete[] rawdata;
    }
    else
    {
        FullSize.Set(xsize, ysize);

        if (img.Init(FullSize))
        {
            pFrame->Alert(_("Memory allocation error"));
            PHD_fits_close_file(fptr);
            return true;
        }

        // Read image
        if (fits_read_pix(fptr, TUSHORT, fpixel, xsize * ysize, nullptr, img.ImageData, nullptr, &status))
        {
            pFrame->Alert(_("Error reading data"));
            PHD_fits_close_file(fptr);
            return true;
        }
    }

    PHD_fits_close_file(fptr);
    return false;
}

bool CameraINDI::StackStream(CapturedFrame *cf)
{
    if (!StackImg)
        return true;

    if (cf->m_size != StackImg->NPixels)
    {
        Debug.Write(wxString::Format("INDI Camera: discarding blob with size %d, expected %u\n", cf->m_size, StackImg->NPixels));
        return true;
    }

    // Add new blob to stacked image
    stacking = true;

    unsigned short *outptr = StackImg->ImageData;
    const unsigned char *inptr = (unsigned char *) cf->m_data;

    for (int i = 0; i < StackImg->NPixels; i++)
        *outptr++ += (unsigned short) *inptr++;

    ++StackFrames;

    stacking = false;

    return false;
}

void CameraINDI::SendBinning()
{
    binning_x->value = Binning;
    binning_y->value = Binning;
    Debug.Write(wxString::Format("INDI Camera: send binning %u\n", Binning));
    sendNewNumber(binning_prop);
    m_curBinning = Binning;
}

bool CameraINDI::Capture(int duration, usImage& img, int options, const wxRect& subframeArg)
{
    if (!Connected)
        return true;

    bool takeSubframe = UseSubframes;
    wxRect subframe(subframeArg);

    // we can set the exposure time directly in the camera
    if (expose_prop && !INDICameraForceVideo)
    {
        if (binning_prop && Binning != m_curBinning)
        {
            SendBinning();
            takeSubframe = false; // subframe may be out of bounds now
            if (Binning == 1)
                FullSize.Set(m_maxSize.x, m_maxSize.y);
            else
                FullSize = UNDEFINED_FRAME_SIZE; // we don't know the binned size until we get a frame
        }

        if (!frame_prop || subframe.width <= 0 || subframe.height <= 0)
        {
            takeSubframe = false;
        }

        if (takeSubframe && FullSize == UNDEFINED_FRAME_SIZE)
        {
            // if we do not know the full frame size, we cannot take a
            // subframe until we receive a full frame and get the frame size
            takeSubframe = false;
        }

        // Program the size
        if (!takeSubframe)
        {
            wxSize sz;
            if (FullSize != UNDEFINED_FRAME_SIZE)
            {
                // we know the actual frame size
                sz = FullSize;
            }
            else
            {
                // the max size divided by the binning may be larger than
                // the actual frame, but setting a larger size should
                // request the full binned frame which we want
                sz.Set(m_maxSize.x / Binning, m_maxSize.y / Binning);
            }
            subframe = wxRect(sz);
        }

        if (frame_prop && subframe != m_roi)
        {
            frame_x->value = subframe.x * Binning;
            frame_y->value = subframe.y * Binning;
            frame_width->value = subframe.width * Binning;
            frame_height->value = subframe.height * Binning;
            sendNewNumber(frame_prop);
            m_roi = subframe;
        }

        if (expose_prop->s == IPS_BUSY)
        {
            if (INDIConfig::Verbose())
                Debug.Write(wxString::Format("INDI Camera Exposure is busy. Waiting\n"));

            CameraWatchdog watchdog(duration, GetTimeoutMs());
            while (true)
            {
                wxMilliSleep(10);
                if (expose_prop->s != IPS_BUSY)
                    break;

                if (WorkerThread::TerminateRequested())
                    return true;

                if (watchdog.Expired())
                {
                    first_frame = false;
                    DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
                    return true;
                }
            }
        }

        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Exposing for %dms\n", duration));

        // Discard any "in between" frames...
        updateLastFrame(nullptr);

        // set the exposure time, this immediately start the exposure
        expose_prop->np->value = (double) duration / 1000;
        sendNewNumber(expose_prop);

        // responsiveness for ui. Frame availability will be notified immediately
        unsigned long loopwait = 100;

        CameraWatchdog watchdog(duration, GetTimeoutMs());

        CapturedFrame *frame = nullptr;
        while (!(frame = waitFrame(loopwait)))
        {
            if (WorkerThread::TerminateRequested())
                return true;
            if (watchdog.Expired())
            {
                if (first_frame && video_prop && !INDICameraForceExposure)
                {
                    // exposure fail, maybe this is a webcam with only streaming
                    // try to use video stream instead of exposure
                    // See: http://www.indilib.org/forum/ccds-dslrs/3078-v4l2-ccd-exposure-property.html
                    // TODO : check if an updated INDI v4l2 driver offer a better solution
                    pFrame->Alert(wxString::Format(_("Camera  %s, exposure error. Trying to use streaming instead."),
                                                   INDICameraName));
                    INDICameraForceVideo = true;
                    first_frame = false;
                    return Capture(duration, img,  options, subframeArg);
                }
                else
                {
                    first_frame = false;
                    DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
                    return true;
                }
            }
        }

        if (INDIConfig::Verbose())
            Debug.Write(wxString::Format("INDI Camera Exposure end\n"));

        first_frame = false;

        // exposure complete, process the file
        if (strcmp(frame->m_format, ".fits") == 0)
        {
            if (INDIConfig::Verbose())
                Debug.Write(wxString::Format("INDI Camera Processing fits file\n"));

            // for CCD camera
            if (!ReadFITS(frame, img, takeSubframe, subframe))
            {
                delete frame;
                if (options & CAPTURE_SUBTRACT_DARK)
                    SubtractDark(img);
                if (HasBayer && Binning == 1 && (options & CAPTURE_RECON))
                    QuickLRecon(img);
                if (options & CAPTURE_RECON)
                {
                    if (PixSizeX != PixSizeY)
                        SquarePixels(img, PixSizeX, PixSizeY);
                }
                return false;
            }
            delete frame;
            return true;
        }

        pFrame->Alert(wxString::Format(_("Unknown image format: %s"), wxString::FromAscii(frame->m_format)));
        delete frame;
        return true;
    }
    else if (video_prop)
    {
        // for a video camera without an exposure time setting we stack
        // frames for duration of the exposure

        first_frame = false;

        if (binning_prop && Binning != m_curBinning)
        {
            SendBinning();
        }

        // for video streaming we do not get the frame size so we have to
        // derive it from the full frame size and the binning

        FullSize.Set(m_maxSize.x / Binning, m_maxSize.y / Binning);

        if (img.Init(FullSize))
        {
            DisconnectWithAlert(CAPT_FAIL_MEMORY);
            return true;
        }

        img.Clear();
        StackImg = &img;

        // Find INDI switch
        ISwitch *v_on;
        ISwitch *v_off;
        if (has_old_videoprop)
        {
            v_on = IUFindSwitch(video_prop, "ON");
            v_off = IUFindSwitch(video_prop, "OFF");
        }
        else
        {
            v_on = IUFindSwitch(video_prop, "STREAM_ON");
            v_off = IUFindSwitch(video_prop, "STREAM_OFF");
        }

        // start streaming if not already active, every video frame is received as a blob
        if (v_on->s != ISS_ON)
        {
            v_on->s = ISS_ON;
            v_off->s = ISS_OFF;
            sendNewSwitch(video_prop);
        }

        modal = true;
        stacking = false;
        StackFrames = 0;

        unsigned long loopwait = duration > 100 ? 10 : 1;
        wxStopWatch swatch;
        swatch.Start();

        // wait the required time
        while (modal)
        {
            wxMilliSleep(loopwait);
            // test exposure complete
            if (swatch.Time() >= duration && StackFrames > 2)
                modal = false;
            // test termination request, stop streaming before to return
            if (WorkerThread::TerminateRequested())
                modal = false;
        }

        if (WorkerThread::StopRequested() ||  WorkerThread::TerminateRequested())
        {
            // Stop video streaming when Stop button is pressed or exiting the program
            v_on->s = ISS_OFF;
            v_off->s = ISS_ON;
            if (video_prop) // can get cleared asynchronously if server disconnects
                sendNewSwitch(video_prop);
        }

        if (WorkerThread::TerminateRequested())
            return true;

        // wait current frame is processed
        while (stacking)
        {
            wxMilliSleep(loopwait);
        }

        pFrame->StatusMsg(wxString::Format(_("%d frames"), StackFrames));

        if (options & CAPTURE_SUBTRACT_DARK)
            SubtractDark(img);

        return false;
    }
    else
    {
        // no capture property.
        wxString msg;
        if (INDICameraForceVideo)
            msg = _("Camera has no VIDEO_STREAM property, please uncheck the option: Camera does not support exposure time.");
        else if (INDICameraForceExposure)
            msg = _("Camera has no CCD_EXPOSURE property, please uncheck the option: Camera does not support stream.");
        else
            msg = _("Camera has no CCD_EXPOSURE or VIDEO_STREAM property");

        DisconnectWithAlert(msg, NO_RECONNECT);
        return true;
    }
}

bool CameraINDI::HasNonGuiCapture()
{
    return true;
}

// Camera ST4 port

bool CameraINDI::ST4HasNonGuiMove()
{
    return true;
}

bool CameraINDI::ST4PulseGuideScope(int direction, int duration)
{
    switch (direction)
    {
        case EAST:
        case WEST:
        case NORTH:
        case SOUTH:
            break;
        default:
            Debug.Write("INDI Camera error CameraINDI::Guide NONE\n");
            return true;
    }

    if (!pulseGuideNS_prop || !pulseGuideEW_prop)
    {
        Debug.Write("INDI Camera missing pulse guide properties!\n");
        return true;
    }

    // set guide active before initiating the pulse

    {
        wxMutexLocker lck(sync_lock);

        if (guide_active)
        {
            // todo: try to abort it?
            Debug.Write("Cannot guide with guide pulse in progress!\n");
            return true;
        }

        guide_active = true;
        guide_active_axis = direction == EAST || direction == WEST ? GUIDE_RA : GUIDE_DEC;

    } // lock scope

    switch (direction)
    {
        case EAST:
            pulseE_prop->value = duration;
            pulseW_prop->value = 0;
            sendNewNumber(pulseGuideEW_prop);
            break;
        case WEST:
            pulseE_prop->value = 0;
            pulseW_prop->value = duration;
            sendNewNumber(pulseGuideEW_prop);
            break;
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
    }

    if (INDIConfig::Verbose())
        Debug.Write("INDI Camera: wait for move complete\n");

    {
        // lock scope
        wxMutexLocker lck(sync_lock);
        while (guide_active)
        {
            sync_cond.WaitTimeout(100);
            if (WorkerThread::InterruptRequested())
            {
                Debug.Write("interrupt requested\n");
                return true;
            }
        }
    } // lock scope

    if (INDIConfig::Verbose())
        Debug.Write("INDI Camera: move completed\n");

    return false;
}

bool CameraINDI::GetDevicePixelSize(double *devPixelSize)
{
    if (!Connected)
        return true; // error

    *devPixelSize = PixSize;
    return false;
}

GuideCamera *INDICameraFactory::MakeINDICamera()
{
    return new CameraINDI();
}

#endif
