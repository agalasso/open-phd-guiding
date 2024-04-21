/*
 *  myframe.h
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
 *  Refactored by Bret McKee
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
 *    Neither the name of Craig Stark, Stark Labs nor the names of its
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

#ifndef MYFRAME_H_INCLUDED
#define MYFRAME_H_INCLUDED

class WorkerThread;
class MyFrame;
class RefineDefMap;
struct alert_params;
class PHDStatusBar;

enum E_MYFRAME_WORKER_THREAD_MESSAGES
{
    MYFRAME_WORKER_THREAD_EXPOSE_COMPLETE = wxID_HIGHEST+1,
    MYFRAME_WORKER_THREAD_MOVE_COMPLETE,
};

wxDECLARE_EVENT(REQUEST_EXPOSURE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(REQUEST_MOUNT_MOVE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WXMESSAGEBOX_PROXY_EVENT, wxCommandEvent);
wxDECLARE_EVENT(STATUSBAR_ENQUEUE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(STATUSBAR_TIMER_EVENT, wxTimerEvent);
wxDECLARE_EVENT(SET_STATUS_TEXT_EVENT, wxThreadEvent);
wxDECLARE_EVENT(ALERT_FROM_THREAD_EVENT, wxThreadEvent);

enum NOISE_REDUCTION_METHOD
{
    NR_NONE,
    NR_2x2MEAN,
    NR_3x3MEDIAN
};

struct AutoExposureCfg
{
    bool enabled;
    int minExposure;
    int maxExposure;
    double targetSNR;
};

struct VarDelayCfg
{
    bool enabled;
    int shortDelay;     // times in milliseconds
    int longDelay;
};

typedef void alert_fn(long);

class MyFrameConfigDialogPane : public ConfigDialogPane
{

public:
    MyFrameConfigDialogPane(wxWindow *pParent, MyFrame *pFrame);
    virtual ~MyFrameConfigDialogPane() {};

    void LayoutControls(BrainCtrlIdMap& CtrlMap);
    virtual void LoadValues() {};
    virtual void UnloadValues() {};
};

enum DitherMode
{
    DITHER_RANDOM,
    DITHER_SPIRAL,
};

struct DitherSpiral
{
    int x, y, dx, dy;
    bool prevRaOnly;

    DitherSpiral() { Reset(); }
    void Reset();
    void GetDither(double amount, bool raOnly, double *dRA, double *dDec);
};

struct SingleExposure
{
    bool enabled;
    int duration;
    wxRect subframe;
};

class MyFrameConfigDialogCtrlSet : public ConfigDialogCtrlSet
{
    MyFrame *m_pFrame;
    wxCheckBox *m_pResetConfiguration;
    wxCheckBox *m_pResetDontAskAgain;
    wxCheckBox *m_updateEnabled;
    wxCheckBox *m_updateMajorOnly;
    wxRadioButton *m_ditherRandom;
    wxRadioButton *m_ditherSpiral;
    wxSpinCtrlDouble *m_ditherScaleFactor;
    wxCheckBox *m_ditherRaOnly;
    wxChoice *m_pNoiseReduction;
    wxSpinCtrl *m_pTimeLapse;
    wxTextCtrl *m_pFocalLength;
    wxChoice *m_pLanguage;
    int m_oldLanguageChoice;
    wxTextCtrl *m_pLogDir;
    wxButton *m_pSelectDir;
    wxCheckBox *m_EnableImageLogging;
    wxStaticBoxSizer *m_LoggingOptions;
    wxCheckBox *m_LogNextNFrames;
    wxCheckBox *m_LogRelErrors;
    wxCheckBox *m_LogAbsErrors;
    wxCheckBox *m_LogDroppedFrames;
    wxCheckBox *m_LogAutoSelectFrames;
    wxSpinCtrlDouble *m_LogRelErrorThresh;
    wxSpinCtrlDouble *m_LogAbsErrorThresh;
    wxSpinCtrl *m_LogNextNFramesCount;
    wxCheckBox *m_pAutoLoadCalibration;
    wxComboBox *m_autoExpDurationMin;
    wxComboBox *m_autoExpDurationMax;
    wxSpinCtrlDouble *m_autoExpSNR;
    wxCheckBox *m_varExposureDelayEnabled;
    wxSpinCtrl *m_varExpDelayShort;
    wxSpinCtrl *m_varExpDelayLong;
    void OnDirSelect(wxCommandEvent& evt);
    void OnImageLogEnableChecked(wxCommandEvent& event);
    void OnVariableDelayChecked(wxCommandEvent& evt);

public:
    MyFrameConfigDialogCtrlSet(MyFrame *pFrame, AdvancedDialog* pAdvancedDialog, BrainCtrlIdMap& CtrlMap);
    virtual ~MyFrameConfigDialogCtrlSet() {};

    virtual void LoadValues();
    virtual void UnloadValues();
    int GetFocalLength() const;
    void SetFocalLength(int val);
};

// NOTE: Custom Workaround for Toolbar Sync
// ------------------------------------------------------------------------
// This derived wxAuiToolBarExt class includes methods to access private
// data of wxAuiToolBar. These methods are used to manually sync the
// toolbar's pane info (wxAuiPaneInfo) in scenarios where the default
// wxWidgets behavior does not suffice (e.g., syncing toolbar layout after
// dynamic changes).
//
// This workaround is currently necessary due to the longstanding use of
// wxWidgets 3.0.5 in PHD2 and its specific UI requirements. If updating
// wxWidgets or adjusting UI implementation, consider removing or
// updating this custom extension.
// ------------------------------------------------------------------------
class wxAuiToolBarExt : public wxAuiToolBar
{
public:
    wxAuiToolBarExt(wxWindow* parent, wxWindowID id = -1,
        const wxPoint& position = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxAUI_TB_DEFAULT_STYLE)
        : wxAuiToolBar(parent, id, position, size, style) {}

    wxSize GetAbsoluteMinSize() const
    {
        return m_absoluteMinSize;
    }

    int GetToolPanes() const
    {   // iterate over m_items and count the number of tool panes
        int count = 0;
        for (size_t i = 0; i < m_items.GetCount(); i++)
        {
            wxAuiToolBarItem& item = m_items.Item(i);
            if (item.GetKind() != wxITEM_SEPARATOR)
                count++;
        }
        return count;
    }
};

class MyFrame : public wxFrame
{
protected:
    NOISE_REDUCTION_METHOD GetNoiseReductionMethod() const;
    bool SetNoiseReductionMethod(int noiseReductionMethod);

    bool GetServerMode() const;
    bool SetServerMode(bool val);

    bool SetTimeLapse(int timeLapse);
    int GetTimeLapse() const;
    int GetExposureDelay();

    wxMutex eventLock;
    bool SetFocalLength(int focalLength);

    friend class MyFrameConfigDialogPane;
    friend class MyFrameConfigDialogCtrlSet;
    friend class WorkerThread;
    friend class PlanetToolWin;

private:

    NOISE_REDUCTION_METHOD m_noiseReductionMethod;
    DitherMode m_ditherMode;
    double m_ditherScaleFactor;
    bool m_ditherRaOnly;
    DitherSpiral m_ditherSpiral;
    bool m_serverMode;
    int  m_timeLapse;       // Delay between frames (useful for vid cameras)
    VarDelayCfg m_varDelayConfig;
    int  m_focalLength;
    bool m_beepForLostStar;
    double m_sampling;
    bool m_autoLoadCalibration;

    wxAuiManager m_mgr;
    PHDStatusBar *m_statusbar;

    bool m_continueCapturing; // should another image be captured?
    SingleExposure m_singleExposure;

public:
    MyFrame();
    virtual ~MyFrame();

    Guider *pGuider;
    wxMenuBar *Menubar;
    wxMenu *tools_menu, *view_menu, *bookmarks_menu, *darks_menu;
    wxMenuItem *m_showBookmarksMenuItem;
    wxMenuItem *m_bookmarkLockPosMenuItem;
    wxAcceleratorEntry *m_showBookmarksAccel;
    wxAcceleratorEntry *m_bookmarkLockPosAccel;
    wxMenuItem *m_connectMenuItem;
    wxMenuItem *m_loopMenuItem;
    wxMenuItem *m_guideMenuItem;
    wxMenuItem *m_stopMenuItem;
    wxMenuItem *m_brainMenuItem;
    wxMenuItem *m_cameraMenuItem;
    wxMenuItem *m_autoSelectStarMenuItem;
    wxMenuItem *m_takeDarksMenuItem;
    wxMenuItem *m_PlanetaryMenuItem;
    wxMenuItem *m_useDarksMenuItem;
    wxMenuItem *m_refineDefMapMenuItem;
    wxMenuItem *m_useDefectMapMenuItem;
    wxMenuItem *m_calibrationMenuItem;
    wxMenuItem *m_importCamCalMenuItem;
    wxMenuItem *m_upgradeMenuItem;
    wxAuiToolBarExt *MainToolbar;
    wxInfoBar *m_infoBar;
    wxComboBox    *Dur_Choice;
    wxCheckBox *HotPixel_Checkbox;
    wxHtmlHelpController *help;
    wxSlider *Gamma_Slider;
    AdvancedDialog *pAdvancedDialog;
    GraphLogWindow *pGraphLog;
    StatsWindow *pStatsWin;
    GraphStepguiderWindow *pStepGuiderGraph;
    GearDialog *pGearDialog;
    ProfileWindow *pProfile;
    TargetWindow *pTarget;
    wxWindow *pDriftTool;
    wxWindow *pPolarDriftTool;
    wxWindow *pStaticPaTool;
    wxWindow *pManualGuide;
    wxDialog *pStarCrossDlg;
    wxWindow *pNudgeLock;
    wxWindow *pCometTool;
    wxWindow *pPlanetTool;
    wxWindow *pGuidingAssistant;
    wxWindow *pierFlipToolWin;
    RefineDefMap *pRefineDefMap;
    wxDialog *pCalSanityCheckDlg;
    wxDialog *pCalReviewDlg;
    wxDialog *pCalibrationAssistant;
    bool CaptureActive; // Is camera looping captures?
    bool m_exposurePending; // exposure scheduled and not completed
    double Stretch_gamma;
    unsigned int m_frameCounter;
    wxDateTime m_guidingStarted;
    wxStopWatch m_guidingElapsed;
    Star::FindMode m_starFindMode;
    Star::FindMode m_StarFindMode_Saved;
    wxString m_StopReason;

    double m_minStarHFD;
    bool m_rawImageMode;
    bool m_rawImageModeWarningDone;
    wxSize m_prevDarkFrameSize;

    void RegisterTextCtrl(wxTextCtrl *ctrl);

    void OnMenuHighlight(wxMenuEvent& evt);
    void OnAnyMenu(wxCommandEvent& evt);
    void OnAnyMenuClose(wxMenuEvent& evt);
    void OnQuit(wxCommandEvent& evt);
    void OnClose(wxCloseEvent& evt);
    void OnAbout(wxCommandEvent& evt);
    void OnHelp(wxCommandEvent& evt);
    void OnOverlay(wxCommandEvent& evt);
    void OnOverlaySlitCoords(wxCommandEvent& evt);
    void OnUpgrade(wxCommandEvent& evt);
    void OnHelpOnline(wxCommandEvent& evt);
    void OnHelpLogFolder(wxCommandEvent& evt);
    void OnHelpUploadLogs(wxCommandEvent& evt);
    void OnInstructions(wxCommandEvent& evt);
    void OnSave(wxCommandEvent& evt);
    void OnSettings(wxCommandEvent& evt);
    void OnSelectGear(wxCommandEvent& evt);
    void OnButtonLoop(wxCommandEvent& evt);
    void OnButtonStop(wxCommandEvent& evt);
    void OnButtonAutoStar(wxCommandEvent& evt);
    void OnDark(wxCommandEvent& evt);
    void OnLoadDark(wxCommandEvent& evt);
    void OnLoadDefectMap(wxCommandEvent& evt);
    void GuideButtonClick(bool interactive, const wxString& context);
    void OnButtonGuide(wxCommandEvent& evt);
    void OnAdvanced(wxCommandEvent& evt);
    void OnIdle(wxIdleEvent& evt);
    void OnTestGuide(wxCommandEvent& evt);
    void OnStarCrossTest(wxCommandEvent& evt);
    void OnPierFlipTool(wxCommandEvent& evt);
    void OnEEGG(wxCommandEvent& evt);
    void OnDriftTool(wxCommandEvent& evt);
    void OnPolarDriftTool(wxCommandEvent& evt);
    void OnStaticPaTool(wxCommandEvent& evt);
    void OnCalibrationAssistant(wxCommandEvent& evt);
    void OnCometTool(wxCommandEvent& evt);
    void OnPlanetTool(wxCommandEvent& evt);
    void OnGuidingAssistant(wxCommandEvent& evt);
    void OnSetupCamera(wxCommandEvent& evt);
    void OnExposureDurationSelected(wxCommandEvent& evt);
    void OnGammaSlider(wxScrollEvent& evt);
    void OnSockServerEvent(wxSocketEvent& evt);
    void OnSockServerClientEvent(wxSocketEvent& evt);
    void HandleSockServerInput(wxSocketBase *sock);
    void OnServerMenu(wxCommandEvent& evt);
    void OnCharHook(wxKeyEvent& evt);
    void OnTextControlSetFocus(wxFocusEvent& evt);
    void OnTextControlKillFocus(wxFocusEvent& evt);
#if defined (GUIDE_INDI) || defined (INDI_CAMERA)
    void OnINDIConfig(wxCommandEvent& evt);
    void OnINDIDialog(wxCommandEvent& evt);
#endif
    void OnPanelClose(wxAuiManagerEvent& evt);
#if defined (V4L_CAMERA)
    void OnSaveSettings(wxCommandEvent& evt);
    void OnRestoreSettings(wxCommandEvent& evt);
#endif
    void OnGraph(wxCommandEvent& evt);
    void OnStats(wxCommandEvent& evt);
    void OnToolBar(wxCommandEvent& evt);
    void OnAoGraph(wxCommandEvent& evt);
    void OnStarProfile(wxCommandEvent& evt);
    void OnTarget(wxCommandEvent& evt);
    void OnRestoreWindows(wxCommandEvent& evt);
    void OnAutoStar(wxCommandEvent& evt);
    void OnBookmarksShow(wxCommandEvent& evt);
    void OnBookmarksSetAtLockPos(wxCommandEvent& evt);
    void OnBookmarksSetAtCurPos(wxCommandEvent& evt);
    void OnBookmarksClearAll(wxCommandEvent& evt);
    void OnRefineDefMap(wxCommandEvent& evt);
    void OnImportCamCal(wxCommandEvent& evt);

    void OnExposeComplete(wxThreadEvent& evt);
    void OnExposeComplete(usImage *image, bool err);
    void OnMoveComplete(wxThreadEvent& evt);

    void LoadProfileSettings();
    void UpdateTitle();

    const std::vector<int>& GetExposureDurations() const;
    bool SetCustomExposureDuration(int ms);
    bool GetExposureInfo(int *currExpMs, bool *autoExp) const;
    bool SetExposureDuration(int val, bool updateCustom = false);
    const AutoExposureCfg& GetAutoExposureCfg() const { return m_autoExp; }
    bool SetAutoExposureCfg(int minExp, int maxExp, double targetSNR);
    void ResetAutoExposure();
    void AdjustAutoExposure(double curSNR);
    static wxString ExposureDurationLabel(int exposure);
    const VarDelayCfg& GetVariableDelayConfig() const { return m_varDelayConfig; }
    void SetVariableDelayConfig(bool varDelayEnabled, int ShortDelayMS, int LongDelayMS);
    double GetDitherScaleFactor() const;
    bool SetDitherScaleFactor(double ditherScaleFactor);
    bool GetDitherRaOnly() const;
    bool SetDitherRaOnly(bool ditherRaOnly);
    static double GetDitherAmount(int ditherType);
    Star::FindMode GetStarFindMode() const;
    Star::FindMode SetStarFindMode(Star::FindMode mode);
    void SaveStarFindMode();
    void RestoreStarFindMode();

    bool GetRawImageMode() const;
    bool SetRawImageMode(bool force);

    bool StartServer(bool state);
    bool FlipCalibrationData();
    int RequestedExposureDuration();
    int GetFocalLength() const;
    bool GetAutoLoadCalibration() const;
    void SetAutoLoadCalibration(bool val);
    void LoadCalibration();
    static wxString GetDefaultFileDir();
    static wxString GetDarksDir();
    bool DarkLibExists(int profileId, bool showAlert);
    bool LoadDarkLibrary();
    void SaveDarkLibrary(const wxString& note);
    static void DeleteDarkLibraryFiles(int profileID);
    static wxString DarkLibFileName(int profileId);
    void SetDarkMenuState();
    bool LoadDarkHandler(bool checkIt);         // Use to also set menu item states
    void LoadDefectMapHandler(bool checkIt);
    void CheckDarkFrameGeometry();
    void UpdateStatusBarCalibrationStatus();
    void UpdateStatusBarStateLabels();
    void UpdateStatusBarStarInfo(double SNR, bool Saturated);
    void UpdateStatusBarGuiderInfo(const GuideStepInfo& info);
    void ClearStatusBarGuiderInfo();
    void UpdateStatsWindowScopePointing();
    static void PlaceWindowOnScreen(wxWindow *window, int x, int y);
    bool GetBeepForLostStar();
    void SetBeepForLostStar(bool beep);

    MyFrameConfigDialogPane *GetConfigDialogPane(wxWindow *pParent);
    MyFrameConfigDialogCtrlSet *GetConfigDlgCtrlSet(MyFrame *pFrame, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap);

    void OnRequestExposure(wxCommandEvent& evt);
    void OnRequestMountMove(wxCommandEvent& evt);

    void ScheduleExposure();

    void SchedulePrimaryMove(Mount *mount, const GuiderOffset& ofs, unsigned int moveOptions);
    void ScheduleSecondaryMove(Mount *mount, const GuiderOffset& ofs, unsigned int moveOptions);
    void ScheduleAxisMove(Mount *mount, const GUIDE_DIRECTION direction, int duration, unsigned int moveOptions);
    void ScheduleManualMove(Mount *mount, const GUIDE_DIRECTION direction, int duration);

    void StartCapturing();
    bool StopCapturing();
    bool StartSingleExposure(int duration, const wxRect& subframe);

    bool AutoSelectStar(const wxRect& roi = wxRect());

    void SetPaused(PauseType pause);

    void StartLoopingInteractive(const wxString& context);

    bool StartLooping(); // stop guiding and continue capturing, or, start capturing
    bool StartGuiding();
    bool Dither(double amount, bool raOnly);

    static bool GuidingRAOnly();
    double CurrentGuideError() const;
    double CurrentGuideErrorSmoothed() const;

    void NotifyUpdateButtonsStatus(); // can be called from any thread
    void UpdateButtonsStatus();
    void UpdateCameraSettings();

    static double GetPixelScale(double pixelSizeMicrons, int focalLengthMm, int binning);
    double GetCameraPixelScale() const;

    void Alert(const wxString& msg, int flags = wxICON_EXCLAMATION);
    void Alert(const wxString& msg, alert_fn *DontShowFn, const wxString& buttonLabel,  alert_fn *SpecialFn, long arg, bool showHelpButton = false, int flags = wxICON_EXCLAMATION);
    void SuppressableAlert(const wxString& configPropKey, const wxString& msg, alert_fn *dontShowFn, long arg, bool showHelpButton = false, int flags = wxICON_EXCLAMATION);
    void ClearAlert();
    void StatusMsg(const wxString& text);
    void StatusMsgNoTimeout(const wxString& text);
    wxString GetSettingsSummary() const;
    wxString ExposureDurationSummary() const;
    wxString PixelScaleSummary() const;
    void TryReconnect();

    double TimeSinceGuidingStarted() const;

    void NotifyGuidingStarted();
    void NotifyGuidingStopped();

    void SetDitherMode(DitherMode mode);
    DitherMode GetDitherMode() const;

    void HandleImageScaleChange(double NewToOldRatio);

    void NotifyGuidingParam(const wxString& name, double val);
    void NotifyGuidingParam(const wxString& name, int val);
    void NotifyGuidingParam(const wxString& name, bool val);
    void NotifyGuidingParam(const wxString& name, const wxString& val);
    void NotifyGuidingParam(const wxString& name, const wxString& val, bool ForceLog);

    void NotifyExposureChanged();

    void NotifyUpdaterStateChanged();

    // Following 2 functions are used by clients that need to size the spin control based on the max text width
    wxSpinCtrl *MakeSpinCtrl(wxWindow *parent, wxWindowID id = -1, const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxSP_ARROW_KEYS,
        int min = 0, int max = 100, int initial = 0, const wxString& name = wxT("wxSpinCtrl"));
    wxSpinCtrlDouble *MakeSpinCtrlDouble(wxWindow *parent, wxWindowID id = wxID_ANY, const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
        long style = wxSP_ARROW_KEYS | wxALIGN_RIGHT, double min = 0, double max = 100, double initial = 0,
        double inc = 1, const wxString& name = wxT("wxSpinCtrlDouble"));

private:
    wxCriticalSection m_CSpWorkerThread;
    WorkerThread *m_pPrimaryWorkerThread;
    WorkerThread *m_pSecondaryWorkerThread;

    wxSocketServer *SocketServer;
    wxTimer m_statusbarTimer;

    int m_exposureDuration;
    AutoExposureCfg m_autoExp;

    alert_fn *m_alertDontShowFn;
    alert_fn *m_alertSpecialFn;
    long m_alertFnArg;

    std::vector<time_t> m_cameraReconnectAttempts; // for rate-limiting camera reconnect attempts

    bool StartWorkerThread(WorkerThread*& pWorkerThread);
    bool StopWorkerThread(WorkerThread*& pWorkerThread);
    void OnStatusMsg(wxThreadEvent& event);
    void DoAlert(const alert_params& params);
    void OnAlertButton(wxCommandEvent& evt);
    void OnAlertHelp(wxCommandEvent& evt);
    void OnAlertFromThread(wxThreadEvent& event);
    void OnReconnectCameraFromThread(wxThreadEvent& event);
    void OnStatusBarTimerEvent(wxTimerEvent& evt);
    void OnUpdaterStateChanged(wxThreadEvent& event);
    void OnMessageBoxProxy(wxCommandEvent& evt);
    void SetupMenuBar();
    void SetupStatusBar();
    void SetupToolBar();
    void SetupKeyboardShortcuts();
    void SetupHelpFile();
    int GetTextWidth(wxControl *pControl, const wxString& string);
    void SetComboBoxWidth(wxComboBox *pComboBox, unsigned int extra);
    void FinishStop();
    void DoTryReconnect();

    // and of course, an event table
    DECLARE_EVENT_TABLE()
};

extern MyFrame *pFrame;

enum {
    MENU_SHOWHELP = 101,
    BEGIN_SCOPES,
      SCOPE_ASCOM,
      SCOPE_CAMERA,
      SCOPE_GPUSB,
      SCOPE_GPINT3BC,
      SCOPE_GPINT378,
      SCOPE_GPINT278,
      SCOPE_VOYAGER,
      SCOPE_EQUINOX,
      SCOPE_EQMAC,
      SCOPE_GCUSBST4,
      SCOPE_INDI,
    END_SCOPES,
    BEGIN_STEPGUIDERS,
      AO_NONE,
      AO_SXAO,
      AO_SIMULATOR,
    END_STEPGUIDERS,
    BUTTON_GEAR,
    BUTTON_CAL,
    BUTTON_LOOP,
    BUTTON_GUIDE,
    BUTTON_STOP,
    BUTTON_AUTOSTAR,
    BUTTON_DURATION,
    BUTTON_ADVANCED,
    BUTTON_PLANETARY,
    BUTTON_CAM_PROPERTIES,
    BUTTON_ALERT_ACTION,
    BUTTON_ALERT_CLOSE,
    BUTTON_ALERT_HELP,
    BUTTON_ALERT_DONTSHOW,
    GEAR_DIALOG_IDS_BEGIN,
        GEAR_PROFILES,
        GEAR_PROFILE_MANAGE,
        GEAR_PROFILE_NEW,
        GEAR_PROFILE_DELETE,
        GEAR_PROFILE_RENAME,
        GEAR_PROFILE_LOAD,
        GEAR_PROFILE_SAVE,
        GEAR_PROFILE_WIZARD,

        GEAR_CHOICE_CAMERA,
        GEAR_BUTTON_SELECT_CAMERA,
        MENU_SELECT_CAMERA_BEGIN, // a range of ids camera selection popup menu
        MENU_SELECT_CAMERA_END = MENU_SELECT_CAMERA_BEGIN + 10,
        GEAR_BUTTON_SETUP_CAMERA,
        GEAR_BUTTON_CONNECT_CAMERA,
        GEAR_BUTTON_DISCONNECT_CAMERA,

        GEAR_CHOICE_SCOPE,
        GEAR_BUTTON_SETUP_SCOPE,
        GEAR_BUTTON_CONNECT_SCOPE,
        GEAR_BUTTON_DISCONNECT_SCOPE,

        GEAR_CHOICE_AUXSCOPE,
        GEAR_BUTTON_SETUP_AUXSCOPE,
        GEAR_BUTTON_CONNECT_AUXSCOPE,
        GEAR_BUTTON_DISCONNECT_AUXSCOPE,

        GEAR_BUTTON_MORE,

        GEAR_CHOICE_STEPGUIDER,
        GEAR_BUTTON_SETUP_STEPGUIDER,
        GEAR_BUTTON_CONNECT_STEPGUIDER,
        GEAR_BUTTON_DISCONNECT_STEPGUIDER,

        GEAR_CHOICE_ROTATOR,
        GEAR_BUTTON_SETUP_ROTATOR,
        GEAR_BUTTON_CONNECT_ROTATOR,
        GEAR_BUTTON_DISCONNECT_ROTATOR,

        GEAR_BUTTON_CONNECT_ALL,
        GEAR_BUTTON_DISCONNECT_ALL,
    GEAR_DIALOG_IDS_END,
    CTRL_GAMMA,
    WIN_VFW,  // Dummy event to capture VFW streams
    MGUIDE1_UP,
    MGUIDE1_DOWN,
    MGUIDE1_RIGHT,
    MGUIDE1_LEFT,
    MGUIDE2_UP,
    MGUIDE2_DOWN,
    MGUIDE2_RIGHT,
    MGUIDE2_LEFT,
    MENU_CONNECT,
    MENU_LOOP,
    MENU_GUIDE,
    MENU_STOP,
    MENU_BRAIN,
    MENU_CAM_SETTINGS,
    MENU_MANGUIDE,
    MENU_XHAIR0,
    MENU_XHAIR1,
    MENU_XHAIR2,
    MENU_XHAIR3,
    MENU_XHAIR4,
    MENU_XHAIR5,
    MENU_SLIT_OVERLAY_COORDS,
    MENU_TAKEDARKS,
    MENU_SERVER,
    MENU_TOOLBAR,
    MENU_GRAPH,
    MENU_STATS,
    MENU_AO_GRAPH,
    MENU_STARPROFILE,
    MENU_RESTORE_WINDOWS,
    MENU_TARGET,
    MENU_AUTOSTAR,
    MENU_DRIFTTOOL,
    MENU_POLARDRIFTTOOL,
    MENU_STATICPATOOL,
    MENU_COMETTOOL,
    MENU_PLANETARY,
    MENU_GUIDING_ASSISTANT,
    MENU_SAVESETTINGS,
    MENU_LOADSETTINGS,
    MENU_LOADDARK,
    MENU_LOADDEFECTMAP,
    MENU_REFINEDEFECTMAP,
    MENU_IMPORTCAMCAL,
    MENU_INDICONFIG,
    MENU_INDIDIALOG,
    MENU_V4LSAVESETTINGS,
    MENU_V4LRESTORESETTINGS,
    BUTTON_GRAPH_LENGTH,
    BUTTON_GRAPH_HEIGHT,
    BUTTON_GRAPH_SETTINGS,
        GRAPH_RADEC,
        GRAPH_DXDY,
        GRAPH_ARCSECS,
        GRAPH_PIXELS,
        GRAPH_STAR_MASS,
        GRAPH_STAR_SNR,
        GRAPH_RADX_COLOR,
        GRAPH_DECDY_COLOR,
        GRAPH_SCALE_CORR,
    BUTTON_GRAPH_CLEAR,
    TARGET_ENABLE_REF_CIRCLE,
    TARGET_REF_CIRCLE_RADIUS,
    MENU_LENGTH_BEGIN, // a range of ids for history size selection popup menus
    MENU_LENGTH_END = MENU_LENGTH_BEGIN + 10,
    MENU_HEIGHT_BEGIN, // a range of ids for height size selection popup menus
    MENU_HEIGHT_END = MENU_HEIGHT_BEGIN + 10,
    CHECKBOX_GRAPH_TRENDLINES,
    CHECKBOX_GRAPH_CORRECTIONS,
    BUTTON_GRAPH_ZOOMIN,
    BUTTON_GRAPH_ZOOMOUT,
    ABOUT_LINK,
    MENU_CALIBRATIONASSIST,
    EEGG_RESTORECAL,
    EEGG_MANUALCAL,
    EEGG_CLEARCAL,
    EEGG_REVIEWCAL,
    EEGG_MANUALLOCK,
    EEGG_COMET_TOOL,
    EEGG_STICKY_LOCK,
    EEGG_FLIPCAL,
    STAR_MASS_ENABLE,
    MULTI_STAR_ENABLE,
    MENU_BOOKMARKS_SHOW,
    MENU_BOOKMARKS_SET_AT_LOCK,
    MENU_BOOKMARKS_SET_AT_STAR,
    MENU_BOOKMARKS_CLEAR_ALL,
    MENU_STARCROSS_TEST,
    MENU_PIERFLIP_TOOL,
    MENU_HELP_UPGRADE,
    MENU_HELP_ONLINE,
    MENU_HELP_UPLOAD_LOGS,
    MENU_HELP_LOG_FOLDER,
    GA_REVIEW_BUTTON,
    GA_REVIEW_ITEMS_BASE,
    GA_REVIEW_ITEMS_LIMIT = GA_REVIEW_ITEMS_BASE + 4,
};

enum {
    SOCK_SERVER_ID = 100,
    SOCK_SERVER_CLIENT_ID,
    EVENT_SERVER_ID,
    EVENT_SERVER_CLIENT_ID,
};

wxDECLARE_EVENT(APPSTATE_NOTIFY_EVENT, wxCommandEvent);

inline static int StringWidth(const wxWindow *window, const wxString& s)
{
    int width, height;
    window->GetTextExtent(s, &width, &height);
    return width;
}

inline static wxSize StringSize(const wxWindow *window, const wxString& s, int extra = 0)
{
    return wxSize(StringWidth(window, s) + extra, -1);
}

inline double MyFrame::GetPixelScale(double pixelSizeMicrons, int focalLengthMm, int binning)
{
    return 206.265 * pixelSizeMicrons * (double) binning / (double) focalLengthMm;
}

inline double MyFrame::TimeSinceGuidingStarted() const
{
    return (double) m_guidingElapsed.Time() / 1000.0;
}

inline Star::FindMode MyFrame::GetStarFindMode() const
{
    return m_starFindMode;
}

inline bool MyFrame::GetRawImageMode() const
{
    return m_rawImageMode;
}

inline DitherMode MyFrame::GetDitherMode() const
{
    return m_ditherMode;
}

inline NOISE_REDUCTION_METHOD MyFrame::GetNoiseReductionMethod() const
{
    return m_noiseReductionMethod;
}

inline double MyFrame::GetDitherScaleFactor() const
{
    return m_ditherScaleFactor;
}

inline bool MyFrame::GetDitherRaOnly() const
{
    return m_ditherRaOnly;
}

inline bool MyFrame::GetAutoLoadCalibration() const
{
    return m_autoLoadCalibration;
}

inline bool MyFrame::GetServerMode() const
{
    return m_serverMode;
}

inline int MyFrame::GetTimeLapse() const
{
    return m_timeLapse;
}

inline int MyFrame::GetFocalLength() const
{
    return m_focalLength;
}

#endif /* MYFRAME_H_INCLUDED */
