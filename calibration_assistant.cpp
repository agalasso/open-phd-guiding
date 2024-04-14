/*
*  calibration_assistant.cpp
*  PHD Guiding
*
*  Created by Bruce Waddington
*  Copyright (c) 2023 Bruce Waddington
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
#include "calibration_assistant.h"
#include "calstep_dialog.h"
#include <algorithm>

enum {defBestDec = 0, defBestOffset = 5, textWrapPoint = 500, slewSettleTime = 2000};
double const siderealSecsPerSec = 0.9973;
#define RateX(spd)  (spd * 3600.0 / (15.0 * siderealSecsPerSec))

class CalibrationAssistant : public wxDialog
{
    // UI controls
    wxStaticText *m_pExplanation;
    wxTextCtrl *m_pCurrOffset;
    wxTextCtrl *m_pCurrDec;
    wxRadioButton *m_pCurrEast;
    wxRadioButton *m_pTargetEast;
    wxRadioButton *m_pCurrWest;
    wxRadioButton *m_pTargetWest;
    wxSpinCtrl *m_pTargetOffset;
    wxSpinCtrl *m_pTargetDec;
    wxStaticText *m_pMessage;
    wxButton *m_pExplainBtn;
    wxStaticText *m_pWarning;
    wxButton *m_pSlewBtn;
    wxButton *m_pCalibrateBtn;
    wxTimer *m_pTimer;
    // Member variables
    bool m_monitoringCalibration;
    bool m_calibrationActive;
    wxString m_lastResult;
    bool m_sanityCheckDone;
    bool m_meridianFlipping;
    bool m_isSlewing;
    bool m_justSlewed;
    double m_currentRA;
    double m_currentDec;
    // Methods
    void ShowExplanationMsg(double dec);
    void ExplainResults(void);
    bool GetCalibPositionRecommendations(int* HA, int* Dec) const;
    void GetCustomLocation(int* PrefHA, int* PrefDec, bool* SingleSide, bool* UsingDefaults) const;
    void InitializeUI(bool forceDefaults);
    void UpdateCurrentPosition(bool fromTimer);
    void ShowError(const wxString& msg, bool fatal);
    void ShowStatus(const wxString& msg);

    void OnSlew(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
    void OnTimer(wxTimerEvent& evt);
    void OnCustom(wxCommandEvent& evt);
    void OnLoadCustom(wxCommandEvent& evt);
    void OnRestore(wxCommandEvent& evt);
    void OnTargetWest(wxCommandEvent& evt);
    void OnTargetEast(wxCommandEvent& evt);
    bool PerformSlew(double ra, double dec);
    void OnClose(wxCloseEvent& evt);
    void OnCalibrate(wxCommandEvent& evt);
    void OnExplain(wxCommandEvent& evt);

public:
    CalibrationAssistant();
    ~CalibrationAssistant();
    double GetCalibrationDec();
    void LoadCustomPosition(int CustHA, int CustDec);
private:
    void PerformSanityChecks(void);
    GUIDER_STATE m_guiderState;
    void TrackCalibration(GUIDER_STATE state);
    void EvaluateCalibration(void);
};

class CalAssistSanityDialog : public wxDialog
{
public:
    CalAssistSanityDialog(CalibrationAssistant* Parent, const wxString& msg);

private:
    CalibrationAssistant* m_parent;
    void OnRecal(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
};

class CalAssistExplanationDialog : public wxDialog
{
public:
    CalAssistExplanationDialog(const wxString& Why);
};

class CalCustomDialog : public wxDialog
{
    CalibrationAssistant *m_Parent;
    wxSpinCtrl *m_pTargetDec;
    wxSpinCtrl *m_pTargetOffset;
    wxRadioButton *m_pTargetWest;
    wxRadioButton *m_pTargetEast;
    wxCheckBox *m_pEastWestOnly;
    void OnOk(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
    void OnTargetWest(wxCommandEvent& evt);
    void OnTargetEast(wxCommandEvent& evt);

public:
    CalCustomDialog(CalibrationAssistant* Parent, int DefaultHA, int DefaultDec);
};

// Utility function to add the <label, input> pairs to a flexgrid
static void AddTableEntryPair(wxWindow *parent, wxFlexGridSizer *pTable, const wxString& label, wxWindow *pControl)
{
    wxStaticText *pLabel = new wxStaticText(parent, wxID_ANY, label + _(": "), wxPoint(-1, -1), wxSize(-1, -1));
    pTable->Add(pLabel, 1, wxALL, 5);
    pTable->Add(pControl, 1, wxALL, 5);
}

static wxSpinCtrl *NewSpinnerInt(wxWindow *parent, const wxSize& size, int val, int minval, int maxval, int inc,
    const wxString& tooltip)
{
    wxSpinCtrl *pNewCtrl = pFrame->MakeSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, size, wxSP_ARROW_KEYS, minval, maxval, val, _("Exposure time"));
    pNewCtrl->SetValue(val);
    pNewCtrl->SetToolTip(tooltip);
    return pNewCtrl;
}

static void MakeBold(wxControl *ctrl)
{
    wxFont font = ctrl->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    ctrl->SetFont(font);
}

CalibrationAssistant::CalibrationAssistant()
    : wxDialog(pFrame, wxID_ANY, _("Calibration Assistant"),
    wxDefaultPosition, wxSize(700, -1), wxCAPTION | wxCLOSE_BOX),
    m_sanityCheckDone(0),
    m_justSlewed(0),
    m_isSlewing(0),
    m_monitoringCalibration(0),
    m_calibrationActive(0)

{
    wxStaticBoxSizer* currSizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Current Pointing Location"));
    wxStaticBoxSizer* tgtSizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Calibration Location"));
    wxFlexGridSizer* currPosSizer = new wxFlexGridSizer(1, 5, 5, 15);
    wxFlexGridSizer* targetPosSizer = new wxFlexGridSizer(1, 5, 5, 15);

    m_pExplanation = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(680, -1), wxALIGN_LEFT);
    MakeBold(m_pExplanation);
    int textWidth = StringWidth(this, "000000000");
    m_pCurrOffset = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(textWidth, -1));
    m_pCurrDec = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(textWidth, -1));
    wxStaticBoxSizer* sizerCurrSOP = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Pointing"));
    m_pCurrWest = new wxRadioButton(this, wxID_ANY, _("West"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_pCurrEast = new wxRadioButton(this, wxID_ANY, _("East"));
    sizerCurrSOP->Add(m_pCurrWest);
    sizerCurrSOP->Add(m_pCurrEast);

    AddTableEntryPair(this, currPosSizer, _("Declination"), m_pCurrDec);
    AddTableEntryPair(this, currPosSizer, _("Meridian offset (degrees)"), m_pCurrOffset);
    currPosSizer->Add(sizerCurrSOP);
    currSizer->Add(currPosSizer);
    MakeBold(m_pCurrDec);
    MakeBold(m_pCurrOffset);

    int spinnerWidth = StringWidth(this, "0000");
    m_pTargetDec = NewSpinnerInt(this, wxSize(spinnerWidth, -1), 0, -50, 50, 5, _("Target declination for slew, as close to Dec = 0 as possible for your location (>=-20 and <= 20) recommended"));
    AddTableEntryPair(this, targetPosSizer, _("Declination"), m_pTargetDec);
    m_pTargetOffset = NewSpinnerInt(this, wxSize(spinnerWidth, -1), 10, 5, 50, 5, _("Target offset from central meridian, in degrees; east or west based on 'Pointing' buttons (less than 15 degrees recommended)"));
    AddTableEntryPair(this, targetPosSizer, _("Meridian offset (degrees)"), m_pTargetOffset);

    wxStaticBoxSizer* sizerTargetSOP = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Pointing"));
    m_pTargetWest = new wxRadioButton(this, wxID_ANY, _("West"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_pTargetWest->SetToolTip(_("Scope on the east side of pier, pointing west"));
    m_pTargetWest->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &CalibrationAssistant::OnTargetWest, this);
    m_pTargetEast = new wxRadioButton(this, wxID_ANY, _("East"));
    m_pTargetEast->SetToolTip(_("Scope on west side of pier, pointing east"));
    m_pTargetEast->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &CalibrationAssistant::OnTargetEast, this);
    sizerTargetSOP->Add(m_pTargetWest);
    sizerTargetSOP->Add(m_pTargetEast);
    targetPosSizer->Add(sizerTargetSOP);
    tgtSizer->Add(targetPosSizer);

    wxBoxSizer* midBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton *pCustomBtn = new wxButton(this, wxID_ANY, _("Save custom values..."));
    pCustomBtn->SetToolTip(_("Save a custom sky location if your site has restricted sky visibility and you can't calibrate at the recommended location"));
    pCustomBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalibrationAssistant::OnCustom, this);
    wxButton *pLoadBtn = new wxButton(this, wxID_ANY, _("Load custom values"));
    pLoadBtn->SetToolTip(_("Reload a previously saved custom location and displays its values in the 'Calibration Location' fields"));
    pLoadBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalibrationAssistant::OnLoadCustom, this);
    wxButton* pRestoreBtn = new wxButton(this, wxID_ANY, _("Restore defaults"));
    pRestoreBtn->SetToolTip(_("Restore the 'Calibration Location' fields to show the recommended pointing location"));
    pRestoreBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalibrationAssistant::OnRestore, this);
    midBtnSizer->Add(pLoadBtn, wxSizerFlags().Center().Border(wxALL, 20));
    midBtnSizer->Add(pCustomBtn, wxSizerFlags().Center().Border(wxALL, 20));
    midBtnSizer->Add(pRestoreBtn, wxSizerFlags().Center().Border(wxALL, 20));

    m_pMessage = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, 75), wxALIGN_CENTER_HORIZONTAL | wxST_NO_AUTORESIZE);
    MakeBold(m_pMessage);
    m_pWarning = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, -1), wxALIGN_CENTER_HORIZONTAL);
    MakeBold(m_pWarning);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_pSlewBtn = new wxButton(this, wxID_ANY, _("Slew"));
    m_pSlewBtn->SetToolTip(_("Start a slew to the calibration location. BE SURE the scope can be safely slewed"));
    m_pSlewBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalibrationAssistant::OnSlew, this);
    m_pCalibrateBtn = new wxButton(this, wxID_ANY, _("Calibrate"));
    m_pCalibrateBtn->SetToolTip(_("Start the PHD2 calibration.  The Calibration Assistant window will remain open to monitor and assess results"));
    m_pCalibrateBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalibrationAssistant::OnCalibrate, this);
    wxButton* pCancelBtn = new wxButton(this, wxID_ANY, _("Cancel"));
    pCancelBtn->SetToolTip(_("Close the Calibration Assistant window.  Any calibration currently underway will continue."));
    pCancelBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalibrationAssistant::OnCancel, this);

    btnSizer->Add(m_pSlewBtn, wxSizerFlags().Border(wxALL, 10));
    btnSizer->Add(m_pCalibrateBtn, wxSizerFlags().Border(wxALL, 10));
    btnSizer->Add(pCancelBtn, wxSizerFlags().Border(wxALL, 10));

    m_pExplainBtn = new wxButton(this, wxID_ANY, _("Explain"));
    m_pExplainBtn->SetToolTip(_("Show additional information about any calibration result that is less than 'good'"));
    m_pExplainBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalibrationAssistant::OnExplain, this);
    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    vSizer->Add(m_pExplanation, wxSizerFlags().Center().Border(wxTop, 5).Border(wxLEFT, 20));
    vSizer->Add(currSizer, wxSizerFlags().Center().Border(wxALL, 20));
    vSizer->Add(tgtSizer, wxSizerFlags().Center());
    vSizer->Add(midBtnSizer, wxSizerFlags().Center().Border(wxTOP, 5));
    vSizer->Add(m_pExplainBtn, wxSizerFlags().Center().Border(wxALL, 10));
    vSizer->Add(m_pWarning, wxSizerFlags().Center().Border(wxTOP, 10));
    vSizer->Add(m_pMessage, wxSizerFlags().Center().Border(wxTOP, 15));
    vSizer->Add(btnSizer, wxSizerFlags().Center().Border(wxTOP, 5));

    m_pTimer = new wxTimer(this, wxID_ANY);     // asynch updates to current position fields
    m_pTimer->SetOwner(this);
    this->Connect(wxEVT_TIMER, wxTimerEventHandler(CalibrationAssistant::OnTimer), NULL, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &CalibrationAssistant::OnClose, this);

    InitializeUI(true);
    m_pCurrOffset->Enable(false);
    m_pCurrDec->Enable(false);
    m_pCurrWest->Enable(false);
    m_pCurrEast->Enable(false);
    SetAutoLayout(true);
    SetSizerAndFit(vSizer);
    m_pExplainBtn->Enable(false);
}

CalibrationAssistant::~CalibrationAssistant()
{
    delete m_pTimer;
    pFrame->pCalibrationAssistant = NULL;
}

void CalibrationAssistant::GetCustomLocation(int* PrefHA, int* PrefDec, bool* SingleSide, bool* UsingDefaults) const
{
    *PrefHA = pConfig->Profile.GetInt("/scope/CalSlew/TgtHA", defBestOffset);
    *PrefDec = pConfig->Profile.GetInt("/scope/CalSlew/TgtDec", defBestDec);
    *SingleSide = pConfig->Profile.GetBoolean("/scope/CalSlew/SingleSide", false);
    *UsingDefaults = (*PrefDec == defBestDec && *PrefHA == defBestOffset && !*SingleSide);
}

void CalibrationAssistant::PerformSanityChecks(void)
{
    if (!pPointingSource || !pPointingSource->IsConnected())
        return;

    double raSpd;
    double decSpd;
    wxString msg = wxEmptyString;

    bool error = pPointingSource->GetGuideRates(&raSpd, &decSpd);
    if (error)
        return;
    if (!pPointingSource->ValidGuideRates(raSpd, decSpd))
        return;

    CalAssistSanityDialog* sanityDlg;
    double minSpd;
    double sidRate;
    if (decSpd != -1)
        minSpd = wxMin(raSpd, decSpd);
    else
        minSpd = raSpd;
    sidRate = RateX(minSpd);
    if (sidRate < 0.5)
    {
        if (sidRate <= 0.2)
            msg = _("Your mount guide speed is too slow for effective calibration and guiding."
                " Use the hand-controller or mount driver to increase the guide speed to at least 0.5x sidereal."
                " Then click the 'Recalc' button so PHD2 can compute a correct calibration step-size.");
        else
            msg = _("Your mount guide speed is below the minimum recommended value of 0.5x sidereal."
            " Use the hand-controller or mount driver to increase the guide speed to at least 0.5x sidereal."
            " Then click the 'Recalc' button so PHD2 can compute a correct calibration step-size.");
    }
    else
    {
        int recDistance = CalstepDialog::GetCalibrationDistance(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(), pCamera->Binning);
        int currStepSize = TheScope()->GetCalibrationDuration();
        int recStepSize;
        CalstepDialog::GetCalibrationStepSize(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(), pCamera->Binning, sidRate,
            CalstepDialog::DEFAULT_STEPS, m_currentDec, recDistance, 0, &recStepSize);
        if (fabs(1.0 - (double)currStepSize / (double)recStepSize) > 0.3)           // Within 30% is good enough
        {
            msg = _("Your current calibration parameters can be adjusted for more accurate results."
                " Click the 'Recalc' button to restore them to the default values.");
        }
    }
    if (!msg.empty())
    {
        sanityDlg = new CalAssistSanityDialog(this, msg);
        sanityDlg->ShowModal();
    }
}

void CalibrationAssistant::ShowError(const wxString& msg, bool fatal)
{
    m_pMessage->SetLabelText(msg);
    if (fatal)
    {
        m_pSlewBtn->Enable(false);
        m_pCalibrateBtn->Enable(false);
    }
}

void CalibrationAssistant::ShowStatus(const wxString& msg)
{
    m_pMessage->SetLabelText(msg);
}

double CalibrationAssistant::GetCalibrationDec()
{
    return m_currentDec;
}

void CalibrationAssistant::TrackCalibration(GUIDER_STATE state)
{
    switch (state)
    {
    case STATE_UNINITIALIZED:
    case STATE_SELECTING:
    case STATE_SELECTED:
        if (m_calibrationActive)
        {
            ShowStatus(_("Calibration failed or was cancelled"));
            m_lastResult = "Incomplete";
            m_calibrationActive = false;
            m_monitoringCalibration = false;
            m_pSlewBtn->Enable(true);
            m_pCalibrateBtn->Enable(true);
            m_pExplainBtn->Enable(true);
        }
        break;
    case STATE_CALIBRATING_PRIMARY:
    case STATE_CALIBRATING_SECONDARY:
        m_calibrationActive = true;
        break;
    case STATE_CALIBRATED:
    case STATE_GUIDING:
    {
        m_calibrationActive = false;
        m_monitoringCalibration = false;
        EvaluateCalibration();
        m_pCalibrateBtn->Enable(true);
        break;
    }
    case STATE_STOP:
        break;

    }
}

void CalibrationAssistant::OnTimer(wxTimerEvent& evt)
{
    if (!m_monitoringCalibration)
        UpdateCurrentPosition(true);
    else
        TrackCalibration(pFrame->pGuider->GetState());
}

void CalibrationAssistant::UpdateCurrentPosition(bool fromTimer)
{
    double ra, dec, lst;
    double hourAngle;
    const int INVALID_DEC = 100;
    static double lastDec = INVALID_DEC;

    bool error = pPointingSource->GetCoordinates(&ra, &dec, &lst);
    if (error)
    {
        ShowError(_("Mount can't report its pointing position"), true);
        if (m_pTimer)
            m_pTimer->Stop();
        return;
    }

    m_currentRA = ra;
    m_currentDec = dec;
    hourAngle = norm(lst - ra, -12.0, 12.0);
    if (hourAngle < 0)
        m_pCurrEast->SetValue(true);
    else
        m_pCurrWest->SetValue(true);
    if (!m_isSlewing)
    {
        if (m_pCurrWest->GetValue() != m_pTargetWest->GetValue())
            m_pWarning->SetLabelText(_("MERIDIAN FLIP!"));
        else
            m_pWarning->SetLabelText(wxEmptyString);
    }
    else
        m_pWarning->SetLabelText(_("WATCH SCOPE DURING SLEWING TO INSURE SAFETY"));
    m_pCurrOffset->SetValue(wxString::Format("%.1f", abs(hourAngle * 15.0)));
    m_pCurrDec->SetValue(wxString::Format("%+.1f", dec));
    if (!m_meridianFlipping)
    {
        if (fromTimer)
        {
            if (fabs(lastDec - dec) > 10.)
            {
                if (lastDec != INVALID_DEC)
                    ShowExplanationMsg(dec);
                lastDec = dec;
            }
        }
        else
            ShowExplanationMsg(dec);
    }
}

// Return of 'true' means error occurred
bool CalibrationAssistant::GetCalibPositionRecommendations(int* HA, int* Dec) const
{
    double hourAngle;
    int bestDec;
    int bestOffset;
    double lst;
    double ra;
    double dec;
    double eqAltitude;
    bool pointingEast = false;
    bool errorSeen = false;

    bestDec = defBestDec;
    bestOffset = defBestOffset;
    try
    {
        if (!pPointingSource || !pPointingSource->CanReportPosition())
            throw ERROR_INFO("CalPositionRecommendations: mount not connected or not reporting position");

        if (pPointingSource->PreparePositionInteractive())
            return true;
        if (pPointingSource->GetCoordinates(&ra, &dec, &lst))
        {
            throw ERROR_INFO("CalPositionRecommendations: Mount not reporting pointing position");
        }
        hourAngle = norm(lst - ra, -12.0, 12.0);
        if (hourAngle <= 0.)
            bestOffset = -bestOffset;
        // Check that we aren't pointing down in the weeds at the default Dec location
        double lat, lon;
        if (pPointingSource->GetSiteLatLong(&lat, &lon))
            bestDec = 0;
        else if (lat >= 0.)
        {
            eqAltitude = 90.0 - lat;
            if (eqAltitude < 30.)        // far north location
                bestDec += 30. - eqAltitude;
        }
        else
        {
            eqAltitude = 90. + lat;
            if (eqAltitude < 30.)        // far south location
                bestDec -= (30. - eqAltitude);
        }
        *HA = bestOffset;
        *Dec = bestDec;
        m_pSlewBtn->Enable(pPointingSource->CanSlew());
    }
    catch (const wxString& msg)
    {
        POSSIBLY_UNUSED(msg);
        *HA = defBestOffset;
        *Dec = defBestDec;
        errorSeen = true;
    }
    return errorSeen;
}

void CalibrationAssistant::ShowExplanationMsg(double dec)
{
    wxString slewCond;
    wxString explanation = wxEmptyString;
    if (pPointingSource->CanSlew())
        slewCond = _("Use the 'slew' button to move the scope to within 20 degrees of Dec = 0 or as close to that as your site will allow.");
    else
        slewCond = _("Slew the scope to within 20 degrees of Dec = 0 or as close to that as your site will allow.");
    if (fabs(dec) > 80.)
    {
        explanation = _("Calibration is likely to fail this close to the pole.") + " " + slewCond;
    }
    else if (fabs(dec) > degrees(Scope::DEC_COMP_LIMIT))
    {
        explanation = _("If you calibrate within 30 degrees of the pole, you will need to recalibrate when you slew to a different target.")
            + " " + slewCond;
    }
    else if (fabs(dec) > 20.)
        explanation = _("Calibration will be more accurate with the scope pointing closer to the celestial equator.") + " " + slewCond;
    m_pExplanation->SetLabelText(explanation);
    m_pExplanation->Wrap(textWrapPoint);
}

void CalibrationAssistant::InitializeUI(bool forceDefaults)
{
    if (!pPointingSource || !pPointingSource->CanReportPosition())
    {
        if (!pPointingSource || !pPointingSource->IsConnected())
            ShowError(_("Mount is not connected"), true);
        else
            ShowError(_("Mount can't report its pointing position"), true);
        return;
    }

    int bestDec;
    int bestOffset;
    bool singleSide = false;
    bool usingDefaults;
    double hourAngle;
    double ra;
    double dec;
    double lst;
    bool pointingEast = false;
    bool noLocationInfo = false;

    if (pPointingSource->GetCoordinates(&ra, &dec, &lst))
    {
        ShowError(_("Mount can't report its pointing position"), true);
        return;
    }

    if (forceDefaults)
    {
        usingDefaults = true;
    }
    else
    {
        CalibrationAssistant::GetCustomLocation(&bestOffset, &bestDec, &singleSide, &usingDefaults);    // Get any custom preferences if present
    }

    ShowExplanationMsg(dec);
    m_currentRA = ra;
    m_currentDec = dec;
    hourAngle = norm(lst - ra, -12.0, 12.0);

    if (!usingDefaults)                                             // Custom pointing position
    {
        if (singleSide)                                             // Use specified E-W orientation
        {
            if (bestOffset <= 0)
                m_pTargetEast->SetValue(true);
            else
                m_pTargetWest->SetValue(true);
        }
        else
        {
            if (hourAngle <= 0)                                     // Use same E-W orientation as current pointing position
                m_pTargetEast->SetValue(true);
            else
                m_pTargetWest->SetValue(true);
        }
        bestOffset = abs(bestOffset);
        m_pTargetOffset->SetValue(wxString::Format("%d", bestOffset));
        m_pTargetDec->SetValue(wxString::Format("%d", bestDec));
    }
    else
    {
        if (!GetCalibPositionRecommendations(&bestOffset, &bestDec))
        {
            pointingEast = (bestOffset <= 0);

            m_pTargetOffset->SetValue(wxString::Format("%d", abs(bestOffset)));
            m_pTargetDec->SetValue(wxString::Format("%d", bestDec));
            if (pointingEast)
                m_pTargetEast->SetValue(true);
            else
                m_pTargetWest->SetValue(true);
        }
        else
        {
            ShowError(_("Mount can't report its pointing position"), true);
            return;
        }
    }           // end of default handling

    // Current position
    m_pCurrOffset->SetValue(wxString::Format("%.1f", abs(hourAngle * 15.0)));
    m_pCurrDec->SetValue(wxString::Format("%+.1f", dec));
    m_pCurrEast->SetValue(hourAngle <= 0);
    if (m_pCurrEast->GetValue() != m_pTargetEast->GetValue())
        m_pWarning->SetLabelText(_("MERIDIAN FLIP!"));
    else
        m_pWarning->SetLabelText(wxEmptyString);

    m_pTimer->Stop();
    m_pTimer->Start(1500, false /* continuous */);
    if (pPointingSource->CanSlew())
        ShowStatus(_("Adjust 'Calibration Location' values if needed for your site, then click 'Slew'"));
    else
        ShowStatus(wxString::Format(_("Manually move the telescope to a Dec location near %d"), bestDec));
}

