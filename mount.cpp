/*
 *  mount.cpp
 *  PHD Guiding
 *
 *  Created by Bret McKee
 *  Copyright (c) 2012 Bret McKee
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
#include "backlash_comp.h"
#include "guiding_assistant.h"
#include "gaussian_process_guider.h"

#include <wx/tokenzr.h>
#include <cstdarg>

enum
{
    Algo_RA_Layout_Height = 200,
    Algo_Dec_Layout_Height = 150
};

inline static PierSide OppositeSide(PierSide p)
{
    switch (p) {
    case PIER_SIDE_EAST: return PIER_SIDE_WEST;
    case PIER_SIDE_WEST: return PIER_SIDE_EAST;
    default:             return PIER_SIDE_UNKNOWN;
    }
}

inline static bool IsOppositeSide(PierSide a, PierSide b)
{
    return (a == PIER_SIDE_EAST && b == PIER_SIDE_WEST) ||
        (a == PIER_SIDE_WEST && b == PIER_SIDE_EAST);
}

inline static wxString PierSideStr(PierSide p)
{
    switch (p) {
    case PIER_SIDE_EAST: return "East";
    case PIER_SIDE_WEST: return "West";
    default:             return "Unknown";
    }
}

inline static wxString PierSideStrTr(PierSide p, const wxString& unknown = _("Unknown"))
{
    switch (p) {
    case PIER_SIDE_EAST: return _("East");
    case PIER_SIDE_WEST: return _("West");
    default:             return unknown;
    }
}

wxString Mount::PierSideStr(PierSide p)
{
    return ::PierSideStr(p);
}

wxString Mount::PierSideStrTr(PierSide p)
{
    return ::PierSideStrTr(p);
}

inline static const char *ParityStr(GuideParity par)
{
    switch (par) {
    case GUIDE_PARITY_EVEN:      return "+";
    case GUIDE_PARITY_ODD:       return "-";
    case GUIDE_PARITY_UNKNOWN:   return "?";
    default: case GUIDE_PARITY_UNCHANGED: return "x";
    }
}

inline static GuideParity OppositeParity(GuideParity p)
{
    switch (p) {
    case GUIDE_PARITY_EVEN: return GUIDE_PARITY_ODD;
    case GUIDE_PARITY_ODD:  return GUIDE_PARITY_EVEN;
    default:                return p;
    }
}

wxString Mount::DeclinationStr(double dec, const wxString& numFormatStr)
{
    return dec == UNKNOWN_DECLINATION ? "Unknown" : wxString::Format(numFormatStr, degrees(dec));
}

wxString Mount::DeclinationStrTr(double dec, const wxString& numFormatStr)
{
    return dec == UNKNOWN_DECLINATION ? _("Unknown") : wxString::Format(numFormatStr, degrees(dec));
}

static ConfigDialogPane *GetGuideAlgoDialogPane(GuideAlgorithm *algo, wxWindow *parent, int fixedHeight)
{
    // we need to force the guide alogorithm config pane to be large enough for
    // any of the guide algorithms
    ConfigDialogPane *pane = algo->GetConfigDialogPane(parent);
    pane->SetMinSize(-1, fixedHeight);
    return pane;
}

Mount::MountConfigDialogPane::MountConfigDialogPane(wxWindow *pParent, const wxString& title, Mount *mount)
    : ConfigDialogPane(title, pParent)
{
    // int width;
    m_pMount = mount;
    m_pParent = pParent;
    m_pAlgoBox = nullptr;
    m_pRABox = nullptr;
    m_pDecBox = nullptr;
}

static GUIDE_ALGORITHM GuideAlgorithmFromName(const wxString& s)
{
    if (s == _("None"))
        return GUIDE_ALGORITHM_IDENTITY;
    if (s == _("Hysteresis"))
        return GUIDE_ALGORITHM_HYSTERESIS;
    if (s == _("Lowpass"))
        return GUIDE_ALGORITHM_LOWPASS;
    if (s == _("Lowpass2"))
        return GUIDE_ALGORITHM_LOWPASS2;
    if (s == _("Resist Switch"))
        return GUIDE_ALGORITHM_RESIST_SWITCH;
    if (s ==_("Predictive PEC"))
        return GUIDE_ALGORITHM_GAUSSIAN_PROCESS;
    if (s.StartsWith(_("ZFilter")))
        return GUIDE_ALGORITHM_ZFILTER;
    return GUIDE_ALGORITHM_NONE;
}

// returns the untranslated name
static wxString GuideAlgorithmName(int algo)
{
    switch (algo)
    {
    case GUIDE_ALGORITHM_NONE:
    case GUIDE_ALGORITHM_IDENTITY:
    default:
        return wxTRANSLATE("None");
    case GUIDE_ALGORITHM_HYSTERESIS:
        return wxTRANSLATE("Hysteresis");
    case GUIDE_ALGORITHM_LOWPASS:
        return wxTRANSLATE("Lowpass");
    case GUIDE_ALGORITHM_LOWPASS2:
        return wxTRANSLATE("Lowpass2");
    case GUIDE_ALGORITHM_RESIST_SWITCH:
        return wxTRANSLATE("Resist Switch");
    case GUIDE_ALGORITHM_GAUSSIAN_PROCESS:
        return wxTRANSLATE("Predictive PEC");
    case GUIDE_ALGORITHM_ZFILTER:
        return wxTRANSLATE("ZFilter");
    }
}

static wxString GuideAlgorithmNameTr(int algo)
{
    return wxGetTranslation(GuideAlgorithmName(algo));
}

// Lots of dynamic controls on this pane - keep the creation/management in ConfigDialogPane
void Mount::MountConfigDialogPane::LayoutControls(wxPanel *pParent, BrainCtrlIdMap& CtrlMap)
{
    int width;
    bool stepGuider = false;

    if (m_pMount)
    {
        stepGuider = m_pMount->IsStepGuider();
        m_pAlgoBox = new wxStaticBoxSizer(wxHORIZONTAL, m_pParent, wxEmptyString);
        m_pRABox = new wxStaticBoxSizer(wxVERTICAL, m_pParent, stepGuider ? _("AO X-Axis") : _("Right Ascension"));
        if (m_pDecBox)
            m_pDecBox->Clear(true);
        m_pDecBox = new wxStaticBoxSizer(wxVERTICAL, m_pParent, stepGuider ? _("AO Y-Axis") : _("Declination"));
        wxSizerFlags def_flags = wxSizerFlags(0).Border(wxALL, 5).Expand();

        static GUIDE_ALGORITHM const RA_ALGORITHMS[] =
        {
            GUIDE_ALGORITHM_HYSTERESIS,
            GUIDE_ALGORITHM_LOWPASS,
            GUIDE_ALGORITHM_LOWPASS2,
            GUIDE_ALGORITHM_RESIST_SWITCH,
            GUIDE_ALGORITHM_GAUSSIAN_PROCESS,
            GUIDE_ALGORITHM_ZFILTER,
        };
        static GUIDE_ALGORITHM const DEC_ALGORITHMS[] =
        {
            GUIDE_ALGORITHM_HYSTERESIS,
            GUIDE_ALGORITHM_LOWPASS,
            GUIDE_ALGORITHM_LOWPASS2,
            GUIDE_ALGORITHM_RESIST_SWITCH,
            GUIDE_ALGORITHM_ZFILTER,
        };
        static GUIDE_ALGORITHM const AO_ALGORITHMS[] =
        {
            GUIDE_ALGORITHM_HYSTERESIS,
            GUIDE_ALGORITHM_LOWPASS,
            GUIDE_ALGORITHM_LOWPASS2,
            GUIDE_ALGORITHM_ZFILTER,
        };

        wxArrayString xAlgorithms;
        if (stepGuider)
        {
            for (int i = 0; i < WXSIZEOF(AO_ALGORITHMS); i++)
                xAlgorithms.push_back(GuideAlgorithmNameTr(AO_ALGORITHMS[i]));
        }
        else
        {
            for (int i = 0; i < WXSIZEOF(RA_ALGORITHMS); i++)
                xAlgorithms.push_back(GuideAlgorithmNameTr(RA_ALGORITHMS[i]));
        }

        width = StringArrayWidth(&xAlgorithms[0], xAlgorithms.size());
        m_pXGuideAlgorithmChoice = new wxChoice(m_pParent, wxID_ANY, wxPoint(-1, -1),
            wxSize(width + 35, -1), xAlgorithms);

        if (stepGuider)
            m_pXGuideAlgorithmChoice->SetToolTip(_("Which Guide Algorithm to use for X-axis"));
        else
            m_pXGuideAlgorithmChoice->SetToolTip(_("Which Guide Algorithm to use for Right Ascension"));

        m_pParent->Connect(m_pXGuideAlgorithmChoice->GetId(), wxEVT_COMMAND_CHOICE_SELECTED,
            wxCommandEventHandler(Mount::MountConfigDialogPane::OnXAlgorithmSelected), 0, this);

        if (!m_pMount->m_pXGuideAlgorithm)
        {
            m_pXGuideAlgorithmConfigDialogPane = nullptr;
        }
        else
        {
            m_pXGuideAlgorithmConfigDialogPane = GetGuideAlgoDialogPane(m_pMount->m_pXGuideAlgorithm, m_pParent, Algo_RA_Layout_Height);
        }
        m_pRABox->Add(m_pXGuideAlgorithmChoice, def_flags);
        m_pRABox->Add(m_pXGuideAlgorithmConfigDialogPane, def_flags);

        if (!stepGuider)
            m_pRABox->Add(GetSizerCtrl(CtrlMap, AD_szMaxRAAmt), wxSizerFlags(0).Border(wxTOP, 35).Center());

        // Parameter resets are applicable to either scope or AO "mounts"
        m_pResetRAParams = new wxButton(m_pParent, wxID_ANY, _("Reset"));
        m_pResetRAParams->SetToolTip(_("Causes an IMMEDIATE reset of the RA algorithm parameters to their default values"));
        m_pResetRAParams->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &Mount::MountConfigDialogPane::OnResetRAParams, this);
        if (!stepGuider)
            m_pRABox->Add(m_pResetRAParams, wxSizerFlags(0).Border(wxTOP, 52).Center());
        else
            m_pRABox->Add(m_pResetRAParams, wxSizerFlags(0).Border(wxTOP, 30).Center());

        wxArrayString yAlgorithms;

        if (stepGuider)
        {
            for (int i = 0; i < WXSIZEOF(AO_ALGORITHMS); i++)
                yAlgorithms.push_back(GuideAlgorithmNameTr(AO_ALGORITHMS[i]));
        }
        else
        {
            for (int i = 0; i < WXSIZEOF(DEC_ALGORITHMS); i++)
                yAlgorithms.push_back(GuideAlgorithmNameTr(DEC_ALGORITHMS[i]));
        }

        width = StringArrayWidth(&yAlgorithms[0], yAlgorithms.size());
        m_pYGuideAlgorithmChoice = new wxChoice(m_pParent, wxID_ANY, wxPoint(-1, -1),
            wxSize(width + 35, -1), yAlgorithms);

        if (stepGuider)
            m_pYGuideAlgorithmChoice->SetToolTip(_("Which Guide Algorithm to use for Y-axis"));
        else
            m_pYGuideAlgorithmChoice->SetToolTip(_("Which Guide Algorithm to use for Declination"));

        m_pParent->Connect(m_pYGuideAlgorithmChoice->GetId(), wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler(Mount::MountConfigDialogPane::OnYAlgorithmSelected), 0, this);

        if (!m_pMount->m_pYGuideAlgorithm)
        {
            m_pYGuideAlgorithmConfigDialogPane = nullptr;
        }
        else
        {
            m_pYGuideAlgorithmConfigDialogPane = GetGuideAlgoDialogPane(m_pMount->m_pYGuideAlgorithm, m_pParent, Algo_Dec_Layout_Height);
        }
        m_pDecBox->Add(m_pYGuideAlgorithmChoice, def_flags);
        m_pDecBox->Add(m_pYGuideAlgorithmConfigDialogPane, def_flags);

        if (!stepGuider)
        {
            wxBoxSizer *pSizer = new wxBoxSizer(wxHORIZONTAL);
            pSizer->Add(GetSizerCtrl(CtrlMap, AD_szBLCompCtrls), wxSizerFlags(0).Border(wxTOP | wxRIGHT, 5).Expand());
            m_pDecBox->Add(pSizer);
            m_pDecBox->Add(GetSizerCtrl(CtrlMap, AD_szMaxDecAmt), wxSizerFlags(0).Border(wxTOP, 10).Center());
            m_pDecBox->Add(GetSizerCtrl(CtrlMap, AD_szDecGuideMode), wxSizerFlags(0).Border(wxTOP, 10).Center());
        }

        m_pResetDecParams = new wxButton(m_pParent, wxID_ANY, _("Reset"));
        m_pResetDecParams->SetToolTip(_("Causes an IMMEDIATE reset of the DEC algorithm parameters to their default values"));
        m_pResetDecParams->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &Mount::MountConfigDialogPane::OnResetDecParams, this);
        m_pDecBox->Add(m_pResetDecParams, wxSizerFlags(0).Border(wxTOP, 20).Center());

        m_pAlgoBox->Add(m_pRABox, def_flags);
        m_pAlgoBox->Add(m_pDecBox, def_flags);
        m_pAlgoBox->Layout();
        this->Add(m_pAlgoBox, def_flags);

        Fit(m_pParent);
    }
}

Mount::MountConfigDialogPane::~MountConfigDialogPane()
{
}

void Mount::MountConfigDialogPane::ResetRAGuidingParams()
{
    // Re-initialize the algorithm params
    if (!m_pMount)
        return;
    GuideAlgorithm *currRAAlgo = m_pMount->m_pXGuideAlgorithm;
    currRAAlgo->ResetParams();                  // Default is to remove the Registry entries below the "X" or "Y" guide algo name
    delete m_pMount->m_pXGuideAlgorithm;        // force creation of a new algo instance
    m_pMount->m_pXGuideAlgorithm = nullptr;
    wxCommandEvent dummy;
    OnXAlgorithmSelected(dummy);                // Update the UI
    // Re-initialize any other RA guiding parameters not part of algos
    if (!m_pMount->IsStepGuider())
    {
        ScopeConfigDialogCtrlSet *scopeCtrlSet = (ScopeConfigDialogCtrlSet *) m_pMount->currConfigDialogCtrlSet;
        scopeCtrlSet->ResetRAParameterUI();
    }
}

// This logic only affects the controls in the UI, nothing permanent here
void Mount::MountConfigDialogPane::OnImageScaleChange()
{
    m_pXGuideAlgorithmConfigDialogPane->OnImageScaleChange();
    m_pYGuideAlgorithmConfigDialogPane->OnImageScaleChange();
}

void Mount::MountConfigDialogPane::OnResetRAParams(wxCommandEvent& evt)
{
    ResetRAGuidingParams();
}

void Mount::MountConfigDialogPane::ResetDecGuidingParams()
{
    // Re-initialize the algorithm params
    if (!m_pMount)
        return;
    GuideAlgorithm *currDecAlgo = m_pMount->m_pYGuideAlgorithm;
    currDecAlgo->ResetParams();
    delete m_pMount->m_pYGuideAlgorithm;
    m_pMount->m_pYGuideAlgorithm = nullptr;
    wxCommandEvent dummy;
    OnYAlgorithmSelected(dummy);
    // Re-initialize any other Dec guiding parameters not part of algos
    if (!m_pMount->IsStepGuider())
    {
        ScopeConfigDialogCtrlSet *scopeCtrlSet = (ScopeConfigDialogCtrlSet *) m_pMount->currConfigDialogCtrlSet;
        scopeCtrlSet->ResetDecParameterUI();
    }
}

void Mount::MountConfigDialogPane::EnableDecControls(bool enable)
{
    if (m_pYGuideAlgorithmConfigDialogPane)
    {
        m_pYGuideAlgorithmConfigDialogPane->EnableDecControls(enable);
    }
}

void Mount::MountConfigDialogPane::OnResetDecParams(wxCommandEvent& evt)
{
    ResetDecGuidingParams();
}

void Mount::MountConfigDialogPane::OnXAlgorithmSelected(wxCommandEvent& evt)
{
    ConfigDialogPane *oldpane = m_pXGuideAlgorithmConfigDialogPane;
    oldpane->Clear(true);
    m_pMount->SetXGuideAlgorithm(GuideAlgorithmFromName(m_pXGuideAlgorithmChoice->GetStringSelection()));
    ConfigDialogPane *newpane = GetGuideAlgoDialogPane(m_pMount->m_pXGuideAlgorithm, m_pParent, Algo_RA_Layout_Height);
    m_pRABox->Replace(oldpane, newpane);
    m_pXGuideAlgorithmConfigDialogPane = newpane;
    m_pXGuideAlgorithmConfigDialogPane->LoadValues();
    m_pRABox->Layout();
    m_pAlgoBox->Layout();
    m_pParent->Layout();
    m_pParent->Update();
    m_pParent->Refresh();

    // we can probably get rid of this when we reduce the number of GP algo settings
    wxWindow *adv = pFrame->pAdvancedDialog;
    adv->GetSizer()->Fit(adv);
}

void Mount::MountConfigDialogPane::OnYAlgorithmSelected(wxCommandEvent& evt)
{
    ConfigDialogPane *oldpane = m_pYGuideAlgorithmConfigDialogPane;
    oldpane->Clear(true);
    m_pMount->SetYGuideAlgorithm(GuideAlgorithmFromName(m_pYGuideAlgorithmChoice->GetStringSelection()));
    ConfigDialogPane *newpane = GetGuideAlgoDialogPane(m_pMount->m_pYGuideAlgorithm, m_pParent, Algo_Dec_Layout_Height);
    m_pDecBox->Replace(oldpane, newpane);
    m_pYGuideAlgorithmConfigDialogPane = newpane;
    m_pYGuideAlgorithmConfigDialogPane->LoadValues();
    m_pDecBox->Layout();
    m_pAlgoBox->Layout();
    m_pParent->Layout();
    m_pParent->Update();
    m_pParent->Refresh();

    // For Dec algo change, enable algo controls based on current UI setting for Dec guide mode
    if (!m_pMount->IsStepGuider())
    {
        ScopeConfigDialogCtrlSet* pScopeCtrlSet = (ScopeConfigDialogCtrlSet*) m_pMount->currConfigDialogCtrlSet;
        DEC_GUIDE_MODE whichMode = pScopeCtrlSet->GetDecGuideModeUI();
        EnableDecControls(whichMode != DEC_NONE);
    }

    // we can probably get rid of this when we reduce the number of GP algo settings
    wxWindow *adv = pFrame->pAdvancedDialog;
    adv->GetSizer()->Fit(adv);
}

void Mount::MountConfigDialogPane::ChangeYAlgorithm(const wxString& algoName)
{
    if (m_pMount)
    {
        int choiceInx = m_pYGuideAlgorithmChoice->FindString(algoName);
        if (choiceInx >= 0)
        {
            m_pYGuideAlgorithmChoice->SetSelection(choiceInx);
            wxCommandEvent dummy;
            OnYAlgorithmSelected(dummy);
        }
    }
}

void Mount::MountConfigDialogPane::LoadValues()
{
    m_initXGuideAlgorithmSelection = m_pMount->GetXGuideAlgorithmSelection();
    m_pXGuideAlgorithmChoice->SetStringSelection(GuideAlgorithmNameTr(m_initXGuideAlgorithmSelection));
    m_pXGuideAlgorithmChoice->Enable(!pFrame->CaptureActive);
    m_initYGuideAlgorithmSelection = m_pMount->GetYGuideAlgorithmSelection();
    m_pYGuideAlgorithmChoice->SetStringSelection(GuideAlgorithmNameTr(m_initYGuideAlgorithmSelection));
    m_pYGuideAlgorithmChoice->Enable(!pFrame->CaptureActive);

    if (m_pXGuideAlgorithmConfigDialogPane)
    {
        m_pXGuideAlgorithmConfigDialogPane->LoadValues();
    }

    if (m_pYGuideAlgorithmConfigDialogPane)
    {
        m_pYGuideAlgorithmConfigDialogPane->LoadValues();
    }
}

void Mount::MountConfigDialogPane::UnloadValues()
{

    // note these two have to be before the SetXxxAlgorithm calls, because if we
    // changed the algorithm, the current one will get freed, and if we make
    // these two calls after that, bad things happen
    if (m_pXGuideAlgorithmConfigDialogPane)
    {
        m_pXGuideAlgorithmConfigDialogPane->UnloadValues();
    }

    if (m_pYGuideAlgorithmConfigDialogPane)
    {
        m_pYGuideAlgorithmConfigDialogPane->UnloadValues();
    }

    m_pMount->SetXGuideAlgorithm(GuideAlgorithmFromName(m_pXGuideAlgorithmChoice->GetStringSelection()));
    m_pMount->SetYGuideAlgorithm(GuideAlgorithmFromName(m_pYGuideAlgorithmChoice->GetStringSelection()));
}

// Restore the guide algorithms - all the UI controls will follow correctly if the actual algorithm choices are correct
void Mount::MountConfigDialogPane::Undo()
{
    if (pMount)
    {
        if (m_pXGuideAlgorithmConfigDialogPane)
        {
            m_pXGuideAlgorithmConfigDialogPane->Undo();
        }

        if (m_pYGuideAlgorithmConfigDialogPane)
        {
            m_pYGuideAlgorithmConfigDialogPane->Undo();
        }
        m_pMount->SetXGuideAlgorithm(m_initXGuideAlgorithmSelection);
        m_pXGuideAlgorithmChoice->SetStringSelection(GuideAlgorithmNameTr(m_initXGuideAlgorithmSelection));
        wxCommandEvent dummy;
        OnXAlgorithmSelected(dummy);
        m_pMount->SetYGuideAlgorithm(m_initYGuideAlgorithmSelection);
        m_pYGuideAlgorithmChoice->SetStringSelection(GuideAlgorithmNameTr(m_initYGuideAlgorithmSelection));
        OnYAlgorithmSelected(dummy);
    }
}
MountConfigDialogCtrlSet *Mount::GetConfigDialogCtrlSet(wxWindow *pParent, Mount *mount, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
{
    return new MountConfigDialogCtrlSet(pParent, mount, pAdvancedDialog, CtrlMap);
}

// These are only controls that are exported to other panes - all the other dynamically updated controls are handled in
// ConfigDialogPane
MountConfigDialogCtrlSet::MountConfigDialogCtrlSet(wxWindow *pParent, Mount *mount, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap) :
ConfigDialogCtrlSet(pParent, pAdvancedDialog, CtrlMap)
{
    bool enableCtrls = mount != nullptr;
    m_pMount = mount;
    if (m_pMount)
    {
        if (!mount->IsStepGuider())
        {
            m_pClearCalibration = new wxCheckBox(GetParentWindow(AD_cbClearCalibration), wxID_ANY, _("Clear mount calibration"));
            m_pClearCalibration->Enable(enableCtrls);
            AddCtrl(CtrlMap, AD_cbClearCalibration, m_pClearCalibration,
                _("Clear the current mount calibration data - calibration will be re-done when guiding is started"));
            m_pEnableGuide = new wxCheckBox(GetParentWindow(AD_cbEnableGuiding), wxID_ANY, _("Enable mount guide output"));
            AddCtrl(CtrlMap, AD_cbEnableGuiding, m_pEnableGuide,
                _("Keep this checked for guiding. Un-check to disable all mount guide commands and allow the mount to run un-guided"));
        }
    }
}

void MountConfigDialogCtrlSet::LoadValues()
{
    if (m_pMount && !m_pMount->IsStepGuider())
    {
        m_pClearCalibration->Enable(m_pMount->IsCalibrated());
        m_pClearCalibration->SetValue(false);
        m_pEnableGuide->SetValue(m_pMount->GetGuidingEnabled());
    }
}

void MountConfigDialogCtrlSet::UnloadValues()
{
    if (m_pMount && !m_pMount->IsStepGuider())
    {
        if (m_pClearCalibration->IsChecked())
        {
            m_pMount->ClearCalibration();
            Debug.Write(wxString::Format("User cleared %s calibration\n", m_pMount->IsStepGuider() ? "AO" : "Mount"));
        }

        m_pMount->SetGuidingEnabled(m_pEnableGuide->GetValue());
    }
}

GUIDE_ALGORITHM Mount::GetXGuideAlgorithmSelection() const
{
    return GetGuideAlgorithm(m_pXGuideAlgorithm);
}

void Mount::SetXGuideAlgorithm(int guideAlgorithm)
{
    if (!m_pXGuideAlgorithm || m_pXGuideAlgorithm->Algorithm() != guideAlgorithm)
    {
        delete m_pXGuideAlgorithm;

        if (guideAlgorithm == GUIDE_ALGORITHM_NONE)
            guideAlgorithm = DefaultXGuideAlgorithm();

        if (CreateGuideAlgorithm(guideAlgorithm, this, GUIDE_X, &m_pXGuideAlgorithm))
        {
            GUIDE_ALGORITHM defaultAlgorithm = DefaultXGuideAlgorithm();
            CreateGuideAlgorithm(defaultAlgorithm, this, GUIDE_X, &m_pXGuideAlgorithm);
            guideAlgorithm = defaultAlgorithm;
        }

        pConfig->Profile.SetInt("/" + GetMountClassName() + "/XGuideAlgorithm", guideAlgorithm);
    }
}

GUIDE_ALGORITHM Mount::GetYGuideAlgorithmSelection() const
{
    return GetGuideAlgorithm(m_pYGuideAlgorithm);
}

void Mount::SetYGuideAlgorithm(int guideAlgorithm)
{
    if (!m_pYGuideAlgorithm || m_pYGuideAlgorithm->Algorithm() != guideAlgorithm)
    {
        delete m_pYGuideAlgorithm;

        if (guideAlgorithm == GUIDE_ALGORITHM_NONE)
            guideAlgorithm = DefaultYGuideAlgorithm();

        if (CreateGuideAlgorithm(guideAlgorithm, this, GUIDE_Y, &m_pYGuideAlgorithm))
        {
            GUIDE_ALGORITHM defaultAlgorithm = DefaultYGuideAlgorithm();
            CreateGuideAlgorithm(defaultAlgorithm, this, GUIDE_Y, &m_pYGuideAlgorithm);
            guideAlgorithm = defaultAlgorithm;
        }

        pConfig->Profile.SetInt("/" + GetMountClassName() + "/YGuideAlgorithm", guideAlgorithm);
    }
}

static void NotifyGuidingEnabled(GuideAlgorithm *xAlgo, GuideAlgorithm *yAlgo, bool enabled)
{
    if (enabled)
    {
        if (xAlgo)
            xAlgo->GuidingEnabled();

        if (yAlgo)
            yAlgo->GuidingEnabled();
    }
    else
    {
        if (xAlgo)
            xAlgo->GuidingDisabled();

        if (yAlgo)
            yAlgo->GuidingDisabled();
    }
}

void Mount::SetGuidingEnabled(bool guidingEnabled)
{
    if (guidingEnabled != m_guidingEnabled)
    {
        const char *s = IsStepGuider() ? "AOGuidingEnabled" : "MountGuidingEnabled";
        Debug.Write(wxString::Format("%s: %d\n", s, guidingEnabled));
        pFrame->NotifyGuidingParam(s, guidingEnabled);
        m_guidingEnabled = guidingEnabled;
        NotifyGuidingEnabled(m_pXGuideAlgorithm, m_pYGuideAlgorithm, guidingEnabled);
        if (guidingEnabled)
        {
            // avoid sending false positive alerts after guiding is
            // re-enabled
            DeferPulseLimitAlertCheck();
        }
    }
}

void Mount::DeferPulseLimitAlertCheck()
{
}

GUIDE_ALGORITHM Mount::GetGuideAlgorithm(const GuideAlgorithm *pAlgorithm)
{
    return pAlgorithm ? pAlgorithm->Algorithm() : GUIDE_ALGORITHM_NONE;
}

static GuideAlgorithm *MakeGaussianProcessGuideAlgo(Mount *mount, GuideAxis axis)
{
    static bool s_gp_debug_inited;
    if (!s_gp_debug_inited)
    {
        class PHD2DebugLogger : public GPDebug {
            void Log(const char *format, ...) {
                va_list ap;
                va_start(ap, format);
                Debug.Write(wxString::FormatV(format + wxString("\n"), ap));
                va_end(ap);
            }
        };
        GPDebug::SetGPDebug(new PHD2DebugLogger());
        s_gp_debug_inited = true;
    }
    return new GuideAlgorithmGaussianProcess(mount, axis);
}

bool Mount::CreateGuideAlgorithm(int guideAlgorithm, Mount *mount, GuideAxis axis, GuideAlgorithm **ppAlgorithm)
{
    bool error = false;

    try
    {
        switch (guideAlgorithm)
        {
            case GUIDE_ALGORITHM_NONE:
            case GUIDE_ALGORITHM_IDENTITY:
                *ppAlgorithm = new GuideAlgorithmIdentity(mount, axis);
                break;
            case GUIDE_ALGORITHM_HYSTERESIS:
                *ppAlgorithm = new GuideAlgorithmHysteresis(mount, axis);
                break;
            case GUIDE_ALGORITHM_LOWPASS:
                *ppAlgorithm = new GuideAlgorithmLowpass(mount, axis);
                break;
            case GUIDE_ALGORITHM_LOWPASS2:
                *ppAlgorithm = new GuideAlgorithmLowpass2(mount, axis);
                break;
            case GUIDE_ALGORITHM_RESIST_SWITCH:
                *ppAlgorithm = new GuideAlgorithmResistSwitch(mount, axis);
                break;
            case GUIDE_ALGORITHM_GAUSSIAN_PROCESS:
                *ppAlgorithm = MakeGaussianProcessGuideAlgo(mount, axis);
                break;
            case GUIDE_ALGORITHM_ZFILTER:
                *ppAlgorithm = new GuideAlgorithmZFilter(mount, axis);
                break;

            default:
                throw ERROR_INFO("invalid guideAlgorithm");
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
    }

    return error;
}

#ifdef TEST_TRANSFORMS
/*
 * TestTransforms() is a routine to do some sanity checking on the transform routines, which
 * have been a source of a huge number of headaches involving getting the reverse transforms
 * to work
 *
 */
