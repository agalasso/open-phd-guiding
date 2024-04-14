/*
*  guiding_assistant.cpp
*  PHD Guiding
*
*  Created by Andy Galasso and Bruce Waddington
*  Copyright (c) 2015 Andy Galasso and Bruce Waddington
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
#include "guiding_assistant.h"
#include "backlash_comp.h"
#include "guiding_stats.h"
#include "optionsbutton.h"

#include <wx/textwrapper.h>
#include <wx/tokenzr.h>

struct GADetails
{
    wxString TimeStamp;
    wxString SNR;
    wxString StarMass;
    wxString SampleCount;
    wxString ElapsedTime;
    wxString ExposureTime;
    wxString RA_HPF_RMS;
    wxString Dec_HPF_RMS;
    wxString Total_HPF_RMS;
    wxString RAPeak;
    wxString RAPeak_Peak;
    wxString RADriftRate;
    wxString RAMaxDriftRate;
    wxString DriftLimitingExposure;
    wxString DecDriftRate;
    wxString DecPeak;
    wxString PAError;
    wxString BackLashInfo;
    wxString Dec_LF_DriftRate;
    wxString DecCorrectedRMS;
    wxString RecRAMinMove;
    wxString RecDecMinMove;
    std::vector<double> BLTNorthMoves;
    std::vector<double> BLTSouthMoves;
    int BLTMsmtPulse;
    wxString BLTAmount;
    wxString Recommendations;
};

inline static void StartRow(int& row, int& column)
{
    ++row;
    column = 0;
}

static void MakeBold(wxControl *ctrl)
{
    wxFont font = ctrl->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    ctrl->SetFont(font);
}

// Dialog for making sure sampling period is adequate for decent measurements
struct SampleWait : public wxDialog
{
    wxStaticText *m_CountdownAmount;
    wxTimer m_SecondsTimer;
    int m_SecondsLeft;

    SampleWait(int SamplePeriod, bool BltNeeded);
    void OnTimer(wxTimerEvent& evt);
    void OnCancel(wxCommandEvent& event);
};

SampleWait::SampleWait(int SecondsLeft, bool BltNeeded) : wxDialog(pFrame, wxID_ANY, _("Extended Sampling"))
{
    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* amtSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* explanation = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    wxString msg;
    if (BltNeeded)
        msg = _("Additional data sampling is being done to better meaure Dec drift. Backlash testing \nwill start automatically when sampling is completed.");
    else
        msg = _("Additional sampling is being done for accurate measurements.  Results will be shown when sampling is complete.");
    explanation->SetLabelText(msg);
    MakeBold(explanation);
    wxStaticText* countDownLabel = new wxStaticText(this, wxID_ANY, _("Seconds remaining: "), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_SecondsLeft = SecondsLeft;
    m_CountdownAmount = new wxStaticText(this, wxID_ANY, std::to_string(wxMax(0, m_SecondsLeft)));
    amtSizer->Add(countDownLabel, wxSizerFlags(0).Border(wxALL, 8));
    amtSizer->Add(m_CountdownAmount, wxSizerFlags(0).Border(wxALL, 8));
    wxButton* cancelBtn = new wxButton(this, wxID_ANY, _("Cancel"));
    cancelBtn->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SampleWait::OnCancel), nullptr, this);
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(cancelBtn, wxSizerFlags(0).Border(wxALL, 8).Center());

    vSizer->Add(explanation, wxSizerFlags(0).Border(wxALL, 8).Center());
    vSizer->Add(amtSizer, wxSizerFlags(0).Border(wxALL, 8).Center());
    vSizer->Add(btnSizer, wxSizerFlags(0).Border(wxALL, 8).Center());

    SetAutoLayout(true);
    SetSizerAndFit(vSizer);

    m_SecondsTimer.Connect(wxEVT_TIMER, wxTimerEventHandler(SampleWait::OnTimer), nullptr, this);
    m_SecondsTimer.Start(1000);
}

void SampleWait::OnTimer(wxTimerEvent& evt)
{
    m_SecondsLeft -= 1;
    if (m_SecondsLeft > 0)
    {
        m_CountdownAmount->SetLabelText(std::to_string(m_SecondsLeft));
        m_CountdownAmount->Update();
    }
    else
    {
        m_SecondsTimer.Stop();
        EndDialog(wxOK);
    }
}

void SampleWait::OnCancel(wxCommandEvent& event)
{
    m_SecondsTimer.Stop();
    if (wxGetKeyState(WXK_CONTROL))
        EndDialog(wxOK);
    else
        EndDialog(wxCANCEL);
}

// Encapsulated struct for implementing the dialog box
struct GuidingAsstWin : public wxDialog
{
    enum DialogState
    {
        STATE_NO_STAR = 0,
        STATE_START_READY = 1,
        STATE_MEASURING = 2,
        STATE_STOPPED = 3
    };
    enum DlgConstants {MAX_BACKLASH_COMP = 3000, GA_MIN_SAMPLING_PERIOD = 120};

    wxButton *m_start;
    wxButton *m_stop;
    OptionsButton* btnReviewPrev;
    wxTextCtrl *m_report;
    wxStaticText *m_instructions;
    wxGrid *m_statusgrid;
    wxGrid *m_displacementgrid;
    wxGrid *m_othergrid;
    wxFlexGridSizer *m_recommendgrid;
    wxBoxSizer *m_vSizer;
    wxStaticBoxSizer *m_recommend_group;
    wxCheckBox *m_backlashCB;
    wxStaticText *m_gaStatus;
    wxButton *m_graphBtn;

    wxGridCellCoords m_timestamp_loc;
    wxGridCellCoords m_starmass_loc;
    wxGridCellCoords m_samplecount_loc;
    wxGridCellCoords m_snr_loc;
    wxGridCellCoords m_elapsedtime_loc;
    wxGridCellCoords m_exposuretime_loc;
    wxGridCellCoords m_hfcutoff_loc;
    wxGridCellCoords m_ra_rms_loc;
    wxGridCellCoords m_dec_rms_loc;
    wxGridCellCoords m_total_rms_loc;
    wxGridCellCoords m_ra_peak_loc;
    wxGridCellCoords m_dec_peak_loc;
    wxGridCellCoords m_ra_peakpeak_loc;
    wxGridCellCoords m_ra_drift_loc;
    wxGridCellCoords m_ra_drift_exp_loc;
    wxGridCellCoords m_dec_drift_loc;
    wxGridCellCoords m_pae_loc;
    wxGridCellCoords m_ra_peak_drift_loc;
    wxGridCellCoords m_backlash_loc;
    wxButton *m_raMinMoveButton;
    wxButton *m_decMinMoveButton;
    wxButton *m_decBacklashButton;
    wxButton *m_decAlgoButton;
    wxStaticText *m_ra_msg;
    wxStaticText *m_dec_msg;
    wxStaticText *m_snr_msg;
    wxStaticText *m_pae_msg;
    wxStaticText *m_hfd_msg;
    wxStaticText *m_backlash_msg;
    wxStaticText *m_exposure_msg;
    wxStaticText *m_calibration_msg;
    wxStaticText *m_binning_msg;
    wxStaticText *m_decAlgo_msg;
    double m_ra_minmove_rec;  // recommended value
    double m_dec_minmove_rec; // recommended value
    double m_min_exp_rec;
    double m_max_exp_rec;

    DialogState m_dlgState;
    bool m_measuring;
    wxLongLong_t m_startTime;
    long m_elapsedSecs;
    PHD_Point m_startPos;
    wxString startStr;
    DescriptiveStats m_hpfRAStats;
    DescriptiveStats m_lpfRAStats;
    DescriptiveStats m_hpfDecStats;
    AxisStats m_decAxisStats;
    AxisStats m_raAxisStats;
    long m_axisTimebase;
    HighPassFilter m_raHPF;
    LowPassFilter m_raLPF;
    HighPassFilter m_decHPF;
    double sumSNR;
    double sumMass;
    double m_lastTime;
    double maxRateRA;               // arc-sec per second
    double decDriftPerMin;          // px per minute
    double decCorrectedRMS;         // RMS of drift-corrected Dec dataset
    double alignmentError;          // arc-minutes
    double m_backlashPx;
    int m_backlashMs;
    double m_backlashSigmaMs;
    int m_backlashRecommendedMs;

    bool m_guideOutputDisabled;
    bool m_savePrimaryMountEnabled;
    bool m_saveSecondaryMountEnabled;
    bool m_measurementsTaken;
    int  m_origSubFrames;
    bool m_suspectCalibration;
    bool inBLTWrapUp = false;
    bool origMultistarMode;
    VarDelayCfg origVarDelayConfig;

    bool m_measuringBacklash;
    BacklashTool *m_backlashTool;
    bool reviewMode;
    GADetails gaDetails;
    bool m_flushConfig;

    GuidingAsstWin();
    ~GuidingAsstWin();

    void OnClose(wxCloseEvent& event);
    void OnMouseMove(wxMouseEvent&);
    void OnAppStateNotify(wxCommandEvent& event);
    void OnStart(wxCommandEvent& event);
    void DoStop(const wxString& status = wxEmptyString);
    void OnStop(wxCommandEvent& event);
    void OnRAMinMove(wxCommandEvent& event);
    void OnDecMinMove(wxCommandEvent& event);
    void OnDecBacklash(wxCommandEvent& event);
    void OnDecAlgoChange(wxCommandEvent& event);
    void OnGraph(wxCommandEvent& event);
    void OnHelp(wxCommandEvent& event);
    void OnReviewPrevious(wxCommandEvent& event);
    void OnGAReviewSelection(wxCommandEvent& evt);

    wxStaticText *AddRecommendationBtn(const wxString& msg,
                                       void (GuidingAsstWin::* handler)(wxCommandEvent&),
                                       wxButton **ppButton);
    wxStaticText *AddRecommendationMsg(const wxString& msg);
    void FillResultCell(wxGrid *pGrid, const wxGridCellCoords& loc, double pxVal,
                        double asVal, const wxString& units1, const wxString& units2,
                        const wxString& extraInfo = wxEmptyString);
    void UpdateInfo(const GuideStepInfo& info);
    void DisplayStaticResults(const GADetails& details);
    void FillInstructions(DialogState eState);
    void MakeRecommendations();
    void DisplayStaticRecommendations(const GADetails& details);
    void LogResults();
    void BacklashStep(const PHD_Point& camLoc);
    void EndBacklashTest(bool completed);
    void BacklashError();
    void StatsReset();
    void LoadGAResults(const wxString& TimeStamp, GADetails* Details);
    void SaveGAResults(const wxString* AllRecommendations);
    int GetGAHistoryCount();
    void GetMinMoveRecs(double& RecRA, double& RecDec);
    bool LikelyBacklash(const CalibrationDetails& calDetails);
    const int MAX_GA_HISTORY = 3;
};

static void HighlightCell(wxGrid *pGrid, wxGridCellCoords where)
{
    pGrid->SetCellBackgroundColour(where.GetRow(), where.GetCol(), "DARK SLATE GREY");
    pGrid->SetCellTextColour(where.GetRow(), where.GetCol(), "white");
}

struct GridTooltipInfo : public wxObject
{
    wxGrid *grid;
    int gridNum;
    wxGridCellCoords prevCoords;
    GridTooltipInfo(wxGrid *g, int i) : grid(g), gridNum(i) { }
};

struct TextWrapper
{
    wxWindow *win;
    int width;
    TextWrapper(wxWindow *win_, int width_) : win(win_), width(width_) { }
    wxString Wrap(const wxString& text) const
    {
        struct Wrapper : public wxTextWrapper
        {
            wxString str;
            Wrapper(wxWindow *win_, const wxString& text, int width_) { Wrap(win_, text, width_); }
            void OnOutputLine(const wxString& line) { str += line; }
            void OnNewLine() { str += '\n'; }
        };
        return Wrapper(win, text, width).str;
    }
};

// Constructor
GuidingAsstWin::GuidingAsstWin()
    : wxDialog(pFrame, wxID_ANY, wxGetTranslation(_("Guiding Assistant"))),
      m_measuring(false),
      m_guideOutputDisabled(false),
      m_measurementsTaken(false),
      m_origSubFrames(-1),
      m_backlashTool(nullptr),
      m_flushConfig(false)
{
    // Sizer hierarchy:
    // m_vSizer has {instructions, vResultsSizer, m_gaStatus, btnSizer}
    // vResultsSizer has {hTopSizer, hBottomSizer}
    // hTopSizer has {status_group, displacement_group}
    // hBottomSizer has {other_group, m_recommendation_group}
    m_vSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vResultsSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* hTopSizer = new wxBoxSizer(wxHORIZONTAL);       // Measurement status and high-frequency results
    wxBoxSizer* hBottomSizer = new wxBoxSizer(wxHORIZONTAL);             // Low-frequency results and recommendations

    m_instructions = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(700, 50), wxALIGN_LEFT | wxST_NO_AUTORESIZE);
    MakeBold(m_instructions);
    m_vSizer->Add(m_instructions, wxSizerFlags(0).Border(wxALL, 8));

    // Grids have either 3 or 4 columns, so compute width of largest label as scaling term for column widths
    double minLeftCol = StringWidth(this,
        _(" -999.99 px/min (-999.99 arc-sec/min )")) + 6;
    double minRightCol = 1.25 * (StringWidth(this,
        _(" 9.99 px ( 9.99 arc-sec)")) + 6);
    // Start of status group
    wxStaticBoxSizer *status_group = new wxStaticBoxSizer(wxVERTICAL, this, _("Measurement Status"));
    m_statusgrid = new wxGrid(this, wxID_ANY);
    m_statusgrid->CreateGrid(3, 4);
    m_statusgrid->GetGridWindow()->Bind(wxEVT_MOTION, &GuidingAsstWin::OnMouseMove, this, wxID_ANY, wxID_ANY, new GridTooltipInfo(m_statusgrid, 1));
    m_statusgrid->SetRowLabelSize(1);
    m_statusgrid->SetColLabelSize(1);
    m_statusgrid->EnableEditing(false);
    m_statusgrid->SetDefaultColSize((round(2.0 * minLeftCol / 4.0) + 0.5));

    int col = 0;
    int row = 0;
    m_statusgrid->SetCellValue(row, col++, _("Start time"));
    m_timestamp_loc.Set(row, col++);
    m_statusgrid->SetCellValue(row, col++, _("Exposure time"));
    m_exposuretime_loc.Set(row, col++);

    StartRow(row, col);
    m_statusgrid->SetCellValue(row, col++, _("SNR"));
    m_snr_loc.Set(row, col++);
    m_statusgrid->SetCellValue(row, col++, _("Star mass"));
    m_starmass_loc.Set(row, col++);

    StartRow(row, col);
    m_statusgrid->SetCellValue(row, col++, _("Elapsed time"));
    m_elapsedtime_loc.Set(row, col++);
    m_statusgrid->SetCellValue(row, col++, _("Sample count"));
    m_samplecount_loc.Set(row, col++);

    //StartRow(row, col);
    //m_statusgrid->SetCellValue(_("Frequency cut-off:"), row, col++);   // Leave out for now, probably not useful to users
    //m_hfcutoff_loc.Set(row, col++);

    status_group->Add(m_statusgrid);
    hTopSizer->Add(status_group, wxSizerFlags(0).Border(wxALL, 8));
    // End of status group

    // Start of star displacement group
    wxStaticBoxSizer *displacement_group = new wxStaticBoxSizer(wxVERTICAL, this, _("High-frequency Star Motion"));
    m_displacementgrid = new wxGrid(this, wxID_ANY);
    m_displacementgrid->CreateGrid(3, 2);
    m_displacementgrid->GetGridWindow()->Bind(wxEVT_MOTION, &GuidingAsstWin::OnMouseMove, this, wxID_ANY, wxID_ANY, new GridTooltipInfo(m_displacementgrid, 2));
    m_displacementgrid->SetRowLabelSize(1);
    m_displacementgrid->SetColLabelSize(1);
    m_displacementgrid->EnableEditing(false);
    m_displacementgrid->SetDefaultColSize(minRightCol);

    row = 0;
    col = 0;
    m_displacementgrid->SetCellValue(row, col++, _("Right ascension, RMS"));
    m_ra_rms_loc.Set(row, col++);

    StartRow(row, col);
    m_displacementgrid->SetCellValue(row, col++, _("Declination, RMS"));
    m_dec_rms_loc.Set(row, col++);

    StartRow(row, col);
    m_displacementgrid->SetCellValue(row, col++, _("Total, RMS"));
    m_total_rms_loc.Set(row, col++);

    displacement_group->Add(m_displacementgrid);
    hTopSizer->Add(displacement_group, wxSizerFlags(0).Border(wxALL, 8));
    vResultsSizer->Add(hTopSizer);
    // End of displacement group

    // Start of "Other" (peak and drift) group
    wxStaticBoxSizer *other_group = new wxStaticBoxSizer(wxVERTICAL, this, _("Other Star Motion"));
    m_othergrid = new wxGrid(this, wxID_ANY);
    m_othergrid->CreateGrid(9, 2);
    m_othergrid->GetGridWindow()->Bind(wxEVT_MOTION, &GuidingAsstWin::OnMouseMove, this, wxID_ANY, wxID_ANY, new GridTooltipInfo(m_othergrid, 3));
    m_othergrid->SetRowLabelSize(1);
    m_othergrid->SetColLabelSize(1);
    m_othergrid->EnableEditing(false);
    m_othergrid->SetDefaultColSize(minLeftCol);

    TextWrapper w(this, minLeftCol);

    row = 0;
    col = 0;
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Right ascension, Peak")));
    m_ra_peak_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Declination, Peak")));
    m_dec_peak_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Right ascension, Peak-Peak")));
    m_ra_peakpeak_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Right ascension Drift Rate")));
    m_ra_drift_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Right ascension Max Drift Rate")));
    m_ra_peak_drift_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Drift-limiting exposure")));
    m_ra_drift_exp_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Declination Drift Rate")));
    m_dec_drift_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Declination Backlash")));
    m_backlash_loc.Set(row, col++);

    StartRow(row, col);
    m_othergrid->SetCellValue(row, col++, w.Wrap(_("Polar Alignment Error")));
    m_pae_loc.Set(row, col++);

    m_othergrid->AutoSizeColumn(0);
    m_othergrid->AutoSizeRows();

    other_group->Add(m_othergrid);
    hBottomSizer->Add(other_group, wxSizerFlags(0).Border(wxALL, 8));
    // End of peak and drift group

    // Start of Recommendations group - just a place-holder for layout, populated in MakeRecommendations
    m_recommend_group = new wxStaticBoxSizer(wxVERTICAL, this, _("Recommendations"));
    m_recommendgrid = new wxFlexGridSizer(2, 0, 0);
    m_recommendgrid->AddGrowableCol(0);
    m_ra_msg = nullptr;
    m_dec_msg = nullptr;
    m_snr_msg = nullptr;
    m_backlash_msg = nullptr;
    m_pae_msg = nullptr;
    m_hfd_msg = nullptr;
    m_exposure_msg = nullptr;
    m_calibration_msg = nullptr;
    m_binning_msg = nullptr;

    m_recommend_group->Add(m_recommendgrid, wxSizerFlags(1).Expand());
    // Add buttons for viewing the Dec backlash graph or getting help
    wxBoxSizer* hBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_graphBtn = new wxButton(this, wxID_ANY, _("Show Backlash Graph"));
    m_graphBtn->SetToolTip(_("Show graph of backlash measurement points"));
    m_graphBtn->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(GuidingAsstWin::OnGraph), nullptr, this);
    m_graphBtn->Enable(false);
    hBtnSizer->Add(m_graphBtn, wxSizerFlags(0).Border(wxALL, 5));
    wxButton* helpBtn = new wxButton(this, wxID_ANY, _("Help"));
    helpBtn->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(GuidingAsstWin::OnHelp), nullptr, this);
    hBtnSizer->Add(50, 0);
    hBtnSizer->Add(helpBtn, wxSizerFlags(0).Border(wxALL, 5));
    m_recommend_group->Add(hBtnSizer, wxSizerFlags(0).Border(wxALL, 5));
    // Recommendations will be hidden/shown depending on state
    hBottomSizer->Add(m_recommend_group, wxSizerFlags(0).Border(wxALL, 8));
    vResultsSizer->Add(hBottomSizer);

    m_vSizer->Add(vResultsSizer);
    m_recommend_group->Show(false);
    // End of recommendations

    m_backlashCB = new wxCheckBox(this, wxID_ANY, _("Measure Declination Backlash"));
    m_backlashCB->SetToolTip(_("PHD2 will move the guide star a considerable distance north, then south to measure backlash. Be sure the selected star has "
        "plenty of room to move in the north direction.  If the guide star is lost, increase the size of the search region to at least 20 px"));
    if (TheScope())
    {
        m_backlashCB->SetValue(!pMount->HasHPEncoders());
        m_backlashCB->Enable(true);
    }
    else
    {
        m_backlashCB->SetValue(false);
        m_backlashCB->Enable(false);
    }
    // Text area for showing backlash measuring steps
    m_gaStatus = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(500, 40), wxALIGN_CENTER);
    MakeBold(m_gaStatus);
    m_vSizer->Add(m_gaStatus, wxSizerFlags(0).Border(wxALL, 8).Center());

    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(10, 0);       // a little spacing left of Start button
    btnSizer->Add(m_backlashCB, wxSizerFlags(0).Border(wxALL, 8));
    btnSizer->Add(40, 0);       // Put a spacer between the button and checkbox

    m_start = new wxButton(this, wxID_ANY, _("Start"), wxDefaultPosition, wxDefaultSize, 0);
    m_start->SetToolTip(_("Start measuring (disables guiding)"));
    btnSizer->Add(m_start, 0, wxALL, 5);
    m_start->Enable(false);

    btnReviewPrev = new OptionsButton(this, GA_REVIEW_BUTTON, _("Review previous"), wxDefaultPosition, wxDefaultSize, 0);
    btnReviewPrev->SetToolTip(_("Review previous Guiding Assistant results"));
    btnReviewPrev->Enable(GetGAHistoryCount() > 0);
    btnSizer->Add(btnReviewPrev, 0, wxALL, 5);

    m_stop = new wxButton(this, wxID_ANY, _("Stop"), wxDefaultPosition, wxDefaultSize, 0);
    m_stop->SetToolTip(_("Stop measuring and re-enable guiding"));
    m_stop->Enable(false);

    btnSizer->Add(m_stop, 0, wxALL, 5);
    m_vSizer->Add(btnSizer, 0, wxEXPAND, 5);

    SetAutoLayout(true);
    SetSizerAndFit(m_vSizer);

    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(GuidingAsstWin::OnClose));
    Connect(APPSTATE_NOTIFY_EVENT, wxCommandEventHandler(GuidingAsstWin::OnAppStateNotify));
    m_start->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(GuidingAsstWin::OnStart), nullptr, this);
    m_stop->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(GuidingAsstWin::OnStop), nullptr, this);
    Bind(wxEVT_BUTTON, &GuidingAsstWin::OnReviewPrevious, this, GA_REVIEW_BUTTON, GA_REVIEW_BUTTON);
    Bind(wxEVT_MENU, &GuidingAsstWin::OnGAReviewSelection, this, GA_REVIEW_ITEMS_BASE, GA_REVIEW_ITEMS_LIMIT);

    if (m_backlashCB->IsEnabled())
        m_backlashTool = new BacklashTool();

    m_measuringBacklash = false;
    origMultistarMode = pFrame->pGuider->GetMultiStarMode();
    origVarDelayConfig = pFrame->GetVariableDelayConfig();
    pFrame->SetVariableDelayConfig(false, origVarDelayConfig.shortDelay, origVarDelayConfig.longDelay);

    int xpos = pConfig->Global.GetInt("/GuidingAssistant/pos.x", -1);
    int ypos = pConfig->Global.GetInt("/GuidingAssistant/pos.y", -1);
    MyFrame::PlaceWindowOnScreen(this, xpos, ypos);

    wxCommandEvent dummy;
    OnAppStateNotify(dummy); // init state-dependent controls

    reviewMode = false;
    if (pFrame->pGuider->IsGuiding())
    {
        OnStart(dummy);             // Auto-start if we're already guiding
    }
}

GuidingAsstWin::~GuidingAsstWin(void)
{
    pFrame->pGuidingAssistant = 0;
    delete m_backlashTool;
}

void GuidingAsstWin::StatsReset()
{
    m_hpfRAStats.ClearAll();
    m_lpfRAStats.ClearAll();
    m_hpfDecStats.ClearAll();
    m_decAxisStats.ClearAll();
    m_raAxisStats.ClearAll();
}

static bool GetGridToolTip(int gridNum, const wxGridCellCoords& coords, wxString *s)
{
    int col = coords.GetCol();

    if (gridNum > 1 && col != 0)
        return false;
    else
    if (col != 0 && col != 2)
        return false;

    switch (gridNum * 100 + coords.GetRow())
    {
        // status grid
        case 101:
        {
            if (col == 0)
                *s = _("Signal-to-noise ratio; a measure of how well PHD2 can isolate the star from the sky/noise background");
            else
                *s = _("Measure of overall star brightness. Consider using 'Auto-select Star' (Alt-S) to choose the star.");
            break;
        }

        // displacement grid
        case 200: *s = _("Measure of typical high-frequency right ascension star movements; guiding usually cannot correct for fluctuations this small."); break;
        case 201: *s = _("Measure of typical high-frequency declination star movements; guiding usually cannot correct for fluctuations this small."); break;

        // other grid
        case 300: *s = _("Maximum sample-sample deflection seen in right ascension."); break;
        case 301: *s = _("Maximum sample-sample deflection seen in declination."); break;
        case 302: *s = _("Maximum peak-peak deflection seen in right ascension during sampling period."); break;
        case 303: *s = _("Estimated overall drift rate in right ascension."); break;
        case 304: *s = _("Maximum drift rate in right ascension during sampling period."); break;
        case 305: *s = _("Exposure time to keep maximum RA drift below the recommended min-move level."); break;
        case 306: *s = _("Estimated overall drift rate in declination."); break;
        case 307: *s = _("Estimated declination backlash if test was completed. Results are time to clear backlash (ms) and corresponding gear angle (arc-sec). Uncertainty estimate is one unit of standard deviation"); break;
        case 308: *s = _("Estimate of polar alignment error. If the scope declination is unknown, the value displayed is a lower bound and the actual error may be larger."); break;

        default: return false;
    }

    return true;
}

void GuidingAsstWin::OnMouseMove(wxMouseEvent& ev)
{
    GridTooltipInfo *info = static_cast<GridTooltipInfo *>(ev.GetEventUserData());
    wxGridCellCoords coords(info->grid->XYToCell(info->grid->CalcUnscrolledPosition(ev.GetPosition())));
    if (coords != info->prevCoords)
    {
        info->prevCoords = coords;
        wxString s;
        if (GetGridToolTip(info->gridNum, coords, &s))
            info->grid->GetGridWindow()->SetToolTip(s);
        else
            info->grid->GetGridWindow()->UnsetToolTip();
    }
    ev.Skip();
}

void GuidingAsstWin::FillInstructions(DialogState eState)
{
    wxString instr;

    switch (eState)
    {
    case STATE_NO_STAR:
        instr = _("Choose a non-saturated star with a good SNR (>= 10) and begin guiding");
        break;
    case STATE_START_READY:
        if (!m_measurementsTaken)
            instr = _("Click Start to begin measurements.  Guiding will be disabled during this time so the star will move around.");
        else
            instr = m_instructions->GetLabel();
        break;
    case STATE_MEASURING:
        instr = _("Guiding output is disabled and star movement is being measured.  Click Stop after 2 minutes (longer if you're measuring RA tracking accuracy of the mount).");
        break;
    case STATE_STOPPED:
        instr = _("Guiding has been resumed. Look at the recommendations and make any desired changes.  Click Start to repeat the measurements, or close the window to continue guiding.");
        break;
    }
    m_instructions->SetLabel(instr);
    m_instructions->Wrap(700);
    m_instructions->Layout();
}

void GuidingAsstWin::BacklashStep(const PHD_Point& camLoc)
{
    BacklashTool::MeasurementResults qual;
    m_backlashTool->DecMeasurementStep(camLoc);
    wxString bl_msg = _("Measuring backlash: ") + m_backlashTool->GetLastStatus();
    m_gaStatus->SetLabel(bl_msg);
    if (m_backlashTool->GetBltState() == BacklashTool::BLT_STATE_COMPLETED)
    {
        wxString bl_msg = _("Measuring backlash: ") + m_backlashTool->GetLastStatus();
        m_gaStatus->SetLabel(bl_msg);
        if (m_backlashTool->GetBltState() == BacklashTool::BLT_STATE_COMPLETED)
        {
            try
            {
                if (inBLTWrapUp)
                {
                    Debug.Write("GA-BLT: Re-entrancy in Backlash step!\n");
                    return;
                }
                else
                    inBLTWrapUp = true;
                Debug.Write("GA-BLT: state = completed\n");
                qual = m_backlashTool->GetMeasurementQuality();
                if (qual == BacklashTool::MEASUREMENT_VALID || qual == BacklashTool::MEASUREMENT_TOO_FEW_NORTH)
                {
                    Debug.Write("GA-BLT: Wrap-up after normal completion\n");
                    // populate result variables
                    m_backlashPx = m_backlashTool->GetBacklashResultPx();
                    m_backlashMs = m_backlashTool->GetBacklashResultMs();
                    double bltSigmaPx;
                    m_backlashTool->GetBacklashSigma(&bltSigmaPx, &m_backlashSigmaMs);
                    double bltGearAngle = (m_backlashPx * pFrame->GetCameraPixelScale());
                    double bltGearAngleSigma = (bltSigmaPx * pFrame->GetCameraPixelScale());
                    wxString preamble = ((m_backlashMs >= 5000 || qual == BacklashTool::MEASUREMENT_TOO_FEW_NORTH) ? ">=" : "");
                    wxString outStr, outStrTr;  // untranslated and translated
                    if (qual == BacklashTool::MEASUREMENT_VALID)
                    {
                        outStr = wxString::Format("%s %d  +/-  %0.0f ms (%0.1f  +/-  %0.1f arc-sec)",
                                                  preamble, wxMax(0, m_backlashMs), m_backlashSigmaMs,
                                                  wxMax(0, bltGearAngle), bltGearAngleSigma);
                        outStrTr = wxString::Format("%s %d  +/-  %0.0f %s (%0.1f  +/-  %0.1f %s)",
                                                    preamble, wxMax(0, m_backlashMs), m_backlashSigmaMs, _("ms"),
                                                    wxMax(0, bltGearAngle), bltGearAngleSigma, _("arc-sec"));
                    }
                    else
                    {
                        outStr = wxString::Format("%s %d  +/-  ms (test impaired)",
                                                  preamble, wxMax(0, m_backlashMs));
                        outStrTr = wxString::Format("%s %d  +/-  %s",
                                                    preamble, wxMax(0, m_backlashMs), _("ms (test impaired)"));
                    }
                    m_othergrid->SetCellValue(m_backlash_loc, outStrTr);
                    HighlightCell(m_othergrid, m_backlash_loc);
                    outStr += "\n";
                    GuideLog.NotifyGAResult("Backlash=" + outStr);
                    Debug.Write("BLT: Reported result = " + outStr);
                    m_graphBtn->Enable(true);
                }
                else
                {
                    m_othergrid->SetCellValue(m_backlash_loc, "");
                }
                EndBacklashTest(qual == BacklashTool::MEASUREMENT_VALID || qual == BacklashTool::MEASUREMENT_TOO_FEW_NORTH);
            }
            catch (const wxString& msg)
            {
                Debug.Write(wxString::Format("GA-BLT: fault in completion-processing at %d, %s\n", __LINE__, msg));
                EndBacklashTest(false);
            }
        }
    }
    else if (m_backlashTool->GetBltState() == BacklashTool::BLT_STATE_ABORTED)
        EndBacklashTest(false);

    inBLTWrapUp = false;
}

void GuidingAsstWin::BacklashError()
{
    EndBacklashTest(false);
}

// Event handlers for applying recommendations
void GuidingAsstWin::OnRAMinMove(wxCommandEvent& event)
{
    GuideAlgorithm *raAlgo = pMount->GetXGuideAlgorithm();

    if (!raAlgo)
        return;

    if (raAlgo->GetMinMove() >= 0.0)
    {
        if (!raAlgo->SetMinMove(m_ra_minmove_rec))
        {
            Debug.Write(wxString::Format("GuideAssistant changed RA_MinMove to %0.2f\n", m_ra_minmove_rec));
            pFrame->pGraphLog->UpdateControls();
            pFrame->NotifyGuidingParam("RA " + raAlgo->GetGuideAlgorithmClassName() + " MinMove ", m_ra_minmove_rec);
            m_raMinMoveButton->Enable(false);
            m_flushConfig = true;
        }
        else
            Debug.Write("GuideAssistant could not change RA_MinMove\n");
    }
    else
        Debug.Write("GuideAssistant logic flaw, RA algorithm has no MinMove property\n");
}

void GuidingAsstWin::OnDecMinMove(wxCommandEvent& event)
{
    GuideAlgorithm *decAlgo = pMount->GetYGuideAlgorithm();

    if (!decAlgo)
        return;

    if (decAlgo->GetMinMove() >= 0.0)
    {
        if (!decAlgo->SetMinMove(m_dec_minmove_rec))
        {
            Debug.Write(wxString::Format("GuideAssistant changed Dec_MinMove to %0.2f\n", m_dec_minmove_rec));
            pFrame->pGraphLog->UpdateControls();
            pFrame->NotifyGuidingParam("Declination " + decAlgo->GetGuideAlgorithmClassName() + " MinMove ", m_dec_minmove_rec);
            m_decMinMoveButton->Enable(false);
            m_flushConfig = true;
        }
        else
            Debug.Write("GuideAssistant could not change Dec_MinMove\n");
    }
    else
        Debug.Write("GuideAssistant logic flaw, Dec algorithm has no MinMove property\n");
}

void GuidingAsstWin::OnDecAlgoChange(wxCommandEvent& event)
{
    if (pMount->IsStepGuider())
        return;                         // should never happen
    pMount->SetGuidingEnabled(false);
    // Need to make algo change through AD UI controls to keep everything in-synch
    Mount::MountConfigDialogPane* currMountPane = pFrame->pAdvancedDialog->GetCurrentMountPane();
    currMountPane->ChangeYAlgorithm("Lowpass2");
    Debug.Write("GuideAssistant changed Dec algo to Lowpass2\n");
    GuideAlgorithm *decAlgo = pMount->GetYGuideAlgorithm();
    if (!decAlgo || decAlgo->GetGuideAlgorithmClassName() != "Lowpass2")
    {
        Debug.Write("GuideAssistant could not set Dec algo to Lowpass2\n");
        return;
    }

    double newAggr = 80.0;
    decAlgo->SetParam("aggressiveness", newAggr);
    decAlgo->SetParam("minMove", m_dec_minmove_rec);
    Debug.Write(wxString::Format("GuideAssistant set Lowpass2 aggressiveness = %0.2f, min-move = %0.2f\n", newAggr, m_dec_minmove_rec));
    pFrame->pGraphLog->UpdateControls();
    pMount->SetGuidingEnabled(true);
    pFrame->NotifyGuidingParam("Declination algorithm", wxString("Lowpass2"));
    pFrame->NotifyGuidingParam("Declination Lowpass2 aggressivness", newAggr);
    pFrame->NotifyGuidingParam("Declination Lowpass2 MinMove", m_dec_minmove_rec);

    m_decAlgoButton->Enable(false);

    m_flushConfig = true;
}

void GuidingAsstWin::OnDecBacklash(wxCommandEvent& event)
{
    BacklashComp *pComp = TheScope()->GetBacklashComp();

    pComp->SetBacklashPulseWidth(m_backlashRecommendedMs, 0. /* floor */, 0. /* ceiling */);
    pComp->EnableBacklashComp(!pMount->IsStepGuider());
    m_decBacklashButton->Enable(false);
    m_flushConfig = true;
}

