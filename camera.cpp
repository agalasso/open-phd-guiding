/*
 *  camera.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
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

// General camera routines not specific to any one cam

#include "phd.h"

#include "camera.h"
#include "gear_simulator.h"

#include <wx/stdpaths.h>

static const int DefaultGuideCameraGain = 95;
static const int DefaultGuideCameraTimeoutMs = 15000;
static const bool DefaultUseSubframes = false;
static const int DefaultReadDelay = 150;

const double GuideCamera::UnknownPixelSize = 0.0;

wxSize UNDEFINED_FRAME_SIZE = wxSize(0, 0);

#if defined (ATIK16)
# include "cam_atik16.h"
#endif

#if defined (IOPTRON_CAMERA)
# include "cam_ioptron.h"
#endif

#if defined (LE_SERIAL_CAMERA)
# include "cam_LESerialWebcam.h"
#endif

#if defined (LE_PARALLEL_CAMERA)
# include "cam_LEParallelwebcam.h"
#endif

#if defined (LE_LXUSB_CAMERA)
# include "cam_LELXUSBwebcam.h"
#endif

#if defined (QGUIDE)
# include "cam_qguide.h"
#endif

#if defined (CAM_QHY5)
# include "cam_qhy5.h"
#endif

#if defined (QHY_CAMERA)
# include "cam_qhy.h"
#endif

#if defined (SVB_CAMERA)
# include "cam_svb.h"
#endif

#if defined (ZWO_ASI)
# include "cam_zwo.h"
#endif

#if defined (TOUPTEK_CAMERA)
# include "cam_touptek.h"
#endif

#if defined (SKYRAIDER_CAMERA)
# include "cam_skyraider.h"
#endif

#if defined (ALTAIR)
# include "cam_altair.h"
#endif

#if defined (ORION_DSCI)
# include "cam_StarShootDSCI.h"
#endif

#if defined (OS_PL130)
# include "cam_OSPL130.h"
#endif

#if defined (VFW_CAMERA)
# include "cam_vfw.h"
#endif

#if defined (OGMA_CAMERA)
# include "cam_ogma.h"
#endif

#if defined (OPENCV_CAMERA)
# include "cam_opencv.h"
#endif

#if defined (WDM_CAMERA)
# include "cam_wdm.h"
#endif

#if defined (STARFISH_CAMERA)
# include "cam_starfish.h"
#endif

#if defined (SXV)
# include "cam_sxv.h"
#endif

#if defined (SBIG)
# include "cam_sbig.h"
#endif

#if defined (NEB_SBIG)
# include "cam_NebSBIG.h"
#endif

#if defined (FIREWIRE_CAMERA)
# include "cam_firewire.h"
#endif

#if defined (MEADE_DSI_CAMERA)
# include "cam_MeadeDSI.h"
#endif

#if defined (MORAVIAN_CAMERA)
# include "cam_moravian.h"
#endif

#if defined (SSAG)
# include "cam_ssag.h"
#endif

#if defined (OPENSSAG_CAMERA)
# include "cam_openssag.h"
#endif

#if defined (KWIQGUIDER_CAMERA)
# include "cam_KWIQGuider.h"
#endif

#if defined (SSPIAG)
# include "cam_sspiag.h"
#endif

#if defined (INOVA_PLC)
# include "cam_INovaPLC.h"
#endif

#if defined (ASCOM_CAMERA)
# include "cam_ascom.h"
#endif

#if defined (INDI_CAMERA)
# include "cam_indi.h"
#endif

#if defined(SBIGROTATOR_CAMERA)
# include "cam_sbigrotator.h"
#endif

#if defined (V4L_CAMERA)
# include "cam_VIDEODEVICE.h"
extern "C" {
#include <libudev.h>
}
#endif

const wxString GuideCamera::DEFAULT_CAMERA_ID = wxEmptyString;

double GuideCamera::GetProfilePixelSize()
{
    return pConfig->Profile.GetDouble("/camera/pixelsize", UnknownPixelSize);
}

GuideCamera::GuideCamera()
{
    Connected = false;
    m_hasGuideOutput = false;
    PropertyDialogType = PROPDLG_NONE;
    HasPortNum = false;
    HasDelayParam = false;
    HasGainControl = false;
    HasShutter = false;
    ShutterClosed = false;
    HasSubframes = false;
    HasCooler = false;
    FullSize = UNDEFINED_FRAME_SIZE;
    UseSubframes = pConfig->Profile.GetBoolean("/camera/UseSubframes", DefaultUseSubframes);
    ReadDelay = pConfig->Profile.GetInt("/camera/ReadDelay", DefaultReadDelay);
    GuideCameraGain = pConfig->Profile.GetInt("/camera/gain", DefaultGuideCameraGain);
    m_timeoutMs = pConfig->Profile.GetInt("/camera/TimeoutMs", DefaultGuideCameraTimeoutMs);
    m_saturationADU = (unsigned short) wxMin(pConfig->Profile.GetInt("/camera/SaturationADU", 0), 65535);
    m_saturationByADU = pConfig->Profile.GetBoolean("/camera/SaturationByADU", true);
    m_pixelSize = GetProfilePixelSize();
    MaxBinning = 1;
    Binning = pConfig->Profile.GetInt("/camera/binning", 1);
    CurrentDarkFrame = nullptr;
    CurrentDefectMap = nullptr;
}

GuideCamera::~GuideCamera()
{
    ClearDarks();
    ClearDefectMap();
}

static int CompareNoCase(const wxString& first, const wxString& second)
{
    return first.CmpNoCase(second);
}

static wxString INDICamName()
{
    wxString indicam = pConfig->Profile.GetString("/indi/INDIcam", wxEmptyString);
    return indicam.empty() ? _T("INDI Camera") : wxString::Format("INDI Camera [%s]", indicam);
}

wxArrayString GuideCamera::GuideCameraList()
{
    wxArrayString CameraList;

    CameraList.Add(_("None"));
#if defined (ASCOM_CAMERA)
    wxArrayString ascomCameras = ASCOMCameraFactory::EnumAscomCameras();
    for (unsigned int i = 0; i < ascomCameras.Count(); i++)
        CameraList.Add(ascomCameras[i]);
#endif
#if defined (ATIK16)
    CameraList.Add(_T("Atik 16 series, mono"));
    CameraList.Add(_T("Atik 16 series, color"));
#endif
#if defined (ATIK_GEN3)
    CameraList.Add(_T("Atik Gen3, mono"));
    CameraList.Add(_T("Atik Gen3, color"));
#endif
#if defined (QGUIDE)
    CameraList.Add(_T("CCD Labs Q-Guider"));
#endif
#if defined (STARFISH_CAMERA)
    CameraList.Add(_T("Fishcamp Starfish"));
#endif
#if defined (INOVA_PLC)
    CameraList.Add(_T("i-Nova PLC-M"));
#endif
#if defined (IOPTRON_CAMERA)
    CameraList.Add(_T("iOptron iGuider"));
#endif
#if defined (SSAG)
    CameraList.Add(_T("StarShoot Autoguider"));
#endif
#if defined (SSPIAG)
    CameraList.Add(_T("StarShoot Planetary Imager & Autoguider"));
#endif
#if defined (OS_PL130)
    CameraList.Add(_T("Opticstar PL-130M"));
    CameraList.Add(_T("Opticstar PL-130C"));
#endif
#if defined (ORION_DSCI)
    CameraList.Add(_T("Orion StarShoot DSCI"));
#endif
#if defined (OPENSSAG_CAMERA)
    CameraList.Add(_T("Orion StarShoot Autoguider"));
#endif
#if defined (KWIQGUIDER_CAMERA)
    CameraList.Add(_T("KWIQGuider"));
#endif
#if defined (QGUIDE)
    CameraList.Add(_T("MagZero MZ-5"));
#endif
#if defined (MEADE_DSI_CAMERA)
    CameraList.Add(_T("Meade DSI I, II, or III"));
#endif
#if defined (MORAVIAN_CAMERA)
    CameraList.Add(_T("Moravian Camera"));
#endif
#if defined (CAM_QHY5)
    CameraList.Add(_T("QHY 5"));
#endif
#if defined (QHY_CAMERA)
    CameraList.Add(_T("QHY Camera"));
#endif
#if defined (ALTAIR)
	CameraList.Add(_T("Altair Camera"));
    CameraList.Add(_T("Altair Camera (2015/2016)"));
#endif
#if defined (ZWO_ASI)
    CameraList.Add(_T("ZWO ASI Camera"));
#endif
#if defined (TOUPTEK_CAMERA)
    CameraList.Add(_T("ToupTek Camera"));
    CameraList.Add(_T("Omegon Pro Camera"));
#endif
#if defined (SKYRAIDER_CAMERA)
    CameraList.Add(_T("MallinCam SkyRaider"));
#endif
#if defined (SBIG)
    CameraList.Add(_T("SBIG"));
#endif
#if defined (SBIGROTATOR_CAMERA)
    CameraList.Add(_T("SBIG Rotator"));
#endif
#if defined (SVB_CAMERA)
    CameraList.Add(_T("Svbony Camera"));
#endif
#if defined (SXV)
    CameraList.Add(_T("Starlight Xpress SXV"));
#endif
#if defined (FIREWIRE_CAMERA)
    CameraList.Add(_T("The Imaging Source (DCAM Firewire)"));
#endif
#if defined (OGMA_CAMERA)
    CameraList.Add(_T("OGMA Camera"));
#endif
#if defined (OPENCV_CAMERA)
    CameraList.Add(_T("OpenCV webcam 1"));
    CameraList.Add(_T("OpenCV webcam 2"));
#endif
#if defined (WDM_CAMERA)
    CameraList.Add(_T("Windows WDM-style webcam camera"));
#endif
#if defined (VFW_CAMERA)
    CameraList.Add(_T("Windows VFW-style webcam camera (older & SAC8)"));
#endif
#if defined (LE_LXUSB_CAMERA)
    CameraList.Add(_T("Long exposure LXUSB webcam"));
#endif
#if defined (LE_PARALLEL_CAMERA)
    CameraList.Add(_T("Long exposure Parallel webcam"));
#endif
#if defined (LE_SERIAL_CAMERA)
    CameraList.Add(_T("Long exposure Serial webcam"));
#endif
#if defined (INDI_CAMERA)
    CameraList.Add(INDICamName());
#endif
#if defined (V4L_CAMERA)
    if (true == Camera_VIDEODEVICE.ProbeDevices()) {
        CameraList.Add(_T("V4L(2) Camera"));
    }
#endif
#if defined (SIMULATOR)
    CameraList.Add(_T("Simulator"));
#endif

#if defined (NEB_SBIG)
    CameraList.Add(_T("Guide chip on SBIG cam in Nebulosity"));
#endif

    CameraList.Sort(&CompareNoCase);

    return CameraList;
}

GuideCamera *GuideCamera::Factory(const wxString& choice)
{
    GuideCamera *pReturn = nullptr;

    try
    {
        if (choice.IsEmpty())
        {
            throw ERROR_INFO("CameraFactory called with choice.IsEmpty()");
        }

        Debug.AddLine(wxString::Format("CameraFactory(%s)", choice));

        if (false) // so else ifs can follow
        {
        }

        // Chack ASCOM and INDI first since those choices may match match other choices below (like Simulator)
#if defined (ASCOM_CAMERA)
        else if (choice.Contains(_T("ASCOM"))) {
            pReturn = ASCOMCameraFactory::MakeASCOMCamera(choice);
        }
#endif
#if defined (INDI_CAMERA)
        else if (choice.Contains(_T("INDI"))) {
            pReturn = INDICameraFactory::MakeINDICamera();
        }
#endif
#if defined (IOPTRON_CAMERA)
        else if (choice == _T("iOptron iGuider"))
            pReturn = IoptronCameraFactory::MakeIoptronCamera();
#endif
        else if (choice == _("None"))
            pReturn = nullptr;
        else if (choice == _T("Simulator"))
            pReturn = GearSimulator::MakeCamSimulator();
#if defined (ATIK16)
        else if (choice.StartsWith("Atik 16 series"))
        {
            bool hsmodel = false;
            bool color = choice.Find(_T("color")) != wxNOT_FOUND;
            pReturn = AtikCameraFactory::MakeAtikCamera(hsmodel, color);
        }
#endif
#if defined (ATIK_GEN3)
        else if (choice.StartsWith(_T("Atik Gen3")))
        {
            bool hsmodel = true;
            bool color = choice.Find(_T("color")) != wxNOT_FOUND;
            pReturn = AtikCameraFactory::MakeAtikCamera(hsmodel, color);
        }
#endif
#if defined (QGUIDE)
        else if (choice.Contains(_T("CCD Labs Q-Guider")))
        {
            pReturn = new CameraQGuider();
            pReturn->Name = _T("Q-Guider");
        }
        else if (choice.Contains(_T("MagZero MZ-5")))
        {
            pReturn = new CameraQGuider();
            pReturn->Name = _T("MagZero MZ-5");
        }
#endif
#if defined (QHY_CAMERA)
        else if (choice == _T("QHY Camera"))
            pReturn = QHYCameraFactory::MakeQHYCamera();
#endif
#if defined(ALTAIR)
        else if (choice == _T("Altair Camera"))
            pReturn = AltairCameraFactory::MakeAltairCamera(ALTAIR_CAM_CURRENT);
        else if (choice == _T("Altair Camera (2015/2016)"))
            pReturn = AltairCameraFactory::MakeAltairCamera(ALTAIR_CAM_LEGACY);
#endif
#if defined(ZWO_ASI)
        else if (choice == _T("ZWO ASI Camera"))
            pReturn = ZWOCameraFactory::MakeZWOCamera();
#endif
#if defined(TOUPTEK_CAMERA)
        else if (choice == _T("ToupTek Camera") ||
                 choice == _T("Omegon Pro Camera"))
        {
            pReturn = ToupTekCameraFactory::MakeToupTekCamera();
        }
#endif
#if defined(SKYRAIDER_CAMERA)
        else if (choice == _T("MallinCam SkyRaider"))
            pReturn = SkyraiderCameraFactory::MakeSkyraiderCamera();
#endif
#if defined (CAM_QHY5) // must come afer other QHY 5's since this pattern would match them
        else if (choice.Contains(_T("QHY 5")))
            pReturn = new CameraQHY5();
#endif
#if defined (OPENSSAG_CAMERA)
        else if (choice.Contains(_T("Orion StarShoot Autoguider")))
            pReturn = new CameraOpenSSAG();
#endif
#if defined (KWIQGUIDER_CAMERA)
        else if (choice.Contains(_T("KWIQGuider")))
            pReturn = KWIQGuiderCameraFactory::MakeKWIQGuiderCamera();
#endif
#if defined (SSAG)
        else if (choice.Contains(_T("StarShoot Autoguider")))
            pReturn = SSAGCameraFactory::MakeSSAGCamera();
#endif
#if defined (SSPIAG)
        else if (choice.Contains(_T("StarShoot Planetary Imager & Autoguider")))
            pReturn = new CameraSSPIAG();
#endif
#if defined (ORION_DSCI)
        else if (choice.Contains(_T("Orion StarShoot DSCI")))
            pReturn = new CameraStarShootDSCI();
#endif
#if defined(SVB_CAMERA)
        else if (choice == _T("Svbony Camera"))
            pReturn = SVBCameraFactory::MakeSVBCamera();
#endif
#if defined(OGMA_CAMERA)
        else if (choice == _T("OGMA Camera"))
            pReturn = OGMACameraFactory::MakeOGMACamera();
#endif
#if defined (OPENCV_CAMERA)
        else if (choice.Contains(_T("OpenCV webcam")))
        {
            int dev = 0;
            if (choice.Contains(_T("2")))
            {
                dev = 1;
            }
            pReturn = new CameraOpenCV(dev);
        }
#endif
#if defined (WDM_CAMERA)
        else if (choice.Contains(_T("Windows WDM")))
            pReturn = WDMCameraFactory::MakeWDMCamera();
#endif
#if defined (VFW_CAMERA)
        else if (choice.Contains(_T("Windows VFW")))
            pReturn = new CameraVFW();
#endif
#if defined (LE_SERIAL_CAMERA)
        else if (choice.Contains(_T("Long exposure Serial webcam")))
            pReturn = LESerialWebcamCameraFactory::MakeLESerialWebcamCamera();
#endif
#if defined (LE_PARALLEL_CAMERA)
        else if (choice.Contains( _T("Long exposure Parallel webcam")))
            pReturn = LEParallelWebcamCameraFactory::MakeLEParallelWebcamCamera();
#endif
#if defined (LE_LXUSB_CAMERA)
        else if (choice.Contains(_T("Long exposure LXUSB webcam")))
            pReturn = LELxUsbWebcamCameraFactory::MakeLELxUsbWebcamCamera();
#endif
#if defined (MEADE_DSI_CAMERA)
        else if (choice.Contains(_T("Meade DSI I, II, or III")))
            pReturn = DSICameraFactory::MakeDSICamera();
#endif
#if defined(MORAVIAN_CAMERA)
        else if (choice == _T("Moravian Camera"))
            pReturn = MoravianCameraFactory::MakeMoravianCamera();
#endif
#if defined (STARFISH_CAMERA)
        else if (choice.Contains(_T("Fishcamp Starfish")))
            pReturn = StarfishCameraFactory::MakeStarfishCamera();
#endif
#if defined (SXV)
        else if (choice.Contains(_T("Starlight Xpress SXV")))
            pReturn = SXVCameraFactory::MakeSXVCamera();
#endif
#if defined (OS_PL130)
        else if (choice.Contains(_T("Opticstar PL-130M")))
        {
            Camera_OSPL130.Color = false;
            Camera_OSPL130.Name = _T("Opticstar PL-130M");
            pReturn = new Camera_OSPL130Class();
        }
        else if (choice.Contains(_T("Opticstar PL-130C")))
        {
            Camera_OSPL130.Color = true;
            Camera_OSPL130.Name = _T("Opticstar PL-130C");
            pReturn = new Camera_OSPL130Class();
        }
#endif
#if defined (NEB_SBIG)
        else if (choice.Contains(_T("Nebulosity")))
            pReturn = new CameraNebSBIG();
#endif
#if defined (SBIGROTATOR_CAMERA)
        // must go above SBIG
        else if (choice.Contains(_T("SBIG Rotator")))
            pReturn = SBIGRotatorCameraFactory::MakeSBIGRotatorCamera();
#endif
#if defined (SBIG)
        else if (choice.Contains(_T("SBIG")))
            pReturn = SBIGCameraFactory::MakeSBIGCamera();
#endif
#if defined (FIREWIRE_CAMERA)
        else if (choice.Contains(_T("The Imaging Source (DCAM Firewire)")))
            pReturn = new CameraFirewire();
#endif
#if defined (INOVA_PLC)
        else if (choice.Contains(_T("i-Nova PLC-M")))
            pReturn = new CameraINovaPLC();
#endif
#if defined (V4L_CAMERA)
        else if (choice.Contains(_T("V4L(2) Camera")))
        {
            // There is at least ONE V4L(2) device ... let's find out exactly
            DeviceInfo *deviceInfo = nullptr;

            if (Camera_VIDEODEVICE.NumberOfDevices() == 1)
            {
                deviceInfo = Camera_VIDEODEVICE.GetDeviceAtIndex(0);

                Camera_VIDEODEVICE.SetDevice(deviceInfo->getDeviceName());
                Camera_VIDEODEVICE.SetVendor(deviceInfo->getVendorId());
                Camera_VIDEODEVICE.SetModel(deviceInfo->getModelId());

                Camera_VIDEODEVICE.Name = deviceInfo->getProduct();
            }
            else
            {
                wxArrayString choices;
                int choice = 0;

                if ((choice = wxGetSinglechoiceIndex(_("Select your camera"), _T("V4L(2) devices"), Camera_VIDEODEVICE.GetProductArray(choices))) != -1)
                {
                    deviceInfo = Camera_VIDEODEVICE.GetDeviceAtIndex(choice);

                    Camera_VIDEODEVICE.SetDevice(deviceInfo->getDeviceName());
                    Camera_VIDEODEVICE.SetVendor(deviceInfo->getVendorId());
                    Camera_VIDEODEVICE.SetModel(deviceInfo->getModelId());

                    Camera_VIDEODEVICE.Name = deviceInfo->getProduct();
                }
                else
                {
                    throw ERROR_INFO("CameraFactory invalid V4L choice");
                }
            }

            pReturn = new Camera_VIDEODEVICEClass();
        }
#endif
        else
        {
            throw ERROR_INFO("CameraFactory: Unknown camera choice");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        if (pReturn)
        {
            delete pReturn;
            pReturn = nullptr;
        }
    }

    return pReturn;
}

bool GuideCamera::HandleSelectCameraButtonClick(wxCommandEvent&)
{
    return false; // not handled
}

bool GuideCamera::EnumCameras(wxArrayString& names, wxArrayString& ids)
{
    return true; // error
}

bool GuideCamera::CamConnectFailed(const wxString& errorMessage)
{
    pFrame->Alert(errorMessage);
    return true; // error
}

bool GuideCamera::SetCameraGain(int cameraGain)
{
    bool bError = false;

    try
    {
        if (cameraGain < 0)
        {
            throw ERROR_INFO("cameraGain < 0");
        }
        else if (cameraGain > 100)
        {
            throw ERROR_INFO("cameraGain > 100");
        }
        GuideCameraGain = cameraGain;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        GuideCameraGain = DefaultGuideCameraGain;
    }

    pConfig->Profile.SetInt("/camera/gain", GuideCameraGain);

    if (pFrame)
        pFrame->UpdateCameraSettings();

    return bError;
}

int GuideCamera::GetDefaultCameraGain()
{
    return DefaultGuideCameraGain;
}

bool GuideCamera::SetBinning(int binning)
{
    if (binning < 1)
        binning = 1;
    if (binning > MaxBinning)
        binning = MaxBinning;

    Debug.Write(wxString::Format("camera: set binning = %u\n", (unsigned int) binning));

    Binning = binning;
    pConfig->Profile.SetInt("/camera/binning", binning);

    return false;
}

void GuideCamera::SetTimeoutMs(int ms)
{
    static const int MIN_TIMEOUT_MS = 5000;

    m_timeoutMs = wxMax(ms, MIN_TIMEOUT_MS);

    pConfig->Profile.SetInt("/camera/TimeoutMs", m_timeoutMs);
}

void GuideCamera::SetSaturationByADU(bool saturationByADU, unsigned short saturationADU)
{
    m_saturationByADU = saturationByADU;
    pConfig->Profile.SetBoolean("/camera/SaturationByADU", saturationByADU);

    if (saturationByADU)
    {
        m_saturationADU = saturationADU;
        pConfig->Profile.SetInt("/camera/SaturationADU", saturationADU);
        Debug.Write(wxString::Format("Saturation detection set to Max-ADU value %d\n", saturationADU));
    }
    else
        Debug.Write("Saturation detection set to star-profile-mode\n");
}

bool GuideCamera::SetCameraPixelSize(double pixel_size)
{
    bool bError = false;

    try
    {
        if (pixel_size <= 0.0)
        {
            throw ERROR_INFO("pixel_size <= 0");
        }

        m_pixelSize = pixel_size;
        if (pFrame->pStatsWin)
            pFrame->pStatsWin->ResetImageSize();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_pixelSize = UnknownPixelSize;
    }

    pConfig->Profile.SetDouble("/camera/pixelsize", m_pixelSize);

    return bError;
}

bool GuideCamera::SetCoolerOn(bool on)
{
    return true; // error
}

bool GuideCamera::SetCoolerSetpoint(double temperature)
{
    return true; // error
}

bool GuideCamera::GetCoolerStatus(bool *on, double *setpoint, double *power, double *temperature)
{
    return true; // error
}

bool GuideCamera::GetSensorTemperature(double *temperature)
{
    return true; // error
}

CameraConfigDialogPane *GuideCamera::GetConfigDialogPane(wxWindow *pParent)
{
    return new CameraConfigDialogPane(pParent, this);
}

static wxSpinCtrl *NewSpinnerInt(wxWindow *parent, int width, int val, int minval, int maxval, int inc)
{
    wxSpinCtrl *pNewCtrl = pFrame->MakeSpinCtrl(parent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, minval, maxval, val);
    pNewCtrl->SetValue(val);
    return pNewCtrl;
}

static wxSpinCtrlDouble *NewSpinnerDouble(wxWindow *parent, int width, double val, double minval, double maxval, double inc,
                                          const wxString& tooltip)
{
    wxSpinCtrlDouble *pNewCtrl = pFrame->MakeSpinCtrlDouble(parent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, minval, maxval, val, inc);
    pNewCtrl->SetDigits(2);
    pNewCtrl->SetToolTip(tooltip);
    return pNewCtrl;
}

CameraConfigDialogPane::CameraConfigDialogPane(wxWindow *pParent, GuideCamera *pCamera)
    : ConfigDialogPane(_("Camera Settings"), pParent)
{
    m_pParent = pParent;
}

static void MakeBold(wxControl *ctrl)
{
    wxFont font = ctrl->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    ctrl->SetFont(font);
}

void CameraConfigDialogPane::LayoutControls(GuideCamera *pCamera, BrainCtrlIdMap& CtrlMap)
{
    wxStaticBoxSizer *pGenGroup = new wxStaticBoxSizer(wxVERTICAL, m_pParent, _("General Properties"));
    wxFlexGridSizer *pTopline = new wxFlexGridSizer(1, 3, 5, 10);
    // Generic controls
    wxSizerFlags def_flags = wxSizerFlags(0).Border(wxALL, 10).Expand();
    pTopline->Add(GetSizerCtrl(CtrlMap, AD_szNoiseReduction));
    pTopline->Add(GetSizerCtrl(CtrlMap, AD_szTimeLapse), wxSizerFlags(0).Border(wxLEFT, 110).Expand());
    pGenGroup->Add(pTopline, def_flags);
    pGenGroup->Add(GetSizerCtrl(CtrlMap, AD_szVariableExposureDelay), def_flags);
    pGenGroup->Add(GetSizerCtrl(CtrlMap, AD_szAutoExposure), def_flags);

    pGenGroup->Layout();

    // Specific controls
    wxStaticBoxSizer *pSpecGroup = new wxStaticBoxSizer(wxVERTICAL, m_pParent, _("Camera-Specific Properties"));
    if (pCamera)
    {
        wxFlexGridSizer *pDetailsSizer = new wxFlexGridSizer(6, 3, 15, 15);     // Will auto-shrink to fit
        // Create all possible property controls then disable individual controls later if camera doesn't support them.  This is safer for "omnibus" style drivers that
        // handle many cameras with different capabilities.  Exceptions are 'port' and 'LE-delay' which will be created conditionally
        wxSizerFlags spec_flags = wxSizerFlags(0).Border(wxALL, 10).Align(wxVERTICAL).Expand();
        pDetailsSizer->Add(GetSizerCtrl(CtrlMap, AD_szPixelSize));
        pDetailsSizer->Add(GetSizerCtrl(CtrlMap, AD_szGain));
        pDetailsSizer->Add(GetSizerCtrl(CtrlMap, AD_szCameraTimeout));
        pDetailsSizer->Add(GetSizerCtrl(CtrlMap, AD_szBinning));
        pDetailsSizer->Add(GetSingleCtrl(CtrlMap, AD_cbUseSubFrames), wxSizerFlags().Border(wxTOP, 3));
        pDetailsSizer->Add(GetSizerCtrl(CtrlMap, AD_szCooler));
        if (pCamera->HasDelayParam)
            pDetailsSizer->Add(GetSizerCtrl(CtrlMap, AD_szDelay));
        if (pCamera->HasPortNum)
            pDetailsSizer->Add(GetSizerCtrl(CtrlMap, AD_szPort));
        pSpecGroup->Add(pDetailsSizer, spec_flags);
        pSpecGroup->Layout();
    }
    else
    {
        wxStaticText *pNoCam = new wxStaticText(m_pParent, wxID_ANY, _("No camera specified"));
        pSpecGroup->Add(pNoCam, wxSizerFlags().Align(wxALIGN_CENTER_HORIZONTAL));
        pSpecGroup->Layout();

    }
    if (pCamera)
        pGenGroup->Add(GetSizerCtrl(CtrlMap, AD_szSaturationOptions), wxSizerFlags(0).Border(wxALL, 2).Expand());
    this->Add(pGenGroup, def_flags);
    if (pCamera && !pCamera->Connected)
    {
        wxStaticText *pNotConnected = new wxStaticText(m_pParent, wxID_ANY,
            _("Camera is not connected.  Property controls may change if you connect to it first."));
        MakeBold(pNotConnected);
        this->Add(pNotConnected, wxSizerFlags().Align(wxALIGN_CENTER_HORIZONTAL).Border(wxALL, 10));
    }

    this->Add(pSpecGroup, wxSizerFlags(0).Border(wxALL, 10).Expand());
    this->Layout();
    Fit(m_pParent);
}

CameraConfigDialogCtrlSet* GuideCamera::GetConfigDlgCtrlSet(wxWindow *pParent, GuideCamera *pCamera, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
{
    return new CameraConfigDialogCtrlSet(pParent, pCamera, pAdvancedDialog, CtrlMap);
}

CameraConfigDialogCtrlSet::CameraConfigDialogCtrlSet(wxWindow *pParent, GuideCamera *pCamera, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
    : ConfigDialogCtrlSet(pParent, pAdvancedDialog, CtrlMap),
      m_pUseSubframes(nullptr)
{
    int textWidth = StringWidth(_T("0000"));
    assert(pCamera);

    m_pCamera = pCamera;
    // Sub-frames
    m_pUseSubframes = new wxCheckBox(GetParentWindow(AD_cbUseSubFrames), wxID_ANY, _("Use Subframes"));
    AddCtrl(CtrlMap, AD_cbUseSubFrames, m_pUseSubframes, _("Check to only download subframes (ROIs). Sub-frame size is equal to search region size."));

    // Pixel size
    m_pPixelSize = NewSpinnerDouble(GetParentWindow(AD_szPixelSize), textWidth, m_pCamera->GetCameraPixelSize(), 0.0, 99.9, 0.1,
        _("Guide camera un-binned pixel size in microns. Used with the guide telescope focal length to display guiding error in arc-seconds."));
    AddLabeledCtrl(CtrlMap, AD_szPixelSize, _("Pixel size"), m_pPixelSize, "");

    // Gain control
    wxWindow *parent = GetParentWindow(AD_szGain);
    wxStaticText *label = new wxStaticText(parent, wxID_ANY, _("Camera gain") + _(": "));
    m_pCameraGain = NewSpinnerInt(parent, textWidth, 100, 0, 100, 1);
    m_pCameraGain->SetToolTip(
        /* xgettext:no-c-format */ _("Camera gain, default = 95%, lower if you experience noise or wish to guide on a very bright star. Not available on all cameras."));
    m_resetGain = new wxButton(GetParentWindow(AD_szGain), wxID_ANY, _("Reset"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_resetGain->SetToolTip(_("Reset gain to camera's default value (disabled when camera is not connected)"));
    m_resetGain->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& evt) {
            m_pCameraGain->SetValue(::pCamera->GetDefaultCameraGain());
        }
    );
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(label, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
    sizer->Add(m_pCameraGain, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
    sizer->Add(m_resetGain, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
    AddGroup(CtrlMap, AD_szGain, sizer);

    // Binning
    m_binning = 0;
    wxArrayString opts;
    m_pCamera->GetBinningOpts(&opts);
    int width = StringArrayWidth(opts);
    m_binning = new wxChoice(GetParentWindow(AD_szBinning), wxID_ANY, wxDefaultPosition,
        wxSize(width + 35, -1), opts);
    AddLabeledCtrl(CtrlMap, AD_szBinning, _("Binning"), m_binning, _("Camera pixel binning"));

    // Delay parameter
    if (m_pCamera->HasDelayParam)
    {
        m_pDelay = NewSpinnerInt(GetParentWindow(AD_szDelay), textWidth, 5, 0, 250, 150);
        AddLabeledCtrl(CtrlMap, AD_szDelay, _("Delay"), m_pDelay, _("LE Read Delay (ms) , Adjust if you get dropped frames"));
    }

    // Port number
    if (m_pCamera->HasPortNum)
    {
        wxString port_choices[] = {
            _T("Port 378"), _T("Port 3BC"), _T("Port 278"), _T("COM1"), _T("COM2"), _T("COM3"), _T("COM4"),
            _T("COM5"), _T("COM6"), _T("COM7"), _T("COM8"), _T("COM9"), _T("COM10"), _T("COM11"), _T("COM12"),
            _T("COM13"), _T("COM14"), _T("COM15"), _T("COM16"),
        };

        width = StringArrayWidth(port_choices, WXSIZEOF(port_choices));
        m_pPortNum = new wxChoice(GetParentWindow(AD_szPort), wxID_ANY, wxDefaultPosition,
            wxSize(width + 35, -1), WXSIZEOF(port_choices), port_choices);
        AddLabeledCtrl(CtrlMap, AD_szPort, _("LE Port"), m_pPortNum, _("Port number for long-exposure control"));
    }

    // Cooler
    wxSizer *sz = new wxBoxSizer(wxHORIZONTAL);
    m_coolerOn = new wxCheckBox(GetParentWindow(AD_szCooler), wxID_ANY, _("Cooler On"));
    m_coolerOn->SetToolTip(_("Turn camera cooler on or off"));
    sz->Add(m_coolerOn, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxRIGHT));
    m_coolerSetpt = NewSpinnerInt(GetParentWindow(AD_szDelay), textWidth, 5, -99, 99, 1);
    wxSizer *szt = MakeLabeledControl(AD_szCooler, _("Set Temperature"), m_coolerSetpt, _("Cooler setpoint temperature"));
    sz->Add(szt, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
    AddGroup(CtrlMap, AD_szCooler, sz);

    // Max ADU and related saturation choices in a single group
    width = StringWidth(_T("65535"));
    parent = GetParentWindow(AD_szSaturationOptions);
    m_camSaturationADU = new wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(1.5 * width, -1));
    m_camSaturationADU->SetToolTip(_("ADU level to determine saturation - 65535 for most 16-bit cameras, or 255 for 8-bit cameras."));
    m_SaturationByADU = new wxRadioButton(parent, wxID_ANY, _("Saturation by Max-ADU value:"));
    m_SaturationByADU->SetToolTip(_("Identify star saturation based on camera maximum-ADU value (recommended)"));
    m_SaturationByADU->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &CameraConfigDialogCtrlSet::OnSaturationChoiceChanged, this);
    wxStaticBoxSizer* szADUGroup = new wxStaticBoxSizer(wxHORIZONTAL, parent,
        wxEmptyString);
    szADUGroup->Add(m_SaturationByADU, wxSizerFlags().Border(wxTOP, 2));
    szADUGroup->Add(m_camSaturationADU, wxSizerFlags().Border(wxLEFT, 6));

    m_SaturationByProfile = new wxRadioButton(parent, wxID_ANY, _("Saturation via star-profile"));
    m_SaturationByProfile->SetToolTip(_("Identify star saturation based on flat-topped profile, regardless of brightness"));
    m_SaturationByProfile->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &CameraConfigDialogCtrlSet::OnSaturationChoiceChanged, this);
    wxFlexGridSizer* szSaturationGroup = new wxFlexGridSizer(1, 2, 5, 15);

    szSaturationGroup->Add(szADUGroup, wxSizerFlags().Border(wxALL, 3).Align(wxALIGN_CENTER_VERTICAL));
    szSaturationGroup->Add(m_SaturationByProfile, wxSizerFlags(0).Border(wxLEFT, 70).Expand().Align(wxALIGN_CENTER_VERTICAL));
    AddGroup(CtrlMap, AD_szSaturationOptions, szSaturationGroup);

    // Watchdog timeout
    m_timeoutVal = NewSpinnerInt(GetParentWindow(AD_szCameraTimeout), textWidth, 5, 5, 9999, 1);
    AddLabeledCtrl(CtrlMap, AD_szCameraTimeout, _("Disconnect nonresponsive          \ncamera after (seconds)"), m_timeoutVal,
        wxString::Format(_("The camera will be disconnected if it fails to respond for this long. "
        "The default value, %d seconds, should be appropriate for most cameras."), DefaultGuideCameraTimeoutMs / 1000));
}

