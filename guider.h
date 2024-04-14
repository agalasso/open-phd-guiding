/*
 *  guider.h
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

#ifndef GUIDER_H_INCLUDED
#define GUIDER_H_INCLUDED

enum GUIDER_STATE
{
    STATE_UNINITIALIZED = 0,
    STATE_SELECTING,
    STATE_SELECTED,
    STATE_CALIBRATING_PRIMARY,
    STATE_CALIBRATING_SECONDARY,
    STATE_CALIBRATED,
    STATE_GUIDING,
    STATE_STOP, // This is a pseudo state
};

enum EXPOSED_STATE
{
    EXPOSED_STATE_NONE = 0,
    EXPOSED_STATE_SELECTED,
    EXPOSED_STATE_CALIBRATING,
    EXPOSED_STATE_GUIDING_LOCKED,
    EXPOSED_STATE_GUIDING_LOST,

    EXPOSED_STATE_PAUSED = 100,
    EXPOSED_STATE_LOOPING,
};

enum DEC_GUIDING_ALGORITHM
{
    DEC_LOWPASS = 0,
    DEC_RESISTSWITCH,
    DEC_LOWPASS2,
};

enum OVERLAY_MODE
{
    OVERLAY_NONE = 0,
    OVERLAY_BULLSEYE,
    OVERLAY_GRID_FINE,
    OVERLAY_GRID_COARSE,
    OVERLAY_RADEC,
    OVERLAY_SLIT,
};

struct OverlaySlitCoords
{
    wxPoint center;
    wxSize size;
    int angle;
    wxPoint corners[5];
};

enum PauseType
{
    PAUSE_NONE,     // not paused
    PAUSE_GUIDING,  // pause guide corrections but continue looping exposures
    PAUSE_FULL,     // pause guide corrections and pause looping exposures
};

struct LockPosShiftParams
{
    bool shiftEnabled;
    PHD_Point shiftRate;
    GRAPH_UNITS shiftUnits;
    bool shiftIsMountCoords;
};

class DefectMap;

/*
 * The Guider class is responsible for running the state machine
 * associated with the GUIDER_STATES enumerated type.
 *
 * It is also responsible for drawing and decorating the acquired
 * image in a way that makes sense for its type.
 *
 */

class GuiderConfigDialogCtrlSet : public ConfigDialogCtrlSet
{
    Guider *m_pGuider;
    wxCheckBox *m_pEnableFastRecenter;
    wxCheckBox *m_pScaleImage;

public:
    GuiderConfigDialogCtrlSet(wxWindow *pParent, Guider *pGuider, AdvancedDialog* pAdvancedDialog, BrainCtrlIdMap& CtrlMap);
    virtual ~GuiderConfigDialogCtrlSet() {};
    virtual void LoadValues();
    virtual void UnloadValues();

};

struct GuiderOffset
{
    PHD_Point cameraOfs;
    PHD_Point mountOfs;
};

class Guider : public wxWindow
{
    wxImage *m_displayedImage;
    OVERLAY_MODE m_overlayMode;
    OverlaySlitCoords m_overlaySlitCoords;
    const DefectMap *m_defectMapPreview;
    double m_polarAlignCircleRadius;
    double m_polarAlignCircleCorrection;
    PHD_Point m_polarAlignCircleCenter;
    PauseType m_paused;
    ShiftPoint m_lockPosition;
    PHD_Point m_ditherRecenterStep;
    wxPoint m_ditherRecenterDir;
    PHD_Point m_ditherRecenterRemaining;
    time_t m_starFoundTimestamp;  // timestamp when star was last found
    double m_avgDistance;         // averaged distance for distance reporting
    double m_avgDistanceRA;       // averaged distance, RA only
    double m_avgDistanceLong;     // averaged distance, more smoothed
    double m_avgDistanceLongRA;   // averaged distance, more smoothed, RA only
    unsigned int m_avgDistanceCnt;
    bool m_avgDistanceNeedReset;
    GUIDER_STATE m_state;
    usImage *m_pCurrentImage;
    bool m_scaleImage;
    bool m_lockPosIsSticky;
    bool m_ignoreLostStarLooping;
    bool m_fastRecenterEnabled;
    LockPosShiftParams m_lockPosShift;
    bool m_measurementMode;
    double m_minStarHFD;
    double m_minAFStarSNR;
    double m_maxStarHFD;
    unsigned int m_autoSelDownsample;  // downsample factor for star auto-selection, 0=Auto

protected:
    int m_searchRegion; // how far u/d/l/r do we do the initial search for a star
    bool m_forceFullFrame;
    double m_scaleFactor;
    bool m_showBookmarks;
    std::vector<wxRealPoint> m_bookmarks;

