/*
 *  starcross_test.cpp
 *  PHD Guiding
 *
 *  Created by Bruce Waddington
 *  Copyright (c) 2016 Bruce Waddington
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
 #include "starcross_test.h"

wxBEGIN_EVENT_TABLE(StarCrossDialog, wxDialog)

EVT_CLOSE(StarCrossDialog::OnCloseWindow)

wxEND_EVENT_TABLE()

#define SCT_DEFAULT_PULSE_SIZE 1000
#define SCT_DEFAULT_PULSE_COUNT 25
#define SCT_DEFAULT_GUIDESPEED 0.5

// Utility function to add the <label, input> pairs to a flexgrid
static void AddTableEntryPair(wxWindow *parent, wxFlexGridSizer *pTable, const wxString& label, wxWindow *pControl)
{
    wxStaticText *pLabel = new wxStaticText(parent, wxID_ANY, label + _(": "), wxPoint(-1, -1), wxSize(-1, -1));
    pTable->Add(pLabel, 1, wxALL, 5);
    pTable->Add(pControl, 1, wxALL, 5);
}

static wxSpinCtrlDouble *NewSpinner(wxWindow *parent, int width, double val, double minval, double maxval, double inc, unsigned int decimals,
                                 const wxString& tooltip)
{
    wxSpinCtrlDouble *pNewCtrl = pFrame->MakeSpinCtrlDouble(parent, wxID_ANY, _T("foo2"), wxPoint(-1, -1),
        wxSize(width, -1), wxSP_ARROW_KEYS, minval, maxval, val, inc);
    pNewCtrl->SetValue(val);
    pNewCtrl->SetToolTip(tooltip);
    pNewCtrl->SetDigits(decimals);
    return pNewCtrl;
}

static void MakeBold(wxControl *ctrl)
{
    wxFont font = ctrl->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    ctrl->SetFont(font);
}

StarCrossDialog::StarCrossDialog(wxWindow *parent) :
    wxDialog(parent, wxID_ANY, _("Star-Cross Test"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* mountSpecSizer = new wxFlexGridSizer(2, 6, 5, 15);
    wxFlexGridSizer* testSpecSizer = new wxFlexGridSizer(1, 5, 5, 15);
    wxFlexGridSizer* testSummarySizer = new wxFlexGridSizer(1, 5, 5, 15);
    double guideSpeedDec = 0.0;
    double guideSpeedRA = 0.0;
    double guideSpeedMultiple;
    const double siderealSecondPerSec = 0.9973;
    int width = StringWidth(this, _("88888"));
    bool knownGuideSpeed = false;

    // Populate top flex grid with parameters relating to image scale and mount properties - these are needed
    // for future use of the guide camera instead of the main camera
    // Focal length
    wxStaticBoxSizer* configGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Configuration"));

    // Guide speed - use best available info, either from ASCOM if available or from whatever the user entered in the new-profile-wizard
    guideSpeedMultiple = pConfig->Profile.GetDouble ("/CalStepCalc/GuideSpeed", SCT_DEFAULT_GUIDESPEED);
    if (pPointingSource && pPointingSource->CanReportPosition())
    {
        if (!pPointingSource->GetGuideRates(&guideSpeedRA, &guideSpeedDec))
        {
            if (guideSpeedRA >= guideSpeedDec)
                guideSpeedMultiple = guideSpeedRA * 3600.0 / (15.0 * siderealSecondPerSec);  // Degrees/sec to Degrees/hour, 15 degrees/hour is roughly sidereal rate
            else
                guideSpeedMultiple = guideSpeedDec * 3600.0 / (15.0 * siderealSecondPerSec);
            knownGuideSpeed = true;

        }
    }
    m_CtlGuideSpeed = NewSpinner(this, width, guideSpeedMultiple, 0.1, 1.0, 0.1, 2,
        /* xgettext:no-c-format */ _("Guide speed, multiple of sidereal rate; if your mount's guide speed is 50% sidereal rate, enter 0.5"));
    m_CtlGuideSpeed->Bind(wxEVT_SPINCTRLDOUBLE, &StarCrossDialog::OnGuideSpeedChange, this);
    AddTableEntryPair(this, mountSpecSizer, _("Guide speed, n.n x sidereal"), m_CtlGuideSpeed);
    configGroup->Add(mountSpecSizer, wxSizerFlags().Border(wxALL, 5));

    // Add the controls for doing the test
    // Test summary - leg duration, details button
    wxStaticBoxSizer* summaryGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Test Summary"));
    int pulseCount = pConfig->Profile.GetDouble("/SCT/PulseCount", SCT_DEFAULT_PULSE_COUNT);
    int pulseSize = pConfig->Profile.GetDouble("/SCT/PulseSize", SCT_DEFAULT_PULSE_SIZE);
    int leg = pulseCount * pulseSize/1000.;
    m_CtlLegDuration = NewSpinner(this, width, leg, 3, 40, 1, 0,
        _("Total guide pulse duration in EACH of 4 directions"));
    m_CtlLegDuration->Bind(wxEVT_SPINCTRLDOUBLE, &StarCrossDialog::OnLegDurationChange, this);
    AddTableEntryPair(this, testSummarySizer, _("Total guide duration, \nEACH direction (s)"),
        m_CtlLegDuration);
    m_CtlTotalDuration = NewSpinner(this, width, 8 * leg, 24, 600, 1, 0,
        _("Total duration of test (s)"));
    AddTableEntryPair(this, testSummarySizer, _("Total test duration (s)"), m_CtlTotalDuration);
    m_CtlTotalDuration->Enable(false);
    m_ViewControlBtn = new wxButton(this, wxID_ANY, _("Show Details"));
    m_ViewControlBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &StarCrossDialog::OnViewControl, this);
    testSummarySizer->Add(m_ViewControlBtn, wxSizerFlags(0).Border(wxALL, 5));
    summaryGroup->Add(testSummarySizer, 1, wxALL, 5);

    // Test details - pulse size, #pulses
    m_DetailsGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Test Details"));
    m_CtlNumPulses = NewSpinner(this, width,
        pulseCount, 5, 40, 1, 0, _("Number of guide pulses in EACH direction"));     // Default of 20 pulses
    m_CtlNumPulses->Bind(wxEVT_SPINCTRLDOUBLE, &StarCrossDialog::OnPulseCountChange, this);

    AddTableEntryPair(this, testSpecSizer, _("Number of guide pulses"), m_CtlNumPulses);
    // Pulse size
    m_CtlPulseSize = NewSpinner(this, width,
        pulseSize, 500, 5000, 50, 0, _("Guide pulse size (ms)"));       // Default of 1-sec pulse
    m_CtlPulseSize->Bind(wxEVT_SPINCTRLDOUBLE, &StarCrossDialog::OnPulseSizeChange, this);

    AddTableEntryPair(this, testSpecSizer, _("Pulse size (ms)"), m_CtlPulseSize);
    // Suggestion button
    wxButton* resetBtn = new wxButton(this, wxID_ANY, _("Reset"));
    resetBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &StarCrossDialog::OnSuggest, this);
    testSpecSizer->Add(resetBtn, 1, wxALL, 5);
    m_DetailsGroup->Add(testSpecSizer, wxSizerFlags().Border(wxTOP | wxBOTTOM | wxRIGHT, 5).Border(wxLEFT, 20));

    // Put an explanation block and progress bar right above the buttons
    m_Explanations = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(500,-1), wxALIGN_CENTER);
    m_Explanations->SetLabel(_("Verify or adjust your parameters, click 'Start' to begin"));
    MakeBold(m_Explanations);
    m_Progress = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(500, -1));

   // Start/stop buttons
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_StartBtn = new wxButton(this, wxID_ANY, _("Start"));
    m_StartBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &StarCrossDialog::OnStart, this);
    m_StopBtn = new wxButton(this, wxID_ANY, _("Stop"));
    m_StopBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &StarCrossDialog::OnCancel, this);
    m_StopBtn->Enable(false);
    btnSizer->Add(
        m_StartBtn,
        wxSizerFlags(0).Align(0).Border(wxALL, 10));
    btnSizer->Add(
        m_StopBtn,
        wxSizerFlags(0).Align(0).Border(wxALL, 10));

    // Stack up the UI elements in the vertical sizer
    m_ShowDetails = false;
    m_DetailsGroup->Show(false);
    configGroup->Show(!knownGuideSpeed);
    vSizer->Add(configGroup, wxSizerFlags().Border(wxALL, 5));
    vSizer->Add(summaryGroup, wxSizerFlags().Border(wxALL, 5));
    vSizer->Add(m_DetailsGroup, wxSizerFlags().Border(wxALL, 5));
    vSizer->Add(m_Explanations, wxSizerFlags().Center().Border(wxALL, 15));
    vSizer->Add(m_Progress, wxSizerFlags().Center());
    vSizer->Add(btnSizer, wxSizerFlags().Center().Border(wxALL, 10));

    SetSizerAndFit(vSizer);
    if (!pConfig->Profile.HasEntry("/SCT/PulseCount"))
        SuggestParams();        // Offer suggestions unless user has already completed a test
}