void CalibrationAssistant::LoadCustomPosition(int CustHA, int CustDec)
{
    m_pTargetOffset->SetValue(abs(CustHA));
    m_pTargetDec->SetValue(CustDec);
    m_pWarning->SetLabelText(wxEmptyString);
    if (CustHA <= 0)
    {
        m_pTargetEast->SetValue(true);
        if (m_pCurrWest->GetValue())
            m_pWarning->SetLabelText("MERIDIAN FLIP!");
    }
    else
    {
        m_pTargetWest->SetValue(true);
        if (m_pCurrEast->GetValue())
            m_pWarning->SetLabelText("MERIDIAN FLIP!");
    }
}

bool CalibrationAssistant::PerformSlew(double ra, double dec)
{
    bool completed = false;
    if (pFrame->CaptureActive)
        pFrame->StopCapturing();

    if (pPointingSource->CanSlewAsync())
    {
        struct SlewInBg : public RunInBg
        {
            double ra, dec;
            SlewInBg(wxWindow *parent, double ra_, double dec_)
                : RunInBg(parent, _("Slew"), _("Slewing...")), ra(ra_), dec(dec_)
            {
                SetPopupDelay(100);
            }
            bool Entry()        //wxWidgets background thread method
            {
                bool err = pPointingSource->SlewToCoordinatesAsync(ra, dec);
                if (err)
                {
                    SetErrorMsg(_("Slew failed! Make sure scope is tracking at sidereal rate"));
                    return true;
                }
                while (pPointingSource->Slewing())
                {
                    wxMilliSleep(500);
                    if (IsCanceled())
                    {
                        pPointingSource->AbortSlew();
                        SetErrorMsg(_("Slew was cancelled"));
                        return true;
                    }
                }
                return false;
            }
        };
        SlewInBg bg(this, ra, dec);
        m_isSlewing = true;
        // Additional 2-sec delays are used here because some mount controllers report slew-completions before the mount has completely stopped moving - for example with behind-the-scenes clearing of RA backlash
        // Starting a calibration before everything has settled will produce a bad result.  The forced delays are simply an extra safety margin to reduce the likelihood of this happening.
        if (bg.Run())
        {
            m_isSlewing = false;
            ShowError(bg.GetErrorMsg(), false);
        }
        else
        {
            m_isSlewing = false;
            ShowStatus(_("Pausing..."));
            wxMilliSleep(slewSettleTime);
            UpdateCurrentPosition(false);
            ShowExplanationMsg(dec);
            completed = true;
        }
    }
    else
    {
        wxBusyCursor busy;
        m_isSlewing = true;
        if (!pPointingSource->SlewToCoordinates(ra, dec))
        {
            m_isSlewing = false;
            ShowStatus(_("Pausing..."));
            wxMilliSleep(slewSettleTime);
            ShowExplanationMsg(dec);
            ShowStatus(_("Wait for tracking to stabilize, then click 'Calibrate' to start calibration or 'Cancel' to exit"));
            completed = true;
        }
        else
        {
            m_isSlewing = false;
            ShowError(_("Slew failed! Make sure scope is tracking at sidereal rate"), false);
            Debug.Write("Cal-slew: slew failed\n");
        }
    }
    m_justSlewed = completed;
    return !completed;
}

