/**
 * PHD2 Guiding
 *
 * @file
 * @date      2014-2017
 * @copyright Max Planck Society
 *
 * @author    Edgar D. Klenske <edgar.klenske@tuebingen.mpg.de>
 * @author    Stephan Wenninger <stephan.wenninger@tuebingen.mpg.de>
 * @author    Raffi Enficiaud <raffi.enficiaud@tuebingen.mpg.de>
 *
 * @brief     Provides a Gaussian process based guiding algorithm
 */

/*
*  This source code is distributed under the following "BSD" license
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*    Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*    Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*    Neither the name of Bret McKee, Dad Dog Development, nor the names of its
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
*/

#include "phd.h"

#include "guide_algorithm_gaussian_process.h"
#include "gaussian_process_guider.h"

#include <ctime>

#include "math_tools.h"
#include "gaussian_process.h"
#include "covariance_functions.h"

/** Default values for the parameters of this algorithm */

static const double DefaultControlGain                   = 0.6; // control gain
static const double DefaultPeriodLengthsForInference     = 2.0; // minimal number of period lengths for full prediction
static const double DefaultMinMove                       = 0.2;

static const double DefaultLengthScaleSE0Ker             = 700.0; // length-scale of the long-range SE-kernel
static const double DefaultSignalVarianceSE0Ker          = 20.0; // signal variance of the long-range SE-kernel
static const double DefaultLengthScalePerKer             = 10.0; // length-scale of the periodic kernel
static const double DefaultPeriodLengthPerKer            = 200.0; // P_p, period-length of the periodic kernel
static const double DefaultSignalVariancePerKer          = 20.0; // signal variance of the periodic kernel
static const double DefaultLengthScaleSE1Ker             = 25.0; // length-scale of the short-range SE-kernel
static const double DefaultSignalVarianceSE1Ker          = 10.0; // signal variance of the short range SE-kernel

static const double DefaultPeriodLengthsForPeriodEstimation = 2.0; // minimal number of period lengts for PL estimation
static const int    DefaultNumPointsForApproximation        = 100; // number of points used in the GP approximation
static const double DefaultPredictionGain                  = 0.5; // amount of GP prediction to blend in

static const double DefaultNoresetMaxPctPeriod = 40.; // max percent of worm period elapsed to skip resetting the model when guiding is stopped and resumed

static const bool   DefaultComputePeriod                 = true;

static void MakeBold(wxControl *ctrl)
{
    wxFont font = ctrl->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    ctrl->SetFont(font);
}

GuideAlgorithmGaussianProcess::
GPExpertDialog::GPExpertDialog(wxWindow *Parent) :
    wxDialog(Parent, wxID_ANY, _("Expert Settings"), wxDefaultPosition, wxDefaultSize),
    m_pPeriodLengthsInference(0), m_pPeriodLengthsPeriodEstimation(0), m_pNumPointsApproximation(0), m_pSE0KLengthScale(0), m_pSE0KSignalVariance(0), m_pPKLengthScale(0),
    m_pPKSignalVariance(0), m_pSE1KLengthScale(0), m_pSE1KSignalVariance(0)
{
    // create the expert options UI
    wxBoxSizer *vSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText *warning = new wxStaticText(this, wxID_ANY, _("Warning!  Intended for use by experts"), wxPoint(-1, -1), wxSize(-1, -1));
    MakeBold(warning);
    vSizer->Add(warning, wxSizerFlags().Center().Border(wxBOTTOM, 10));

    wxFlexGridSizer *flexGrid = new wxFlexGridSizer(10, 2, 5, 5);
    int width;

    width = StringWidth(this, _T("0000"));
    m_pNumPointsApproximation = pFrame->MakeSpinCtrl(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0, 2000, DefaultNumPointsForApproximation);
    AddTableEntry(flexGrid, _("Approximation Data Points"), m_pNumPointsApproximation,
        wxString::Format(_("Number of data points used in the approximation. Both prediction accuracy "
        "as well as runtime rise with the number of datapoints. Default = %d"), DefaultNumPointsForApproximation));

    width = StringWidth(this, _T("0.00"));
    m_pPeriodLengthsInference = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 10.0, DefaultPeriodLengthsForInference, 0.1);
    m_pPeriodLengthsInference->SetDigits(2);
    AddTableEntry(flexGrid, _("Minimum Worm Cycles (Prediction)"), m_pPeriodLengthsInference,
        wxString::Format(_("Minimal number of worm cycles needed to use the prediction. "
        "If there are too little data points, the prediction might be poor. "
        "Default = %.2f"), DefaultPeriodLengthsForInference));

    width = StringWidth(this, _T("0000"));
    m_pPeriodLengthsPeriodEstimation = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 10.0, DefaultPeriodLengthsForPeriodEstimation, 0.1);
    m_pPeriodLengthsPeriodEstimation->SetDigits(2);
    AddTableEntry(flexGrid, _("Minimum Worm Cycles (Period Estimation)"), m_pPeriodLengthsPeriodEstimation,
        wxString::Format(_("Minimal number of worm cycles for estimating the period length. "
        "If there are too little data points, the estimation might not work. Default = %d"),
        (int)DefaultPeriodLengthsForPeriodEstimation));

    width = StringWidth(this, _T("0000.0"));
    m_pSE0KLengthScale = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 100.0, 5000.0, DefaultLengthScaleSE0Ker, 10.0);
    m_pSE0KLengthScale->SetDigits(2);
    AddTableEntry(flexGrid, _("Length Scale (Long Range)"), m_pSE0KLengthScale,
        wxString::Format(_("The length scale (in seconds) of the large non-periodic structure. "
        "This is essentially a high-pass filter for the periodic error and the length scale "
        "defines the corner frequency. Default = %.2f"),
        DefaultLengthScaleSE0Ker));

    width = StringWidth(this, _T("000.0"));
    m_pSE0KSignalVariance = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 100.0, DefaultSignalVarianceSE0Ker, 1.0);
    m_pSE0KSignalVariance->SetDigits(2);
    AddTableEntry(flexGrid, _("Signal Variance (Long Range)"), m_pSE0KSignalVariance,
        wxString::Format(_("Signal variance (in pixels) of the long-term variations. Default = %.2f"), DefaultSignalVarianceSE0Ker));

    width = StringWidth(this, _T("000.0"));
    m_pPKLengthScale = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 1.0, 50.0, DefaultLengthScalePerKer, 5.0);
    m_pPKLengthScale->SetDigits(2);
    AddTableEntry(flexGrid, _("Length Scale (Periodic)"), m_pPKLengthScale,
        wxString::Format(_("The length scale (in seconds) defines the \"wigglyness\" of the periodic structure. "
        "The smaller the length scale, the more structure can be learned. If chosen too "
        "small, some non-periodic structure might be picked up as well. Default = %.2f"), DefaultLengthScalePerKer));

    width = StringWidth(this, _T("000.0"));
    m_pPKSignalVariance = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 100.0, DefaultSignalVariancePerKer, 0.1);
    m_pPKSignalVariance->SetDigits(2);
    AddTableEntry(flexGrid, _("Signal Variance (Periodic)"), m_pPKSignalVariance,
        wxString::Format(_("The signal variance (in pixels) of the periodic error. "
        "Default = %.2f"), DefaultSignalVariancePerKer));

    width = StringWidth(this, _T("000.0"));
    m_pSE1KLengthScale = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 1.0, 100.0, DefaultLengthScaleSE1Ker, 1.0);
    m_pSE1KLengthScale->SetDigits(2);
    AddTableEntry(flexGrid, _("Length Scale (Short Range)"), m_pSE1KLengthScale,
        wxString::Format(_("Length scale (in seconds) of the short range non-periodic parts "
        "of the gear error. This is essentially a low-pass filter for the periodic error and the length "
        "scale defines the corner frequency. Default = %.2f"), DefaultLengthScaleSE1Ker));

    width = StringWidth(this, _T("000.0"));
    m_pSE1KSignalVariance = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 100.0, DefaultSignalVarianceSE1Ker, 0.1);
    m_pSE1KSignalVariance->SetDigits(2);
    AddTableEntry(flexGrid, _("Signal Variance (Short Range)"), m_pSE1KSignalVariance,
        wxString::Format(_("Signal variance (in pixels) of the short-term variations. Default = %.2f"), DefaultSignalVarianceSE1Ker));

    vSizer->Add(flexGrid);
    SetSizerAndFit(vSizer);
}