    // Things related to the Advanced Config Dialog
public:
    class GuiderConfigDialogPane : public ConfigDialogPane
    {
        Guider *m_pGuider;

    public:
        GuiderConfigDialogPane(wxWindow *pParent, Guider *pGuider);
        ~GuiderConfigDialogPane() {};

        void LoadValues() {};
        void UnloadValues() {};
        virtual void LayoutControls(Guider *pGuider, BrainCtrlIdMap& CtrlMap);
    };

    OVERLAY_MODE GetOverlayMode() const;

public:
    virtual GuiderConfigDialogPane *GetConfigDialogPane(wxWindow *pParent)= 0;
    virtual GuiderConfigDialogCtrlSet *GetConfigDialogCtrlSet(wxWindow *pParent, Guider *pGuider, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap);

protected:
    Guider(wxWindow *parent, int xSize, int ySize);
    virtual ~Guider();

    bool PaintHelper(wxAutoBufferedPaintDCBase& dc, wxMemoryDC& memDC);
    void SetState(GUIDER_STATE newState);
    void UpdateCurrentDistance(double distance, double distanceRA);

    void ToggleBookmark(const wxRealPoint& pt);

public:
    bool IsPaused() const;
    PauseType GetPauseType() const;
    PauseType SetPaused(PauseType pause);
    GUIDER_STATE GetState() const;
    static EXPOSED_STATE GetExposedState();
    bool IsCalibratingOrGuiding() const;
    bool IsCalibrating() const;
    bool IsRecentering() const { return m_ditherRecenterRemaining.IsValid(); }
    bool IsGuiding() const;
    void OnClose(wxCloseEvent& evt);
    void OnErase(wxEraseEvent& evt);
    void UpdateImageDisplay(usImage *pImage = nullptr);

    bool MoveLockPosition(const PHD_Point& mountDelta);
    virtual bool SetLockPosition(const PHD_Point& position);
    bool SetLockPosToStarAtPosition(const PHD_Point& starPositionHint);
    bool ShiftLockPosition();
    void EnableLockPosShift(bool enable);
    void SetLockPosShiftRate(const PHD_Point& rate, GRAPH_UNITS units, bool isMountCoords, bool updateToolWin);
    bool LockPosShiftEnabled() const { return m_lockPosShift.shiftEnabled; }
    void SetLockPosIsSticky(bool isSticky) { m_lockPosIsSticky = isSticky; }
    bool LockPosIsSticky() const { return m_lockPosIsSticky; }
    void SetIgnoreLostStarLooping(bool ignore);
    const ShiftPoint& LockPosition() const;
    const LockPosShiftParams& GetLockPosShiftParams() const { return m_lockPosShift; }
    void ForceFullFrame();

    bool SetOverlayMode(int newMode);
    void GetOverlaySlitCoords(wxPoint *center, wxSize *size, int *angle);
    void SetOverlaySlitCoords(const wxPoint& center, const wxSize& size, int angle);
    void SetDefectMapPreview(const DefectMap *preview);
    void SetPolarAlignCircle(const PHD_Point& center, double radius);
    void SetPolarAlignCircleCorrection(double val);
    double GetPolarAlignCircleCorrection() const;
    bool SaveCurrentImage(const wxString& fileName);

    void StartGuiding();
    void StopGuiding();
    void UpdateGuideState(usImage *pImage, bool bStopping=false);
    void DisplayImage(usImage *img);

    bool SetScaleImage(bool newScaleValue);
    bool GetScaleImage() const;

    int GetSearchRegion() const;
    double CurrentError(bool raOnly);
    double CurrentErrorSmoothed(bool raOnly);
    unsigned int CurrentErrorFrameCount() const { return m_avgDistanceCnt; }

    bool GetBookmarksShown() const;
    void SetBookmarksShown(bool show);
    void ToggleShowBookmarks();
    void DeleteAllBookmarks();
    void BookmarkLockPosition();
    void BookmarkCurPosition();

    void Reset(bool fullReset);
    void EnableMeasurementMode(bool enabled);
    double GetMinStarHFD() const;
    double GetMinStarHFDFloor() const;
    double GetMinStarHFDDefault() const;
    void SetMinStarHFD(double val);
    double GetMaxStarHFD() const;
    void SetMaxStarHFD(double val);
    // 'AF' methods deal with SNR threshold used only during Auto-Find - lets user specify a higher than default SNR floor for populating a multi-star list. Normal 'find' methods do not use this property
    void SetAFMinStarSNR(double val);
    double GetAFMinStarSNR() const;

