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

#include "backlash_comp.h"
#include "calreview_dialog.h"
#include "calstep_dialog.h"
#include "image_math.h"
#include "socket_server.h"

#include <wx/textfile.h>

static const int DefaultCalibrationDuration = 750;
static const int DefaultMaxDecDuration = 2500;
static const int DefaultMaxRaDuration = 2500;
enum { MAX_DURATION_MIN = 50, MAX_DURATION_MAX = 8000, };

static const DEC_GUIDE_MODE DefaultDecGuideMode = DEC_AUTO;
static const GUIDE_ALGORITHM DefaultRaGuideAlgorithm = GUIDE_ALGORITHM_HYSTERESIS;
static const GUIDE_ALGORITHM DefaultDecGuideAlgorithm = GUIDE_ALGORITHM_RESIST_SWITCH;
static const int MAX_CALIBRATION_STEPS = 60;
static const int CAL_ALERT_MINSTEPS = 4;
static const double CAL_ALERT_ORTHOGONALITY_TOLERANCE = 12.5;               // Degrees
static const double CAL_ALERT_DECRATE_DIFFERENCE = 0.20;                    // Ratio tolerance
static const double CAL_ALERT_AXISRATES_TOLERANCE = 0.20;                   // Ratio tolerance
static const bool SANITY_CHECKING_ACTIVE = true;                            // Control calibration sanity checking

static int LIMIT_REACHED_WARN_COUNT = 5;
static int MAX_NUDGES = 3;
static double NUDGE_TOLERANCE = 2.0;

// enable dec compensation when calibration declination is less than this
const double Scope::DEC_COMP_LIMIT = M_PI / 2.0 * 2.0 / 3.0;   // 60 degrees
const double Scope::DEFAULT_MOUNT_GUIDE_SPEED = 0.5;

Scope::Scope()
    :
    m_maxDecDuration(0),
    m_maxRaDuration(0),
    m_decGuideMode(DEC_NONE),
    m_raLimitReachedDirection(NONE),
    m_raLimitReachedCount(0),
    m_decLimitReachedDirection(NONE),
    m_decLimitReachedCount(0),
    m_bogusGuideRatesFlagged(0)
{
    m_calibrationSteps = 0;
    m_limitReachedDeferralTime = wxDateTime::GetTimeNow();
    m_graphControlPane = nullptr;
    m_CalDetailsValidated = false;

    wxString prefix = "/" + GetMountClassName();
    int calibrationDuration = pConfig->Profile.GetInt(prefix + "/CalibrationDuration", DefaultCalibrationDuration);
    SetCalibrationDuration(calibrationDuration);

    int calibrationDistance = pConfig->Profile.GetInt(prefix + "/CalibrationDistance", CalstepDialog::DEFAULT_DISTANCE);
    SetCalibrationDistance(calibrationDistance);

    int maxRaDuration  = pConfig->Profile.GetInt(prefix + "/MaxRaDuration", DefaultMaxRaDuration);
    SetMaxRaDuration(maxRaDuration);

    int maxDecDuration = pConfig->Profile.GetInt(prefix + "/MaxDecDuration", DefaultMaxDecDuration);
    SetMaxDecDuration(maxDecDuration);

    int decGuideMode = pConfig->Profile.GetInt(prefix + "/DecGuideMode", DefaultDecGuideMode);
    SetDecGuideMode(decGuideMode);

    int raGuideAlgorithm = pConfig->Profile.GetInt(prefix + "/XGuideAlgorithm", DefaultRaGuideAlgorithm);
    SetXGuideAlgorithm(raGuideAlgorithm);

    int decGuideAlgorithm = pConfig->Profile.GetInt(prefix + "/YGuideAlgorithm", DefaultDecGuideAlgorithm);
    SetYGuideAlgorithm(decGuideAlgorithm);

    bool val = pConfig->Profile.GetBoolean(prefix + "/CalFlipRequiresDecFlip", false);
    SetCalibrationFlipRequiresDecFlip(val);

    val = pConfig->Profile.GetBoolean(prefix + "/AssumeOrthogonal", false);
    SetAssumeOrthogonal(val);

    val = pConfig->Profile.GetBoolean(prefix + "/UseDecComp", true);
    EnableDecCompensation(val);

    m_hasHPEncoders = pConfig->Profile.GetBoolean("/scope/HiResEncoders", false);

    m_backlashComp = new BacklashComp(this);
}

Scope::~Scope()
{
    if (m_graphControlPane)
    {
        m_graphControlPane->m_pScope = nullptr;
    }
}

GUIDE_ALGORITHM Scope::DefaultXGuideAlgorithm() const
{
    return DefaultRaGuideAlgorithm;
}

GUIDE_ALGORITHM Scope::DefaultYGuideAlgorithm() const
{
    return DefaultDecGuideAlgorithm;
}

bool Scope::SetCalibrationDuration(int calibrationDuration)
{
    bool bError = false;

    try
    {
        if (calibrationDuration <= 0)
        {
            throw ERROR_INFO("invalid calibrationDuration");
        }

        m_calibrationDuration = calibrationDuration;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_calibrationDuration = DefaultCalibrationDuration;
    }

    pConfig->Profile.SetInt("/scope/CalibrationDuration", m_calibrationDuration);

    return bError;
}

bool Scope::SetCalibrationDistance(int calibrationDistance)
{
    bool bError = false;

    try
    {
        if (calibrationDistance <= 0)
        {
            throw ERROR_INFO("invalid calibrationDistance");
        }

        m_calibrationDistance = calibrationDistance;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_calibrationDistance = CalstepDialog::DEFAULT_DISTANCE;
    }

    pConfig->Profile.SetInt("/scope/CalibrationDistance", m_calibrationDistance);
    return bError;
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

        if (m_maxDecDuration != maxDecDuration)
            pFrame->NotifyGuidingParam("Dec Max Duration", maxDecDuration);

        m_maxDecDuration = maxDecDuration;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_maxDecDuration = DefaultMaxDecDuration;
    }

    pConfig->Profile.SetInt("/scope/MaxDecDuration", m_maxDecDuration);

    return bError;
}