void GuidingAsstWin::OnGraph(wxCommandEvent& event)
{
    if (reviewMode)
        m_backlashTool->ShowGraph(this, gaDetails.BLTNorthMoves, gaDetails.BLTSouthMoves, gaDetails.BLTMsmtPulse);
    else
        m_backlashTool->ShowGraph(this, m_backlashTool->GetNorthSteps(), m_backlashTool->GetSouthSteps(), m_backlashTool->GetBLTMsmtPulseSize());
}

void GuidingAsstWin::OnHelp(wxCommandEvent& event)
{
    pFrame->help->Display("Tools.htm#Guiding_Assistant");   // named anchors in help file are not subject to translation
}

static wxString SizedMsg(const wxString& msg)
{
    if (msg.length() < 70)
        return msg + wxString(' ', 70 - msg.length());
    else
        return msg;
}

// Adds a recommendation string and a button bound to the passed event handler
wxStaticText *GuidingAsstWin::AddRecommendationBtn(const wxString& msg,
                                                   void (GuidingAsstWin::* handler)(wxCommandEvent&),
                                                   wxButton **ppButton)
{
    wxStaticText *rec_label;

    rec_label = new wxStaticText(this, wxID_ANY, SizedMsg(msg));
    rec_label->Wrap(250);
    m_recommendgrid->Add(rec_label, 1, wxALIGN_LEFT | wxALL, 5);
    if (handler)
    {
        int min_h;
        int min_w;
        this->GetTextExtent(_("Apply"), &min_w, &min_h);
        *ppButton = new wxButton(this, wxID_ANY, _("Apply"), wxDefaultPosition, wxSize(min_w + 8, min_h + 8), 0);
        m_recommendgrid->Add(*ppButton, 0, wxALIGN_RIGHT | wxALL, 5);
        (*ppButton)->Connect(wxEVT_COMMAND_BUTTON_CLICKED, reinterpret_cast<wxObjectEventFunction>(handler), nullptr, this);
    }
    else
    {
        wxStaticText *rec_tmp = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
        m_recommendgrid->Add(rec_tmp, 0, wxALL, 5);
    }
    return rec_label;
}