StarCrossDialog::~StarCrossDialog()
{
    // Null the parent pointer to us
    pFrame->pStarCrossDlg = NULL;
}

void StarCrossDialog::SuggestParams()
{
    double guideSpeed;
    // Want to handle mounts with 25 sec of backlash at guide speed of 0.5x
    guideSpeed = m_CtlGuideSpeed->GetValue();
    m_CtlNumPulses->SetValue((int)ceil((24.0 * 0.5 / guideSpeed)));
    m_CtlPulseSize->SetValue(1000);
    SynchSummarySliders();
}

SCT_StepInfo StarCrossDialog::NextStep(const SCT_StepInfo& prevStep)
{
    SCT_StepInfo nextStep;

    switch (prevStep.state)
    {
    case SCT_STATE_NONE:
        nextStep.state = SCT_STATE_WEST;
        nextStep.direction = WEST;
        nextStep.pulseCount = m_DirectionalPulseCount;
        break;
    case SCT_STATE_WEST:
        nextStep.state = SCT_STATE_EAST;
        nextStep.direction = EAST;
        nextStep.pulseCount = 2 * m_DirectionalPulseCount;
        break;
    case SCT_STATE_EAST:
        nextStep.state = SCT_STATE_WEST_RETURN;
        nextStep.direction = WEST;
        nextStep.pulseCount = m_DirectionalPulseCount;
        break;
    case SCT_STATE_WEST_RETURN:
        nextStep.state = SCT_STATE_NORTH;
        nextStep.direction = NORTH;
        nextStep.pulseCount = m_DirectionalPulseCount;
        break;
    case SCT_STATE_NORTH:
        nextStep.state = SCT_STATE_SOUTH;
        nextStep.direction = SOUTH;
        nextStep.pulseCount = 2 * m_DirectionalPulseCount;
        break;
    case SCT_STATE_SOUTH:
        nextStep.state = SCT_STATE_NORTH_RETURN;
        nextStep.direction = NORTH;
        nextStep.pulseCount = m_DirectionalPulseCount;
        break;
    case SCT_STATE_NORTH_RETURN:
    case SCT_STATE_DONE:
        nextStep.state = SCT_STATE_DONE;
        nextStep.direction = NONE;
        nextStep.pulseCount = 0;
        break;
    }
    return nextStep;
}