bool Scope::SetMaxRaDuration(int maxRaDuration)
{
    bool bError = false;

    try
    {
        if (maxRaDuration < 0)
        {
            throw ERROR_INFO("maxRaDuration < 0");
        }

        if (m_maxRaDuration != maxRaDuration)
        {
            pFrame->NotifyGuidingParam("RA Max Duration", maxRaDuration);

            m_maxRaDuration = maxRaDuration;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_maxRaDuration = DefaultMaxRaDuration;
    }

    pConfig->Profile.SetInt("/scope/MaxRaDuration", m_maxRaDuration);

    return bError;
}

wxString Scope::DecGuideModeStr(DEC_GUIDE_MODE m)
{
    switch (m)
    {
    case DEC_NONE:  return "Off";
    case DEC_AUTO:  return "Auto";
    case DEC_NORTH: return "North";
    case DEC_SOUTH: return "South";
    default:        return "Invalid";
    }
}

wxString Scope::DecGuideModeLocaleStr(DEC_GUIDE_MODE m)
{
    switch (m)
    {
    case DEC_NONE:  return _("Off");
    case DEC_AUTO:  return _("Auto");
    case DEC_NORTH: return _("North");
    case DEC_SOUTH: return _("South");
    default:        return _("Invalid");
    }
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

        if (m_decGuideMode != decGuideMode)
        {
            m_decGuideMode = (DEC_GUIDE_MODE) decGuideMode;
            if (pFrame && pFrame->pGraphLog)
                pFrame->pGraphLog->EnableDecControls(decGuideMode != DEC_NONE);

            wxString s = DecGuideModeStr(m_decGuideMode);
            Debug.Write(wxString::Format("DecGuideMode set to %s (%d)\n", s, decGuideMode));
            pFrame->NotifyGuidingParam("Dec Guide Mode", s);
            BacklashComp *blc = GetBacklashComp();
            if (blc)
            {
                if (decGuideMode != DEC_AUTO)
                    blc->EnableBacklashComp(false);             // Can't do blc in uni-direction mode because there's no recovery from over-shoots
                else
                    blc->ResetBLCState();
            }
            pConfig->Profile.SetInt("/scope/DecGuideMode", m_decGuideMode);
            if (pFrame)
                pFrame->UpdateStatusBarCalibrationStatus();
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

static int CompareNoCase(const wxString& first, const wxString& second)
{
    return first.CmpNoCase(second);
}

static wxString INDIMountName()
{
    wxString val = pConfig->Profile.GetString("/indi/INDImount", wxEmptyString);
    return val.empty() ? _("INDI Mount") : wxString::Format(_("INDI Mount [%s]"), val);
}

wxArrayString Scope::MountList()
{
    wxArrayString ScopeList;

    ScopeList.Add(_("None"));
#ifdef GUIDE_ASCOM
    wxArrayString ascomScopes = ScopeASCOM::EnumAscomScopes();
    for (unsigned int i = 0; i < ascomScopes.Count(); i++)
        ScopeList.Add(ascomScopes[i]);
#endif
#ifdef GUIDE_ONCAMERA
    ScopeList.Add(_T("On-camera"));
#endif
#ifdef GUIDE_ONSTEPGUIDER
    ScopeList.Add(_T("On-AO"));
#endif
#ifdef GUIDE_GPUSB
    ScopeList.Add(_T("GPUSB"));
#endif
#ifdef GUIDE_GPINT
    ScopeList.Add(_T("GPINT 3BC"));
    ScopeList.Add(_T("GPINT 378"));
    ScopeList.Add(_T("GPINT 278"));
#endif
#ifdef GUIDE_VOYAGER
    ScopeList.Add(_T("Voyager"));
#endif
#ifdef GUIDE_EQUINOX
    ScopeList.Add(_T("Equinox 6"));
    ScopeList.Add(_T("EQMAC"));
#endif
#ifdef GUIDE_GCUSBST4
    ScopeList.Add(_T("GC USB ST4"));
#endif
#ifdef GUIDE_INDI
    ScopeList.Add(INDIMountName());
#endif

    ScopeList.Sort(&CompareNoCase);

    return ScopeList;
}

wxArrayString Scope::AuxMountList()
{
    wxArrayString scopeList;
    scopeList.Add(_("None"));      // Keep this at the top of the list

#ifdef GUIDE_ASCOM
    wxArrayString positionAwareScopes = ScopeASCOM::EnumAscomScopes();
    positionAwareScopes.Sort(&CompareNoCase);
    for (unsigned int i = 0; i < positionAwareScopes.Count(); i++)
        scopeList.Add(positionAwareScopes[i]);
#endif

#ifdef GUIDE_INDI
    scopeList.Add(INDIMountName());
#endif

    scopeList.Add(ScopeManualPointing::GetDisplayName());

    return scopeList;
}

Scope *Scope::Factory(const wxString& choice)
{
    Scope *pReturn = nullptr;

    try
    {
        if (choice.IsEmpty())
        {
            throw ERROR_INFO("ScopeFactory called with choice.IsEmpty()");
        }

        Debug.Write(wxString::Format("ScopeFactory(%s)\n", choice));

        if (false) // so else ifs can follow
        {
        }
        // do ASCOM and INDI first since they includes choices that could match stings below like Simulator
#ifdef GUIDE_ASCOM
        else if (choice.Contains(_T("ASCOM")))
            pReturn = new ScopeASCOM(choice);
#endif
#ifdef GUIDE_INDI
        else if (choice.Contains(_("INDI")))
            pReturn = INDIScopeFactory::MakeINDIScope();
#endif
        else if (choice == _("None"))
            pReturn = nullptr;
#ifdef GUIDE_ONCAMERA
        else if (choice == _T("On-camera"))
            pReturn = new ScopeOnCamera();
#endif
#ifdef GUIDE_ONSTEPGUIDER
        else if (choice == _T("On-AO"))
            pReturn = new ScopeOnStepGuider();
#endif
#ifdef GUIDE_GPUSB
        else if (choice.Contains(_T("GPUSB")))
            pReturn = new ScopeGpUsb();
#endif
#ifdef GUIDE_GPINT
        else if (choice.Contains(_T("GPINT 3BC")))
            pReturn = new ScopeGpInt((short) 0x3BC);
        else if (choice.Contains(_T("GPINT 378")))
            pReturn = new ScopeGpInt((short) 0x378);
        else if (choice.Contains(_T("GPINT 278")))
            pReturn = new ScopeGpInt((short) 0x278);
#endif
#ifdef GUIDE_VOYAGER
        else if (choice.Contains(_T("Voyager")))
        {
            This needs work.  We have to move the setting of the IP address
                into the connect routine
            ScopeVoyager *pVoyager = new ScopeVoyager();
        }
#endif
#ifdef GUIDE_EQUINOX
        else if (choice.Contains(_T("Equinox 6")))
            pReturn = new ScopeEquinox();
#endif
#ifdef GUIDE_EQMAC
        else if (choice.Contains(_T("EQMAC")))
            pReturn = new ScopeEQMac();
#endif
#ifdef GUIDE_GCUSBST4
        else if (choice.Contains(_T("GC USB ST4")))
            pReturn = new ScopeGCUSBST4();
#endif
        else if (choice.Contains(ScopeManualPointing::GetDisplayName()))
            pReturn = new ScopeManualPointing();
        else
        {
            throw ERROR_INFO("ScopeFactory: Unknown Scope choice");
        }

        if (pReturn)
        {
            // virtual function call means we cannot do this in the Scope constructor
            pReturn->EnableStopGuidingWhenSlewing(pConfig->Profile.GetBoolean("/scope/StopGuidingWhenSlewing",
                pReturn->CanCheckSlewing()));
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

bool Scope::RequiresCamera()
{
    return false;
}

bool Scope::RequiresStepGuider()
{
    return false;
}

bool Scope::CalibrationFlipRequiresDecFlip()
{
    return m_calibrationFlipRequiresDecFlip;
}

// No 'Set' function, property is set via profile in new-profile-wizard
bool Scope::HasHPEncoders() const
{
    return m_hasHPEncoders;
}

void Scope::SetCalibrationFlipRequiresDecFlip(bool val)
{
    m_calibrationFlipRequiresDecFlip = val;
    pConfig->Profile.SetBoolean("/scope/CalFlipRequiresDecFlip", val);
}

void Scope::SetAssumeOrthogonal(bool val)
{
    m_assumeOrthogonal = val;
    pConfig->Profile.SetBoolean("/scope/AssumeOrthogonal", val);
}

void Scope::EnableStopGuidingWhenSlewing(bool enable)
{
    if (enable)
        Debug.Write("Scope: enabling slew check, guiding will stop when slew is detected\n");
    else
        Debug.Write("Scope: slew check disabled\n");

    pConfig->Profile.SetBoolean("/scope/StopGuidingWhenSlewing", enable);
    m_stopGuidingWhenSlewing = enable;
}

void Scope::StartDecDrift()
{
    m_saveDecGuideMode = m_decGuideMode;
    m_decGuideMode = DEC_NONE;

    Debug.Write(wxString::Format("StartDecDrift: DecGuideMode set to %s (%d)\n",
        DecGuideModeStr(m_decGuideMode), m_decGuideMode));

    if (m_graphControlPane)
    {
        m_graphControlPane->m_pDecMode->SetSelection(DEC_NONE);
        m_graphControlPane->m_pDecMode->Enable(false);
    }
}

void Scope::EndDecDrift()
{
    m_decGuideMode = m_saveDecGuideMode;

    Debug.Write(wxString::Format("EndDecDrift: DecGuideMode set to %s (%d)\n",
        DecGuideModeStr(m_decGuideMode), m_decGuideMode));

    if (m_graphControlPane)
    {
        m_graphControlPane->m_pDecMode->SetSelection(m_decGuideMode);
        m_graphControlPane->m_pDecMode->Enable(true);
    }
}

bool Scope::IsDecDrifting() const
{
    return m_decGuideMode == DEC_NONE;
}

Mount::MOVE_RESULT Scope::MoveAxis(GUIDE_DIRECTION direction, int duration, unsigned int moveOptions)
{
    MOVE_RESULT result = MOVE_OK;

    Debug.Write(wxString::Format("scope move axis dir= %d dur= %d opts= 0x%x\n", direction, duration, moveOptions));

    try
    {
        MoveResultInfo move;
        result = MoveAxis(direction, duration, moveOptions, &move);

        if (result != MOVE_OK)
        {
            throw THROW_INFO("Move failed");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    return result;
}

int Scope::CalibrationMoveSize()
{
    return m_calibrationDuration;
}

static wxString LimitReachedWarningKey(long axis)
{
    // we want the key to be under "/Confirm" so ConfirmDialog::ResetAllDontAskAgain() resets it, but we also want the setting to be per-profile
    return wxString::Format("/Confirm/%d/Max%sLimitWarningEnabled", pConfig->GetCurrentProfileId(), axis == GUIDE_RA ? "RA" : "Dec");
}

static void SuppressLimitReachedWarning(long axis)
{
    pConfig->Global.SetBoolean(LimitReachedWarningKey(axis), false);
}

void Scope::DeferPulseLimitAlertCheck()
{
    enum { LIMIT_REACHED_GRACE_PERIOD_SECONDS = 120 };

    m_limitReachedDeferralTime =
        wxDateTime::GetTimeNow() + LIMIT_REACHED_GRACE_PERIOD_SECONDS;
}

void Scope::AlertLimitReached(int duration, GuideAxis axis)
{
    static time_t s_lastLogged;

    time_t now = wxDateTime::GetTimeNow();
    if (s_lastLogged != 0 && now < s_lastLogged + 30)
        return;

    s_lastLogged = now;

    if (now < m_limitReachedDeferralTime)
        return;

    if (duration < MAX_DURATION_MAX)
    {
        int defaultVal = axis == GUIDE_RA ? DefaultMaxRaDuration : DefaultMaxDecDuration;
        if (duration >= defaultVal)          // Max duration is probably ok, some other kind of problem
        {
            wxString msg;
            if (axis == GUIDE_RA)
            {
                if (CanPulseGuide())
                {
                    msg = _("PHD2 is not able to make sufficient corrections in RA.  Check for cable snags, try re-doing your calibration, and "
                            "check for problems with the mount mechanics.");
                }
                else
                {
                    msg = _("PHD2 is not able to make sufficient corrections in RA.  Check for cable snags, try re-doing your calibration, and "
                            "confirm the ST-4 cable is working properly.");
                }
            }
            else                          // Dec axis problems
            {
                if (CanPulseGuide())
                {
                    msg = _("PHD2 is not able to make sufficient corrections in Dec.  If you have just done a meridian flip, "
                            "check to see if the 'Reverse Dec output option' on the Advanced Dialog guiding tab is wrong.  Otherwise, "
                            "check for cable snags, try re-doing your calibration, and check for problems with the mount mechanics.");
                }
                else
                {
                    msg = _("PHD2 is not able to make sufficient corrections in Dec.  Check for cable snags, try re-doing your calibration and "
                            "confirm the ST-4 cable is working properly.");
                }
            }
            pFrame->SuppressableAlert(LimitReachedWarningKey(axis), msg, SuppressLimitReachedWarning, axis, false, wxICON_INFORMATION);
        }
        else                              // Max duration has been decreased by user, start by recommending use of default value
        {
            wxString s = axis == GUIDE_RA ? _("Max RA Duration setting") : _("Max Dec Duration setting");
            pFrame->SuppressableAlert(LimitReachedWarningKey(axis),
                wxString::Format(_("Your %s is preventing PHD from making adequate corrections to keep the guide star locked. "
                                   "Try restoring %s to its default value to allow PHD2 to make larger corrections."), s, s),
                    SuppressLimitReachedWarning, axis, false, wxICON_INFORMATION);
        }
    }
    else                       // Already at maximum allowed value
    {
        wxString which_axis = axis == GUIDE_RA ? _("RA") : _("Dec");
        pFrame->SuppressableAlert(LimitReachedWarningKey(axis),
            wxString::Format(_("Even using the maximum moves, PHD2 can't properly correct for the large guide star movements in %s. "
                "Guiding will be impaired until you can eliminate the source of these problems."), which_axis),
            SuppressLimitReachedWarning, axis, false, wxICON_INFORMATION);
    }
}

Mount::MOVE_RESULT Scope::MoveAxis(GUIDE_DIRECTION direction, int duration, unsigned int moveOptions, MoveResultInfo *moveResult)
{
    MOVE_RESULT result = MOVE_OK;
    bool limitReached = false;

    try
    {
        Debug.Write(wxString::Format("MoveAxis(%s, %d, %s)\n", DirectionChar(direction), duration, DumpMoveOptionBits(moveOptions)));

        if (!m_guidingEnabled && (moveOptions & MOVEOPT_MANUAL) == 0)
        {
            throw THROW_INFO("Guiding disabled");
        }

        // Compute the actual guide durations

        switch (direction)
        {
            case NORTH:
            case SOUTH:

                // Enforce dec guide mode and max duration for guide step (or deduced step) moves
                if (moveOptions & (MOVEOPT_ALGO_RESULT | MOVEOPT_ALGO_DEDUCE))
                {
                    if ((m_decGuideMode == DEC_NONE) ||
                        (direction == SOUTH && m_decGuideMode == DEC_NORTH) ||
                        (direction == NORTH && m_decGuideMode == DEC_SOUTH))
                    {
                        duration = 0;
                        Debug.Write("duration set to 0 by GuideMode\n");
                    }

                    if (duration > m_maxDecDuration)
                    {
                        duration = m_maxDecDuration;
                        Debug.Write(wxString::Format("duration set to %d by maxDecDuration\n", duration));
                        limitReached = true;
                    }

                    if (limitReached && direction == m_decLimitReachedDirection)
                    {
                        if (++m_decLimitReachedCount >= LIMIT_REACHED_WARN_COUNT)
                            AlertLimitReached(duration, GUIDE_DEC);
                    }
                    else
                        m_decLimitReachedCount = 0;

                    if (limitReached)
                        m_decLimitReachedDirection = direction;
                    else
                        m_decLimitReachedDirection = NONE;
                }
                break;
            case EAST:
            case WEST:

                // Enforce max duration for guide step (or deduced step) moves
                if (moveOptions & (MOVEOPT_ALGO_RESULT | MOVEOPT_ALGO_DEDUCE))
                {
                    if (duration > m_maxRaDuration)
                    {
                        duration = m_maxRaDuration;
                        Debug.Write(wxString::Format("duration set to %d by maxRaDuration\n", duration));
                        limitReached = true;
                    }

                    if (limitReached && direction == m_raLimitReachedDirection)
                    {
                        if (++m_raLimitReachedCount >= LIMIT_REACHED_WARN_COUNT)
                            AlertLimitReached(duration, GUIDE_RA);
                    }
                    else
                        m_raLimitReachedCount = 0;

                    if (limitReached)
                        m_raLimitReachedDirection = direction;
                    else
                        m_raLimitReachedDirection = NONE;
                }
                break;

            case NONE:
                break;
        }

        // Actually do the guide
        if (duration > 0)
        {
            result = Guide(direction, duration);
            if (result != MOVE_OK)
            {
                throw ERROR_INFO("guide failed");
            }
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        if (result == MOVE_OK)
            result = MOVE_ERROR;
        duration = 0;
    }

    Debug.Write(wxString::Format("Move returns status %d, amount %d\n", result, duration));

    if (moveResult)
    {
        moveResult->amountMoved = duration;
        moveResult->limited = limitReached;
    }

    return result;
}

static wxString CalibrationWarningKey(CalibrationIssueType etype)
{
    wxString qual;
    switch (etype)
    {
    case CI_Angle:
        qual = "Angle";
        break;
    case CI_Different:
        qual = "Diff";
        break;
    case CI_Steps:
        qual = "Steps";
        break;
    case CI_Rates:
        qual = "Rates";
        break;
    case CI_None:
        qual = "Bogus";
        break;

    }
    wxString rtn = wxString::Format("/Confirm/%d/CalWarning_%s", pConfig->GetCurrentProfileId(), qual);
    return rtn;
}

void Scope::SetCalibrationWarning(CalibrationIssueType etype, bool val)
{
    pConfig->Global.SetBoolean(CalibrationWarningKey(etype), val);
}

// Generic hook for "details" button in calibration sanity check alert
static void ShowCalibrationIssues(long scopeptr)
{
    Scope *pscope = reinterpret_cast<Scope *>(scopeptr);
    pscope->HandleSanityCheckDialog();
}

// Handle the "details" dialog for the calibration sanity check
void Scope::HandleSanityCheckDialog()
{
    if (pFrame->pCalSanityCheckDlg)
        pFrame->pCalSanityCheckDlg->Destroy();

    pFrame->pCalSanityCheckDlg = new CalSanityDialog(pFrame, m_prevCalibration, m_prevCalibrationDetails, m_lastCalibrationIssue);
    pFrame->pCalSanityCheckDlg->Show();
}

// Do some basic sanity checking on the just-completed calibration, looking for things that are fishy.  Do the checking in the order of
// importance/confidence, since we only alert about a single condition
void Scope::SanityCheckCalibration(const Calibration& oldCal, const CalibrationDetails& oldDetails)
{
    Calibration newCal;
    GetLastCalibration(&newCal);

    CalibrationDetails newDetails;
    LoadCalibrationDetails(&newDetails);

    m_lastCalibrationIssue = CI_None;
    int xSteps = newDetails.raStepCount;
    int ySteps = newDetails.decStepCount;

    wxString detailInfo;

    // Too few steps
    if (xSteps < CAL_ALERT_MINSTEPS || (ySteps < CAL_ALERT_MINSTEPS && ySteps > 0))            // Dec guiding might be disabled
    {
        m_lastCalibrationIssue = CI_Steps;
        detailInfo = wxString::Format("Actual RA calibration steps = %d, Dec calibration steps = %d", xSteps, ySteps);
    }
    else
    {
        // Non-orthogonal RA/Dec axes
        double nonOrtho = degrees(fabs(fabs(norm_angle(newCal.xAngle - newCal.yAngle)) - M_PI / 2.));         // Delta from the nearest multiple of 90 degrees
        if (nonOrtho >  CAL_ALERT_ORTHOGONALITY_TOLERANCE)
        {
            m_lastCalibrationIssue = CI_Angle;
            detailInfo = wxString::Format("Non-orthogonality = %0.3f", nonOrtho);
        }
        else
        {
            // RA/Dec rates should be related by cos(dec) but don't check if Dec is too high or Dec guiding is disabled.  Also don't check if DecComp
            // is disabled because a Sitech controller might be monkeying around with the apparent rates
            if (newCal.declination != UNKNOWN_DECLINATION && newCal.yRate != CALIBRATION_RATE_UNCALIBRATED &&
                fabs(newCal.declination) <= Scope::DEC_COMP_LIMIT && DecCompensationEnabled())
            {
                double expectedRatio = cos(newCal.declination);
                double speedRatio;
                if (newDetails.raGuideSpeed > 0.)                   // for mounts that may have different guide speeds on RA and Dec axes
                    speedRatio = newDetails.decGuideSpeed / newDetails.raGuideSpeed;
                else
                    speedRatio = 1.0;
                double actualRatio = newCal.xRate * speedRatio / newCal.yRate;
                if (fabs(expectedRatio - actualRatio) > CAL_ALERT_AXISRATES_TOLERANCE)
                {
                    m_lastCalibrationIssue = CI_Rates;
                    detailInfo = wxString::Format("Expected ratio at dec=%0.1f is %0.3f, actual is %0.3f", degrees(newCal.declination), expectedRatio, actualRatio);
                }
            }
        }

        // Finally check for a significantly different result but don't be stupid - ignore differences if the configuration looks quite different
        // Can't do straight equality checks because of rounding - the "old" values have passed through the registry get/set routines
        if (m_lastCalibrationIssue == CI_None && oldCal.isValid &&
            fabs(oldDetails.imageScale - newDetails.imageScale) < 0.1 && (fabs(degrees(oldCal.xAngle - newCal.xAngle)) < 5.0))
        {
            if (newCal.yRate != CALIBRATION_RATE_UNCALIBRATED && oldCal.yRate != CALIBRATION_RATE_UNCALIBRATED)
            {
                double newDecRate = newCal.yRate;
                if (newDecRate != 0.)
                {
                    if (fabs(1.0 - (oldCal.yRate / newDecRate)) > CAL_ALERT_DECRATE_DIFFERENCE)
                    {
                        m_lastCalibrationIssue = CI_Different;
                        detailInfo = wxString::Format("Current/previous Dec rate ratio is %0.3f", oldCal.yRate / newDecRate);
                    }
                }
            }
        }
    }

    if (m_lastCalibrationIssue != CI_None)
    {
        wxString alertMsg;

        FlagCalibrationIssue(newDetails, m_lastCalibrationIssue);
        switch (m_lastCalibrationIssue)
        {
        case CI_Steps:
            alertMsg = _("Advisory: Calibration completed but few guide steps were used, so accuracy is questionable");
            break;
        case CI_Angle:
            alertMsg = _("Advisory: Calibration completed but RA/Dec axis angles are questionable and guiding may be impaired");
            break;
        case CI_Different:
            alertMsg = _("Advisory: This calibration is substantially different from the previous one - have you changed configurations?");
            break;
        case CI_Rates:
            alertMsg = _("Advisory: Calibration completed but RA and Dec rates vary by an unexpected amount (often caused by large Dec backlash)");
        default:
            break;
        }

        // Suppression of calibration alerts is handled in the 'Details' dialog - a special case
        if (pConfig->Global.GetBoolean(CalibrationWarningKey(m_lastCalibrationIssue), true))        // User hasn't disabled this type of alert
        {
            // Generate alert with 'Help' button that will lead to trouble-shooting section
            pFrame->Alert(alertMsg, 0,
                _("Details..."), ShowCalibrationIssues, (long)this, true);
        }
        else
        {
            Debug.Write(wxString::Format("Alert detected in scope calibration but not shown to user - suppressed message was: %s\n", alertMsg));
        }

        Debug.Write(wxString::Format("Calibration alert details: %s\n", detailInfo));
    }
    else
    {
        Debug.Write("Calibration passed sanity checks...\n");
    }
}

void Scope::ClearCalibration()
{
    Mount::ClearCalibration();

    m_calibrationState = CALIBRATION_STATE_CLEARED;
}

// Handle gross errors in cal_step duration when user changes mount guide speeds or binning.
void Scope::CheckCalibrationDuration(int currDuration)
{
    CalibrationDetails calDetails;
    LoadCalibrationDetails(&calDetails);

    bool binningChange = pCamera->Binning != calDetails.origBinning;

    // if binning changed, may need to update the calibration distance
    if (binningChange)
    {
        int prevDistance = GetCalibrationDistance();
        int newDistance = CalstepDialog::GetCalibrationDistance(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(),
            pCamera->Binning);

        if (newDistance != prevDistance)
        {
            Debug.Write(wxString::Format("CalDistance adjusted at start of calibration from %d to %d because of binning change\n",
                prevDistance, newDistance));

            SetCalibrationDistance(newDistance);
        }
    }

    double raSpd;
    double decSpd;
    double const siderealSecsPerSec = 0.9973;
    bool haveRates = !pPointingSource->GetGuideRates(&raSpd, &decSpd);          // units of degrees/sec as in ASCOM

    double currSpdX = raSpd * 3600.0 / (15.0 * siderealSecsPerSec);             // multiple of sidereal

    // Don't check the step size on very first calibration and don't adjust if the reported mount guide speeds are bogus
    if (!haveRates || calDetails.raGuideSpeed <= 0)
        return;

    bool refineStepSize = binningChange || (fabs(1.0 - raSpd / calDetails.raGuideSpeed) > 0.05);     // binning change or speed change of > 5%
    if (!refineStepSize)
        return;

    int rslt;
    CalstepDialog::GetCalibrationStepSize(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(),
        pCamera->Binning, currSpdX, CalstepDialog::DEFAULT_STEPS, 0.0, GetCalibrationDistance(), 0, &rslt);

    wxString why = binningChange ? " binning " : " mount guide speed ";
    Debug.Write(wxString::Format("CalDuration adjusted at start of calibration from %d to %d because of %s change\n",
        currDuration, rslt, why));

    SetCalibrationDuration(rslt);
}

bool Scope::BeginCalibration(const PHD_Point& currentLocation)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("Not connected");
        }

        if (!currentLocation.IsValid())
        {
            throw ERROR_INFO("Must have a valid lock position");
        }

        CheckCalibrationDuration(m_calibrationDuration);          // Make sure guide speeds or binning haven't changed underneath us
        ClearCalibration();
        m_calibrationSteps = 0;
        m_calibrationInitialLocation = currentLocation;
        m_calibrationStartingLocation.Invalidate();
        m_calibrationStartingCoords.Invalidate();
        m_calibrationState = CALIBRATION_STATE_GO_WEST;
        m_calibrationDetails.raSteps.clear();
        m_calibrationDetails.decSteps.clear();
        m_raSteps = 0;
        m_decSteps = 0;
        m_calibrationDetails.lastIssue = CI_None;
        m_eastAlertShown = false;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void Scope::SetCalibration(const Calibration& cal)
{
    m_calibration = cal;
    Mount::SetCalibration(cal);
}

void Scope::SetCalibrationDetails(const CalibrationDetails& calDetails, double xAngle, double yAngle, double binning)
{
    m_calibrationDetails = calDetails;

    double ra_rate;
    double dec_rate;
    if (pPointingSource->GetGuideRates(&ra_rate, &dec_rate))        // true means error
    {
        ra_rate = -1.0;
        dec_rate = -1.0;
    }

    m_calibrationDetails.raGuideSpeed = ra_rate;
    m_calibrationDetails.decGuideSpeed = dec_rate;
    m_calibrationDetails.focalLength = pFrame->GetFocalLength();
    m_calibrationDetails.imageScale = pFrame->GetCameraPixelScale();
    m_calibrationDetails.orthoError = degrees(fabs(fabs(norm_angle(xAngle - yAngle)) - M_PI / 2.));         // Delta from the nearest multiple of 90 degrees
    m_calibrationDetails.origBinning = binning;
    m_calibrationDetails.origTimestamp = wxDateTime::Now().Format();
    m_calibrationDetails.origPierSide = pPointingSource->SideOfPier();

    SaveCalibrationDetails(m_calibrationDetails);
}

void Scope::FlagCalibrationIssue(const CalibrationDetails& calDetails, CalibrationIssueType issue)
{
    m_calibrationDetails = calDetails;
    m_calibrationDetails.lastIssue = issue;

    SaveCalibrationDetails(m_calibrationDetails);
}

bool Scope::IsCalibrated() const
{
    if (!Mount::IsCalibrated())
        return false;

    switch (m_decGuideMode)
    {
    case DEC_NONE:
        return true;
    case DEC_AUTO:
    case DEC_NORTH:
    case DEC_SOUTH:
        {
            bool have_ns_calibration = m_calibration.yRate != CALIBRATION_RATE_UNCALIBRATED;
            return have_ns_calibration;
        }
    default:
        assert(false);
        return true;
    }
}

void Scope::EnableDecCompensation(bool enable)
{
    m_useDecCompensation = enable;
    wxString prefix = "/" + GetMountClassName();
    pConfig->Profile.SetBoolean(prefix + "/UseDecComp", enable);
}

int Scope::CalibrationTotDistance()
{
    return GetCalibrationDistance();
}

// Convert camera coords to mount coords
static PHD_Point MountCoords(const PHD_Point& cameraVector, double xCalibAngle, double yCalibAngle)
{
    double hyp = cameraVector.Distance();
    double cameraTheta = cameraVector.Angle();
    double yAngleError = norm_angle((xCalibAngle - yCalibAngle) + M_PI / 2);
    double xAngle = cameraTheta - xCalibAngle;
    double yAngle = cameraTheta - (xCalibAngle + yAngleError);
    return PHD_Point(hyp * cos(xAngle), hyp * sin(yAngle));
}

static void GetRADecCoordinates(PHD_Point *coords)
{
    double ra, dec, lst;
    bool err = pPointingSource->GetCoordinates(&ra, &dec, &lst);
    if (err)
        coords->Invalidate();
    else
        coords->SetXY(ra, dec);
}

static wxString DecBacklashAlertKey()
{
    // we want the key to be under "/Confirm" so ConfirmDialog::ResetAllDontAskAgain() resets it, but we also want the setting to be per-profile
    return wxString::Format("/Confirm/%d/DecBacklashWarningEnabled", pConfig->GetCurrentProfileId());
}

static void SuppressDecBacklashAlert(long)
{
    pConfig->Global.SetBoolean(DecBacklashAlertKey(), false);
}

static void CalibrationStatus(CalibrationStepInfo& info, const wxString& msg)
{
    info.msg = msg;
    pFrame->StatusMsg(info.msg);
    EvtServer.NotifyCalibrationStep(info);
}

bool Scope::UpdateCalibrationState(const PHD_Point& currentLocation)
{
    bool bError = false;

    try
    {
        if (!m_calibrationStartingLocation.IsValid())
        {
            m_calibrationStartingLocation = currentLocation;
            GetRADecCoordinates(&m_calibrationStartingCoords);

            Debug.Write(wxString::Format("Scope::UpdateCalibrationstate: starting location = %.2f,%.2f coords = %s\n",
                currentLocation.X, currentLocation.Y,
                m_calibrationStartingCoords.IsValid() ?
                    wxString::Format("%.2f,%.1f", m_calibrationStartingCoords.X, m_calibrationStartingCoords.Y) : wxString("N/A")));
        }

        double dX = m_calibrationStartingLocation.dX(currentLocation);
        double dY = m_calibrationStartingLocation.dY(currentLocation);
        double dist = m_calibrationStartingLocation.Distance(currentLocation);
        double dist_crit = GetCalibrationDistance();
        double blDelta;
        double blCumDelta;
        double nudge_amt;
        double nudgeDirCosX;
        double nudgeDirCosY;
        double cos_theta;
        double theta;
        double southDistMoved;
        double northDistMoved;
        double southAngle;

        switch (m_calibrationState)
        {
            case CALIBRATION_STATE_CLEARED:
                assert(false);
                break;

            case CALIBRATION_STATE_GO_WEST: {

                // step number in the log is the step that just finished
                CalibrationStepInfo info(this, _T("West"), m_calibrationSteps, dX, dY, currentLocation, dist);
                GuideLog.CalibrationStep(info);
                m_calibrationDetails.raSteps.push_back(wxRealPoint(dX, dY));

                if (dist < dist_crit)
                {
                    if (m_calibrationSteps++ > MAX_CALIBRATION_STEPS)
                    {
                        wxString msg(wxTRANSLATE("RA Calibration Failed: star did not move enough"));
                        const wxString& translated(wxGetTranslation(msg));
                        pFrame->Alert(translated);
                        GuideLog.CalibrationFailed(this, msg);
                        EvtServer.NotifyCalibrationFailed(this, msg);
                        throw ERROR_INFO("RA calibration failed");
                    }
                    CalibrationStatus(info, wxString::Format(_("West step %3d, dist=%4.1f"), m_calibrationSteps, dist));
                    pFrame->ScheduleAxisMove(this, WEST, m_calibrationDuration, MOVEOPTS_CALIBRATION_MOVE);
                    break;
                }

                // West calibration complete

                m_calibration.xAngle = m_calibrationStartingLocation.Angle(currentLocation);
                m_calibration.xRate = dist / (m_calibrationSteps * m_calibrationDuration);

                m_calibration.raGuideParity = GUIDE_PARITY_UNKNOWN;
                if (m_calibrationStartingCoords.IsValid())
                {
                    PHD_Point endingCoords;
                    GetRADecCoordinates(&endingCoords);
                    if (endingCoords.IsValid())
                    {
                        // true westward motion decreases RA
                        double ONE_ARCSEC = 24.0 / (360. * 60. * 60.); // hours
                        double dra = endingCoords.X - m_calibrationStartingCoords.X;
                        if (dra < -ONE_ARCSEC)
                            m_calibration.raGuideParity = GUIDE_PARITY_EVEN;
                        else if (dra > ONE_ARCSEC)
                            m_calibration.raGuideParity = GUIDE_PARITY_ODD;
                    }
                }

                Debug.Write(wxString::Format("WEST calibration completes with steps=%d angle=%.1f rate=%.3f parity=%d\n",
                    m_calibrationSteps, degrees(m_calibration.xAngle), m_calibration.xRate * 1000.0, m_calibration.raGuideParity));

                m_raSteps = m_calibrationSteps;
                GuideLog.CalibrationDirectComplete(this, "West", m_calibration.xAngle, m_calibration.xRate, m_calibration.raGuideParity);

                // for GO_EAST m_recenterRemaining contains the total remaining duration.
                // Choose the largest pulse size that will not lose the guide star or exceed
                // the user-specified max pulse

                m_recenterRemaining = m_calibrationSteps * m_calibrationDuration;

                if (pFrame->pGuider->IsFastRecenterEnabled())
                {
                    m_recenterDuration = (int)floor((double)pFrame->pGuider->GetMaxMovePixels() / m_calibration.xRate);
                    if (m_recenterDuration > m_maxRaDuration)
                        m_recenterDuration = m_maxRaDuration;
                    if (m_recenterDuration < m_calibrationDuration)
                        m_recenterDuration = m_calibrationDuration;
                }
                else
                    m_recenterDuration = m_calibrationDuration;

                m_calibrationSteps = DIV_ROUND_UP(m_recenterRemaining, m_recenterDuration);
                m_calibrationState = CALIBRATION_STATE_GO_EAST;
                m_eastStartingLocation = currentLocation;

                // fall through
                Debug.Write("Falling Through to state GO_EAST\n");
            }

            case CALIBRATION_STATE_GO_EAST: {

                CalibrationStepInfo info(this, _T("East"), m_calibrationSteps, dX, dY, currentLocation, dist);
                GuideLog.CalibrationStep(info);
                m_calibrationDetails.raSteps.push_back(wxRealPoint(dX, dY));

                if (m_recenterRemaining > 0)
                {
                    int duration = m_recenterDuration;
                    if (duration > m_recenterRemaining)
                        duration = m_recenterRemaining;

                    CalibrationStatus(info, wxString::Format(_("East step %3d, dist=%4.1f"), m_calibrationSteps, dist));

                    m_recenterRemaining -= duration;
                    --m_calibrationSteps;
                    m_lastLocation = currentLocation;

                    pFrame->ScheduleAxisMove(this, EAST, duration, MOVEOPTS_CALIBRATION_MOVE);
                    break;
                }

                // If not pulse-guiding check for obvious guide cable problem and no useful east moves
                if (!CanPulseGuide())
                {
                    double eastDistMoved = m_eastStartingLocation.Distance(currentLocation);
                    double westDistMoved = m_calibrationStartingLocation.Distance(m_eastStartingLocation);
                    double eastAngle = m_eastStartingLocation.Angle(currentLocation);
                    // Want a significant east movement that re-traces the west vector to within 30 degrees
                    if (fabs(eastDistMoved) < 0.25 * westDistMoved || fabs(norm_angle(eastAngle - (m_calibration.xAngle + M_PI))) > radians(30))
                    {
                        wxString msg(wxTRANSLATE("Advisory: Little or no east movement was measured, so guiding will probably be impaired. "
                            "Check the guide cable and use the Manual Guide tool to confirm basic operation of the mount."));
                        const wxString& translated(wxGetTranslation(msg));
                        pFrame->Alert(translated, 0, wxEmptyString, 0, 0, true);
                        Debug.Write("Calibration alert: " + msg + "\n");
                        m_eastAlertShown = true;
                    }
                }
                // setup for clear backlash

                m_calibrationSteps = 0;
                dist = dX = dY = 0.0;
                m_calibrationStartingLocation = currentLocation;

                if (m_decGuideMode == DEC_NONE)
                {
                    Debug.Write("Skipping Dec calibration as DecGuideMode == NONE\n");
                    m_calibrationState = CALIBRATION_STATE_COMPLETE;
                    m_calibration.yAngle = norm_angle(m_calibration.xAngle + M_PI / 2.); // choose an arbitrary angle perpendicular to xAngle
                    // indicate lack of Dec calibration data, see Scope::IsCalibrated.
                    m_calibration.yRate = CALIBRATION_RATE_UNCALIBRATED;
                    m_calibration.decGuideParity = GUIDE_PARITY_UNKNOWN;
                    break;
                }

                m_calibrationState = CALIBRATION_STATE_CLEAR_BACKLASH;
                m_blMarkerPoint = currentLocation;
                GetRADecCoordinates(&m_calibrationStartingCoords);
                m_blExpectedBacklashStep = m_calibration.xRate * m_calibrationDuration * 0.6;

                double RASpeed;
                double DecSpeed;
                if (!pPointingSource->GetGuideRates(&RASpeed, &DecSpeed) &&
                    RASpeed != 0.0 && RASpeed != DecSpeed)
                {
                    m_blExpectedBacklashStep *= DecSpeed / RASpeed;
                }

                m_blMaxClearingPulses = wxMax(8, BL_MAX_CLEARING_TIME / m_calibrationDuration);
                m_blLastCumDistance = 0;
                m_blAcceptedMoves = 0;
                Debug.Write(wxString::Format("Backlash: Looking for 3 moves of %0.1f px, max attempts = %d\n", m_blExpectedBacklashStep, m_blMaxClearingPulses));
                // fall through
                Debug.Write("Falling Through to state CLEAR_BACKLASH\n");
            }

            case CALIBRATION_STATE_CLEAR_BACKLASH: {

                CalibrationStepInfo info(this, _T("Backlash"), m_calibrationSteps, dX, dY, currentLocation, dist);
                GuideLog.CalibrationStep(info);
                blDelta = m_blMarkerPoint.Distance(currentLocation);
                blCumDelta = dist;

                // Want to see the mount moving north for 3 moves of >= expected distance pixels without any direction reversals
                if (m_calibrationSteps == 0)
                {
                    // Get things moving with the first clearing pulse
                    Debug.Write(wxString::Format("Backlash: Starting north clearing using pulse width of %d\n",
                        m_calibrationDuration));
                    pFrame->ScheduleAxisMove(this, NORTH, m_calibrationDuration, MOVEOPTS_CALIBRATION_MOVE);
                    m_calibrationSteps = 1;
                    CalibrationStatus(info, _("Clearing backlash step 1"));
                    break;
                }

                if (blDelta >= m_blExpectedBacklashStep)
                {
                    if (m_blAcceptedMoves == 0 || (blCumDelta > m_blLastCumDistance))    // Just starting or still moving in same direction
                    {
                        m_blAcceptedMoves++;
                        Debug.Write(wxString::Format("Backlash: Accepted clearing move of %0.1f\n", blDelta));
                    }
                    else
                    {
                        m_blAcceptedMoves = 0;            // Reset on a direction reversal
                        Debug.Write(wxString::Format("Backlash: Rejected clearing move of %0.1f, direction reversal\n", blDelta));
                    }
                }
                else
                {
                    if (blCumDelta < m_blLastCumDistance)
                    {
                        m_blAcceptedMoves = 0;
                        Debug.Write(wxString::Format("Backlash: Rejected small direction reversal of %0.1f px\n", blDelta));
                    }
                    else
                        Debug.Write(wxString::Format("Backlash: Rejected small move of %0.1f px\n", blDelta));
                }

                if (m_blAcceptedMoves < BL_BACKLASH_MIN_COUNT)                    // More work to do
                {
                    if (m_calibrationSteps < m_blMaxClearingPulses && blCumDelta < dist_crit)
                    {
                        // Still have attempts left, haven't moved the star by 25 px yet
                        pFrame->ScheduleAxisMove(this, NORTH, m_calibrationDuration, MOVEOPTS_CALIBRATION_MOVE);
                        m_calibrationSteps++;
                        m_blMarkerPoint = currentLocation;
                        GetRADecCoordinates(&m_calibrationStartingCoords);
                        m_blLastCumDistance = blCumDelta;
                        CalibrationStatus(info, wxString::Format(_("Clearing backlash step %3d"), m_calibrationSteps));
                        Debug.Write(wxString::Format("Backlash: %s, Last Delta = %0.2f px, CumDistance = %0.2f px\n", info.msg, blDelta, blCumDelta));
                        break;
                    }
                    else
                    {
                        // Used up all our attempts - might be ok or not
                        if (blCumDelta >= BL_MIN_CLEARING_DISTANCE)
                        {
                            // Exhausted all the clearing pulses without reaching the goal - but we did move the mount > 3 px (same as PHD1)
                            m_calibrationSteps = 0;
                            m_calibrationStartingLocation = currentLocation;
                            dX = 0;
                            dY = 0;
                            dist = 0;
                            Debug.Write("Backlash: Reached clearing limit but total displacement > 3px - proceeding with calibration\n");
                        }
                        else
                        {
                            wxString msg(wxTRANSLATE("Backlash Clearing Failed: star did not move enough"));
                            const wxString& translated(wxGetTranslation(msg));
                            pFrame->Alert(translated);
                            GuideLog.CalibrationFailed(this, msg);
                            EvtServer.NotifyCalibrationFailed(this, msg);
                            throw ERROR_INFO("Clear backlash failed");
                        }
                    }
                }
                else        //Got our 3 moves, move ahead
                {
                    // We know the last backlash clearing move was big enough - include that as a north calibration move

                    // log the starting point
                    CalibrationStepInfo info(this, _T("North"), 0, 0.0, 0.0, m_blMarkerPoint, 0.0);
                    GuideLog.CalibrationStep(info);
                    m_calibrationDetails.decSteps.push_back(wxRealPoint(0.0, 0.0));

                    m_calibrationSteps = 1;
                    m_calibrationStartingLocation = m_blMarkerPoint;
                    dX = m_blMarkerPoint.dX(currentLocation);
                    dY = m_blMarkerPoint.dY(currentLocation);
                    dist = m_blMarkerPoint.Distance(currentLocation);
                    Debug.Write("Backlash: Got 3 acceptable moves, using last move as step 1 of N calibration\n");
                }

                m_blDistanceMoved = m_blMarkerPoint.Distance(m_calibrationInitialLocation);     // Need this to set nudging limit

                Debug.Write(wxString::Format("Backlash: North calibration moves starting at {%0.1f,%0.1f}, Offset = %0.1f px\n",
                    m_blMarkerPoint.X, m_blMarkerPoint.Y, m_blDistanceMoved));
                Debug.Write(wxString::Format("Backlash: Total distance moved = %0.1f\n",
                    currentLocation.Distance(m_calibrationInitialLocation)));

                m_calibrationState = CALIBRATION_STATE_GO_NORTH;
                // falling through to start moving north
                Debug.Write("Backlash: Falling Through to state GO_NORTH\n");
            }

            case CALIBRATION_STATE_GO_NORTH: {

                CalibrationStepInfo info(this, _T("North"), m_calibrationSteps, dX, dY, currentLocation, dist);
                GuideLog.CalibrationStep(info);
                m_calibrationDetails.decSteps.push_back(wxRealPoint(dX, dY));

                if (dist < dist_crit)
                {
                    if (m_calibrationSteps++ > MAX_CALIBRATION_STEPS)
                    {
                        wxString msg(wxTRANSLATE("DEC Calibration Failed: star did not move enough"));
                        const wxString& translated(wxGetTranslation(msg));
                        pFrame->Alert(translated);
                        GuideLog.CalibrationFailed(this, msg);
                        EvtServer.NotifyCalibrationFailed(this, msg);
                        throw ERROR_INFO("Dec calibration failed");
                    }
                    CalibrationStatus(info, wxString::Format(_("North step %3d, dist=%4.1f"), m_calibrationSteps, dist));
                    pFrame->ScheduleAxisMove(this, NORTH, m_calibrationDuration, MOVEOPTS_CALIBRATION_MOVE);
                    break;
                }

                // note: this calculation is reversed from the ra calculation, because
                // that one was calibrating WEST, but the angle is really relative
                // to EAST
                if (m_assumeOrthogonal)
                {
                    double a1 = norm_angle(m_calibration.xAngle + M_PI / 2.);
                    double a2 = norm_angle(m_calibration.xAngle - M_PI / 2.);
                    double yAngle = currentLocation.Angle(m_calibrationStartingLocation);
                    m_calibration.yAngle = fabs(norm_angle(a1 - yAngle)) < fabs(norm_angle(a2 - yAngle)) ? a1 : a2;
                    double dec_dist = dist * cos(yAngle - m_calibration.yAngle);
                    m_calibration.yRate = dec_dist / (m_calibrationSteps * m_calibrationDuration);

                    Debug.Write(wxString::Format("Assuming orthogonal axes: measured Y angle = %.1f, X angle = %.1f, orthogonal = %.1f, %.1f, best = %.1f, dist = %.2f, dec_dist = %.2f\n",
                        degrees(yAngle), degrees(m_calibration.xAngle), degrees(a1), degrees(a2), degrees(m_calibration.yAngle), dist, dec_dist));
                }
                else
                {
                    m_calibration.yAngle = currentLocation.Angle(m_calibrationStartingLocation);
                    m_calibration.yRate = dist / (m_calibrationSteps * m_calibrationDuration);
                }

                m_decSteps = m_calibrationSteps;

                m_calibration.decGuideParity = GUIDE_PARITY_UNKNOWN;
                if (m_calibrationStartingCoords.IsValid())
                {
                    PHD_Point endingCoords;
                    GetRADecCoordinates(&endingCoords);
                    if (endingCoords.IsValid())
                    {
                        // real Northward motion increases Dec
                        double ONE_ARCSEC = 1.0 / (60. * 60.); // degrees
                        double ddec = endingCoords.Y - m_calibrationStartingCoords.Y;
                        if (ddec > ONE_ARCSEC)
                            m_calibration.decGuideParity = GUIDE_PARITY_EVEN;
                        else if (ddec < -ONE_ARCSEC)
                            m_calibration.decGuideParity = GUIDE_PARITY_ODD;
                    }
                }

                Debug.Write(wxString::Format("NORTH calibration completes with angle=%.1f rate=%.3f parity=%d\n",
                    degrees(m_calibration.yAngle), m_calibration.yRate * 1000.0, m_calibration.decGuideParity));

                GuideLog.CalibrationDirectComplete(this, "North", m_calibration.yAngle, m_calibration.yRate, m_calibration.decGuideParity);

                // for GO_SOUTH m_recenterRemaining contains the total remaining duration.
                // Choose the largest pulse size that will not lose the guide star or exceed
                // the user-specified max pulse
                m_recenterRemaining = m_calibrationSteps * m_calibrationDuration;

                if (pFrame->pGuider->IsFastRecenterEnabled())
                {
                    m_recenterDuration = (int)floor(0.8 * (double)pFrame->pGuider->GetMaxMovePixels() / m_calibration.yRate);
                    if (m_recenterDuration > m_maxDecDuration)
                        m_recenterDuration = m_maxDecDuration;
                    if (m_recenterDuration < m_calibrationDuration)
                        m_recenterDuration = m_calibrationDuration;
                }
                else
                    m_recenterDuration = m_calibrationDuration;

                m_calibrationSteps = DIV_ROUND_UP(m_recenterRemaining, m_recenterDuration);
                m_calibrationState = CALIBRATION_STATE_GO_SOUTH;
                m_southStartingLocation = currentLocation;

                // fall through
                Debug.Write("Falling Through to state GO_SOUTH\n");
            }

            case CALIBRATION_STATE_GO_SOUTH: {

                CalibrationStepInfo info(this, _T("South"), m_calibrationSteps, dX, dY, currentLocation, dist);
                GuideLog.CalibrationStep(info);
                m_calibrationDetails.decSteps.push_back(wxRealPoint(dX, dY));

                if (m_recenterRemaining > 0)
                {
                    int duration = m_recenterDuration;
                    if (duration > m_recenterRemaining)
                        duration = m_recenterRemaining;

                    CalibrationStatus(info, wxString::Format(_("South step %3d, dist=%4.1f"), m_calibrationSteps, dist));

                    m_recenterRemaining -= duration;
                    --m_calibrationSteps;

                    pFrame->ScheduleAxisMove(this, SOUTH, duration, MOVEOPTS_CALIBRATION_MOVE);
                    break;
                }

                // Check for obvious guide cable problem and no useful south moves
                southDistMoved = m_southStartingLocation.Distance(currentLocation);
                northDistMoved = m_calibrationStartingLocation.Distance(m_southStartingLocation);
                southAngle = currentLocation.Angle(m_southStartingLocation);
                // Want a significant south movement that re-traces the north vector to within 30 degrees
                if (fabs(southDistMoved) < 0.25 * northDistMoved || fabs(norm_angle(southAngle - (m_calibration.yAngle + M_PI))) > radians(30))
                {
                    wxString msg;
                    if (!CanPulseGuide())
                    {
                        if (fabs(southDistMoved) < 0.10 * northDistMoved)
                            msg = wxTRANSLATE("Advisory: Calibration succeessful but little or no south movement was measured, so guiding will probably be impaired.\n "
                            "This is usually caused by a faulty guide cable or very large Dec backlash. \n"
                            "Check the guide cable and read the online Help for how to identify these types of problems (Manual Guide, Declination backlash).");
                        else
                            msg = wxTRANSLATE("Advisory: Calibration successful but little south movement was measured, so guiding will probably be impaired. \n"
                            "This is usually caused by very large Dec backlash or other problems with the mount mechanics. \n"
                            "Read the online Help for how to identify these types of problems (Manual Guide, Declination backlash).");
                    }
                    else
                        msg = wxTRANSLATE("Advisory: Calibration successful but little south movement was measured, so guiding may be impaired.\n "
                        "This is usually caused by very large Dec backlash or other problems with the mount mechanics. \n"
                        "Read the online help for how to deal with this type of problem (Declination backlash).");

                    //if (!m_eastAlertShown)
                    //{
                    //    const wxString& translated(wxGetTranslation(msg));
                    //    pFrame->SuppressableAlert(DecBacklashAlertKey(), translated, SuppressDecBacklashAlert, 0, true);
                    //}
                    Debug.Write("Omitted calibration alert: " + msg + "\n");
                }

                m_lastLocation = currentLocation;
                // Compute the vector for the north moves we made - use it to make sure any nudging is going in the correct direction
                // These are the direction cosines of the vector
                m_northDirCosX = m_calibrationInitialLocation.dX(m_southStartingLocation) / m_calibrationInitialLocation.Distance(m_southStartingLocation);
                m_northDirCosY = m_calibrationInitialLocation.dY(m_southStartingLocation) / m_calibrationInitialLocation.Distance(m_southStartingLocation);
                //Debug.Write(wxString::Format("Nudge: InitStart:{%0.1f,%0.1f}, southStart:{%.1f,%0.1f}, north_l:%0.2f, north_m:%0.2f\n",
                //    m_calibrationInitialLocation.X, m_calibrationInitialLocation.Y,
                //    m_southStartingLocation.X, m_southStartingLocation.Y, m_northDirCosX, m_northDirCosY));
                // Get magnitude and sign convention for the south moves we already made
                m_totalSouthAmt = MountCoords(m_southStartingLocation - m_lastLocation, m_calibration.xAngle, m_calibration.yAngle).Y;
                m_calibrationState = CALIBRATION_STATE_NUDGE_SOUTH;
                m_calibrationSteps = 0;
                // Fall through to nudging
                Debug.Write("Falling Through to state CALIBRATION_STATE_NUDGE_SOUTH\n");
            }

            case CALIBRATION_STATE_NUDGE_SOUTH:
                // Nudge further South on Dec, get within 2 px North/South of starting point, don't try more than 3 times and don't do nudging at all if
                // we're starting too far away from the target
                nudge_amt = currentLocation.Distance(m_calibrationInitialLocation);
                // Compute the direction cosines for the expected nudge op
                nudgeDirCosX = currentLocation.dX(m_calibrationInitialLocation) / nudge_amt;
                nudgeDirCosY = currentLocation.dY(m_calibrationInitialLocation) / nudge_amt;
                // Compute the angle between the nudge and north move vector - they should be reversed, i.e. something close to 180 deg
                cos_theta = nudgeDirCosX * m_northDirCosX + nudgeDirCosY * m_northDirCosY;
                theta = acos(cos_theta);
                //Debug.Write(wxString::Format("Nudge: currLoc:{%0.1f,%0.1f}, m_nudgeDirCosX: %0.2f, nudgeDirCosY: %0.2f, cos_theta: %0.2f\n",
                //    currentLocation.X, currentLocation.Y, nudgeDirCosX, nudgeDirCosY, cos_theta));
                Debug.Write(wxString::Format("Nudge: theta = %0.2f\n", theta));
                if (fabs(fabs(theta) * 180.0/M_PI - 180.0) < 40.0)      // We're going at least roughly in the right direction
                {
                    if (m_calibrationSteps <= MAX_NUDGES && nudge_amt > NUDGE_TOLERANCE &&
                        nudge_amt < dist_crit + m_blDistanceMoved)
                    {
                        // Compute how much more south we need to go
                        double decAmt = MountCoords(currentLocation - m_calibrationInitialLocation, m_calibration.xAngle, m_calibration.yAngle).Y;
                        Debug.Write(wxString::Format("South nudging, decAmt = %.3f, Normal south moves = %.3f\n", decAmt, m_totalSouthAmt));

                        if (decAmt * m_totalSouthAmt > 0.0)           // still need to move south to reach target based on matching sign
                        {
                            decAmt = fabs(decAmt);           // Sign doesn't matter now, we're always moving south
                            decAmt = wxMin(decAmt, (double)pFrame->pGuider->GetMaxMovePixels());
                            int pulseAmt = (int)floor(decAmt / m_calibration.yRate);
                            if (pulseAmt > m_calibrationDuration)
                                pulseAmt = m_calibrationDuration;               // Be conservative, use durations that pushed us north in the first place
                            Debug.Write(wxString::Format("Sending NudgeSouth pulse of duration %d ms\n", pulseAmt));
                            ++m_calibrationSteps;
                            CalibrationStepInfo info(this, _T("NudgeSouth"), m_calibrationSteps, dX, dY, currentLocation, dist);
                            CalibrationStatus(info, wxString::Format(_("Nudge South %3d"), m_calibrationSteps));
                            pFrame->ScheduleAxisMove(this, SOUTH, pulseAmt, MOVEOPTS_CALIBRATION_MOVE);
                            break;
                        }
                    }
                }
                else
                {
                    Debug.Write(wxString::Format("Nudging discontinued, wrong direction: %0.2f\n", theta));
                }

                Debug.Write(wxString::Format("Final south nudging status: Current loc = {%.3f,%.3f}, targeting {%.3f,%.3f}\n",
                    currentLocation.X, currentLocation.Y, m_calibrationInitialLocation.X, m_calibrationInitialLocation.Y));

                m_calibrationState = CALIBRATION_STATE_COMPLETE;
                // fall through
                Debug.Write("Falling Through to state CALIBRATION_COMPLETE\n");

            case CALIBRATION_STATE_COMPLETE:
                GetLastCalibration(&m_prevCalibration);
                LoadCalibrationDetails(&m_prevCalibrationDetails);
                Calibration cal(m_calibration);
                cal.declination = pPointingSource->GetDeclinationRadians();
                cal.pierSide = pPointingSource->SideOfPier();
                cal.rotatorAngle = Rotator::RotatorPosition();
                cal.binning = pCamera->Binning;
                SetCalibration(cal);
                m_calibrationDetails.raStepCount = m_raSteps;
                m_calibrationDetails.decStepCount = m_decSteps;
                SetCalibrationDetails(m_calibrationDetails, m_calibration.xAngle, m_calibration.yAngle, pCamera->Binning);
                if (SANITY_CHECKING_ACTIVE)
                    SanityCheckCalibration(m_prevCalibration, m_prevCalibrationDetails);  // method gets "new" info itself
                pFrame->StatusMsg(_("Calibration complete"));
                GuideLog.CalibrationComplete(this);
                EvtServer.NotifyCalibrationComplete(this);
                Debug.Write("Calibration Complete\n");
                pConfig->Flush();
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);

        ClearCalibration();

        bError = true;
    }

    return bError;
}

// Get a value of declination, in radians, that can be used for adjusting the RA guide rate,
// or UNKNOWN_DECLINATION if the declination is not known.
double Scope::GetDeclinationRadians()
{
    return UNKNOWN_DECLINATION;
}

// Baseline implementations for non-ASCOM subclasses.  Methods will
// return a sensible default or an error (true)
bool Scope::GetGuideRates(double *pRAGuideRate, double *pDecGuideRate)
{
    return true; // error, not implemented
}

bool Scope::GetCoordinates(double *ra, double *dec, double *siderealTime)
{
    return true; // error
}

bool Scope::GetSiteLatLong(double *latitude, double *longitude)
{
    return true; // error
}

bool Scope::CanSlew()
{
    return false;
}

bool Scope::CanSlewAsync()
{
    return false;
}

bool Scope::PreparePositionInteractive()
{
    return false; // no error
}

bool Scope::CanReportPosition()
{
    return false;
}

bool Scope::CanPulseGuide()
{
    return false;
}

bool Scope::SlewToCoordinates(double ra, double dec)
{
    return true; // error
}

bool Scope::SlewToCoordinatesAsync(double ra, double dec)
{
    return true; // error
}

void Scope::AbortSlew()
{
}

bool Scope::CanCheckSlewing()
{
    return false;
}

bool Scope::Slewing()
{
    return false;
}

PierSide Scope::SideOfPier()
{
    return PIER_SIDE_UNKNOWN;
}

// Sanity check that guide speeds reported by mount are sensible
bool Scope::ValidGuideRates(double RAGuideRate, double DecGuideRate)
{
    double const siderealSecsPerSec = 0.9973;
    double spd;
    bool err = false;

    spd = RAGuideRate * 3600.0 / (15.0 * siderealSecsPerSec);
    if (spd > 0.1 && spd < 1.2)
    {
        spd = DecGuideRate * 3600.0 / (15.0 * siderealSecsPerSec);
        if (spd != -1.0)                        // RA-only tracking devices
            if (spd < 0.1 || spd > 1.2)
                err = true;
    }
    else
        err = true;
    m_CalDetailsValidated = true;

    if (err)
    {
        Debug.Write(wxString::Format("Invalid mount guide speeds: RA: %0.4f, Dec: %0.4f\n", RAGuideRate, DecGuideRate));
        return false;
    }
    else
        return true;
}

static wxString GuideSpeedSummary()
{
    // use the pointing source's guide speeds on the assumption that
    // the ASCOM/INDI reported guide rate will be the same as the ST4
    // guide rate
    Scope *scope = pPointingSource;

    double raSpeed, decSpeed;
    if (!scope->GetGuideRates(&raSpeed, &decSpeed))
    {
        return wxString::Format("RA Guide Speed = %0.1f a-s/s, Dec Guide Speed = %0.1f a-s/s",
            3600.0 * raSpeed, 3600.0 * decSpeed);
    }
    else
        return "RA Guide Speed = Unknown, Dec Guide Speed = Unknown";
}

// unstranslated settings summary
wxString Scope::GetSettingsSummary() const
{
    Calibration calInfo;
    GetLastCalibration(&calInfo);

    CalibrationDetails calDetails;
    LoadCalibrationDetails(&calDetails);

    // return a loggable summary of current mount settings
    wxString ret = Mount::GetSettingsSummary() +
        wxString::Format(
            "Max RA duration = %d, Max DEC duration = %d, DEC guide mode = %s\n",
            GetMaxRaDuration(),
            GetMaxDecDuration(),
            DecGuideModeStr(GetDecGuideMode()));

    ret += GuideSpeedSummary() + ", ";

    ret += wxString::Format("Cal Dec = %s, Last Cal Issue = %s, Timestamp = %s\n",
        DeclinationStr(calInfo.declination, "%0.1f"), Mount::GetIssueString(calDetails.lastIssue),
        calDetails.origTimestamp);

    return ret;
}

wxString Scope::CalibrationSettingsSummary() const
{
    return wxString::Format("Calibration Step = %d ms, Calibration Distance = %d px, "
                            "Assume orthogonal axes = %s\n",
                            GetCalibrationDuration(), GetCalibrationDistance(),
                            IsAssumeOrthogonal() ? "yes" : "no") +
            GuideSpeedSummary();
}

wxString Scope::GetMountClassName() const
{
    return wxString("scope");
}

Mount::MountConfigDialogPane *Scope::GetConfigDialogPane(wxWindow *pParent)
{
    return new ScopeConfigDialogPane(pParent, this);
}

Scope::ScopeConfigDialogPane::ScopeConfigDialogPane(wxWindow *pParent, Scope *pScope)
    : MountConfigDialogPane(pParent, _("Mount Guide Algorithms"), pScope)
{
    m_pScope = pScope;
}

void Scope::ScopeConfigDialogPane::LayoutControls(wxPanel *pParent, BrainCtrlIdMap& CtrlMap)
{
    // All of the scope UI controls are hosted in the parent
    MountConfigDialogPane::LayoutControls(pParent, CtrlMap);
}

void Scope::ScopeConfigDialogPane::LoadValues()
{
    MountConfigDialogPane::LoadValues();
}

void Scope::ScopeConfigDialogPane::UnloadValues()
{
    MountConfigDialogPane::UnloadValues();
}

MountConfigDialogCtrlSet *Scope::GetConfigDialogCtrlSet(wxWindow *pParent, Mount *pScope, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
{
    return new ScopeConfigDialogCtrlSet(pParent, (Scope *) pScope, pAdvancedDialog, CtrlMap);
}

ScopeConfigDialogCtrlSet::ScopeConfigDialogCtrlSet(wxWindow *pParent, Scope *pScope, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
    : MountConfigDialogCtrlSet(pParent, pScope, pAdvancedDialog, CtrlMap)
{
    int width;
    bool enableCtrls = pScope != nullptr;

    m_pScope = pScope;
    width = StringWidth(_T("00000"));

    wxBoxSizer *pCalibSizer = new wxBoxSizer(wxHORIZONTAL);
    m_pCalibrationDuration = pFrame->MakeSpinCtrl(GetParentWindow(AD_szCalibrationDuration), wxID_ANY, wxEmptyString, wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0, 10000, 1000, _T("Cal_Dur"));
    pCalibSizer->Add(MakeLabeledControl(AD_szCalibrationDuration, _("Calibration step (ms)"), m_pCalibrationDuration,
        _("How long a guide pulse should be used during calibration? Click \"Advanced...\" to compute a suitable value.")));
    m_pCalibrationDuration->Enable(enableCtrls);

    // create the 'auto' button and bind it to the associated event-handler
    wxButton *pAutoDuration = new wxButton(GetParentWindow(AD_szCalibrationDuration), wxID_OK, _("Advanced...") );
    pAutoDuration->SetToolTip(_("Click to open the Calibration Calculator Dialog to review or change all calibration parameters"));
    pAutoDuration->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &ScopeConfigDialogCtrlSet::OnCalcCalibrationStep, this);
    pAutoDuration->Enable(enableCtrls);

    pCalibSizer->Add(pAutoDuration);

    wxBoxSizer *pCalibGroupSizer = new wxBoxSizer(wxVERTICAL);
    pCalibGroupSizer->Add(pCalibSizer);

    AddGroup(CtrlMap, AD_szCalibrationDuration, pCalibGroupSizer);

    m_pNeedFlipDec = new wxCheckBox(GetParentWindow(AD_cbReverseDecOnFlip), wxID_ANY,
        _("Reverse Dec output after meridian flip"));
    AddCtrl(CtrlMap, AD_cbReverseDecOnFlip, m_pNeedFlipDec,
        _("Check if your mount needs Dec output reversed after a meridian flip. Changing this setting will clear the existing calibration data"));
    m_pNeedFlipDec->Enable(enableCtrls);

    bool usingAO = TheAO() != nullptr;
    if (pScope && pScope->CanCheckSlewing())
    {
        m_pStopGuidingWhenSlewing = new wxCheckBox(GetParentWindow(AD_cbSlewDetection), wxID_ANY, _("Stop guiding when mount slews"));
        AddCtrl(CtrlMap, AD_cbSlewDetection, m_pStopGuidingWhenSlewing,
            _("When checked, PHD will stop guiding if the mount starts slewing"));
    }
    else
        m_pStopGuidingWhenSlewing = 0;

    m_assumeOrthogonal = new wxCheckBox(GetParentWindow(AD_cbAssumeOrthogonal), wxID_ANY,
        _("Assume Dec orthogonal to RA"));
    m_assumeOrthogonal->Enable(enableCtrls);
    AddCtrl(CtrlMap, AD_cbAssumeOrthogonal, m_assumeOrthogonal,
        _("Assume Dec axis is perpendicular to RA axis, regardless of calibration. Prevents RA periodic error from affecting Dec calibration. Option takes effect when calibrating DEC."));

    if (pScope)
    {
        wxBoxSizer *pComp1 = new wxBoxSizer(wxHORIZONTAL);
        BRAIN_CTRL_IDS blcCtrlId;
        if (usingAO)
            blcCtrlId = AD_szBumpBLCompCtrls;
        else
            blcCtrlId = AD_szBLCompCtrls;
        wxWindow *blcHostTab = GetParentWindow(blcCtrlId);
        m_pUseBacklashComp = new wxCheckBox(blcHostTab, wxID_ANY, _("Enable"));
        m_pUseBacklashComp->SetToolTip(_("Check this if you want to apply a backlash compensation guide pulse when declination direction is reversed."));
        pComp1->Add(m_pUseBacklashComp);

        double const blcMinVal = BacklashComp::GetBacklashPulseMinValue();
        double const blcMaxVal = BacklashComp::GetBacklashPulseMaxValue();

        m_pBacklashPulse = pFrame->MakeSpinCtrlDouble(blcHostTab, wxID_ANY, wxEmptyString, wxDefaultPosition,
            wxSize(width, -1), wxSP_ARROW_KEYS, blcMinVal, blcMaxVal, 450, blcMinVal);
        pComp1->Add(MakeLabeledControl(blcCtrlId, _("Amount"), m_pBacklashPulse,
            _("Size of backlash compensation guide pulse (mSec)")), wxSizerFlags().Border(wxLEFT, 26));

        wxBoxSizer *pCompVert =
            new wxStaticBoxSizer(wxVERTICAL, blcHostTab,
            usingAO ? _("Mount Backlash Compensation") : _("Backlash Compensation"));
        pCompVert->Add(pComp1);

        if (!usingAO)              // AO doesn't use auto-adjustments, so don't show min/max controls
        {
            wxBoxSizer *pComp2 = new wxBoxSizer(wxHORIZONTAL);
            m_pBacklashFloor = pFrame->MakeSpinCtrlDouble(blcHostTab, wxID_ANY, wxEmptyString, wxDefaultPosition,
                wxSize(width, -1), wxSP_ARROW_KEYS, blcMinVal, blcMaxVal, 300, blcMinVal);
            m_pBacklashCeiling = pFrame->MakeSpinCtrlDouble(blcHostTab, wxID_ANY, wxEmptyString, wxDefaultPosition,
                wxSize(width, -1), wxSP_ARROW_KEYS, blcMinVal, blcMaxVal, 300, blcMinVal);
            pComp2->Add(MakeLabeledControl(blcCtrlId, _("Min"), m_pBacklashFloor,
                _("Minimum length of backlash compensation pulse (mSec).")), wxSizerFlags().Border(wxLEFT, 0));
            pComp2->Add(MakeLabeledControl(blcCtrlId, _("Max"), m_pBacklashCeiling,
                _("Maximum length of backlash compensation pulse (mSec).")), wxSizerFlags().Border(wxLEFT, 18));
            pCompVert->Add(pComp2);
        }
        AddGroup(CtrlMap, blcCtrlId, pCompVert);
        if (!usingAO)
        {
            m_pUseDecComp = new wxCheckBox(GetParentWindow(AD_cbUseDecComp), wxID_ANY, _("Use Dec compensation"));
            m_pUseDecComp->Enable(enableCtrls && pPointingSource);
            AddCtrl(CtrlMap, AD_cbUseDecComp, m_pUseDecComp,
                _("Automatically adjust RA guide rate based on scope declination"));

            width = StringWidth(_T("00000"));
            m_pMaxRaDuration = pFrame->MakeSpinCtrl(GetParentWindow(AD_szMaxRAAmt), wxID_ANY, wxEmptyString,
                wxDefaultPosition, wxSize(width, -1), wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 150,
                _T("MaxRA_Dur"));
            AddLabeledCtrl(CtrlMap, AD_szMaxRAAmt, _("Max RA duration"), m_pMaxRaDuration,
                _("Longest length of pulse to send in RA\nDefault = 2500 ms."));

            m_pMaxDecDuration = pFrame->MakeSpinCtrl(GetParentWindow(AD_szMaxDecAmt), wxID_ANY, wxEmptyString,
                wxDefaultPosition, wxSize(width, -1), wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 150,
                _T("MaxDec_Dur"));
            AddLabeledCtrl(CtrlMap, AD_szMaxDecAmt, _("Max Dec duration"), m_pMaxDecDuration,
                _("Longest length of pulse to send in declination\nDefault = 2500 ms.  Increase if drift is fast."));

            wxString dec_choices[] = {
                Scope::DecGuideModeLocaleStr(DEC_NONE),
                Scope::DecGuideModeLocaleStr(DEC_AUTO),
                Scope::DecGuideModeLocaleStr(DEC_NORTH),
                Scope::DecGuideModeLocaleStr(DEC_SOUTH),
            };

            width = StringArrayWidth(dec_choices, WXSIZEOF(dec_choices));
            m_pDecMode = new wxChoice(GetParentWindow(AD_szDecGuideMode), wxID_ANY, wxDefaultPosition,
                wxSize(width + 35, -1), WXSIZEOF(dec_choices), dec_choices);
            AddLabeledCtrl(CtrlMap, AD_szDecGuideMode, _("Dec guide mode"), m_pDecMode, _("Directions in which Dec guide commands will be issued"));
            m_pDecMode->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &ScopeConfigDialogCtrlSet::OnDecModeChoice, this);
        }
        m_pScope->currConfigDialogCtrlSet = this;
    }
}

void ScopeConfigDialogCtrlSet::LoadValues()
{
    MountConfigDialogCtrlSet::LoadValues();
    int stepSize = m_pScope->GetCalibrationDuration();
    m_pCalibrationDuration->SetValue(stepSize);
    m_calibrationDistance = m_pScope->GetCalibrationDistance();
    m_pNeedFlipDec->SetValue(m_pScope->CalibrationFlipRequiresDecFlip());
    if (m_pStopGuidingWhenSlewing)
        m_pStopGuidingWhenSlewing->SetValue(m_pScope->IsStopGuidingWhenSlewingEnabled());
    m_assumeOrthogonal->SetValue(m_pScope->IsAssumeOrthogonal());
    int pulseSize;
    int floor;
    int ceiling;
    m_pScope->m_backlashComp->GetBacklashCompSettings(&pulseSize, &floor, &ceiling);
    m_pBacklashPulse->SetValue(pulseSize);
    m_pUseBacklashComp->SetValue(m_pScope->m_backlashComp->IsEnabled());
    bool usingAO = TheAO() != nullptr;
    if (!usingAO)
    {
        m_pBacklashFloor->SetValue(floor);
        m_pBacklashCeiling->SetValue(ceiling);
        m_pMaxRaDuration->SetValue(m_pScope->GetMaxRaDuration());
        m_pMaxDecDuration->SetValue(m_pScope->GetMaxDecDuration());
        int whichDecMode = m_pScope->GetDecGuideMode();
        m_pDecMode->SetSelection(whichDecMode);
        Mount::MountConfigDialogPane* pCurrMountPane = pFrame->pAdvancedDialog->GetCurrentMountPane();
        pCurrMountPane->EnableDecControls(whichDecMode != DEC_NONE);
        m_pUseDecComp->SetValue(m_pScope->DecCompensationEnabled());
        m_origBLCEnabled = m_pScope->m_backlashComp->IsEnabled();
        if (whichDecMode == DEC_AUTO)
        {
            m_pUseBacklashComp->SetValue(m_origBLCEnabled);
            m_pUseBacklashComp->Enable(true);
        }
        else
        {
            m_pUseBacklashComp->SetValue(false);
            m_pUseBacklashComp->Enable(false);
        }
    }
}

void ScopeConfigDialogCtrlSet::UnloadValues()
{
    bool usingAO = TheAO() != nullptr;
    m_pScope->SetCalibrationDuration(m_pCalibrationDuration->GetValue());
    m_pScope->SetCalibrationDistance(m_calibrationDistance);
    bool oldFlip = m_pScope->CalibrationFlipRequiresDecFlip();
    bool newFlip = m_pNeedFlipDec->GetValue();
    m_pScope->SetCalibrationFlipRequiresDecFlip(newFlip);
    if (oldFlip != newFlip)
    {
        m_pScope->ClearCalibration();
        Debug.Write(wxString::Format("User changed 'Dec-Flip' setting from %d to %d, calibration cleared\n", oldFlip, newFlip));
    }
    if (m_pStopGuidingWhenSlewing)
        m_pScope->EnableStopGuidingWhenSlewing(m_pStopGuidingWhenSlewing->GetValue());
    m_pScope->SetAssumeOrthogonal(m_assumeOrthogonal->GetValue());
    int newBC = m_pBacklashPulse->GetValue();
    int newFloor;
    int newCeiling;
    // Is using an AO, don't adjust the blc pulse size
    if (!usingAO)
    {
        newFloor = m_pBacklashFloor->GetValue();
        newCeiling = m_pBacklashCeiling->GetValue();
    }
    else
    {
        newFloor = newBC;
        newCeiling = newBC;
    }

    // SetBacklashPulseWidth will handle floor/ceiling values that don't make sense
    m_pScope->m_backlashComp->EnableBacklashComp(m_pUseBacklashComp->GetValue());
    m_pScope->m_backlashComp->SetBacklashPulseWidth(newBC, newFloor, newCeiling);

    // Following needed in case user changes max_duration with blc value already set
    if (m_pScope->m_backlashComp->IsEnabled() && m_pScope->GetMaxDecDuration() < newBC)
        m_pScope->SetMaxDecDuration(newBC);
    if (pFrame)
        pFrame->UpdateStatusBarCalibrationStatus();

    if (!usingAO)
    {
        m_pScope->EnableDecCompensation(m_pUseDecComp->GetValue());
        m_pScope->SetMaxRaDuration(m_pMaxRaDuration->GetValue());
        if (!m_pScope->m_backlashComp->IsEnabled())                       // handled above
            m_pScope->SetMaxDecDuration(m_pMaxDecDuration->GetValue());
        m_pScope->SetDecGuideMode(m_pDecMode->GetSelection());

    }
    MountConfigDialogCtrlSet::UnloadValues();
}

void ScopeConfigDialogCtrlSet::ResetRAParameterUI()
{
    m_pMaxRaDuration->SetValue(DefaultMaxRaDuration);
}

void ScopeConfigDialogCtrlSet::ResetDecParameterUI()
{
    m_pMaxDecDuration->SetValue(DefaultMaxDecDuration);
    m_pDecMode->SetSelection(1);                // 'Auto'
    m_pUseBacklashComp->SetValue(false);
}

DEC_GUIDE_MODE ScopeConfigDialogCtrlSet::GetDecGuideModeUI()
{
    return (DEC_GUIDE_MODE)m_pDecMode->GetSelection();
}

int ScopeConfigDialogCtrlSet::GetCalStepSizeCtrlValue()
{
    return m_pCalibrationDuration->GetValue();
}

void ScopeConfigDialogCtrlSet::SetCalStepSizeCtrlValue(int newStep)
{
    m_pCalibrationDuration->SetValue(newStep);
}

void ScopeConfigDialogCtrlSet::OnDecModeChoice(wxCommandEvent& evt)
{
    int which = m_pDecMode->GetSelection();
    // User choice of 'none' will disable Dec algo params in UI
    Mount::MountConfigDialogPane* pCurrMountPane = pFrame->pAdvancedDialog->GetCurrentMountPane();
    pCurrMountPane->EnableDecControls(which != DEC_NONE);
    m_pUseDecComp->SetValue(m_pScope->DecCompensationEnabled());
    if (which != DEC_AUTO)
    {
        m_pUseBacklashComp->SetValue(false);
        m_pUseBacklashComp->Enable(false);
    }
    else
    {
        m_pUseBacklashComp->SetValue(m_origBLCEnabled);
        m_pUseBacklashComp->Enable(true);
    }
}

void ScopeConfigDialogCtrlSet::OnCalcCalibrationStep(wxCommandEvent& evt)
{
    int focalLength = 0;
    double pixelSize = 0;
    int binning = 1;
    AdvancedDialog *pAdvancedDlg = pFrame->pAdvancedDialog;

    if (pAdvancedDlg)
    {
        pixelSize = pAdvancedDlg->GetPixelSize();
        binning = pAdvancedDlg->GetBinning();
        focalLength = pAdvancedDlg->GetFocalLength();
    }

    CalstepDialog calc(m_pParent, focalLength, pixelSize, binning);
    if (calc.ShowModal() == wxID_OK)
    {
        int calibrationStep;
        int distance;
        if (calc.GetResults(&focalLength, &pixelSize, &binning, &calibrationStep, &distance))
        {
            // Following sets values in the UI controls of the various dialog tabs - not underlying data values
            pAdvancedDlg->SetFocalLength(focalLength);
            pAdvancedDlg->SetPixelSize(pixelSize);
            pAdvancedDlg->SetBinning(binning);
            m_pCalibrationDuration->SetValue(calibrationStep);
            m_calibrationDistance = distance;
        }
    }
}

GraphControlPane *Scope::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return new ScopeGraphControlPane(pParent, this, label);
}

Scope::ScopeGraphControlPane::ScopeGraphControlPane(wxWindow *pParent, Scope *pScope, const wxString& label)
    : GraphControlPane(pParent, label)
{
    int width;
    m_pScope = pScope;
    pScope->m_graphControlPane = this;

    width = StringWidth(_T("0000"));
    m_pMaxRaDuration = pFrame->MakeSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1),
        wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 0);
    m_pMaxRaDuration->SetToolTip(_("Longest length of pulse to send in RA\nDefault = 2500 ms."));
    m_pMaxRaDuration->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &Scope::ScopeGraphControlPane::OnMaxRaDurationSpinCtrl, this);
    DoAdd(m_pMaxRaDuration, _("Mx RA"));

    width = StringWidth(_T("0000"));
    m_pMaxDecDuration = pFrame->MakeSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1),
        wxSP_ARROW_KEYS, MAX_DURATION_MIN, MAX_DURATION_MAX, 0);
    m_pMaxDecDuration->SetToolTip(_("Longest length of pulse to send in declination\nDefault = 2500 ms.  Increase if drift is fast."));
    m_pMaxDecDuration->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &Scope::ScopeGraphControlPane::OnMaxDecDurationSpinCtrl, this);
    DoAdd(m_pMaxDecDuration, _("Mx DEC"));

    wxString dec_choices[] = {
        DecGuideModeLocaleStr(DEC_NONE),
        DecGuideModeLocaleStr(DEC_AUTO),
        DecGuideModeLocaleStr(DEC_NORTH),
        DecGuideModeLocaleStr(DEC_SOUTH),
    };
    m_pDecMode = new wxChoice(this, wxID_ANY,
        wxDefaultPosition,wxDefaultSize, WXSIZEOF(dec_choices), dec_choices );
    m_pDecMode->SetToolTip(_("Directions in which Dec guide commands will be issued"));
    m_pDecMode->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &Scope::ScopeGraphControlPane::OnDecModeChoice, this);
    m_pControlSizer->Add(m_pDecMode);

    m_pMaxRaDuration->SetValue(m_pScope->GetMaxRaDuration());
    m_pMaxDecDuration->SetValue(m_pScope->GetMaxDecDuration());
    m_pDecMode->SetSelection(m_pScope->GetDecGuideMode());
}

Scope::ScopeGraphControlPane::~ScopeGraphControlPane()
{
    if (m_pScope)
    {
        m_pScope->m_graphControlPane = nullptr;
    }
}

void Scope::ScopeGraphControlPane::OnMaxRaDurationSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pScope->SetMaxRaDuration(m_pMaxRaDuration->GetValue());
}

void Scope::ScopeGraphControlPane::OnMaxDecDurationSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pScope->SetMaxDecDuration(m_pMaxDecDuration->GetValue());
}

void Scope::ScopeGraphControlPane::OnDecModeChoice(wxCommandEvent& WXUNUSED(evt))
{
    m_pScope->SetDecGuideMode(m_pDecMode->GetSelection());
}