void CameraConfigDialogCtrlSet::OnSaturationChoiceChanged(wxCommandEvent& event)
{
    m_camSaturationADU->Enable(m_SaturationByADU->GetValue());
}

static unsigned short SaturationValFromBPP(GuideCamera *cam)
{
    return (unsigned short) ((1U << cam->BitsPerPixel()) - 1);
}

void CameraConfigDialogCtrlSet::LoadValues()
{
    assert(m_pCamera);

    if (m_pCamera->HasSubframes)
    {
        m_pUseSubframes->SetValue(m_pCamera->UseSubframes);
    }
    else
    {
        m_pUseSubframes->Enable(false);
    }

    if (m_pCamera->HasGainControl)
    {
        m_pCameraGain->SetValue(m_pCamera->GetCameraGain());
        m_resetGain->Enable(m_pCamera->Connected);
    }
    else
    {
        m_pCameraGain->Enable(false);
        m_resetGain->Enable(false);
    }

    if (m_binning)
    {
        int idx = m_pCamera->Binning - 1;
        m_binning->Select(idx);
        m_prevBinning = idx + 1;
        // don't allow binning change when calibrating or guiding
        m_binning->Enable(!pFrame->pGuider || !pFrame->pGuider->IsCalibratingOrGuiding());
    }
    else
        m_binning->Enable(false);

    m_timeoutVal->SetValue(m_pCamera->GetTimeoutMs() / 1000);

    bool saturationByADU = m_pCamera->IsSaturationByADU();
    m_SaturationByADU->SetValue(saturationByADU);
    m_SaturationByProfile->SetValue(!saturationByADU);

    if (pConfig->Profile.HasEntry("/camera/SaturationADU"))
    {
        unsigned int maxADU = wxMin(pConfig->Profile.GetInt("/camera/SaturationADU", 0), 65535);
        m_camSaturationADU->SetValue(wxString::Format("%u", maxADU));
    }
    else
    {
        // first time initialization
        int val = SaturationValFromBPP(m_pCamera);
        Debug.Write(wxString::Format("initializing cam saturation ADU val to %d\n", val));
        m_camSaturationADU->SetValue(wxString::Format("%d", val));
    }
    wxCommandEvent dummy;
    OnSaturationChoiceChanged(dummy);

    // do not allow saturation detection changes unless the camera is connected.
    // The Max ADU value needs to know the camera's BPP which may not be available
    // unless the camera is connected
    if (!m_pCamera->Connected)
    {
        m_SaturationByADU->Enable(false);
        m_SaturationByProfile->Enable(false);
        m_camSaturationADU->Enable(false);
    }

    if (m_pCamera->HasDelayParam)
    {
        m_pDelay->SetValue(m_pCamera->ReadDelay);
    }

    if (m_pCamera->HasPortNum)
    {
        switch (m_pCamera->Port)
        {
        case 0x3BC:
            m_pPortNum->SetSelection(1);
            break;
        case 0x278:
            m_pPortNum->SetSelection(2);
            break;
        case 1:  // COM1
            m_pPortNum->SetSelection(3);
            break;
        case 2:  // COM2
            m_pPortNum->SetSelection(4);
            break;
        case 3:  // COM3
            m_pPortNum->SetSelection(5);
            break;
        case 4:  // COM4
            m_pPortNum->SetSelection(6);
            break;
        case 5:  // COM5
            m_pPortNum->SetSelection(7);
            break;
        case 6:  // COM6
            m_pPortNum->SetSelection(8);
            break;
        case 7:  // COM7
            m_pPortNum->SetSelection(9);
            break;
        case 8:  // COM8
            m_pPortNum->SetSelection(10);
            break;
        case 9:  // COM9
            m_pPortNum->SetSelection(11);
            break;
        case 10:  // COM10
            m_pPortNum->SetSelection(12);
            break;
        case 11:  // COM11
            m_pPortNum->SetSelection(13);
            break;
        case 12:  // COM12
            m_pPortNum->SetSelection(14);
            break;
        case 13:  // COM13
            m_pPortNum->SetSelection(15);
            break;
        case 14:  // COM14
            m_pPortNum->SetSelection(16);
            break;
        case 15:  // COM15
            m_pPortNum->SetSelection(17);
            break;
        case 16:  // COM16
            m_pPortNum->SetSelection(18);
            break;
        default:
            m_pPortNum->SetSelection(0);
            break;
        }

        m_pPortNum->Enable(!pFrame->CaptureActive);
    }

    double pxSize;
    if (m_pCamera->GetDevicePixelSize(&pxSize))         // true=>error
    {
        pxSize = m_pCamera->GetCameraPixelSize();
        m_pPixelSize->Enable(!pFrame->CaptureActive);
    }
    else
        m_pPixelSize->Enable(false);                // Got a device-level pixel size, disable the control

    m_pPixelSize->SetValue(pxSize);

    if (m_pCamera->HasCooler)
    {
        bool ok = false;
        bool on;
        double setpt;

        if (m_pCamera->Connected)
        {
            double power, temp;
            bool err = m_pCamera->GetCoolerStatus(&on, &setpt, &power, &temp);
            if (!err)
                ok = true;
        }

        if (ok)
        {
            m_coolerOn->SetValue(on);
            if (!on)
            {
                setpt = pConfig->Profile.GetDouble("/camera/CoolerSetpt", 10.0);
            }
            m_coolerSetpt->SetValue((int)floor(setpt));
        }

        m_coolerOn->Enable(ok);
        m_coolerSetpt->Enable(ok);
    }
    else
    {
        m_coolerOn->Enable(false);
        m_coolerSetpt->Enable(false);
    }
}