wxString StarCrossDialog::Explanation(const SCT_StepInfo& currStep, int dirCount)
{
    wxString rslt;
    rslt = wxString::Format(_("%d ms move "), m_Amount);
    switch (currStep.state)
    {
    case SCT_STATE_NONE:
        rslt = wxEmptyString;
        break;
    case SCT_STATE_WEST:
        rslt += _("WEST");
        break;
    case SCT_STATE_EAST:
        rslt += _("EAST");
        break;
    case SCT_STATE_WEST_RETURN:
        rslt += _("WEST toward starting point");
        break;
    case SCT_STATE_NORTH:
        rslt += _("NORTH");
        break;
    case SCT_STATE_SOUTH:
        rslt += _("SOUTH");
        break;
    case SCT_STATE_NORTH_RETURN:
        rslt += _("NORTH toward starting point");
        break;
    case SCT_STATE_DONE:
        rslt = _("Test completed");
        break;
    }
    if (currStep.state != SCT_STATE_DONE)
        rslt += wxString::Format(_(", step %d of %d"), dirCount, currStep.pulseCount);
    return rslt;
}

void StarCrossDialog::ExecuteTest()
{
    bool done = false;
    int count = 0;
    int dirCount = 0;
    int totalPulses;
    bool errorCaught = false;
    Mount::MOVE_RESULT moveRslt;
    wxString dirString;
    wxString logMsg;
    SCT_StepInfo currStep = { 0, NONE, SCT_STATE_NONE };
    Mount* theMount;

    m_Amount = m_CtlPulseSize->GetValue();   // Don't allow changes until done
    m_DirectionalPulseCount = m_CtlNumPulses->GetValue();
    totalPulses = 8 * m_DirectionalPulseCount;
    m_StartBtn->Enable(false);
    m_StopBtn->Enable(true);
    m_Progress->SetRange(totalPulses);
    m_Progress->SetValue(0);

    theMount = (pMount && pMount->IsStepGuider() ? pSecondaryMount : pMount);
    // Make sure looping is active so user can see something happening
    // This will also cleanly stop guiding if it's active
    if (pCamera && pCamera->Connected)
    {
        pFrame->StartLoopingInteractive(_T("PolarDrift:execute"));
    }
    m_CancelTest = false;
    // Leave plenty of room for camera exposure and mount response overhead
    wxMessageBox(wxString::Format(_("Start a %d exposure on your main camera, then click 'Ok'"),
        (2 * totalPulses * m_Amount) / 1000));
    while (!done && !m_CancelTest)
    {
        if (dirCount == currStep.pulseCount)
        {
            currStep = NextStep(currStep);
            dirCount = 0;
            dirString = (wxString) pMount->DirectionStr(currStep.direction);
            if (currStep.state == SCT_STATE_DONE)
            {
                Debug.Write("Star-cross test completed\n");
                m_Explanations->SetLabel(Explanation(currStep, dirCount));
                wxMessageBox(
                    _("Wait for the main camera exposure to complete, then save that image for review")
                    );
                pConfig->Profile.SetDouble("/SCT/PulseCount", m_DirectionalPulseCount);
                pConfig->Profile.SetDouble("/SCT/PulseSize", m_Amount);
                done = true;
            }

        }
        if (pMount && pMount->IsConnected())
        {
            if (!done)
            {
                count++;
                dirCount++;
                logMsg = wxString::Format("Star-cross move %d/%d, %s for %d ms", count, totalPulses, dirString, m_Amount );
                Debug.Write(logMsg + "\n");
                m_Explanations->SetLabel(Explanation(currStep, dirCount));
                moveRslt = theMount->MoveAxis(currStep.direction, m_Amount, MOVEOPTS_CALIBRATION_MOVE);
                m_Progress->SetValue(count);
                wxYield();
                if (moveRslt != Mount::MOVE_OK)
                {
                    Debug.Write("Star-cross move failed, test cancelled\n");
                    m_Explanations->SetLabel(_("Star-cross moved failed, test cancelled"));
                    m_CancelTest = true;
                    errorCaught = true;
                }
            }
        }
        else
        {
            Debug.Write("Star-cross error, mount connection lost, test cancelled\n");
            m_Explanations->SetLabel(_("Mount connection lost, test cancelled"));
            m_CancelTest = true;
            errorCaught = true;
        }
    }
    if (m_CancelTest)
    {
        if (errorCaught)
            Debug.Write("Star-cross test cancelled because of exception\n");
        else
        {
            m_Explanations->SetLabel(_("Test cancelled"));
            Debug.Write("Star-cross test cancelled by user\n");
        }
    }
    m_StartBtn->Enable(true);
    m_StopBtn->Enable(false);

}

