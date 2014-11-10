//
//  guide_gaussian_process.cpp
//  PHD
//
//  Created by Stephan Wenninger
//  Copyright 2014, Max Planck Society.

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
#include "guide_gaussian_process.h"
#include <wx/stopwatch.h>
#include "gaussian_process/tools/math_tools.h"
#include "UDPGuidingInteraction.h"

static const double DefaultControlGain = 1.0;

GuideGaussianProcess::GuideGaussianProcess(Mount *pMount, GuideAxis axis)
    : GuideAlgorithm(pMount, axis),
      udpInteraction(_T("localhost"),_T("1308"),_T("1309")),
      timestamps_(180),
      measurements_(180),
      modified_measurements_(180),
      timer_(),
      control_signal_(0.0),
      number_of_measurements_(0),
      elapsed_time_ms_(0.0)
{
    wxString configPath = GetConfigPath();
    double control_gain = pConfig->Profile.GetDouble(configPath + "/controlGain",
                                                     DefaultControlGain);
    SetControlGain(control_gain);

    reset();
}

GuideGaussianProcess::~GuideGaussianProcess(void)
{
}


ConfigDialogPane *GuideGaussianProcess::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuideGaussianProcessDialogPane(pParent, this);
}


GuideGaussianProcess::
GuideGaussianProcessDialogPane::
GuideGaussianProcessDialogPane(wxWindow *pParent, GuideGaussianProcess *pGuideAlgorithm)
    : ConfigDialogPane(_("Gaussian Process Guide Algorithm"),pParent)
{
    m_pGuideAlgorithm = pGuideAlgorithm;

    int width = StringWidth(_T("000.00"));
    m_pControlGain = new wxSpinCtrlDouble(pParent, wxID_ANY, _T("foo2"),
                                          wxPoint(-1,-1),wxSize(width+30, -1),
                                          wxSP_ARROW_KEYS, 0.0, 1.0, 0.0, 0.05,
                                          _T("Control Gain"));
    m_pControlGain->SetDigits(2);


    //
    // TODO Add description of the control part!
    //
    DoAdd(_("Control Gain"), m_pControlGain,
          _("Description of the control gain. Default = 1.0"));
}

GuideGaussianProcess::
GuideGaussianProcessDialogPane::
~GuideGaussianProcessDialogPane()
{
}

bool GuideGaussianProcess::SetControlGain(double control_gain)
{
    bool error = false;

    try {
        if (control_gain < 0) {
            throw ERROR_INFO("invalid controlGain");
        }

        control_gain_ = control_gain;
    } catch (wxString Msg) {
        POSSIBLY_UNUSED(Msg);
        error = true;
        control_gain_ = DefaultControlGain;
    }

    pConfig->Profile.SetDouble(GetConfigPath() + "/controlGain", control_gain_);

    return error;
}

double GuideGaussianProcess::GetControlGain()
{
    return control_gain_;
}

wxString GuideGaussianProcess::GetSettingsSummary() {
    return wxString::Format("Control Gain = %.3f\n",
                            GetControlGain());
}

void GuideGaussianProcess::
    GuideGaussianProcessDialogPane::
    UnloadValues(void)
{
    m_pGuideAlgorithm->SetControlGain(m_pControlGain->GetValue());
}

void GuideGaussianProcess::
    GuideGaussianProcessDialogPane::
    LoadValues(void)
{
    m_pControlGain->SetValue(m_pGuideAlgorithm->GetControlGain());
}


GUIDE_ALGORITHM GuideGaussianProcess::Algorithm(void)
{
    return GUIDE_ALGORITHM_GAUSSIAN_PROCESS;
}

void GuideGaussianProcess::HandleTimestamps()
{
    if (number_of_measurements_ == 0)
    {
        timer_.Start();
    }
    double time_now = timer_.Time();
    double delta_measurement_time_ms = time_now - elapsed_time_ms_;
    elapsed_time_ms_ = time_now;
    timestamps_.append(elapsed_time_ms_ - delta_measurement_time_ms / 2);
}

void GuideGaussianProcess::HandleMeasurements(double input)
{
    measurements_.append(input);
}

void GuideGaussianProcess::HandleModifiedMeasurements(double input)
{
    // If there is no previous measurement, a random one is generated.
    if(number_of_measurements_ == 0)
    {
        //The daytime indoor measurement noise SD is 0.25-0.35
        double indoor_noise_standard_deviation = 0.25;
        double first_random_measurement = indoor_noise_standard_deviation *
            math_tools::generate_normal_random_double();
        double new_modified_measurement =
            control_signal_ +
            first_random_measurement * (1 - control_gain_) -
            input;
        modified_measurements_.append(new_modified_measurement);
    }
    else
    {
        double new_modified_measurement =
            control_signal_ +
            measurements_.getSecondLastElement() * (1 - control_gain_) -
            measurements_.getLastElement();
        modified_measurements_.append(new_modified_measurement);
    }
}

double GuideGaussianProcess::result(double input)
{
    HandleTimestamps();
    HandleMeasurements(input);
    HandleModifiedMeasurements(input);
    number_of_measurements_++;

    /*
     * Need to read this value here because it is not loaded at the construction
     * time of this object.
     */
    double delta_controller_time_ms = pFrame->RequestedExposureDuration();

    if (number_of_measurements_ > 5)
    {
        /*
        // Inference
        gp_->infer(*timestamps_.getEigenVector(),
                   *modified_measurements_.getEigenVector());
        // Prediction of new control_signal_
        Eigen::VectorXd prediction =
            gp_->predict(elapsed_time_ms_ + delta_controller_time_ms / 2) -
            control_gain_ * input / delta_controller_time_ms;
        control_signal_ = prediction(0);
         */
    }
    else
    {
        control_signal_ = -input / delta_controller_time_ms;
    }

    if (number_of_measurements_ > 30) {
        std::cout << *measurements_.getEigenVector() << std::endl;
        std::cout << measurements_.getEigenVector()->size() << std::endl;
        std::cout << "\n" << std::endl;

        std::cout << *modified_measurements_.getEigenVector() << std::endl;
        std::cout << modified_measurements_.getEigenVector()->size() << std::endl;
        std::cout << "\n" << std::endl;

        std::cout << *timestamps_.getEigenVector() << std::endl;
        std::cout << timestamps_.getEigenVector()->size() << std::endl;
        std::cout << "\n" << std::endl;
    }

    return control_signal_;

    // Old UDP Interaction
    /*
     double buf[] = {input};

     udpInteraction.sendToUDPPort(buf, sizeof(buf));
     udpInteraction.receiveFromUDPPort(buf, sizeof(buf)); // this command blocks until matlab sends back something

     return buf[0];
     */
}


void GuideGaussianProcess::reset()
{
    timestamps_.clear();
    measurements_.clear();
    modified_measurements_.clear();
    number_of_measurements_ = 0;
    return;
}
