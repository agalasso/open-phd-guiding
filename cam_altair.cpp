/*
 *  cam_altair.cpp
 *  PHD Guiding
 *
 *  Created by Robin Glover.
 *  Copyright (c) 2014 Robin Glover
 *  Copyright (c) 2018 Andy Galasso
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

#ifdef ALTAIR

#include "cam_altair.h"
#include "altaircam.h"

#ifdef __WINDOWS__

struct SDKLib
{
    HMODULE m_module;

#define SDK(f) \
    decltype(Altaircam_ ## f) *f;
#define SDK_OPT(f) SDK(f)
# include "cameras/altaircam_sdk.h"
#undef SDK
#undef SDK_OPT

    SDKLib() : m_module(nullptr) { }
    ~SDKLib() { Unload(); }

    bool _Load(LPCTSTR filename, const char *prefix)
    {
        if (m_module)
            return true;

        Debug.Write(wxString::Format("Altair: loading %s\n", filename));

        m_module = LoadLibrary(filename);
        if (!m_module)
        {
            Debug.Write(wxString::Format("Altair: could not load library %s\n", filename));
            return false;
        }

        try
        {
#define _GPA(f) \
            std::ostringstream os; \
            os << prefix << #f; \
            std::string name = os.str(); \
            f = reinterpret_cast<decltype(Altaircam_ ## f) *>(GetProcAddress(m_module, name.c_str()))
#define SDK(f) do { \
            _GPA(f); \
            if (!f) \
                throw name; \
        } while (false);
#define SDK_OPT(f) do { \
            _GPA(f); \
        } while (false);
# include "cameras/altaircam_sdk.h"
#undef SDK
#undef SDK_OPT
#undef _GPA
        }
        catch (const std::string& name)
        {
            Debug.Write(wxString::Format("Altair: %s missing symbol %s\n", filename, name.c_str()));
            Unload();
            return false;
        }

        Debug.Write(wxString::Format("Altair: SDK version %s\n", Version()));

        return true;
    }

    bool Load()
    {
        return _Load(_T("altaircam.dll"), "Altaircam_");
    }

    bool LoadLegacy()
    {
        return _Load(_T("AltairCam_legacy.dll"), "Toupcam_");
    }

    void Unload()
    {
        if (m_module)
        {
            FreeLibrary(m_module);
            m_module = nullptr;
        }
    }
};

#endif // __WINDOWS__

struct AltairCamera : public GuideCamera
{
    enum { MAX_DISCARD_FRAMES = 5 };

    AltairCamType m_type;
    SDKLib m_sdk;
    wxRect m_frame;
    unsigned char *m_buffer;
    bool m_isColor;
    bool m_capturing;
    unsigned int m_discardCnt;
    int m_minGain;
    int m_maxGain;
    double m_devicePixelSize;
    HAltaircam m_handle;
    volatile bool m_frameReady;
    bool m_reduceResolution;
    unsigned int m_framesToDiscard;

    AltairCamera(AltairCamType type);
    ~AltairCamera();

    bool LoadSDK();

    bool CanSelectCamera() const override { return true; }
    bool EnumCameras(wxArrayString& names, wxArrayString& ids) override;
    bool Capture(int duration, usImage& img, int options, const wxRect& subframe) override;
    bool Connect(const wxString& camId) override;
    bool Disconnect() override;

    bool ST4PulseGuideScope(int direction, int duration) override;
    void ClearGuidePort();

    void ShowPropertyDialog() override;

    bool HasNonGuiCapture() override { return true; }
    bool ST4HasNonGuiMove() override { return true; }
    wxByte BitsPerPixel() override;
    bool GetDevicePixelSize(double *devPixelSize) override;

    void StopCapture();
};

class AltairCameraDlg : public wxDialog
{
public:
    wxCheckBox *m_reduceRes;
    wxSpinCtrl *m_framesToDiscard;

    AltairCameraDlg(wxWindow *parent);
    ~AltairCameraDlg() { }
};

AltairCameraDlg::AltairCameraDlg(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _("Altair Camera Settings"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer *top_sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer *sbSizer3 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Settings")), wxVERTICAL);

    wxBoxSizer *sizer1 = new wxBoxSizer(wxHORIZONTAL);
    m_reduceRes = new wxCheckBox(this, wxID_ANY,
            wxString::Format(_("Reduced Resolution (by ~%d%%)"), 20),
            wxDefaultPosition, wxDefaultSize, 0);
    sizer1->Add(m_reduceRes, 0, wxALL, 5);
    sbSizer3->Add(sizer1);

    wxBoxSizer *sizer2 = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *txt1 = new wxStaticText(this, wxID_ANY, _("Discard Frames"));
    sizer2->Add(txt1, 0, wxALL, 5);
    int width = StringWidth(this, _T("00"));
    m_framesToDiscard = pFrame->MakeSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1),
        wxSP_ARROW_KEYS, 0, AltairCamera::MAX_DISCARD_FRAMES, 0);
    m_framesToDiscard->SetToolTip(_("Discard this many frames whan capturing starts. "
        "Useful for preventing initial under-exposed frames interfering with automatic star selection."));
    sizer2->Add(m_framesToDiscard, 0, wxALL, 5);
    sbSizer3->Add(sizer2);

    top_sizer->Add(sbSizer3, 1, wxEXPAND, 5);

    wxStdDialogButtonSizer *sdbSizer2 = new wxStdDialogButtonSizer();
    wxButton *sdbSizer2OK = new wxButton(this, wxID_OK);
    wxButton *sdbSizer2Cancel = new wxButton(this, wxID_CANCEL);
    sdbSizer2->AddButton(sdbSizer2OK);
    sdbSizer2->AddButton(sdbSizer2Cancel);
    sdbSizer2->Realize();

    top_sizer->Add(sdbSizer2, 0, wxALL | wxEXPAND, 5);

    SetSizer(top_sizer);
    Layout();
    Fit();

    Centre(wxBOTH);
}

static int GetConfigDiscardFrames()
{
    int n = pConfig->Profile.GetInt("/camera/Altair/DiscardFrames", 0);
    return wxMax(0, wxMin((int) AltairCamera::MAX_DISCARD_FRAMES, n));
}

AltairCamera::AltairCamera(AltairCamType type)
    :
    m_type(type),
    m_buffer(nullptr),
    m_capturing(false)
{
    Name = _T("Altair Camera");
    Connected = false;
    m_hasGuideOutput = true;
    HasSubframes = false;
    HasGainControl = true; // workaround: ok to set to false later, but brain dialog will crash if we start false then change to true later when the camera is connected
    PropertyDialogType = PROPDLG_WHEN_DISCONNECTED;

    this->m_framesToDiscard = GetConfigDiscardFrames();
}

AltairCamera::~AltairCamera()
{
    delete[] m_buffer;
}

wxByte AltairCamera::BitsPerPixel()
{
    return 8;
}

inline static int cam_gain(int minval, int maxval, int pct)
{
    return minval + pct * (maxval - minval) / 100;
}

inline static int gain_pct(int minval, int maxval, int val)
{
    return (val - minval) * 100 / (maxval - minval);
}

static unsigned int Enum(const SDKLib& sdk, AltaircamDeviceV2 inst[ALTAIRCAM_MAX])
{
    if (sdk.EnumV2)
        return sdk.EnumV2(inst);

    static AltaircamModelV2 s_model[ALTAIRCAM_MAX];
    static AltaircamDevice s_inst1[ALTAIRCAM_MAX];

    unsigned int count = sdk.Enum(s_inst1);
    for (unsigned int i = 0; i < count; i++)
    {
        memcpy(&inst[i].displayname[0], &s_inst1[i].displayname[0], sizeof(inst[i].displayname));
        memcpy(&inst[i].id[0], &s_inst1[i].id[0], sizeof(inst[i].id));
        s_model[i].name = s_inst1[i].model->name;
        s_model[i].flag = s_inst1[i].model->flag;
        s_model[i].maxspeed = s_inst1[i].model->maxspeed;
        s_model[i].preview = s_inst1[i].model->preview;
        s_model[i].still = 0; // unknwown
        s_model[i].maxfanspeed = 0; // unknown
        s_model[i].ioctrol = 0;
        s_model[i].xpixsz = 0.f;
        s_model[i].ypixsz = 0.f;
        memcpy(&s_model[i].res[0], &s_inst1[i].model->res[0], sizeof(s_model[i].res));
        inst[i].model = &s_model[i];
    }

    return count;
}

bool AltairCamera::LoadSDK()
{
    switch (m_type)
    {
    default:
    case ALTAIR_CAM_CURRENT:
        return m_sdk.Load();

    case ALTAIR_CAM_LEGACY:
        return m_sdk.LoadLegacy();
    }
}

bool AltairCamera::EnumCameras(wxArrayString& names, wxArrayString& ids)
{
    if (!LoadSDK())
        return true;

    AltaircamDeviceV2 ai[ALTAIRCAM_MAX];
    unsigned numCameras = Enum(m_sdk, ai);

    for (int i = 0; i < numCameras; i++)
    {
        names.Add(ai[i].displayname);
        ids.Add(ai[i].id);
    }

    return false;
}

bool AltairCamera::Connect(const wxString& camIdArg)
{
    if (!LoadSDK())
        return true;

    AltaircamDeviceV2 ainst[ALTAIRCAM_MAX];
    unsigned int numCameras = Enum(m_sdk, ainst);

    if (numCameras == 0)
    {
        return CamConnectFailed(_("No Altair cameras detected"));
    }

    wxString camId(camIdArg);
    if (camId == DEFAULT_CAMERA_ID)
        camId = ainst[0].id;

    const AltaircamDeviceV2 *pai = nullptr;
    for (unsigned int i = 0; i < numCameras; i++)
    {
        if (camId == ainst[i].id)
        {
            pai = &ainst[i];
            break;
        }
    }
    if (!pai)
    {
        return CamConnectFailed(_("Specified Altair Camera not found."));
    }

    m_handle = m_sdk.Open(camId);
    if (!m_handle)
    {
        return CamConnectFailed(_("Failed to open Altair Camera."));
    }

    Connected = true;

    Name = pai->displayname;
    bool hasROI = (pai->model->flag & ALTAIRCAM_FLAG_ROI_HARDWARE) != 0;
    bool hasSkip = (pai->model->flag & ALTAIRCAM_FLAG_BINSKIP_SUPPORTED) != 0;
    m_isColor = (pai->model->flag & ALTAIRCAM_FLAG_MONO) == 0;

    Debug.Write(wxString::Format("ALTAIR: isColor = %d, hasROI = %d, hasSkip = %d\n",
                                 m_isColor, hasROI, hasSkip));

    int width, height;
    if (FAILED(m_sdk.get_Resolution(m_handle, 0, &width, &height)))
    {
        Disconnect();
        return CamConnectFailed(_("Failed to get camera resolution for Altair Camera."));
    }

    delete[] m_buffer;
    m_buffer = new unsigned char[width * height]; // new SDK has issues with some ROI functions needing full resolution buffer size

    m_reduceResolution = pConfig->Profile.GetBoolean("/camera/Altair/ReduceResolution", false);
    if (hasROI && m_reduceResolution)
    {
        width *= 0.8;
        height *= 0.8;
    }

    FullSize.x = width;
    FullSize.y = height;

    float xSize, ySize;
    m_devicePixelSize = 3.75; // for all cameras so far....
    if (m_sdk.get_PixelSize(m_handle, 0, &xSize, &ySize) == 0)
    {
        m_devicePixelSize = xSize;
    }

    HasGainControl = false;

    unsigned short min, max, def;
    if (SUCCEEDED(m_sdk.get_ExpoAGainRange(m_handle, &min, &max, &def)))
    {
        m_minGain = min;
        m_maxGain = max;
        HasGainControl = max > min;
    }

    m_sdk.put_Speed(m_handle, 0);
    m_sdk.put_RealTime(m_handle, TRUE);

    if (hasSkip)
        m_sdk.put_Mode(m_handle, 0);

    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_RAW, 1);

#if 0
    // TODO: this is the initiailization code copied from cam_touptek.cpp
    // I was hoping this one of these might help with the problem of the first
    // frame exposure being very low, but it had no effect. Leaving these
    // disabled for now rather than risk introducing a change that is
    // incompatible with one of the camera models I am unable to test with.
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_PROCESSMODE, 0);
    //m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_BITDEPTH, m_cam.m_bpp == 8 ? 0 : 1);
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_LINEAR, 0);
    //m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_CURVE, 0); // resetting this one fails on all the cameras I have
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_COLORMATIX, 0);
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_WBGAIN, 0);
    //m_sdk.put_Option(ALTAIRCAM_OPTION_TRIGGER, 1);  // software trigger
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_AUTOEXP_POLICY, 0); // 0="Exposure Only" 1="Exposure Preferred"
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_ROTATE, 0);
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_UPSIDE_DOWN, 0);
    //m_cam.SetOption(m_handle, ALTAIRCAM_OPTION_CG, 0); // "Conversion Gain" 0=LCG 1=HCG 2=HDR // setting this fails
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_FFC, 0);
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_DFC, 0);
    m_sdk.put_Option(m_handle, ALTAIRCAM_OPTION_SHARPENING, 0);
#endif

    m_sdk.put_AutoExpoEnable(m_handle, 0);

    m_frame = wxRect(FullSize);

    Debug.Write(wxString::Format("Altair: frame (%d,%d)+(%d,%d)\n",
        m_frame.x, m_frame.y, m_frame.width, m_frame.height));

    if (hasROI && m_reduceResolution)
    {
        m_sdk.put_Roi(m_handle, 0, 0, width, height);
    }

    return false;
}

void AltairCamera::StopCapture()
{
    if (m_capturing)
    {
        Debug.AddLine("Altair: stopcapture");
        m_sdk.Stop(m_handle);
        m_capturing = false;
    }
}

bool AltairCamera::GetDevicePixelSize(double *devPixelSize)
{
    if (!Connected)
        return true;

    *devPixelSize = m_devicePixelSize;
    return false;                               // Pixel size is known in any case
}

void AltairCamera::ShowPropertyDialog()
{
    AltairCameraDlg dlg(wxGetApp().GetTopWindow());

    bool value = pConfig->Profile.GetBoolean("/camera/Altair/ReduceResolution", false);
    dlg.m_reduceRes->SetValue(value);

    int n = GetConfigDiscardFrames();
    dlg.m_framesToDiscard->SetValue(n);

    if (dlg.ShowModal() == wxID_OK)
    {
        m_reduceResolution = dlg.m_reduceRes->GetValue();
        pConfig->Profile.SetBoolean("/camera/Altair/ReduceResolution", m_reduceResolution);

        m_framesToDiscard = dlg.m_framesToDiscard->GetValue();
        pConfig->Profile.SetInt("/camera/Altair/DiscardFrames", m_framesToDiscard);
    }
}

bool AltairCamera::Disconnect()
{
    StopCapture();
    m_sdk.Close(m_handle);

    Connected = false;

    delete[] m_buffer;
    m_buffer = nullptr;

    return false;
}

inline static int round_down(int v, int m)
{
    return v & ~(m - 1);
}

inline static int round_up(int v, int m)
{
    return round_down(v + m - 1, m);
}

void __stdcall CameraCallback(unsigned int event, void *pCallbackCtx)
{
    if (event == ALTAIRCAM_EVENT_IMAGE)
    {
        AltairCamera *cam = (AltairCamera *) pCallbackCtx;
        cam->m_frameReady = true;
    }
}

//static void flush_buffered_image(int cameraId, usImage& img)
//{
//    enum { NUM_IMAGE_BUFFERS = 2 }; // camera has 2 internal frame buffers
//
//    // clear buffered frames if any
//
//    for (unsigned int num_cleared = 0; num_cleared < NUM_IMAGE_BUFFERS; num_cleared++)
//    {
//        ASI_ERROR_CODE status = ASIGetVideoData(cameraId, (unsigned char *) img.ImageData, img.NPixels * sizeof(unsigned short), 0);
//        if (status != ASI_SUCCESS)
//            break; // no more buffered frames
//
//        Debug.Write(wxString::Format("Altair: getimagedata clearbuf %u ret %d\n", num_cleared + 1, status));
//    }
//}

bool AltairCamera::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    if (img.Init(FullSize))
    {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }

    wxRect frame;
    frame = wxRect(FullSize);

    long exposureUS = duration * 1000;
    unsigned int cur_exp;
    if (m_sdk.get_ExpoTime(m_handle, &cur_exp) == 0 &&
        cur_exp != exposureUS)
    {
        Debug.Write(wxString::Format("Altair: set CONTROL_EXPOSURE %d\n", exposureUS));
        m_sdk.put_ExpoTime(m_handle, exposureUS);
    }

    long new_gain = cam_gain(m_minGain, m_maxGain, GuideCameraGain);
    unsigned short cur_gain;
    if (m_sdk.get_ExpoAGain(m_handle, &cur_gain) == 0 &&
        new_gain != cur_gain)
    {
        Debug.Write(wxString::Format("Altair: set CONTROL_GAIN %d%% %d\n", GuideCameraGain, new_gain));
        m_sdk.put_ExpoAGain(m_handle, new_gain);
    }

    // the camera and/or driver will buffer frames and return the oldest frame,
    // which could be quite stale. read out all buffered frames so the frame we
    // get is current

    // flush_buffered_image(m_handle, img);
    unsigned int width, height;
    while (SUCCEEDED(m_sdk.PullImage(m_handle, m_buffer, 8, &width, &height)))
    {
    }

    if (!m_capturing)
    {
        Debug.AddLine("Altair: startcapture");
        m_frameReady = false;
        HRESULT result = m_sdk.StartPullModeWithCallback(m_handle, CameraCallback, this);
        if (result != 0)
        {
            Debug.Write(wxString::Format("Altaircam_StartPullModeWithCallback failed with code %d\n", result));
            return true;
        }
        m_capturing = true;
        m_discardCnt = m_framesToDiscard;
    }

    int frameSize = frame.GetWidth() * frame.GetHeight();

    int poll = wxMin(duration, 100);

    while (true) // frame discard loop
    {
        CameraWatchdog watchdog(duration, duration + GetTimeoutMs() + 10000); // total timeout is 2 * duration + 15s (typically)

        // do not wait here, as we will miss a frame most likely, leading to poor flow of frames.
//        if (WorkerThread::MilliSleep(duration, WorkerThread::INT_ANY) &&
//            (WorkerThread::TerminateRequested() || StopCapture()))
//        {
//            return true;
//        }

        while (true) // PullImage retry loop
        {
            if (m_frameReady)
            {
                m_frameReady = false;

                if (SUCCEEDED(m_sdk.PullImage(m_handle, m_buffer, 8, &width, &height)))
                    break;
            }
            WorkerThread::MilliSleep(poll, WorkerThread::INT_ANY);
            if (WorkerThread::InterruptRequested())
            {
                StopCapture();
                return true;
            }
            if (watchdog.Expired())
            {
                Debug.AddLine("Altair: getimagedata failed");
                StopCapture();
                DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
                return true;
            }
        }

        if (!m_discardCnt)
            break;

        Debug.Write(wxString::Format("Altair: discard frame %u\n", m_discardCnt));

        --m_discardCnt;

    } // discard loop

    for (unsigned int i = 0; i < img.NPixels; i++)
        img.ImageData[i] = m_buffer[i];

    if (options & CAPTURE_SUBTRACT_DARK)
        SubtractDark(img);
    if (m_isColor && Binning == 1 && (options & CAPTURE_RECON))
        QuickLRecon(img);

    return false;
}

inline static int GetAltairDirection(int direction)
{
    switch (direction)
    {
    default:
    case NORTH:
        return 0;
    case EAST:
        return 2;
    case WEST:
        return 3;
    case SOUTH:
        return 1;
    }
}

bool AltairCamera::ST4PulseGuideScope(int direction, int duration)
{
    int d = GetAltairDirection(direction);
    m_sdk.ST4PlusGuide(m_handle, d, duration);

    return false;
}

void AltairCamera::ClearGuidePort()
{
    m_sdk.ST4PlusGuide(m_handle, 0, 0);
}

GuideCamera *AltairCameraFactory::MakeAltairCamera(AltairCamType type)
{
    return new AltairCamera(type);
}

#endif // Altaircam_ASI
