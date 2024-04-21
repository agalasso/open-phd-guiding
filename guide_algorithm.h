/*
 *  guide_algorithm.h
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

#ifndef GUIDE_ALGORITHM_H_INCLUDED
#define GUIDE_ALGORITHM_H_INCLUDED

/*
 * The Guide Algorithm class is responsible for providing a
 * mechanism insert various algorithms into the guiding
 * loop
 *
 * It provides a method:
 *
 * double result(double input)
 *
 * that returns the result of whatever processing it does on input.
 *
 * Optionally, the guide algorithm can implement
 *
 * double deduceResult()
 *
 * to produce a mount move when the guide star has been lost (dead reckoning)
 */

class Mount;
class GraphControlPane;

enum GuideAxis
{
    GUIDE_RA,
    GUIDE_X = GUIDE_RA,
    GUIDE_DEC,
    GUIDE_Y = GUIDE_DEC,
};

class GuideAlgorithm
{
protected:
    Mount *m_pMount;
    GuideAxis m_guideAxis;

public:
    GuideAlgorithm(Mount *pMount, GuideAxis axis) : m_pMount(pMount), m_guideAxis(axis) {};
    virtual ~GuideAlgorithm() {};
    virtual GUIDE_ALGORITHM Algorithm() const = 0;

    virtual void reset() = 0;
    virtual double result(double input) = 0;
    virtual double deduceResult() { return 0.0; }

    virtual void GuidingStarted();
    virtual void GuidingStopped();
    virtual void GuidingPaused();
    virtual void GuidingResumed();
    virtual void GuidingDithered(double amt);
    virtual void GuidingDitherSettleDone(bool success);
    virtual void DirectMoveApplied(double amt);
    virtual void GuidingEnabled();
    virtual void GuidingDisabled();

    virtual ConfigDialogPane *GetConfigDialogPane(wxWindow *pParent) = 0;
    virtual GraphControlPane *GetGraphControlPane(wxWindow *pParent, const wxString& label) { return nullptr; };
    virtual wxString GetSettingsSummary() const { return wxEmptyString; }
    virtual wxString GetGuideAlgorithmClassName() const = 0;
    virtual void GetParamNames(wxArrayString& names) const;
    virtual bool GetParam(const wxString& name, double *val) const;
    virtual bool SetParam(const wxString& name, double val);
    virtual double GetMinMove() const { return -1.0; };
    virtual bool SetMinMove(double minMove) { return true; };       // true indicates error
    wxString GetConfigPath() const;
    wxString GetAxis() const;
    virtual void ResetParams();     // Override if fine-tuned logic is needed by a particular algo
    static void AdjustMinMoveSpinCtrl(wxSpinCtrlDouble* minMoveCtrl);
    static double SmartDefaultMinMove();
    static double SmartDefaultMinMove(int focalLength, double pixelSize, int binning);
};

#endif /* GUIDE_ALGORITHM_H_INCLUDED */