// Jacket for simple addition of a text-only recommendation
wxStaticText *GuidingAsstWin::AddRecommendationMsg(const wxString& msg)
{
    return AddRecommendationBtn(msg, nullptr, nullptr);
}

void GuidingAsstWin::LogResults()
{
    wxString str;
    Debug.Write("Guiding Assistant results follow:\n");
    str = wxString::Format("SNR=%s, Samples=%s, Elapsed Time=%s, RA HPF-RMS=%s, Dec HPF-RMS=%s, Total HPF-RMS=%s\n",
        m_statusgrid->GetCellValue(m_snr_loc), m_statusgrid->GetCellValue(m_samplecount_loc), m_statusgrid->GetCellValue(m_elapsedtime_loc),
        m_displacementgrid->GetCellValue(m_ra_rms_loc),
        m_displacementgrid->GetCellValue(m_dec_rms_loc), m_displacementgrid->GetCellValue(m_total_rms_loc));

    GuideLog.NotifyGAResult(str);
    Debug.Write(str);
    str = wxString::Format("RA Peak=%s, RA Peak-Peak %s, RA Drift Rate=%s, Max RA Drift Rate=%s, Drift-Limiting Exp=%s\n",
        m_othergrid->GetCellValue(m_ra_peak_loc),
        m_othergrid->GetCellValue(m_ra_peakpeak_loc), m_othergrid->GetCellValue(m_ra_drift_loc),
        m_othergrid->GetCellValue(m_ra_peak_drift_loc),
        m_othergrid->GetCellValue(m_ra_drift_exp_loc)
        );
    GuideLog.NotifyGAResult(str);
    Debug.Write(str);
    str = wxString::Format("Dec Drift Rate=%s, Dec Peak=%s, PA Error=%s\n",
        m_othergrid->GetCellValue(m_dec_drift_loc), m_othergrid->GetCellValue(m_dec_peak_loc),
        m_othergrid->GetCellValue(m_pae_loc));
    GuideLog.NotifyGAResult(str);
    Debug.Write(str);
}

