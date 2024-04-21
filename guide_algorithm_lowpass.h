/*
 *  guide_algorithm_lowpass.h
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

#ifndef GUIDE_ALGORITHM_LOWPASS_H_INCLUDED
#define GUIDE_ALGORITHM_LOWPASS_H_INCLUDED

#include "guiding_stats.h"

class GuideAlgorithmLowpass : public GuideAlgorithm
{
    static const unsigned int HISTORY_SIZE = 10;
    double m_slopeWeight;
    double m_minMove;
    WindowedAxisStats m_axisStats;
    int m_timeBase;

protected:
    class GuideAlgorithmLowpassConfigDialogPane : public ConfigDialogPane
    {
        GuideAlgorithmLowpass *m_pGuideAlgorithm;
        wxSpinCtrlDouble *m_pSlopeWeight;
        wxSpinCtrlDouble *m_pMinMove;

    public:
        GuideAlgorithmLowpassConfigDialogPane(wxWindow *pParent, GuideAlgorithmLowpass *pGuideAlgorithm);
        ~GuideAlgorithmLowpassConfigDialogPane();

        void LoadValues() override;
        void UnloadValues() override;
        void OnImageScaleChange() override;
        void EnableDecControls(bool enable) override;
    };

    class GuideAlgorithmLowpassGraphControlPane : public GraphControlPane
    {
    public:
        GuideAlgorithmLowpassGraphControlPane(wxWindow *pParent, GuideAlgorithmLowpass *pGuideAlgorithm, const wxString& label);
        ~GuideAlgorithmLowpassGraphControlPane();
        void EnableDecControls(bool enable) override;

    private:
        GuideAlgorithmLowpass *m_pGuideAlgorithm;
        wxSpinCtrlDouble *m_pSlopeWeight;
        wxSpinCtrlDouble *m_pMinMove;

        void OnSlopeWeightSpinCtrlDouble(wxSpinDoubleEvent& evt);
        void OnMinMoveSpinCtrlDouble(wxSpinDoubleEvent& evt);
    };

    double GetMinMove() const override;
    bool SetMinMove(double minMove) override;
    double GetSlopeWeight() const;
    bool SetSlopeWeight(double SlopeWeight);

    friend class GuideAlgorithmLowpassConfigDialogPane;

public:
    GuideAlgorithmLowpass(Mount *pMount, GuideAxis axis);
    ~GuideAlgorithmLowpass();
    GUIDE_ALGORITHM Algorithm() const override;

    void reset() override;
    double result(double input) override;
    ConfigDialogPane *GetConfigDialogPane(wxWindow *pParent) override;
    GraphControlPane *GetGraphControlPane(wxWindow *pParent, const wxString& label) override;
    wxString GetSettingsSummary() const override;
    wxString GetGuideAlgorithmClassName() const override { return "Lowpass"; }
    void GetParamNames(wxArrayString& names) const override;
    bool GetParam(const wxString& name, double *val) const override;
    bool SetParam(const wxString& name, double val) override;
};

inline double GuideAlgorithmLowpass::GetMinMove() const
{
    return m_minMove;
}

inline double GuideAlgorithmLowpass::GetSlopeWeight() const
{
    return m_slopeWeight;
}

#endif /* GUIDE_ALGORITHM_LOWPASS_H_INCLUDED */