void GuideAlgorithmGaussianProcess::GPExpertDialog::AddTableEntry(wxFlexGridSizer *Grid, const wxString& Label, wxWindow *Ctrl, const wxString& ToolTip)
{
    wxStaticText *pLabel = new wxStaticText(this, wxID_ANY, Label + _(": "), wxPoint(-1, -1), wxSize(-1, -1));
    Grid->Add(pLabel, 1, wxALL, 5);
    Grid->Add(Ctrl, 1, wxALL, 5);
    Ctrl->SetToolTip(ToolTip);
}

void GuideAlgorithmGaussianProcess::GPExpertDialog::LoadExpertValues(GuideAlgorithmGaussianProcess *m_pGuideAlgorithm, const std::vector<double>& hyperParams)
{
    m_pPeriodLengthsInference->SetValue(m_pGuideAlgorithm->GetPeriodLengthsInference());
    m_pPeriodLengthsPeriodEstimation->SetValue(m_pGuideAlgorithm->GetPeriodLengthsPeriodEstimation());
    m_pNumPointsApproximation->SetValue(m_pGuideAlgorithm->GetNumPointsForApproximation());

    m_pSE0KLengthScale->SetValue(hyperParams[SE0KLengthScale]);
    m_pSE0KSignalVariance->SetValue(hyperParams[SE0KSignalVariance]);
    m_pPKLengthScale->SetValue(hyperParams[PKLengthScale]);
    m_pPKSignalVariance->SetValue(hyperParams[PKSignalVariance]);
    m_pSE1KLengthScale->SetValue(hyperParams[SE1KLengthScale]);
    m_pSE1KSignalVariance->SetValue(hyperParams[SE1KSignalVariance]);
}

void GuideAlgorithmGaussianProcess::GPExpertDialog::UnloadExpertValues(GuideAlgorithmGaussianProcess *m_pGuideAlgorithm, std::vector<double>& hyperParams)
{
    m_pGuideAlgorithm->SetPeriodLengthsInference(m_pPeriodLengthsInference->GetValue());
    m_pGuideAlgorithm->SetPeriodLengthsPeriodEstimation(m_pPeriodLengthsPeriodEstimation->GetValue());
    m_pGuideAlgorithm->SetNumPointsForApproximation(m_pNumPointsApproximation->GetValue());

    hyperParams[SE0KLengthScale] = m_pSE0KLengthScale->GetValue();
    hyperParams[SE0KSignalVariance] = m_pSE0KSignalVariance->GetValue();
    hyperParams[PKLengthScale] = m_pPKLengthScale->GetValue();
    hyperParams[PKSignalVariance] = m_pPKSignalVariance->GetValue();
    hyperParams[SE1KLengthScale] = m_pSE1KLengthScale->GetValue();
    hyperParams[SE1KSignalVariance] = m_pSE1KSignalVariance->GetValue();
}

