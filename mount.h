/*
 *  mount.h
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

#ifndef MOUNT_H_INCLUDED
#define MOUNT_H_INCLUDED

#include "guide_algorithms.h"
#include "image_math.h"
#include "messagebox_proxy.h"

class BacklashComp;
struct GuiderOffset;

enum GUIDE_DIRECTION
{
    NONE  = -1,
    UP = 0,
    NORTH = UP,     // Dec + for eq mounts
    DOWN,
    SOUTH = DOWN,   // Dec-
    RIGHT,
    EAST = RIGHT,   // RA-
    LEFT,
    WEST = LEFT     // RA+
};

enum PierSide
{
    PIER_SIDE_UNKNOWN = -1,
    PIER_SIDE_EAST = 0,
    PIER_SIDE_WEST = 1,
};

enum GuideParity
{
    GUIDE_PARITY_EVEN = 1,      // Guide(NORTH) moves scope north
    GUIDE_PARITY_ODD = -1,      // Guide(NORTH) moves scope south
    GUIDE_PARITY_UNKNOWN = 0,   // we don't know or care
    GUIDE_PARITY_UNCHANGED = -5, // special case for SetCalibration, leave value unchanged
};

#define UNKNOWN_DECLINATION 997.0

struct Calibration
{
    double xRate;
    double yRate;
    double xAngle;
    double yAngle;
    double declination; // radians, or UNKNOWN_DECLINATION
    double rotatorAngle;
    unsigned short binning;
    PierSide pierSide;
    GuideParity raGuideParity;
    GuideParity decGuideParity;
    bool isValid;
    wxString timestamp;

    Calibration() : isValid(false) { }
};

enum CalibrationIssueType
{
    CI_None,
    CI_Steps,
    CI_Angle,
    CI_Rates,
    CI_Different
};

static const wxString CalibrationIssueString[] = { "None", "Steps", "Orthogonality", "Rates", "Difference" };

struct CalibrationDetails
{
    int focalLength;
    double imageScale;
    double raGuideSpeed;
    double decGuideSpeed;
    double orthoError;
    double origBinning;
    std::vector<wxRealPoint> raSteps;
    std::vector<wxRealPoint> decSteps;
    int raStepCount;
    int decStepCount;
    CalibrationIssueType lastIssue;
    wxString origTimestamp;
    PierSide origPierSide;

    CalibrationDetails() : raStepCount(0) { }
    bool IsValid() const { return raStepCount > 0; }
};

enum MountMoveOptionBits
{
    MOVEOPT_ALGO_RESULT = (1<<0),    // filter move through guide algorithm
    MOVEOPT_ALGO_DEDUCE = (1<<1),    // use guide algorithm to deduce the move amount (when paused or star lost)
    MOVEOPT_USE_BLC     = (1<<2),    // use backlash comp for this move
    MOVEOPT_GRAPH       = (1<<3),    // display the move on the graphs
    MOVEOPT_MANUAL      = (1<<4),    // manual move - allow even when guiding disabled
};

enum
{
    MOVEOPTS_CALIBRATION_MOVE = 0,
    MOVEOPTS_GUIDE_STEP       = MOVEOPT_ALGO_RESULT | MOVEOPT_USE_BLC | MOVEOPT_GRAPH,
    MOVEOPTS_DEDUCED_MOVE     = MOVEOPT_ALGO_DEDUCE | MOVEOPT_USE_BLC | MOVEOPT_GRAPH,
    MOVEOPTS_RECOVERY_MOVE    = MOVEOPT_USE_BLC,
    MOVEOPTS_AO_BUMP          = MOVEOPT_USE_BLC,
};

extern wxString DumpMoveOptionBits(unsigned int moveOptions);

struct MoveResultInfo
{
    int amountMoved;
    bool limited;

    MoveResultInfo() : amountMoved(0), limited(false) { }
};

class MountConfigDialogCtrlSet : public ConfigDialogCtrlSet
{
    Mount* m_pMount;
    wxCheckBox *m_pClearCalibration;
    wxCheckBox *m_pEnableGuide;

public:
    MountConfigDialogCtrlSet(wxWindow *pParent, Mount *pMount, AdvancedDialog* pAdvancedDialog, BrainCtrlIdMap& CtrlMap);
    virtual ~MountConfigDialogCtrlSet() {};
    virtual void LoadValues();
    virtual void UnloadValues();
};

class Mount : public wxMessageBoxProxy
{
    bool m_connected;
    int m_requestCount;
    int m_errorCount;

    bool m_calibrated;
    Calibration m_cal;
    double m_xRate;         // rate adjusted for declination
    double m_yAngleError;   // orthogonality error

protected:
    bool m_guidingEnabled;

    GuideAlgorithm *m_pXGuideAlgorithm;
    GuideAlgorithm *m_pYGuideAlgorithm;

    wxString m_Name;
    BacklashComp *m_backlashComp;
    GuideStepInfo m_lastStep;

    // Things related to the Advanced Config Dialog
public:
    class MountConfigDialogPane : public wxEvtHandler, public ConfigDialogPane
    {
    protected:
        Mount *m_pMount;
        wxWindow* m_pParent;
        wxChoice   *m_pXGuideAlgorithmChoice;
        wxChoice   *m_pYGuideAlgorithmChoice;
        int        m_initXGuideAlgorithmSelection;
        int        m_initYGuideAlgorithmSelection;
        ConfigDialogPane *m_pXGuideAlgorithmConfigDialogPane;
        ConfigDialogPane *m_pYGuideAlgorithmConfigDialogPane;
        wxStaticBoxSizer* m_pAlgoBox;
        wxStaticBoxSizer* m_pRABox;
        wxStaticBoxSizer* m_pDecBox;
        wxButton* m_pResetRAParams;
        wxButton* m_pResetDecParams;
        void OnResetRAParams(wxCommandEvent& evt);
        void OnResetDecParams(wxCommandEvent& evt);

    public:
        MountConfigDialogPane(wxWindow *pParent, const wxString& title, Mount *pMount);
        ~MountConfigDialogPane();

        virtual void LoadValues();
        virtual void UnloadValues();
        virtual void LayoutControls(wxPanel *pParent, BrainCtrlIdMap& CtrlMap);
        virtual void OnImageScaleChange();
        bool IsValid() { return m_pMount != nullptr; }

        virtual void Undo();

        void OnXAlgorithmSelected(wxCommandEvent& evt);
        void OnYAlgorithmSelected(wxCommandEvent& evt);
        void ChangeYAlgorithm(const wxString& algoName);
        void ResetRAGuidingParams();
        void ResetDecGuidingParams();
        void EnableDecControls(bool enable);
    };

    GUIDE_ALGORITHM GetXGuideAlgorithmSelection() const;
    GUIDE_ALGORITHM GetYGuideAlgorithmSelection() const;

    void SetXGuideAlgorithm(int guideAlgorithm);
    void SetYGuideAlgorithm(int guideAlgorithm);

    virtual GUIDE_ALGORITHM DefaultXGuideAlgorithm() const = 0;
    virtual GUIDE_ALGORITHM DefaultYGuideAlgorithm() const = 0;

    static GUIDE_ALGORITHM GetGuideAlgorithm(const GuideAlgorithm *pAlgorithm);
    static bool CreateGuideAlgorithm(int guideAlgorithm, Mount *mount, GuideAxis axis, GuideAlgorithm **ppAlgorithm);

#ifdef TEST_TRANSFORMS
    void Mount::TestTransforms();
#endif

    // functions with an implementation in Mount that cannot be over-ridden
    // by a subclass
public:

    enum MOVE_RESULT {
        MOVE_OK = 0,             // move succeeded
        MOVE_ERROR,              // move failed for unspecified reason
        MOVE_ERROR_SLEWING,      // move failed due to scope slewing
        MOVE_ERROR_AO_LIMIT_REACHED, // move failed due to AO limit
    };

    Mount();
    virtual ~Mount();

    static const wxString& GetIssueString(CalibrationIssueType issue) { return CalibrationIssueString[issue]; };

    double yAngle() const;
    double yRate() const;
    double xAngle() const;
    double xRate() const;
    GuideParity RAParity() const;
    GuideParity DecParity() const;
    double GetCalibrationDeclination() const; // in radians

    bool FlipCalibration();
    bool GetGuidingEnabled() const;
    void SetGuidingEnabled(bool guidingEnabled);
    virtual void DeferPulseLimitAlertCheck();

    virtual MOVE_RESULT MoveOffset(GuiderOffset *guiderOffset, unsigned int moveOptions);

    bool TransformCameraCoordinatesToMountCoordinates(const PHD_Point& cameraVectorEndpoint,
                                                      PHD_Point& mountVectorEndpoint, bool logged = true);

    bool TransformMountCoordinatesToCameraCoordinates(const PHD_Point& mountVectorEndpoint,
                                                      PHD_Point& cameraVectorEndpoint, bool logged = true);

    void LogGuideStepInfo();

    GraphControlPane *GetXGuideAlgorithmControlPane(wxWindow *pParent);
    GraphControlPane *GetYGuideAlgorithmControlPane(wxWindow *pParent);
    virtual GraphControlPane *GetGraphControlPane(wxWindow *pParent, const wxString& label);

    virtual bool DecCompensationEnabled() const;
    virtual void AdjustCalibrationForScopePointing();

    // untranslated strings for logging
    static wxString DeclinationStr(double dec, const wxString& numFormatStr = "%.1f");
    static wxString PierSideStr(PierSide side);
    // translated strings for UI
    static wxString DeclinationStrTr(double dec, const wxString& numFormatStr = "%.1f");
    static wxString PierSideStrTr(PierSide side);

    bool IsBusy() const;
    void IncrementRequestCount();
    void DecrementRequestCount();

    int ErrorCount() const;
    void IncrementErrorCount();
    void ResetErrorCount();

    // pure virtual functions -- these MUST be overridden by a subclass
public:
    // move the requested direction, return the actual amount of the move
    virtual MOVE_RESULT MoveAxis(GUIDE_DIRECTION direction, int amount, unsigned int moveOptions, MoveResultInfo *moveResultInfo) = 0;
    virtual MOVE_RESULT MoveAxis(GUIDE_DIRECTION direction, int duration, unsigned int moveOptions) = 0;
    virtual int CalibrationMoveSize() = 0;
    virtual int CalibrationTotDistance() = 0;

    // Calibration related routines
    virtual bool BeginCalibration(const PHD_Point &currentLocation) = 0;
    virtual bool UpdateCalibrationState(const PHD_Point &currentLocation) = 0;

    virtual void NotifyGuidingStarted();
    virtual void NotifyGuidingStopped();
    virtual void NotifyGuidingPaused();
    virtual void NotifyGuidingResumed();
    virtual void NotifyGuidingDithered(double dx, double dy, bool mountCoords);
    virtual void NotifyGuidingDitherSettleDone(bool success);
    virtual void NotifyDirectMove(const PHD_Point& dist);

    virtual MountConfigDialogPane *GetConfigDialogPane(wxWindow *pParent) = 0;
    virtual MountConfigDialogCtrlSet *GetConfigDialogCtrlSet(wxWindow *pParent, Mount *pMount, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap) = 0;
    ConfigDialogCtrlSet* currConfigDialogCtrlSet;               // instance currently in-use by AD

    virtual wxString GetMountClassName() const = 0;

    GuideAlgorithm *GetXGuideAlgorithm() const;
    GuideAlgorithm *GetYGuideAlgorithm() const;

    void GetLastCalibration(Calibration *cal) const;
    BacklashComp *GetBacklashComp() const { return m_backlashComp; }

    // virtual functions -- these CAN be overridden by a subclass, which should
    // consider whether they need to call the base class functions as part of
    // their operation
public:

    virtual bool HasNonGuiMove();
    virtual bool SynchronousOnly();
    virtual bool HasSetupDialog() const;
    virtual void SetupDialog();

    virtual const wxString& Name() const;
    virtual bool IsStepGuider() const;
    virtual wxPoint GetAoPos() const;
    virtual wxPoint GetAoMaxPos() const;
    virtual const char *DirectionStr(GUIDE_DIRECTION d) const;
    virtual const char *DirectionChar(GUIDE_DIRECTION d) const;

    virtual bool IsCalibrated() const;
    virtual void ClearCalibration();
    virtual void SetCalibration(const Calibration& cal);

    void SaveCalibrationDetails(const CalibrationDetails& calDetails) const;
    void LoadCalibrationDetails(CalibrationDetails *calDetails) const;

    virtual bool IsConnected() const;
    virtual bool Connect();
    virtual bool Disconnect();

    virtual wxString GetSettingsSummary() const;
    virtual wxString CalibrationSettingsSummary() const { return wxEmptyString; }

    virtual bool CalibrationFlipRequiresDecFlip();
    virtual bool HasHPEncoders() const { return false; }

    virtual void StartDecDrift();
    virtual void EndDecDrift();
    virtual bool IsDecDrifting() const;

    bool MountIsCalibrated() const { return m_calibrated; }
    const Calibration& MountCal() const { return m_cal; }
};

inline double Mount::xRate() const
{
    return m_xRate;
}

inline double Mount::yRate() const
{
    return m_cal.yRate;
}

inline double Mount::xAngle() const
{
    return m_cal.xAngle;
}

inline bool Mount::GetGuidingEnabled() const
{
    return m_guidingEnabled;
}

inline bool Mount::IsBusy() const
{
    return m_requestCount > 0;
}

inline int Mount::ErrorCount() const
{
    return m_errorCount;
}

inline void Mount::IncrementErrorCount()
{
    ++m_errorCount;
}

inline void Mount::ResetErrorCount()
{
    m_errorCount = 0;
}

inline GuideAlgorithm *Mount::GetXGuideAlgorithm() const
{
    return m_pXGuideAlgorithm;
}

inline GuideAlgorithm *Mount::GetYGuideAlgorithm() const
{
    return m_pYGuideAlgorithm;
}

inline bool Mount::IsConnected() const
{
    return m_connected;
}

inline GuideParity Mount::RAParity() const
{
    return m_calibrated ? m_cal.raGuideParity : GUIDE_PARITY_UNKNOWN;
}

inline GuideParity Mount::DecParity() const
{
    return m_calibrated ? m_cal.decGuideParity : GUIDE_PARITY_UNKNOWN;
}

inline double Mount::GetCalibrationDeclination() const
{
    return m_calibrated ? m_cal.declination : UNKNOWN_DECLINATION;
}

#endif /* MOUNT_H_INCLUDED */
