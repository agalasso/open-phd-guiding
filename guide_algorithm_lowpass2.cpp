/*
 *  guide_algorithm_lowpass2.cpp
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
static const double DefaultAggressiveness = 80.0;

GuideAlgorithmLowpass2::GuideAlgorithmLowpass2(Mount *pMount, GuideAxis axis)
    : GuideAlgorithm(pMount, axis)
{
    double minMove = pConfig->Profile.GetDouble(GetConfigPath() + "/minMove", DefaultMinMove);
    SetMinMove(minMove);

    double aggr = pConfig->Profile.GetDouble(GetConfigPath() + "/Aggressiveness", DefaultAggressiveness);
    SetAggressiveness(aggr);
    m_axisStats = WindowedAxisStats(HISTORY_SIZE);          // Auto-windowed
    m_timeBase = 0;

    reset();
}

GuideAlgorithmLowpass2::~GuideAlgorithmLowpass2(void)
{

}

GUIDE_ALGORITHM GuideAlgorithmLowpass2::Algorithm() const
{
    return GUIDE_ALGORITHM_LOWPASS2;
}

void GuideAlgorithmLowpass2::reset(void)
{
    m_axisStats.ClearAll();
    m_timeBase = 0;
    m_rejects = 0;
}

double GuideAlgorithmLowpass2::result(double input)
{
    m_axisStats.AddGuideInfo(m_timeBase++, input, 0);              // AxisStats instance is auto-windowed
    unsigned int numpts = m_axisStats.GetCount();
    double dReturn;
    double attenuation = m_aggressiveness / 100.;
    double newSlope = 0;

    if (numpts < 4)
        dReturn = input * attenuation;                    // Don't fall behind while we're figuring things out
    else
    {
        if (fabs(input) > 4.0 * m_minMove)                            // Outlier deflection - dump the history
        {
            dReturn = input * attenuation;
            reset();
            numpts = 0;
            Debug.Write("Lowpass2 history cleared, outlier deflection\n");
        }
        else
        {
            double intcpt;
            m_axisStats.GetLinearFitResults(&newSlope, &intcpt);
            dReturn = newSlope * (double)numpts * attenuation;
            // Don't return a result that will push the star further in the wrong direction
            if (input * dReturn < 0)
                dReturn = 0;
        }
    }

    if (fabs(dReturn) > fabs(input))            // Keep guide pulses below magnitude of last deflection
    {
        Debug.Write(wxString::Format("GuideAlgorithmLowpass2::Result() input %.2f is < calculated value %.2f, using input\n", input, dReturn));
        dReturn = input * attenuation;
        m_rejects++;
        if (m_rejects > 3)          // 3-in-a-row, our slope is not useful
        {
            reset();
            Debug.Write("Lowpass2 history cleared, 3 successive rejected correction values\n");
        }
    }
    else
        m_rejects = 0;

    if (fabs(input) < m_minMove)
        dReturn = 0.0;

    Debug.Write(wxString::Format("GuideAlgorithmLowpass2::Result() returns %.2f from input %.2f, slope = %.2f\n", dReturn, input, newSlope));
    return dReturn;
}

bool GuideAlgorithmLowpass2::SetMinMove(double minMove)
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

void GuideAlgorithmLowpass2::GetParamNames(wxArrayString& names) const
{
    names.push_back("minMove");
    names.push_back("aggressiveness");
}

bool GuideAlgorithmLowpass2::GetParam(const wxString& name, double *val) const
{
    bool ok = true;

    if (name == "minMove")
        *val = GetMinMove();
    else if (name == "aggressiveness")
        *val = GetAggressiveness();
    else
        ok = false;

    return ok;
}

bool GuideAlgorithmLowpass2::SetParam(const wxString& name, double val)
{
    bool err;

    if (name == "minMove")
        err = SetMinMove(val);
    else if (name == "aggressiveness")
        err = SetAggressiveness(val);
    else
        err = true;

    return !err;
}

bool GuideAlgorithmLowpass2::SetAggressiveness(double aggressiveness)
{
    bool bError = false;

    try
    {
        if (aggressiveness < 0.0)
        {
            throw ERROR_INFO("invalid aggressiveness");
        }

        m_aggressiveness = aggressiveness;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        aggressiveness = DefaultAggressiveness;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/Aggressiveness", aggressiveness);

    return bError;
}

ConfigDialogPane *GuideAlgorithmLowpass2::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuideAlgorithmLowpass2ConfigDialogPane(pParent, this);
}

wxString GuideAlgorithmLowpass2::GetSettingsSummary() const
{
    // return a loggable summary of current mount settings
    return wxString::Format("Aggressiveness = %.3f, Minimum move = %.3f\n",
        GetAggressiveness(),
        GetMinMove()
        );
}

GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2ConfigDialogPane::
GuideAlgorithmLowpass2ConfigDialogPane(wxWindow *pParent, GuideAlgorithmLowpass2 *pGuideAlgorithm)
    : ConfigDialogPane(_("Lowpass2 Guide Algorithm"), pParent)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    m_pGuideAlgorithm = pGuideAlgorithm;

    width = StringWidth(_T("000.00"));
    m_pAggressiveness = pFrame->MakeSpinCtrl(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 100.0, 0.0, _T("Aggressiveness"));

    DoAdd(_("Aggressiveness"), m_pAggressiveness,
        wxString::Format(_("What percentage of the computed correction should be applied? Default = %.f%%"), DefaultAggressiveness));

    width = StringWidth(_T("000.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);

    DoAdd(_("Minimum Move (pixels)"), m_pMinMove,
        wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
        "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));
}

GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2ConfigDialogPane::
~GuideAlgorithmLowpass2ConfigDialogPane(void)
{
}

void GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2ConfigDialogPane::
LoadValues(void)
{
    m_pAggressiveness->SetValue(m_pGuideAlgorithm->GetAggressiveness());
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());
}

void GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2ConfigDialogPane::
UnloadValues(void)
{
    m_pGuideAlgorithm->SetAggressiveness(m_pAggressiveness->GetValue());
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
}

void GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2ConfigDialogPane::OnImageScaleChange()
{
    GuideAlgorithm::AdjustMinMoveSpinCtrl(m_pMinMove);
}

void GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2ConfigDialogPane::EnableDecControls(bool enable)
{
    m_pAggressiveness->Enable(enable);
    m_pMinMove->Enable(enable);
}

GraphControlPane *GuideAlgorithmLowpass2::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return new GuideAlgorithmLowpass2GraphControlPane(pParent, this, label);
}

GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2GraphControlPane::
GuideAlgorithmLowpass2GraphControlPane(wxWindow *pParent, GuideAlgorithmLowpass2 *pGuideAlgorithm, const wxString& label)
: GraphControlPane(pParent, label)
{
    int width;

    m_pGuideAlgorithm = pGuideAlgorithm;

    width = StringWidth(_T("000.00"));
    m_pAggressiveness = pFrame->MakeSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 100.0, 0.0, _T("Aggressiveness"));
    m_pAggressiveness->SetToolTip(wxString::Format(_("What percentage of the computed correction should be applied? Default = %.f%%"), DefaultAggressiveness));
    m_pAggressiveness->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GuideAlgorithmLowpass2::GuideAlgorithmLowpass2GraphControlPane::OnAggrSpinCtrl, this);
    DoAdd(m_pAggressiveness, _("Agg"));

    width = StringWidth(_T("000.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);
    m_pMinMove->SetToolTip(wxString::Format(_("How many (fractional) pixels must the star move to trigger a guide pulse? \n"
        "If camera is binned, this is a fraction of the binned pixel size. Default = %.2f"), DefaultMinMove));
    m_pMinMove->Bind(wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, &GuideAlgorithmLowpass2::GuideAlgorithmLowpass2GraphControlPane::OnMinMoveSpinCtrlDouble, this);
    DoAdd(m_pMinMove, _("MnMo"));

    m_pAggressiveness->SetValue(m_pGuideAlgorithm->GetAggressiveness());
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());

    if (TheScope() && pGuideAlgorithm->GetAxis() == "DEC")
    {
        DEC_GUIDE_MODE currDecGuideMode = TheScope()->GetDecGuideMode();
        m_pAggressiveness->Enable(currDecGuideMode != DEC_NONE);
        m_pMinMove->Enable(currDecGuideMode != DEC_NONE);
    }
}

GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2GraphControlPane::
~GuideAlgorithmLowpass2GraphControlPane(void)
{
}

void GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2GraphControlPane::EnableDecControls(bool enable)
{
    m_pAggressiveness->Enable(enable);
    m_pMinMove->Enable(enable);
}

void GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2GraphControlPane::
OnAggrSpinCtrl(wxSpinEvent& evt)
{
    m_pGuideAlgorithm->SetAggressiveness(m_pAggressiveness->GetValue());
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Low-pass2 aggressiveness", m_pAggressiveness->GetValue());
}

void GuideAlgorithmLowpass2::
GuideAlgorithmLowpass2GraphControlPane::
OnMinMoveSpinCtrlDouble(wxSpinDoubleEvent& evt)
{
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " Low-pass2 minimum move", m_pMinMove->GetValue());
}