void CalibrationAssistant::OnSlew(wxCommandEvent& evt)
{
    double offsetSlew = (double) m_pTargetOffset->GetValue();
    double decSlew = (double) m_pTargetDec->GetValue();
    double slew_ra;
    bool clearBacklash = false;

    double cur_ra, cur_dec, cur_st;
    ShowStatus("");
    if (pPointingSource->GetCoordinates(&cur_ra, &cur_dec, &cur_st))
    {
        Debug.Write("Cal-slew: slew failed to get scope coordinates\n");
        ShowError("Could not get coordinates from mount", true);
        return;
    }

    if (m_pTargetEast->GetValue())
        slew_ra = norm_ra(cur_st + (offsetSlew / 15.0));
    else
        slew_ra = norm_ra(cur_st - (offsetSlew / 15.0));

    m_pSlewBtn->Enable(false);
    m_pCalibrateBtn->Enable(false);
    m_meridianFlipping = (m_pWarning->GetLabelText().Length() > 0);
    Debug.Write(wxString::Format("CalAsst: slew from ra %.2f, dec %.1f to ra %.2f, dec %.1f, M/F = %d\n",
        cur_ra, cur_dec, slew_ra, decSlew, m_meridianFlipping));
    if (decSlew <= cur_dec || m_meridianFlipping)         // scope will slew sky-south regardless of north or south hemisphere
    {
        ShowStatus(_("Initial slew to approximate position"));
        if (!PerformSlew(slew_ra, decSlew - 1.0))
        {
            ShowStatus(_("Final slew north to pre-clear Dec backlash"));
            if (!PerformSlew(slew_ra, decSlew))
                ShowStatus(_("Wait for tracking to stabilize, then click 'Calibrate' to start calibration or 'Cancel' to exit"));
        }
    }
    else
    {
        ShowStatus(_("Slewing to calibration location"));
        if (!PerformSlew(slew_ra, decSlew))
            ShowStatus(_("Wait for tracking to stabilize, then Click 'Calibrate' to start calibration or 'Cancel' to exit"));
    }
    m_pSlewBtn->Enable(true);
    if (TheScope())
        m_pCalibrateBtn->Enable(true);
}