// Make sure we get unloaded when user is done, start-up becomes deterministic
void StarCrossDialog::OnCloseWindow(wxCloseEvent& event)
{
    wxDialog::Destroy();
    event.Skip();
}

void StarCrossDialog::OnGuideSpeedChange(wxSpinDoubleEvent& evt)
{
    SuggestParams();
}

void StarCrossDialog::SynchSummarySliders()
{
    int val = (int) ceil(m_CtlNumPulses->GetValue() * m_CtlPulseSize->GetValue() / 1000.);
    m_CtlLegDuration->SetValue(val);
    m_CtlTotalDuration->SetValue(8 * val);
}

void StarCrossDialog::SynchDetailSliders()
{
    int leg = m_CtlLegDuration->GetValue();
    m_CtlNumPulses->SetValue((int) ceil(leg * 1000. / m_CtlPulseSize->GetValue()));

}

// Note: these events are fired only via user actions
void StarCrossDialog::OnLegDurationChange(wxSpinDoubleEvent& evt)
{
    m_CtlTotalDuration->SetValue((int) ceil(8 * m_CtlLegDuration->GetValue()));
    SynchDetailSliders();
}

void StarCrossDialog::OnPulseCountChange(wxSpinDoubleEvent& evt)
{
    SynchSummarySliders();
}

void StarCrossDialog::OnPulseSizeChange(wxSpinDoubleEvent& evt)
{
    SynchSummarySliders();
}

void StarCrossDialog::OnStart(wxCommandEvent& evt)
{
    if (pMount && pMount->IsConnected())
    {
        ExecuteTest();
    }
    else
        wxMessageBox(_("Mount connection must be restored"));
}

void StarCrossDialog::OnCancel(wxCommandEvent& evt)
{
    m_CancelTest = true;
    m_Progress->SetValue(0);
}

void StarCrossDialog::OnSuggest(wxCommandEvent& evt)
{
    SuggestParams();
}

void StarCrossDialog::OnViewControl(wxCommandEvent& evt)
{
    m_ShowDetails = !m_ShowDetails;
    if (m_ShowDetails)
    {
        m_DetailsGroup->Show(true);
        m_CtlLegDuration->Enable(false);
        m_ViewControlBtn->SetLabel(_("Hide Details"));
    }
    else
    {
        m_DetailsGroup->Show(false);
        m_CtlLegDuration->Enable(true);
        m_ViewControlBtn->SetLabel(_("Show Details"));
    }
    Layout();
    GetSizer()->Fit(this);
}

