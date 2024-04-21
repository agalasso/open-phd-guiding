/*
 *  scope.h
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

#ifndef SCOPE_H_INCLUDED
#define SCOPE_H_INCLUDED

#define CALIBRATION_RATE_UNCALIBRATED 123e4

class Scope;

enum DEC_GUIDE_MODE
{
    DEC_NONE = 0,
    DEC_AUTO,
    DEC_NORTH,
    DEC_SOUTH
};

// Well known telescope tracking rates
enum DriveRates
{
    driveSidereal = 0,   // Sidereal tracking rate(15.041 arcseconds per second).
    driveLunar = 1,      // Lunar tracking rate(14.685 arcseconds per second).
    driveSolar = 2,      // Solar tracking rate(15.0 arcseconds per second).
    driveKing = 3,       // King tracking rate(15.0369 arcseconds per second).
    driveMaxRate = 4     // Maximum tracking rate
};

// RA offsets relataive to sidereal rate for lunar and solar tracking
#define RA_LUNAR_RATE_OFFSET (-0.52965200)
#define RA_SOLAR_RATE_OFFSET (-0.04106700)

class ScopeConfigDialogCtrlSet : public MountConfigDialogCtrlSet
{
    Scope *m_pScope;
    wxSpinCtrl *m_pCalibrationDuration;
    wxCheckBox *m_pNeedFlipDec;
    wxCheckBox *m_pStopGuidingWhenSlewing;
    wxCheckBox *m_assumeOrthogonal;
    wxSpinCtrl *m_pMaxRaDuration;
    wxSpinCtrl *m_pMaxDecDuration;
    wxChoice   *m_pDecMode;
    wxCheckBox *m_pUseBacklashComp;
    wxSpinCtrlDouble *m_pBacklashPulse;
    wxSpinCtrlDouble *m_pBacklashFloor;
    wxSpinCtrlDouble *m_pBacklashCeiling;
    wxCheckBox *m_pUseDecComp;
    int m_calibrationDistance;
    bool m_origBLCEnabled;

    void OnCalcCalibrationStep(wxCommandEvent& evt);
    void OnDecModeChoice(wxCommandEvent& evt);

public:
    ScopeConfigDialogCtrlSet(wxWindow *pParent, Scope *pScope, AdvancedDialog* pAdvancedDialog, BrainCtrlIdMap& CtrlMap);
    virtual ~ScopeConfigDialogCtrlSet() {};
    void LoadValues() override;
    void UnloadValues() override;
    void ResetRAParameterUI();
    void ResetDecParameterUI();
    int GetCalStepSizeCtrlValue();
    void SetCalStepSizeCtrlValue(int newStep);
    DEC_GUIDE_MODE GetDecGuideModeUI();
};

class Scope : public Mount
{
    int m_calibrationDuration;
    int m_maxDecDuration;
    int m_maxRaDuration;
    DEC_GUIDE_MODE m_decGuideMode;
    DEC_GUIDE_MODE m_saveDecGuideMode;

    time_t m_limitReachedDeferralTime;
    GUIDE_DIRECTION m_raLimitReachedDirection;
    int m_raLimitReachedCount;
    GUIDE_DIRECTION m_decLimitReachedDirection;
    int m_decLimitReachedCount;

    // Calibration variables
    int m_calibrationSteps;
    int m_calibrationDistance;
    int m_recenterRemaining;
    int m_recenterDuration;
    PHD_Point m_calibrationInitialLocation;   // initial position of guide star
    PHD_Point m_calibrationStartingLocation;  // position of guide star at start of calibration measurement (after clear backlash etc.)
    PHD_Point m_calibrationStartingCoords;    // ra,dec coordinates at start of calibration measurement
    PHD_Point m_southStartingLocation;        // Needed to be sure nudging is in south-only direction
    PHD_Point m_eastStartingLocation;         // For basic sanity check that east moves worked at all
    PHD_Point m_lastLocation;
    double m_totalSouthAmt;
    double m_northDirCosX;
    double m_northDirCosY;
    bool m_eastAlertShown;

    // backlash-related variables
    PHD_Point m_blMarkerPoint;
    double m_blExpectedBacklashStep;
    double m_blLastCumDistance;
    int m_blAcceptedMoves;
    double m_blDistanceMoved;
    int m_blMaxClearingPulses;
    enum blConstants { BL_BACKLASH_MIN_COUNT = 3, BL_MAX_CLEARING_TIME = 60000, BL_MIN_CLEARING_DISTANCE = 3 };

    Calibration m_calibration;
    CalibrationDetails m_calibrationDetails;
    bool m_assumeOrthogonal;
    int m_raSteps;
    int m_decSteps;

    bool m_calibrationFlipRequiresDecFlip;
    bool m_stopGuidingWhenSlewing;
    Calibration m_prevCalibration;
    CalibrationDetails m_prevCalibrationDetails;
    CalibrationIssueType m_lastCalibrationIssue;

    bool m_useDecCompensation;
    bool m_hasHPEncoders;

    enum CALIBRATION_STATE
    {
        CALIBRATION_STATE_CLEARED,
        CALIBRATION_STATE_GO_WEST,
        CALIBRATION_STATE_GO_EAST,
        CALIBRATION_STATE_CLEAR_BACKLASH,
        CALIBRATION_STATE_GO_NORTH,
        CALIBRATION_STATE_GO_SOUTH,
        CALIBRATION_STATE_NUDGE_SOUTH,
        CALIBRATION_STATE_COMPLETE
    };
    CALIBRATION_STATE m_calibrationState;
public:
    bool m_CalDetailsValidated;
    bool m_bogusGuideRatesFlagged;

    bool m_canSetTracking;
    struct GuideRateInfo
    {
        wxString name;
        bool canSet;
    } m_mountRates [4];

    // Things related to the Advanced Config Dialog
protected:
    class ScopeConfigDialogPane : public MountConfigDialogPane
    {
        Scope *m_pScope;

    public:
        ScopeConfigDialogPane(wxWindow *pParent, Scope *pScope);
        ~ScopeConfigDialogPane() {};

        void LoadValues() override;
        void UnloadValues() override;
        void LayoutControls(wxPanel *pParent, BrainCtrlIdMap& CtrlMap) override;
    };

    class ScopeGraphControlPane : public GraphControlPane
    {
    public:
        ScopeGraphControlPane(wxWindow *pParent, Scope *pScope, const wxString& label);
        ~ScopeGraphControlPane();

    private:
        friend class Scope;

        Scope *m_pScope;
        wxSpinCtrl *m_pMaxRaDuration;
        wxSpinCtrl *m_pMaxDecDuration;
        wxChoice   *m_pDecMode;

        void OnMaxRaDurationSpinCtrl(wxSpinEvent& evt);
        void OnMaxDecDurationSpinCtrl(wxSpinEvent& evt);
        void OnDecModeChoice(wxCommandEvent& evt);
    };
    ScopeGraphControlPane *m_graphControlPane;

    friend class GraphLogWindow;
    friend class ScopeConfigDialogCtrlSet;

    GUIDE_ALGORITHM DefaultXGuideAlgorithm() const override;
    GUIDE_ALGORITHM DefaultYGuideAlgorithm() const override;

public:

    int GetCalibrationDuration() const;  // calibration step size, ms
    bool SetCalibrationDuration(int calibrationDuration);
    int GetCalibrationDistance() const;  // calibration distance, px
    bool SetCalibrationDistance(int calibrationDistance);
    int GetMaxDecDuration() const;
    bool SetMaxDecDuration(int maxDecDuration);
    int GetMaxRaDuration() const;
    bool SetMaxRaDuration(int maxRaDuration);
    void DeferPulseLimitAlertCheck() override;
    DEC_GUIDE_MODE GetDecGuideMode() const;
    bool SetDecGuideMode(int decGuideMode);

    static wxString DecGuideModeStr(DEC_GUIDE_MODE m);
    static wxString DecGuideModeLocaleStr(DEC_GUIDE_MODE m);

    MountConfigDialogPane *GetConfigDialogPane(wxWindow *pParent) override;
    MountConfigDialogCtrlSet *GetConfigDialogCtrlSet(wxWindow *pParent, Mount *pScope, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap) override;

    GraphControlPane *GetGraphControlPane(wxWindow *pParent, const wxString& label) override;
    wxString GetSettingsSummary() const override;
    wxString CalibrationSettingsSummary() const override;
    wxString GetMountClassName() const override;

    static wxArrayString MountList();
    static wxArrayString AuxMountList();
    static Scope *Factory(const wxString& choice);

    Scope();
    virtual ~Scope();

    void SetCalibration(const Calibration& cal) override;
    void SetCalibrationDetails(const CalibrationDetails& calDetails, double xAngle, double yAngle, double binning);
    virtual void FlagCalibrationIssue(const CalibrationDetails& calDetails, CalibrationIssueType issue);
    bool IsCalibrated() const override;
    bool BeginCalibration(const PHD_Point& currentLocation) override;
    bool UpdateCalibrationState(const PHD_Point& currentLocation) override;

    static const double DEC_COMP_LIMIT; // declination compensation limit
    static const double DEFAULT_MOUNT_GUIDE_SPEED;              // Presumptive mount guide speed if no usable mount connection
    void EnableDecCompensation(bool enable);
    bool DecCompensationEnabled() const override;

    virtual bool RequiresCamera();
    virtual bool RequiresStepGuider();
    bool CalibrationFlipRequiresDecFlip() override;
    bool HasHPEncoders() const override;
    void SetCalibrationFlipRequiresDecFlip(bool val);
    void EnableStopGuidingWhenSlewing(bool enable);
    bool IsStopGuidingWhenSlewingEnabled() const;
    void SetAssumeOrthogonal(bool val);
    bool IsAssumeOrthogonal() const;
    void HandleSanityCheckDialog();
    void SetCalibrationWarning(CalibrationIssueType etype, bool val);
    bool ValidGuideRates(double RAGuideRate, double DecGuideRate);

    virtual double GetDeclinationRadians(); // declination in radians, or UNKNOWN_DECLINATION
    virtual bool GetGuideRates(double *pRAGuideRate, double *pDecGuideRate);
    virtual bool GetCoordinates(double *ra, double *dec, double *siderealTime);
    virtual bool GetSiteLatLong(double *latitude, double *longitude);
    virtual bool CanSlew();
    virtual bool CanSlewAsync();
    virtual bool SlewToCoordinates(double ra, double dec);
    virtual bool SlewToCoordinatesAsync(double ra, double dec);
    virtual void AbortSlew();
    virtual bool CanCheckSlewing();
    virtual bool Slewing();
    virtual PierSide SideOfPier();
    virtual bool CanReportPosition();  // Can report RA, Dec, side-of-pier, etc.
    // Will be called before guiding starts, before any call to GetCoordinates, GetDeclination, or SideOfPier.
    // Does not get called unless guiding was started interactively (by clicking the guide button)
    virtual bool PreparePositionInteractive();
    virtual bool CanPulseGuide();
    virtual void EnumerateTrackingRates();
    virtual bool GetTracking(bool* tracking, bool verbose = false);
    virtual bool SetTracking(bool tracking);
    virtual bool GetTrackingRate(enum DriveRates* rate, bool verbose = false);
    virtual bool GetTrackingRate(enum DriveRates* rate, double *ra_rate, double* dec_rate, bool verbose);
    virtual bool SetTrackingRate(enum DriveRates rate);
    virtual bool SetTrackingRateOffsets(double raRateOffset, double decRateOffset);
    virtual bool CanSetTracking();

    void StartDecDrift() override;
    void EndDecDrift() override;
    bool IsDecDrifting() const override;

private:
    // functions with an implemenation in Scope that cannot be over-ridden
    // by a subclass
    MOVE_RESULT MoveAxis(GUIDE_DIRECTION direction, int durationMs, unsigned int moveOptions, MoveResultInfo *moveResultInfo) final;
    MOVE_RESULT MoveAxis(GUIDE_DIRECTION direction, int duration, unsigned int moveOptions) final;
    int CalibrationMoveSize() override;
    void CheckCalibrationDuration(int currDuration);
    int CalibrationTotDistance() override;

    void ClearCalibration() override;
    wxString GetCalibrationStatus(double dX, double dY, double dist, double dist_crit);
    void SanityCheckCalibration(const Calibration& oldCal, const CalibrationDetails& oldDetails);

    void AlertLimitReached(int duration, GuideAxis axis);

// these MUST be supplied by a subclass
private:
    virtual MOVE_RESULT Guide(GUIDE_DIRECTION direction, int durationMs) = 0;
};

inline bool Scope::IsStopGuidingWhenSlewingEnabled() const
{
    return m_stopGuidingWhenSlewing;
}

inline bool Scope::IsAssumeOrthogonal() const
{
    return m_assumeOrthogonal;
}

inline bool Scope::DecCompensationEnabled() const
{
    return m_useDecCompensation;
}

inline int Scope::GetCalibrationDuration() const
{
    return m_calibrationDuration;
}

inline int Scope::GetCalibrationDistance() const
{
    return m_calibrationDistance;
}

inline int Scope::GetMaxDecDuration() const
{
    return m_maxDecDuration;
}

inline int Scope::GetMaxRaDuration() const
{
    return m_maxRaDuration;
}

inline DEC_GUIDE_MODE Scope::GetDecGuideMode() const
{
    return m_decGuideMode;
}

#endif /* SCOPE_H_INCLUDED */