static wxString VectorToString(const wxString& separator, const std::vector<wxString>& vec)
{
    if (vec.empty())
        return wxEmptyString;

    size_t l = 0;
    std::for_each(vec.begin(), vec.end(), [&l](const wxString& s) { l += s.length(); });
    wxString buf;
    buf.reserve(l + separator.length() * (vec.size() - 1));
    std::for_each(vec.begin(), vec.end(),
        [&buf, &separator](const wxString& s) {
        if (!buf.empty())
            buf += separator;
        buf += s;
    });
    return buf;
}

void CalibrationAssistant::EvaluateCalibration(void)
{
    static const int MAX_CALIBRATION_STEPS = 60;
    static const int CAL_ALERT_MINSTEPS = 4;
    static const double CAL_ALERT_ORTHOGONALITY_TOLERANCE = 12.5;               // Degrees
    static const double CAL_ALERT_DECRATE_DIFFERENCE = 0.20;                    // Ratio tolerance
    static const double CAL_ALERT_AXISRATES_TOLERANCE = 0.20;                   // Ratio tolerance
    bool ratesMeaningful = false;
    bool goodRslt = true;
    bool acceptableRslt;
    wxString evalWhy = wxEmptyString;
    std::vector<wxString> reasons;
    wxString debugVals = "CalAsst: ";
    double actualRatio = 1.0;
    double expectedRatio = 1.0;
    double speedRatio = 1.0;

    if (TheScope()->IsCalibrated())
    {
        Calibration newCal;
        TheScope()->GetLastCalibration(&newCal);
        CalibrationDetails newDetails;
        TheScope()->LoadCalibrationDetails(&newDetails);
        // See if we already triggered a sanity check alert on this last calibration
        acceptableRslt = (newDetails.lastIssue == CalibrationIssueType::CI_None || newDetails.lastIssue == CalibrationIssueType::CI_Different);

        // RA/Dec rates should be related by cos(dec) but don't check if Dec is too high or Dec guiding is disabled.  Also don't check if DecComp
        // is disabled because a Sitech controller might be monkeying around with the apparent rates
        if (newCal.declination != UNKNOWN_DECLINATION && newCal.yRate != CALIBRATION_RATE_UNCALIBRATED &&
            fabs(newCal.declination) <= Scope::DEC_COMP_LIMIT && TheScope()->DecCompensationEnabled())
        {
            double expectedRatio = cos(newCal.declination);
            if (newDetails.raGuideSpeed > 0.)                   // for mounts that may have different guide speeds on RA and Dec axes
                speedRatio = newDetails.decGuideSpeed / newDetails.raGuideSpeed;
            else
                speedRatio = 1.0;
            actualRatio = newCal.xRate * speedRatio / newCal.yRate;
            ratesMeaningful = true;
            debugVals += wxString::Format("Spds: %.1fX,%.1fX, ", RateX(newDetails.raGuideSpeed), RateX(newDetails.decGuideSpeed)) +
                wxString::Format("Dec: %.1f, Rates: %.1f, %.1f, ", degrees(newCal.declination), RateX(newCal.xRate), RateX(newCal.yRate));
        }
        else
            debugVals += "Spds: N/A, ";

        goodRslt = (newDetails.raStepCount >= 2 * CAL_ALERT_MINSTEPS || (newDetails.decStepCount >= 2 * CAL_ALERT_MINSTEPS && newDetails.decStepCount > 0)) &&         // Dec guiding might be disabled
            (newDetails.raStepCount <= 30 || (newDetails.decStepCount <= 30 && newDetails.decStepCount > 0));       // Not too few, not too many
        debugVals += wxString::Format("Steps: %d,%d, ", newDetails.raStepCount, newDetails.decStepCount);
        if (!goodRslt)
            reasons.push_back(_("Steps"));

        // Non-orthogonal RA/Dec axes. Values in Calibration structures are in radians
        double nonOrtho = degrees(fabs(fabs(norm_angle(newCal.xAngle - newCal.yAngle)) - M_PI / 2.));         // Delta from the nearest multiple of 90 degrees
        debugVals += wxString::Format("Ortho: %.2f, ", nonOrtho);
        if (nonOrtho > 5)
        {
            reasons.push_back(_("Orthogonality"));
            goodRslt = false;
            acceptableRslt = acceptableRslt && (nonOrtho <= CAL_ALERT_ORTHOGONALITY_TOLERANCE);
        }
        if (ratesMeaningful)
        {
            debugVals += wxString::Format("Rates: %.2f (Expect) vs %.2f (Act)", cos(newCal.declination), actualRatio);
            if (fabs(cos(newCal.declination) - actualRatio) > 0.1)
            {
                reasons.push_back(_("Rates"));
                goodRslt = false;
                acceptableRslt = acceptableRslt && fabs(expectedRatio - actualRatio) < CAL_ALERT_AXISRATES_TOLERANCE;
            }
        }
        if (fabs(degrees(newCal.declination)) > 60)
        {
            goodRslt = false;
            acceptableRslt = false;
            reasons.push_back(_("Sky location (Dec comp disabled)"));
        }
        else if (fabs(degrees(newCal.declination)) > 20)
        {
            goodRslt = false;
            reasons.push_back(_("Sky location"));
        }

        if (!reasons.empty())
            evalWhy = wxString::Format(_("(%s)"), VectorToString(_(", "), reasons));
        m_lastResult = evalWhy;
        Debug.Write(debugVals + "\n");

        if (goodRslt)
        {
            ShowStatus(_("Calibration result was good, guiding is active using the new calibration"));
            Debug.Write("CalAsst: good result\n");
        }
        else if (acceptableRslt)
        {
            {
                ShowStatus(_("Calibration result was acceptable, guiding is active using the new calibration") + "\n" + evalWhy);
                Debug.Write("CalAsst: acceptable result, " + evalWhy + "\n");
            }
        }
        else
        {
            ShowStatus(_("Calibration result was poor, consider re-doing it while following all recommendations") + "\n" + evalWhy);
            Debug.Write("CalAsst: poor result, " + evalWhy + "\n");
        }
    }
    else
    {
        ShowStatus(_("Calibration failed - probably because the mount didn't move at all"));
        Debug.Write("CalAsst: calibration failed\n");
    }
    m_pExplainBtn->Enable(!goodRslt);
    m_pSlewBtn->Enable(true);
}