GuideAlgorithmGaussianProcess::
GuideAlgorithmGPGraphControlPane::GuideAlgorithmGPGraphControlPane(wxWindow *pParent, GuideAlgorithmGaussianProcess *pAlgorithm, const wxString& label)
    : GraphControlPane(pParent, label)
{
    int width;

    m_pGuideAlgorithm = pAlgorithm;

    // Prediction Weight
    width = StringWidth(_T("000"));
    m_pWeight = pFrame->MakeSpinCtrl(this, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS | wxALIGN_RIGHT, 0, 100, 0, _T("Prediction Weight"));
    m_pWeight->SetToolTip(wxString::Format(_("What percent of the predictive guide correction should be applied? "
        "Default = %.f%%, increase to rely more on the predictions"), 100 * DefaultPredictionGain));
    m_pWeight->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GuideAlgorithmGaussianProcess::GuideAlgorithmGPGraphControlPane::OnWeightSpinCtrl, this);
    DoAdd(m_pWeight, _("PredWt"));

    // Reactive Weight
    width = StringWidth(_T("000"));
    m_pAggressiveness = pFrame->MakeSpinCtrl(this, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS | wxALIGN_RIGHT, 0, 100, 0, _T("Reactive Weight"));
    m_pAggressiveness->SetToolTip(wxString::Format(_("What percent of the reactive guide correction should be applied? "
        "Default = %.f%%, adjust if responding too much or too slowly"), 100 * DefaultControlGain));
    m_pAggressiveness->Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &GuideAlgorithmGaussianProcess::GuideAlgorithmGPGraphControlPane::OnAggressivenessSpinCtrl, this);
    DoAdd(m_pAggressiveness, _("ReactWt"));

    // Min move
    width = StringWidth(_T("00.00"));
    m_pMinMove = pFrame->MakeSpinCtrlDouble(this, wxID_ANY, _T(""), wxPoint(-1, -1),
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 20.0, 0.0, 0.01, _T("MinMove"));
    m_pMinMove->SetDigits(2);
    m_pMinMove->SetToolTip(wxString::Format(_("How many (fractional) pixels must the star move to trigger a reactive guide correction? "
        "If camera is binned, this is a fraction of the binned pixel size. Note this has no effect "
        "on the predictive guide correction. Default = %.2f"), DefaultMinMove));
    m_pMinMove->Bind(wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, &GuideAlgorithmGaussianProcess::GuideAlgorithmGPGraphControlPane::OnMinMoveSpinCtrlDouble, this);
    DoAdd(m_pMinMove, _("MnMo"));

    m_pWeight->SetValue(100.0 * m_pGuideAlgorithm->GetPredictionGain());
    m_pAggressiveness->SetValue(100.0 * m_pGuideAlgorithm->GetControlGain());
    m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());
}

GuideAlgorithmGaussianProcess::
GuideAlgorithmGPGraphControlPane::~GuideAlgorithmGPGraphControlPane()
{

}

void GuideAlgorithmGaussianProcess::GuideAlgorithmGPGraphControlPane::OnWeightSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetPredictionGain(m_pWeight->GetValue() / 100.0);
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " PPEC prediction weight", m_pWeight->GetValue());
}

void GuideAlgorithmGaussianProcess::GuideAlgorithmGPGraphControlPane::OnAggressivenessSpinCtrl(wxSpinEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetControlGain(m_pAggressiveness->GetValue() / 100.0);
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " PPEC aggressiveness", m_pAggressiveness->GetValue());
}

void GuideAlgorithmGaussianProcess::GuideAlgorithmGPGraphControlPane::OnMinMoveSpinCtrlDouble(wxSpinDoubleEvent& WXUNUSED(evt))
{
    m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());
    pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " PPEC minimum move", m_pMinMove->GetValue());
}

static double GetRetainModelPct(const GuideAlgorithm *algo)
{
    return pConfig->Profile.GetDouble(algo->GetConfigPath() + "/noreset_max_pct_period", DefaultNoresetMaxPctPeriod);
}

static void SetRetainModelPct(const GuideAlgorithm *algo, double val)
{
    pConfig->Profile.SetDouble(algo->GetConfigPath() + "/noreset_max_pct_period", val);
}

class GuideAlgorithmGaussianProcess::GuideAlgorithmGaussianProcessDialogPane : public wxEvtHandler, public ConfigDialogPane
{
    GuideAlgorithmGaussianProcess *m_pGuideAlgorithm;
    wxSpinCtrl *m_pControlGain;
    wxSpinCtrlDouble *m_pMinMove;
    wxSpinCtrlDouble *m_pPKPeriodLength;
    wxSpinCtrl *m_pPredictionGain;
    wxCheckBox       *m_checkboxComputePeriod;
    wxSpinCtrlDouble *m_retainModelPct;
    wxButton         *m_btnExpertOptions;

public:
    GuideAlgorithmGaussianProcessDialogPane(wxWindow *pParent, GuideAlgorithmGaussianProcess *pGuideAlgorithm)
        :
        ConfigDialogPane(_("Predictive PEC Guide Algorithm"),pParent), m_pGuideAlgorithm(pGuideAlgorithm)
    {
        int width;

        width = StringWidth(_T("0.00"));
        m_pPredictionGain = pFrame->MakeSpinCtrl(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
            wxSize(width, -1), wxSP_ARROW_KEYS, 0, 100, 100 * DefaultPredictionGain);
        DoAdd(_("Predictive Weight"), m_pPredictionGain,
            wxString::Format(_("What percent of the predictive guide correction should be applied? "
            "Default = %.f%%, increase to rely more on the predictions"), 100 * DefaultPredictionGain));

        width = StringWidth(_T("0.00"));
        m_pControlGain = pFrame->MakeSpinCtrl(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
            wxSize(width, -1), wxSP_ARROW_KEYS, 0, 100, 100 * DefaultControlGain);
        DoAdd(_("Reactive Weight"), m_pControlGain,
            wxString::Format(_("What percent of the reactive guide correction should be applied? "
            "Default = %.f%%, adjust if responding too much or too slowly"), 100 * DefaultControlGain));

        width = StringWidth(_T("0.00"));
        m_pMinMove = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
            wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 5.0, DefaultMinMove, 0.01);
        m_pMinMove->SetDigits(2);
        DoAdd(_("Minimum Move (pixels)"), m_pMinMove,
            wxString::Format(_("How many (fractional) pixels must the star move to trigger a reactive guide correction? "
            "If camera is binned, this is a fraction of the binned pixel size. Note this has no effect "
            "on the predictive guide correction. Default = %.2f"), DefaultMinMove));