// Get info regarding any saved GA sessions that include a BLT
static void GetBLTHistory(const std::vector<wxString>&Timestamps, int* oldestBLTInx, int* BLTCount)
{
    int oldestInx = -1;
    int bltCount = 0;
    for (int inx = 0; inx < Timestamps.size(); inx++)
    {
        wxString northBLT = "/GA/" + Timestamps[inx] + "/BLT_north";
        if (pConfig->Profile.GetString(northBLT, wxEmptyString) != wxEmptyString)
        {
            bltCount++;
            if (oldestInx < 0)
                oldestInx = inx;
        }
    }
    *oldestBLTInx = oldestInx;
    *BLTCount = bltCount;
}

int GuidingAsstWin::GetGAHistoryCount()
{
    std::vector<wxString> timeStamps = pConfig->Profile.GetGroupNames("/GA");
    return timeStamps.size();
}

// Insure that no more than 3 GA sessions are kept in the profile while also keeping at least one BLT measurement if one exists
static void TrimGAHistory(bool FreshBLT, int HistoryDepth)
{
    int bltCount;
    int oldestBLTInx;
    int totalGAs;
    wxString targetEntry;

    std::vector<wxString> timeStamps;
    timeStamps = pConfig->Profile.GetGroupNames("/GA");
    totalGAs = timeStamps.size();
    GetBLTHistory(timeStamps, &oldestBLTInx, &bltCount);
    if (totalGAs > HistoryDepth)
    {
        if (FreshBLT || bltCount == 0 || oldestBLTInx > 0 || bltCount > 1 || bltCount == totalGAs)
            targetEntry = timeStamps[0];
        else
            targetEntry = timeStamps[1];
        pConfig->Profile.DeleteGroup("/GA/" + targetEntry);
        Debug.Write(wxString::Format("GA-History: removed entry for %s\n", targetEntry));
    }
}

// Save the results from the most recent GA run in the profile
void GuidingAsstWin::SaveGAResults(const wxString* AllRecommendations)
{
    wxString prefix = "/GA/" + startStr;

    pConfig->Profile.SetString(prefix + "/timestamp", m_statusgrid->GetCellValue(m_timestamp_loc));
    pConfig->Profile.SetString(prefix + "/snr", m_statusgrid->GetCellValue(m_snr_loc));
    pConfig->Profile.SetString(prefix + "/star_mass", m_statusgrid->GetCellValue(m_starmass_loc));
    pConfig->Profile.SetString(prefix + "/sample_count", m_statusgrid->GetCellValue(m_samplecount_loc));
    pConfig->Profile.SetString(prefix + "/elapsed_time", m_statusgrid->GetCellValue(m_elapsedtime_loc));
    pConfig->Profile.SetString(prefix + "/exposure_time", m_statusgrid->GetCellValue(m_exposuretime_loc));
    pConfig->Profile.SetString(prefix + "/ra_hpf_rms", m_displacementgrid->GetCellValue(m_ra_rms_loc));
    pConfig->Profile.SetString(prefix + "/dec_hpf_rms", m_displacementgrid->GetCellValue(m_dec_rms_loc));
    pConfig->Profile.SetString(prefix + "/total_hpf_rms", m_displacementgrid->GetCellValue(m_total_rms_loc));
    pConfig->Profile.SetString(prefix + "/ra_peak", m_othergrid->GetCellValue(m_ra_peak_loc));
    pConfig->Profile.SetString(prefix + "/ra_peak_peak", m_othergrid->GetCellValue(m_ra_peakpeak_loc));
    pConfig->Profile.SetString(prefix + "/ra_drift_rate", m_othergrid->GetCellValue(m_ra_drift_loc));
    pConfig->Profile.SetString(prefix + "/ra_peak_drift_rate", m_othergrid->GetCellValue(m_ra_peak_drift_loc));
    pConfig->Profile.SetString(prefix + "/ra_drift_exposure", m_othergrid->GetCellValue(m_ra_drift_exp_loc));
    pConfig->Profile.SetString(prefix + "/dec_drift_rate", m_othergrid->GetCellValue(m_dec_drift_loc));
    pConfig->Profile.SetString(prefix + "/dec_peak", m_othergrid->GetCellValue(m_dec_peak_loc));
    pConfig->Profile.SetString(prefix + "/pa_error", m_othergrid->GetCellValue(m_pae_loc));
    pConfig->Profile.SetString(prefix + "/dec_corrected_rms", std::to_string(decCorrectedRMS));
    pConfig->Profile.SetString(prefix + "/backlash_info", m_othergrid->GetCellValue(m_backlash_loc));
    pConfig->Profile.SetString(prefix + "/dec_lf_drift_rate", std::to_string(decDriftPerMin));
    pConfig->Profile.SetString(prefix + "/rec_ra_minmove", std::to_string(m_ra_minmove_rec));
    pConfig->Profile.SetString(prefix + "/rec_dec_minmove", std::to_string(m_dec_minmove_rec));
    if (m_backlashRecommendedMs > 0)
        pConfig->Profile.SetString(prefix + "/BLT_pulse", std::to_string(m_backlashMs));
    pConfig->Profile.SetString(prefix + "/recommendations", *AllRecommendations);
    bool freshBLT = m_backlashTool && m_backlashTool->IsGraphable();        // Just did a BLT that is viewable
    if (freshBLT)
    {
        pConfig->Profile.SetInt(prefix + "/BLT_MsmtPulse", m_backlashTool->GetBLTMsmtPulseSize());
        std::vector<double> northSteps = m_backlashTool->GetNorthSteps();
        std::vector<double> southSteps = m_backlashTool->GetSouthSteps();
        wxString stepStr = "";

        for (std::vector<double>::const_iterator it = northSteps.begin(); it != northSteps.end(); ++it)
        {
            stepStr += wxString::Format("%0.1f,", *it);
        }
        stepStr = stepStr.Left(stepStr.length() - 2);
        pConfig->Profile.SetString(prefix + "/BLT_north", stepStr);

        stepStr = "";

        for (std::vector<double>::const_iterator it = southSteps.begin(); it != southSteps.end(); ++it)
        {
            stepStr += wxString::Format("%0.1f,", *it);
        }
        stepStr = stepStr.Left(stepStr.length() - 2);
        pConfig->Profile.SetString(prefix + "/BLT_South", stepStr);
    }
    TrimGAHistory(freshBLT, MAX_GA_HISTORY);
}