void CameraConfigDialogCtrlSet::UnloadValues()
{
    assert(m_pCamera);

    if (m_pCamera->HasSubframes)
    {
        m_pCamera->UseSubframes = m_pUseSubframes->GetValue();
        pConfig->Profile.SetBoolean("/camera/UseSubframes", m_pCamera->UseSubframes);
    }

    if (m_pCamera->HasGainControl)
    {
        m_pCamera->SetCameraGain(m_pCameraGain->GetValue());
    }

    if (m_binning)
    {
        int oldBin = m_pCamera->Binning;
        int newBin = m_binning->GetSelection() + 1;
        if (oldBin != newBin)
            pFrame->pAdvancedDialog->MakeImageScaleAdjustments();           // Do this now to preserve old (device value) and new (UI value) for scale adjustment
        m_pCamera->SetBinning(m_binning->GetSelection() + 1);
    }

    m_pCamera->SetTimeoutMs(m_timeoutVal->GetValue() * 1000);

    if (m_pCamera->HasDelayParam)
    {
        m_pCamera->ReadDelay = m_pDelay->GetValue();
        pConfig->Profile.SetInt("/camera/ReadDelay", m_pCamera->ReadDelay);
    }

    if (m_pCamera->HasPortNum)
    {
        switch (m_pPortNum->GetSelection())
        {
        case 0:
            m_pCamera->Port = 0x378;
            break;
        case 1:
            m_pCamera->Port = 0x3BC;
            break;
        case 2:
            m_pCamera->Port = 0x278;
            break;
        case 3:
            m_pCamera->Port = 1;
            break;
        case 4:
            m_pCamera->Port = 2;
            break;
        case 5:
            m_pCamera->Port = 3;
            break;
        case 6:
            m_pCamera->Port = 4;
            break;
        }
    }

    double oldPxSz = m_pCamera->GetCameraPixelSize();
    double newPxSz = m_pPixelSize->GetValue();
    if (oldPxSz != newPxSz)
        pFrame->pAdvancedDialog->MakeImageScaleAdjustments();       // Preserve old (device value) and new (UI value) for scale adjustment
    m_pCamera->SetCameraPixelSize(m_pPixelSize->GetValue());

    bool saturationByADU = m_SaturationByADU->GetValue();
    unsigned short saturationVal = 0;

    if (saturationByADU)
    {
        long val = 0;
        m_camSaturationADU->GetValue().ToLong(&val);
        if (val > 0)
        {
            saturationVal = wxMin(val, SaturationValFromBPP(m_pCamera));
        }
        else
        {
            // user-entered zero treated as 'set to default'
            saturationVal = SaturationValFromBPP(m_pCamera);
        }
    }

    m_pCamera->SetSaturationByADU(saturationByADU, saturationVal);

    if (m_pCamera->HasCooler)
    {
        bool on = m_coolerOn->GetValue();
        m_pCamera->SetCoolerOn(on);
        double setpt = (double) m_coolerSetpt->GetValue();
        m_pCamera->SetCoolerSetpoint(setpt);
        pConfig->Profile.SetDouble("/camera/CoolerSetpt", setpt);
    }

    pFrame->pStatsWin->UpdateCooler();
}