        width = StringWidth(_T("0000.0"));
        m_pPKPeriodLength = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, _T(" "), wxDefaultPosition,
            wxSize(width, -1), wxSP_ARROW_KEYS, 10.0, 2000.0, DefaultPeriodLengthPerKer, 1);
        m_pPKPeriodLength->SetDigits(2);
        m_checkboxComputePeriod = new wxCheckBox(pParent, wxID_ANY, _("Auto-adjust period"));
        m_checkboxComputePeriod->SetToolTip(wxString::Format(_("Auto-adjust the period length based on identified repetitive errors. Default = %s"),
            DefaultComputePeriod ? _("On") : _("Off")));

        DoAdd(_("Period Length"), m_pPKPeriodLength,
            wxString::Format(_("The period length (in seconds) of the strongest periodic error component.  Default = %.2f"),
            DefaultPeriodLengthPerKer));
        DoAdd(m_checkboxComputePeriod);

        width = StringWidth(_T("888"));
        m_retainModelPct = pFrame->MakeSpinCtrlDouble(pParent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                                      wxSize(width, -1), wxSP_ARROW_KEYS, 0.0, 80.0, DefaultNoresetMaxPctPeriod, 1.0);
        m_retainModelPct->SetDigits(0);
        DoAdd(wxString::Format(_("Retain model (%% period)")), m_retainModelPct,
            wxString::Format(_("Enter a percentage greater than 0 to retain the PPEC model after guiding stops. "
                               "Default = %.f%% of the period length."), DefaultNoresetMaxPctPeriod));

        m_btnExpertOptions = new wxButton(pParent, wxID_ANY, _("Expert..."));
        m_btnExpertOptions->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &GuideAlgorithmGaussianProcessDialogPane::OnExpertButton, this);
        m_btnExpertOptions->SetToolTip(_("Change expert options for tuning the predictions. Use at your own risk!"));
        this->Add(m_btnExpertOptions, wxSizerFlags().Center().Border(wxALL, 3));

        if (!m_pGuideAlgorithm->m_expertDialog)
            m_pGuideAlgorithm->m_expertDialog = new GPExpertDialog(m_pParent);      // Do this here so the parent window has been built
    }

    virtual ~GuideAlgorithmGaussianProcessDialogPane()
    {
      // no need to destroy the widgets, this is done by the parent...
    }

    /* Fill the GUI with the parameters that are currently configured in the
     * guiding algorithm.
     */
    virtual void LoadValues()
    {
        m_pControlGain->SetValue(100 * m_pGuideAlgorithm->GetControlGain());
        m_pPredictionGain->SetValue(100 * m_pGuideAlgorithm->GetPredictionGain());
        m_pMinMove->SetValue(m_pGuideAlgorithm->GetMinMove());

        std::vector<double> hyperparameters = m_pGuideAlgorithm->GetGPHyperparameters();
        assert(hyperparameters.size() == NumParameters);

        m_pPKPeriodLength->SetValue(hyperparameters[PKPeriodLength]);
        m_pPKPeriodLength->Enable(!pFrame->pGuider || !pFrame->pGuider->IsCalibratingOrGuiding());
        m_checkboxComputePeriod->SetValue(m_pGuideAlgorithm->GetBoolComputePeriod());
        m_checkboxComputePeriod->Enable(!pFrame->pGuider || !pFrame->pGuider->IsCalibratingOrGuiding());
        m_retainModelPct->SetValue(GetRetainModelPct(m_pGuideAlgorithm));

        m_pGuideAlgorithm->m_expertDialog->LoadExpertValues(m_pGuideAlgorithm, hyperparameters);

        m_pParent->Layout();
    }

    // Set the parameters chosen in the GUI in the actual guiding algorithm
    virtual void UnloadValues()
    {
        m_pGuideAlgorithm->SetControlGain(m_pControlGain->GetValue() / 100.0);
        m_pGuideAlgorithm->SetPredictionGain(m_pPredictionGain->GetValue() / 100.0);
        m_pGuideAlgorithm->SetMinMove(m_pMinMove->GetValue());

        std::vector<double> hyperparameters(NumParameters);
        hyperparameters[PKPeriodLength] = m_pPKPeriodLength->GetValue();
        m_pGuideAlgorithm->m_expertDialog->UnloadExpertValues(m_pGuideAlgorithm, hyperparameters);

        m_pGuideAlgorithm->SetGPHyperparameters(hyperparameters);
        if (pFrame->pGuider->IsGuiding() && m_pGuideAlgorithm->GetBoolComputePeriod() != m_checkboxComputePeriod->GetValue())
            pFrame->NotifyGuidingParam(m_pGuideAlgorithm->GetAxis() + " PPEC Adjust Period Length", m_checkboxComputePeriod->GetValue());
        m_pGuideAlgorithm->SetBoolComputePeriod(m_checkboxComputePeriod->GetValue());

        SetRetainModelPct(m_pGuideAlgorithm, m_retainModelPct->GetValue());
    }

    virtual void OnImageScaleChange()
    {
        GuideAlgorithm::AdjustMinMoveSpinCtrl(m_pMinMove);
    }

    virtual void OnExpertButton(wxCommandEvent& evt)
    {
         m_pGuideAlgorithm->m_expertDialog->ShowModal();
    }

    // make the class non-copyable (if we ever need this, we must implement a deep copy)
    GuideAlgorithmGaussianProcessDialogPane(const GuideAlgorithmGaussianProcess::GuideAlgorithmGaussianProcessDialogPane&) = delete;
    GuideAlgorithmGaussianProcessDialogPane& operator=(const GuideAlgorithmGaussianProcess::GuideAlgorithmGaussianProcessDialogPane&) = delete;
};

