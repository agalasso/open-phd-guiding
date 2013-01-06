/*
 *  scope.cpp
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
#include "phd.h"

#include "image_math.h"
#include "wx/textfile.h"
#include "socket_server.h"

static const int DefaultCalibrationDuration = 750;
static const int DefaultMaxDecDuration  = 1000;
static const int DefaultMaxRaDuration  = 1000;
static const DEC_GUIDE_MODE DefaultDecGuideMode = DEC_AUTO;

Scope::Scope(void)
{
    m_calibrationSteps = 0;

    int calibrationDuration = pConfig->GetInt("/scope/CalibrationDuration", DefaultCalibrationDuration);
    SetCalibrationDuration(calibrationDuration);

    int maxRaDuration  = pConfig->GetInt("/scope/MaxRaDuration", DefaultMaxRaDuration);
    SetMaxRaDuration(maxRaDuration);

    int maxDecDuration = pConfig->GetInt("/scope/MaxDecDuration", DefaultMaxDecDuration);
    SetMaxDecDuration(maxDecDuration);

    int decGuideMode = pConfig->GetInt("/scope/DecGuideMode", DefaultDecGuideMode);
    SetDecGuideMode(decGuideMode);

}

Scope::~Scope(void)
{
}

int Scope::GetCalibrationDuration(void)
{
    return m_calibrationDuration;
}

bool Scope::BacklashClearingFailed(void)
{
    bool bError = false;

    wxMessageBox(_T("Unable to clear DEC backlash -- turning off Dec guiding"), _T("Alert"), wxOK | wxICON_ERROR);
    SetDecGuideMode(DEC_NONE);

    return bError;
}

bool Scope::SetCalibrationDuration(int calibrationDuration)
{
    bool bError = false;

    try
    {
        if (calibrationDuration <= 0.0)
        {
            throw ERROR_INFO("invalid calibrationDuration");
        }

        m_calibrationDuration = calibrationDuration;

    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_calibrationDuration = DefaultCalibrationDuration;
    }

    pConfig->SetInt("/scope/CalibrationDuration", m_calibrationDuration);

    return bError;
}

int Scope::GetMaxDecDuration(void)
{
    return m_maxDecDuration;
}

bool Scope::SetMaxDecDuration(int maxDecDuration)
{
    bool bError = false;

    try
    {
        if (maxDecDuration < 0)
        {
            throw ERROR_INFO("maxDecDuration < 0");
        }

        m_maxDecDuration = maxDecDuration;
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_maxDecDuration = DefaultMaxDecDuration;
    }

    pConfig->SetInt("/scope/MaxDecDuration", m_maxDecDuration);

    return bError;
}

int Scope::GetMaxRaDuration(void)
{
    return m_maxRaDuration;
}

bool Scope::SetMaxRaDuration(double maxRaDuration)
{
    bool bError = false;

    try
    {
        if (maxRaDuration < 0)
        {
            throw ERROR_INFO("maxRaDuration < 0");
        }

        m_maxRaDuration =  maxRaDuration;
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_maxRaDuration = DefaultMaxRaDuration;
    }

    pConfig->SetInt("/scope/MaxRaDuration", m_maxRaDuration);

    return bError;
}

DEC_GUIDE_MODE Scope::GetDecGuideMode(void)
{
    return m_decGuideMode;
}

bool Scope::SetDecGuideMode(int decGuideMode)
{
    bool bError = false;

    try
    {
        switch (decGuideMode)
        {
            case DEC_NONE:
            case DEC_AUTO:
            case DEC_NORTH:
            case DEC_SOUTH:
                break;
            default:
                throw ERROR_INFO("invalid decGuideMode");
                break;
        }

        m_decGuideMode = (DEC_GUIDE_MODE)decGuideMode;
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_decGuideMode = (DEC_GUIDE_MODE)DefaultDecGuideMode;
    }

    pConfig->SetInt("/scope/DecGuideMode", m_decGuideMode);

    return bError;
}

void MyFrame::OnConnectScope(wxCommandEvent& WXUNUSED(event)) {
//	wxStandardPathsBase& stdpath = wxStandardPaths::Get();
//	wxMessageBox(stdpath.GetDocumentsDir() + PATHSEPSTR + _T("PHD_log.txt"));
    Scope *pNewScope = NULL;

	if (pGuider->GetState() > STATE_SELECTED) return;
	if (CaptureActive) return;  // Looping an exposure already
	if (pMount->IsConnected()) pMount->Disconnect();

    if (false)
    {
        // this dummy if is here because otherwise we can't have the 
        // else if construct below, since we don't know which camera
        // will be first.
        //
        // With this here and always false, the rest can safely begin with
        // else if 
    }
#ifdef GUIDE_ASCOM
    else if (mount_menu->IsChecked(MOUNT_ASCOM)) {
        pNewScope = new ScopeASCOM();

        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: ASCOM connection");
        }
        else
        {
			SetStatusText(_T("ASCOM connected"));
        }
	}
	#endif

	#ifdef GUIDE_GPUSB
    else if (mount_menu->IsChecked(MOUNT_GPUSB)) {
        pNewScope = new ScopeGpUsb();

		if (pNewScope->Connect()) {
			SetStatusText(_T("FAIL: GPUSB"));
		}
		else {
			SetStatusText(_T("GPUSB connected"));
		}
	}
	#endif

	#ifdef GUIDE_GPINT
	else if (mount_menu->IsChecked(MOUNT_GPINT3BC)) {
        pNewScope = new ScopeGpInt((short) 0x3BC);

        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: GPINT 3BC connection");
        }
        else
        {
            SetStatusText(_T("GPINT 3BC selected"));
        }
	}
	else if (mount_menu->IsChecked(MOUNT_GPINT378)) {
        pNewScope = new ScopeGpInt((short) 0x378);

        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: GPINT 378 connection");
        }
        else
        {
            SetStatusText(_T("GPINT 378 selected"));
        }
	}
	else if (mount_menu->IsChecked(MOUNT_GPINT278)) {
        pNewScope = new ScopeGpInt((short) 0x278);

        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: GPINT 278 connection");
        }
        else
        {
            SetStatusText(_T("GPINT 278 selected"));
        }
	}
#endif

#ifdef GUIDE_GCUSBST4
	else if (mount_menu->IsChecked(MOUNT_GCUSBST4)) {
        ScopeGCUSBST4 *pGCUSBST4 = new ScopeGCUSBST4();
        pNewScope = pGCUSBST4;
        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: GCUSB-ST4 connection");
        }
        else
        {
            SetStatusText(_T("GCUSB-ST4 selected"));
        }
	}
#endif

#ifdef GUIDE_ONBOARD
	else if (mount_menu->IsChecked(MOUNT_CAMERA)) {
        pNewScope = new ScopeOnCamera();
        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: OnCamera connection");
        }
        else
        {
            SetStatusText(_T("OnCamera selected"));
        }
	}
	#endif
	#ifdef GUIDE_VOYAGER
	else if (mount_menu->IsChecked(MOUNT_VOYAGER)) {
        ScopeVoyager *pVoyager = new ScopeVoyager();
        pNewScope = pVoyager;

        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: Voyager localhost");

            wxString IPstr = wxGetTextFromUser(_T("Enter IP address"),_T("Voyager not found on localhost"));

            // we have to use the ScopeVoyager pointer to pass the address to connect
            if (pVoyager->Connect(IPstr))
            {
                SetStatusText("Voyager IP failed");
            }
        }

        if (pNewScope->IsConnected())
        {
            SetStatusText(_T("Voyager selected"));
        }
	}
	#endif
#ifdef GUIDE_EQUINOX
	else if (mount_menu->IsChecked(MOUNT_EQUINOX)) {
        pNewScope = new ScopeEquinox();

        if (pNewScope->Connect())
        {
            SetStatusText("FAIL: Equinox mount");
        }
        else
        {
            SetStatusText(_T("Equinox connected"));
        }
	}
#endif
	#ifdef GUIDE_EQMAC
	else if (mount_menu->IsChecked(MOUNT_EQMAC)) {
        ScopeEQMac *pEQMac = new ScopeEQMac();
        pNewScope = pEQMac;

        // must use pEquinox to pass an arument to connect
        if (pEQMac->Connect())
        {
            SetStatusText("FAIL: EQMac mount");
        }
        else
        {
            SetStatusText(_T("EQMac connected"));
        }
	}
	#endif
	#ifdef GUIDE_INDI
    else if (mount_menu->IsChecked(MOUNT_INDI)) {
        if (!INDI_ScopeConnect()) {
            pMount->IsConnected() = MOUNT_INDI;
        } else {
            pMount->IsConnected() = 0;
            SetStatusText(_T("FAIL: INDI mount"));
        }
    }
#endif
	if (pNewScope && pNewScope->IsConnected()) {
        delete pMount;
        pMount = pNewScope;
		SetStatusText(_T("Mount connected"));
		SetStatusText(_T("Scope"),4);
        // now store the scope we selected so we can use it as the default next time.
        wxMenuItemList items = mount_menu->GetMenuItems();
        wxMenuItemList::iterator iter;

        for(iter = items.begin(); iter != items.end(); iter++)
        {
            wxMenuItem *pItem = *iter;

            if (pItem->IsChecked())
            {
                wxString value = pItem->GetItemLabelText();
                pConfig->SetString("/scope/LastMenuChoice", pItem->GetItemLabelText());
            }
        }
	}
	else
    {
		SetStatusText(_T("No scope"),4);
	}

    UpdateButtonsStatus();
}

bool Scope::Move(GUIDE_DIRECTION direction)
{
    return Guide(direction, m_calibrationDuration);
}

bool Scope::Move(const Point& currentLocation, const Point& desiredLocation)
{
    bool bError = false;

    try
    {
        if (!currentLocation.IsValid())
        {
            throw THROW_INFO("invalid CurrentPosition");
        }

        if (!desiredLocation.IsValid())
        {
            throw ERROR_INFO("invalid LockPosition");
        }

        double theta = desiredLocation.Angle(currentLocation);
        double hyp   = desiredLocation.Distance(currentLocation);

        // Convert theta and hyp into RA and DEC

        double raDistance  = cos(pMount->RaAngle() - theta) * hyp;
        double decDistance = cos(pMount->DecAngle() - theta) * hyp;

        pFrame->GraphLog->AppendData(desiredLocation.dX(currentLocation), desiredLocation.dY(currentLocation),
                raDistance, decDistance);

        // Feed the raw distances to the guide algorithms

        raDistance = m_pRaGuideAlgorithm->result(raDistance);
        decDistance = m_pDecGuideAlgorithm->result(decDistance);

        // Figure out the guide directions based on the (possibly) updated distances
        GUIDE_DIRECTION raDirection = raDistance > 0 ? EAST : WEST;
        GUIDE_DIRECTION decDirection = decDistance > 0 ? SOUTH : NORTH;

        // Compute the required guide durations
        double raDuration = fabs(raDistance/RaRate());
        double decDuration = fabs(decDistance/DecRate());

        if (m_guidingEnabled)
        {
            // enforce max RA duration
            if (raDuration > m_maxRaDuration)
            {
                raDuration = m_maxRaDuration;
            }

            // and the dec guiding mode
            if ((m_decGuideMode == DEC_NONE) ||
                (decDirection == SOUTH && m_decGuideMode == DEC_NORTH) ||
                (decDirection == NORTH && m_decGuideMode == DEC_SOUTH))
            {
                decDuration = 0.0;
            }

            // and the max dec duration
            if  (decDuration > m_maxDecDuration)
            {
                decDuration = m_maxDecDuration;
            }

        }
        else
        {
            raDuration = 0.0;
            decDuration = 0.0;
        }

        pFrame->SetStatusText("",1);

        // We are now ready to actually guide
        assert(raDuration >= 0);
        if (raDuration > 0.0)
        {
            pFrame->SetStatusText(wxString::Format("%c dur=%.1f dist=%.2f", (raDirection==EAST)?'E':'W', raDuration, raDistance),1);
            if (Guide(raDirection, raDuration))
            {
                throw ERROR_INFO("ra guide failed");
            }
        }

        assert(decDuration >= 0);
        if (decDuration > 0.0)
        {
            pFrame->SetStatusText(wxString::Format("%c dur=%.1f dist=%.2f", (decDirection==SOUTH)?'S':'N', decDuration, decDistance),1);
            if (Guide(decDirection, decDuration))
            {
                throw ERROR_INFO("dec guide failed");
            }
        }
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}
    
double Scope::CalibrationTime(int nCalibrationSteps)
{
    return nCalibrationSteps * m_calibrationDuration;
}

ConfigDialogPane *Scope::GetConfigDialogPane(wxWindow *pParent)
{
    return new ScopeConfigDialogPane(pParent, this);
}

Scope::ScopeConfigDialogPane::ScopeConfigDialogPane(wxWindow *pParent, Scope *pScope)
    : MountConfigDialogPane(pParent, pScope)
{
    int width;

    m_pScope = pScope;

    width = StringWidth(_T("00000"));
	m_pCalibrationDuration = new wxSpinCtrl(pParent, wxID_ANY,_T("foo2"), wxPoint(-1,-1),
            wxSize(width+30, -1), wxSP_ARROW_KEYS, 0, 10000, 1000,_T("Cal_Dur"));

	DoAdd(_T("Calibration step (ms)"), m_pCalibrationDuration,
        _T("How long a guide pulse should be used during calibration? Default = 750ms, increase for short f/l scopes and decrease for longer f/l scopes"));

    width = StringWidth(_T("00000"));
    m_pMaxRaDuration = new wxSpinCtrl(pParent,wxID_ANY,_T("foo"),wxPoint(-1,-1),
            wxSize(width+30, -1), wxSP_ARROW_KEYS, 0, 2000, 150, _T("MaxDec_Dur"));
    DoAdd(_T("Max RA Duration"),  m_pMaxRaDuration,
	      _T("Longest length of pulse to send in RA\nDefault = 1000 ms. "));

    width = StringWidth(_T("00000"));
    m_pMaxDecDuration = new wxSpinCtrl(pParent,wxID_ANY,_T("foo"),wxPoint(-1,-1),
            wxSize(width+30, -1), wxSP_ARROW_KEYS,0,2000,150,_T("MaxDec_Dur"));
    DoAdd(_T("Max Dec Duration"),  m_pMaxDecDuration,
	      _T("Longest length of pulse to send in declination\nDefault = 100 ms.  Increase if drift is fast."));

	wxString dec_choices[] = {
		_T("Off"),_T("Auto"),_T("North"),_T("South")
	};
    width = StringArrayWidth(dec_choices, WXSIZEOF(dec_choices));
	m_pDecMode= new wxChoice(pParent, wxID_ANY, wxPoint(-1,-1), 
            wxSize(width+35, -1), WXSIZEOF(dec_choices), dec_choices);
    DoAdd(_T("Dec guide mode"), m_pDecMode, 
	      _T("Guide in declination as well?"));
}

Scope::ScopeConfigDialogPane::~ScopeConfigDialogPane(void)
{
}

void Scope::ScopeConfigDialogPane::LoadValues(void)
{
    MountConfigDialogPane::LoadValues();
    m_pCalibrationDuration->SetValue(m_pScope->GetCalibrationDuration());
    m_pMaxRaDuration->SetValue(m_pScope->GetMaxRaDuration());
    m_pMaxDecDuration->SetValue(m_pScope->GetMaxDecDuration());
    m_pDecMode->SetSelection(m_pScope->GetDecGuideMode());

}

void Scope::ScopeConfigDialogPane::UnloadValues(void)
{
    m_pScope->SetCalibrationDuration(m_pCalibrationDuration->GetValue());
    m_pScope->SetMaxRaDuration(m_pMaxRaDuration->GetValue());
    m_pScope->SetMaxDecDuration(m_pMaxDecDuration->GetValue());
    m_pScope->SetDecGuideMode(m_pDecMode->GetSelection());

    MountConfigDialogPane::UnloadValues();
}