double CameraConfigDialogCtrlSet::GetPixelSize()
{
    return m_pPixelSize->GetValue();
}

void CameraConfigDialogCtrlSet::SetPixelSize(double val)
{
    m_pPixelSize->SetValue(val);
}

int CameraConfigDialogCtrlSet::GetBinning()
{
    return m_binning ? m_binning->GetSelection() + 1 : 1;
}

void CameraConfigDialogCtrlSet::SetBinning(int binning)
{
    if (m_binning)
        m_binning->Select(binning - 1);
}

void GuideCamera::GetBinningOpts(int maxBin, wxArrayString *opts)
{
    for (int i = 1; i <= maxBin; i++)
        opts->Add(wxString::Format("%d", i));
}

wxString GuideCamera::GetSettingsSummary()
{
    int darkDur;

    { // lock scope
        wxCriticalSectionLocker lck(DarkFrameLock);
        darkDur = CurrentDarkFrame ? CurrentDarkFrame->ImgExpDur : 0;
    } // lock scope

    // return a loggable summary of current camera settings
    wxString pixelSizeStr;
    if (m_pixelSize == UnknownPixelSize)
        pixelSizeStr = _("unspecified");
    else
        pixelSizeStr = wxString::Format(_("%0.1f um"), m_pixelSize);

    return wxString::Format("Camera = %s%s%s%s, full size = %d x %d, %s, %s, pixel size = %s\n",
                            Name,
                            HasGainControl ? wxString::Format(", gain = %d", GuideCameraGain) : "",
                            HasDelayParam ? wxString::Format(", delay = %d", ReadDelay) : "",
                            HasPortNum ? wxString::Format(", port = 0x%hx", Port) : "",
                            FullSize.GetWidth(), FullSize.GetHeight(),
                            darkDur ? wxString::Format("have dark, dark dur = %d", darkDur) : "no dark",
                            CurrentDefectMap ? "defect map in use" : "no defect map",
                            pixelSizeStr);
}