void Mount::TestTransforms()
{
    static bool bTested = false;

    if (!bTested)
    {
        for(int inverted=0;inverted<2;inverted++)
        {
            // we test every 15 degrees, starting at -195 degrees and
            // ending at 375 degrees

            for(int i=-13;i<14;i++)
            {
                double xAngle = ((double)i) * M_PI/12.0;
                double yAngle;

                yAngle = xAngle+M_PI/2.0;

                if (inverted)
                {
                    yAngle -= M_PI;
                }

                // normalize the angles since real callers of setCalibration
                // will get the angle from atan2().

                xAngle = atan2(sin(xAngle), cos(xAngle));
                yAngle = atan2(sin(yAngle), cos(yAngle));

                Debug.Write(wxString::Format("xidx=%.2f, yIdx=%.2f\n", xAngle/M_PI*180.0/15, yAngle/M_PI*180.0/15));

                SetCalibration(xAngle, yAngle, 1.0, 1.0);

                for(int j=-13;j<14;j++)
                {
                    double cameraAngle = ((double)i) * M_PI/12.0;
                    cameraAngle = atan2(sin(cameraAngle), cos(cameraAngle));

                    PHD_Point p0(cos(cameraAngle), sin(cameraAngle));
                    assert(fabs((p0.X*p0.X + p0.Y*p0.Y) - 1.00) < 0.01);

                    double p0Angle = p0.Angle();
                    assert(fabs(cameraAngle - p0Angle) < 0.01);

                    PHD_Point p1, p2;

                    if (TransformCameraCoordinatesToMountCoordinates(p0, p1))
                    {
                        assert(false);
                    }

                    assert(fabs((p1.X*p1.X + p1.Y*p1.Y) - 1.00) < 0.01);

                    double adjustedCameraAngle = cameraAngle - xAngle;

                    double cosAdjustedCameraAngle = cos(adjustedCameraAngle);

                    // check that the X value is correct
                    assert(fabs(cosAdjustedCameraAngle - p1.X) < 0.01);

                    double sinAdjustedCameraAngle = sin(adjustedCameraAngle);

                    if (inverted)
                    {
                        sinAdjustedCameraAngle *= -1;
                    }
                    // and check that the Y value is correct;
                    assert(fabs(sinAdjustedCameraAngle - p1.Y) < 0.01);

                    if (TransformMountCoordinatesToCameraCoordinates(p1, p2))
                    {
                        assert(false);
                    }

                    double p2Angle = p2.Angle();
                    //assert(fabs(p0Angle - p2Angle) < 0.01);
                    assert(fabs((p2.X*p2.X + p2.Y*p2.Y) - 1.00) < 0.01);
                    assert(fabs(p0.X - p2.X) < 0.01);
                    assert(fabs(p0.Y - p2.Y) < 0.01);
                }
            }
        }

        bTested = true;
        ClearCalibration();
    }
}
#endif