GuideAlgorithmGaussianProcess::GuideAlgorithmGaussianProcess(Mount *pMount, GuideAxis axis)
    : GuideAlgorithm(pMount, axis), GPG(0), dark_tracking_mode_(false)
{
    // create guide parameters, load default values at first
    GaussianProcessGuider::guide_parameters parameters;
    parameters.control_gain_ = DefaultControlGain;
    parameters.min_periods_for_inference_ = DefaultPeriodLengthsForInference;
    parameters.min_move_ = DefaultMinMove;
    parameters.SE0KLengthScale_ = DefaultLengthScaleSE0Ker;
    parameters.SE0KSignalVariance_ = DefaultSignalVarianceSE0Ker;
    parameters.PKLengthScale_ = DefaultLengthScalePerKer;
    parameters.PKPeriodLength_ = DefaultPeriodLengthPerKer;
    parameters.PKSignalVariance_ = DefaultSignalVariancePerKer;
    parameters.SE1KLengthScale_ = DefaultLengthScaleSE1Ker;
    parameters.SE1KSignalVariance_ = DefaultSignalVarianceSE1Ker;
    parameters.min_periods_for_period_estimation_ = DefaultPeriodLengthsForPeriodEstimation;
    parameters.points_for_approximation_ = DefaultNumPointsForApproximation;
    parameters.prediction_gain_ = DefaultPredictionGain;
    parameters.compute_period_ = DefaultComputePeriod;

    // create instance of the worker
    GPG = new GaussianProcessGuider(parameters);

    wxString configPath = GetConfigPath();

    double control_gain = pConfig->Profile.GetDouble(configPath + "/gp_control_gain", DefaultControlGain);
    SetControlGain(control_gain);

    double min_move = pConfig->Profile.GetDouble(configPath + "/gp_min_move", DefaultMinMove);
    SetMinMove(min_move);

    double period_lengths_for_inference = pConfig->Profile.GetDouble(configPath + "/gp_period_lengths_inference", DefaultPeriodLengthsForInference);
    SetPeriodLengthsInference(period_lengths_for_inference);

    double period_lengths_for_period_estimation = pConfig->Profile.GetDouble(configPath + "/gp_period_lengths_period_estimation", DefaultPeriodLengthsForPeriodEstimation);
    SetPeriodLengthsPeriodEstimation(period_lengths_for_period_estimation);

    int num_points_approximation = pConfig->Profile.GetInt(configPath + "/gp_points_for_approximation", DefaultNumPointsForApproximation);
    SetNumPointsForApproximation(num_points_approximation);

    double prediction_gain = pConfig->Profile.GetDouble(configPath + "/gp_prediction_gain", DefaultPredictionGain);
    SetPredictionGain(prediction_gain);

    std::vector<double> v_hyperparameters(NumParameters);
    v_hyperparameters[SE0KLengthScale] = pConfig->Profile.GetDouble(configPath + "/gp_length_scale_se0_kern",  DefaultLengthScaleSE0Ker);
    v_hyperparameters[SE0KSignalVariance] = pConfig->Profile.GetDouble(configPath + "/gp_sigvar_se0_kern",        DefaultSignalVarianceSE0Ker);
    v_hyperparameters[PKLengthScale] = pConfig->Profile.GetDouble(configPath + "/gp_length_scale_per_kern",  DefaultLengthScalePerKer);
    v_hyperparameters[PKSignalVariance] = pConfig->Profile.GetDouble(configPath + "/gp_sigvar_per_kern",        DefaultSignalVariancePerKer);
    v_hyperparameters[SE1KLengthScale] = pConfig->Profile.GetDouble(configPath + "/gp_length_scale_se1_kern",  DefaultLengthScaleSE1Ker);
    v_hyperparameters[SE1KSignalVariance] = pConfig->Profile.GetDouble(configPath + "/gp_sigvar_se1_kern",        DefaultSignalVarianceSE1Ker);
    v_hyperparameters[PKPeriodLength] = pConfig->Profile.GetDouble(configPath + "/gp_period_per_kern",        DefaultPeriodLengthPerKer);

    SetGPHyperparameters(v_hyperparameters);

    bool compute_period = pConfig->Profile.GetBoolean(configPath + "/gp_compute_period", DefaultComputePeriod);
    SetBoolComputePeriod(compute_period);
    m_expertDialog = NULL;
    block_updates_ = !(m_pMount->GetGuidingEnabled());
    guiding_ra_ = math_tools::NaN;
    guiding_pier_side_ = PIER_SIDE_UNKNOWN;
    reset();
}

GuideAlgorithmGaussianProcess::~GuideAlgorithmGaussianProcess()
{
    if (m_expertDialog)
    {
        m_expertDialog->Destroy();
        m_expertDialog = NULL;
    }
    delete GPG;
}

ConfigDialogPane *GuideAlgorithmGaussianProcess::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuideAlgorithmGaussianProcessDialogPane(pParent, this);
}

GraphControlPane *GuideAlgorithmGaussianProcess::GetGraphControlPane(wxWindow *pParent, const wxString& label)
{
    return new GuideAlgorithmGaussianProcess::GuideAlgorithmGPGraphControlPane(pParent, this, label);
}

