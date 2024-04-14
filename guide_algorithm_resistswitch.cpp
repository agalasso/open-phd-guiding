/*
*  guide_algorithm_resistswitch.cpp
*  PHD Guiding
*
*  Created by Bret McKee
*  Copyright (c) 2012 Bret McKee
*  All rights reserved.
*
*  Based upon work by Craig Stark.
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

static const double DefaultMinMove = 0.2;
static const double DefaultAggression = 1.0;

GuideAlgorithmResistSwitch::GuideAlgorithmResistSwitch(Mount *pMount, GuideAxis axis)
    : GuideAlgorithm(pMount, axis)
{
    double minMove  = pConfig->Profile.GetDouble(GetConfigPath() + "/minMove", DefaultMinMove);
    SetMinMove(minMove);

    double aggr = pConfig->Profile.GetDouble(GetConfigPath() + "/aggression", DefaultAggression);
    SetAggression(aggr);

    bool enable = pConfig->Profile.GetBoolean(GetConfigPath() + "/fastSwitch", true);
    SetFastSwitchEnabled(enable);

    reset();
}

GuideAlgorithmResistSwitch::~GuideAlgorithmResistSwitch(void)
{
}

GUIDE_ALGORITHM GuideAlgorithmResistSwitch::Algorithm() const
{
    return GUIDE_ALGORITHM_RESIST_SWITCH;
}
void GuideAlgorithmResistSwitch::reset(void)
{
    m_history.Empty();

    while (m_history.GetCount() < HISTORY_SIZE)
    {
        m_history.Add(0.0);
    }

    m_currentSide = 0;
}

static int sign(double x)
{
    int iReturn = 0;

    if (x > 0.0)
    {
        iReturn = 1;
    }
    else if (x < 0.0)
    {
        iReturn = -1;
    }

    return iReturn;
}

double GuideAlgorithmResistSwitch::result(double input)
{
    double rslt = input;

    m_history.Add(input);
    m_history.RemoveAt(0);

    try
    {
        if (fabs(input) < m_minMove)
        {
            throw THROW_INFO("input < m_minMove");
        }

        if (m_fastSwitchEnabled)
        {
            double thresh = 3.0 * m_minMove;
            if (sign(input) != m_currentSide && fabs(input) > thresh)
            {
                Debug.Write(wxString::Format("resist switch: large excursion: input %.2f thresh %.2f direction from %d to %d\n", input, thresh, m_currentSide, sign(input)));
                // force switch
                m_currentSide = 0;
                unsigned int i;
                for (i = 0; i < HISTORY_SIZE - 3; i++)
                    m_history[i] = 0.0;
                for (; i < HISTORY_SIZE; i++)
                    m_history[i] = input;
            }
        }

        int decHistory = 0;

        for (unsigned int i = 0; i < m_history.GetCount(); i++)
        {
            if (fabs(m_history[i]) > m_minMove)
            {
                decHistory += sign(m_history[i]);
            }
        }

        if (m_currentSide == 0 || sign(m_currentSide) == -sign(decHistory))
        {
            if (abs(decHistory) < 3)
            {
                throw THROW_INFO("not compelling enough");
            }

            double oldest = 0.0;
            double newest = 0.0;

            for (int i = 0; i < 3; i++)
            {
                oldest += m_history[i];
                newest += m_history[m_history.GetCount() - (i + 1)];
            }

            if (fabs(newest) <= fabs(oldest))
            {
                throw THROW_INFO("Not getting worse");
            }

            Debug.Write(wxString::Format("switching direction from %d to %d - decHistory=%d oldest=%.2f newest=%.2f\n", m_currentSide, sign(decHistory), decHistory, oldest, newest));

            m_currentSide = sign(decHistory);
        }

        if (m_currentSide != sign(input))
        {
            throw THROW_INFO("must have overshot -- vetoing move");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        rslt = 0.0;
    }

    rslt *= m_aggression;

    Debug.Write(wxString::Format("GuideAlgorithmResistSwitch::result() returns %.2f from input %.2f\n", rslt, input));

    return rslt;
}

bool GuideAlgorithmResistSwitch::SetMinMove(double minMove)
{
    bool bError = false;

    try
    {
        if (minMove <= 0.0)
        {
            throw ERROR_INFO("invalid minMove");
        }

        m_minMove = minMove;
        m_currentSide = 0;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_minMove = DefaultMinMove;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/minMove", m_minMove);

    Debug.Write(wxString::Format("GuideAlgorithmResistSwitch::SetMinMove() returns %d, m_minMove=%.2f\n", bError, m_minMove));

    return bError;
}

bool GuideAlgorithmResistSwitch::SetAggression(double aggr)
{
    bool bError = false;

    try
    {
        if (aggr < 0.0 || aggr > 1.0)
        {
            throw ERROR_INFO("invalid aggression");
        }

        m_aggression = aggr;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_aggression = DefaultAggression;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/aggression", m_aggression);

    Debug.Write(wxString::Format("GuideAlgorithmResistSwitch::SetAggression() returns %d, m_aggression=%.2f\n", bError, m_aggression));

    return bError;
}

void GuideAlgorithmResistSwitch::SetFastSwitchEnabled(bool enable)
{
    m_fastSwitchEnabled = enable;
    pConfig->Profile.SetBoolean(GetConfigPath() + "/fastSwitch", m_fastSwitchEnabled);
    Debug.Write(wxString::Format("GuideAlgorithmResistSwitch::SetFastSwitchEnabled(%d)\n", m_fastSwitchEnabled));
}

void GuideAlgorithmResistSwitch::GetParamNames(wxArrayString& names) const
{
    names.push_back("minMove");
    names.push_back("fastSwitch");
    names.push_back("aggression");
}

bool GuideAlgorithmResistSwitch::GetParam(const wxString& name, double *val) const
{
    bool ok = true;

    if (name == "minMove")
        *val = GetMinMove();
    else if (name == "fastSwitch")
        *val = GetFastSwitchEnabled() ? 1.0 : 0.0;
    else if (name == "aggression")
        *val = GetAggression();
    else
        ok = false;

    return ok;
}

bool GuideAlgorithmResistSwitch::SetParam(const wxString& name, double val)
{
    bool err;

    if (name == "minMove")
        err = SetMinMove(val);
    else if (name == "fastSwitch") {
        SetFastSwitchEnabled(val != 0.0);
        err = false;
    }
    else if (name == "aggression")
        err = SetAggression(val);
    else
        err = true;

    return !err;
}

wxString GuideAlgorithmResistSwitch::GetSettingsSummary() const
{
    // return a loggable summary of current mount settings
    return wxString::Format("Minimum move = %.3f Aggression = %.f%% FastSwitch = %s\n",
        GetMinMove(), GetAggression() * 100.0, GetFastSwitchEnabled() ? "enabled" : "disabled");
}

ConfigDialogPane *GuideAlgorithmResistSwitch::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuideAlgorithmResistSwitchConfigDialogPane(pParent, this);
}

GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchConfigDialogPane::
    GuideAlgorithmResistSwitchConfigDialogPane(wxWindow *pParent, GuideAlgorithmResistSwitch *pGuideAlgorithm)
    : ConfigDialogPane(_("ResistSwitch Guide Algorithm"), pParent)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    width = StringWidth(_T("000"));
    m_pAggression = pFrame->MakeSpinCtrl(pParent, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 1.0, 100.0, 100.0, _T("Aggressiveness"));

    DoAdd(_("Aggressiveness"), m_pAggression,
        wxString::Format(_("What percentage of the computed correction should be applied? Default = %.f%%"), DefaultAggression * 100.0));

    width = StringWidth(_T("00.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);

    DoAdd(_("Minimum Move (pixels)"), m_pMinMove,
        wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
        "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));

    m_pFastSwitch = new wxCheckBox(pParent, wxID_ANY, _("Fast switch for large deflections"));
    DoAdd(m_pFastSwitch, _("Ordinarily the Resist Switch algortithm waits several frames before switching direction. With Fast Switch enabled PHD2 will switch direction immediately if it sees a very large deflection. Enable this option if your mount has a substantial amount of backlash and PHD2 sometimes overcorrects."));
}

GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchConfigDialogPane::
    ~GuideAlgorithmResistSwitchConfigDialogPane(void)
{
}

void GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchConfigDialogPane::
    LoadValues(void)
{
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());
    m_pAggression->SetValue(m_pGuideAlgorithm->GetAggression() * 100.0);
    m_pFastSwitch->SetValue(m_pGuideAlgorithm->GetFastSwitchEnabled());
}

void GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchConfigDialogPane::
    UnloadValues(void)
{
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
    m_pGuideAlgorithm->SetAggression(m_pAggression->GetValue() / 100.0);
    m_pGuideAlgorithm->SetFastSwitchEnabled(m_pFastSwitch->GetValue());
}

void GuideAlgorithmResistSwitch::
GuideAlgorithmResistSwitchConfigDialogPane::OnImageScaleChange()
{
    GuideAlgorithm::AdjustMinMoveSpinCtrl(m_pMinMove);
}
void GuideAlgorithmResistSwitch::
GuideAlgorithmResistSwitchConfigDialogPane::EnableDecControls(bool enable)
{
    m_pAggression->Enable(enable);
    m_pMinMove->Enable(enable);
    m_pFastSwitch->Enable(enable);
}

GraphControlPane *GuideAlgorithmResistSwitch::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return new GuideAlgorithmResistSwitchGraphControlPane(pParent, this, label);
}

GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchGraphControlPane::
    GuideAlgorithmResistSwitchGraphControlPane(wxWindow *pParent, GuideAlgorithmResistSwitch *pGuideAlgorithm, const wxString& label)
    : GraphControlPane(pParent, label)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    // Aggression
    width = StringWidth(_T("000"));
    m_pAggression = pFrame->MakeSpinCtrl(this, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 1.0, 100.0, 100.0, _T("Aggressiveness"));
    m_pAggression->SetToolTip(wxString::Format(_("What percentage of the computed correction should be applied? Default = %.f%%"), DefaultAggression * 100.0));
    m_pAggression->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GuideAlgorithmResistSwitch::GuideAlgorithmResistSwitchGraphControlPane::OnAggressionSpinCtrl, this);
    DoAdd(m_pAggression, _T("Agr"));
    m_pAggression->SetValue(m_pGuideAlgorithm->GetAggression() * 100.0);

    // Min move
    width = StringWidth(_T("00.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);
    m_pMinMove->SetToolTip(wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
        "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));
    m_pMinMove->Bind(wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, &GuideAlgorithmResistSwitch::GuideAlgorithmResistSwitchGraphControlPane::OnMinMoveSpinCtrlDouble, this);
    DoAdd(m_pMinMove, _("MnMo"));
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());

    if (TheScope() != nullptr && pGuideAlgorithm->GetAxis() == "DEC")
    {
        DEC_GUIDE_MODE currDecGuideMode = TheScope()->GetDecGuideMode();
        m_pAggression->Enable(currDecGuideMode != DEC_NONE);
        m_pMinMove->Enable(currDecGuideMode != DEC_NONE);
    }
}

GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchGraphControlPane::
    ~GuideAlgorithmResistSwitchGraphControlPane(void)
{
}

void GuideAlgorithmResistSwitch::
GuideAlgorithmResistSwitchGraphControlPane::EnableDecControls(bool enable)
{
    m_pAggression->Enable(enable);
    m_pMinMove->Enable(enable);
}

void GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchGraphControlPane::
    OnMinMoveSpinCtrlDouble(wxSpinDoubleEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Resist switch minimum move", m_pMinMove->GetValue());
}

void GuideAlgorithmResistSwitch::
    GuideAlgorithmResistSwitchGraphControlPane::
    OnAggressionSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetAggression(m_pAggression->GetValue() / 100.0);
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Resist switch aggression", m_pAggression->GetValue());
}