Mount::Mount()
{
    m_connected = false;
    m_requestCount = 0;
    m_errorCount = 0;

    m_pYGuideAlgorithm = nullptr;
    m_pXGuideAlgorithm = nullptr;
    m_guidingEnabled = true;

    m_backlashComp = nullptr;
    m_lastStep.mount = this;
    m_lastStep.frameNumber = -1; // invalidate

    ClearCalibration();

#ifdef TEST_TRANSFORMS
    TestTransforms();
#endif
}

Mount::~Mount()
{
    delete m_pXGuideAlgorithm;
    delete m_pYGuideAlgorithm;
    delete m_backlashComp;
}

double Mount::yAngle() const
{
    return norm_angle(m_cal.xAngle - m_yAngleError + M_PI / 2.);
}

static wxString RotAngleStr(double rotAngle)
{
    if (rotAngle == Rotator::POSITION_UNKNOWN)
        return _("None");
    return wxString::Format("%.1f", rotAngle);
}

bool Mount::FlipCalibration()
{
    bool bError = false;

    try
    {
        if (!IsCalibrated())
        {
            throw ERROR_INFO("cannot flip if not calibrated");
        }

        double origX = xAngle();
        double origY = yAngle();

        bool decFlipRequired = CalibrationFlipRequiresDecFlip();

        Debug.Write(wxString::Format("FlipCalibration before: x=%.1f, y=%.1f decFlipRequired=%d sideOfPier=%s rotAngle=%s parity=%s/%s\n",
            degrees(origX), degrees(origY), decFlipRequired, ::PierSideStr(m_cal.pierSide), RotAngleStr(m_cal.rotatorAngle),
            ParityStr(m_cal.raGuideParity), ParityStr(m_cal.decGuideParity)));

        double newX = origX + M_PI;
        double newY = origY;

        if (decFlipRequired)
        {
            newY += M_PI;
        }

        Debug.Write(wxString::Format("FlipCalibration pre-normalize: x=%.1f, y=%.1f\n", degrees(newX), degrees(newY)));

        // normalize
        newX = norm_angle(newX);
        newY = norm_angle(newY);

        PierSide priorPierSide = m_cal.pierSide;
        PierSide newPierSide = OppositeSide(m_cal.pierSide);

        // Dec polarity changes when pier side changes, i.e. if Guide(NORTH) moves the star north on one side,
        // then Guide(NORTH) will move the star south on the other side of the pier.
        // For mounts with CalibrationFlipRequiresDecFlip, the parity does not change after the flip.
        GuideParity newDecParity = decFlipRequired ? m_cal.decGuideParity : OppositeParity(m_cal.decGuideParity);

        Debug.Write(wxString::Format("FlipCalibration after: x=%.1f y=%.1f sideOfPier=%s parity=%s/%s\n",
            degrees(newX), degrees(newY), ::PierSideStr(newPierSide), ParityStr(m_cal.raGuideParity), ParityStr(newDecParity)));

        Calibration cal(m_cal);
        cal.xAngle = newX;
        cal.yAngle = newY;
        cal.pierSide = newPierSide;
        cal.decGuideParity = newDecParity;

        SetCalibration(cal);

        pFrame->StatusMsg(wxString::Format(_("CAL: %s(%.f,%.f)->%s(%.f,%.f)"),
            ::PierSideStrTr(priorPierSide, wxEmptyString), degrees(origX), degrees(origY),
            ::PierSideStrTr(newPierSide, wxEmptyString), degrees(newX), degrees(newY)));
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void Mount::LogGuideStepInfo()
{
    if (m_lastStep.frameNumber < 0)
        return;

    pFrame->UpdateStatusBarGuiderInfo(m_lastStep);
    GuideLog.GuideStep(m_lastStep);
    EvtServer.NotifyGuideStep(m_lastStep);

    if (m_lastStep.moveOptions & MOVEOPT_GRAPH)
    {
        pFrame->pGraphLog->AppendData(m_lastStep);
        pFrame->pTarget->AppendData(m_lastStep);
        GuidingAssistant::NotifyGuideStep(m_lastStep);
    }

    m_lastStep.frameNumber = -1; // invalidate
}

Mount::MOVE_RESULT Mount::MoveOffset(GuiderOffset *ofs, unsigned int moveOptions)
{
    MOVE_RESULT result = MOVE_OK;

    try
    {
        double xDistance, yDistance;

        if (moveOptions & MOVEOPT_ALGO_DEDUCE)
        {
            xDistance = m_pXGuideAlgorithm ? m_pXGuideAlgorithm->deduceResult() : 0.0;
            yDistance = m_pYGuideAlgorithm ? m_pYGuideAlgorithm->deduceResult() : 0.0;
            if (xDistance == 0.0 && yDistance == 0.0)
                return result;
            ofs->mountOfs.SetXY(xDistance, yDistance);

            Debug.Write(wxString::Format("Dead-reckoning move xDistance=%.2f yDistance=%.2f\n",
                xDistance, yDistance));
        }
        else
        {
            // direct move or guide step

            if (!ofs->mountOfs.IsValid())
            {
                if (TransformCameraCoordinatesToMountCoordinates(ofs->cameraOfs, ofs->mountOfs, true))
                {
                    throw ERROR_INFO("Unable to transform camera coordinates");
                }
            }

            xDistance = ofs->mountOfs.X;
            yDistance = ofs->mountOfs.Y;

            Debug.Write(wxString::Format("Moving (%.2f, %.2f) raw xDistance=%.2f yDistance=%.2f\n",
                ofs->cameraOfs.X, ofs->cameraOfs.Y, xDistance, yDistance));

            // Let BLC track the raw offsets in Dec
            if (m_backlashComp)
                m_backlashComp->TrackBLCResults(moveOptions, yDistance);

            if (moveOptions & MOVEOPT_ALGO_RESULT)
            {
                // Feed the raw distances to the guide algorithms
                if (m_pXGuideAlgorithm)
                {
                    xDistance = m_pXGuideAlgorithm->result(xDistance);
                }

                if (m_pYGuideAlgorithm)
                {
                    yDistance = m_pYGuideAlgorithm->result(yDistance);
                }
            }
        }

        // Figure out the guide directions based on the (possibly) updated distances
        GUIDE_DIRECTION xDirection = xDistance > 0.0 ? LEFT : RIGHT;
        GUIDE_DIRECTION yDirection = yDistance > 0.0 ? DOWN : UP;

        int requestedXAmount = ROUND(fabs(xDistance / m_xRate));
        MoveResultInfo xMoveResult;
        result = MoveAxis(xDirection, requestedXAmount, moveOptions, &xMoveResult);

        MoveResultInfo yMoveResult;
        if (result != MOVE_ERROR_SLEWING && result != MOVE_ERROR_AO_LIMIT_REACHED)
        {
            int requestedYAmount = ROUND(fabs(yDistance / m_cal.yRate));

            if (m_backlashComp)
                m_backlashComp->ApplyBacklashComp(moveOptions, yDistance, &requestedYAmount);

            result = MoveAxis(yDirection, requestedYAmount, moveOptions, &yMoveResult);
        }

        // Record the info about the guide step. The info will be picked up back in the main UI thread.
        // We don't want to do anything with the info here in the worker thread since UI operations are
        // not allowed outside the main UI thread.

        GuideStepInfo& info = m_lastStep;

        info.moveOptions = moveOptions;
        info.frameNumber = pFrame->m_frameCounter;
        info.time = pFrame->TimeSinceGuidingStarted();
        info.cameraOffset = ofs->cameraOfs;
        info.mountOffset = ofs->mountOfs;
        info.guideDistanceRA = xDistance;
        info.guideDistanceDec = yDistance;
        info.durationRA = xMoveResult.amountMoved;
        info.directionRA = xDirection;
        info.durationDec = yMoveResult.amountMoved;
        info.directionDec = yDirection;
        info.raLimited = xMoveResult.limited;
        info.decLimited = yMoveResult.limited;
        info.aoPos = GetAoPos();
        const Star& star = pFrame->pGuider->PrimaryStar();
        info.starMass = star.Mass;
        info.starSNR = star.SNR;
        info.starHFD = star.HFD;
        info.avgDist = pFrame->CurrentGuideError();
        info.starError = star.GetError();
    }
    catch (const wxString& errMsg)
    {
        POSSIBLY_UNUSED(errMsg);
        if (result == MOVE_OK)
            result = MOVE_ERROR;
    }

    return result;
}

/*
 * The transform code has proven really tricky to get right.  For future generations
 * (and for me the next time I try to work on it), I'm going to put some notes here.
 *
 * The goal of TransformCameraCoordinatesToMountCoordinates is to transform a camera
 * pixel coordinate into an x and y, and TransformMountCoordinatesToCameraCoordinates
 * does the reverse, converting a mount x and y into pixels coordinates.
 *
 * Note: If a mount's x and y axis are not perfectly perpendicular, the reverse transform
 * will not be able to accurately reverse the forward transform. The amount of inaccuracy
 * depends upon the perpendicular error.
 *
 * The mount is calibrated but moving 1 axis, then the other, watching where the mount
 * started and stopped.  Looking at the coordinates of the start and stop point, we
 * can compute the angle and the speed.
 *
 * The original PHD code used cos() against both the angles, but that code had issues
 * with sign reversal for some of the reverse transforms.  I spent some time looking
 * at that code (OK, a lot of time) and I never managed to get it to work, so I
 * rewrote it.
 *
 * After calibration, we use the x axis angle, and compute the y axis error by
 * subtracting 90 degrees.  If the mount was perfectly perpendicular, the error
 * will either be 0 or 180, depending on whether the axis motion was reversed
 * during calibration.
 *
 * I put this quote in when I was trying to fix the cos/cos code. Hopefully
 * since completely rewrote that code, I understand the new code. But I'm going
 * to leave it here as it still seems relevant, given the amount of trouble
 * I had getting this code to work.
 *
 * In the words of Stevie Wonder, noted Computer Scientist and singer/songwriter
 * of some repute:
 *
 * "When you believe in things that you don't understand
 * Then you suffer
 * Superstition ain't the way"
 *
 * As part of trying to get the transforms to work, I wrote a routine called TestTransforms()
 * which does a bunch of forward/reverse transform pairs, checking the results as it goes.
 *
 * If you ever have change the transform functions, it would be wise to #define TEST_TRANSFORMS
 * to make sure that you (at least) didn't break anything that TestTransforms() checks for.
 *
 */

bool Mount::TransformCameraCoordinatesToMountCoordinates(const PHD_Point& cameraVectorEndpoint,
                                                         PHD_Point& mountVectorEndpoint, bool logged)
{
    bool bError = false;

    try
    {
        if (!cameraVectorEndpoint.IsValid())
        {
            throw ERROR_INFO("invalid cameraVectorEndPoint");
        }

        double hyp   = cameraVectorEndpoint.Distance();
        double cameraTheta = cameraVectorEndpoint.Angle();

        double xAngle = cameraTheta - m_cal.xAngle;                   // xAngle measures RA axis rotation vs camera X axis, positive is CW from x axis
        double yAngle = cameraTheta - (m_cal.xAngle + m_yAngleError); // m_yAngleError is the orthogonality error

        // Convert theta and hyp into X and Y

        mountVectorEndpoint.SetXY(
            cos(xAngle) * hyp,
            sin(yAngle) * hyp
            );

        if (logged)
        {
            Debug.Write(wxString::Format("CameraToMount -- cameraTheta (%.2f) - m_xAngle (%.2f) = xAngle (%.2f = %.2f)\n",
                cameraTheta, m_cal.xAngle, xAngle, norm_angle(xAngle)));
            Debug.Write(wxString::Format("CameraToMount -- cameraTheta (%.2f) - (m_xAngle (%.2f) + m_yAngleError (%.2f)) = yAngle (%.2f = %.2f)\n",
                cameraTheta, m_cal.xAngle, m_yAngleError, yAngle, norm_angle(yAngle)));
            Debug.Write(wxString::Format("CameraToMount -- cameraX=%.2f cameraY=%.2f hyp=%.2f cameraTheta=%.2f mountX=%.2f mountY=%.2f, mountTheta=%.2f\n",
                cameraVectorEndpoint.X, cameraVectorEndpoint.Y, hyp, cameraTheta, mountVectorEndpoint.X, mountVectorEndpoint.Y,
                mountVectorEndpoint.Angle()));
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        mountVectorEndpoint.Invalidate();
    }

    return bError;
}

bool Mount::TransformMountCoordinatesToCameraCoordinates(const PHD_Point& mountVectorEndpoint,
                                                        PHD_Point& cameraVectorEndpoint, bool logged)
{
    bool bError = false;

    try
    {
        if (!mountVectorEndpoint.IsValid())
        {
            throw ERROR_INFO("invalid mountVectorEndPoint");
        }

        double hyp = mountVectorEndpoint.Distance();
        double mountTheta = mountVectorEndpoint.Angle();

        if (fabs(m_yAngleError) > M_PI / 2.)
        {
            mountTheta = -mountTheta;
        }

        double xAngle = mountTheta + m_cal.xAngle;

        cameraVectorEndpoint.SetXY(
                cos(xAngle) * hyp,
                sin(xAngle) * hyp
                );

        if (logged)
        {
            Debug.Write(wxString::Format("MountToCamera -- mountTheta (%.2f) + m_xAngle (%.2f) = xAngle (%.2f = %.2f)\n",
                mountTheta, m_cal.xAngle, xAngle, norm_angle(xAngle)));
            Debug.Write(wxString::Format("MountToCamera -- mountX=%.2f mountY=%.2f hyp=%.2f mountTheta=%.2f cameraX=%.2f, cameraY=%.2f cameraTheta=%.2f\n",
                mountVectorEndpoint.X, mountVectorEndpoint.Y, hyp, mountTheta, cameraVectorEndpoint.X, cameraVectorEndpoint.Y,
                cameraVectorEndpoint.Angle()));
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        cameraVectorEndpoint.Invalidate();
    }

    return bError;
}

GraphControlPane *Mount::GetXGuideAlgorithmControlPane(wxWindow *pParent)
{
    return m_pXGuideAlgorithm->GetGraphControlPane(pParent, _("RA:"));
}

GraphControlPane *Mount::GetYGuideAlgorithmControlPane(wxWindow *pParent)
{
    return m_pYGuideAlgorithm->GetGraphControlPane(pParent, _("DEC:"));
}

GraphControlPane *Mount::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return nullptr;
};

bool Mount::DecCompensationEnabled() const
{
    return false;
}

/*
 * Adjust the calibration data for the scope's current coordinates.
 *
 * This includes adjusting the xRate to compensate for changes in declination
 * relative to the declination where calibration was done, and possibly flipping
 * the calibration data if the mount is known to be on the other side of the
 * pier from where calibration was done.
 */
void Mount::AdjustCalibrationForScopePointing()
{
    double newDeclination = pPointingSource->GetDeclinationRadians();
    PierSide newPierSide = pPointingSource->SideOfPier();
    double newRotatorAngle = Rotator::RotatorPosition();
    unsigned short binning = pCamera->Binning;

    Debug.AddLine(wxString::Format("AdjustCalibrationForScopePointing (%s): current dec=%s pierSide=%d, cal dec=%s pierSide=%d rotAngle=%s bin=%hu",
        GetMountClassName(), DeclinationStr(newDeclination), newPierSide, DeclinationStr(m_cal.declination), m_cal.pierSide,
        RotAngleStr(newRotatorAngle), binning));

    // See if the user has changed mount guide speeds after the last calibration.  If so, raise an alert that can't be avoided
    if (pPointingSource->CanReportPosition())
    {
        CalibrationDetails calDetails;
        LoadCalibrationDetails(&calDetails);

        if (calDetails.raGuideSpeed > 0 && calDetails.decGuideSpeed > 0)
        {
            double currRASpeed;
            double currDecSpeed;
            if (!pPointingSource->GetGuideRates(&currRASpeed, &currDecSpeed))
            {
                if (fabs(1.0 - currRASpeed / calDetails.raGuideSpeed) > 0.05 || fabs(1.0 - currDecSpeed / calDetails.decGuideSpeed) > 0.05)
                {
                    pFrame->Alert(_("Mount guide speeds are different from those used in last calibration.  Do a new calibration or reset mount guide speed settings to previous values. "));
                    Debug.Write(wxString::Format("Guide speeds have changed since calibration.  Orig RA = %0.1f, Orig Dec = %0.1f, "
                        "Curr RA = %0.1f, Curr Dec = %0.1f, Units are arc-sec/sec\n", calDetails.raGuideSpeed * 3600.0, calDetails.decGuideSpeed * 3600.0,
                        currRASpeed * 3600.0, currDecSpeed * 3600.0));
                }
            }
        }
    }

    if (newPierSide != PIER_SIDE_UNKNOWN && m_cal.pierSide == PIER_SIDE_UNKNOWN)
    {
        pFrame->Alert(_("Current calibration did not have side-of-pier information, so PHD2 can't automatically correct for meridian flips. "
            "You should do a fresh calibration to correct this problem."));
    }

    // Compensate for binning or pixel size changes. At least one cam driver (ASCOM/Lodestar) can lie about the binning while changing
    // the reported pixel size
    double scaleAdjustment = 1.0;
    if (fabs(pCamera->GetCameraPixelSize() - GuideCamera::GetProfilePixelSize()) >= 1.0)
    {
        // Punt on this, it probably means the user-supplied pixel size doesn't match what the camera is reporting
        pFrame->Alert(_("Profile pixel size doesn't match camera-reported pixel size.  Re-calibrate to restore correct guiding."));
        Debug.Write(wxString::Format("Camera pixel size changed from %0.1f to %0.1f\n",
            GuideCamera::GetProfilePixelSize(), pCamera->GetCameraPixelSize()));
        scaleAdjustment = pCamera->GetCameraPixelSize() / GuideCamera::GetProfilePixelSize();
        pCamera->SetCameraPixelSize(pCamera->GetCameraPixelSize());
    }

    // The following won't happen unless the camera binning is changed outside the AD UI. Goal is to revert to baseline guiding params
    // while keeping the UI consistent with the apparent image scale
    if (binning != m_cal.binning && m_cal.isValid)
    {
        Calibration cal(m_cal);

        scaleAdjustment *= binning / m_cal.binning;
        double adj = (double)m_cal.binning / (double)binning;
        cal.xRate *= adj;
        cal.yRate *= adj;
        cal.binning = binning;

        Debug.Write(wxString::Format("Binning %hu -> %hu, rates (%.3f, %.3f) -> (%.3f, %.3f)\n",
            m_cal.binning, binning, m_cal.xRate * 1000., m_cal.yRate * 1000., cal.xRate * 1000., cal.yRate * 1000.));

        SetCalibration(cal);
    }

    // If the image scale has changed, make some other adjustments
    if (fabs(scaleAdjustment - 1.0) >= 0.01)
    {
        Debug.Write("Mount::AdjustCalibrationForScopePointing: imageScaleRatio changed\n");

        // Device-specified pixel size and binning will be reflected in AD when LoadValues() occurs
        pFrame->HandleImageScaleChange(scaleAdjustment);          // Revert the guide params, get the various UIs sorted out
        // Force a fresh calibration when guiding is started next
        ClearCalibration();
    }

    if (IsOppositeSide(newPierSide, m_cal.pierSide))
    {
        Debug.AddLine(wxString::Format("Guiding starts on opposite side of pier: calibration data "
            "side is %s, current side is %s", ::PierSideStr(m_cal.pierSide), ::PierSideStr(newPierSide)));
        FlipCalibration();
    }

    if (newRotatorAngle != Rotator::POSITION_UNKNOWN)
    {
        if (m_cal.rotatorAngle == Rotator::POSITION_UNKNOWN)
        {
            // we do not know the rotator position at calibration time so cannot automatically adjust calibration
            pFrame->Alert(_("Rotator position has changed, recalibration is needed."));
            m_cal.rotatorAngle = newRotatorAngle;
        }
        else
        {
            double da = newRotatorAngle - m_cal.rotatorAngle;

            if (fabs(da) > 0.05)
            {
                Debug.Write(wxString::Format("New rotator position %.1f deg, prev = %.1f deg, delta = %.1f deg\n", newRotatorAngle, m_cal.rotatorAngle, da));

                da = radians(da);

                Calibration cal(m_cal);
                cal.xAngle = norm_angle(cal.xAngle - da);
                cal.yAngle = norm_angle(cal.yAngle - da);
                cal.rotatorAngle = newRotatorAngle;

                SetCalibration(cal);
            }
        }
    }

    // compensate RA guide rate for declination if the declination changed and we know both the
    // calibration declination and the current declination.  This must be done after all other adjustments that may
    // update the calibration data in the registry.  The adjusted x_Rate is never persisted
    bool deccomp = false;

    if (newDeclination != m_cal.declination &&
        newDeclination != UNKNOWN_DECLINATION && m_cal.declination != UNKNOWN_DECLINATION)
    {
        // avoid division by zero and gross errors.  If the user didn't calibrate
        // somewhere near the celestial equator, we don't do this
        if (fabs(m_cal.declination) > Scope::DEC_COMP_LIMIT)
        {
            Debug.AddLine("skipping Dec comp: initial calibration too far from equator");
            pFrame->Alert(_("Calibration was too far from equator, recalibration is needed."));
        }
        else if (!DecCompensationEnabled())
        {
            Debug.AddLine("skipping Dec comp: Dec Comp not enabled");
        }
        else
        {
            // Don't do a full dec comp too close to pole - xRate will become a huge number and will cause problems downstream
            newDeclination = wxMax(radians(-89.0), wxMin(radians(89.0), newDeclination));
            m_xRate = (m_cal.xRate / cos(m_cal.declination)) * cos(newDeclination);
            deccomp = true;

            Debug.Write(wxString::Format("Dec comp: XRate %.3f -> %.3f for dec %.1f -> dec %.1f\n",
                m_cal.xRate * 1000.0, m_xRate * 1000.0, degrees(m_cal.declination), degrees(newDeclination)));
        }
    }
    if (!deccomp && m_xRate != m_cal.xRate)
    {
        Debug.Write(wxString::Format("No dec comp, asserted base xRate %.3f\n", m_cal.xRate * 1000.0));
        m_xRate = m_cal.xRate;
    }
}

void Mount::IncrementRequestCount()
{
    m_requestCount++;

    // for the moment we never enqueue requests if the mount is busy, but we can
    // enqueue them two at a time.  There is no reason we can't, it's just that
    // right now we don't, and this might catch an error
    assert(m_requestCount <= 2);
}

void Mount::DecrementRequestCount()
{
    assert(m_requestCount > 0);
    m_requestCount--;
}

bool Mount::HasNonGuiMove()
{
    return false;
}

bool Mount::SynchronousOnly()
{
    return false;
}

bool Mount::HasSetupDialog() const
{
    return false;
}

void Mount::SetupDialog()
{
}

const wxString& Mount::Name() const
{
    return m_Name;
}

bool Mount::IsStepGuider() const
{
    return false;
}

wxPoint Mount::GetAoPos() const
{
    return wxPoint();
}

wxPoint Mount::GetAoMaxPos() const
{
    return wxPoint();
}

const char *Mount::DirectionStr(GUIDE_DIRECTION d) const
{
    // these are used internally in the guide log and event server and are not translated
    switch (d) {
    case NONE:  return "None";
    case NORTH: return "North";
    case SOUTH: return "South";
    case EAST:  return "East";
    case WEST:  return "West";
    default:    return "?";
    }
}

const char *Mount::DirectionChar(GUIDE_DIRECTION d) const
{
    // these are used internally in the guide log and event server and are not translated
    switch (d) {
    case NONE:  return "-";
    case NORTH: return "N";
    case SOUTH: return "S";
    case EAST:  return "E";
    case WEST:  return "W";
    default:    return "?";
    }
}

wxString DumpMoveOptionBits(unsigned int moveOptions)
{
    if (!moveOptions)
        return "-";
    char buf[16];
    char *p = &buf[0];
    if (moveOptions & MOVEOPT_ALGO_RESULT)
        *p++ = 'A';
    if (moveOptions & MOVEOPT_ALGO_DEDUCE)
        *p++ = 'D';
    if (moveOptions & MOVEOPT_USE_BLC)
        *p++ = 'B';
    if (moveOptions & MOVEOPT_GRAPH)
        *p++ = 'G';
    if (moveOptions & MOVEOPT_MANUAL)
        *p++ = 'M';
    *p = 0;
    return buf;
}

bool Mount::IsCalibrated() const
{
    bool bReturn = false;

    if (IsConnected())
    {
        bReturn = m_calibrated;
    }

    return bReturn;
}

void Mount::ClearCalibration()
{
    m_calibrated = false;
    if (pFrame)
        pFrame->UpdateStatusBarCalibrationStatus();
}

void Mount::SetCalibration(const Calibration& cal)
{
    Debug.Write(wxString::Format("Mount::SetCalibration (%s) -- xAngle=%.1f yAngle=%.1f xRate=%.3f yRate=%.3f bin=%hu dec=%s pierSide=%d par=%s/%s rotAng=%s\n",
        GetMountClassName(), degrees(cal.xAngle), degrees(cal.yAngle), cal.xRate * 1000.0, cal.yRate * 1000.0, cal.binning,
        DeclinationStr(cal.declination), cal.pierSide, ParityStr(cal.raGuideParity), ParityStr(cal.decGuideParity),
        RotAngleStr(cal.rotatorAngle)));

    // we do the rates first, since they just get stored
    m_cal.xRate = cal.xRate;
    m_cal.yRate = cal.yRate;
    m_cal.binning = cal.binning;
    m_cal.declination = cal.declination;
    m_cal.pierSide = cal.pierSide;
    if (cal.raGuideParity != GUIDE_PARITY_UNCHANGED)
        m_cal.raGuideParity = cal.raGuideParity;
    if (cal.decGuideParity != GUIDE_PARITY_UNCHANGED)
        m_cal.decGuideParity = cal.decGuideParity;
    m_cal.rotatorAngle = cal.rotatorAngle;
    m_cal.isValid = true;

    m_xRate  = cal.xRate;

    // the angles are more difficult because we have to turn yAngle into a yError.
    m_cal.xAngle = cal.xAngle;
    m_cal.yAngle = cal.yAngle;
    m_yAngleError = norm_angle(cal.xAngle - cal.yAngle + M_PI / 2.);

    Debug.AddLine(wxString::Format("Mount::SetCalibration (%s) -- sets m_xAngle=%.1f m_yAngleError=%.1f",
        GetMountClassName(), degrees(m_cal.xAngle), degrees(m_yAngleError)));

    m_calibrated = true;

    if (pFrame)
        pFrame->UpdateStatusBarCalibrationStatus();

    // store calibration data
    wxString prefix = "/" + GetMountClassName() + "/calibration/";
    pConfig->Profile.SetString(prefix + "timestamp", wxDateTime::Now().Format());
    pConfig->Profile.SetDouble(prefix + "xAngle", m_cal.xAngle);
    pConfig->Profile.SetDouble(prefix + "yAngle", m_cal.yAngle);
    pConfig->Profile.SetDouble(prefix + "xRate", m_cal.xRate);
    pConfig->Profile.SetDouble(prefix + "yRate", m_cal.yRate);
    pConfig->Profile.SetInt(prefix + "binning", m_cal.binning);
    pConfig->Profile.SetDouble(prefix + "declination", m_cal.declination);
    pConfig->Profile.SetInt(prefix + "pierSide", m_cal.pierSide);
    pConfig->Profile.SetInt(prefix + "raGuideParity", m_cal.raGuideParity);
    pConfig->Profile.SetInt(prefix + "decGuideParity", m_cal.decGuideParity);
    pConfig->Profile.SetDouble(prefix + "rotatorAngle", m_cal.rotatorAngle);
}

void Mount::SaveCalibrationDetails(const CalibrationDetails& calDetails) const
{
    wxString prefix = "/" + GetMountClassName() + "/calibration/";
    wxString stepStr = "";
    bool guideSpeedsOk = true;

    pConfig->Profile.SetInt(prefix + "focal_length", calDetails.focalLength);
    pConfig->Profile.SetDouble(prefix + "image_scale", calDetails.imageScale);
    if (pPointingSource)
    {
        if (!(calDetails.raGuideSpeed == -1.0 && calDetails.decGuideSpeed == -1.0))
        {
            if (!pPointingSource->ValidGuideRates(calDetails.raGuideSpeed, calDetails.decGuideSpeed))
            {
                guideSpeedsOk = false;
            }
        }
    }
    if (guideSpeedsOk)
    {
        pConfig->Profile.SetDouble(prefix + "ra_guide_rate", calDetails.raGuideSpeed);
        pConfig->Profile.SetDouble(prefix + "dec_guide_rate", calDetails.decGuideSpeed);
    }
    else
    {
        pConfig->Profile.SetDouble(prefix + "ra_guide_rate", -1.0);
        pConfig->Profile.SetDouble(prefix + "dec_guide_rate", -1.0);
        Debug.Write("Bogus guide speeds over-written in SaveCalibrationDetails\n");
    }
    pConfig->Profile.SetDouble(prefix + "ortho_error", calDetails.orthoError);
    pConfig->Profile.SetDouble(prefix + "orig_binning", calDetails.origBinning);
    pConfig->Profile.SetString(prefix + "orig_timestamp", calDetails.origTimestamp);
    pConfig->Profile.SetInt(prefix + "orig_pierside", calDetails.origPierSide);

    for (std::vector<wxRealPoint>::const_iterator it = calDetails.raSteps.begin(); it != calDetails.raSteps.end(); ++it)
    {
        stepStr += wxString::Format("{%0.1f %0.1f}, ", it->x, it->y);
    }
    stepStr = stepStr.Left(stepStr.length() - 2);
    pConfig->Profile.SetString(prefix + "ra_steps", stepStr);

    stepStr = "";

    for (std::vector<wxRealPoint>::const_iterator it = calDetails.decSteps.begin(); it != calDetails.decSteps.end(); ++it)
    {
        stepStr += wxString::Format("{%0.1f %0.1f}, ", it->x, it->y);
    }
    stepStr = stepStr.Left(stepStr.length() - 2);
    pConfig->Profile.SetString(prefix + "dec_steps", stepStr);

    pConfig->Profile.SetInt(prefix + "ra_step_count", calDetails.raStepCount);
    pConfig->Profile.SetInt(prefix + "dec_step_count", calDetails.decStepCount);
    pConfig->Profile.SetInt(prefix + "last_issue", (int)calDetails.lastIssue);
}

inline static PierSide pier_side(int val)
{
    return val == PIER_SIDE_EAST ? PIER_SIDE_EAST : val == PIER_SIDE_WEST ? PIER_SIDE_WEST : PIER_SIDE_UNKNOWN;
}

inline static GuideParity guide_parity(int val)
{
    switch (val) {
    case GUIDE_PARITY_EVEN: return GUIDE_PARITY_EVEN;
    case GUIDE_PARITY_ODD: return GUIDE_PARITY_ODD;
    default: return GUIDE_PARITY_UNKNOWN;
    }
}

void Mount::NotifyGuidingStarted()
{
    Debug.Write("Mount: notify guiding started\n");

    if (m_pXGuideAlgorithm)
        m_pXGuideAlgorithm->GuidingStarted();

    if (m_pYGuideAlgorithm)
        m_pYGuideAlgorithm->GuidingStarted();
}

void Mount::NotifyGuidingStopped()
{
    Debug.Write("Mount: notify guiding stopped\n");

    if (m_pXGuideAlgorithm)
        m_pXGuideAlgorithm->GuidingStopped();

    if (m_pYGuideAlgorithm)
        m_pYGuideAlgorithm->GuidingStopped();

    if (m_backlashComp)
        m_backlashComp->ResetBLCState();
}

void Mount::NotifyGuidingPaused()
{
    Debug.Write("Mount: notify guiding paused\n");

    if (m_pXGuideAlgorithm)
        m_pXGuideAlgorithm->GuidingPaused();

    if (m_pYGuideAlgorithm)
        m_pYGuideAlgorithm->GuidingPaused();
}

void Mount::NotifyGuidingResumed()
{
    Debug.Write("Mount: notify guiding resumed\n");

    if (m_pXGuideAlgorithm)
        m_pXGuideAlgorithm->GuidingResumed();

    if (m_pYGuideAlgorithm)
        m_pYGuideAlgorithm->GuidingResumed();
}

void Mount::NotifyGuidingDithered(double dx, double dy, bool mountCoords)
{
    Debug.Write(wxString::Format("Mount: notify guiding dithered (%.1f, %.1f)\n", dx, dy));

    if (!mountCoords)
    {
        PHD_Point cam(dx, dy);
        PHD_Point mnt;
        bool err = TransformCameraCoordinatesToMountCoordinates(cam, mnt, false);
        if (!err)
        {
            dx = mnt.X;
            dy = mnt.Y;
        }
    }

    if (m_pXGuideAlgorithm)
        m_pXGuideAlgorithm->GuidingDithered(dx);

    if (m_pYGuideAlgorithm)
        m_pYGuideAlgorithm->GuidingDithered(dy);
}

void Mount::NotifyGuidingDitherSettleDone(bool success)
{
    Debug.Write(wxString::Format("Mount: notify guiding dither settle done success=%d\n", success));
    if (m_pXGuideAlgorithm)
        m_pXGuideAlgorithm->GuidingDitherSettleDone(success);
    if (m_pYGuideAlgorithm)
        m_pYGuideAlgorithm->GuidingDitherSettleDone(success);
}

void Mount::NotifyDirectMove(const PHD_Point& dist)
{
    Debug.Write(wxString::Format("Mount: notify direct move %.2f,%.2f\n", dist.X, dist.Y));
    if (m_pXGuideAlgorithm)
        m_pXGuideAlgorithm->DirectMoveApplied(dist.X);
    if (m_pYGuideAlgorithm)
        m_pYGuideAlgorithm->DirectMoveApplied(dist.Y);
}

void Mount::GetLastCalibration(Calibration *cal) const
{
    wxString prefix = "/" + GetMountClassName() + "/calibration/";
    wxString timestamp = pConfig->Profile.GetString(prefix + "timestamp", wxEmptyString);
    if (!timestamp.empty())
    {
        cal->xRate = pConfig->Profile.GetDouble(prefix + "xRate", 1.0);
        cal->yRate = pConfig->Profile.GetDouble(prefix + "yRate", 1.0);
        cal->binning = (unsigned short) pConfig->Profile.GetInt(prefix + "binning", 1);
        cal->xAngle = pConfig->Profile.GetDouble(prefix + "xAngle", 0.0);
        cal->yAngle = pConfig->Profile.GetDouble(prefix + "yAngle", 0.0);
        cal->declination = pConfig->Profile.GetDouble(prefix + "declination", 0.0);
        cal->pierSide = pier_side(pConfig->Profile.GetInt(prefix + "pierSide", PIER_SIDE_UNKNOWN));
        cal->raGuideParity = guide_parity(pConfig->Profile.GetInt(prefix + "raGuideParity", GUIDE_PARITY_UNKNOWN));
        cal->decGuideParity = guide_parity(pConfig->Profile.GetInt(prefix + "decGuideParity", GUIDE_PARITY_UNKNOWN));
        cal->rotatorAngle = pConfig->Profile.GetDouble(prefix + "rotatorAngle", Rotator::POSITION_UNKNOWN);
        cal->timestamp = timestamp;
        cal->isValid = true;
    }
    else
    {
        cal->isValid = false;
    }
}

void Mount::LoadCalibrationDetails(CalibrationDetails *details) const
{
    wxString prefix = "/" + GetMountClassName() + "/calibration/";

    details->focalLength = pConfig->Profile.GetInt(prefix + "focal_length", 0);
    details->imageScale = pConfig->Profile.GetDouble(prefix + "image_scale", 1.0);
    details->raGuideSpeed = pConfig->Profile.GetDouble(prefix + "ra_guide_rate", -1.0);
    details->decGuideSpeed = pConfig->Profile.GetDouble(prefix + "dec_guide_rate", -1.0);
    details->orthoError = pConfig->Profile.GetDouble(prefix + "ortho_error", 0.0);
    details->raStepCount = pConfig->Profile.GetInt(prefix + "ra_step_count", 0);
    details->decStepCount = pConfig->Profile.GetInt(prefix + "dec_step_count", 0);
    details->origBinning = pConfig->Profile.GetDouble(prefix + "orig_binning", 1.0);
    details->lastIssue = (CalibrationIssueType) pConfig->Profile.GetInt(prefix + "last_issue", 0);
    details->origTimestamp = pConfig->Profile.GetString(prefix + "orig_timestamp", "Unknown");
    details->origPierSide = pier_side(pConfig->Profile.GetInt(prefix + "orig_pierside", PIER_SIDE_UNKNOWN));
    if (pPointingSource && !pPointingSource->m_CalDetailsValidated)
    {
        if (!(details->raGuideSpeed == -1.0 || details->decGuideSpeed == -1.0))
        {
            if (!pPointingSource->ValidGuideRates(details->raGuideSpeed, details->decGuideSpeed))
            {
                details->raGuideSpeed = -1.0;
                details->decGuideSpeed = -1.0;
                pConfig->Profile.SetDouble(prefix + "ra_guide_rate", -1.0);
                pConfig->Profile.SetDouble(prefix + "dec_guide_rate", -1.0);
                // Need to prevent old bogus values in registry from being propagated
                Debug.Write("Bogus guide speeds cleared in LoadCalibrationDetails\n");
            }
        }
    }

    // Populate raSteps
    wxString stepStr = pConfig->Profile.GetString(prefix + "ra_steps", "");
    wxStringTokenizer tok;
    tok.SetString(stepStr, "},", wxTOKEN_STRTOK);
    details->raSteps.clear();
    bool err = false;
    while (tok.HasMoreTokens() && !err)
    {
        wxString tk = (tok.GetNextToken()).Trim(false);           // looks like {x y, left-trimmed
        int blankLoc = tk.find(" ");
        double x;
        double y;
        if (!tk.Mid(1, blankLoc-1).ToDouble(&x))
            err = true;
        tk = tk.Mid(blankLoc + 1);
        if (!tk.ToDouble(&y))
            err = true;
        if (!err)
            details->raSteps.push_back(wxRealPoint(x, y));
    }
    // Do the same for decSteps
    stepStr = pConfig->Profile.GetString(prefix + "dec_steps", "");
    tok.SetString(stepStr, "},", wxTOKEN_STRTOK);
    details->decSteps.clear();
    while (tok.HasMoreTokens() && !err)
    {
        wxString tk = (tok.GetNextToken()).Trim(false);
        int blankLoc = tk.find(" ");
        double x;
        double y;
        if (!tk.Mid(1, blankLoc - 1).ToDouble(&x))
            err = true;
        tk = tk.Mid(blankLoc + 1);
        if (!tk.ToDouble(&y))
            err = true;
        if (!err)
            details->decSteps.push_back(wxRealPoint(x, y));
    }
}

bool Mount::Connect()
{
    m_connected = true;
    ResetErrorCount();

    if (pFrame)
        pFrame->UpdateStatusBarCalibrationStatus();

    return false;
}

bool Mount::Disconnect()
{
    m_connected = false;

    if (pFrame)
        pFrame->UpdateStatusBarCalibrationStatus();

    return false;
}

inline static wxString OrthoErrorStr(const CalibrationDetails& det)
{
    return det.IsValid() ?
        wxString::Format("%.1f deg", det.orthoError) : "unknown";
}

wxString Mount::GetSettingsSummary() const
{
    // return a loggable summary of current mount settings

    wxString s =
        wxString::Format("%s = %s,%s connected, guiding %s, ",
                         IsStepGuider() ? "AO" : "Mount",
                         m_Name,
                         IsConnected() ? "" : " not",
                         m_guidingEnabled ? "enabled" : "disabled");

    if (pPointingSource && pPointingSource->IsConnected() &&
        pPointingSource->CanReportPosition() &&
        pPointingSource != TheScope())
    {
        s += wxString::Format("AuxMount=%s, ", pPointingSource->Name());
    }

    if (IsCalibrated())
    {
        double xRatePx = xRate() * 1000.0;
        double yRatePx = yRate() * 1000.0;

        s += wxString::Format("xAngle = %.1f, xRate = %.3f, "
                              "yAngle = %.1f, yRate = %.3f, "
                              "parity = %s/%s\n",
                              degrees(xAngle()), xRatePx,
                              degrees(yAngle()), yRatePx,
                              ParityStr(m_cal.raGuideParity), ParityStr(m_cal.decGuideParity));

        CalibrationDetails det;
        LoadCalibrationDetails(&det);

        double scale = det.IsValid() ? det.imageScale : 1.0;

        if (IsStepGuider())
        {
            s += wxString::Format("Norm rates X = %.f mas/step, "
                                  "Y = %.f mas/step; ortho.err. = %s\n",
                                  m_cal.xRate * 1000. * scale,
                                  m_cal.yRate * 1000. * scale,
                                  OrthoErrorStr(det));
        }
        else
        {
            double xRateAsD0;
            if (m_cal.declination != UNKNOWN_DECLINATION)
                xRateAsD0 = m_cal.xRate * 1000. / cos(m_cal.declination) * scale;
            else
                xRateAsD0 = m_cal.xRate * 1000. * scale;
            double yRateAs = yRatePx * scale;

            s += wxString::Format("Norm rates RA = %.1f\"/s @ dec 0, "
                                  "Dec = %.1f\"/s; ortho.err. = %s\n",
                                  xRateAsD0,
                                  yRateAs,
                                  OrthoErrorStr(det));
        }
    }
    else
        s += "not calibrated\n";

    s += wxString::Format("X guide algorithm = %s, %s",
                          GuideAlgorithmName(GetXGuideAlgorithmSelection()),
                          m_pXGuideAlgorithm->GetSettingsSummary()) +
        wxString::Format("Y guide algorithm = %s, %s",
                         GuideAlgorithmName(GetYGuideAlgorithmSelection()),
                         m_pYGuideAlgorithm->GetSettingsSummary());

    if (m_backlashComp)
    {
        s += wxString::Format("Backlash comp = %s, pulse = %d ms\n",
            m_backlashComp->IsEnabled() ? "enabled" : "disabled",
            m_backlashComp->GetBacklashPulseWidth());
    }

    return s;
}

bool Mount::CalibrationFlipRequiresDecFlip()
{
    return false;
}

void Mount::StartDecDrift()
{
}

void Mount::EndDecDrift()
{
}

bool Mount::IsDecDrifting() const
{
    return false;
}