void GuideAlgorithmGaussianProcess::GetParamNames(wxArrayString& names) const
{
    names.push_back("minMove");
    names.push_back("predictiveWeight");
    names.push_back("reactiveWeight");
    names.push_back("periodLength");
}

bool GuideAlgorithmGaussianProcess::GetParam(const wxString& name, double *val) const
{
    bool ok = true;

    if (name == "minMove")
        *val = GetMinMove();
    else if (name == "predictiveWeight")
        *val = GetPredictionGain();
    else if (name == "reactiveWeight")
        *val = GetControlGain();
    else
    if (name == "periodLength")
    {
        std::vector<double> hyperparameters = GetGPHyperparameters();
        assert(hyperparameters.size() == NumParameters);
        *val = hyperparameters[PKPeriodLength];
    }
    else
        ok = false;

    return ok;
}

bool GuideAlgorithmGaussianProcess::SetParam(const wxString& name, double val)
{
    bool err;

    if (name == "minMove")
        err = SetMinMove(val);
    else if (name == "predictiveWeight")
        err = SetPredictionGain(val);
    else if (name == "reactiveWeight")
        err = SetControlGain(val);
    else
    if (name == "periodLength")
    {
        std::vector<double> hyperparameters(NumParameters);
        hyperparameters[PKPeriodLength] = val;
        err = SetGPHyperparameters(hyperparameters);
    }
    else
        err = true;

    return !err;
}

