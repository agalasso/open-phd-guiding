/*
 *  guide_algorithm_ra.cpp
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

static const double DefaultMinMove    = 0.2;
static const double DefaultHysteresis = 0.1;
static const double DefaultAggression = 0.7;
static const double MaxAggression = 2.0;
static const double MaxHysteresis = 0.99;

GuideAlgorithmHysteresis::GuideAlgorithmHysteresis(Mount *pMount, GuideAxis axis)
    : GuideAlgorithm(pMount, axis)
{
    wxString configPath = GetConfigPath();

    double minMove    = pConfig->Profile.GetDouble(configPath + "/minMove", DefaultMinMove);
    SetMinMove(minMove);

    double hysteresis = pConfig->Profile.GetDouble(configPath + "/hysteresis", DefaultHysteresis);
    SetHysteresis(hysteresis);

    double aggression = pConfig->Profile.GetDouble(configPath + "/aggression", DefaultAggression);
    SetAggression(aggression);

    reset();
}

GuideAlgorithmHysteresis::~GuideAlgorithmHysteresis(void)
{
}

GUIDE_ALGORITHM GuideAlgorithmHysteresis::Algorithm() const
{
    return GUIDE_ALGORITHM_HYSTERESIS;
}
void GuideAlgorithmHysteresis::reset(void)
{
    m_lastMove = 0;
}

double GuideAlgorithmHysteresis::result(double input)
{
    double dReturn = (1.0 - m_hysteresis) * input + m_hysteresis * m_lastMove;

    dReturn *= m_aggression;

    if (fabs(input) < m_minMove)
    {
        dReturn = 0.0;
    }

    m_lastMove = dReturn;

    Debug.Write(wxString::Format("GuideAlgorithmHysteresis::Result() returns %.2f from input %.2f\n", dReturn, input));

    return dReturn;
}

bool GuideAlgorithmHysteresis::SetMinMove(double minMove)
{
    bool bError = false;

    try
    {
        if (minMove < 0.0)
        {
            throw ERROR_INFO("invalid minMove");
        }

        m_minMove = minMove;

    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_minMove = DefaultMinMove;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/minMove", m_minMove);

    return bError;
}

bool GuideAlgorithmHysteresis::SetHysteresis(double hysteresis)
{
    bool bError = false;

    try
    {
        if (hysteresis < 0.0 || hysteresis > MaxHysteresis)
        {
            throw ERROR_INFO("invalid hysteresis");
        }

        m_hysteresis = hysteresis;

    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_hysteresis = wxMin(wxMax(hysteresis, 0.0), MaxHysteresis);
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/hysteresis", m_hysteresis);

    return bError;
}

bool GuideAlgorithmHysteresis::SetAggression(double aggression)
{
    bool bError = false;

    try
    {
        if (aggression < 0.0 || aggression > MaxAggression)
        {
            throw ERROR_INFO("invalid aggression");
        }

        m_aggression = aggression;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_aggression = DefaultAggression;
    }

    m_lastMove = 0.0;
    pConfig->Profile.SetDouble(GetConfigPath() + "/aggression", m_aggression);

    return bError;
}

wxString GuideAlgorithmHysteresis::GetSettingsSummary() const
{
    // return a loggable summary of current mount settings
    return wxString::Format("Hysteresis = %.3f, Aggression = %.3f, Minimum move = %.3f\n",
            GetHysteresis(),
            GetAggression(),
            GetMinMove()
        );
}

void GuideAlgorithmHysteresis::GetParamNames(wxArrayString& names) const
{
    names.push_back("minMove");
    names.push_back("hysteresis");
    names.push_back("aggression");
}

bool GuideAlgorithmHysteresis::GetParam(const wxString& name, double *val) const
{
    bool ok = true;

    if (name == "minMove")
        *val = GetMinMove();
    else if (name == "hysteresis")
        *val = GetHysteresis();
    else if (name == "aggression")
        *val = GetAggression();
    else
        ok = false;

    return ok;
}

bool GuideAlgorithmHysteresis::SetParam(const wxString& name, double val)
{
    bool err;

    if (name == "minMove")
        err = SetMinMove(val);
    else if (name == "hysteresis")
        err = SetHysteresis(val);
    else if (name == "aggression")
        err = SetAggression(val);
    else
        err = true;

    return !err;
}

ConfigDialogPane *GuideAlgorithmHysteresis::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuideAlgorithmHysteresisConfigDialogPane(pParent, this);
}

GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisConfigDialogPane::
GuideAlgorithmHysteresisConfigDialogPane(wxWindow *pParent, GuideAlgorithmHysteresis *pGuideAlgorithm)
    : ConfigDialogPane(_("Hysteresis Guide Algorithm"), pParent)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    width = StringWidth(_T("000"));
    m_pHysteresis = pFrame->MakeSpinCtrl(pParent, wxID_ANY,_T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, MaxHysteresis * 100.0, 0.0, _T("Hysteresis"));

    DoAdd(_("Hysteresis"), m_pHysteresis,
           wxString::Format(_("How much history of previous guide pulses should be applied\nDefault = %.f%%, increase to smooth out guiding commands"), DefaultHysteresis * 100.0));

    width = StringWidth(_T("000"));
    m_pAggression = pFrame->MakeSpinCtrl(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, MaxAggression * 100.0, 0.0, _T("Aggressiveness"));

    DoAdd(_("Aggressiveness"), m_pAggression,
          wxString::Format(_("What percentage of the computed correction should be applied? Default = %.f%%, adjust if responding too much or too slowly"), DefaultAggression * 100.0));

    width = StringWidth(_T("00.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);

    DoAdd(_("Minimum Move (pixels)"), m_pMinMove,
          wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
          "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));
}

GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisConfigDialogPane::
~GuideAlgorithmHysteresisConfigDialogPane(void)
{
}

void GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisConfigDialogPane::
LoadValues(void)
{
    m_pHysteresis->SetValue(100.0 * m_pGuideAlgorithm->GetHysteresis());
    m_pAggression->SetValue(100.0 * m_pGuideAlgorithm->GetAggression());
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());
}

void GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisConfigDialogPane::
UnloadValues(void)
{
    m_pGuideAlgorithm->SetHysteresis(m_pHysteresis->GetValue() / 100.0);
    m_pGuideAlgorithm->SetAggression(m_pAggression->GetValue() / 100.0);
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
}

void GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisConfigDialogPane::OnImageScaleChange()
{
    GuideAlgorithm::AdjustMinMoveSpinCtrl(m_pMinMove);
}

void GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisConfigDialogPane::EnableDecControls(bool enable)
{
    m_pAggression->Enable(enable);
    m_pMinMove->Enable(enable);
    m_pHysteresis->Enable(enable);
}

GraphControlPane *GuideAlgorithmHysteresis::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return new GuideAlgorithmHysteresisGraphControlPane(pParent, this, label);
}

GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisGraphControlPane::
GuideAlgorithmHysteresisGraphControlPane(wxWindow *pParent, GuideAlgorithmHysteresis *pGuideAlgorithm, const wxString& label)
    : GraphControlPane(pParent, label)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    // Aggression
    width = StringWidth(_T("000"));
    m_pAggression = pFrame->MakeSpinCtrl(this, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS | wxALIGN_RIGHT, 0.0, MaxAggression * 100.0, 0.0, _T("Aggressiveness"));
    m_pAggression->SetToolTip(wxString::Format(_("What percentage of the computed correction should be applied?? Default = %.f%%, adjust if responding too much or too slowly"), DefaultAggression * 100.0));
    m_pAggression->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GuideAlgorithmHysteresis::GuideAlgorithmHysteresisGraphControlPane::OnAggressionSpinCtrl, this);
    DoAdd(m_pAggression, _("Agr"));

    // Hysteresis
    width = StringWidth(_T("000"));
    m_pHysteresis = pFrame->MakeSpinCtrl(this, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS | wxALIGN_RIGHT, 0.0, MaxHysteresis * 100.0, 0.0, _T("Hysteresis"));
    m_pHysteresis->SetToolTip(wxString::Format(_("How much history of previous guide pulses should be applied\nDefault = %.f%%, increase to smooth out guiding commands"), DefaultHysteresis * 100.0));
    m_pHysteresis->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GuideAlgorithmHysteresis::GuideAlgorithmHysteresisGraphControlPane::OnHysteresisSpinCtrl, this);
    DoAdd(m_pHysteresis,_("Hys"));

    // Min move
    width = StringWidth(_T("00.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(""), wxPoint(-1, -1),
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);
    m_pMinMove->SetToolTip(wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
        "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));
    m_pMinMove->Bind(wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, &GuideAlgorithmHysteresis::GuideAlgorithmHysteresisGraphControlPane::OnMinMoveSpinCtrlDouble, this);
    DoAdd(m_pMinMove,_("MnMo"));

    m_pHysteresis->SetValue(100.0 * m_pGuideAlgorithm->GetHysteresis());
    m_pAggression->SetValue(100.0 * m_pGuideAlgorithm->GetAggression());
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());

    if (TheScope() && pGuideAlgorithm->GetAxis() == "DEC")
    {
        DEC_GUIDE_MODE currDecGuideMode = TheScope()->GetDecGuideMode();
        m_pAggression->Enable(currDecGuideMode != DEC_NONE);
        m_pMinMove->Enable(currDecGuideMode != DEC_NONE);
        m_pHysteresis->Enable(currDecGuideMode != DEC_NONE);
    }
}

GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisGraphControlPane::
~GuideAlgorithmHysteresisGraphControlPane(void)
{
}

void GuideAlgorithmHysteresis::
GuideAlgorithmHysteresisGraphControlPane::EnableDecControls(bool enable)
{
    m_pAggression->Enable(enable);
    m_pHysteresis->Enable(enable);
    m_pMinMove->Enable(enable);
}

void GuideAlgorithmHysteresis::
    GuideAlgorithmHysteresisGraphControlPane::
    OnAggressionSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetAggression(m_pAggression->GetValue() / 100.0);
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Hysteresis aggression", m_pAggression->GetValue());
}

void GuideAlgorithmHysteresis::
    GuideAlgorithmHysteresisGraphControlPane::
    OnHysteresisSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetHysteresis(this->m_pHysteresis->GetValue() / 100.0);
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Hysteresis hysteresis", m_pHysteresis->GetValue());
}

void GuideAlgorithmHysteresis::
    GuideAlgorithmHysteresisGraphControlPane::
    OnMinMoveSpinCtrlDouble(wxSpinDoubleEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Hysteresis minimum move", m_pMinMove->GetValue());
}
