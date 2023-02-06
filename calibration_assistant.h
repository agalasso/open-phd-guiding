/*
*  calibration_assistant.h
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

#ifndef Calilbration_Assistant_h_included
#define Calilbration_Assistant_h_included

class CalibrationAssistant : public wxDialog
{
private:
    // wx UI controls
    wxStaticText* m_pExplanation;
    wxTextCtrl *m_pCurrOffset;
    wxTextCtrl *m_pCurrDec;
    wxRadioButton *m_pCurrEast;
    wxRadioButton *m_pTargetEast;
    wxRadioButton *m_pCurrWest;
    wxRadioButton *m_pTargetWest;
    wxSpinCtrl *m_pTargetOffset;
    wxSpinCtrl *m_pTargetDec;
    wxStaticText *m_pMessage;
    wxButton* m_pExplainBtn;
    wxStaticText *m_pWarning;
    wxButton *m_pSlewBtn;
    wxButton *m_pCalibrateBtn;
    wxTimer *m_pTimer;


public:
    CalibrationAssistant();
    ~CalibrationAssistant(void);
    void UpdateTargetPosition(int CustHA, int CustDec);
    double m_currentRA;
    double m_currentDec;

private:
    void ShowExplanationMsg(double dec);
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
    bool m_monitoringCalibration;
    bool m_calibrationActive;
    void OnExplain(wxCommandEvent& evt);
    wxString m_lastResult;
    void ExplainResults();
    bool GetCalibPositionRecommendations(int* HA, int* Dec) const;
    void GetCustomLocation(int* PrefHA, int* PrefDec, bool* SingleSide, bool* UsingDefaults) const;
    bool m_sanityCheckDone;
    bool m_meridianFlipping;
    bool m_isSlewing;
    bool m_justSlewed;

    void PerformSanityChecks();
    GUIDER_STATE m_guiderState;
    void TrackCalibration(GUIDER_STATE state);
    void EvaluateCalibration();
};

class CalCustomDialog : public wxDialog
{
public:
    CalCustomDialog(CalibrationAssistant* Parent, int DefaultHA, int DefaultDec);

private:
    CalibrationAssistant* m_Parent;
    wxSpinCtrl* m_pTargetDec;
    wxSpinCtrl* m_pTargetOffset;
    wxRadioButton* m_pTargetWest;
    wxRadioButton* m_pTargetEast;
    wxCheckBox* m_pEastWestOnly;
    void OnOk(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);
    void OnTargetWest(wxCommandEvent& evt);
    void OnTargetEast(wxCommandEvent& evt);
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
#endif