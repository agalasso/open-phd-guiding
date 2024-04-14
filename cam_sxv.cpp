/*
 *  cam_sxv.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2008-2010 Craig Stark
 *  Copyright (c) 2016-2018 Andy Galasso
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

#if defined (SXV)

#include "cam_sxv.h"
#include "image_math.h"

#include <wx/choicdlg.h>

#if defined (__WINDOWS__)
# include "cameras/SXUSB.h"
typedef struct t_sxccd_params sxccd_params_t;
typedef HANDLE sxccd_handle_t;
# define SXCCD_EXP_FLAGS_NOWIPE_FRAME CCD_EXP_FLAGS_NOWIPE_FRAME
# define SXCCD_EXP_FLAGS_FIELD_ODD    CCD_EXP_FLAGS_FIELD_ODD
# define SXCCD_EXP_FLAGS_FIELD_BOTH   CCD_EXP_FLAGS_FIELD_BOTH
#else
# include "cameras/SXMacLib.h"
typedef void *sxccd_handle_t;
#endif

class CameraSXV : public GuideCamera
{
    sxccd_handle_t hCam;
    sxccd_params_t CCDParams;
    unsigned short *RawData;
    unsigned int RawDataSize;
    usImage tmpImg;
    unsigned short CameraModel;
    unsigned short m_prevBin;
    bool Interlaced;
    bool ColorSensor;
    bool SquarePixels;
    wxSize m_darkFrameSize;
    double m_devicePixelSize;

public:

    CameraSXV();
    ~CameraSXV();

    bool CanSelectCamera() const override { return true; }
    bool EnumCameras(wxArrayString& names, wxArrayString& ids) override;
    bool Capture(int duration, usImage& img, int options, const wxRect& subframe) override;
    bool Connect(const wxString& camId) override;
    bool Disconnect() override;
    void ShowPropertyDialog() override;
    const wxSize& DarkFrameSize() override { return m_darkFrameSize; }

    bool HasNonGuiCapture() override { return true; }
    bool ST4HasNonGuiMove() override { return true; }
    bool ST4PulseGuideScope(int direction, int duration) override;
    wxByte BitsPerPixel() override;
    bool GetDevicePixelSize(double *devPixelSize) override;

private:
    void InitFrameSizes();
};

enum {
    SX_CMOS_GUIDER = 39,
};

inline static bool IsCMOSGuider(int model)
{
    return model == SX_CMOS_GUIDER;
}

static wxString NameFromModel(int model)
{
    wxString m;

    switch (model)
    {
        case 0x05: m = "SX-H5"; break;
        case 0x85: m = "SX-H5C"; break;
        case 0x09: m = "SX-H9"; break; // this is almost certainly a Superstar
        case 0x89: m = "SX-H9C"; break;
        case 0x39: m = "SX-LS9"; break;
        case 0x19: m = "SX-SX9"; break;
        case 0x99: m = "SX-SX9C"; break;
        case 0x10: m = "SX-H16"; break;
        case 0x90: m = "SX-H16C"; break;
        case 0x11: m = "SX-H17"; break;
        case 0x91: m = "SX-H17C"; break;
        case 0x12: m = "SX-H18"; break;
        case 0x92: m = "SX-H18C"; break;
        case 0x23: m = "SX-H35"; break;
        case 0xB3: m = "SX-H35C"; break;
        case 0x24: m = "SX-H36"; break;
        case 0xB4: m = "SX-H36C"; break;
        case 0x56: m = "SX-H674"; break;
        case 0xB6: m = "SX-H674C"; break;
        case 0x57: m = "SX-H694"; break;
        case 0xB7: m = "SX-H694C"; break;
        case 0x28: m = "SX-H814"; break;
        case 0xA8: m = "SX-H814C"; break;
        case 0x29: m = "SX-H834"; break;
        case 0xA9: m = "SX-H834C"; break;
        case 0x3B: m = "SX-H825"; break;
        case 0xBB: m = "SX-H825C"; break;
        case 0x3C: m = "SX-US825"; break;
        case 0xBC: m = "SX-US825C"; break;
            // interlaced models
        case 0x45: m = "SX-MX5"; break;
        case 0x84: m = "SX-MX5C"; break;
        case 0x46: m = "SX-LX1"; break;
        case 0x47: m = "SX-MX7"; break;
        case 0xC7: m = "SX-MX7C"; break;
        case 0x48: m = "SX-MX8"; break;
        case 0xC8: m = "SX-MX8C"; break;
        case 0x49: m = "SX-MX9"; break;
        case 0x59: m = "SX-M25"; break;
        case 0x5A: m = "SX-M26"; break;
            // development models
        case 0x0C: m = "SX-DEV1"; break;
        case 0x0D: m = "SX-DEV2"; break;
        case 0x0E: m = "SX-DEV3"; break;
        case 0x0F: m = "SX-DEV4"; break;

        default: m = wxString::Format("SX Camera Model %d", model); break;
    }

    if (model == 0x46)
        m = _T("SX Lodestar");
    else if (model == SX_CMOS_GUIDER)
        m = _T("SX CoStar");
    else if (model == 0x3A || model == 0x19)
        m = _T("SX Superstar");
    else if (model == 0x3C || model == 0xBC)
        m = _T("SX Ultrastar");

    return m;
}

CameraSXV::CameraSXV()
{
    Connected = false;
    Name = _T("Starlight Xpress SXV");
    HasGainControl = false;
    m_hasGuideOutput = true;
    Interlaced = false;
    RawData = nullptr;
    RawDataSize = 0;
    HasSubframes = true;
    PropertyDialogType = PROPDLG_WHEN_DISCONNECTED;
    SquarePixels = pConfig->Profile.GetBoolean("/camera/SXV/SquarePixels", false);
}

CameraSXV::~CameraSXV()
{
    delete[] RawData;
}

wxByte CameraSXV::BitsPerPixel()
{
    return 16;
}

class SXCameraDlg : public wxDialog
{
public:
    wxCheckBox* m_squarePixels;

    SXCameraDlg(wxWindow *parent, wxWindowID id = wxID_ANY, const wxString& title = _("SX Camera Settings"),
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(268, 133), long style = wxDEFAULT_DIALOG_STYLE);
    ~SXCameraDlg() { }
};

SXCameraDlg::SXCameraDlg(wxWindow *parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style)
    : wxDialog(parent, id, title, pos, size, style)
{
    SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer *bSizer12 = new wxBoxSizer(wxVERTICAL);
    wxStaticBoxSizer *sbSizer3 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Settings")), wxVERTICAL);

    m_squarePixels = new wxCheckBox(this, wxID_ANY, wxT("Square Pixels"), wxDefaultPosition, wxDefaultSize, 0);
    sbSizer3->Add(m_squarePixels, 0, wxALL, 5);
    bSizer12->Add(sbSizer3, 1, wxEXPAND, 5);

    wxStdDialogButtonSizer* sdbSizer2 = new wxStdDialogButtonSizer();
    wxButton *sdbSizer2OK = new wxButton(this, wxID_OK);
    wxButton* sdbSizer2Cancel = new wxButton(this, wxID_CANCEL);
    sdbSizer2->AddButton(sdbSizer2OK);
    sdbSizer2->AddButton(sdbSizer2Cancel);
    sdbSizer2->Realize();
    bSizer12->Add(sdbSizer2, 0, wxALL | wxEXPAND, 5);

    SetSizer(bSizer12);
    Layout();

    Centre(wxBOTH);
}

void CameraSXV::ShowPropertyDialog()
{
    SXCameraDlg dlg(wxGetApp().GetTopWindow());
    dlg.m_squarePixels->SetValue(SquarePixels);
    if (dlg.ShowModal() == wxID_OK)
    {
        SquarePixels = dlg.m_squarePixels->GetValue();
        pConfig->Profile.SetBoolean("/camera/SXV/SquarePixels", SquarePixels);
    }
}

bool CameraSXV::EnumCameras(wxArrayString& names, wxArrayString& ids)
{
    sxccd_handle_t hCams[SXCCD_MAX_CAMS];

    int ncams = sxOpen(hCams);

    for (int i = 0; i < ncams; i++)
    {
        unsigned short model = sxGetCameraModel(hCams[i]);
        names.Add(wxString::Format("%d: %s", i + 1, NameFromModel(model)));
        ids.Add(wxString::Format("%d", i));
    }

    // close handles
    for (int j = 0; j < ncams; j++)
        sxClose(hCams[j]);

    return false;
}

void CameraSXV::InitFrameSizes()
{
    if (Interlaced)
    {
        // The interlaced CCDs report the size of a field for the height
        m_darkFrameSize = wxSize(CCDParams.width / Binning, CCDParams.height); // always vertically binned
        if (SquarePixels)
        {
            FullSize.SetWidth(CCDParams.width / Binning);
            // This is the height after squaring pixels.
            FullSize.SetHeight((int)floor((float)(CCDParams.height / Binning) * CCDParams.pix_height / CCDParams.pix_width));
        }
        else
        {
            FullSize = wxSize(CCDParams.width / Binning, CCDParams.height * 2 / Binning);
        }
    }
    else
    {
        FullSize = wxSize(CCDParams.width / Binning, CCDParams.height / Binning);
        m_darkFrameSize = FullSize;
    }

    Debug.Write(wxString::Format("SXV: Bin = %hu, dark size = %dx%d, frame size = %dx%d\n",
        Binning, m_darkFrameSize.x, m_darkFrameSize.y, FullSize.x, FullSize.y));
}

bool CameraSXV::Connect(const wxString& camId)
{
    // returns true on error

#if defined (__APPLE__) || defined (__linux__) || defined (__FreeBSD__)
    sxSetTimeoutMS(m_timeoutMs);
#endif

    long idx = -1;
    if (camId == DEFAULT_CAMERA_ID)
        idx = 0;
    else
        camId.ToLong(&idx);

    sxccd_handle_t hCams[SXCCD_MAX_CAMS];

    int ncams = sxOpen(hCams);
    if (ncams == 0)
    {
        return CamConnectFailed(_("No SX cameras found"));
    }

    if (idx < 0 || idx >= ncams)
    {
        Debug.AddLine(wxString::Format("SXV: invalid camera id: '%s', ncams = %d", camId, ncams));
        return true;
    }

    // close the ones not selected
    for (int i = 0; i < ncams; i++)
        if (i != idx)
            sxClose(hCams[i]);

    hCam = hCams[idx];

    bool err = false;

    // Load parameters
    sxGetCameraParams(hCam, 0, &CCDParams);

    if (CCDParams.width == 0 || CCDParams.height == 0)
    {
        return CamConnectFailed(_("Connect failed: could not retrieve camera parameters."));
    }

    // deal with what if no porch in there ??
    // WRITE??

    CameraModel = sxGetCameraModel(hCam);

    Name = NameFromModel(CameraModel);

    if (CameraModel & 0x80)  // color
        ColorSensor = true;
    else
        ColorSensor = false;

    if (CameraModel & 0x40)
        Interlaced = true;
    else
        Interlaced = false;

    unsigned short SubType = CameraModel & 0x1F;
    if (SubType == 25)
        Interlaced = false;

    // do not allow SquarePixels if they are already square
    if (SquarePixels && fabs(CCDParams.pix_width - CCDParams.pix_height) / CCDParams.pix_width < 0.01)
    {
        Debug.Write("SXV: Disabling SquarePixels as pixels are already square\n");
        SquarePixels = false;
    }

    if (Interlaced)
    {
        if (SquarePixels)
            m_devicePixelSize = CCDParams.pix_width;
        else
            m_devicePixelSize = std::min(CCDParams.pix_width, CCDParams.pix_height / 2.f);
    }
    else
        m_devicePixelSize = std::min(CCDParams.pix_width, CCDParams.pix_height);

    if (!IsCMOSGuider(CameraModel))
    {
        MaxBinning = 2;
    }
    if (Binning > MaxBinning)
        Binning = MaxBinning;

    InitFrameSizes();
    m_prevBin = Binning;

    if (CCDParams.extra_caps & 0x20)
        HasShutter = true;

    if (IsCMOSGuider(CameraModel))
    {
        HasSubframes = false;
        FullSize.x -= 16;
        m_darkFrameSize.x -= 16;
    }

    if (tmpImg.Init(m_darkFrameSize))
    {
        Debug.AddLine("SX camera: tmpImg Init failed!");
        err = true;
    }

    Debug.AddLine("SX Camera: " + Name);
    Debug.AddLine(wxString::Format("SX Camera Params: %u x %u (reported as %u x %u) PixSz: %.2f x %.2f; #Pix: %u Array color type: %u,%u Interlaced: %d Model: %u, Subype: %u, Porch: %u,%u %u,%u Extras: %u",
            FullSize.GetWidth(), FullSize.GetHeight(), CCDParams.width, CCDParams.height,
            CCDParams.pix_width, CCDParams.pix_height, FullSize.GetHeight() * FullSize.GetWidth(),
            CCDParams.color_matrix, (int) ColorSensor, (int) Interlaced,
            CameraModel, SubType, CCDParams.hfront_porch, CCDParams.hback_porch, CCDParams.vfront_porch, CCDParams.vback_porch,
            CCDParams.extra_caps));

    if (!err)
        Connected = true;

    return err;
}

bool CameraSXV::Disconnect()
{
    delete[] RawData;
    RawData = nullptr;
    RawDataSize = 0;
    Connected = false;
    sxReset(hCam);
    sxClose(hCam);

    hCam = nullptr;

    return false;
}

bool CameraSXV::GetDevicePixelSize(double *devPixelSize)
{
    if (!Connected)
        return true;

    *devPixelSize = m_devicePixelSize;
    return false;
}

static bool InitImgCMOSGuider(usImage& img, const wxSize& FullSize, const unsigned short *raw)
{
    // CMOS guider -- crop and clean

    // Re-assemble image
    int output_xsize = FullSize.GetWidth();
    int output_ysize = FullSize.GetHeight();

    if (img.Init(output_xsize, output_ysize))
        return true;

    const unsigned short *rawptr = raw;
    unsigned short *dataptr = img.ImageData;

    for (int y = 0; y < output_ysize; y++)
    {
        int oddbias, evenbias;
        oddbias = evenbias = 0;
        for (int x = 0; x < 16; x += 2) { // Figure the offsets for this line
            oddbias += (int)*rawptr++;
            evenbias += (int)*rawptr++;
        }
        oddbias = oddbias / 8 - 1000;  // Create avg and pre-build in the offset to keep off of the floor
        evenbias = evenbias / 8 - 1000;
        for (int x = 0; x < output_xsize; x += 2) { // Load value into new image array pulling out right bias
            int val = (int)*rawptr++ - oddbias;
            if (val < 0) val = 0;  // Bounds check
            else if (val > 65535) val = 65535;
            *dataptr++ = (unsigned short)val;
            val = (int)*rawptr++ - evenbias;
            if (val < 0) val = 0;  // Bounds check
            else if (val > 65535) val = 65535;
            *dataptr++ = (unsigned short)val;
        }
    }

    return false;
}

static bool InitImgInterlacedInterp(usImage& img, const wxSize& FullSize, bool subframe, const wxRect& frame, const usImage& tmp)
{
    if (img.Init(FullSize))
        return true;

    if (subframe)
    {
        img.Subframe = frame;
        img.Clear();
    }

    const unsigned short *raw = tmp.ImageData;
    int const fullw = FullSize.GetWidth();
    int const framew = frame.GetWidth();
    int const xofs = frame.GetLeft();

    int end = frame.GetBottom();
    if ((end & 1) && end == FullSize.GetHeight() - 1)
        --end;

    for (int y = frame.GetTop(); y <= end; y++)
    {
        unsigned short *dst = img.ImageData + y * fullw + xofs;
        if ((y & 1) == 0)
        {
            // even row - copy directly
            const unsigned short *src = raw + (y / 2) * fullw + xofs;
            memcpy(dst, src, framew * sizeof(unsigned short));
        }
        else
        {
            // odd row - interpolate
            const unsigned short *src0 = raw + (y / 2) * fullw + xofs;
            const unsigned short *src1 = src0 + fullw;
            for (int x = 0; x < framew; x++)
                *dst++ = (*src0++ + *src1++) / 2;
        }
    }

    if ((frame.GetBottom() & 1) && frame.GetBottom() == FullSize.GetHeight() - 1)
    {
        unsigned short *dst = img.ImageData + frame.GetBottom() * fullw + xofs;
        const unsigned short *src = dst - fullw;
        memcpy(dst, src, framew * sizeof(unsigned short));
    }

    return false;
}

static bool InitImgInterlacedSquare(usImage& img, const wxSize& FullSize, bool subframe, const wxRect& frame,
    const sxccd_params_t& ccdparams, int binning, const usImage& tmp)
{
    // pixels are vertically binned. resample to create square, un-binned pixels
    //
    //  xsize = number of columns (752)
    //  ysize = number of rows read from camera (290)

    float const pw = ccdparams.pix_width * binning;   // bin1: 8.6, bin2: 17.2
    float const ph = ccdparams.pix_height;  // reported value is for the binned pixel (16.6)
    float const r0 = pw / ph;

    if (img.Init(FullSize))
        return true;

    if (subframe)
    {
        img.Subframe = frame;
        img.Clear();
    }

    const unsigned short *raw = tmp.ImageData;
    int const fullw = FullSize.GetWidth();
    int const framew = frame.GetWidth();
    int const xofs = frame.GetLeft();

    float y0 = frame.GetTop() * pw;
    float y1 = y0 + pw;
    int p0 = (int) floor(y0 / ph);

    for (int row = frame.GetTop(); row <= frame.GetBottom(); row++)
    {
        float yp1 = floor(y1 / ph);
        int p1 = (int) yp1;
        yp1 *= ph;

        unsigned short *dst = img.ImageData + row * fullw + xofs;
        if (p1 == p0)
        {
            const unsigned short *src = raw + p0 * fullw + xofs;
            for (int x = 0; x < framew; x++)
                *dst++ = (unsigned short)(r0 * (float) *src++);
        }
        else
        {
            float const r0 = (yp1 - y0) / ph;
            float const r1 = (y1 - yp1) / ph;
            const unsigned short *src0 = raw + p0 * fullw + xofs;
            const unsigned short *src1 = raw + p1 * fullw + xofs;
            for (int x = 0; x < framew; x++)
                *dst++ = (unsigned short)(r0 * (float)*src0++ + r1 * (float)*src1++);
        }

        y0 = y1;
        p0 = p1;
        y1 += pw;
    }

    return false;
}

static bool InitImgProgressive(usImage& img, unsigned int xofs, unsigned int yofs, unsigned int xsize,
    unsigned int ysize, bool subframe, const wxSize& FullSize, const unsigned short *raw)
{
    if (img.Init(FullSize))
        return true;

    if (subframe)
    {
        const unsigned short *src = raw;
        img.Subframe = wxRect(xofs, yofs, xsize, ysize);
        img.Clear();
        for (unsigned int y = 0; y < ysize; y++)
        {
            unsigned short *dst = img.ImageData + (yofs + y) * FullSize.GetWidth() + xofs;
            memcpy(dst, src, xsize * sizeof(unsigned short));
            src += xsize;
        }
    }
    else
    {
        memcpy(img.ImageData, raw, img.NPixels * sizeof(unsigned short));
    }

    return false;
}

inline static void swap(unsigned short *&a, unsigned short *&b)
{
    unsigned short *tmp = a;
    a = b;
    b = tmp;
}

static bool ClearPixels(sxccd_handle_t sxHandle, unsigned short flags)
{
    int ret = sxClearPixels(sxHandle, flags, 0);
    if (ret == 0)
    {
        Debug.Write("sxClearPixels failed!\n");
        return false;
    }
    return true;
}

static bool LatchPixels(sxccd_handle_t sxHandle, unsigned short flags, unsigned short xoffset,
    unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin,
    unsigned short ybin)
{
    int ret = sxLatchPixels(sxHandle, flags, 0, xoffset, yoffset, width, height, xbin, ybin);
    if (ret == 0)
    {
        Debug.Write("sxLatchPixels failed!\n");
        return false;
    }
    return true;
}

static bool ExposePixels(sxccd_handle_t sxHandle, unsigned short flags, unsigned short xoffset,
    unsigned short yoffset, unsigned short width, unsigned short height, unsigned short xbin,
    unsigned short ybin, unsigned int msec)
{
    int ret = sxExposePixels(sxHandle, flags, 0, xoffset, yoffset, width, height, xbin, ybin, msec);
    if (ret == 0)
    {
        Debug.Write("sxExposePixels failed!\n");
        return false;
    }
    return true;
}

static bool ReadPixels(sxccd_handle_t sxHandle, unsigned short *pixels, unsigned int count)
{
    int ret;

    ret = sxReadPixels(sxHandle, pixels, count);

    if (ret != count * sizeof(unsigned short))
    {
        Debug.Write(wxString::Format("sxReadPixels failed! ret = %d\n", ret));
        return false;
    }
    return true;
}

bool CameraSXV::Capture(int duration, usImage& img, int options, const wxRect& subframeArg)
{
    bool takeSubframe = UseSubframes;
    wxRect subframe(subframeArg);

    if (subframe.width <= 0 || subframe.height <= 0)
    {
        takeSubframe = false;
    }

    if (HasShutter && ShutterClosed)
    {
        sxSetShutter(hCam, 1);  // Close the shutter if needed
        wxMilliSleep(200);
    }

    if (Binning != m_prevBin)
    {
        InitFrameSizes();
        if (tmpImg.Init(m_darkFrameSize))
        {
            DisconnectWithAlert(CAPT_FAIL_MEMORY);
            return true;
        }
        m_prevBin = Binning;
        takeSubframe = false; // subframe may be out of bounds now
    }

    unsigned short xbin, ybin;
    if (Interlaced)
    {
        xbin = Binning;
        ybin = 1;
    }
    else
    {
        xbin = ybin = Binning;
    }

    // driver expects offsets and sizes in un-binned pixel coordinates
    unsigned short xofs, yofs;
    unsigned short xsize, ysize;

    if (takeSubframe)
    {
        xofs = subframe.GetLeft() * xbin;
        xsize = subframe.GetWidth() * xbin;

        if (Interlaced)
        {
            // Interlaced cams are run in "high speed" mode (vertically binned)

            if (options & CAPTURE_RECON)
            {
                if (SquarePixels)
                {
                    //  incoming subframe coordinates are in squared pixel coordinate system, convert to camera pixel coordinates
                    float r = CCDParams.pix_width * (float) xbin / CCDParams.pix_height;
                    unsigned int y0 = (unsigned int) floor(subframe.GetTop() * r);
                    unsigned int y1 = (unsigned int) floor(subframe.GetBottom() * r);
                    yofs = y0;
                    ysize = y1 - y0 + 1;
                }
                else
                {
                    if (Binning == 1)
                    {
                        unsigned int y0 = (unsigned int) subframe.GetTop() / 2;
                        // interpolation may require the next row
                        unsigned int y1 = (unsigned int)(subframe.GetBottom() + 1) / 2;
                        if (y1 >= CCDParams.height)
                            y1 = CCDParams.height - 1;
                        yofs = y0;
                        ysize = y1 - y0 + 1;
                    }
                    else
                    {
                        // no interpolation
                        yofs = (unsigned int) subframe.GetTop();
                        ysize = subframe.GetHeight();
                    }
                }
            }
            else // no RECON
            {
                // subframe represents actual binned pixels
                yofs = (unsigned int) subframe.GetTop();
                unsigned int y1 = (unsigned int) subframe.GetBottom();
                if (y1 > CCDParams.height - 1)
                    y1 = CCDParams.height - 1;
                ysize = y1 - yofs + 1;
            }
        }
        else
        {
            // not interlaced (progressive)
            yofs = subframe.GetTop() * Binning;
            ysize = subframe.GetHeight() * Binning;
        }
    }
    else
    {
        // not using subframe
        subframe = FullSize;
        xofs = 0;
        yofs = 0;
        xsize = CCDParams.width;
        ysize = CCDParams.height;
    }

    unsigned int nPixelsToRead = (xsize / xbin) * (ysize / ybin);

    if (nPixelsToRead > RawDataSize)
    {
        delete[] RawData;
        RawData = new unsigned short[nPixelsToRead];
        if (!RawData)
        {
            RawDataSize = 0;
            DisconnectWithAlert(CAPT_FAIL_MEMORY);
            return true;
        }
        RawDataSize = nPixelsToRead;
    }

    // Do exposure
    if (IsCMOSGuider(CameraModel))
    {
        ClearPixels(hCam, SXCCD_EXP_FLAGS_NOWIPE_FRAME);
        ExposePixels(hCam, SXCCD_EXP_FLAGS_FIELD_ODD, xofs, yofs, xsize, ysize, xbin, ybin, duration);
    }
    else
    {
        // use camera internal timer for durations less than 1 second

        ClearPixels(hCam, 0);

        if (duration < 1000)
        {
            ExposePixels(hCam, SXCCD_EXP_FLAGS_FIELD_BOTH, xofs, yofs, xsize, ysize, xbin, ybin, duration);
        }
        else
        {
            WorkerThread::MilliSleep(duration, WorkerThread::INT_ANY);
            LatchPixels(hCam, SXCCD_EXP_FLAGS_FIELD_BOTH, xofs, yofs, xsize, ysize, xbin, ybin);
        }
    }

    // do not return without reading pixels or camera will hang
    // if (WorkerThread::InterruptRequested())
    //    return true;

    if (!ReadPixels(hCam, RawData, nPixelsToRead))  // stop exposure and read but only the one frame
    {
        DisconnectWithAlert(_("Lost connection to camera"), RECONNECT);
        return true;
    }

    if (HasShutter && ShutterClosed)
    {
        sxSetShutter(hCam, 0);  // Open it back up
        wxMilliSleep(200);
    }

    // Re-assemble image

    if (!Interlaced || (Binning > 1 && !SquarePixels))
    {
        bool error;

        if (IsCMOSGuider(CameraModel))
            error = InitImgCMOSGuider(img, FullSize, RawData);
        else
            error = InitImgProgressive(img, xofs / xbin, yofs / ybin, xsize / xbin, ysize / ybin, takeSubframe, FullSize, RawData);

        if (error)
        {
            DisconnectWithAlert(CAPT_FAIL_MEMORY);
            return true;
        }

        if (options & CAPTURE_SUBTRACT_DARK)
            SubtractDark(img);

        return false;
    }

    // interlaced

    // prepare to for dark subtraction by copying the camera frame to the appropriate location in tmpImg

    if (takeSubframe)
    {
        tmpImg.Clear();
        const unsigned short *src = RawData;
        for (int y = 0; y < ysize / ybin; y++)
        {
            unsigned short *dst = tmpImg.ImageData + (yofs / ybin + y) * FullSize.GetWidth() + xofs / xbin;
            memcpy(dst, src, xsize / xbin * sizeof(unsigned short));
            src += xsize / xbin;
        }
        tmpImg.Subframe = wxRect(xofs / xbin, yofs / ybin, xsize / xbin, ysize / ybin);
    }
    else
    {
        swap(RawData, tmpImg.ImageData);
        RawDataSize = tmpImg.NPixels;
        tmpImg.Subframe = wxRect();
    }

    if (options & CAPTURE_SUBTRACT_DARK)
        SubtractDark(tmpImg);

    if (options & CAPTURE_RECON)
    {
        bool error;

        if (SquarePixels)
            error = InitImgInterlacedSquare(img, FullSize, takeSubframe, subframe, CCDParams, Binning, tmpImg);
        else
            error = InitImgInterlacedInterp(img, FullSize, takeSubframe, subframe, tmpImg);

        if (error)
        {
            DisconnectWithAlert(CAPT_FAIL_MEMORY);
            return true;
        }
    }
    else
    {
        if (img.Init(tmpImg.Size))
        {
            DisconnectWithAlert(CAPT_FAIL_MEMORY);
            return true;
        }

        if (takeSubframe)
            img.Subframe = tmpImg.Subframe;

        img.SwapImageData(tmpImg);
    }

    return false;
}

bool CameraSXV::ST4PulseGuideScope(int direction, int duration)
{
    // Guide port values
    // West = 1
    // East = 8
    // North = 2
    // South = 4
    unsigned char dircmd = 0;
    switch (direction) {
        case WEST:
            dircmd = (unsigned char) 1;
            break;
        case EAST:
            dircmd = (unsigned char) 8;
            break;
        case NORTH:
            dircmd = (unsigned char) 2;
            break;
        case SOUTH:
            dircmd = (unsigned char) 4;
            break;
    }
    sxSetSTAR2000(hCam,dircmd);
    WorkerThread::MilliSleep(duration);
    dircmd = 0;
    sxSetSTAR2000(hCam,dircmd);

    return false;
}

GuideCamera *SXVCameraFactory::MakeSXVCamera()
{
    return new CameraSXV();
}

#endif // SXV