// Reload GA results for the passed timestamp
void GuidingAsstWin::LoadGAResults(const wxString& TimeStamp, GADetails* Details)
{
    wxString prefix = "/GA/" + TimeStamp;
    *Details = {};              // Reset all vars
    Details->TimeStamp = pConfig->Profile.GetString(prefix + "/timestamp", wxEmptyString);
    Details->SNR = pConfig->Profile.GetString(prefix + "/snr", wxEmptyString);
    Details->StarMass = pConfig->Profile.GetString(prefix + "/star_mass", wxEmptyString);
    Details->SampleCount = pConfig->Profile.GetString(prefix + "/sample_count", wxEmptyString);
    Details->ExposureTime = pConfig->Profile.GetString(prefix + "/exposure_time", wxEmptyString);
    Details->ElapsedTime = pConfig->Profile.GetString(prefix + "/elapsed_time", wxEmptyString);
    Details->RA_HPF_RMS = pConfig->Profile.GetString(prefix + "/ra_hpf_rms", wxEmptyString);
    Details->Dec_HPF_RMS = pConfig->Profile.GetString(prefix + "/dec_hpf_rms", wxEmptyString);
    Details->Total_HPF_RMS = pConfig->Profile.GetString(prefix + "/total_hpf_rms", wxEmptyString);
    Details->RAPeak = pConfig->Profile.GetString(prefix + "/ra_peak", wxEmptyString);
    Details->RAPeak_Peak = pConfig->Profile.GetString(prefix + "/ra_peak_peak", wxEmptyString);
    Details->RADriftRate = pConfig->Profile.GetString(prefix + "/ra_drift_rate", wxEmptyString);
    Details->RAMaxDriftRate = pConfig->Profile.GetString(prefix + "/ra_peak_drift_rate", wxEmptyString);
    Details->DriftLimitingExposure = pConfig->Profile.GetString(prefix + "/ra_drift_exposure", wxEmptyString);
    Details->DecDriftRate = pConfig->Profile.GetString(prefix + "/dec_drift_rate", wxEmptyString);
    Details->DecPeak = pConfig->Profile.GetString(prefix + "/dec_peak", wxEmptyString);
    Details->PAError = pConfig->Profile.GetString(prefix + "/pa_error", wxEmptyString);
    Details->DecCorrectedRMS = pConfig->Profile.GetString(prefix + "/dec_corrected_rms", wxEmptyString);
    Details->BackLashInfo = pConfig->Profile.GetString(prefix + "/backlash_info", wxEmptyString);
    Details->Dec_LF_DriftRate = pConfig->Profile.GetString(prefix + "/dec_lf_drift_rate", wxEmptyString);
    Details->RecRAMinMove = pConfig->Profile.GetString(prefix + "/rec_ra_minmove", wxEmptyString);
    Details->RecDecMinMove = pConfig->Profile.GetString(prefix + "/rec_dec_minmove", wxEmptyString);
    Details->BLTAmount = pConfig->Profile.GetString(prefix + "/BLT_pulse", wxEmptyString);
    Details->Recommendations = pConfig->Profile.GetString(prefix + "/recommendations", wxEmptyString);
    wxString northBLT = pConfig->Profile.GetString(prefix + "/BLT_North", wxEmptyString);
    wxString southBLT = pConfig->Profile.GetString(prefix + "/BLT_South", wxEmptyString);
    Details->BLTMsmtPulse = pConfig->Profile.GetInt(prefix + "/BLT_MsmtPulse", -1);
    if (northBLT.Length() > 0 && southBLT.Length() > 0)
    {
        wxStringTokenizer tok;
        wxString strVal;
        double ptVal;
        tok.SetString(northBLT, ",");
        while (tok.HasMoreTokens())
        {
            strVal = tok.GetNextToken();
            strVal.ToDouble(&ptVal);
            Details->BLTNorthMoves.push_back(ptVal);
        }

        tok.SetString(southBLT, ",");
        while (tok.HasMoreTokens())
        {
            strVal = tok.GetNextToken();
            strVal.ToDouble(&ptVal);
            Details->BLTSouthMoves.push_back(ptVal);
        }
    }
}

// Compute a drift-corrected value for Dec RMS and use that as a seeing estimate.  For long GA runs, compute values for overlapping
// 2-minute intervals and use the smallest result
// Perform suitable sanity checks, revert to default "smart" recommendations if things look wonky
void GuidingAsstWin::GetMinMoveRecs(double& RecRA, double&RecDec)
{
    AxisStats decVals;
    double bestEstimate = 1000;
    double slope = 0;
    double intcpt = 0;
    double rSquared = 0;
    double selRSquared = 0;
    double selSlope = 0;
    double correctedRMS;
    const int MEASUREMENT_WINDOW_SIZE = 120;     // seconds
    const int WINDOW_ADJUSTMENT = MEASUREMENT_WINDOW_SIZE / 2;

    int lastInx = m_decAxisStats.GetCount() - 1;
    double pxscale = pFrame->GetCameraPixelScale();
    StarDisplacement val = m_decAxisStats.GetEntry(0);
    double tStart = val.DeltaTime;
    double multiplier_ra;                                           // 65% of Dec recommendation, but 100% for encoder mounts
    double multiplier_dec = (pxscale < 1.5) ? 1.28 : 1.65;          // 20% or 10% activity target based on normal distribution
    double minMoveFloor = 0.1;

    try
    {
        if (m_decAxisStats.GetLastEntry().DeltaTime - tStart > 1.2 * MEASUREMENT_WINDOW_SIZE)           //Long GA run, more than 2.4 minutes
        {
            bool done = false;
            int inx = 0;
            while (!done)
            {
                val = m_decAxisStats.GetEntry(inx);
                decVals.AddGuideInfo(val.DeltaTime, val.StarPos, 0);
                // Compute the minimum sigma for sliding, overlapping 2-min elapsed time intervals. Include the final interval if it's >= 1.6 minutes
                if (val.DeltaTime - tStart >= MEASUREMENT_WINDOW_SIZE || (inx == lastInx && val.DeltaTime - tStart >= 0.8 * MEASUREMENT_WINDOW_SIZE))
                {
                    if (decVals.GetCount() > 1)
                    {
                        double simpleSigma = decVals.GetSigma();
                        rSquared = decVals.GetLinearFitResults(&slope, &intcpt, &correctedRMS);
                        // If there is little drift relative to the random movements, the drift-correction is irrelevant and can actually degrade the result.  So don't use the drift-corrected
                        // RMS unless it's smaller than the simple sigma
                        if (correctedRMS < simpleSigma)
                        {
                            if (correctedRMS < bestEstimate)            // Keep track of the smallest value seen
                            {
                                bestEstimate = correctedRMS;
                                selRSquared = rSquared;
                                selSlope = slope;
                            }
                        }
                        else
                            bestEstimate = wxMin(bestEstimate, simpleSigma);
                        Debug.Write(wxString::Format("GA long series, window start=%0.0f, window end=%0.0f, Uncorrected RMS=%0.3f, Drift=%0.3f, Corrected RMS=%0.3f, R-sq=%0.3f\n",
                            tStart, val.DeltaTime, simpleSigma, slope * 60, correctedRMS, rSquared));
                    }
                    // Move the start of the next window earlier by 1 minute
                    double targetTime = val.DeltaTime - WINDOW_ADJUSTMENT;
                    while (m_decAxisStats.GetEntry(inx).DeltaTime > targetTime)
                        inx--;
                    tStart = m_decAxisStats.GetEntry(inx).DeltaTime;

                    decVals.ClearAll();
                }
                else
                {
                    inx++;
                }
                done = (inx > lastInx);
            }
            Debug.Write(wxString::Format("Full uncorrected RMS=%0.3fpx, Selected Dec drift=%0.3f px/min, Best seeing estimate=%0.3fpx, R-sq=%0.3f\n",
                m_decAxisStats.GetSigma(), selSlope * 60, bestEstimate, selRSquared));
        }
        else         // Normal GA run of <= 2.4 minutes, just use the entire interval for stats
        {
            if (m_decAxisStats.GetCount() > 1)
            {
                double simpleSigma = m_decAxisStats.GetSigma();
                rSquared = m_decAxisStats.GetLinearFitResults(&slope, &intcpt, &correctedRMS);
                // If there is little drift relative to the random movements, the drift-correction is irrelevant and can actually degrade the result.  So don't use the drift-corrected
                // RMS unless it's smaller than the simple sigma
                if (correctedRMS < simpleSigma)
                    bestEstimate = correctedRMS;
                else
                    bestEstimate = simpleSigma;
                Debug.Write(wxString::Format("Uncorrected Dec RMS=%0.3fpx, Dec drift=%0.3f px/min, Best seeing estimate=%0.3fpx, R-sq=%0.3f\n",
                    simpleSigma, slope * 60, bestEstimate, rSquared));
            }
        }
        if (origMultistarMode)
        {
            bestEstimate *= 0.9;
            minMoveFloor = 0.05;
        }
        if (pMount->HasHPEncoders())
            multiplier_ra = 1.0;
        else
            multiplier_ra = 0.65;
        // round up to next multiple of .05, but do not go below 0.05 pixel for multi-star, 0.1 for single-star
        double const unit = 0.05;
        double roundUpEst = std::max(round(bestEstimate * multiplier_dec / unit + 0.5) * unit, 0.05);
        // Now apply a sanity check - there are still numerous things that could have gone wrong during the GA
        if (pxscale * roundUpEst <= 1.25)           // Min-move below 1.25 arc-sec is credible
        {
            RecDec = roundUpEst;
            RecRA = wxMax(minMoveFloor, RecDec * multiplier_ra);
            Debug.Write(wxString::Format("GA Min-Move recommendations are seeing-based: Dec=%0.3f, RA=%0.3f\n", RecDec, RecRA));
        }
        else
        {
            // Just reiterate the estimates made in the new-profile-wiz
            RecDec = GuideAlgorithm::SmartDefaultMinMove(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(), pCamera->Binning);
            RecRA = wxMax(minMoveFloor, RecDec * multiplier_ra);
            Debug.Write(wxString::Format("GA Min-Move calcs failed sanity-check, DecEst=%0.3f, Dec-HPF-Sigma=%0.3f\n", roundUpEst, m_hpfDecStats.GetSigma()));
            Debug.Write(wxString::Format("GA Min-Move recs reverting to smart defaults, RA=%0.3f, Dec=%0.3f\n", RecRA, RecDec));
        }
    }
    catch (const wxString& msg)
    {
        Debug.Write("Exception thrown in GA min-move calcs: " + msg + "\n");
        // Punt by reiterating estimates made by new-profile-wiz
        RecDec = GuideAlgorithm::SmartDefaultMinMove(pFrame->GetFocalLength(), pCamera->GetCameraPixelSize(), pCamera->Binning);
        RecRA = RecDec * multiplier_ra / multiplier_dec;
        Debug.Write(wxString::Format("GA Min-Move recs reverting to smart defaults, RA=%0.3f, Dec=%0.3f\n", RecRA, RecDec));
    }
}