    void SetAutoSelDownsample(unsigned int val);
    unsigned int GetAutoSelDownsample() const;

    // virtual functions -- these CAN be overridden by a subclass, which should
    // consider whether they need to call the base class functions as part of
    // their operation
private:
    virtual void InvalidateLockPosition();
public:
    virtual void LoadProfileSettings();

    // pure virtual functions -- these MUST be overridden by a subclass
public:
    virtual bool IsValidLockPosition(const PHD_Point& pt) = 0;
    virtual bool IsValidSecondaryStarPosition(const PHD_Point& pt) = 0;
    virtual void InvalidateCurrentPosition(bool fullReset = false) = 0;
private:
    virtual bool UpdateCurrentPosition(const usImage *pImage, GuiderOffset *ofs, FrameDroppedInfo *errorInfo) = 0;
    virtual bool SetCurrentPosition(const usImage *pImage, const PHD_Point& position) = 0;

public:
    virtual void OnPaint(wxPaintEvent& evt) = 0;

    virtual bool IsLocked() const = 0;
    virtual bool AutoSelect(const wxRect& roi = wxRect()) = 0;

    virtual const PHD_Point& CurrentPosition() const = 0;
    virtual wxRect GetBoundingBox() const = 0;
    virtual int GetMaxMovePixels() const = 0;

    virtual const Star& PrimaryStar() const = 0;

    virtual bool GetMultiStarMode() const { return false; }
    virtual void SetMultiStarMode(bool On) {};
    virtual wxString GetStarCount() const { return wxEmptyString; }

    usImage *CurrentImage() const;
    wxImage *DisplayedImage() const;
    double ScaleFactor() const;

    virtual wxString GetSettingsSummary() const;

    bool IsFastRecenterEnabled() const;
    void EnableFastRecenter(bool enable);

private:
    void UpdateLockPosShiftCameraCoords();
    DECLARE_EVENT_TABLE()
};

inline bool Guider::IsPaused() const
{
    return m_paused != PAUSE_NONE;
}

inline PauseType Guider::GetPauseType() const
{
    return m_paused;
}

inline OVERLAY_MODE Guider::GetOverlayMode() const
{
    return m_overlayMode;
}

inline bool Guider::GetScaleImage() const
{
    return m_scaleImage;
}

inline const ShiftPoint& Guider::LockPosition() const
{
    return m_lockPosition;
}

inline GUIDER_STATE Guider::GetState() const
{
    return m_state;
}

inline bool Guider::IsGuiding() const
{
    return m_state == STATE_GUIDING;
}

inline bool Guider::IsCalibratingOrGuiding() const
{
    return m_state >= STATE_CALIBRATING_PRIMARY && m_state <= STATE_GUIDING;
}

inline bool Guider::IsCalibrating() const
{
    return m_state >= STATE_CALIBRATING_PRIMARY && m_state < STATE_CALIBRATED;
}

inline int Guider::GetSearchRegion() const
{
    return m_searchRegion;
}

inline bool Guider::IsFastRecenterEnabled() const
{
    return m_fastRecenterEnabled;
}

inline double Guider::GetPolarAlignCircleCorrection() const
{
    return m_polarAlignCircleCorrection;
}

inline void Guider::SetPolarAlignCircleCorrection(double val)
{
    m_polarAlignCircleCorrection = val;
}

inline usImage *Guider::CurrentImage() const
{
    return m_pCurrentImage;
}

inline wxImage *Guider::DisplayedImage() const
{
    return m_displayedImage;
}

inline double Guider::ScaleFactor() const
{
    return m_scaleFactor;
}

inline bool Guider::GetBookmarksShown() const
{
    return m_showBookmarks;
}

inline double Guider::GetMinStarHFD() const
{
    return m_minStarHFD;
}

inline double Guider::GetMinStarHFDFloor() const
{
    return 0.1;
}

inline double Guider::GetMinStarHFDDefault() const
{
    return 1.5;
}

inline double Guider::GetAFMinStarSNR() const
{
    return m_minAFStarSNR;
}

inline double Guider::GetMaxStarHFD() const
{
    return m_maxStarHFD;
}

inline unsigned int Guider::GetAutoSelDownsample() const
{
    return m_autoSelDownsample;
}

#endif /* GUIDER_H_INCLUDED */