void GuideCamera::AddDark(usImage *dark)
{
    int const expdur = dark->ImgExpDur;

    { // lock scope
        wxCriticalSectionLocker lck(DarkFrameLock);

        // free the prior dark with this exposure duration
        ExposureImgMap::iterator pos = Darks.find(expdur);
        if (pos != Darks.end())
        {
            usImage *prior = pos->second;
            if (prior == CurrentDarkFrame)
                CurrentDarkFrame = dark;
            delete prior;
        }

    } // lock scope

    Darks[expdur] = dark;
}

void GuideCamera::SelectDark(int exposureDuration)
{
    // select the dark frame with the smallest exposure >= the requested exposure.
    // if there are no darks with exposures > the select exposure, select the dark with the greatest exposure

    wxCriticalSectionLocker lck(DarkFrameLock);

    CurrentDarkFrame = 0;
    for (ExposureImgMap::const_iterator it = Darks.begin(); it != Darks.end(); ++it)
    {
        CurrentDarkFrame = it->second;
        if (it->first >= exposureDuration)
            break;
    }
}

void GuideCamera::GetDarklibProperties(int *pNumDarks, double *pMinExp, double *pMaxExp)
{
    double minExp = 9999.0;
    double maxExp = -9999.0;
    int ct = 0;

    { // lock scope
        wxCriticalSectionLocker lck(DarkFrameLock);

        for (auto it = Darks.begin(); it != Darks.end(); ++it)
        {
            if (it->first < minExp)
                minExp = it->first;
            if (it->first > maxExp)
                maxExp = it->first;
            ++ct;
        }
    } // lock scope

    *pNumDarks = ct;
    *pMinExp = minExp;
    *pMaxExp = maxExp;
}