// See if the mount probably has large Dec backlash, using either blt results or from inference. If so, we should relax the recommendations regarding
// polar alignment error
bool GuidingAsstWin::LikelyBacklash(const CalibrationDetails& calDetails)
{
    bool likely = false;
    BacklashComp* blc = TheScope()->GetBacklashComp();              // Always valid

    try
    {
        if (m_backlashTool->GetBltState() == BacklashTool::BLT_STATE_COMPLETED && m_backlashMs > MAX_BACKLASH_COMP)
        {
            // Just ran the BLT and the result is too big for BLC
            likely = true;
        }
        if (!likely)
        {
            // May have tried BLC in the past with a too-large pulse size
            int pulseSize;
            int floor;
            int ceiling;
            blc->GetBacklashCompSettings(&pulseSize, &floor, &ceiling);
            likely = (pulseSize > MAX_BACKLASH_COMP);
        }
        if (!likely)
        {
            // If guide mode isn't 'Auto' or 'None', user can benefit from larger polar alignment error
            DEC_GUIDE_MODE decMode = TheScope()->GetDecGuideMode();
            likely = (decMode != DEC_AUTO && decMode != DEC_NONE);
        }
        if (!likely && calDetails.decStepCount > 0)
        {
            // See if the last calibration showed little or no Dec movement to the south
            wxRealPoint northStart = calDetails.decSteps[0];
            wxRealPoint northEnd = calDetails.decSteps[calDetails.decStepCount - 1];
            double northDist = sqrt(pow(northStart.x - northEnd.x, 2) + pow(northStart.y - northEnd.y, 2));
            wxRealPoint southEnd = calDetails.decSteps[calDetails.decSteps.size() - 1];
            double southDist = sqrt(pow(northEnd.x - southEnd.x, 2) + pow(northEnd.y - southEnd.y, 2));
            likely = (southDist <= 0.1 * northDist);
        }
    }
    catch (const wxString& msg)
    {
        Debug.Write(wxString::Format("GA-LikelyBacklash: exception at %d, %s\n", __LINE__, msg));
    }

    return likely;
}

// Produce recommendations for "live" GA run
void GuidingAsstWin::MakeRecommendations()
{
    CalibrationDetails calDetails;
    TheScope()->LoadCalibrationDetails(&calDetails);
    m_suspectCalibration = calDetails.lastIssue != CI_None || m_backlashTool->GetBacklashExempted();

    GetMinMoveRecs(m_ra_minmove_rec, m_dec_minmove_rec);

    // Refine the drift-limiting exposure value based on the ra_min_move recommendation
    m_othergrid->SetCellValue(m_ra_drift_exp_loc, maxRateRA <= 0.0 ? _(" ") :
        wxString::Format("%6.1f %s ", m_ra_minmove_rec / maxRateRA, (_("s"))));

    LogResults();               // Dump the raw statistics

    // REMINDER: Any new recommendations must also be done in 'DisplayStaticRecommendations'
    // Clump the no-button messages at the top
    // ideal exposure ranges in general
    double rarms = m_hpfRAStats.GetSigma();
    double multiplier_ra  = 1.0;   // 66% prediction interval
    double ideal_min_exposure = 2.0;
    double ideal_max_exposure = 4.0;
    // adjust the min-exposure downward if drift limiting exposure is lower; then adjust range accordingly
    double drift_exp;
    if (maxRateRA > 0)
        drift_exp = ceil((multiplier_ra * rarms / maxRateRA) / 0.5) * 0.5;                       // Rounded up to nearest 0.5 sec
    else
        drift_exp = ideal_min_exposure;

    double min_rec_range = 2.0;
    double pxscale = pFrame->GetCameraPixelScale();
    m_min_exp_rec = std::max(1.0, std::min(drift_exp, ideal_min_exposure));                         // smaller of drift and ideal, never less than 1.0

    if (drift_exp > m_min_exp_rec)
    {
        if (drift_exp < ideal_max_exposure)
            m_max_exp_rec = std::max(drift_exp, m_min_exp_rec + min_rec_range);
        else
            m_max_exp_rec = ideal_max_exposure;
    }
    else
        m_max_exp_rec = m_min_exp_rec + min_rec_range;

    m_recommendgrid->Clear(true);

    wxString logStr;
    wxString allRecommendations;

    // Always make a recommendation on exposure times
    wxString msg = wxString::Format(_("Try to keep your exposure times in the range of %.1fs to %.1fs"), m_min_exp_rec, m_max_exp_rec);
    allRecommendations += "Exp:" + msg + "\n";
    m_exposure_msg = AddRecommendationMsg(msg);
    Debug.Write(wxString::Format("Recommendation: %s\n", msg));
    // Binning opportunity if image scale is < 0.5
    if (pxscale <= 0.5 && pCamera->Binning == 1 && pCamera->MaxBinning > 1)
    {
        wxString msg = _("Try binning your guide camera");
        allRecommendations += "Bin:" + msg + "\n";
        m_binning_msg = AddRecommendationMsg(msg);
        Debug.Write(wxString::Format("Recommendation: %s\n", msg));
    }
    // Previous calibration alert
    if (m_suspectCalibration)
    {
        wxString msg = _("Consider re-doing your calibration ");
        if (calDetails.lastIssue != CI_None)
            msg += _("(Prior alert)");
        else
            msg += _("(Backlash clearing)");
        allRecommendations += "Cal:" + msg + "\n";
        m_calibration_msg = AddRecommendationMsg(msg);
        logStr = wxString::Format("Recommendation: %s\n", msg);
        Debug.Write(logStr);
        GuideLog.NotifyGAResult(logStr);
    }
    // SNR
    if ((sumSNR / (double)m_lpfRAStats.GetCount()) < 10.0)
    {
        wxString msg(_("Consider using a brighter star for the test or increasing the exposure time"));
        allRecommendations += "Star:" + msg + "\n";
        m_snr_msg = AddRecommendationMsg(msg);
        logStr = wxString::Format("Recommendation: %s\n", msg);
        Debug.Write(logStr);
        GuideLog.NotifyGAResult(logStr);
    }

    // Alignment error
    if (alignmentError > 5.0)
    {
        wxString msg = "";
        // If the mount looks like it has large Dec backlash, ignore alignment error below 10 arc-min
        if (LikelyBacklash(calDetails))
        {
            if (alignmentError > 10.0)
                msg = _("Polar alignment error > 10 arc-min; try using the Drift Align tool to improve alignment.");
        }
        else
        {
            msg = alignmentError < 10.0 ?
                _("Polar alignment error > 5 arc-min; that could probably be improved.") :
                _("Polar alignment error > 10 arc-min; try using the Drift Align tool to improve alignment.");
        }
        if (msg != "")
        {
            allRecommendations += "PA:" + msg + "\n";
            m_pae_msg = AddRecommendationMsg(msg);
            logStr = wxString::Format("Recommendation: %s\n", msg);
            Debug.Write(logStr);
            GuideLog.NotifyGAResult(logStr);
        }
    }

    // Star HFD
    const Star& star = pFrame->pGuider->PrimaryStar();
    if (pxscale > 1.0 && star.HFD > 4.5)
    {
        wxString msg(_("Consider trying to improve focus on the guide camera"));
        allRecommendations += "StarHFD:" + msg + "\n";
        m_hfd_msg = AddRecommendationMsg(msg);
        logStr = wxString::Format("Recommendation: %s\n", msg);
        Debug.Write(logStr);
        GuideLog.NotifyGAResult(logStr);
    }

    // RA min-move
    if (pMount->GetXGuideAlgorithm() && pMount->GetXGuideAlgorithm()->GetMinMove() >= 0.0)
    {
        wxString msgText = wxString::Format(_("Try setting RA min-move to %0.2f"), m_ra_minmove_rec);
        allRecommendations += "RAMinMove:" + msgText + "\n";
        m_ra_msg = AddRecommendationBtn(msgText, &GuidingAsstWin::OnRAMinMove, &m_raMinMoveButton);
        logStr = wxString::Format("Recommendation: %s\n", msgText);
        Debug.Write(logStr);
        GuideLog.NotifyGAResult(logStr);
    }

    // Dec min-move
    if (pMount->GetYGuideAlgorithm() && pMount->GetYGuideAlgorithm()->GetMinMove() >= 0.0)
    {
        wxString msgText = wxString::Format(_("Try setting Dec min-move to %0.2f"), m_dec_minmove_rec);
        allRecommendations += "DecMinMove:" + msgText + "\n";
        m_dec_msg = AddRecommendationBtn(msgText, &GuidingAsstWin::OnDecMinMove, &m_decMinMoveButton);
        logStr = wxString::Format("Recommendation: %s\n", msgText);
        Debug.Write(logStr);
        GuideLog.NotifyGAResult(logStr);
    }

    // Backlash comp
    bool smallBacklash = false;
    if (m_backlashTool->GetBltState() == BacklashTool::BLT_STATE_COMPLETED)
    {
        wxString msg;

        if (m_backlashMs > 0)
        {
            m_backlashRecommendedMs = (int)(floor(m_backlashMs / 10) * 10);        // round down to nearest 10ms
            m_backlashRecommendedMs = wxMax(m_backlashRecommendedMs, 10);
        }
        else
            m_backlashRecommendedMs = 0;
        bool largeBL = m_backlashMs > MAX_BACKLASH_COMP;
        if (m_backlashMs < 100 || pMount->HasHPEncoders())
        {
            if (pMount->HasHPEncoders())
                msg = _("Mount has absolute encoders, no compensation needed");
            else
                msg = _("Backlash is small, no compensation needed");              // assume it was a small measurement error
            smallBacklash = true;
        }
        else if (m_backlashMs <= MAX_BACKLASH_COMP)
            msg = wxString::Format(_("Try starting with a Dec backlash compensation of %d ms"), m_backlashRecommendedMs);
        else
        {
            msg = wxString::Format(_("Backlash is >= %d ms; you may need to guide in only one Dec direction (currently %s)"), m_backlashMs,
                decDriftPerMin >= 0 ? _("South") : _("North"));
        }
        allRecommendations += "BLT:" + msg + "\n";
        m_backlash_msg = AddRecommendationBtn(msg, &GuidingAsstWin::OnDecBacklash, &m_decBacklashButton);
        m_decBacklashButton->Enable(!largeBL && m_backlashRecommendedMs > 100);
        logStr = wxString::Format("Recommendation: %s\n", msg);
        Debug.Write(logStr);
        GuideLog.NotifyGAResult(logStr);
    }

    bool hasEncoders = pMount->HasHPEncoders();
    if (hasEncoders || smallBacklash)               // Uses encoders or has zero backlash
    {
        GuideAlgorithm *decAlgo = pMount->GetYGuideAlgorithm();
        wxString algoChoice = decAlgo->GetGuideAlgorithmClassName();
        if (algoChoice == "ResistSwitch")           // automatically rules out AO's
        {
            wxString msgText = _("Try using Lowpass2 for Dec guiding");
            allRecommendations += "DecAlgo:" + msgText + "\n";
            m_decAlgo_msg = AddRecommendationBtn(msgText, &GuidingAsstWin::OnDecAlgoChange, &m_decAlgoButton);
            logStr = wxString::Format("Recommendation: %s\n", msgText);
            Debug.Write(logStr);
            GuideLog.NotifyGAResult(logStr);
        }
    }

    GuideLog.NotifyGACompleted();
    SaveGAResults(&allRecommendations);
    m_recommend_group->Show(true);

    m_statusgrid->Layout();
    Layout();
    GetSizer()->Fit(this);
    Debug.Write("End of Guiding Assistant output....\n");
}