void CalibrationAssistant::OnCalibrate(wxCommandEvent& evt)
{
    SettleParams settle;
    wxString msg;
    settle.tolerancePx = 99.;
    settle.settleTimeSec = 9999;
    settle.timeoutSec = 9999;
    settle.frames = 5;

    if (pPointingSource->PreparePositionInteractive())
        return;

    double lst;
    if (pPointingSource->GetCoordinates(&m_currentRA, &m_currentDec, &lst))
    {
        ShowError(_("Scope isn't reporting current position"), true);
        return;
    }
    if (!m_sanityCheckDone)
    {
        m_sanityCheckDone = true;
        PerformSanityChecks();
    }

    if (m_currentDec >= 80)
    {
        ShowStatus(_("Slew the scope closer to Dec = 0"));
        return;
    }
    if (!m_justSlewed && pPointingSource->CanSlew())
    {
        if (pFrame->CaptureActive)
            pFrame->StopCapturing();
        ShowStatus(_("Pre-clearing backlash"));
        if (PerformSlew(m_currentRA, m_currentDec + 2.0 / 60.))        // Status messages generated are handled by PerformSlew
            return;
    }
    m_pSlewBtn->Enable(false);
    m_pCalibrateBtn->Enable(false);
    // Looping left inactive will force another auto-find which we need because the guide star will have moved
    if (PhdController::Guide(GUIDEOPT_FORCE_RECAL, settle, wxRect(), &msg))
    {
        ShowStatus(_("Waiting for calibration to complete"));
        m_monitoringCalibration = true;
        m_justSlewed = false;
    }
    else
    {
        ShowError(_("Calibration could not start - suspend any imaging automation apps"), false);
        m_pSlewBtn->Enable(true);
        m_pCalibrateBtn->Enable(true);
    }
}