void GuideCamera::ClearDefectMap()
{
    wxCriticalSectionLocker lck(DarkFrameLock);

    if (CurrentDefectMap)
    {
        Debug.AddLine("Clearing defect map...");
        delete CurrentDefectMap;
        CurrentDefectMap = nullptr;
    }
}

void GuideCamera::SetDefectMap(DefectMap *defectMap)
{
    wxCriticalSectionLocker lck(DarkFrameLock);
    delete CurrentDefectMap;
    CurrentDefectMap = defectMap;
}

void GuideCamera::ClearDarks()
{
    wxCriticalSectionLocker lck(DarkFrameLock);
    while (!Darks.empty())
    {
        ExposureImgMap::iterator it = Darks.begin();
        delete it->second;
        Darks.erase(it);
    }
    CurrentDarkFrame = nullptr;
}

void GuideCamera::SubtractDark(usImage& img)
{
    // dark subtraction is done in the camera worker thread, so we need to acquire the
    // DarkFrameLock to protect against the dark frame disappearing when the main
    // thread does "Load Darks" or "Clear Darks"

    wxCriticalSectionLocker lck(DarkFrameLock);

    if (CurrentDefectMap)
    {
        RemoveDefects(img, *CurrentDefectMap);
    }
    else if (CurrentDarkFrame)
    {
        Subtract(img, *CurrentDarkFrame);
    }
}

