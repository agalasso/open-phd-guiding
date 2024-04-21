/*
 *  starcross_test.h
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

#ifndef starcross_h_included
#define starcross_h_included

enum SCT_STATES
{
    SCT_STATE_NONE,
    SCT_STATE_WEST,
    SCT_STATE_EAST,
    SCT_STATE_WEST_RETURN,
    SCT_STATE_NORTH,
    SCT_STATE_SOUTH,
    SCT_STATE_NORTH_RETURN,
    SCT_STATE_DONE
};
struct SCT_StepInfo
{
    int pulseCount;
    GUIDE_DIRECTION direction;
    SCT_STATES state;
    wxString explanation;
};

class StarCrossDialog : public wxDialog
{
    DECLARE_EVENT_TABLE()

public:
    StarCrossDialog(wxWindow *parent);
    ~StarCrossDialog();
private:

    wxSpinCtrlDouble* m_CtlGuideSpeed;
    wxSpinCtrlDouble* m_CtlNumPulses;
    wxSpinCtrlDouble* m_CtlPulseSize;
    wxSpinCtrlDouble* m_CtlLegDuration;
    wxSpinCtrlDouble* m_CtlTotalDuration;
    wxStaticBoxSizer* m_DetailsGroup;
    wxGauge* m_Progress;
    wxButton* m_StartBtn;
    wxButton* m_StopBtn;
    wxButton* m_ViewControlBtn;
    wxStaticText* m_Explanations;
    bool m_CancelTest;
    int m_DirectionalPulseCount;
    int m_Amount;
    bool m_ShowDetails;

    void SuggestParams();
    void SynchDetailSliders();
    void SynchSummarySliders();
public:
    void OnSuggest(wxCommandEvent& evt);
    void OnViewControl(wxCommandEvent& evt);
    void OnStart(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
    void OnGuideSpeedChange(wxSpinDoubleEvent& evt);
    void OnLegDurationChange(wxSpinDoubleEvent& evt);
    void OnPulseCountChange(wxSpinDoubleEvent& evt);
    void OnPulseSizeChange(wxSpinDoubleEvent& evt);
    void OnCloseWindow(wxCloseEvent& event);
    void ExecuteTest();
    SCT_StepInfo NextStep(const SCT_StepInfo& prevStep);
    wxString Explanation(const SCT_StepInfo& currStep, int dirCount);
};
#endif