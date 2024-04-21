/*
*  cam_svb.cpp
*  PHD2 Guiding
*
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

#ifdef SVB_CAMERA

#include "cam_svb.h"
#include "cameras/SVBCameraSDK.h"

#ifdef __WINDOWS__
# include <Shlwapi.h>
# include <DelayImp.h>
#endif

enum CaptureMode
{
    CM_SNAP,
    CM_VIDEO,
};

class SVBCamera : public GuideCamera
{
    wxRect m_maxSize;
    wxRect m_frame;
    unsigned short m_prevBinning;
    void *m_buffer;
    size_t m_buffer_size;
    wxByte m_bpp;  // bits per pixel: 8 or 16
    CaptureMode m_mode;
    bool m_capturing;
    int m_cameraId;
    int m_minGain;
    int m_maxGain;
    int m_defaultGainPct;
    bool m_isColor;
    double m_devicePixelSize;

public:
    SVBCamera();
    ~SVBCamera();

    bool CanSelectCamera() const override { return true; }
    bool EnumCameras(wxArrayString& names, wxArrayString& ids) override;
    bool Capture(int duration, usImage& img, int options, const wxRect& subframe) override;
    bool Connect(const wxString& camId) override;
    bool Disconnect() override;

    bool ST4PulseGuideScope(int direction, int duration) override;

    void ShowPropertyDialog() override;
    bool HasNonGuiCapture() override { return true; }
    bool ST4HasNonGuiMove() override { return true; }
    wxByte BitsPerPixel() override;
    bool GetDevicePixelSize(double *devPixelSize) override;
    int GetDefaultCameraGain() override;

private:
    void StopCapture();
    bool StopExposure();
};

SVBCamera::SVBCamera()
    :
    m_buffer(nullptr)
{
    Name = _T("Svbony Camera");
    PropertyDialogType = PROPDLG_WHEN_DISCONNECTED;
    Connected = false;
    m_hasGuideOutput = false; // updated when connected
    HasSubframes = true;
    HasGainControl = true; // workaround: ok to set to false later, but brain dialog will crash if we start false then change to true later when the camera is connected
    m_defaultGainPct = GuideCamera::GetDefaultCameraGain();
    int value = pConfig->Profile.GetInt("/camera/svb/bpp", 16);
    m_bpp = value == 8 ? 8 : 16;
}

SVBCamera::~SVBCamera()
{
    ::free(m_buffer);
}

wxByte SVBCamera::BitsPerPixel()
{
    return m_bpp;
}

struct SVBCameraDlg : public wxDialog
{
    wxRadioButton *m_bpp8;
    wxRadioButton *m_bpp16;
    SVBCameraDlg();
};

SVBCameraDlg::SVBCameraDlg()
    : wxDialog(wxGetApp().GetTopWindow(), wxID_ANY, _("Svbony Camera Properties"))
{
    SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer *bSizer12 = new wxBoxSizer(wxVERTICAL);
    wxStaticBoxSizer *sbSizer3 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Camera Mode")), wxHORIZONTAL);

    m_bpp8 = new wxRadioButton(this, wxID_ANY, _("8-bit"));
    m_bpp16 = new wxRadioButton(this, wxID_ANY, _("16-bit"));
    sbSizer3->Add(m_bpp8, 0, wxALL, 5);
    sbSizer3->Add(m_bpp16, 0, wxALL, 5);
    bSizer12->Add(sbSizer3, 1, wxEXPAND, 5);

    wxStdDialogButtonSizer *sdbSizer2 = new wxStdDialogButtonSizer();
    wxButton *sdbSizer2OK = new wxButton(this, wxID_OK);
    wxButton* sdbSizer2Cancel = new wxButton(this, wxID_CANCEL);
    sdbSizer2->AddButton(sdbSizer2OK);
    sdbSizer2->AddButton(sdbSizer2Cancel);
    sdbSizer2->Realize();
    bSizer12->Add(sdbSizer2, 0, wxALL | wxEXPAND, 5);

    SetSizer(bSizer12);
    Layout();
    Fit();

    Centre(wxBOTH);
}

void SVBCamera::ShowPropertyDialog()
{
    SVBCameraDlg dlg;
    int value = pConfig->Profile.GetInt("/camera/svb/bpp", m_bpp);
    if (value == 8)
        dlg.m_bpp8->SetValue(true);
    else
        dlg.m_bpp16->SetValue(true);
    if (dlg.ShowModal() == wxID_OK)
    {
        m_bpp = dlg.m_bpp8->GetValue() ? 8 : 16;
        pConfig->Profile.SetInt("/camera/svb/bpp", m_bpp);
    }
}

inline static int cam_gain(int minval, int maxval, int pct)
{
    return minval + pct * (maxval - minval) / 100;
}

inline static int gain_pct(int minval, int maxval, int val)
{
    return (val - minval) * 100 / (maxval - minval);
}

static bool TryLoadDll(wxString *err)
{
    static bool s_logged;
    if (!s_logged)
    {
        const char *ver = SVBGetSDKVersion();
        Debug.Write(wxString::Format("SVB: SDK Version = [%s]\n", ver));
        s_logged = true;
    }

    return true;
}

bool SVBCamera::EnumCameras(wxArrayString& names, wxArrayString& ids)
{
    wxString err;
    if (!TryLoadDll(&err))
    {
        wxMessageBox(err, _("Error"), wxOK | wxICON_ERROR);
        return true;
    }

    // Find available cameras
    int numCameras = SVBGetNumOfConnectedCameras();

    for (int i = 0; i < numCameras; i++)
    {
        SVB_CAMERA_INFO info;
        if (SVBGetCameraInfo(&info, i) == SVB_SUCCESS)
        {
            if (numCameras > 1)
                names.Add(wxString::Format("%d: %s S/N %s", i + 1, info.FriendlyName, info.CameraSN));
            else
                names.Add(wxString::Format("%s S/N %s", info.FriendlyName, info.CameraSN));
            ids.Add(info.CameraSN);
        }
    }

    return false;
}

static int FindCamera(const wxString& camId, wxString *err)
{
    int numCameras = SVBGetNumOfConnectedCameras();

    Debug.Write(wxString::Format("SVB: find camera id: [%s], ncams = %d\n", camId, numCameras));

    if (numCameras <= 0)
    {
        *err = _("No Svbony cameras detected.");
        return -1;
    }

    if (camId == GuideCamera::DEFAULT_CAMERA_ID)
    {
        // camera id specified, connect to the first camera
        return 0;
    }

    // find the camera with the matching serial number

    for (int i = 0; i < numCameras; i++)
    {
        SVB_CAMERA_INFO info;
        if (SVBGetCameraInfo(&info, i) == SVB_SUCCESS)
        {
            Debug.Write(wxString::Format("SVB: cam [%d] id %d %s S/N %s\n", i, info.CameraID, info.FriendlyName, info.CameraSN));
            if (info.CameraSN == camId)
            {
                Debug.Write(wxString::Format("SVB: found matching camera at idx %d, id=%d\n", i, info.CameraID));
                return i;
            }
        }
    }

    Debug.Write("SVB: no matching cameras\n");
    *err = wxString::Format(_("Camera %s not found"), camId);
    return -1;
}

inline static int ImgTypeBits(SVB_IMG_TYPE t)
{
    switch (t) {
    case SVB_IMG_RAW8:
    case SVB_IMG_Y8:
        return 8;
    case SVB_IMG_RAW10:
    case SVB_IMG_Y10:
        return 10;
    case SVB_IMG_RAW12:
    case SVB_IMG_Y12:
        return 12;
    case SVB_IMG_RAW14:
    case SVB_IMG_Y14:
        return 14;
    case SVB_IMG_RAW16:
    case SVB_IMG_Y16:
        return 16;
    default:
        return -1;
    }
}

bool SVBCamera::Connect(const wxString& camId)
{
    wxString err;
    if (!TryLoadDll(&err))
    {
        return CamConnectFailed(err);
    }

    int selected = FindCamera(camId, &err);
    if (selected == -1)
    {
        return CamConnectFailed(err);
    }

    SVB_ERROR_CODE r;
    SVB_CAMERA_INFO info;
    if ((r = SVBGetCameraInfo(&info, selected)) != SVB_SUCCESS)
    {
        Debug.Write(wxString::Format("SVBGetCameraInfo ret %d\n", r));
        return CamConnectFailed(_("Failed to get camera info for Svbony camera."));
    }

    m_cameraId = info.CameraID;

    if ((r = SVBOpenCamera(m_cameraId)) != SVB_SUCCESS)
    {
        Debug.Write(wxString::Format("SVBOpenCamera ret %d\n", r));
        return CamConnectFailed(_("Failed to open Svbony camera."));
    }

    SVB_CAMERA_PROPERTY props;
    if ((r = SVBGetCameraProperty(m_cameraId, &props)) != SVB_SUCCESS)
    {
        Disconnect();
        Debug.Write(wxString::Format("SVBGetCameraProperty ret %d\n", r));
        return CamConnectFailed(_("Failed to get camera properties for Svbony camera."));
    }

    Debug.Write(wxString::Format("SVB: name = [%s] SN = [%s]\n", info.FriendlyName, info.CameraSN));

    // find the best image type matching our bpp selection
    SVB_IMG_TYPE img_type = SVB_IMG_END;
    int maxbits = -1;
    for (int i = 0; i < WXSIZEOF(props.SupportedVideoFormat); i++)
    {
        SVB_IMG_TYPE fmt = props.SupportedVideoFormat[i];
        if (fmt == SVB_IMG_END)
            break;
        int bits = ImgTypeBits(fmt);
        if (m_bpp == 8)
        {
            if (bits == 8)
            {
                img_type = fmt;
                break;
            }
        }
        else
        {
            if (bits > 8 && bits <= 16 && bits > maxbits)
            {
                maxbits = bits;
                img_type = fmt;
            }
        }
    }

    Debug.Write(wxString::Format("SVB: using mode BPP = %u, image type %d\n", (unsigned int)m_bpp, img_type));

    if (img_type == SVB_IMG_END)
    {
        Disconnect();
        return CamConnectFailed(wxString::Format(_("The camera does not support %s mode, try selecting %s mode"),
            m_bpp == 8 ? _("8-bit") : _("16-bit"),
            m_bpp == 8 ? _("16-bit") : _("8-bit")));
    }

    m_mode = CM_VIDEO;

    if (props.IsTriggerCam)
    {
        SVB_SUPPORTED_MODE sm;
        SVBGetCameraSupportMode(m_cameraId, &sm);
        for (int i = 0; i < sizeof(sm.SupportedCameraMode) / sizeof(sm.SupportedCameraMode[0]); i++)
        {
            if (sm.SupportedCameraMode[i] == SVB_MODE_END)
                break;
            if (sm.SupportedCameraMode[i] == SVB_MODE_TRIG_SOFT)
            {
                m_mode = CM_SNAP;
                break;
            }
        }
    }

    if (m_mode == CM_SNAP)
    {
        Debug.Write("SVB: selecting trigger mode\n");
        if ((r = SVBSetCameraMode(m_cameraId, SVB_MODE_TRIG_SOFT)) != SVB_SUCCESS)
        {
            Debug.Write(wxString::Format("SVBSetCameraMode(SVB_MODE_TRIG_SOFT) ret %d\n", r));
            // fall-back to video mode
            m_mode = CM_VIDEO;
        }
    }
    if (m_mode == CM_VIDEO)
    {
        Debug.Write("SVB: selecting video mode\n");
        if ((r = SVBSetCameraMode(m_cameraId, SVB_MODE_NORMAL)) != SVB_SUCCESS)
        {
            Debug.Write(wxString::Format("SVBSetCameraMode(SVB_MODE_NORMAL) ret %d\n", r));
            Disconnect();
            return CamConnectFailed(_("Unable to initialize camera."));
        }
    }

    Connected = true;
    Name = info.FriendlyName;
    m_isColor = props.IsColorCam != SVB_FALSE;
    Debug.Write(wxString::Format("SVB: IsColorCam = %d\n", m_isColor));

    HasShutter = false;

    int maxBin = 1;
    for (int i = 0; i <= WXSIZEOF(props.SupportedBins); i++)
    {
        if (!props.SupportedBins[i])
            break;
        Debug.Write(wxString::Format("SVB: supported bin %d = %d\n", i, props.SupportedBins[i]));
        if (props.SupportedBins[i] > maxBin)
            maxBin = props.SupportedBins[i];
    }
    MaxBinning = maxBin;

    if (Binning > MaxBinning)
        Binning = MaxBinning;

    m_maxSize.x = props.MaxWidth;
    m_maxSize.y = props.MaxHeight;

    FullSize.x = m_maxSize.x / Binning;
    FullSize.y = m_maxSize.y / Binning;
    m_prevBinning = Binning;

    ::free(m_buffer);
    m_buffer_size = props.MaxWidth * props.MaxHeight * (m_bpp == 8 ? 1 : 2);
    m_buffer = ::malloc(m_buffer_size);

    float pxsize;
    if ((r = SVBGetSensorPixelSize(m_cameraId, &pxsize)) == SVB_SUCCESS)
        m_devicePixelSize = pxsize;
    else
        Debug.Write(wxString::Format("SVBGetSensorPixelSize ret %d\n", r));

    SVBStopVideoCapture(m_cameraId);
    m_capturing = false;

    int numControls;
    if ((r = SVBGetNumOfControls(m_cameraId, &numControls)) != SVB_SUCCESS)
    {
        Debug.Write(wxString::Format("SVBGetNumOfControls ret %d\n", r));
        Disconnect();
        return CamConnectFailed(_("Failed to get camera properties for Svbony camera."));
    }

    SVB_BOOL cpg;
    if ((r = SVBCanPulseGuide(m_cameraId, &cpg)) == SVB_SUCCESS)
    {
        m_hasGuideOutput = cpg != SVB_FALSE;
        Debug.Write(wxString::Format("SVBCanPulseGuide: %s\n", m_hasGuideOutput ? "yes" : "no"));
    }
    else
    {
        Debug.Write(wxString::Format("SVBCanPulseGuide ret %d, assuming no ST4 output\n", r));
    }

    HasGainControl = false;
    HasCooler = false;

    for (int i = 0; i < numControls; i++)
    {
        SVB_CONTROL_CAPS caps;
        if (SVBGetControlCaps(m_cameraId, i, &caps) == SVB_SUCCESS)
        {
            switch (caps.ControlType)
            {
            case SVB_GAIN:
                if (caps.IsWritable)
                {
                    HasGainControl = true;
                    m_minGain = caps.MinValue;
                    m_maxGain = caps.MaxValue;
                    m_defaultGainPct = gain_pct(m_minGain, m_maxGain, caps.DefaultValue);
                    Debug.Write(wxString::Format("SVB: gain range = %d .. %d default = %ld (%d%%)\n",
                        m_minGain, m_maxGain, caps.DefaultValue, m_defaultGainPct));
                }
                // fall through

            // set everything to default and no auto
            case SVB_EXPOSURE:
            case SVB_GAMMA:
            case SVB_GAMMA_CONTRAST:
            case SVB_WB_R:
            case SVB_WB_G:
            case SVB_WB_B:
            case SVB_FLIP: // reference: enum SVB_FLIP_STATUS
            case SVB_FRAME_SPEED_MODE: // 0:low speed, 1:medium speed, 2:high speed
            case SVB_CONTRAST:
            case SVB_SHARPNESS:
            case SVB_SATURATION:
            case SVB_AUTO_TARGET_BRIGHTNESS:
            case SVB_BLACK_LEVEL: // black level offset
                SVBSetControlValue(m_cameraId, caps.ControlType, caps.DefaultValue, SVB_FALSE);
                break;

            default:
                break;
            }
        }

    }

    m_frame = wxRect(FullSize);
    Debug.Write(wxString::Format("SVB: frame (%d,%d)+(%d,%d)\n", m_frame.x, m_frame.y, m_frame.width, m_frame.height));

    SVBSetOutputImageType(m_cameraId, img_type);

    SVBSetROIFormat(m_cameraId, m_frame.GetLeft(), m_frame.GetTop(),
        m_frame.GetWidth(), m_frame.GetHeight(), Binning);

    return false;
}

void SVBCamera::StopCapture()
{
    if (m_capturing)
    {
        Debug.Write("SVB: stopcapture\n");
        // we used to call SVBStopVideoCapture() at this point, but we found in testing that
        // the call can occasionally hang, and also that it is not necessary, even when ROI
        // or binning changes.
        m_capturing = false;
    }
}

bool SVBCamera::Disconnect()
{
    StopCapture();
    SVBCloseCamera(m_cameraId);

    Connected = false;

    ::free(m_buffer);
    m_buffer = nullptr;

    return false;
}

bool SVBCamera::StopExposure()
{
    Debug.Write("SVB: stopexposure\n");
    // FIXME - TODO
    return true;
}

bool SVBCamera::GetDevicePixelSize(double *devPixelSize)
{
    if (!Connected)
        return true;

    *devPixelSize = m_devicePixelSize;
    return false;
}

int SVBCamera::GetDefaultCameraGain()
{
    return m_defaultGainPct;
}

inline static int round_down(int v, int m)
{
    return v & ~(m - 1);
}

inline static int round_up(int v, int m)
{
    return round_down(v + m - 1, m);
}

static void flush_buffered_image(int cameraId, void *buf, size_t size)
{
    enum { NUM_IMAGE_BUFFERS = 2 }; // camera has 2 internal frame buffers

    // clear buffered frames if any

    for (unsigned int num_cleared = 0; num_cleared < NUM_IMAGE_BUFFERS; num_cleared++)
    {
        SVB_ERROR_CODE status = SVBGetVideoData(cameraId, (unsigned char *) buf, size, 0);
        if (status != SVB_SUCCESS)
            break; // no more buffered frames

        Debug.Write(wxString::Format("SVB: getimagedata clearbuf %u ret %d\n", num_cleared + 1, status));
    }
}

bool SVBCamera::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    bool binning_change = false;
    if (Binning != m_prevBinning)
    {
        FullSize.x = m_maxSize.x / Binning;
        FullSize.y = m_maxSize.y / Binning;
        m_prevBinning = Binning;
        binning_change = true;
    }

    if (img.Init(FullSize))
    {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }

    wxRect frame;
    wxPoint subframePos; // position of subframe within frame

    bool useSubframe = UseSubframes;

    if (subframe.width <= 0 || subframe.height <= 0)
        useSubframe = false;

    if (useSubframe)
    {
        // ensure transfer size is a multiple of 1024
        //  moving the sub-frame or resizing it is somewhat costly (stopCapture / startCapture)

        frame.SetLeft(round_down(subframe.GetLeft(), 32));
        frame.SetRight(round_up(subframe.GetRight() + 1, 32) - 1);
        frame.SetTop(round_down(subframe.GetTop(), 32));
        frame.SetBottom(round_up(subframe.GetBottom() + 1, 32) - 1);

        subframePos = subframe.GetLeftTop() - frame.GetLeftTop();
    }
    else
    {
        frame = wxRect(FullSize);
    }

    SVB_BOOL tmp;
    long cur_exp;
    // The returned exposure value may differ from the requested exposure by several usecs,
    // so round the returned exposure to the nearest millisecond.
    if (SVBGetControlValue(m_cameraId, SVB_EXPOSURE, &cur_exp, &tmp) == SVB_SUCCESS &&
        (cur_exp + 500) / 1000 != duration)
    {
        Debug.Write(wxString::Format("SVB: set CONTROL_EXPOSURE %d\n", duration * 1000));
        SVBSetControlValue(m_cameraId, SVB_EXPOSURE, duration * 1000, SVB_FALSE);
    }

    long new_gain = cam_gain(m_minGain, m_maxGain, GuideCameraGain);
    long cur_gain;
    if (SVBGetControlValue(m_cameraId, SVB_GAIN, &cur_gain, &tmp) == SVB_SUCCESS &&
        new_gain != cur_gain)
    {
        Debug.Write(wxString::Format("SVB: set CONTROL_GAIN %d%% %d\n", GuideCameraGain, new_gain));
        SVBSetControlValue(m_cameraId, SVB_GAIN, new_gain, SVB_FALSE);
    }

    bool size_change = frame.GetSize() != m_frame.GetSize();
    bool pos_change = frame.GetLeftTop() != m_frame.GetLeftTop();

    if (size_change || pos_change)
    {
        m_frame = frame;
        Debug.Write(wxString::Format("SVB: frame (%d,%d)+(%d,%d)\n", m_frame.x, m_frame.y, m_frame.width, m_frame.height));
    }

    if (pos_change || size_change || binning_change)
    {
        StopCapture();

        SVB_ERROR_CODE status = SVBSetROIFormat(m_cameraId, frame.GetLeft(), frame.GetTop(),
            frame.GetWidth(), frame.GetHeight(), Binning);

        if (status != SVB_SUCCESS)
            Debug.Write(wxString::Format("SVB: setImageFormat(%d,%d,%d,%d,%hu) => %d\n",
                frame.GetLeft(), frame.GetTop(), frame.GetWidth(), frame.GetHeight(), Binning, status));
    }

    int poll = wxMin(duration, 100);

    unsigned char *const buffer =
        m_bpp == 16 && !useSubframe ? (unsigned char *) img.ImageData : (unsigned char *) m_buffer;

    if (m_mode == CM_VIDEO)
    {
        // the camera and/or driver will buffer frames and return the oldest frame,
        // which could be quite stale. read out all buffered frames so the frame we
        // get is current

        flush_buffered_image(m_cameraId, m_buffer, m_buffer_size);

        if (!m_capturing)
        {
            Debug.Write("SVB: startcapture\n");
            SVBStartVideoCapture(m_cameraId);
            m_capturing = true;
        }

        CameraWatchdog watchdog(duration, duration + GetTimeoutMs() + 10000); // total timeout is 2 * duration + 15s (typically)

        while (true)
        {
            SVB_ERROR_CODE status = SVBGetVideoData(m_cameraId, buffer, m_buffer_size, poll);
            if (status == SVB_SUCCESS)
                break;
            if (WorkerThread::InterruptRequested())
            {
                StopCapture();
                return true;
            }
            if (watchdog.Expired())
            {
                Debug.Write(wxString::Format("SVB: getimagedata ret %d\n", status));
                StopCapture();
                DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
                return true;
            }
        }
    }
    else
    {
        // CM_SNAP

        bool frame_ready = false;

        if (!m_capturing)
        {
            Debug.Write("SVB: startcapture\n");
            SVBStartVideoCapture(m_cameraId);
            m_capturing = true;
        }

        for (int tries = 1; tries <= 3 && !frame_ready; tries++)
        {
            if (tries > 1)
                Debug.Write("SVB: getexpstatus EXP_FAILED, retry exposure\n");

            SVBSendSoftTrigger(m_cameraId);

            enum { GRACE_PERIOD_MS = 500 }; // recommended by Svbony
            CameraWatchdog watchdog(duration, duration + GRACE_PERIOD_MS);

            if (duration > 100)
            {
                // wait until near end of exposure
                if (WorkerThread::MilliSleep(duration - 100, WorkerThread::INT_ANY) &&
                    (WorkerThread::TerminateRequested() || StopExposure()))
                {
                    StopExposure();
                    return true;
                }
            }

            while (true)
            {
                SVB_ERROR_CODE status = SVBGetVideoData(m_cameraId, buffer, m_buffer_size, poll);
                if (status == SVB_SUCCESS)
                {
                    frame_ready = true;
                    break;
                }
                if (WorkerThread::InterruptRequested())
                {
                    StopCapture();
                    return true;
                }
                if (watchdog.Expired())
                {
                    // exposure timed-out, retry
                    Debug.Write("SVB: exposure timed-out, retry\n");
                    break;
                }
            }
        }

        if (!frame_ready)
        {
            Debug.Write("SVB: exposure failed, giving up\n");
            DisconnectWithAlert(_("Lost connection to camera"), RECONNECT);
            return true;
        }
    }

    if (useSubframe)
    {
        img.Subframe = subframe;

        // Clear out the image
        img.Clear();

        if (m_bpp == 8)
        {
            for (int y = 0; y < subframe.height; y++)
            {
                const unsigned char *src = buffer + (y + subframePos.y) * frame.width + subframePos.x;
                unsigned short *dst = img.ImageData + (y + subframe.y) * FullSize.GetWidth() + subframe.x;
                for (int x = 0; x < subframe.width; x++)
                    *dst++ = *src++;
            }
        }
        else
        {
            for (int y = 0; y < subframe.height; y++)
            {
                const unsigned short *src = (unsigned short *) buffer + (y + subframePos.y) * frame.width + subframePos.x;
                unsigned short *dst = img.ImageData + (y + subframe.y) * FullSize.GetWidth() + subframe.x;
                for (int x = 0; x < subframe.width; x++)
                    *dst++ = *src++;
            }
        }
    }
    else
    {
        if (m_bpp == 8)
        {
            for (unsigned int i = 0; i < img.NPixels; i++)
                img.ImageData[i] = buffer[i];
        }
        else
        {
            // 16-bit mode and no subframe: data is already in img.ImageData
        }
    }

    if (options & CAPTURE_SUBTRACT_DARK)
        SubtractDark(img);
    if (m_isColor && Binning == 1 && (options & CAPTURE_RECON))
       QuickLRecon(img);

    return false;
}

inline static SVB_GUIDE_DIRECTION GetSVBDirection(int direction)
{
    switch (direction)
    {
    default:
    case NORTH:
        return SVB_GUIDE_NORTH;
    case SOUTH:
        return SVB_GUIDE_SOUTH;
    case EAST:
        return SVB_GUIDE_EAST;
    case WEST:
        return SVB_GUIDE_WEST;
    }
}

bool SVBCamera::ST4PulseGuideScope(int direction, int duration)
{
    SVB_GUIDE_DIRECTION d = GetSVBDirection(direction);
    SVBPulseGuide(m_cameraId, d, duration);
    WorkerThread::MilliSleep(duration, WorkerThread::INT_ANY);
    return false;
}

GuideCamera *SVBCameraFactory::MakeSVBCamera()
{
    return new SVBCamera();
}

#endif // SVB_CAMERA