// Show recommendations from a previous GA that is being reviewed
void GuidingAsstWin::DisplayStaticRecommendations(const GADetails& details)
{
    std::vector<wxString> recList;
    wxString allRecs = details.Recommendations;
    bool done = false;
    size_t end;

    m_recommendgrid->Clear(true);               // Always start fresh, delete any child buttons
    while (!done)
    {
        end = allRecs.find_first_of("\n");
        if (end > 0)
        {
            wxString rec = allRecs.Left(end);
            size_t colPos = rec.find_first_of(":");
            wxString which = rec.SubString(0, colPos - 1);
            wxString what = rec.SubString(colPos + 1, end);
            if (which == "Exp")
            {
                m_exposure_msg = AddRecommendationMsg(what);
            }
            else if (which == "Bin")
            {
                m_binning_msg = AddRecommendationMsg(what);
            }
            else if (which == "Cal")
            {
                m_calibration_msg = AddRecommendationMsg(what);
            }
            else if (which == "Star")
            {
                m_snr_msg = AddRecommendationMsg(what);
            }
            else if (which == "PA")
            {
                m_pae_msg = AddRecommendationMsg(what);
            }
            else if (which == "StarHFD")
            {
                m_hfd_msg = AddRecommendationMsg(what);
            }
            else if (which == "RAMinMove")
            {
                details.RecRAMinMove.ToDouble(&m_ra_minmove_rec);
                m_ra_msg = AddRecommendationBtn(what, &GuidingAsstWin::OnRAMinMove, &m_raMinMoveButton);
            }
            else if (which == "DecMinMove")
            {
                details.RecDecMinMove.ToDouble(&m_dec_minmove_rec);
                m_dec_msg = AddRecommendationBtn(what, &GuidingAsstWin::OnDecMinMove, &m_decMinMoveButton);
            }
            else if (which == "DecAlgo")
            {
                m_decAlgo_msg = AddRecommendationBtn(what, &GuidingAsstWin::OnDecAlgoChange, &m_decAlgoButton);
            }
            else if (which == "BLT")
            {
                m_backlashMs = wxAtoi(details.BLTAmount);
                bool largeBL = m_backlashMs > MAX_BACKLASH_COMP;
                m_backlash_msg = AddRecommendationBtn(what, &GuidingAsstWin::OnDecBacklash, &m_decBacklashButton);
                m_decBacklashButton->Enable(!largeBL && m_backlashRecommendedMs > 100);
            }
            allRecs = allRecs.Mid(end + 1);
            done = allRecs.size() == 0;
        }
    }
    m_recommend_group->Show(true);

    m_statusgrid->Layout();
    Layout();
    GetSizer()->Fit(this);
}

void GuidingAsstWin::OnStart(wxCommandEvent& event)
{
    if (!pFrame->pGuider->IsGuiding())
        return;

    double exposure = (double) pFrame->RequestedExposureDuration() / 1000.0;
    double lp_cutoff = wxMax(6.0, 3.0 * exposure);
    double hp_cutoff = 1.0;

    pFrame->pGuider->SetMultiStarMode(false);
    StatsReset();
    m_raHPF = HighPassFilter(hp_cutoff, exposure);
    m_raLPF = LowPassFilter(lp_cutoff, exposure);
    m_decHPF = HighPassFilter(hp_cutoff, exposure);

    sumSNR = sumMass = 0.0;

    m_start->Enable(false);
    m_stop->Enable(true);
    btnReviewPrev->Enable(false);
    reviewMode = false;
    m_dlgState = STATE_MEASURING;
    FillInstructions(m_dlgState);
    m_gaStatus->SetLabel(_("Measuring..."));
    m_recommend_group->Show(false);
    HighlightCell(m_displacementgrid, m_ra_rms_loc);
    HighlightCell(m_displacementgrid, m_dec_rms_loc);
    HighlightCell(m_displacementgrid, m_total_rms_loc);

    Debug.AddLine("GuidingAssistant: Disabling guide output");

    if (pMount)
    {
        m_savePrimaryMountEnabled = pMount->GetGuidingEnabled();
        pMount->SetGuidingEnabled(false);
    }
    if (pSecondaryMount)
    {
        m_saveSecondaryMountEnabled = pSecondaryMount->GetGuidingEnabled();
        pSecondaryMount->SetGuidingEnabled(false);
    }

    m_guideOutputDisabled = true;

    startStr = wxDateTime::Now().FormatISOCombined(' ');
    m_measuring = true;
    m_startTime = ::wxGetUTCTimeMillis().GetValue();
    SetSizerAndFit(m_vSizer);
}

// For a mouse-click on the 'Review Previous' options button, show a pop-up menu for whatever saved GAs are available
void GuidingAsstWin::OnReviewPrevious(wxCommandEvent& event)
{
    std::vector<wxString> entryNames;
    entryNames = pConfig->Profile.GetGroupNames("/GA");

    wxMenu* reviewList = new wxMenu();
    for (int inx = 0; inx < entryNames.size(); inx++)
    {
        reviewList->Append(GA_REVIEW_ITEMS_BASE + inx, entryNames[inx]);
    }
    PopupMenu(reviewList, btnReviewPrev->GetPosition().x,
        btnReviewPrev->GetPosition().y + btnReviewPrev->GetSize().GetHeight());

    wxDELETE(reviewList);
}

// Handle the user's choice of a GA entry for review (see above)
void GuidingAsstWin::OnGAReviewSelection(wxCommandEvent& evt)
{
    int id = evt.GetId();
    wxMenu *menu = static_cast<wxMenu *>(evt.GetEventObject());
    wxString timeStamp = menu->GetLabelText(id);

    reviewMode = true;
    LoadGAResults(timeStamp, &gaDetails);
    m_graphBtn->Enable(gaDetails.BLTNorthMoves.size() > 0);
    DisplayStaticResults(gaDetails);
}

void GuidingAsstWin::DoStop(const wxString& status)
{
    m_measuring = false;
    m_recommendgrid->Show(true);
    m_dlgState = STATE_STOPPED;
    m_measurementsTaken = true;

    FillInstructions(m_dlgState);

    if (m_guideOutputDisabled)
    {
        Debug.Write(wxString::Format("GuidingAssistant: Re-enabling guide output (%d, %d)\n", m_savePrimaryMountEnabled, m_saveSecondaryMountEnabled));

        if (pMount)
            pMount->SetGuidingEnabled(m_savePrimaryMountEnabled);
        if (pSecondaryMount)
            pSecondaryMount->SetGuidingEnabled(m_saveSecondaryMountEnabled);

        m_guideOutputDisabled = false;
        pFrame->pGuider->SetMultiStarMode(origMultistarMode);           // may force an auto-find to refresh secondary star data
        pFrame->SetVariableDelayConfig(origVarDelayConfig.enabled, origVarDelayConfig.shortDelay, origVarDelayConfig.longDelay);
    }

    m_start->Enable(pFrame->pGuider->IsGuiding());
    btnReviewPrev->Enable(GetGAHistoryCount() > 0);
    m_stop->Enable(false);

    if (m_origSubFrames != -1)
    {
        pCamera->UseSubframes = m_origSubFrames ? true : false;
        m_origSubFrames = -1;
    }
}

void GuidingAsstWin::EndBacklashTest(bool completed)
{
    if (!completed)
    {
        m_backlashTool->StopMeasurement();
        m_othergrid->SetCellValue(m_backlash_loc, _("Backlash test aborted, see graph..."));
        m_graphBtn->Enable(m_backlashTool->IsGraphable());
    }

    m_measuringBacklash = false;
    m_backlashCB->Enable(true);
    Layout();
    GetSizer()->Fit(this);

    m_start->Enable(pFrame->pGuider->IsGuiding());
    m_stop->Enable(false);
    MakeRecommendations();
    if (!completed)
    {
        wxCommandEvent dummy;
        OnAppStateNotify(dummy);            // Make sure UI is in synch
    }
    DoStop();
}

void GuidingAsstWin::OnStop(wxCommandEvent& event)
{
    bool performBLT = m_backlashCB->IsChecked();
    bool longEnough;
    if (m_elapsedSecs < GA_MIN_SAMPLING_PERIOD && !m_measuringBacklash)
    {
        SampleWait waitDlg(GA_MIN_SAMPLING_PERIOD - m_elapsedSecs, performBLT);
        longEnough = (waitDlg.ShowModal() == wxOK);
    }
    else
        longEnough = true;

    m_gaStatus->SetLabel(wxEmptyString);
    if (longEnough && performBLT)
    {
        if (!m_measuringBacklash)                               // Run the backlash test after the sampling was completed
        {
            m_measuringBacklash = true;
            if (m_origSubFrames == -1)
                m_origSubFrames = pCamera->UseSubframes ? 1 : 0;
            pCamera->UseSubframes = false;

            m_gaStatus->SetLabelText(_("Measuring backlash... ") + m_backlashTool->GetLastStatus());
            Layout();
            GetSizer()->Fit(this);
            m_backlashCB->Enable(false);                        // Don't let user turn it off once we've started
            m_measuring = false;
            m_backlashTool->StartMeasurement(decDriftPerMin);
            m_instructions->SetLabel(_("Measuring backlash... "));
        }
        else
        {
            // User hit stop during bl test
            m_gaStatus->SetLabelText(wxEmptyString);
            EndBacklashTest(false);
        }
    }
    else
    {
        if (longEnough)
            MakeRecommendations();
        DoStop();
    }
}

void GuidingAsstWin::OnAppStateNotify(wxCommandEvent& WXUNUSED(event))
{
    if (m_measuring || m_measuringBacklash)
    {
        if (!pFrame->pGuider->IsGuiding())
        {
            // if guiding stopped, stop measuring
            DoStop(_("Guiding stopped"));
        }
    }
    else
    {
        bool can_start = pFrame->pGuider->IsGuiding();
        m_start->Enable(can_start);
        if (can_start)
            m_dlgState = STATE_START_READY;
        else
            m_dlgState = STATE_NO_STAR;
        FillInstructions(m_dlgState);
    }
}

