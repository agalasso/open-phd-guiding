/*
*  guide_algorithm_lowpass.cpp
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

static const double DefaultMinMove     = 0.2;
static const double DefaultSlopeWeight = 5.0;

GuideAlgorithmLowpass::GuideAlgorithmLowpass(Mount *pMount, GuideAxis axis)
    : GuideAlgorithm(pMount, axis)
{
    double minMove     = pConfig->Profile.GetDouble(GetConfigPath() + "/minMove", DefaultMinMove);
    SetMinMove(minMove);

    double slopeWeight = pConfig->Profile.GetDouble(GetConfigPath() + "/SlopeWeight", DefaultSlopeWeight);
    SetSlopeWeight(slopeWeight);

    m_axisStats = WindowedAxisStats(0);           // self-managed window
    m_timeBase = 0;

    reset();
}

GuideAlgorithmLowpass::~GuideAlgorithmLowpass(void)
{

}

GUIDE_ALGORITHM GuideAlgorithmLowpass::Algorithm() const
{
    return GUIDE_ALGORITHM_LOWPASS;
}
void GuideAlgorithmLowpass::reset(void)
{
    m_axisStats.ClearAll();
    m_timeBase = 0;

    // Needs to be zero-filled to start
    while (m_axisStats.GetCount() < HISTORY_SIZE)
    {
        m_axisStats.AddGuideInfo(m_timeBase++, 0, 0);
    }

}

double GuideAlgorithmLowpass::result(double input)
{
    // Manual trimming of window (instead of auto-size) is done for full backward compatibility with original algo
    m_axisStats.AddGuideInfo(m_timeBase++, input, 0);
    double median = m_axisStats.GetMedian();
    m_axisStats.RemoveOldestEntry();
    double slope;
    double intcpt;
    m_axisStats.GetLinearFitResults(&slope, &intcpt);
    double dReturn = median + m_slopeWeight * slope;

    if (fabs(dReturn) > fabs(input))
    {
        Debug.Write(wxString::Format("GuideAlgorithmLowpass::Result() input %.2f is < calculated value %.2f, using input\n", input, dReturn));
        dReturn = input;
    }

    if (fabs(input) < m_minMove)
    {
        dReturn = 0.0;
    }

    Debug.Write(wxString::Format("GuideAlgorithmLowpass::Result() returns %.2f from input %.2f\n", dReturn, input));

    return dReturn;
}

bool GuideAlgorithmLowpass::SetMinMove(double minMove)
{
    bool bError = false;

    try
    {
        if (minMove < 0)
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

bool GuideAlgorithmLowpass::SetSlopeWeight(double slopeWeight)
{
    bool bError = false;

    try
    {
        if (slopeWeight < 0.0)
        {
            throw ERROR_INFO("invalid slopeWeight");
        }

        m_slopeWeight = slopeWeight;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_slopeWeight = DefaultSlopeWeight;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/SlopeWeight", m_slopeWeight);

    return bError;
}

void GuideAlgorithmLowpass::GetParamNames(wxArrayString& names) const
{
    names.push_back("minMove");
    names.push_back("slopeWeight");
}

bool GuideAlgorithmLowpass::GetParam(const wxString& name, double *val) const
{
    bool ok = true;

    if (name == "minMove")
        *val = GetMinMove();
    else if (name == "slopeWeight")
        *val = GetSlopeWeight();
    else
        ok = false;

    return ok;
}

bool GuideAlgorithmLowpass::SetParam(const wxString& name, double val)
{
    bool err;

    if (name == "minMove")
        err = SetMinMove(val);
    else if (name == "slopeWeight")
        err = SetSlopeWeight(val);
    else
        err = true;

    return !err;
}

wxString GuideAlgorithmLowpass::GetSettingsSummary() const
{
    // return a loggable summary of current mount settings
    return wxString::Format("Slope weight = %.3f, Minimum move = %.3f\n",
            GetSlopeWeight(),
            GetMinMove()
        );
}

ConfigDialogPane *GuideAlgorithmLowpass::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuideAlgorithmLowpassConfigDialogPane(pParent, this);
}

GuideAlgorithmLowpass::
    GuideAlgorithmLowpassConfigDialogPane::
    GuideAlgorithmLowpassConfigDialogPane(wxWindow *pParent, GuideAlgorithmLowpass *pGuideAlgorithm)
    : ConfigDialogPane(_("Lowpass Guide Algorithm"), pParent)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    width = StringWidth(_T("000.00"));
    m_pSlopeWeight = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.5, _T("SlopeWeight"));
    m_pSlopeWeight->SetDigits(2);

    DoAdd(_("Slope Weight"), m_pSlopeWeight,
        wxString::Format(_("Weighting of slope parameter in lowpass auto-dec. Default = %.1f"), DefaultSlopeWeight));

    width = StringWidth(_T("000.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);

    DoAdd(_("Minimum Move (pixels)"), m_pMinMove,
        wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
        "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));

}

GuideAlgorithmLowpass::
    GuideAlgorithmLowpassConfigDialogPane::
    ~GuideAlgorithmLowpassConfigDialogPane(void)
{
}

void GuideAlgorithmLowpass::
    GuideAlgorithmLowpassConfigDialogPane::
    LoadValues(void)
{
    m_pSlopeWeight->SetValue(m_pGuideAlgorithm->GetSlopeWeight());
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());
}

void GuideAlgorithmLowpass::
    GuideAlgorithmLowpassConfigDialogPane::
    UnloadValues(void)
{
    m_pGuideAlgorithm->SetSlopeWeight(m_pSlopeWeight->GetValue());
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
}

void GuideAlgorithmLowpass::
GuideAlgorithmLowpassConfigDialogPane::OnImageScaleChange()
{
    GuideAlgorithm::AdjustMinMoveSpinCtrl(m_pMinMove);
}

void GuideAlgorithmLowpass::
GuideAlgorithmLowpassConfigDialogPane::EnableDecControls(bool enable)
{
    m_pMinMove->Enable(enable);
    m_pSlopeWeight->Enable(enable);
}

GraphControlPane *GuideAlgorithmLowpass::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return new GuideAlgorithmLowpassGraphControlPane(pParent, this, label);
}

GuideAlgorithmLowpass::
    GuideAlgorithmLowpassGraphControlPane::
    GuideAlgorithmLowpassGraphControlPane(wxWindow *pParent, GuideAlgorithmLowpass *pGuideAlgorithm, const wxString& label)
    : GraphControlPane(pParent, label)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    width = StringWidth(_T("000.00"));
    m_pSlopeWeight = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.5, _T("SlopeWeight"));
    m_pSlopeWeight->SetDigits(2);
    m_pSlopeWeight->SetToolTip(wxString::Format(_("Weighting of slope parameter in lowpass auto-dec. Default = %.1f"), DefaultSlopeWeight));
    m_pSlopeWeight->Bind(wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, &GuideAlgorithmLowpass::GuideAlgorithmLowpassGraphControlPane::OnSlopeWeightSpinCtrlDouble, this);
    DoAdd(m_pSlopeWeight, _("Sl W"));

    width = StringWidth(_T("000.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);
    m_pMinMove->SetToolTip(wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
        "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));
    m_pMinMove->Bind(wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, &GuideAlgorithmLowpass::GuideAlgorithmLowpassGraphControlPane::OnMinMoveSpinCtrlDouble, this);
    DoAdd(m_pMinMove, _("MnMo"));

    m_pSlopeWeight->SetValue(m_pGuideAlgorithm->GetSlopeWeight());
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());

    if (TheScope() && pGuideAlgorithm->GetAxis() == "DEC")
    {
        DEC_GUIDE_MODE currDecGuideMode = TheScope()->GetDecGuideMode();
        m_pSlopeWeight->Enable(currDecGuideMode != DEC_NONE);
        m_pMinMove->Enable(currDecGuideMode != DEC_NONE);
    }
}

GuideAlgorithmLowpass::
    GuideAlgorithmLowpassGraphControlPane::
    ~GuideAlgorithmLowpassGraphControlPane(void)
{
}

void GuideAlgorithmLowpass::
GuideAlgorithmLowpassGraphControlPane::EnableDecControls(bool enable)
{
    m_pSlopeWeight->Enable(enable);
    m_pMinMove->Enable(enable);
}

void GuideAlgorithmLowpass::
    GuideAlgorithmLowpassGraphControlPane::
    OnSlopeWeightSpinCtrlDouble(wxSpinDoubleEvent& evt)
{
    m_pGuideAlgorithm->SetSlopeWeight(m_pSlopeWeight->GetValue());
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Low-pass slope weight", m_pSlopeWeight->GetValue());
}

void GuideAlgorithmLowpass::
    GuideAlgorithmLowpassGraphControlPane::
    OnMinMoveSpinCtrlDouble(wxSpinDoubleEvent& evt)
{
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Low-pass minimum move", m_pMinMove->GetValue());
}