void CalibrationAssistant::ExplainResults()
{
    CalAssistExplanationDialog* dlg = new CalAssistExplanationDialog(m_lastResult);

    dlg->ShowModal();
}

void CalibrationAssistant::OnTargetWest(wxCommandEvent& evt)
{
    if (m_pCurrEast->GetValue())
        m_pWarning->SetLabelText(_("MERIDIAN FLIP!"));
    else
        m_pWarning->SetLabelText(wxEmptyString);
}

void CalibrationAssistant::OnTargetEast(wxCommandEvent& evt)
{
    if (m_pCurrWest->GetValue())
        m_pWarning->SetLabelText("MERIDIAN FLIP!");
    else
        m_pWarning->SetLabelText(wxEmptyString);
}

void CalibrationAssistant::OnCancel(wxCommandEvent& evt)
{
    wxDialog::Destroy();
}

void CalibrationAssistant::OnExplain(wxCommandEvent& evt)
{
    //m_lastResult = "Steps, Rates, Orthogonality, Sky location, Incomplete";
    ExplainResults();
}
void CalibrationAssistant::OnClose(wxCloseEvent& evt)
{
    wxDialog::Destroy();
}

void CalibrationAssistant::OnRestore(wxCommandEvent& evt)
{
    InitializeUI(true);
}