void GuidingAsstWin::OnClose(wxCloseEvent& evt)
{
    DoStop();

    // save the window position
    int x, y;
    GetPosition(&x, &y);
    pConfig->Global.SetInt("/GuidingAssistant/pos.x", x);
    pConfig->Global.SetInt("/GuidingAssistant/pos.y", y);

    if (m_flushConfig)
    {
        pConfig->Flush();
        m_flushConfig = false;
    }

    Destroy();
}

void GuidingAsstWin::FillResultCell(wxGrid *pGrid, const wxGridCellCoords& loc, double pxVal, double asVal, const wxString& units1, const wxString& units2,
    const wxString& extraInfo)
{
    pGrid->SetCellValue(loc, wxString::Format("%6.2f %s (%6.2f %s %s)", pxVal, units1, asVal, units2, extraInfo));
}

void GuidingAsstWin::DisplayStaticResults(const GADetails& details)
{
    wxString SEC(_("s"));
    wxString MSEC(_("ms"));
    wxString PX(_("px"));
    wxString ARCSEC(_("arc-sec"));
    wxString ARCMIN(_("arc-min"));
    wxString PXPERMIN(_("px/min"));
    wxString PXPERSEC(_("px/sec"));
    wxString ARCSECPERMIN(_("arc-sec/min"));
    wxString ARCSECPERSEC(_("arc-sec/sec"));

    // Display high-freq stats
    m_statusgrid->SetCellValue(m_timestamp_loc, details.TimeStamp);
    m_statusgrid->SetCellValue(m_exposuretime_loc, details.ExposureTime);
    m_statusgrid->SetCellValue(m_snr_loc, details.SNR);
    m_statusgrid->SetCellValue(m_starmass_loc, details.StarMass);
    m_statusgrid->SetCellValue(m_elapsedtime_loc, details.ElapsedTime);
    m_statusgrid->SetCellValue(m_samplecount_loc, details.SampleCount);

    // Fill other grids
    m_displacementgrid->SetCellValue(m_ra_rms_loc, details.RA_HPF_RMS);
    m_displacementgrid->SetCellValue(m_dec_rms_loc, details.Dec_HPF_RMS);
    m_displacementgrid->SetCellValue(m_total_rms_loc, details.Total_HPF_RMS);
    m_othergrid->SetCellValue(m_ra_peak_loc, details.RAPeak);
    m_othergrid->SetCellValue(m_dec_peak_loc, details.DecPeak);
    m_othergrid->SetCellValue(m_ra_peakpeak_loc, details.RAPeak_Peak);
    m_othergrid->SetCellValue(m_ra_drift_loc, details.RADriftRate);
    m_othergrid->SetCellValue(m_ra_peak_drift_loc, details.RAMaxDriftRate);
    m_othergrid->SetCellValue(m_ra_drift_exp_loc, details.DriftLimitingExposure);
    m_othergrid->SetCellValue(m_dec_drift_loc, details.DecDriftRate);
    m_othergrid->SetCellValue(m_backlash_loc, details.BackLashInfo);
    m_othergrid->SetCellValue(m_pae_loc, details.PAError);

    if (details.Recommendations.size() > 0)
        DisplayStaticRecommendations(details);
}

void GuidingAsstWin::UpdateInfo(const GuideStepInfo& info)
{
    double ra = info.mountOffset.X;
    double dec = info.mountOffset.Y;
    if (pMount->IsStepGuider())
    {
        PHD_Point mountLoc;
        TheScope()->TransformCameraCoordinatesToMountCoordinates(info.cameraOffset, mountLoc);
        ra = mountLoc.X;
        dec = mountLoc.Y;
    }
    // Update the time measures
    wxLongLong_t elapsedms = ::wxGetUTCTimeMillis().GetValue() - m_startTime;
    m_elapsedSecs = (double)elapsedms / 1000.0;
    // add offset info to various stats accumulations
    m_hpfRAStats.AddValue(m_raHPF.AddValue(ra));
    double prevRAlpf = m_raLPF.GetCurrentLPF();
    double newRAlpf = m_raLPF.AddValue(ra);
    if (m_lpfRAStats.GetCount() == 0)
        prevRAlpf = newRAlpf;
    m_lpfRAStats.AddValue(newRAlpf);
    m_hpfDecStats.AddValue(m_decHPF.AddValue(dec));
    if (m_decAxisStats.GetCount() == 0)
        m_axisTimebase = wxGetCurrentTime();
    m_decAxisStats.AddGuideInfo(wxGetCurrentTime() - m_axisTimebase, dec, 0);
    m_raAxisStats.AddGuideInfo(wxGetCurrentTime() - m_axisTimebase, ra, 0);

    // Compute the maximum interval RA movement rate using low-passed-filtered data
    if (m_lpfRAStats.GetCount() == 1)
    {
        m_startPos = info.mountOffset;
        maxRateRA = 0.0;
    }
    else
    {
        double dt = info.time - m_lastTime;
        if (dt > 0.0001)
        {
            double raRate = fabs(newRAlpf - prevRAlpf) / dt;
            if (raRate > maxRateRA)
                maxRateRA = raRate;
        }
    }

    m_lastTime = info.time;
    sumSNR += info.starSNR;
    sumMass += info.starMass;
    double n = (double)m_lpfRAStats.GetCount();

    wxString SEC(_("s"));
    wxString MSEC(_("ms"));
    wxString PX(_("px"));
    wxString ARCSEC(_("arc-sec"));
    wxString ARCMIN(_("arc-min"));
    wxString PXPERMIN(_("px/min"));
    wxString PXPERSEC(_("px/sec"));
    wxString ARCSECPERMIN(_("arc-sec/min"));
    wxString ARCSECPERSEC(_("arc-sec/sec"));

    m_statusgrid->SetCellValue(m_timestamp_loc, startStr);
    m_statusgrid->SetCellValue(m_exposuretime_loc, wxString::Format("%g%s", (double)pFrame->RequestedExposureDuration() / 1000.0, SEC));
    m_statusgrid->SetCellValue(m_snr_loc, wxString::Format("%.1f", sumSNR / n));
    m_statusgrid->SetCellValue(m_starmass_loc, wxString::Format("%.1f", sumMass / n));
    m_statusgrid->SetCellValue(m_elapsedtime_loc, wxString::Format("%u%s", (unsigned int)(elapsedms / 1000), SEC));
    m_statusgrid->SetCellValue(m_samplecount_loc, wxString::Format("%.0f", n));

    if (n > 1)
    {
        // Update the realtime high-frequency stats
        double rarms = m_hpfRAStats.GetSigma();
        double decrms = m_hpfDecStats.GetSigma();
        double combined = hypot(rarms, decrms);

        // Update the running estimate of polar alignment error using linear-fit dec drift rate
        double pxscale = pFrame->GetCameraPixelScale();
        double declination = pPointingSource->GetDeclinationRadians();
        double cosdec;
        if (declination == UNKNOWN_DECLINATION)
            cosdec = 1.0; // assume declination 0
        else
            cosdec = cos(declination);
        // polar alignment error from Barrett:
        // http://celestialwonders.com/articles/polaralignment/PolarAlignmentAccuracy.pdf
        double intcpt;
        m_decAxisStats.GetLinearFitResults(&decDriftPerMin, &intcpt);
        decDriftPerMin = 60.0 * decDriftPerMin;
        alignmentError = 3.8197 * fabs(decDriftPerMin) * pxscale / cosdec;

        // update grid display w/ running stats
        FillResultCell(m_displacementgrid, m_ra_rms_loc, rarms, rarms * pxscale, PX, ARCSEC);
        FillResultCell(m_displacementgrid, m_dec_rms_loc, decrms, decrms * pxscale, PX, ARCSEC);
        FillResultCell(m_displacementgrid, m_total_rms_loc, combined, combined * pxscale, PX, ARCSEC);
        FillResultCell(m_othergrid, m_ra_peak_loc,
            m_raAxisStats.GetMaxDelta(), m_raAxisStats.GetMaxDelta() * pxscale, PX, ARCSEC);
        FillResultCell(m_othergrid, m_dec_peak_loc,
            m_decAxisStats.GetMaxDelta(), m_decAxisStats.GetMaxDelta() * pxscale, PX, ARCSEC);
        double raPkPk = m_lpfRAStats.GetMaximum() - m_lpfRAStats.GetMinimum();
        FillResultCell(m_othergrid, m_ra_peakpeak_loc, raPkPk, raPkPk * pxscale, PX, ARCSEC);
        double raDriftRate = (ra - m_startPos.X) / m_elapsedSecs * 60.0;            // Raw max-min, can't smooth this one reliably
        FillResultCell(m_othergrid, m_ra_drift_loc, raDriftRate, raDriftRate * pxscale, PXPERMIN, ARCSECPERMIN);
        FillResultCell(m_othergrid, m_ra_peak_drift_loc, maxRateRA, maxRateRA * pxscale, PXPERSEC, ARCSECPERSEC);
        m_othergrid->SetCellValue(m_ra_drift_exp_loc, maxRateRA <= 0.0 ? _(" ") :
            wxString::Format("%6.1f %s ", 1.3 * rarms / maxRateRA, SEC));              // Will get revised when min-move is computed
        FillResultCell(m_othergrid, m_dec_drift_loc, decDriftPerMin, decDriftPerMin * pxscale, PXPERMIN, ARCSECPERMIN);
        m_othergrid->SetCellValue(m_pae_loc, wxString::Format("%s %.1f %s", declination == UNKNOWN_DECLINATION ? "> " : "", alignmentError, ARCMIN));
    }
}

wxWindow *GuidingAssistant::CreateDialogBox()
{
    return new GuidingAsstWin();
}

void GuidingAssistant::NotifyGuideStep(const GuideStepInfo& info)
{
    if (pFrame && pFrame->pGuidingAssistant)
    {
        GuidingAsstWin *win = static_cast<GuidingAsstWin *>(pFrame->pGuidingAssistant);
        if (win->m_measuring)
            win->UpdateInfo(info);
    }
}

void GuidingAssistant::NotifyFrameDropped(const FrameDroppedInfo& info)
{
    if (pFrame && pFrame->pGuidingAssistant)
    {
        // anything needed?
    }
}

void GuidingAssistant::NotifyBacklashStep(const PHD_Point& camLoc)
{
    if (pFrame && pFrame->pGuidingAssistant)
    {
        GuidingAsstWin *win = static_cast<GuidingAsstWin *>(pFrame->pGuidingAssistant);
        if (win->m_measuringBacklash)
            win->BacklashStep(camLoc);
    }
}

void GuidingAssistant::NotifyBacklashError()
{
    if (pFrame && pFrame->pGuidingAssistant)
    {
        GuidingAsstWin *win = static_cast<GuidingAsstWin *>(pFrame->pGuidingAssistant);
        if (win->m_measuringBacklash)
            win->BacklashError();
    }
}

void GuidingAssistant::UpdateUIControls()
{
    // notify GuidingAssistant window to update its controls
    if (pFrame && pFrame->pGuidingAssistant)
    {
        wxCommandEvent event(APPSTATE_NOTIFY_EVENT, pFrame->GetId());
        event.SetEventObject(pFrame);
        wxPostEvent(pFrame->pGuidingAssistant, event);
    }
}
