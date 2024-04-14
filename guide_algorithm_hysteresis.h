/*
 *  guide_algorithm_hysteresis.h
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

#ifndef GUIDE_ALGORITHM_HYSTERESIS_H_INCLUDED
#define GUIDE_ALGORITHM_HYSTERESIS_H_INCLUDED

class GuideAlgorithmHysteresis : public GuideAlgorithm
{
    double m_minMove;
    double m_hysteresis;
    double m_aggression;
    double m_lastMove;

protected:

    class GuideAlgorithmHysteresisConfigDialogPane : public ConfigDialogPane
    {
        GuideAlgorithmHysteresis *m_pGuideAlgorithm;
        wxSpinCtrl *m_pHysteresis;
        wxSpinCtrl *m_pAggression;
        wxSpinCtrlDouble *m_pMinMove;

    public:
        GuideAlgorithmHysteresisConfigDialogPane(wxWindow *pParent, GuideAlgorithmHysteresis *pGuideAlgorithm);
        ~GuideAlgorithmHysteresisConfigDialogPane();

        void LoadValues() override;
        void UnloadValues() override;
        void OnImageScaleChange() override;
        void EnableDecControls(bool enable) override;
    };

    class GuideAlgorithmHysteresisGraphControlPane : public GraphControlPane
    {
    public:
        GuideAlgorithmHysteresisGraphControlPane(wxWindow *pParent, GuideAlgorithmHysteresis *pGuideAlgorithm, const wxString& label);
        ~GuideAlgorithmHysteresisGraphControlPane();
        void EnableDecControls(bool enable) override;

    private:
        GuideAlgorithmHysteresis *m_pGuideAlgorithm;
        wxSpinCtrl *m_pAggression;
        wxSpinCtrl *m_pHysteresis;
        wxSpinCtrlDouble *m_pMinMove;

        void OnAggressionSpinCtrl(wxSpinEvent& evt);
        void OnHysteresisSpinCtrl(wxSpinEvent& evt);
        void OnMinMoveSpinCtrlDouble(wxSpinDoubleEvent& evt);
    };

    double GetMinMove() const override;
    bool SetMinMove(double minMove) override;
    double GetHysteresis() const;
    bool SetHysteresis(double minMove);
    double GetAggression() const;
    bool SetAggression(double minMove);

    friend class GuideAlgorithmHysteresisConfigDialogPane;
    friend class GraphLogWindow;

public:

    GuideAlgorithmHysteresis(Mount *pMount, GuideAxis axis);
    ~GuideAlgorithmHysteresis();

    GUIDE_ALGORITHM Algorithm() const override;

    void reset() override;
    double result(double input) override;
    ConfigDialogPane *GetConfigDialogPane(wxWindow *pParent) override;
    GraphControlPane *GetGraphControlPane(wxWindow *pParent, const wxString& label) override;
    wxString GetSettingsSummary() const override;
    wxString GetGuideAlgorithmClassName() const override { return "Hysteresis"; }
    void GetParamNames(wxArrayString& names) const override;
    bool GetParam(const wxString& name, double *val) const override;
    bool SetParam(const wxString& name, double val) override;
};

inline double GuideAlgorithmHysteresis::GetMinMove() const
{
    return m_minMove;
}

inline double GuideAlgorithmHysteresis::GetHysteresis() const
{
    return m_hysteresis;
}

inline double GuideAlgorithmHysteresis::GetAggression() const
{
    return m_aggression;
}
#endif /* GUIDE_ALGORITHM_HYSTERESIS_H_INCLUDED */