void CalibrationAssistant::OnLoadCustom(wxCommandEvent& evt)
{
    InitializeUI(false);
}

void CalibrationAssistant::OnCustom(wxCommandEvent& evt)
{
    int ha = m_pTargetOffset->GetValue();
    int dec = m_pTargetDec->GetValue();
    bool sideEast = m_pTargetEast->GetValue() == 1;
    if (sideEast)
        ha = -ha;
    CalCustomDialog custDlg(this, ha, dec);
    custDlg.ShowModal();            // Dialog handles the UI updates
}

CalCustomDialog::CalCustomDialog(CalibrationAssistant* Parent, int DefaultHA, int DefaultDec)
    : wxDialog(pFrame, wxID_ANY, _("Save Customized Calibration Position"),
    wxDefaultPosition, wxSize(474, -1), wxCAPTION | wxCLOSE_BOX)
{
    m_Parent = Parent;
    wxStaticBoxSizer* tgtSizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Target Position"));
    wxFlexGridSizer* targetPosSizer = new wxFlexGridSizer(1, 5, 5, 15);

    int spinnerWidth = StringWidth(this, "0000");
    m_pTargetDec = NewSpinnerInt(this, wxSize(spinnerWidth, -1), DefaultDec, -50, 50, 5, _("Target declination for slew, as close to Dec = 0 as possible for your location"));
    AddTableEntryPair(this, targetPosSizer, _("Declination"), m_pTargetDec);
    m_pTargetOffset = NewSpinnerInt(this, wxSize(spinnerWidth, -1), abs(DefaultHA), 5, 50, 5, _("Target offset from central meridian, in degrees; east or west based on 'Pointing' buttons"));
    AddTableEntryPair(this, targetPosSizer, _("Meridian offset (degrees)"), m_pTargetOffset);

    wxStaticBoxSizer* sizerTargetSOP = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Pointing"));
    m_pTargetWest = new wxRadioButton(this, wxID_ANY, _("West"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_pTargetWest->SetToolTip(_("Scope on the east side of pier, pointing west"));
    m_pTargetWest->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &CalCustomDialog::OnTargetWest, this);
    m_pTargetEast = new wxRadioButton(this, wxID_ANY, _("East"));
    m_pTargetEast->SetToolTip(_("Scope on the west side of pier, pointing east"));
    m_pTargetEast->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &CalCustomDialog::OnTargetEast, this);
    if (DefaultHA <= 0)
        m_pTargetEast->SetValue(true);
    else
        m_pTargetWest->SetValue(true);
    sizerTargetSOP->Add(m_pTargetWest);
    sizerTargetSOP->Add(m_pTargetEast);
    targetPosSizer->Add(sizerTargetSOP);
    tgtSizer->Add(targetPosSizer);

    m_pEastWestOnly = new wxCheckBox(this, wxID_ANY, wxEmptyString);
    m_pEastWestOnly->SetToolTip(_("Check this if calibration can only be done on a particular side of the meridian because of nearby obstructions"));
    if (m_pTargetWest->GetValue())
        m_pEastWestOnly->SetLabelText(_("Western sky only"));
    else
        m_pEastWestOnly->SetLabelText(_("Eastern sky only"));

    wxStaticText* pMessage = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, -1), wxALIGN_CENTER_HORIZONTAL);
    pMessage->SetLabelText(_("If your site location requires a unique sky position for calibration, you can specify it here."));
    pMessage->Wrap(textWrapPoint);
    MakeBold(pMessage);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* OkBtn = new wxButton(this, wxID_ANY, _("Ok"));
    OkBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalCustomDialog::OnOk, this);
    wxButton* cancelBtn = new wxButton(this, wxID_ANY, _("Cancel"));
    cancelBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalCustomDialog::OnCancel, this);
    btnSizer->Add(OkBtn, wxSizerFlags().Border(wxALL, 20));
    btnSizer->Add(cancelBtn, wxSizerFlags().Border(wxALL, 20));

    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    vSizer->Add(tgtSizer, wxSizerFlags().Center());
    vSizer->Add(m_pEastWestOnly, wxSizerFlags().Center().Border(wxTOP, 15));
    vSizer->Add(pMessage, wxSizerFlags().Center().Border(wxTOP, 15));
    vSizer->Add(btnSizer, wxSizerFlags().Center().Border(wxTOP, 15));

    SetAutoLayout(true);
    SetSizerAndFit(vSizer);
};

void CalCustomDialog::OnOk(wxCommandEvent& evt)
{
    int cDec = m_pTargetDec->GetValue();
    int cHA = m_pTargetOffset->GetValue();
    if (m_pTargetEast->GetValue())
        cHA = -cHA;
    pConfig->Profile.SetInt("/scope/CalSlew/TgtHA", cHA);
    pConfig->Profile.SetInt("/scope/CalSlew/TgtDec", cDec);
    pConfig->Profile.SetBoolean("/scope/CalSlew/SingleSide", m_pEastWestOnly->GetValue());
    m_Parent->LoadCustomPosition(cHA, cDec);

    EndDialog(wxOK);
}

void CalCustomDialog::OnCancel(wxCommandEvent& evt)
{
    EndDialog(wxCANCEL);
}
void CalCustomDialog::OnTargetWest(wxCommandEvent& evt)
{
    m_pEastWestOnly->SetLabelText(_("Western sky only"));
}
void CalCustomDialog::OnTargetEast(wxCommandEvent& evt)
{
    m_pEastWestOnly->SetLabelText(_("Eastern sky only"));
}

CalAssistSanityDialog::CalAssistSanityDialog(CalibrationAssistant* Parent, const wxString& msg)
    : wxDialog(pFrame, wxID_ANY, _("Calibration Parameters Check"),
    wxDefaultPosition, wxSize(600, -1), wxCAPTION | wxCLOSE_BOX)
{
    m_parent = Parent;
    wxStaticText* pMessage = new wxStaticText(this, wxID_ANY, msg, wxDefaultPosition, wxSize(600, -1), wxALIGN_LEFT);
    pMessage->Wrap(textWrapPoint);
    MakeBold(pMessage);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* pRecalBtn = new wxButton(this, wxID_ANY, _("Recalc"));
    pRecalBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalAssistSanityDialog::OnRecal, this);
    wxButton* cancelBtn = new wxButton(this, wxID_ANY, _("Cancel"));
    cancelBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CalAssistSanityDialog::OnCancel, this);
    btnSizer->Add(pRecalBtn, wxSizerFlags().Border(wxALL, 20));
    btnSizer->Add(cancelBtn, wxSizerFlags().Border(wxALL, 20));

    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    vSizer->Add(pMessage, wxSizerFlags().Center().Border(wxTOP, 15).Border(wxLEFT, 20));
    vSizer->Add(btnSizer, wxSizerFlags().Center().Border(wxTOP, 15));

    SetAutoLayout(true);
    SetSizerAndFit(vSizer);
}