static void InitiateReconnect()
{
    WorkerThread *thr = WorkerThread::This();
    if (thr)
    {
        // Defer sending the completion of exposure message until after
        // the camera re-connecttion attempt
        thr->SetSkipExposeComplete();
    }
    pFrame->TryReconnect();
}

void GuideCamera::DisconnectWithAlert(CaptureFailType type)
{
    switch (type)
    {
    case CAPT_FAIL_MEMORY:
        DisconnectWithAlert(_("Memory allocation error during capture"), NO_RECONNECT);
        break;

    case CAPT_FAIL_TIMEOUT:
        {
            wxString msg;
            // Dark library exposure times won't match the selected exposure time in the pull-down menu of the main window
            if (!ShutterClosed)
                msg = (wxString::Format(_("After %.1f sec the camera has not completed a %.1f sec exposure, so "
                "it has been disconnected to prevent other problems. Refer to Trouble-shooting section of Help."),
                (pFrame->RequestedExposureDuration() + m_timeoutMs) / 1000.,
                pFrame->RequestedExposureDuration() / 1000.));
            else
                msg = _("The camera has not completed an exposure in at least 15 seconds, so "
                "it has been disconnected to prevent other problems. Refer to Trouble-shooting section of Help.");

            DisconnectWithAlert(msg, RECONNECT);
        }
        break;
    }
}