bool GuideAlgorithmGaussianProcess::SetControlGain(double control_gain)
{
    bool error = false;

    try
    {
        if (control_gain < 0)
        {
            throw ERROR_INFO("invalid controlGain");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        control_gain = DefaultControlGain;
    }

    GPG->SetControlGain(control_gain);

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_control_gain", control_gain);

    return error;
}

bool GuideAlgorithmGaussianProcess::SetMinMove(double min_move)
{
    bool error = false;

    try
    {
        if (min_move < 0)
        {
            throw ERROR_INFO("invalid minimum move");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        min_move = DefaultMinMove;
    }

    GPG->SetMinMove(min_move);

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_min_move", min_move);

    return error;
}

bool GuideAlgorithmGaussianProcess::SetPeriodLengthsInference(double num_periods)
{
    bool error = false;

    try
    {
        if (num_periods < 0.0)
        {
            throw ERROR_INFO("invalid number of elements");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        num_periods = DefaultPeriodLengthsForInference;
    }

    GPG->SetPeriodLengthsInference(num_periods);

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_period_lengths_inference", num_periods);

    return error;
}

bool GuideAlgorithmGaussianProcess::SetPeriodLengthsPeriodEstimation(double num_periods)
{
    bool error = false;

    try
    {
        if (num_periods < 0.0)
        {
            throw ERROR_INFO("invalid number of period lengths");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        num_periods = DefaultPeriodLengthsForPeriodEstimation;
    }

    GPG->SetPeriodLengthsPeriodEstimation(num_periods);

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_period_lengths_period_estimation", num_periods);

    return error;
}

bool GuideAlgorithmGaussianProcess::SetNumPointsForApproximation(int num_points)
{
    bool error = false;

    try
    {
        if (num_points < 0)
        {
            throw ERROR_INFO("invalid number of points");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        num_points = DefaultNumPointsForApproximation;
    }

    GPG->SetNumPointsForApproximation(num_points);

    pConfig->Profile.SetInt(GetConfigPath() + "/gp_points_approximation", num_points);

    return error;
}

bool GuideAlgorithmGaussianProcess::SetGPHyperparameters(const std::vector<double>& _hyperparameters)
{
    if (_hyperparameters.size() != NumParameters)
        return false;

    bool error = false;

    std::vector<double> hyperparameters(_hyperparameters);

    // we do this check in sequence: maybe there would be additional checks on this later.

    // length scale short SE kernel
    try
    {
        if (hyperparameters[SE0KLengthScale] < 1.0) // zero length scale is unstable
        {
            throw ERROR_INFO("invalid length scale for short SE kernel");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        hyperparameters[SE0KLengthScale] = DefaultLengthScaleSE0Ker;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_length_scale_se0_kern", hyperparameters[SE0KLengthScale]);

    // signal variance short SE kernel
    try
    {
        if (hyperparameters[SE0KSignalVariance] < 0.0)
        {
            throw ERROR_INFO("invalid signal variance for the short SE kernel");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        hyperparameters[SE0KSignalVariance] = DefaultSignalVarianceSE0Ker;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_sigvar_se0_kern", hyperparameters[SE0KSignalVariance]);

    // length scale periodic kernel
    try
    {
        if (hyperparameters[PKLengthScale] < 1.0) // zero length scale is unstable
        {
            throw ERROR_INFO("invalid length scale for periodic kernel");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        hyperparameters[PKLengthScale] = DefaultLengthScalePerKer;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_length_scale_per_kern", hyperparameters[PKLengthScale]);

    // signal variance periodic kernel
    try
    {
        if (hyperparameters[PKSignalVariance] < 0.0)
        {
            throw ERROR_INFO("invalid signal variance for the periodic kernel");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        hyperparameters[PKSignalVariance] = DefaultSignalVariancePerKer;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_sigvar_per_kern", hyperparameters[PKSignalVariance]);

    // length scale long SE kernel
    try
    {
        if (hyperparameters[SE1KLengthScale] < 1.0) // zero length scale is unstable
        {
            throw ERROR_INFO("invalid length scale for SE kernel");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        hyperparameters[SE1KLengthScale] = DefaultLengthScaleSE1Ker;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_length_scale_se1_kern", hyperparameters[SE1KLengthScale]);

    // signal variance SE kernel
    try
    {
        if (hyperparameters[SE1KSignalVariance] < 0.0)
        {
            throw ERROR_INFO("invalid signal variance for the SE kernel");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        hyperparameters[SE1KSignalVariance] = DefaultSignalVarianceSE1Ker;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_sigvar_se1_kern", hyperparameters[SE1KSignalVariance]);

    // period length periodic kernel
    try
    {
        if (hyperparameters[PKPeriodLength] < 1.0) // zero period length is unstable
        {
            throw ERROR_INFO("invalid period length for periodic kernel");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        hyperparameters[PKPeriodLength] = DefaultPeriodLengthPerKer;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_period_per_kern", hyperparameters[PKPeriodLength]);

    GPG->SetGPHyperparameters(hyperparameters);
    return error;
}

bool GuideAlgorithmGaussianProcess::SetPredictionGain(double prediction_gain)
{
    bool error = false;

    try
    {
        if (prediction_gain < 0 || prediction_gain > 1.0)
        {
            throw ERROR_INFO("invalid prediction gain");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
        prediction_gain = DefaultPredictionGain;
    }

    GPG->SetPredictionGain(prediction_gain);

    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_prediction_gain", prediction_gain);

    return error;
}

bool GuideAlgorithmGaussianProcess::SetBoolComputePeriod(bool active)
{
    GPG->SetBoolComputePeriod(active);
    pConfig->Profile.SetBoolean(GetConfigPath() + "/gp_compute_period", active);
    return true;
}

double GuideAlgorithmGaussianProcess::GetControlGain() const
{
    return GPG->GetControlGain();
}

double GuideAlgorithmGaussianProcess::GetMinMove() const
{
    return GPG->GetMinMove();
}

double GuideAlgorithmGaussianProcess::GetPeriodLengthsInference() const
{
    return GPG->GetPeriodLengthsInference();
}

double GuideAlgorithmGaussianProcess::GetPeriodLengthsPeriodEstimation() const
{
    return GPG->GetPeriodLengthsPeriodEstimation();
}

int GuideAlgorithmGaussianProcess::GetNumPointsForApproximation() const
{
    return GPG->GetNumPointsForApproximation();
}

std::vector<double> GuideAlgorithmGaussianProcess::GetGPHyperparameters() const
{
    return GPG->GetGPHyperparameters();
}

double GuideAlgorithmGaussianProcess::GetPredictionGain() const
{
    return GPG->GetPredictionGain();
}

bool GuideAlgorithmGaussianProcess::GetBoolComputePeriod() const
{
    return GPG->GetBoolComputePeriod();
}

bool GuideAlgorithmGaussianProcess::GetDarkTracking() const
{
    return dark_tracking_mode_;
}

bool GuideAlgorithmGaussianProcess::SetDarkTracking(bool value)
{
    dark_tracking_mode_ = value;
    return false;
}

wxString GuideAlgorithmGaussianProcess::GetSettingsSummary() const
{
    static const char* format =
      "Control gain = %.3f\n"
      "Prediction gain = %.3f\n"
      "Minimum move = %.3f\n"
      "Hyperparameters\n"
      "\tLength scale long range SE kernel = %.3f\n"
      "\tSignal variance long range SE kernel = %.3f\n"
      "\tLength scale periodic kernel = %.3f\n"
      "\tSignal variance periodic kernel = %.3f\n"
      "\tLength scale short range SE kernel = %.3f\n"
      "\tSignal variance short range SE kernel = %.3f\n"
      "\tPeriod length periodic kernel = %.3f\n"
      "\tFFT called after = %.3f worm cycles\n"
      "\tAuto-adjust period length = %s\n"
    ;

    std::vector<double> hyperparameters = GetGPHyperparameters();

    return wxString::Format(
        format,
        GetControlGain(),
        GetPredictionGain(),
        GetMinMove(),
        hyperparameters[SE0KLengthScale],
        hyperparameters[SE0KSignalVariance],
        hyperparameters[PKLengthScale],
        hyperparameters[PKSignalVariance],
        hyperparameters[SE1KLengthScale],
        hyperparameters[SE1KSignalVariance],
        hyperparameters[PKPeriodLength],
        GetPeriodLengthsPeriodEstimation(),
        GetBoolComputePeriod() ? "On" : "Off");
}

GUIDE_ALGORITHM GuideAlgorithmGaussianProcess::Algorithm() const
{
    return GUIDE_ALGORITHM_GAUSSIAN_PROCESS;
}

double GuideAlgorithmGaussianProcess::result(double input)
{
    if (block_updates_)
        return(0);

    if (dark_tracking_mode_ == true)
    {
        return deduceResult();
    }

    // the third parameter of result() is a floating-point in seconds, while RequestedExposureDuration() returns milliseconds
    const Star& star = pFrame->pGuider->PrimaryStar();
    double control_signal = GPG->result(input, star.SNR, (double) pFrame->RequestedExposureDuration() / 1000.0);

    Debug.Write(wxString::Format("PPEC: input: %.2f, control: %.2f, exposure: %d\n",
        input, control_signal, pFrame->RequestedExposureDuration()));

    return control_signal;
}

double GuideAlgorithmGaussianProcess::deduceResult()
{
    double control_signal = GPG->deduceResult((double) pFrame->RequestedExposureDuration() / 1000.0);

    Debug.Write(wxString::Format("PPEC (deduced): control: %.2f, exposure: %d\n", control_signal,
        pFrame->RequestedExposureDuration()));

    return control_signal;
}

void GuideAlgorithmGaussianProcess::reset()
{
    Debug.Write("PPEC: reset GP model\n");
    GPG->reset();
}

static double CurrentRA()
{
    if (pPointingSource)
    {
        double ra, dec, st;
        bool err = pPointingSource->GetCoordinates(&ra, &dec, &st);
        if (!err)
        {
            return ra;
        }
    }

    return math_tools::NaN;
}

static PierSide CurrentPierSide()
{
    return pPointingSource ? pPointingSource->SideOfPier() : PIER_SIDE_UNKNOWN;
}

inline static wxString FormatRA(double ra)
{
    return math_tools::isNaN(ra) ? _T("unknown") : wxString::Format("%.4f hr", ra);
}

void GuideAlgorithmGaussianProcess::GuidingStarted()
{
    bool need_reset = true;
    double ra_offset;    // RA delta in SI seconds

    auto now = std::chrono::system_clock::now();

    double prev_ra = guiding_ra_;
    guiding_ra_ = CurrentRA();

    PierSide prev_side = guiding_pier_side_;
    guiding_pier_side_ = CurrentPierSide();

    Debug.Write(wxString::Format("PPEC: guiding starts RA = %s, pier %s, prev RA = %s, pier %s\n",
                                 FormatRA(guiding_ra_), Mount::PierSideStr(guiding_pier_side_),
                                 FormatRA(prev_ra), Mount::PierSideStr(prev_side)));

    // retain the model (do not reset) if:
    //   - guiding on the same side of pier, and
    //   - worm rotation is less than 40% of the worm period

    if (!math_tools::isNaN(guiding_ra_) && !math_tools::isNaN(prev_ra) &&
        prev_side != PIER_SIDE_UNKNOWN && prev_side == guiding_pier_side_)
    {
        const double SECONDS_PER_HOUR = 60. * 60.;
        const double SIDEREAL_SECONDS_PER_SEC = 0.9973;

        // Calculate ra shift, handling 0/24 boundary.
        // An increase in RA corresponds to a negative shift in "gear
        // time", i.e., an increase in RA means the worm moved
        // backwards relative to ordinary tracking
        ra_offset = norm(prev_ra - guiding_ra_, -12., 12.);
        ra_offset *= SECONDS_PER_HOUR / SIDEREAL_SECONDS_PER_SEC; // RA hours to SI seconds

        double elapsed_time = std::chrono::duration<double>(now - guiding_stopped_time_).count();
        double worm_offset = elapsed_time + ra_offset;

        double period_length = GPG->GetGPHyperparameters()[PKPeriodLength];
        double max_pct_period = GetRetainModelPct(this);

        Debug.Write(wxString::Format("PPEC: guiding was stopped for %.1f seconds, deltaRA %+.1fs, worm delta %+.1fs, %.1f%% of period "
                                     "(%.1fs), limit %.1f%% (%.1fs)\n",
                                     elapsed_time, -ra_offset, worm_offset,
                                     fabs(worm_offset) / period_length * 100., period_length,
                                     max_pct_period, max_pct_period / 100. * period_length));

        if (fabs(worm_offset) < max_pct_period / 100. * period_length)
        {
            need_reset = false;
        }
        // TODO: the GP Guider cannot currently handle the case of the
        // worm moving backwards. The expectation is that measurements
        // proceed with gear time monotonically increasing.
        if (!need_reset && worm_offset < 0.)
        {
            Debug.Write("PPEC: worm offset is negative, model reset required\n");
            need_reset = true;
        }
    }

    if (need_reset)
    {
        reset();
    }
    else
    {
        Debug.Write(wxString::Format("PPEC: resume guiding with gear time offset %.1f seconds\n", ra_offset));

        // update gear time offset like dither
        GPG->GuidingDithered(ra_offset, 1.0);
    }
}

void GuideAlgorithmGaussianProcess::GuidingStopped()
{
    // need to store the estimated period length in case the user exits the application
    double period_length = GPG->GetGPHyperparameters()[PKPeriodLength];
    pConfig->Profile.SetDouble(GetConfigPath() + "/gp_period_per_kern", period_length);

    guiding_stopped_time_ = std::chrono::system_clock::now();
}

void GuideAlgorithmGaussianProcess::GuidingPaused()
{
}

void GuideAlgorithmGaussianProcess::GuidingResumed()
{
}

void GuideAlgorithmGaussianProcess::GuidingDisabled()
{
    // Don't submit guide star movements to the GP while guiding is disabled
    Debug.Write("PPEC model updates disabled\n");
    block_updates_ = true;
    GuidingStopped();               // Keep our last state and reset
}

void GuideAlgorithmGaussianProcess::GuidingEnabled()
{
    Debug.Write("PPEC model updates enabled\n");
    block_updates_ = false;
}

static double GetRAGuideRate(const Mount *mount)
{
    CalibrationDetails calDetails;
    mount->LoadCalibrationDetails(&calDetails);

    double guide_speed = 1.0; // normalized to 15 a-s per second
                              // 1.0 means 15 a-s/sec,
                              // 0.5 means 7.5 a-s/sec, etc.

    if (calDetails.raGuideSpeed != -1.0)
    {
        // raGuideSpeed is stored in a-s per hour.
        // We want to normalize relative to the sideral 15 a-s per second.
        guide_speed = 3600.0 * calDetails.raGuideSpeed / 15.0; // normalize!
    }

    // the guide rate here is normalized to seconds and adjusted for the speed
    double guide_rate = 1000. * mount->xRate() / guide_speed;

    return guide_rate;
}

void GuideAlgorithmGaussianProcess::GuidingDithered(double amt)
{
    // just hand it on to the guide algorithm, and pass the RA rate
    GPG->GuidingDithered(amt, GetRAGuideRate(m_pMount));
}

void GuideAlgorithmGaussianProcess::GuidingDitherSettleDone(bool success)
{
    // just hand it on to the guide algorithm
    GPG->GuidingDitherSettleDone(success);
}

void GuideAlgorithmGaussianProcess::DirectMoveApplied(double amt)
{
    GPG->DirectMoveApplied(amt, GetRAGuideRate(m_pMount));
}