void CalAssistSanityDialog::OnCancel(wxCommandEvent& evt)
{
    EndDialog(wxCANCEL);
}

void CalAssistSanityDialog::OnRecal(wxCommandEvent& evt)
{
    if (pPointingSource && pPointingSource->IsConnected())
    {
        double raSpd;
        double decSpd;

        if (!pPointingSource->GetGuideRates(&raSpd, &decSpd))
        {
            if (pPointingSource->ValidGuideRates(raSpd, decSpd))
            {
                double minSpd;
                if (decSpd != -1)
                    minSpd = wxMin(raSpd, decSpd);
                else
                    minSpd = raSpd;
                double sidrate = RateX(minSpd);
                int calibrationStep;
                int recDistance = CalstepDialog::GetCalibrationDistance(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(), pCamera->Binning);
                CalstepDialog::GetCalibrationStepSize(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(), pCamera->Binning, sidrate,
                    CalstepDialog::DEFAULT_STEPS, m_parent->GetCalibrationDec(), recDistance, nullptr, &calibrationStep);
                TheScope()->SetCalibrationDuration(calibrationStep);
                EndDialog(wxOK);
            }
        }
    }
}

CalAssistExplanationDialog::CalAssistExplanationDialog(const wxString& Why)
    : wxDialog(pFrame, wxID_ANY, _("Explanation of Results"),
    wxDefaultPosition, wxSize(700, -1), wxCAPTION | wxCLOSE_BOX)
{
    const int wrapPoint = 550;
    const int textHeight = 80;
    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* pHeader = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(680, textHeight), wxALIGN_LEFT);
    pHeader->SetLabelText(_("Assuming you have followed all the recommendations of the Calibration Assistant, "
        "the information below can help you identify any remaining problems."
    ));
    vSizer->Add(pHeader, wxSizerFlags().Center().Border(wxALL, 10));
    if (Why.Contains("Steps"))
    {
        wxStaticBoxSizer* stepsGrp = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Too Few Steps"));
        wxStaticText* pSteps = new wxStaticText(stepsGrp->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, textHeight), wxALIGN_LEFT);
        pSteps->SetLabelText(_(
            "Calibration was completed with fewer than the desired number of steps.  This is usually caused by "
            "changes to binning, focal length, or mount guide speed without using the new-profile-wizard. "
            "Run the wizard to restore the correct calibration parameters."
            ));
        pSteps->Wrap(wrapPoint);
        stepsGrp->Add(pSteps, wxSizerFlags().Center().Border(wxALL, 5));
        vSizer->Add(stepsGrp, wxSizerFlags().Center().Border(wxALL, 10));
    }

    if (Why.Contains("Rates"))
    {
        wxStaticBoxSizer* ratesGrp = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Unexpected Rates"));
        wxStaticText* pRates = new wxStaticText(ratesGrp->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, textHeight), wxALIGN_LEFT);
        pRates->SetLabelText(_(
            "Measured RA and Dec movements on the camera sensor aren't related by the expected ratio (cos(dec)).  This can be caused "
            "by sustantial weight imbalance in Dec or physical resistance to movement because of cables or over-tight gear mesh."
            ));
        pRates->Wrap(wrapPoint);
        ratesGrp->Add(pRates, wxSizerFlags().Center().Border(wxALL, 5));
        vSizer->Add(ratesGrp, wxSizerFlags().Center().Border(wxALL, 10));
    }

    if (Why.Contains("Orthogonality"))
    {
        wxStaticBoxSizer* orthGrp = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Orthogonality"));
        wxStaticText* pOrtho = new wxStaticText(orthGrp->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, textHeight), wxALIGN_LEFT);
        pOrtho->SetLabelText(_(
            "The mount is wandering off-target on one axis while PHD2 is measuring movement on the other axis. "
            "This can be caused by large periodic error in RA or polar alignment > 10 arc-min. If the orthogonality "
            "error is very large, the mount is probably not guiding correctly."
            ));
        pOrtho->Wrap(wrapPoint);
        orthGrp->Add(pOrtho, wxSizerFlags().Center().Border(wxALL, 5));
        vSizer->Add(orthGrp, wxSizerFlags().Center().Border(wxALL, 10));
    }

    if (Why.Contains("Sky location"))
    {
        wxStaticBoxSizer* locationGrp = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Sky Location"));
        wxStaticText* pLocation = new wxStaticText(locationGrp->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, textHeight), wxALIGN_LEFT);
        pLocation->SetLabelText(_(
            "Calibration wasn't done with Dec in the range of -20 to +20.  Outside that range, measurement errors may degrade the calibration accuracy. "
            "If Dec is outside the range of -60 to +60, declination compensation will not work correctly."
            ));
        pLocation->Wrap(wrapPoint);
        locationGrp->Add(pLocation, wxSizerFlags().Center().Border(wxALL, 5));
        vSizer->Add(locationGrp, wxSizerFlags().Center().Border(wxALL, 10));
    }

    if (Why.Contains("Incomplete"))
    {
        wxStaticBoxSizer* failGrp = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Not Enough Movement"));
        wxStaticText* pFail = new wxStaticText(failGrp->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, textHeight), wxALIGN_LEFT);
        pFail->SetLabelText(_(
            "If you saw an alert saying the guide star did not move enough, the mount may not be receiving or handling guide commands. "
            "If you are using an ST-4 guide cable, try replacing it. Otherwise, use the Star-Cross and Manual Guide tools in PHD2 to help "
            "isolate the mechanical problem."
            ));
        pFail->Wrap(wrapPoint);
        failGrp->Add(pFail, wxSizerFlags().Center().Border(wxALL, 5));
        vSizer->Add(failGrp, wxSizerFlags().Center().Border(wxALL, 5));
    }

    wxButton* okBtn = new wxButton(this, wxID_OK, _("Ok"));
    vSizer->Add(okBtn, wxSizerFlags().Center().Border(wxALL, 20));

    SetAutoLayout(true);
    SetSizerAndFit(vSizer);
}

wxDialog *CalibrationAssistantFactory::MakeCalibrationAssistant()
{
    return new CalibrationAssistant();
}

#undef RateX