void GuideCamera::DisconnectWithAlert(const wxString& msg, ReconnectType reconnect)
{
    Disconnect();

    // CAUTION: this function can be called from the worker thread, so
    // care must be taken not to make any direct UI updates

    pFrame->UpdateStatusBarStateLabels();
    pFrame->NotifyUpdateButtonsStatus(); // in case camera dialog button depends on connected state

    if (reconnect == RECONNECT)
    {
        pFrame->Alert(msg + "\n" + _("PHD will make several attempts to re-connect the camera."));
        InitiateReconnect();
    }
    else
    {
        pFrame->Alert(msg + "\n" + _("The camera has been disconnected. Please resolve the problem and re-connect the camera."));
    }
}

void GuideCamera::InitCapture()
{
}

bool GuideCamera::Capture(GuideCamera *camera, int duration, usImage& img, int captureOptions, const wxRect& subframe)
{
    img.InitImgStartTime();
    img.BitsPerPixel = camera->BitsPerPixel();
    img.ImgExpDur = duration;
    bool err = camera->Capture(duration, img, captureOptions, subframe);
    return err;
}

bool GuideCamera::ST4HasGuideOutput()
{
    return m_hasGuideOutput;
}

bool GuideCamera::ST4HostConnected()
{
    return Connected;
}

bool GuideCamera::ST4HasNonGuiMove()
{
    // should never be called

    assert(false);
    return true;
}

bool GuideCamera::ST4PulseGuideScope(int direction, int duration)
{
    // should never be called

    assert(false);
    return true;
}
