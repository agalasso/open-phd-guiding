/*
 *  myframe.cpp
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

#include "phd.h"

#include "aui_controls.h"
#include "comet_tool.h"
#include "planetary_tool.h"
#include "config_indi.h"
#include "guiding_assistant.h"
#include "phdupdate.h"
#include "pierflip_tool.h"
#include "Refine_DefMap.h"

#include <algorithm>
#include <memory>

#include <wx/filesys.h>
#include <wx/fs_zip.h>
#include <wx/artprov.h>
#include <wx/dirdlg.h>
#include <wx/dnd.h>
#include <wx/textwrapper.h>
#include "wx/valnum.h"

static const int DefaultNoiseReductionMethod = 0;
static const double DefaultDitherScaleFactor = 1.00;
static const bool DefaultDitherRaOnly = false;
static const DitherMode DefaultDitherMode = DITHER_RANDOM;
static const bool DefaultServerMode = true;
static const int DefaultTimelapse = 0;
static const int DefaultFocalLength = 0;
static const int DefaultExposureDuration = 1000;
static const int DefaultAutoExpMin = 1000;
static const int DefaultAutoExpMax = 5000;
static const double DefaultAutoExpSNR = 6.0;

wxDEFINE_EVENT(REQUEST_EXPOSURE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(REQUEST_MOUNT_MOVE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WXMESSAGEBOX_PROXY_EVENT, wxCommandEvent);
wxDEFINE_EVENT(STATUSBAR_ENQUEUE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(STATUSBAR_TIMER_EVENT, wxTimerEvent);
wxDEFINE_EVENT(SET_STATUS_TEXT_EVENT, wxThreadEvent);
wxDEFINE_EVENT(ALERT_FROM_THREAD_EVENT, wxThreadEvent);
wxDEFINE_EVENT(RECONNECT_CAMERA_EVENT, wxThreadEvent);
wxDEFINE_EVENT(UPDATER_EVENT, wxThreadEvent);

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU_HIGHLIGHT_ALL(MyFrame::OnMenuHighlight)
    EVT_MENU_CLOSE(MyFrame::OnAnyMenuClose)
    EVT_MENU(wxID_ANY, MyFrame::OnAnyMenu)
    EVT_MENU(wxID_EXIT,  MyFrame::OnQuit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_MENU(EEGG_RESTORECAL, MyFrame::OnEEGG)
    EVT_MENU(EEGG_MANUALCAL, MyFrame::OnEEGG)
    EVT_MENU(EEGG_CLEARCAL, MyFrame::OnEEGG)
    EVT_MENU(EEGG_REVIEWCAL, MyFrame::OnEEGG)
    EVT_MENU(MENU_CALIBRATIONASSIST, MyFrame::OnCalibrationAssistant)
    EVT_MENU(EEGG_MANUALLOCK, MyFrame::OnEEGG)
    EVT_MENU(EEGG_STICKY_LOCK, MyFrame::OnEEGG)
    EVT_MENU(EEGG_FLIPCAL, MyFrame::OnEEGG)
    EVT_MENU(MENU_DRIFTTOOL, MyFrame::OnDriftTool)
    EVT_MENU(MENU_POLARDRIFTTOOL, MyFrame::OnPolarDriftTool)
    EVT_MENU(MENU_STATICPATOOL, MyFrame::OnStaticPaTool)
    EVT_MENU(MENU_COMETTOOL, MyFrame::OnCometTool)
    EVT_MENU(MENU_PLANETARY, MyFrame::OnPlanetTool)
    EVT_MENU(MENU_GUIDING_ASSISTANT, MyFrame::OnGuidingAssistant)
    EVT_MENU(MENU_HELP_UPGRADE, MyFrame::OnUpgrade)
    EVT_MENU(MENU_HELP_ONLINE, MyFrame::OnHelpOnline)
    EVT_MENU(MENU_HELP_LOG_FOLDER, MyFrame::OnHelpLogFolder)
    EVT_MENU(MENU_HELP_UPLOAD_LOGS, MyFrame::OnHelpUploadLogs)
    EVT_MENU(wxID_HELP_PROCEDURES, MyFrame::OnInstructions)
    EVT_MENU(wxID_HELP_CONTENTS,MyFrame::OnHelp)
    EVT_MENU(wxID_SAVE, MyFrame::OnSave)
    EVT_MENU(MENU_TAKEDARKS,MyFrame::OnDark)
    EVT_MENU(MENU_LOADDARK,MyFrame::OnLoadDark)
    EVT_MENU(MENU_LOADDEFECTMAP,MyFrame::OnLoadDefectMap)
    EVT_MENU(MENU_MANGUIDE, MyFrame::OnTestGuide)
    EVT_MENU(MENU_STARCROSS_TEST, MyFrame::OnStarCrossTest)
    EVT_MENU(MENU_PIERFLIP_TOOL, MyFrame::OnPierFlipTool)
    EVT_MENU(MENU_XHAIR0, MyFrame::OnOverlay)
    EVT_MENU(MENU_XHAIR1,MyFrame::OnOverlay)
    EVT_MENU(MENU_XHAIR2,MyFrame::OnOverlay)
    EVT_MENU(MENU_XHAIR3,MyFrame::OnOverlay)
    EVT_MENU(MENU_XHAIR4,MyFrame::OnOverlay)
    EVT_MENU(MENU_XHAIR5,MyFrame::OnOverlay)
    EVT_MENU(MENU_SLIT_OVERLAY_COORDS, MyFrame::OnOverlaySlitCoords)
    EVT_MENU(MENU_BOOKMARKS_SHOW, MyFrame::OnBookmarksShow)
    EVT_MENU(MENU_BOOKMARKS_SET_AT_LOCK, MyFrame::OnBookmarksSetAtLockPos)
    EVT_MENU(MENU_BOOKMARKS_SET_AT_STAR, MyFrame::OnBookmarksSetAtCurPos)
    EVT_MENU(MENU_BOOKMARKS_CLEAR_ALL, MyFrame::OnBookmarksClearAll)
    EVT_MENU(MENU_REFINEDEFECTMAP,MyFrame::OnRefineDefMap)
    EVT_MENU(MENU_IMPORTCAMCAL,MyFrame::OnImportCamCal)

    EVT_CHAR_HOOK(MyFrame::OnCharHook)

#if defined (V4L_CAMERA)
    EVT_MENU(MENU_V4LSAVESETTINGS, MyFrame::OnSaveSettings)
    EVT_MENU(MENU_V4LRESTORESETTINGS, MyFrame::OnRestoreSettings)
#endif

    EVT_MENU(MENU_TOOLBAR,MyFrame::OnToolBar)
    EVT_MENU(MENU_GRAPH, MyFrame::OnGraph)
    EVT_MENU(MENU_STATS, MyFrame::OnStats)
    EVT_MENU(MENU_AO_GRAPH, MyFrame::OnAoGraph)
    EVT_MENU(MENU_TARGET, MyFrame::OnTarget)
    EVT_MENU(MENU_SERVER, MyFrame::OnServerMenu)
    EVT_MENU(MENU_STARPROFILE, MyFrame::OnStarProfile)
    EVT_MENU(MENU_RESTORE_WINDOWS, MyFrame::OnRestoreWindows)
    EVT_MENU(MENU_AUTOSTAR,MyFrame::OnAutoStar)
    EVT_TOOL(BUTTON_GEAR,MyFrame::OnSelectGear)
    EVT_MENU(MENU_CONNECT,MyFrame::OnSelectGear)
    EVT_TOOL(BUTTON_LOOP, MyFrame::OnButtonLoop)
    EVT_MENU(MENU_LOOP, MyFrame::OnButtonLoop)
    EVT_TOOL(BUTTON_STOP, MyFrame::OnButtonStop)
    EVT_TOOL(BUTTON_AUTOSTAR, MyFrame::OnButtonAutoStar)
    EVT_MENU(MENU_STOP, MyFrame::OnButtonStop)
    EVT_TOOL(BUTTON_ADVANCED, MyFrame::OnAdvanced)
    EVT_TOOL(BUTTON_PLANETARY, MyFrame::OnPlanetTool)
    EVT_MENU(MENU_BRAIN, MyFrame::OnAdvanced)
    EVT_TOOL(BUTTON_GUIDE,MyFrame::OnButtonGuide)
    EVT_MENU(MENU_GUIDE,MyFrame::OnButtonGuide)
    EVT_MENU(BUTTON_ALERT_CLOSE,MyFrame::OnAlertButton) // Bit of a hack -- not actually on the menu but need an event to accelerate
    EVT_TOOL(BUTTON_CAM_PROPERTIES,MyFrame::OnSetupCamera)
    EVT_MENU(MENU_CAM_SETTINGS, MyFrame::OnSetupCamera)
    EVT_COMMAND_SCROLL(CTRL_GAMMA, MyFrame::OnGammaSlider)
    EVT_COMBOBOX(BUTTON_DURATION, MyFrame::OnExposureDurationSelected)
    EVT_SOCKET(SOCK_SERVER_ID, MyFrame::OnSockServerEvent)
    EVT_SOCKET(SOCK_SERVER_CLIENT_ID, MyFrame::OnSockServerClientEvent)
    EVT_CLOSE(MyFrame::OnClose)
    EVT_THREAD(MYFRAME_WORKER_THREAD_EXPOSE_COMPLETE, MyFrame::OnExposeComplete)
    EVT_THREAD(MYFRAME_WORKER_THREAD_MOVE_COMPLETE, MyFrame::OnMoveComplete)

    EVT_COMMAND(wxID_ANY, REQUEST_EXPOSURE_EVENT, MyFrame::OnRequestExposure)
    EVT_COMMAND(wxID_ANY, WXMESSAGEBOX_PROXY_EVENT, MyFrame::OnMessageBoxProxy)

    EVT_THREAD(SET_STATUS_TEXT_EVENT, MyFrame::OnStatusMsg)
    EVT_THREAD(ALERT_FROM_THREAD_EVENT, MyFrame::OnAlertFromThread)
    EVT_THREAD(RECONNECT_CAMERA_EVENT, MyFrame::OnReconnectCameraFromThread)
    EVT_THREAD(UPDATER_EVENT, MyFrame::OnUpdaterStateChanged)
    EVT_COMMAND(wxID_ANY, REQUEST_MOUNT_MOVE_EVENT, MyFrame::OnRequestMountMove)
    EVT_TIMER(STATUSBAR_TIMER_EVENT, MyFrame::OnStatusBarTimerEvent)

    EVT_AUI_PANE_CLOSE(MyFrame::OnPanelClose)
wxEND_EVENT_TABLE()

struct FileDropTarget : public wxFileDropTarget
{
    FileDropTarget() { }

    wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult defResult)
    {
        if (!pFrame->CaptureActive)
            return wxDragResult::wxDragCopy;
        return wxDragResult::wxDragNone;
    }

    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
    {
        if (filenames.size() != 1)
            return false;

        if (pFrame->CaptureActive)
            return false;

        std::unique_ptr<usImage> img(new usImage());

        if (img->Load(filenames[0]))
            return false;

        img->CalcStats();
        pFrame->pGuider->DisplayImage(img.release());

        return true;
    }
};

// ---------------------- Main Frame -------------------------------------
// frame constructor
MyFrame::MyFrame()
    :
    wxFrame(nullptr, wxID_ANY, wxEmptyString),
    pGuider(nullptr),
    pPlanetTool(nullptr),
    m_showBookmarksAccel(0),
    m_bookmarkLockPosAccel(0),
    pStatsWin(nullptr)
{
    m_mgr.SetManagedWindow(this);

    m_frameCounter = 0;
    m_pPrimaryWorkerThread = nullptr;
    StartWorkerThread(m_pPrimaryWorkerThread);
    m_pSecondaryWorkerThread = nullptr;
    StartWorkerThread(m_pSecondaryWorkerThread);

    m_statusbarTimer.SetOwner(this, STATUSBAR_TIMER_EVENT);

    SocketServer = nullptr;

    bool serverMode = pConfig->Global.GetBoolean("/ServerMode", DefaultServerMode);
    SetServerMode(serverMode);

    m_sampling = 1.0;

    #include "icons/phd2_128.png.h"
    wxBitmap phd2(wxBITMAP_PNG_FROM_DATA(phd2_128));
    wxIcon icon;
    icon.CopyFromBitmap(phd2);
    SetIcon(icon);

    //SetIcon(wxIcon(_T("progicon")));
    SetBackgroundColour(*wxLIGHT_GREY);

    // Setup menus
    SetupMenuBar();

    // Setup button panel
    SetupToolBar();

    // Setup Status bar
    SetupStatusBar();

    LoadProfileSettings();

    // Setup container window for alert message info bar and guider window
    wxWindow *guiderWin = new wxWindow(this, wxID_ANY);
    wxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    m_infoBar = new wxInfoBar(guiderWin);
    m_infoBar->Connect(BUTTON_ALERT_ACTION, wxEVT_BUTTON,
        wxCommandEventHandler(MyFrame::OnAlertButton), nullptr, this);
    m_infoBar->Connect(BUTTON_ALERT_DONTSHOW, wxEVT_BUTTON,
        wxCommandEventHandler(MyFrame::OnAlertButton), nullptr, this);
    m_infoBar->Connect(BUTTON_ALERT_CLOSE, wxEVT_BUTTON,
        wxCommandEventHandler(MyFrame::OnAlertButton), nullptr, this);
    m_infoBar->Connect(BUTTON_ALERT_HELP, wxEVT_BUTTON,
        wxCommandEventHandler(MyFrame::OnAlertHelp), nullptr, this);

    sizer->Add(m_infoBar, wxSizerFlags().Expand());

    pGuider = new GuiderMultiStar(guiderWin);
    sizer->Add(pGuider, wxSizerFlags().Proportion(1).Expand());

    guiderWin->SetSizer(sizer);

    pGuider->LoadProfileSettings();

    bool sticky = pConfig->Global.GetBoolean("/StickyLockPosition", false);
    pGuider->SetLockPosIsSticky(sticky);
    tools_menu->Check(EEGG_STICKY_LOCK, sticky);

    SetMinSize(wxSize(300, 300));
    SetSize(800, 600);

    wxString geometry = pConfig->Global.GetString("/geometry", wxEmptyString);
    wxArrayString fields = wxSplit(geometry, ';');
    long w, h, x, y;
    if (fields.size() == 5 &&
        fields[1].ToLong(&w) &&
        fields[2].ToLong(&h) &&
        fields[3].ToLong(&x) &&
        fields[4].ToLong(&y))
    {
        wxSize screen = wxGetDisplaySize();
        if (x + w <= screen.GetWidth() &&
            x >= 0 &&
            y + h <= screen.GetHeight() &&
            y >= 0)
        {
            SetSize(w, h);
            SetPosition(wxPoint(x, y));
        }
        else
        {
            // looks like screen size changed, ignore position and revert to default size
        }
        if (fields[0] == "1")
        {
            Maximize();
        }
    }

    // Setup some keyboard shortcuts
    SetupKeyboardShortcuts();

    SetDropTarget(new FileDropTarget());

    m_mgr.AddPane(MainToolbar, wxAuiPaneInfo().
        Name(_T("MainToolBar")).Caption(_T("Main tool bar")).
        ToolbarPane().Bottom());

    guiderWin->SetMinSize(wxSize(XWinSize,YWinSize));
    guiderWin->SetSize(wxSize(XWinSize,YWinSize));
    m_mgr.AddPane(guiderWin, wxAuiPaneInfo().
        Name(_T("Guider")).Caption(_T("Guider")).
        CenterPane().MinSize(wxSize(XWinSize,YWinSize)));

    pGraphLog = new GraphLogWindow(this);
    m_mgr.AddPane(pGraphLog, wxAuiPaneInfo().
        Name(_T("GraphLog")).Caption(_("History")).
        Hide());

    pStatsWin = new StatsWindow(this);
    m_mgr.AddPane(pStatsWin, wxAuiPaneInfo().
        Name(_T("Stats")).Caption(_("Guide Stats")).
        Hide());

    pStepGuiderGraph = new GraphStepguiderWindow(this);
    m_mgr.AddPane(pStepGuiderGraph, wxAuiPaneInfo().
        Name(_T("AOPosition")).Caption(_("AO Position")).
        Hide());

    pProfile = new ProfileWindow(this);
    m_mgr.AddPane(pProfile, wxAuiPaneInfo().
        Name(_T("Profile")).Caption(_("Star Profile")).
        Hide());

    pTarget = new TargetWindow(this);
    m_mgr.AddPane(pTarget, wxAuiPaneInfo().
        Name(_T("Target")).Caption(_("Target")).
        Hide());

    pAdvancedDialog = new AdvancedDialog(this);

    pGearDialog = new GearDialog(this);

    pDriftTool = nullptr;
    pPolarDriftTool = nullptr;
    pStaticPaTool = nullptr;
    pManualGuide = nullptr;
    pStarCrossDlg = nullptr;
    pNudgeLock = nullptr;
    pCometTool = nullptr;
    pPlanetTool = nullptr;
    pGuidingAssistant = nullptr;
    pRefineDefMap = nullptr;
    pCalSanityCheckDlg = nullptr;
    pCalReviewDlg = nullptr;
    pCalibrationAssistant = nullptr;
    pierFlipToolWin = nullptr;
    m_starFindMode = Star::FIND_CENTROID;
    m_StarFindMode_Saved = m_starFindMode;
    m_StopReason = wxEmptyString;
    m_rawImageMode = false;
    m_rawImageModeWarningDone = false;

    UpdateTitle();

    SetupHelpFile();

    if (m_serverMode)
    {
        tools_menu->Check(MENU_SERVER,true);
        StartServer(true);
    }

    #include "xhair.xpm"
    wxImage Cursor = wxImage(mac_xhair);
    Cursor.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X,8);
    Cursor.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y,8);
    pGuider->SetCursor(wxCursor(Cursor));

    m_continueCapturing = false;
    CaptureActive     = false;
    m_exposurePending = false;

    m_singleExposure.enabled = false;
    m_singleExposure.duration = 0;

    m_mgr.GetArtProvider()->SetColour(wxAUI_DOCKART_BACKGROUND_COLOUR, *wxBLACK);
    m_mgr.GetArtProvider()->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_VERTICAL);
    m_mgr.GetArtProvider()->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, wxColour(0, 153, 255));
    m_mgr.GetArtProvider()->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, *wxBLACK);
    m_mgr.GetArtProvider()->SetColor(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, *wxWHITE);

    wxString perspective = pConfig->Global.GetString("/perspective", wxEmptyString);
    if (perspective != wxEmptyString)
    {
        m_mgr.LoadPerspective(perspective);
        m_mgr.GetPane(_T("MainToolBar")).Caption(_T("Main tool bar"));
        m_mgr.GetPane(_T("Guider")).Caption(_T("Guider"));
        m_mgr.GetPane(_T("GraphLog")).Caption(_("History"));
        m_mgr.GetPane(_T("Stats")).Caption(_("Guide Stats"));
        m_mgr.GetPane(_T("AOPosition")).Caption(_("AO Position"));
        m_mgr.GetPane(_T("Profile")).Caption(_("Star Profile"));
        m_mgr.GetPane(_T("Target")).Caption(_("Target"));
        m_mgr.GetPane(_T("Guider")).PaneBorder(false);
    }

    bool panel_state;

    panel_state = m_mgr.GetPane(_T("MainToolBar")).IsShown();
    Menubar->Check(MENU_TOOLBAR, panel_state);

    panel_state = m_mgr.GetPane(_T("GraphLog")).IsShown();
    pGraphLog->SetState(panel_state);
    Menubar->Check(MENU_GRAPH, panel_state);

    panel_state = m_mgr.GetPane(_T("Stats")).IsShown();
    pStatsWin->SetState(panel_state);
    Menubar->Check(MENU_STATS, panel_state);

    panel_state = m_mgr.GetPane(_T("AOPosition")).IsShown();
    pStepGuiderGraph->SetState(panel_state);
    Menubar->Check(MENU_AO_GRAPH, panel_state);

    panel_state = m_mgr.GetPane(_T("Profile")).IsShown();
    pProfile->SetState(panel_state);
    Menubar->Check(MENU_STARPROFILE, panel_state);

    panel_state = m_mgr.GetPane(_T("Target")).IsShown();
    pTarget->SetState(panel_state);
    Menubar->Check(MENU_TARGET, panel_state);

    // update MainToolbar size to reflect its actual dimensions
    wxAuiPaneInfo& MenubarInfo = m_mgr.GetPane(_T("MainToolBar"));
    MenubarInfo.Layer(MainToolbar->GetToolPanes());
    MenubarInfo.BestSize(MainToolbar->GetAbsoluteMinSize());

    // update panes based on current panes state
    m_mgr.Update();

    // this forces force a resize of MainToolbar in case size changed from the saved perspective
    MainToolbar->Realize();
}

MyFrame::~MyFrame()
{
    delete pGearDialog;
    pGearDialog = nullptr;

    pAdvancedDialog->Destroy();

    if (pDriftTool)
        pDriftTool->Destroy();
    if (pPolarDriftTool)
        pPolarDriftTool->Destroy();
    if (pStaticPaTool)
        pStaticPaTool->Destroy();
    if (pRefineDefMap)
        pRefineDefMap->Destroy();
    if (pCalSanityCheckDlg)
        pCalSanityCheckDlg->Destroy();
    if (pCalReviewDlg)
        pCalReviewDlg->Destroy();
    if (pStarCrossDlg)
        pStarCrossDlg->Destroy();
    if (pierFlipToolWin)
        pierFlipToolWin->Destroy();
    if (pPlanetTool)
        pPlanetTool->Destroy();

    m_mgr.UnInit();

    delete m_showBookmarksAccel;
    delete m_bookmarkLockPosAccel;
}

void MyFrame::UpdateTitle()
{
    int inst = wxGetApp().GetInstanceNumber();
    wxString prof = pConfig->GetCurrentProfile();

    wxString title = inst > 1 ?
        wxString::Format(_T("%s(#%d) %s - %s"), APPNAME, inst, FULLVER, prof) :
        wxString::Format(_T("%s %s - %s"), APPNAME, FULLVER, prof);

    SetTitle(title);
}

void MyFrame::SetupMenuBar()
{
    wxMenu *file_menu = new wxMenu();
    file_menu->Append(wxID_SAVE, _("&Save Image..."), _("Save current image"));
    file_menu->Append(wxID_EXIT, _("E&xit\tAlt-X"), _("Quit this program"));

    wxMenu *guide_menu = new wxMenu();
    m_connectMenuItem = guide_menu->Append(MENU_CONNECT, _("&Connect Equipment\tCtrl-C"), _("Connect or disconnect equipment"));
    m_loopMenuItem = guide_menu->Append(MENU_LOOP, _("&Loop Exposures\tCtrl-L"), _("Begin looping exposures"));
    m_loopMenuItem->Enable(false);
    m_guideMenuItem = guide_menu->Append(MENU_GUIDE, _("&Guide\tCtrl-G"), _("Begin guiding"));
    m_guideMenuItem->Enable(false);
    m_stopMenuItem = guide_menu->Append(MENU_STOP, _("&Stop\tCtrl-S"), _("Stop looping and guiding"));
    m_stopMenuItem->Enable(false);
    m_brainMenuItem = guide_menu->Append(MENU_BRAIN, _("&Advanced Settings\tCtrl-A"), _("Advanced Settings"));
    m_cameraMenuItem = guide_menu->Append(MENU_CAM_SETTINGS, _("&Camera Settings"), _("Camera settings"));
    m_cameraMenuItem->Enable(false);

    tools_menu = new wxMenu;
    tools_menu->Append(MENU_MANGUIDE, _("&Manual Guide"), _("Manual / test guide dialog"));
    m_autoSelectStarMenuItem = tools_menu->Append(MENU_AUTOSTAR, _("&Auto-select Star\tAlt-S"), _("Automatically select star"));
    tools_menu->Append(MENU_CALIBRATIONASSIST, _("Calibration Assistant..."), _("Slew to a preferred calibration position"));
    tools_menu->Append(EEGG_REVIEWCAL, _("&Review Calibration Data\tAlt-C"), _("Review calibration data from last successful calibration"));

    wxMenu *calib_menu = new wxMenu;
    calib_menu->Append(EEGG_RESTORECAL, _("Restore Calibration Data..."), _("Restore calibration data from last successful calibration"));
    calib_menu->Append(EEGG_MANUALCAL, _("Enter Calibration Data..."), _("Manually enter the calibration data"));
    calib_menu->Append(EEGG_FLIPCAL, _("Flip Calibration Now"), _("Flip the calibration data now"));
    calib_menu->Append(EEGG_CLEARCAL, _("Clear Calibration Data..."), _("Clear calibration data currently in use"));
    m_calibrationMenuItem = tools_menu->AppendSubMenu(calib_menu, _("Modify Calibration"));
    m_calibrationMenuItem->Enable(false);

    tools_menu->Append(EEGG_MANUALLOCK, _("Adjust &Lock Position"), _("Adjust the lock position"));
    tools_menu->Append(MENU_COMETTOOL, _("&Comet Tracking"), _("Run the Comet Tracking tool"));
    m_PlanetaryMenuItem = tools_menu->AppendCheckItem(MENU_PLANETARY, _("&Planetary Guiding\tCtrl-P"), _("Run the Planetary Guiding tool"));
    tools_menu->Append(MENU_STARCROSS_TEST, _("Star-Cross Test"), _("Run a star-cross test for mount diagnostics"));
    tools_menu->Append(MENU_PIERFLIP_TOOL, _("Calibrate meridian flip"), _("Automatically determine the correct meridian flip settings"));
    tools_menu->Append(MENU_GUIDING_ASSISTANT, _("&Guiding Assistant"), _("Run the Guiding Assistant"));
    tools_menu->Append(MENU_DRIFTTOOL, _("&Drift Align"), _("Align by analysing star drift near the celestial equator (Accurate)"));
    tools_menu->Append(MENU_POLARDRIFTTOOL, _("&Polar Drift Align"), _("Align by analysing star drift near the celestial pole (Simple)"));
    tools_menu->Append(MENU_STATICPATOOL, _("&Static Polar Align"), _("Align by measuring the RA axis offset from the celestial pole (Fast)"));
    tools_menu->AppendSeparator();
    tools_menu->AppendCheckItem(MENU_SERVER,_("Enable Server"),_("Enable PHD2 server capability"));
    tools_menu->AppendCheckItem(EEGG_STICKY_LOCK,_("Sticky Lock Position"),_("Keep the same lock position when guiding starts"));

    view_menu = new wxMenu();
    view_menu->AppendCheckItem(MENU_TOOLBAR,_("Display Toolbar"),_("Enable / disable tool bar"));
    view_menu->AppendCheckItem(MENU_GRAPH,_("Display &Graph"),_("Enable / disable graph"));
    view_menu->AppendCheckItem(MENU_STATS, _("Display &Stats"), _("Enable / disable guide stats"));
    view_menu->AppendCheckItem(MENU_AO_GRAPH, _("Display &AO Graph"), _("Enable / disable AO graph"));
    view_menu->AppendCheckItem(MENU_TARGET,_("Display &Target"),_("Enable / disable target"));
    view_menu->AppendCheckItem(MENU_STARPROFILE,_("Display Star &Profile"),_("Enable / disable star profile view"));
    view_menu->AppendSeparator();
    view_menu->AppendRadioItem(MENU_XHAIR0, _("&No Overlay"),_("No additional crosshairs"));
    view_menu->AppendRadioItem(MENU_XHAIR1, _("&Bullseye"),_("Centered bullseye overlay"));
    view_menu->AppendRadioItem(MENU_XHAIR2, _("&Fine Grid"),_("Grid overlay"));
    view_menu->AppendRadioItem(MENU_XHAIR3, _("&Coarse Grid"),_("Grid overlay"));
    view_menu->AppendRadioItem(MENU_XHAIR4, _("&RA/Dec"),_("RA and Dec overlay"));
    view_menu->AppendRadioItem(MENU_XHAIR5, _("Spectrograph S&lit"), _("Spectrograph slit overlay"));
    view_menu->AppendSeparator();
    view_menu->Append(MENU_SLIT_OVERLAY_COORDS, _("Slit Position..."));
    view_menu->AppendSeparator();
    view_menu->Append(MENU_RESTORE_WINDOWS, _("Restore Window Positions"), _("Restore all windows to their default/docked positions"));

    darks_menu = new wxMenu();
    m_takeDarksMenuItem = darks_menu->Append(MENU_TAKEDARKS, _("Dark &Library..."), _("Build a dark library for this profile"));
    m_refineDefMapMenuItem = darks_menu->Append(MENU_REFINEDEFECTMAP, _("Bad-pixel &Map..."), _("Adjust parameters to create or modify the bad-pixel map"));
    m_importCamCalMenuItem = darks_menu->Append(MENU_IMPORTCAMCAL, _("Import From Profile..."), _("Import existing dark library/bad-pixel map from a different profile"));
    darks_menu->AppendSeparator();
    m_useDarksMenuItem =  darks_menu->AppendCheckItem(MENU_LOADDARK, _("Use &Dark Library"), _("Use the the dark library for this profile"));
    m_useDefectMapMenuItem = darks_menu->AppendCheckItem(MENU_LOADDEFECTMAP, _("Use &Bad-pixel Map"), _("Use the bad-pixel map for this profile"));

#if defined (V4L_CAMERA)
    wxMenu *v4l_menu = new wxMenu();

    v4l_menu->Append(MENU_V4LSAVESETTINGS, _("&Save settings"), _("Save current camera settings"));
    v4l_menu->Append(MENU_V4LRESTORESETTINGS, _("&Restore settings"), _("Restore camera settings"));
#endif

    bookmarks_menu = new wxMenu();
    m_showBookmarksMenuItem = bookmarks_menu->AppendCheckItem(MENU_BOOKMARKS_SHOW, _("Show &Bookmarks\tb"), _("Hide or show bookmarks"));
    m_showBookmarksAccel = m_showBookmarksMenuItem->GetAccel();
    bookmarks_menu->Check(MENU_BOOKMARKS_SHOW, true);
    m_bookmarkLockPosMenuItem = bookmarks_menu->Append(MENU_BOOKMARKS_SET_AT_LOCK, _("Bookmark &Lock Pos\tShift-B"), _("Set a bookmark at the current lock position"));
    m_bookmarkLockPosAccel = m_bookmarkLockPosMenuItem->GetAccel();
    bookmarks_menu->Append(MENU_BOOKMARKS_SET_AT_STAR, _("Bookmark &Star Pos"), _("Set a bookmark at the position of the currently selected star"));
    bookmarks_menu->Append(MENU_BOOKMARKS_CLEAR_ALL, _("&Delete all\tCtrl-B"), _("Remove all bookmarks"));

    wxMenu *help_menu = new wxMenu;
    help_menu->Append(wxID_ABOUT, _("&About..."), wxString::Format(_("About %s"), APPNAME));
    m_upgradeMenuItem = help_menu->Append(MENU_HELP_UPGRADE, _("&Check for updates"), _("Check for PHD2 software updates"));
    help_menu->Append(MENU_HELP_ONLINE,_("Online Support"),_("Ask for help in the PHD2 Forum"));
    help_menu->Append(MENU_HELP_LOG_FOLDER, _("Open Log Folder"), _("Open the log folder"));
    help_menu->Append(MENU_HELP_UPLOAD_LOGS, _("Upload Log Files..."), _("Upload log files for review"));
    help_menu->Append(wxID_HELP_CONTENTS,_("&Contents...\tF1"),_("Full help"));
    help_menu->Append(wxID_HELP_PROCEDURES,_("&Impatient Instructions"),_("Quick instructions for the impatient"));

    Menubar = new wxMenuBar();
    Menubar->Append(file_menu, _("&File"));
    Menubar->Append(guide_menu, _("&Guide"));

#if defined (V4L_CAMERA)
    Menubar->Append(v4l_menu, _T("&V4L"));

    Menubar->Enable(MENU_V4LSAVESETTINGS, false);
    Menubar->Enable(MENU_V4LRESTORESETTINGS, false);
#endif

    Menubar->Append(tools_menu, _("&Tools"));
    Menubar->Append(view_menu, _("&View"));
    Menubar->Append(darks_menu, _("&Darks"));
    Menubar->Append(bookmarks_menu, _("&Bookmarks"));
    Menubar->Append(help_menu, _("&Help"));
    SetMenuBar(Menubar);
}

int MyFrame::GetTextWidth(wxControl *pControl, const wxString& string)
{
    int width;

    pControl->GetTextExtent(string, &width, nullptr);

    return width;
}

// Get either timelapse value or state-dependent variable-exposure-delay
int MyFrame::GetExposureDelay()
{
    int rslt;

    if (!m_varDelayConfig.enabled)
        rslt = m_timeLapse;
    else if (pGuider->IsGuiding() && PhdController::IsIdle() && !pGuider->IsRecentering() &&
        pMount->GetGuidingEnabled())
    {
        rslt = m_varDelayConfig.longDelay;
    }
    else
        rslt = m_varDelayConfig.shortDelay;

    static int s_lastExposureDelay = -1;
    if (rslt != s_lastExposureDelay)
    {
        Debug.Write(wxString::Format("Exposure delay set to %d\n", rslt));
        s_lastExposureDelay = rslt;
    }

    return rslt;
}

void MyFrame::SetComboBoxWidth(wxComboBox *pComboBox, unsigned int extra)
{
    unsigned int i;
    int width = GetTextWidth(pComboBox, _("Custom: 0.999 s"));

    for (i = 0; i < pComboBox->GetCount(); i++)
    {
        int thisWidth = GetTextWidth(pComboBox, pComboBox->GetString(i));

        if (thisWidth > width)
        {
            width = thisWidth;
        }
    }

    pComboBox->SetMinSize(wxSize(width + extra, -1));
}

static std::vector<int> exposure_durations;

const std::vector<int>& MyFrame::GetExposureDurations() const
{
    return exposure_durations;
}

bool MyFrame::SetCustomExposureDuration(int ms)
{
    auto end = exposure_durations.end() - 1;
    for (auto it = exposure_durations.begin(); it != end; ++it)
        if (ms == *it)
            return true; // error, duplicate value
    if (m_exposureDuration == *end && *end != ms)
    {
        m_exposureDuration = ms;
        NotifyExposureChanged();
    }
    *end = ms;
    Dur_Choice->SetString(1 + exposure_durations.size() - 1, wxString::Format(_("Custom: %g s"), (double) ms / 1000.));
    pConfig->Profile.SetInt("/CustomExposureDuration", ms);
    return false;
}

bool MyFrame::GetExposureInfo(int *currExpMs, bool *autoExp) const
{
    bool bEerr = false;
    if (!pCamera || !pCamera->Connected)
    {
        *currExpMs = 0;
        *autoExp = false;
        bEerr = true;
    }
    else
    {
        *currExpMs = m_exposureDuration;
        *autoExp = m_autoExp.enabled;
    }
    return bEerr;
}

static int dur_index(int duration)
{
    for (auto it = exposure_durations.begin(); it != exposure_durations.end(); ++it)
        if (duration == *it)
            return it - exposure_durations.begin();
    return -1;
}

bool MyFrame::SetExposureDuration(int val, bool updateCustom)
{
    if (val < 0)
    {
        // Auto
        Dur_Choice->SetSelection(0);
    }
    else
    {
        int idx = dur_index(val);
        if (idx == -1)
        {
            if (updateCustom)
            {
                SetCustomExposureDuration(val);
                idx = dur_index(val);
            }
            if (idx == -1)
                return false;
        }
        Dur_Choice->SetSelection(idx + 1); // skip Auto
    }

    wxCommandEvent dummy;
    OnExposureDurationSelected(dummy);
    return true;
}

bool MyFrame::SetAutoExposureCfg(int minExp, int maxExp, double targetSNR)
{
    Debug.Write(wxString::Format("AutoExp: config min = %d max = %d snr = %.2f\n", minExp, maxExp, targetSNR));

    pConfig->Profile.SetInt("/auto_exp/exposure_min", minExp);
    pConfig->Profile.SetInt("/auto_exp/exposure_max", maxExp);
    pConfig->Profile.SetDouble("/auto_exp/target_snr", targetSNR);

    bool changed =
            m_autoExp.minExposure != minExp ||
            m_autoExp.maxExposure != maxExp ||
            m_autoExp.targetSNR != targetSNR;

    m_autoExp.minExposure = minExp;
    m_autoExp.maxExposure = maxExp;
    m_autoExp.targetSNR = targetSNR;

    return changed;
}

wxString MyFrame::ExposureDurationSummary() const
{
    wxString rslt;
    if (m_autoExp.enabled)
        rslt = wxString::Format("Auto (min = %d ms, max = %d ms, SNR = %.2f)", m_autoExp.minExposure, m_autoExp.maxExposure, m_autoExp.targetSNR);
    else
        rslt = wxString::Format("%d ms", m_exposureDuration);
    if (m_varDelayConfig.enabled)
        rslt += wxString::Format(", VarDelay (short = %d ms, long = %d ms)", m_varDelayConfig.shortDelay, m_varDelayConfig.longDelay);
    return rslt;
}

void MyFrame::ResetAutoExposure()
{
    if (m_autoExp.enabled)
    {
        Debug.Write(wxString::Format("AutoExp: reset exp to %d\n", m_autoExp.maxExposure));
        m_exposureDuration = m_autoExp.maxExposure;
    }
}

void MyFrame::AdjustAutoExposure(double curSNR)
{
    if (m_autoExp.enabled)
    {
        if (curSNR < 1.0)
        {
            Debug.Write(wxString::Format("AutoExp: low SNR (%.2f), reset exp to %d\n", curSNR, m_autoExp.maxExposure));
            m_exposureDuration = m_autoExp.maxExposure;
        }
        else
        {
            double r = m_autoExp.targetSNR / curSNR;
            double exp = (double) m_exposureDuration;
            // assume snr ~ sqrt(exposure)
            double newExp = exp * r * r;
            // use hysteresis to avoid overshooting
            // if our snr is below target, increase exposure rapidly (weak hysteresis, large alpha)
            // if our snr is above target, decrease exposure slowly (strong hysteresis, small alpha)
            static double const alpha_slow = .15; // low weighting for latest sample
            static double const alpha_fast = .20; // high weighting for latest sample
            double alpha = curSNR < m_autoExp.targetSNR ? alpha_fast : alpha_slow;
            exp += alpha * (newExp - exp);
            m_exposureDuration = (int) floor(exp + 0.5);
            if (m_exposureDuration < m_autoExp.minExposure)
                m_exposureDuration = m_autoExp.minExposure;
            else if (m_exposureDuration > m_autoExp.maxExposure)
                m_exposureDuration = m_autoExp.maxExposure;
            Debug.Write(wxString::Format("AutoExp: adjust SNR=%.2f new exposure %d\n", curSNR, m_exposureDuration));
        }
    }
}

wxString MyFrame::ExposureDurationLabel(int duration)
{
    if (duration >= 10000)
        return wxString::Format(_("%g s"), (double) duration / 1000.);

    int digits = duration < 100 ? 2 : 1;
    return wxString::Format(_("%.*f s"), digits, (double) duration / 1000.);
}

void MyFrame::SaveStarFindMode()
{
    m_StarFindMode_Saved = m_starFindMode;
}

void MyFrame::RestoreStarFindMode()
{
    m_starFindMode = m_StarFindMode_Saved;
}

Star::FindMode MyFrame::SetStarFindMode(Star::FindMode mode)
{
    Star::FindMode prev = m_starFindMode;
    Debug.Write(wxString::Format("Setting StarFindMode = %d\n", mode));
    m_starFindMode = mode;
    return prev;
}

bool MyFrame::SetRawImageMode(bool mode)
{
    bool prev = m_rawImageMode;
    Debug.Write(wxString::Format("Setting RawImageMode = %d\n", mode));
    m_rawImageMode = mode;
    if (mode)
        m_rawImageModeWarningDone = false;
    return prev;
}

static void LoadImageLoggerSettings()
{
    ImageLoggerSettings settings;

    settings.loggingEnabled = pConfig->Profile.GetBoolean("/ImageLogger/LoggingEnabled", false);
    settings.logFramesOverThreshRel = pConfig->Profile.GetBoolean("/ImageLogger/LogFramesOverThreshRel", false);
    settings.logFramesOverThreshPx = pConfig->Profile.GetBoolean("/ImageLogger/LogFramesOverThreshPx", false);
    settings.logFramesDropped = pConfig->Profile.GetBoolean("/ImageLogger/LogFramesDropped", false);
    settings.logAutoSelectFrames = pConfig->Profile.GetBoolean("/ImageLogger/LogAutoSelectFrames", false);
    settings.logNextNFrames = false;
    settings.logNextNFramesCount = 1;
    settings.guideErrorThreshRel = pConfig->Profile.GetDouble("/ImageLogger/ErrorThreshRel", 4.0);
    settings.guideErrorThreshPx = pConfig->Profile.GetDouble("/ImageLogger/ErrorThreshPx", 4.0);

    ImageLogger::ApplySettings(settings);
}

static void SaveImageLoggerSettings(const ImageLoggerSettings& settings)
{
    pConfig->Profile.SetBoolean("/ImageLogger/LoggingEnabled", settings.loggingEnabled);
    pConfig->Profile.SetBoolean("/ImageLogger/LogFramesOverThreshRel", settings.logFramesOverThreshRel);
    pConfig->Profile.SetBoolean("/ImageLogger/LogFramesOverThreshPx", settings.logFramesOverThreshPx);
    pConfig->Profile.SetBoolean("/ImageLogger/LogFramesDropped", settings.logFramesDropped);
    pConfig->Profile.SetBoolean("/ImageLogger/LogAutoSelectFrames", settings.logAutoSelectFrames);
    pConfig->Profile.SetDouble("/ImageLogger/ErrorThreshRel", settings.guideErrorThreshRel);
    pConfig->Profile.SetDouble("/ImageLogger/ErrorThreshPx", settings.guideErrorThreshPx);
}

enum {
    GAMMA_MIN = 10,
    GAMMA_MAX = 300,
    GAMMA_DEFAULT = 100,
};

void MyFrame::LoadProfileSettings()
{
    int noiseReductionMethod = pConfig->Profile.GetInt("/NoiseReductionMethod", DefaultNoiseReductionMethod);
    SetNoiseReductionMethod(noiseReductionMethod);

    double ditherScaleFactor = pConfig->Profile.GetDouble("/DitherScaleFactor", DefaultDitherScaleFactor);
    SetDitherScaleFactor(ditherScaleFactor);

    bool ditherRaOnly = pConfig->Profile.GetBoolean("/DitherRaOnly", DefaultDitherRaOnly);
    SetDitherRaOnly(ditherRaOnly);

    int ditherMode = pConfig->Profile.GetInt("/DitherMode", DefaultDitherMode);
    SetDitherMode(ditherMode == DITHER_RANDOM ? DITHER_RANDOM : DITHER_SPIRAL);

    int timeLapse = pConfig->Profile.GetInt("/frame/timeLapse", DefaultTimelapse);
    SetTimeLapse(timeLapse);

    SetVariableDelayConfig(pConfig->Profile.GetBoolean("/frame/var_delay/enabled", false),
        pConfig->Profile.GetInt("/frame/var_delay/short_delay", 1000),
        pConfig->Profile.GetInt("/frame/var_delay/long_delay", 10000));

    // Don't re-save the setting here with a call to SetAutoLoadCalibration().  An un-initialized registry key (-1) will
    // be populated after the 1st calibration
    int autoLoad = pConfig->Profile.GetInt("/AutoLoadCalibration", -1);
    m_autoLoadCalibration = (autoLoad == 1);        // new profile=> false

    int focalLength = pConfig->Profile.GetInt("/frame/focalLength", DefaultFocalLength);
    SetFocalLength(focalLength);

    int minExp = pConfig->Profile.GetInt("/auto_exp/exposure_min", DefaultAutoExpMin);
    int maxExp = pConfig->Profile.GetInt("/auto_exp/exposure_max", DefaultAutoExpMax);
    double targetSNR = pConfig->Profile.GetDouble("/auto_exp/target_snr", DefaultAutoExpSNR);
    SetAutoExposureCfg(minExp, maxExp, targetSNR);
    // force reset of auto-exposure state
    m_autoExp.enabled = true; // OnExposureDurationSelected below will set the actual value
    ResetAutoExposure();

    // set custom exposure duration vector value and drop-down list string from profile setting
    int customDuration = pConfig->Profile.GetInt("/CustomExposureDuration", 30000);
    *exposure_durations.rbegin() = customDuration;
    Dur_Choice->SetString(1 + exposure_durations.size() - 1, wxString::Format(_("Custom: %g s"), (double) customDuration / 1000.));

    // for backward compatibity:
    // exposure duration used to be stored as the formatted string value appearing in the drop-down list,
    // but this does not work if the locale is changed
    // TODO: remove this code after a few releases (code added 5/30/2017)
    // replace with:
    // int exposureDuration = pConfig->Profile.GetInt("/ExposureDurationMs", DefaultExposureDuration);
    int exposureDuration = DefaultExposureDuration;
    if (pConfig->Profile.HasEntry("/ExposureDurationMs"))
        exposureDuration = pConfig->Profile.GetInt("/ExposureDurationMs", DefaultExposureDuration);
    else if (pConfig->Profile.HasEntry("/ExposureDuration"))
    {
        wxString s = pConfig->Profile.GetString("/ExposureDuration", wxEmptyString);
        const wxStringCharType *start = s.wx_str();
        wxStringCharType *end;
        double d = wxStrtod(start, &end);
        if (end != start)
            exposureDuration = (int)(d * 1000.0);
        else if (s == _("Auto"))
            exposureDuration = -1;
        pConfig->Profile.DeleteEntry("/ExposureDuration");
    }
    SetExposureDuration(exposureDuration);
    m_beepForLostStar = pConfig->Profile.GetBoolean("/BeepForLostStar", true);

    int val = pConfig->Profile.GetInt("/Gamma", GAMMA_DEFAULT);
    if (val < GAMMA_MIN) val = GAMMA_MIN;
    if (val > GAMMA_MAX) val = GAMMA_MAX;
    Stretch_gamma = (double) val / 100.0;
    Gamma_Slider->SetValue(val);

    LoadImageLoggerSettings();
    INDIConfig::LoadProfileSettings();
}

void MyFrame::SetupToolBar()
{
    MainToolbar = new wxAuiToolBarExt(this, -1, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE);

#   include "icons/loop.png.h"
    wxBitmap loop_bmp(wxBITMAP_PNG_FROM_DATA(loop));

#   include "icons/loop_disabled.png.h"
    wxBitmap loop_bmp_disabled(wxBITMAP_PNG_FROM_DATA(loop_disabled));

#   include "icons/guide.png.h"
    wxBitmap guide_bmp(wxBITMAP_PNG_FROM_DATA(guide));

#   include "icons/guide_disabled.png.h"
    wxBitmap guide_bmp_disabled(wxBITMAP_PNG_FROM_DATA(guide_disabled));

#   include "icons/stop.png.h"
    wxBitmap stop_bmp(wxBITMAP_PNG_FROM_DATA(stop));

#   include "icons/stop_disabled.png.h"
    wxBitmap stop_bmp_disabled(wxBITMAP_PNG_FROM_DATA(stop_disabled));

#   include "icons/auto_select.png.h"
    wxBitmap auto_select_bmp(wxBITMAP_PNG_FROM_DATA(auto_select));

#   include "icons/auto_select_disabled.png.h"
    wxBitmap auto_select_disabled_bmp(wxBITMAP_PNG_FROM_DATA(auto_select_disabled));

#   include "icons/connect.png.h"
    wxBitmap connect_bmp(wxBITMAP_PNG_FROM_DATA(connect));

#   include "icons/connect_disabled.png.h"
    wxBitmap connect_bmp_disabled(wxBITMAP_PNG_FROM_DATA(connect_disabled));

#   include "icons/brain.png.h"
    wxBitmap brain_bmp(wxBITMAP_PNG_FROM_DATA(brain));

#   include "icons/cam_setup.png.h"
    wxBitmap cam_setup_bmp(wxBITMAP_PNG_FROM_DATA(cam_setup));

#   include "icons/cam_setup_disabled.png.h"
    wxBitmap cam_setup_bmp_disabled(wxBITMAP_PNG_FROM_DATA(cam_setup_disabled));

    // This mage sourced from https://www.pngegg.com/en/png-zffyt
    // The image is used under its "Non-commercial use, DMCA" terms.
#   include "icons/eclipse.png.h"
    wxBitmap eclipse_bmp(wxBITMAP_PNG_FROM_DATA(eclipse));

    int dur_values[] = {
        10, 20, 50,
        100, 200, 500, 1000, 1500,
        2000, 2500, 3000, 3500, 4000, 4500, 5000,
        6000, 7000, 8000, 9000, 10000, 15000,
        30000, // final entry is custom value
    };
    for (unsigned int i = 0; i < WXSIZEOF(dur_values); i++)
        exposure_durations.push_back(dur_values[i]);

    wxArrayString durs;
    durs.Add(_("Auto"));
    for (unsigned int i = 0; i < WXSIZEOF(dur_values) - 1; i++)
        durs.Add(ExposureDurationLabel(dur_values[i]));
    durs.Add(wxString::Format(_("Custom: %g s"), 9999.0));
    durs.Add(_("Edit Custom..."));

    Dur_Choice = new wxComboBox(MainToolbar, BUTTON_DURATION, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        durs, wxCB_READONLY);
    Dur_Choice->SetToolTip(_("Camera exposure duration"));
    SetComboBoxWidth(Dur_Choice, 35);

    Gamma_Slider = new wxSlider(MainToolbar, CTRL_GAMMA, GAMMA_DEFAULT, GAMMA_MIN, GAMMA_MAX, wxPoint(-1,-1), wxSize(160,-1));
    Gamma_Slider->SetBackgroundColour(wxColor(60, 60, 60));         // Slightly darker than toolbar background
    Gamma_Slider->SetToolTip(_("Screen gamma (brightness)"));

    MainToolbar->AddTool(BUTTON_GEAR, connect_bmp, connect_bmp_disabled, false, 0, _("Connect to equipment. Shift-click to reconnect the same equipment last connected."));
    MainToolbar->AddTool(BUTTON_LOOP, loop_bmp, loop_bmp_disabled, false, 0, _("Begin looping exposures for frame and focus"));
    MainToolbar->AddTool(BUTTON_AUTOSTAR, auto_select_bmp, auto_select_disabled_bmp, false, 0, _("Auto-select Star. Shift-click to de-select star."));
    MainToolbar->AddTool(BUTTON_GUIDE, guide_bmp, guide_bmp_disabled, false, 0, _("Begin guiding (PHD). Shift-click to force calibration."));
    MainToolbar->AddTool(BUTTON_STOP, stop_bmp, stop_bmp_disabled, false, 0, _("Stop looping and guiding"));
    MainToolbar->AddSeparator();
    MainToolbar->AddControl(Dur_Choice, _("Exposure duration"));
    MainToolbar->AddControl(Gamma_Slider, _("Gamma"));
    MainToolbar->AddSeparator();
    MainToolbar->AddTool(BUTTON_ADVANCED, _("Advanced Settings"), brain_bmp, _("Advanced Settings"));
    MainToolbar->AddTool(BUTTON_CAM_PROPERTIES, cam_setup_bmp, cam_setup_bmp_disabled, false, 0, _("Camera settings"));
    MainToolbar->AddTool(BUTTON_PLANETARY, _("Planetary Guiding"), eclipse_bmp, _("Planetary Guiding"));
    MainToolbar->EnableTool(BUTTON_CAM_PROPERTIES, false);
    MainToolbar->EnableTool(BUTTON_LOOP, false);
    MainToolbar->EnableTool(BUTTON_AUTOSTAR, false);
    MainToolbar->EnableTool(BUTTON_GUIDE, false);
    MainToolbar->EnableTool(BUTTON_STOP, false);
    MainToolbar->Realize();

    MainToolbar->SetArtProvider(new PHDToolBarArt);             // Get the custom background we want
}

void MyFrame::SetupStatusBar()
{
    m_statusbar = PHDStatusBar::CreateInstance(this, wxSTB_DEFAULT_STYLE);
    SetStatusBar(m_statusbar);
    PositionStatusBar();
    UpdateStatusBarCalibrationStatus();
}

void MyFrame::SetupKeyboardShortcuts()
{
    wxAcceleratorEntry entries[] = {
        wxAcceleratorEntry(wxACCEL_CTRL,  (int) '0', EEGG_CLEARCAL),
        wxAcceleratorEntry(wxACCEL_CTRL,  (int) 'A', MENU_BRAIN),
        wxAcceleratorEntry(wxACCEL_CTRL,  (int) 'C', MENU_CONNECT),
        wxAcceleratorEntry(wxACCEL_CTRL|wxACCEL_SHIFT,  (int) 'C', MENU_CONNECT),
        wxAcceleratorEntry(wxACCEL_CTRL,  (int) 'G', MENU_GUIDE),
        wxAcceleratorEntry(wxACCEL_CTRL,  (int) 'L', MENU_LOOP),
        wxAcceleratorEntry(wxACCEL_CTRL|wxACCEL_SHIFT,  (int) 'M', EEGG_MANUALCAL),
        wxAcceleratorEntry(wxACCEL_CTRL,  (int) 'S', MENU_STOP),
        wxAcceleratorEntry(wxACCEL_CTRL,  (int) 'D', BUTTON_ALERT_CLOSE),
    };
    wxAcceleratorTable accel(WXSIZEOF(entries), entries);
    SetAcceleratorTable(accel);
}

struct PHDHelpController : public wxHtmlHelpController
{
    PHDHelpController()
    {
        UseConfig(pConfig->Global.GetWxConfig(), "/help");
    }
};

void MyFrame::SetupHelpFile()
{
    wxFileSystem::AddHandler(new wxZipFSHandler);

    int langid = wxGetApp().GetLocale().GetLanguage();

    // first try to find locale-specific help file
    wxString filename = wxGetApp().GetLocalesDir() + wxFILE_SEP_PATH
        + wxLocale::GetLanguageCanonicalName(langid) + wxFILE_SEP_PATH
        + _T("PHD2GuideHelp.zip");

    Debug.Write(wxString::Format("SetupHelpFile: langid=%d, locale-specific help = %s\n", langid, filename));

    if (!wxFileExists(filename))
    {
        filename = wxGetApp().GetPHDResourcesDir() + wxFILE_SEP_PATH
            + _T("PHD2GuideHelp.zip");

        Debug.Write(wxString::Format("SetupHelpFile: using default help %s\n", filename));
    }

    help = new PHDHelpController();

    if (!help->AddBook(filename))
    {
        Alert(wxString::Format(_("Could not find help file %s"), filename));
    }
}

static bool cond_update_tool(wxAuiToolBar *tb, int toolId, wxMenuItem *mi, bool enable)
{
    bool ret = false;
    if (tb->GetToolEnabled(toolId) != enable)
    {
        tb->EnableTool(toolId, enable);
        mi->Enable(enable);
        ret = true;
    }
    return ret;
}

void MyFrame::UpdateCameraSettings()
{
    eventLock.Lock();
    if (pPlanetTool)
    {
        wxCommandEvent event(APPSTATE_NOTIFY_EVENT, GetId());
        event.SetEventObject(this);
        wxPostEvent(pPlanetTool, event);
    }
    eventLock.Unlock();
}

void MyFrame::UpdateButtonsStatus()
{
    assert(wxThread::IsMain());
    assert(pGuider);

    bool need_update = false;

    bool const loop_enabled =
        (!CaptureActive || pGuider->IsCalibratingOrGuiding()) &&
        pCamera && pCamera->Connected;

    if (cond_update_tool(MainToolbar, BUTTON_LOOP, m_loopMenuItem, loop_enabled))
        need_update = true;

    if (cond_update_tool(MainToolbar, BUTTON_GEAR, m_connectMenuItem, !CaptureActive))
        need_update = true;

    if (cond_update_tool(MainToolbar, BUTTON_STOP, m_stopMenuItem, CaptureActive))
        need_update = true;

    if (cond_update_tool(MainToolbar, BUTTON_AUTOSTAR, m_autoSelectStarMenuItem, CaptureActive))
        need_update = true;

    bool dark_enabled = loop_enabled && !CaptureActive;
    if (dark_enabled ^ m_takeDarksMenuItem->IsEnabled())
    {
        m_takeDarksMenuItem->Enable(dark_enabled);
        need_update = true;
    }

    bool guiding_active = pGuider->IsCalibratingOrGuiding();         // Not the same as 'bGuideable below

    if (!guiding_active ^ m_autoSelectStarMenuItem->IsEnabled())
    {
        m_autoSelectStarMenuItem->Enable(!guiding_active);
        cond_update_tool(MainToolbar, BUTTON_AUTOSTAR, m_autoSelectStarMenuItem, !guiding_active);
        need_update = true;
    }

    if (!guiding_active ^ m_refineDefMapMenuItem->IsEnabled())
    {
        m_refineDefMapMenuItem->Enable(!guiding_active);
        need_update = true;
    }

    bool mod_calibration_ok = !guiding_active && pMount && pMount->IsConnected();
    if (mod_calibration_ok ^ m_calibrationMenuItem->IsEnabled())
    {
        m_calibrationMenuItem->Enable(mod_calibration_ok);
        need_update = true;
    }

    bool bGuideable = pGuider->GetState() == STATE_SELECTED &&
        pMount && pMount->IsConnected();

    if (cond_update_tool(MainToolbar, BUTTON_GUIDE, m_guideMenuItem, bGuideable))
        need_update = true;

    bool cam_props = pCamera &&
        (pCamera->PropertyDialogType & PROPDLG_WHEN_CONNECTED) != 0 && pCamera->Connected;

    if (cond_update_tool(MainToolbar, BUTTON_CAM_PROPERTIES, m_cameraMenuItem, cam_props))
        need_update = true;

    if (pDriftTool)
    {
        // let the drift tool update its buttons too
        wxCommandEvent event(APPSTATE_NOTIFY_EVENT, GetId());
        event.SetEventObject(this);
        wxPostEvent(pDriftTool, event);
    }
    if (pPolarDriftTool)
    {
        // let the Polar drift tool update its buttons too
        wxCommandEvent event(APPSTATE_NOTIFY_EVENT, GetId());
        event.SetEventObject(this);
        wxPostEvent(pPolarDriftTool, event);
    }
    if (pStaticPaTool)
    {
        // let the static PA tool update its buttons too
        wxCommandEvent event(APPSTATE_NOTIFY_EVENT, GetId());
        event.SetEventObject(this);
        wxPostEvent(pStaticPaTool, event);
    }

    if (pCometTool)
        CometTool::UpdateCometToolControls(false);

    if (pGuidingAssistant)
        GuidingAssistant::UpdateUIControls();

    if (pierFlipToolWin)
        PierFlipTool::UpdateUIControls();

    if (pGuider->m_Planet.UpdateCaptureState(CaptureActive))
        need_update = true;

    if (need_update)
    {
        if (pGuider->GetState() < STATE_SELECTED)
            m_statusbar->ClearStarInfo();
        if (!guiding_active)
            m_statusbar->ClearGuiderInfo();
        Update();
        Refresh();
    }
}

static wxString WrapText(wxWindow *win, const wxString& text, int width)
{
    struct Wrapper : public wxTextWrapper
    {
        wxString m_str;
        Wrapper(wxWindow *win, const wxString& text, int width) {
            Wrap(win, text, width);
        }
        const wxString& Str() const { return m_str; }
        void OnOutputLine(const wxString& line) { m_str += line; }
        void OnNewLine() { m_str += '\n'; }
    };
    return Wrapper(win, text, width).Str();
}

struct alert_params
{
    wxString msg;
    wxString buttonLabel;
    int flags;
    alert_fn *fnDontShow;
    alert_fn *fnSpecial;
    long arg;
    bool showHelp;
};

void MyFrame::OnAlertButton(wxCommandEvent& evt)
{
    if (evt.GetId() == BUTTON_ALERT_ACTION && m_alertSpecialFn)
        (*m_alertSpecialFn)(m_alertFnArg);
    if (evt.GetId() == BUTTON_ALERT_DONTSHOW && m_alertDontShowFn)
    {
        (*m_alertDontShowFn)(m_alertFnArg);
        // Don't show should also mean close the window
        m_infoBar->Dismiss();
    }
    if (evt.GetId() == BUTTON_ALERT_CLOSE)
        m_infoBar->Dismiss();
}

void MyFrame::ClearAlert()
{
    m_infoBar->Dismiss();
}

void MyFrame::OnAlertHelp(wxCommandEvent& evt)
{
    // Any open help window will be re-directed
    help->Display(_("Trouble-shooting and Analysis"));
}

// Alerts may have a combination of 'Don't show', help, close, and 'Custom' buttons.  The 'close' button is added automatically if any of
// the other buttons are present.
void MyFrame::DoAlert(const alert_params& params)
{
    Debug.Write(wxString::Format("Alert: %s\n", params.msg));

    m_alertDontShowFn = params.fnDontShow;
    m_alertSpecialFn = params.fnSpecial;
    m_alertFnArg = params.arg;

    int buttonSpace = 80;
    m_infoBar->RemoveButton(BUTTON_ALERT_ACTION);
    m_infoBar->RemoveButton(BUTTON_ALERT_CLOSE);
    m_infoBar->RemoveButton(BUTTON_ALERT_HELP);
    m_infoBar->RemoveButton(BUTTON_ALERT_DONTSHOW);
    if (params.fnDontShow)
    {
        m_infoBar->AddButton(BUTTON_ALERT_DONTSHOW, _("Don't show\n again"));
        buttonSpace += 80;
    }
    if (params.fnSpecial)
    {
        m_infoBar->AddButton(BUTTON_ALERT_ACTION, params.buttonLabel);
        buttonSpace += 80;
    }
    if (params.fnSpecial || params.fnDontShow || params.showHelp)
    {
        m_infoBar->AddButton(BUTTON_ALERT_CLOSE, _("Close"));
        buttonSpace += 80;
    }
    if (params.showHelp)
    {
        m_infoBar->AddButton(BUTTON_ALERT_HELP, _("Help"));
        buttonSpace += 80;
    }
    wxString wrappedText;
    if (pFrame && pFrame->pGuider)
    {
        int textWidth = wxMax(pFrame->pGuider->GetSize().GetWidth() - buttonSpace, 100);
        wrappedText = WrapText(m_infoBar, params.msg, textWidth);
    }
    else
    {
        wrappedText = params.msg;
    }
    int showMessageFlags = params.flags;
#ifdef __APPLE__
    // starting with MacOS Sonoma 14.3 wxWidgets (3.1.7 and 3.2.4) crashes in
    // wxBitmapBundle::FromSVG when the wxInfoBar tries to display an icon. As a
    // workaround, do not display any icon on Mac.
    showMessageFlags = wxICON_NONE;
#endif
    m_infoBar->ShowMessage(wrappedText, showMessageFlags);
    m_statusbar->UpdateStates();        // might have disconnected a device
    EvtServer.NotifyAlert(params.msg, params.flags);
}

void MyFrame::Alert(const wxString& msg, alert_fn *DontShowFn, const wxString& buttonLabel,  alert_fn *SpecialFn, long arg, bool showHelpButton, int flags)
{
    if (wxThread::IsMain())
    {
        alert_params params;
        params.msg = msg;
        params.buttonLabel = buttonLabel;
        params.flags = flags;
        params.fnDontShow = DontShowFn;
        params.fnSpecial = SpecialFn;
        params.arg = arg;
        params.showHelp = showHelpButton;
        DoAlert(params);
    }
    else
    {
        alert_params *params = new alert_params;
        params->msg = msg;
        params->buttonLabel = buttonLabel;
        params->flags = flags;
        params->fnDontShow = DontShowFn;
        params->fnSpecial = SpecialFn;
        params->arg = arg;
        params->showHelp = showHelpButton;
        wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, ALERT_FROM_THREAD_EVENT);
        event->SetExtraLong((long) params);
        wxQueueEvent(this, event);
    }
}

// Standardized version for building an alert that has the "don't show again" option button.  Insures that debug log entry is made if
// the user has blocked the alert for this type of problem
void MyFrame::SuppressableAlert(const wxString& configPropKey, const wxString& msg, alert_fn *dontShowFn, long arg, bool showHelpButton, int flags)
{
    if (pConfig->Global.GetBoolean(configPropKey, true))
    {
        Alert(msg, dontShowFn, wxEmptyString,  0, arg, showHelpButton);
    }
    else
        Debug.Write(wxString::Format("Suppressed alert:  %s\n", msg));
}

void MyFrame::Alert(const wxString& msg, int flags)
{
    Alert(msg, 0, wxEmptyString, 0, 0, false, flags);
}

void MyFrame::OnAlertFromThread(wxThreadEvent& event)
{
    alert_params *params = (alert_params *) event.GetExtraLong();
    DoAlert(*params);
    delete params;
}

void MyFrame::OnReconnectCameraFromThread(wxThreadEvent& event)
{
    DoTryReconnect();
}

void MyFrame::TryReconnect()
{
    if (wxThread::IsMain())
        DoTryReconnect();
    else
    {
        Debug.Write("worker thread queueing reconnect event to GUI thread\n");
        wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, RECONNECT_CAMERA_EVENT);
        wxQueueEvent(this, event);
    }
}

void MyFrame::DoTryReconnect()
{
    // do not reconnect more than 3 times in 1 minute
    enum { TIME_WINDOW = 60, MAX_ATTEMPTS = 3 };
    time_t now = wxDateTime::GetTimeNow();
    Debug.Write(wxString::Format("Try camera reconnect, now = %lu\n", (unsigned long) now));
    while (m_cameraReconnectAttempts.size() > 0 && now - m_cameraReconnectAttempts[0] > TIME_WINDOW)
        m_cameraReconnectAttempts.erase(m_cameraReconnectAttempts.begin());
    if (m_cameraReconnectAttempts.size() + 1 > MAX_ATTEMPTS)
    {
        Debug.Write(wxString::Format("More than %d camera reconnect attempts in less than %d seconds, "
            "return without reconnect.\n", MAX_ATTEMPTS, TIME_WINDOW));
        OnExposeComplete(0, true);
        return;
    }
    m_cameraReconnectAttempts.push_back(now);

    bool err = pGearDialog->ReconnectCamera();
    if (err)
    {
        Debug.Write("Camera Re-connect failed\n");
        // complete the pending exposure notification
        OnExposeComplete(0, true);
    }
    else
    {
        Debug.Write("Camera Re-connect succeeded, resume exposures\n");
        UpdateStatusBarStateLabels();
        m_exposurePending = false; // exposure no longer pending
        ScheduleExposure();
    }
}

/*
 * The base class wxFrame::StatusMsg() is not
 * safe to call from worker threads.
 *
 * For non-main threads this routine queues the request
 * to the frames event queue, and it gets displayed by the main
 * thread as part of event processing.
 *
 */

// Use a timer to show a status message for 10 seconds, then revert back to basic state info
static void StartStatusBarTimer(wxTimer& timer)
{
    const int DISPLAY_MS = 10000;
    timer.Start(DISPLAY_MS, wxTIMER_ONE_SHOT);
}

static void SetStatusMsg(PHDStatusBar *statusbar, const wxString& text)
{
    Debug.Write(wxString::Format("Status Line: %s\n", text));
    statusbar->StatusMsg(text);
}

enum StatusBarThreadMsgType
{
    THR_SB_MSG_TEXT,
    THR_SB_STATE_LABELS,
    THR_SB_CALIBRATION,
    THR_SB_BUTTON_STATE,
};

static void QueueStatusBarTextMsg(wxEvtHandler *frame, const wxString& text, bool withTimeout)
{
    wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, SET_STATUS_TEXT_EVENT);
    event->SetExtraLong(THR_SB_MSG_TEXT);
    event->SetString(text);
    event->SetInt(withTimeout);
    wxQueueEvent(frame, event);
}

static void QueueStatusBarUpdateMsg(wxEvtHandler *frame, StatusBarThreadMsgType type)
{
    wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, SET_STATUS_TEXT_EVENT);
    event->SetExtraLong(type);
    wxQueueEvent(frame, event);
}

void MyFrame::StatusMsg(const wxString& text)
{
    if (wxThread::IsMain())
    {
        SetStatusMsg(m_statusbar, text);
        StartStatusBarTimer(m_statusbarTimer);
    }
    else
        QueueStatusBarTextMsg(this, text, true);
}

void MyFrame::StatusMsgNoTimeout(const wxString& text)
{
    if (wxThread::IsMain())
        SetStatusMsg(m_statusbar, text);
    else
        QueueStatusBarTextMsg(this, text, false);
}

void MyFrame::OnStatusMsg(wxThreadEvent& event)
{
    switch (event.GetExtraLong()) {
    case THR_SB_MSG_TEXT: {
        wxString msg(event.GetString());
        bool withTimeout = event.GetInt() ? true : false;

        SetStatusMsg(m_statusbar, msg);

        if (withTimeout)
            StartStatusBarTimer(m_statusbarTimer);
        break;
    }
    case THR_SB_STATE_LABELS:
        m_statusbar->UpdateStates();
        break;
    case THR_SB_CALIBRATION:
        UpdateStatusBarCalibrationStatus();
        break;
    case THR_SB_BUTTON_STATE:
        UpdateButtonsStatus();
        break;
    }
}

void MyFrame::UpdateStatusBarStarInfo(double SNR, bool Saturated)
{
    assert(wxThread::IsMain());
    m_statusbar->UpdateStarInfo(SNR, Saturated);
}

void MyFrame::UpdateStatusBarStateLabels()
{
    if (wxThread::IsMain())
        m_statusbar->UpdateStates();
    else
        QueueStatusBarUpdateMsg(this, THR_SB_STATE_LABELS);
}

void MyFrame::UpdateStatusBarCalibrationStatus()
{
    if (wxThread::IsMain())
    {
        m_statusbar->UpdateStates();
        UpdateStatsWindowScopePointing();
    }
    else
    {
        QueueStatusBarUpdateMsg(this, THR_SB_CALIBRATION);
    }
}

void MyFrame::UpdateStatsWindowScopePointing()
{
    if(pStatsWin)
    {
        pStatsWin->UpdateScopePointing();
    }
}

void MyFrame::NotifyUpdateButtonsStatus()
{
    if (wxThread::IsMain())
    {
        UpdateButtonsStatus();
    }
    else
    {
        QueueStatusBarUpdateMsg(this, THR_SB_BUTTON_STATE);
    }
}

void MyFrame::UpdateStatusBarGuiderInfo(const GuideStepInfo& info)
{
    Debug.Write(wxString::Format("GuideStep: %.1f px %d ms %s, %.1f px %d ms %s\n",
        info.mountOffset.X, info.durationRA, info.directionRA == EAST ? "EAST" : "WEST",
        info.mountOffset.Y, info.durationDec, info.directionDec == NORTH ? "NORTH" : "SOUTH"));

    assert(wxThread::IsMain());
    m_statusbar->UpdateGuiderInfo(info);
}

void MyFrame::ClearStatusBarGuiderInfo()
{
    assert(wxThread::IsMain());
    m_statusbar->ClearGuiderInfo();
}

void MyFrame::OnUpgrade(wxCommandEvent& evt)
{
    PHD2Updater::CheckNow();
}

void MyFrame::NotifyUpdaterStateChanged()
{
    wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, UPDATER_EVENT);
    wxQueueEvent(this, event);
}

void MyFrame::OnUpdaterStateChanged(wxThreadEvent& event)
{
    PHD2Updater::OnUpdaterStateChanged();
}

bool MyFrame::StartWorkerThread(WorkerThread*& pWorkerThread)
{
    bool bError = false;
    wxCriticalSectionLocker lock(m_CSpWorkerThread);

    try
    {
        Debug.Write(wxString::Format("StartWorkerThread(%p) begins\n", pWorkerThread));

        if (!pWorkerThread || !pWorkerThread->IsRunning())
        {
            delete pWorkerThread;
            pWorkerThread = new WorkerThread(this);

            if (pWorkerThread->Create() != wxTHREAD_NO_ERROR)
            {
                throw ERROR_INFO("Could not Create() the worker thread!");
            }

            if (pWorkerThread->Run() != wxTHREAD_NO_ERROR)
            {
                throw ERROR_INFO("Could not Run() the worker thread!");
            }
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        delete pWorkerThread;
        pWorkerThread = nullptr;
        bError = true;
    }

    Debug.Write(wxString::Format("StartWorkerThread(%p) ends\n", pWorkerThread));

    return bError;
}

bool MyFrame::StopWorkerThread(WorkerThread*& pWorkerThread)
{
    bool killed = false;

    wxCriticalSectionLocker lock(m_CSpWorkerThread);

    Debug.Write(wxString::Format("StopWorkerThread(0x%p) begins\n", pWorkerThread));

    if (pWorkerThread && pWorkerThread->IsRunning())
    {
        pWorkerThread->EnqueueWorkerThreadTerminateRequest();

        enum { TIMEOUT_MS = 1000 };
        wxStopWatch swatch;
        while (pWorkerThread->IsAlive() && swatch.Time() < TIMEOUT_MS)
            wxGetApp().Yield();

        if (pWorkerThread->IsAlive())
        {
            while (pWorkerThread->IsAlive() && !pWorkerThread->IsKillable())
            {
                Debug.Write(wxString::Format("Worker thread 0x%p is not killable, waiting...\n", pWorkerThread));
                wxStopWatch swatch2;
                while (pWorkerThread->IsAlive() && !pWorkerThread->IsKillable() && swatch2.Time() < TIMEOUT_MS)
                    wxGetApp().Yield();
            }
            if (pWorkerThread->IsAlive())
            {
                Debug.Write(wxString::Format("StopWorkerThread(0x%p) thread did not terminate, force kill\n", pWorkerThread));
                pWorkerThread->Kill();
                killed = true;
            }
        }
        else
        {
            wxThread::ExitCode threadExitCode = pWorkerThread->Wait();
            Debug.Write(wxString::Format("StopWorkerThread() threadExitCode=%d\n", threadExitCode));
        }
    }

    Debug.Write(wxString::Format("StopWorkerThread(0x%p) ends\n", pWorkerThread));

    delete pWorkerThread;
    pWorkerThread = nullptr;

    return killed;
}

void MyFrame::OnRequestExposure(wxCommandEvent& evt)
{
    EXPOSE_REQUEST *req = (EXPOSE_REQUEST *) evt.GetClientData();
    bool error = GuideCamera::Capture(pCamera, req->exposureDuration, *req->pImage, req->options, req->subframe);
    req->error = error;
    req->pSemaphore->Post();
}

void MyFrame::OnRequestMountMove(wxCommandEvent& evt)
{
    MOVE_REQUEST *request = (MOVE_REQUEST *) evt.GetClientData();

    Debug.Write("OnRequestMountMove() begins\n");

    if (request->axisMove)
    {
        request->moveResult = request->mount->MoveAxis(request->direction, request->duration, request->moveOptions);
    }
    else
    {
        request->moveResult = request->mount->MoveOffset(&request->ofs, request->moveOptions);
    }

    request->semaphore->Post();
    Debug.Write("OnRequestMountMove() ends\n");
}

void MyFrame::OnStatusBarTimerEvent(wxTimerEvent& evt)
{
    if (pGuider->IsGuiding())
        m_statusbar->StatusMsg(_("Guiding"));
    else if (CaptureActive)
        m_statusbar->StatusMsg(_("Looping"));
    else
        m_statusbar->StatusMsg(wxEmptyString);
}

void MyFrame::ScheduleExposure()
{
    int exposureDuration = RequestedExposureDuration();
    int exposureOptions = GetRawImageMode() ? CAPTURE_BPM_REVIEW : CAPTURE_LIGHT;
    const wxRect& subframe =
        m_singleExposure.enabled ? m_singleExposure.subframe : pGuider->GetBoundingBox();

    Debug.Write(wxString::Format("ScheduleExposure(%d,%x,%d) exposurePending=%d\n",
        exposureDuration, exposureOptions, !subframe.IsEmpty(), m_exposurePending));

    assert(wxThread::IsMain()); // m_exposurePending only updated in main thread
    assert(!m_exposurePending);

    m_exposurePending = true;

    usImage *img = new usImage();

    wxCriticalSectionLocker lock(m_CSpWorkerThread);

    if (m_pPrimaryWorkerThread) // can be null when app is shutting down (unlikely but possible)
        m_pPrimaryWorkerThread->EnqueueWorkerThreadExposeRequest(img, exposureDuration, exposureOptions, subframe);
}

void MyFrame::SchedulePrimaryMove(Mount *mount, const GuiderOffset& ofs, unsigned int moveOptions)
{
    Debug.Write(wxString::Format("SchedulePrimaryMove(%p, x=%.2f, y=%.2f, opts=%u)\n", mount, ofs.cameraOfs.X, ofs.cameraOfs.Y, moveOptions));

    wxCriticalSectionLocker lock(m_CSpWorkerThread);

    assert(mount);

    // Manual moves do not affect the request count for IsBusy()
    if ((moveOptions & MOVEOPT_MANUAL) == 0)
        mount->IncrementRequestCount();

    assert(m_pPrimaryWorkerThread);
    m_pPrimaryWorkerThread->EnqueueWorkerThreadMoveRequest(mount, ofs, moveOptions);
}

void MyFrame::ScheduleSecondaryMove(Mount *mount, const GuiderOffset& ofs, unsigned int moveOptions)
{
    Debug.Write(wxString::Format("ScheduleSecondaryMove(%p, x=%.2f, y=%.2f, opts=%u)\n", mount, ofs.cameraOfs.X, ofs.cameraOfs.Y, moveOptions));

    wxCriticalSectionLocker lock(m_CSpWorkerThread);

    assert(mount);

    if (mount->SynchronousOnly())
    {
        // some mounts must run on the Primary thread even if the secondary is requested
        // to ensure synchronous ST4 guide / camera exposure
        SchedulePrimaryMove(mount, ofs, moveOptions);
    }
    else
    {
        if ((moveOptions & MOVEOPT_MANUAL) == 0)
            mount->IncrementRequestCount();

        assert(m_pSecondaryWorkerThread);
        m_pSecondaryWorkerThread->EnqueueWorkerThreadMoveRequest(mount, ofs, moveOptions);
    }
}

void MyFrame::ScheduleAxisMove(Mount *mount, const GUIDE_DIRECTION direction, int duration, unsigned int moveOptions)
{
    wxCriticalSectionLocker lock(m_CSpWorkerThread);

    assert(mount);

    if ((moveOptions & MOVEOPT_MANUAL) == 0)
        mount->IncrementRequestCount();

    assert(m_pPrimaryWorkerThread);
    m_pPrimaryWorkerThread->EnqueueWorkerThreadAxisMove(mount, direction, duration, moveOptions);
}

void MyFrame::ScheduleManualMove(Mount *mount, const GUIDE_DIRECTION direction, int duration)
{
    GuideLog.NotifyManualGuide(mount, direction, duration);

    GuideStepInfo step = { 0 };
    step.mount = mount;
    step.moveOptions = MOVEOPT_MANUAL;
    step.mountOffset.SetXY(0., 0.);
    switch (direction)
    {
    case GUIDE_DIRECTION::NORTH:
    case GUIDE_DIRECTION::SOUTH:
        step.durationDec = duration;
        step.directionDec = direction;
        break;
    case GUIDE_DIRECTION::EAST:
    case GUIDE_DIRECTION::WEST:
        step.durationRA = duration;
        step.directionRA = direction;
        break;
    default:
        break;
    }
    UpdateStatusBarGuiderInfo(step);

    ScheduleAxisMove(mount, direction, duration, MOVEOPT_MANUAL);
}

bool MyFrame::StartSingleExposure(int duration, const wxRect& subframe)
{
    Debug.Write(wxString::Format("StartSingleExposure duration=%d\n", duration));

    if (!pCamera || !pCamera->Connected)
    {
        Debug.Write("StartSingleExposure: camera not connected\n");
        return true;
    }

    StatusMsgNoTimeout(_("Capturing single exposure"));

    m_singleExposure.enabled = true;
    m_singleExposure.duration = duration;
    m_singleExposure.subframe = subframe;
    if (!m_singleExposure.subframe.IsEmpty())
        m_singleExposure.subframe.Intersect(wxRect(pCamera->FullSize));

    StartCapturing();

    return false;
}

bool MyFrame::AutoSelectStar(const wxRect& roi)
{
    if (pGuider->IsCalibratingOrGuiding())
    {
        Debug.Write("cannot auto-select star while calibrating or guiding\n");
        return true; // error
    }

    return pGuider->AutoSelect(roi);
}

void MyFrame::StartCapturing()
{
    Debug.Write(wxString::Format("StartCapturing CaptureActive=%d continueCapturing=%d exposurePending=%d\n", CaptureActive, m_continueCapturing, m_exposurePending));

    if (!CaptureActive)
    {
        if (!m_singleExposure.enabled)
            m_continueCapturing = true;

        CaptureActive = true;
        m_frameCounter = 0;

        CheckDarkFrameGeometry();
        UpdateButtonsStatus();

        // m_exposurePending should always be false here since CaptureActive is cleared on exposure
        // completion, but be paranoid and check it anyway

        if (!m_exposurePending)
        {
            pCamera->InitCapture();
            ScheduleExposure();
        }
    }
}

bool MyFrame::StopCapturing()
{
    Debug.Write(wxString::Format("StopCapturing CaptureActive=%d continueCapturing=%d exposurePending=%d\n", CaptureActive, m_continueCapturing, m_exposurePending));

    bool finished = true;
    bool continueCapturing = m_continueCapturing;

    if (pGuider->IsPaused())
    {
        // setting m_continueCapturing to false before calling
        // SetPaused(PAUSE_NONE) ensures that SetPaused(PAUSE_NONE)
        // does not schedule another exposure
        m_continueCapturing = false;
        SetPaused(PAUSE_NONE);
    }

    if (continueCapturing || m_exposurePending)
    {
        StatusMsgNoTimeout(_("Waiting for devices..."));
        m_continueCapturing = false;

        if (m_exposurePending)
        {
            m_pPrimaryWorkerThread->RequestStop();
            finished = false;
        }
        else
        {
            CaptureActive = false;
            if (pGuider->IsCalibratingOrGuiding())
            {
                pGuider->StopGuiding();
                pGuider->UpdateImageDisplay();
            }
            FinishStop();
        }
    }

    return finished;
}

void MyFrame::SetPaused(PauseType pause)
{
    bool const isPaused = pGuider->IsPaused();

    Debug.Write(wxString::Format("SetPaused type=%d isPaused=%d exposurePending=%d\n", pause, isPaused, m_exposurePending));

    if (pause != PAUSE_NONE && !isPaused)
    {
        pGuider->SetPaused(pause);
        StatusMsgNoTimeout(_("Paused") + (pause == PAUSE_FULL ? _("/full") : _("/looping")));
        GuideLog.ServerCommand(pGuider, "PAUSE");
        EvtServer.NotifyPaused();
    }
    else if (pause == PAUSE_NONE && isPaused)
    {
        pGuider->SetPaused(PAUSE_NONE);
        if (pMount)
        {
            Debug.Write("un-pause: clearing mount guide algorithm history\n");
            pMount->NotifyGuidingResumed();
        }
        if (m_continueCapturing && !m_exposurePending)
            ScheduleExposure();
        StatusMsg(_("Resumed"));
        GuideLog.ServerCommand(pGuider, "RESUME");
        EvtServer.NotifyResumed();
    }
}

bool MyFrame::StartLooping()
{
    bool error = false;

    try
    {
        if (!pCamera || !pCamera->Connected)
        {
            throw ERROR_INFO("Camera not connected");
        }

        if (CaptureActive)
        {
            // if we are guiding, stop guiding and go back to looping
            if (pGuider->IsCalibratingOrGuiding())
            {
                pGuider->StopGuiding();
            }
            else
            {
                // already looping, nothing to do
                return false;
            }
        }

        StatusMsg(_("Looping"));
        StartCapturing();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
    }

    return error;
}

bool MyFrame::StartGuiding()
{
    bool error = true;

    if (pRefineDefMap && pRefineDefMap->IsShown())
    {
        Alert(_("Cannot guide while refining a Bad-pixel Map. Please close the Refine Bad-pixel Map window."));
        return error;
    }

    wxGetApp().CheckLogRollover();

    if (pMount && pMount->IsConnected() &&
        pCamera && pCamera->Connected &&
        pGuider->GetState() >= STATE_SELECTED)
    {
        pGuider->StartGuiding();
        StartCapturing();
        UpdateButtonsStatus();
        // reset dither state when guiding starts
        m_ditherSpiral.Reset();
        error = false;
    }

    return error;
}

void DitherSpiral::Reset()
{
    Debug.Write("reset dither spiral\n");
    x = y = 0;
    dx = -1;
    dy = 0;
    prevRaOnly = false;
}

inline static void ROT(int& dx, int& dy)
{
    int t = -dx;
    dx = dy;
    dy = t;
}

void DitherSpiral::GetDither(double amount, bool raOnly, double *dRa, double *dDec)
{
    // reset state when switching between ra only and ra/dec
    if (raOnly != prevRaOnly)
    {
        Reset();
        prevRaOnly = raOnly;
    }

    if (raOnly)
    {
        // x = 0,1,-1,-2,2,3,-3,-4,4,5,...
        ROT(dx, dy);
        int x0 = x;
        if (dy == 0)
            x = -x;
        else
            x += dy;

        *dRa = (double)(x - x0) * amount;
        *dDec = 0.0;
    }
    else
    {
        if (x == y || (x > 0 && x == -y) || (x <= 0 && y == 1 - x))
            ROT(dx, dy);

        x += dx;
        y += dy;

        *dRa  = (double) dx * amount;
        *dDec = (double) dy * amount;
    }
}

void MyFrame::SetDitherMode(DitherMode mode)
{
    Debug.Write(wxString::Format("set dither mode %d\n", mode));
    m_ditherMode = mode;
    pConfig->Profile.SetInt("/DitherMode", mode);
}

bool MyFrame::Dither(double amount, bool raOnly)
{
    bool error = false;

    try
    {
        if (!pGuider->IsGuiding())
        {
            throw ERROR_INFO("cannot dither if not guiding");
        }

        amount *= m_ditherScaleFactor;

        double dRa = 0.0;
        double dDec = 0.0;

        if (m_ditherMode == DITHER_SPIRAL)
        {
            m_ditherSpiral.GetDither(amount, raOnly, &dRa, &dDec);
        }
        else
        {
            // DITHER_RANDOM
            dRa  =  amount * ((rand() / (double)RAND_MAX) * 2.0 - 1.0);
            dDec =  raOnly ? 0.0 : amount * ((rand() / (double)RAND_MAX) * 2.0 - 1.0);
        }

        Debug.Write(wxString::Format("dither: size=%.2f, dRA=%.2f dDec=%.2f\n", amount, dRa, dDec));

        bool err = pGuider->MoveLockPosition(PHD_Point(dRa, dDec));
        if (err)
        {
            throw ERROR_INFO("move lock failed");
        }

        StatusMsg(wxString::Format(_("Dither by %.2f,%.2f"), dRa, dDec));
        GuideLog.NotifyGuidingDithered(pGuider, dRa, dDec);
        EvtServer.NotifyGuidingDithered(dRa, dDec);
        DitherInfo info;
        info.timestamp = ::wxGetUTCTimeMillis().GetValue();
        info.dRa = dRa;
        info.dDec = dDec;
        pGraphLog->AppendData(info);

        if (pMount->IsStepGuider())
        {
            StepGuider *ao = static_cast<StepGuider *>(pMount);
            if (ao->GetBumpOnDither())
            {
                Debug.Write("Dither: starting AO bump\n");
                ao->ForceStartBump();
            }
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
    }

    return error;
}

bool MyFrame::GuidingRAOnly()
{
    const Scope *const scope = TheScope();
    return scope && scope->GetDecGuideMode() == DEC_NONE;
}

double MyFrame::CurrentGuideError() const
{
    return pGuider->CurrentError(GuidingRAOnly());
}

double MyFrame::CurrentGuideErrorSmoothed() const
{
    return pGuider->CurrentErrorSmoothed(GuidingRAOnly());
}

void MyFrame::OnClose(wxCloseEvent& event)
{
    if (CaptureActive && event.CanVeto())
    {
        bool confirmed = ConfirmDialog::Confirm(_("Are you sure you want to exit while capturing is active?"),
            "/quit_when_looping_ok", _("Confirm Exit"));
        if (!confirmed)
        {
            event.Veto();
            return;
        }
    }

    Debug.Write("MyFrame::OnClose proceeding\n");

    StopCapturing();

    bool killed = StopWorkerThread(m_pPrimaryWorkerThread);
    if (StopWorkerThread(m_pSecondaryWorkerThread))
        killed = true;

    // disconnect all gear
    pGearDialog->Shutdown(killed);

    PHD2Updater::StopUpdater();

    // stop the socket server and event server
    StartServer(false);

    GuideLog.CloseGuideLog();

    pConfig->Global.SetString("/perspective", m_mgr.SavePerspective());
    wxString geometry = wxString::Format("%c;%d;%d;%d;%d",
        this->IsMaximized() ? '1' : '0',
        this->GetSize().x, this->GetSize().y,
        this->GetScreenPosition().x, this->GetScreenPosition().y);
    pConfig->Global.SetString("/geometry", geometry);

    if (help->GetFrame())
        help->GetFrame()->Close();
    delete help;
    help = 0;

    Destroy();
}

bool MyFrame::SetNoiseReductionMethod(int noiseReductionMethod)
{
    bool bError = false;

    try
    {
        switch (noiseReductionMethod)
        {
            case NR_NONE:
            case NR_2x2MEAN:
            case NR_3x3MEDIAN:
                break;
            default:
                throw ERROR_INFO("invalid noiseReductionMethod");
        }
        m_noiseReductionMethod = (NOISE_REDUCTION_METHOD)noiseReductionMethod;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);

        bError = true;
        m_noiseReductionMethod = (NOISE_REDUCTION_METHOD)DefaultNoiseReductionMethod;
    }

    pConfig->Profile.SetInt("/NoiseReductionMethod", m_noiseReductionMethod);

    return bError;
}

bool MyFrame::SetDitherScaleFactor(double ditherScaleFactor)
{
    bool bError = false;

    try
    {
        if (ditherScaleFactor <= 0)
        {
            throw ERROR_INFO("ditherScaleFactor <= 0");
        }
        m_ditherScaleFactor = ditherScaleFactor;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_ditherScaleFactor = DefaultDitherScaleFactor;
    }

    pConfig->Profile.SetDouble("/DitherScaleFactor", m_ditherScaleFactor);

    return bError;
}

bool MyFrame::SetDitherRaOnly(bool ditherRaOnly)
{
    bool bError = false;

    m_ditherRaOnly = ditherRaOnly;

    pConfig->Profile.SetBoolean("/DitherRaOnly", m_ditherRaOnly);

    return bError;
}

void MyFrame::NotifyGuidingStarted()
{
    StatusMsg(_("Guiding"));

    m_guidingStarted = wxDateTime::UNow();
    m_guidingElapsed.Start();
    m_frameCounter = 0;

    if (pMount)
        pMount->NotifyGuidingStarted();
    if (pSecondaryMount)
        pSecondaryMount->NotifyGuidingStarted();

    GuideLog.GuidingStarted();
    EvtServer.NotifyGuidingStarted();
}

void MyFrame::NotifyGuidingStopped()
{
    assert(!pMount || !pMount->IsBusy());
    assert(!pSecondaryMount || !pSecondaryMount->IsBusy());

    if (pMount)
        pMount->NotifyGuidingStopped();
    if (pSecondaryMount)
        pSecondaryMount->NotifyGuidingStopped();

    EvtServer.NotifyGuidingStopped();
    GuideLog.GuidingStopped();
    PhdController::AbortController("Guiding stopped");
}

void MyFrame::SetAutoLoadCalibration(bool val)
{
    if (m_autoLoadCalibration != val)
    {
        m_autoLoadCalibration = val;
        pConfig->Profile.SetBoolean("/AutoLoadCalibration", m_autoLoadCalibration);
    }
}

inline static GuideParity guide_parity(int p)
{
    switch (p) {
    case GUIDE_PARITY_EVEN: return GUIDE_PARITY_EVEN;
    case GUIDE_PARITY_ODD: return GUIDE_PARITY_ODD;
    default: return GUIDE_PARITY_UNKNOWN;
    }
}

static void load_calibration(Mount *mnt)
{
    wxString prefix = "/" + mnt->GetMountClassName() + "/calibration/";
    if (!pConfig->Profile.HasEntry(prefix + "timestamp"))
        return;

    Calibration cal;
    cal.xRate = pConfig->Profile.GetDouble(prefix + "xRate", 1.0);
    cal.yRate = pConfig->Profile.GetDouble(prefix + "yRate", 1.0);
    cal.binning = (unsigned short) pConfig->Profile.GetInt(prefix + "binning", 1);
    cal.xAngle = pConfig->Profile.GetDouble(prefix + "xAngle", 0.0);
    cal.yAngle = pConfig->Profile.GetDouble(prefix + "yAngle", M_PI / 2.0);
    cal.declination = pConfig->Profile.GetDouble(prefix + "declination", 0.0);
    int t = pConfig->Profile.GetInt(prefix + "pierSide", PIER_SIDE_UNKNOWN);
    cal.pierSide = t == PIER_SIDE_EAST ? PIER_SIDE_EAST :
        t == PIER_SIDE_WEST ? PIER_SIDE_WEST : PIER_SIDE_UNKNOWN;
    cal.raGuideParity = guide_parity(pConfig->Profile.GetInt(prefix + "raGuideParity", GUIDE_PARITY_UNKNOWN));
    cal.decGuideParity = guide_parity(pConfig->Profile.GetInt(prefix + "decGuideParity", GUIDE_PARITY_UNKNOWN));
    cal.rotatorAngle = pConfig->Profile.GetDouble(prefix + "rotatorAngle", Rotator::POSITION_UNKNOWN);
    cal.isValid = true;

    mnt->SetCalibration(cal);
}

void MyFrame::LoadCalibration()
{
    if (pMount)
    {
        load_calibration(pMount);
    }
    if (pSecondaryMount)
    {
        load_calibration(pSecondaryMount);
    }
}

static bool save_multi_darks(const ExposureImgMap& darks, const wxString& fname, const wxString& note)
{
    bool bError = false;

    try
    {
        fitsfile *fptr;  // FITS file pointer
        int status = 0;  // CFITSIO status value MUST be initialized to zero!

        PHD_fits_create_file(&fptr, fname, true, &status);
        if (status)
            throw ERROR_INFO("fits_create_file failed");

        for (ExposureImgMap::const_iterator it = darks.begin(); it != darks.end(); ++it)
        {
            const usImage *const img = it->second;
            long fsize[] = {
                (long)img->Size.GetWidth(),
                (long)img->Size.GetHeight(),
            };
            if (!status)
                fits_create_img(fptr, USHORT_IMG, 2, fsize, &status);

            float exposure = (float) img->ImgExpDur / 1000.0f;
            char *keyname = const_cast<char *>("EXPOSURE");
            char *comment = const_cast<char *>("Exposure time in seconds");
            if (!status) fits_write_key(fptr, TFLOAT, keyname, &exposure, comment, &status);

            if (!note.IsEmpty())
            {
                char *USERNOTE = const_cast<char *>("USERNOTE");
                if (!status) fits_write_key(fptr, TSTRING, USERNOTE, note.char_str(), nullptr, &status);
            }

            if (!status)
            {
                long fpixel[3] = { 1, 1, 1 };
                fits_write_pix(fptr, TUSHORT, fpixel, img->NPixels, img->ImageData, &status);
            }

            Debug.Write(wxString::Format("saving dark frame exposure = %d\n", img->ImgExpDur));
        }

        PHD_fits_close_file(fptr);
        bError = status ? true : false;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

static bool load_multi_darks(GuideCamera *camera, const wxString& fname)
{
    bool bError = false;
    fitsfile *fptr = 0;
    int status = 0;  // CFITSIO status value MUST be initialized to zero!
    long last_frame_size [] = { -1L, -1L };

    try
    {
        if (!wxFileExists(fname))
        {
            throw ERROR_INFO("File does not exist");
        }

        if (PHD_fits_open_diskfile(&fptr, fname, READONLY, &status) == 0)
        {
            int nhdus = 0;
            fits_get_num_hdus(fptr, &nhdus, &status);

            while (true)
            {
                int hdutype;
                fits_get_hdu_type(fptr, &hdutype, &status);
                if (hdutype != IMAGE_HDU)
                {
                    pFrame->Alert(wxString::Format(_("FITS file is not of an image: %s"), fname));
                    throw ERROR_INFO("FITS file is not an image");
                }

                int naxis;
                fits_get_img_dim(fptr, &naxis, &status);
                if (naxis != 2)
                {
                    pFrame->Alert(wxString::Format(_("Unsupported type or read error loading FITS file %s"), fname));
                    throw ERROR_INFO("unsupported type");
                }

                long fsize[2];
                fits_get_img_size(fptr, 2, fsize, &status);
                if (last_frame_size[0] != -1L)
                {
                    if (last_frame_size[0] != fsize[0] || last_frame_size[1] != fsize[1])
                    {
                        pFrame->Alert(_("Existing dark library has frames with incompatible formats - please rebuild the dark library from scratch."));
                        throw ERROR_INFO("Incompatible frame sizes in dark library");
                    }
                }
                last_frame_size[0] = fsize[0];
                last_frame_size[1] = fsize[1];

                std::unique_ptr<usImage> img(new usImage());

                if (img->Init((int)fsize[0], (int)fsize[1]))
                {
                    pFrame->Alert(wxString::Format(_("Memory allocation error reading FITS file %s"), fname));
                    throw ERROR_INFO("Memory Allocation failure");
                }

                long fpixel[] = { 1, 1, 1 };
                if (fits_read_pix(fptr, TUSHORT, fpixel, fsize[0] * fsize[1], nullptr, img->ImageData, nullptr, &status))
                {
                    pFrame->Alert(wxString::Format(_("Error reading data from %s"), fname));
                    throw ERROR_INFO("Error reading");
                }

                char keyname[] = "EXPOSURE";
                float exposure;
                if (fits_read_key(fptr, TFLOAT, keyname, &exposure, nullptr, &status))
                {
                    exposure = (float)pFrame->RequestedExposureDuration() / 1000.0;
                    Debug.Write(wxString::Format("missing EXPOSURE value, assume %.3f\n", exposure));
                    status = 0;
                }
                img->ImgExpDur = ROUNDF(exposure * 1000.0);

                img->CalcStats();

                Debug.Write(wxString::Format("loaded dark frame exposure = %d, med = %u\n",
                                             img->ImgExpDur, img->MedianADU));

                camera->AddDark(img.release());

                // if this is the last hdu, we are done
                int hdunr = 0;
                fits_get_hdu_num(fptr, &hdunr);
                if (status || hdunr >= nhdus)
                    break;

                // move to the next hdu
                fits_movrel_hdu(fptr, +1, nullptr, &status);
            }
        }
        else
        {
            pFrame->Alert(wxString::Format(_("Error opening FITS file %s"), fname));
            throw ERROR_INFO("error opening file");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    if (fptr)
    {
        PHD_fits_close_file(fptr);
    }

    return bError;
}

wxString MyFrame::GetDarksDir()
{
    wxString dirpath = GetDefaultFileDir() + PATHSEPSTR + "darks_defects";
    if (!wxDirExists(dirpath))
        if (!wxFileName::Mkdir(dirpath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
            dirpath = GetDefaultFileDir();             // should never happen
    return dirpath;
}

wxString MyFrame::DarkLibFileName(int profileId)
{
    int inst = wxGetApp().GetInstanceNumber();
    return MyFrame::GetDarksDir() + PATHSEPSTR +
        wxString::Format("PHD2_dark_lib%s_%d.fit", inst > 1 ? wxString::Format("_%d", inst) : "", profileId);
}

bool MyFrame::DarkLibExists(int profileId, bool showAlert)
{
    bool bOk = false;
    wxString fileName = MyFrame::DarkLibFileName(profileId);

    if (wxFileExists(fileName))
    {
        const wxSize& sensorSize = pCamera->DarkFrameSize();
        if (sensorSize == UNDEFINED_FRAME_SIZE)
        {
            bOk = true;
            Debug.Write("DarkLib check: undefined frame size for current camera\n");
        }
        else
        {
            fitsfile *fptr;
            int status = 0;  // CFITSIO status value MUST be initialized to zero!

            if (PHD_fits_open_diskfile(&fptr, fileName, READONLY, &status) == 0)
            {
                long fsize[2];
                fits_get_img_size(fptr, 2, fsize, &status);
                if (status == 0 && fsize[0] == sensorSize.x && fsize[1] == sensorSize.y)
                    bOk = true;
                else
                {
                    Debug.Write(wxString::Format("DarkLib check: failed geometry check - fits status = %d, cam dimensions = {%d,%d}, "
                        " dark dimensions = {%d,%d}\n", status, sensorSize.x, sensorSize.y, fsize[0], fsize[1]));

                    if (showAlert)
                        Alert(_("Dark library does not match the camera in this profile. Check that you are "
                        "connected to the camera you want to use for guiding."));
                }

                PHD_fits_close_file(fptr);
            }
            else
                Debug.Write(wxString::Format("DarkLib check: fitsio error on open_diskfile = %d\n", status));
        }
    }

    return bOk;
}

// Confirm that in-use darks or bpms have the same sensor size as the current camera.  Added to protect against
// surprise changes in binning
void MyFrame::CheckDarkFrameGeometry()
{
    bool haveDefectMap = DefectMap::DefectMapExists(pConfig->GetCurrentProfileId(), m_useDefectMapMenuItem->IsEnabled());
    bool haveDarkLib = DarkLibExists(pConfig->GetCurrentProfileId(), m_useDarksMenuItem->IsEnabled());
    bool defectMapOk = true;

    if (m_useDefectMapMenuItem->IsEnabled())
    {
        if (!haveDefectMap)
        {
            if (m_useDefectMapMenuItem->IsChecked())
                LoadDefectMapHandler(false);
            m_useDefectMapMenuItem->Enable(false);
            Debug.Write("CheckDarkFrameGeometry: BPM incompatibility found\n");
            defectMapOk = false;
        }
    }
    else if (haveDefectMap)
    {
        m_useDefectMapMenuItem->Enable(true);
    }

    if (m_useDarksMenuItem->IsEnabled())
    {
        if (!haveDarkLib)
        {
            if (m_useDarksMenuItem->IsChecked())
                LoadDarkHandler(false);
            m_useDarksMenuItem->Enable(false);
            Debug.Write("CheckDarkFrameGeometry: Dark lib incompatibility found\n");
            if (!defectMapOk)
                pFrame->Alert(_("Dark library and bad-pixel maps don't match the current camera. "
                "Check that you are connected to the camera you want to use for guiding."));
        }
    }
    else if (haveDarkLib)
    {
        m_useDarksMenuItem->Enable(true);
    }

    m_prevDarkFrameSize = pCamera->DarkFrameSize();
    m_statusbar->UpdateStates();
}

void MyFrame::SetDarkMenuState()
{
    bool haveDarkLib = DarkLibExists(pConfig->GetCurrentProfileId(), true);
    m_useDarksMenuItem->Enable(haveDarkLib);
    if (!haveDarkLib)
        m_useDarksMenuItem->Check(false);
    bool haveDefectMap = DefectMap::DefectMapExists(pConfig->GetCurrentProfileId(), true);
    m_useDefectMapMenuItem->Enable(haveDefectMap);
    if (!haveDefectMap)
        m_useDefectMapMenuItem->Check(false);
    m_statusbar->UpdateStates();
}

bool MyFrame::LoadDarkLibrary()
{
    wxString filename = MyFrame::DarkLibFileName(pConfig->GetCurrentProfileId());

    if (!pCamera || !pCamera->Connected)
    {
        Alert(_("You must connect a camera before loading dark frames"));
        return false;
    }

    if (load_multi_darks(pCamera, filename))
    {
        Debug.Write(wxString::Format("failed to load dark frames from %s\n", filename));
        StatusMsg(_("Darks not loaded"));
        return false;
    }
    else
    {
        Debug.Write(wxString::Format("loaded dark library from %s\n", filename));
        pCamera->SelectDark(m_exposureDuration);
        StatusMsg(_("Darks loaded"));
        return true;
    }
}

void MyFrame::SaveDarkLibrary(const wxString& note)
{
    wxString filename = MyFrame::DarkLibFileName(pConfig->GetCurrentProfileId());

    Debug.Write("saving dark library\n");

    if (save_multi_darks(pCamera->Darks, filename, note))
    {
        Alert(wxString::Format(_("Error saving darks FITS file %s"), filename));
    }
}

// Delete both the dark library file and any defect map file for this profile
void MyFrame::DeleteDarkLibraryFiles(int profileId)
{
    wxString filename = MyFrame::DarkLibFileName(profileId);

    if (wxFileExists(filename))
    {
        Debug.Write(wxString::Format("Removing dark library file: %s\n", filename));
        wxRemoveFile(filename);
    }

    DefectMap::DeleteDefectMap(profileId);
}

bool MyFrame::SetServerMode(bool serverMode)
{
    bool bError = false;

    m_serverMode = serverMode;

    pConfig->Global.SetBoolean("/ServerMode", m_serverMode);

    return bError;
}

bool MyFrame::SetTimeLapse(int timeLapse)
{
    bool bError = false;

    try
    {
        if (timeLapse < 0)
        {
            throw ERROR_INFO("timeLapse < 0");
        }

        m_timeLapse = timeLapse;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_timeLapse = DefaultTimelapse;
    }

    pConfig->Profile.SetInt("/frame/timeLapse", m_timeLapse);

    UpdateCameraSettings();

    return bError;
}

bool MyFrame::SetFocalLength(int focalLength)
{
    bool bError = false;

    try
    {
        if (focalLength < 0)
        {
            throw ERROR_INFO("focal length < 0");
        }

        m_focalLength = focalLength;
        if (pStatsWin)
            pStatsWin->ResetImageSize();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_focalLength = DefaultFocalLength;
    }

    pConfig->Profile.SetInt("/frame/focalLength", m_focalLength);

    return bError;
}

void MyFrame::SetVariableDelayConfig(bool varDelayEnabled, int ShortDelayMS, int LongDelayMS)
{
    Debug.Write(wxString::Format("Variable delay: %s, Short = %d ms, Long = %d ms\n", (varDelayEnabled ? "Enabled" : "Disabled"), ShortDelayMS, LongDelayMS));

    m_varDelayConfig.enabled = varDelayEnabled;
    m_varDelayConfig.shortDelay = ShortDelayMS;
    m_varDelayConfig.longDelay = wxMax(LongDelayMS, ShortDelayMS);

    pConfig->Profile.SetInt("/frame/var_delay/short_delay", ShortDelayMS);
    pConfig->Profile.SetInt("/frame/var_delay/long_delay", LongDelayMS);
    pConfig->Profile.SetBoolean("/frame/var_delay/enabled", varDelayEnabled);
}

wxString MyFrame::GetDefaultFileDir()
{
    wxStandardPathsBase& stdpath = wxStandardPaths::Get();
    wxString rslt = stdpath.GetUserLocalDataDir();          // Automatically includes app name

    if (!wxDirExists(rslt))
        if (!wxFileName::Mkdir(rslt, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
            rslt = stdpath.GetUserLocalDataDir();             // should never happen

    return rslt;
}

double MyFrame::GetCameraPixelScale() const
{
    if (!pCamera || pCamera->GetCameraPixelSize() == 0.0 || m_focalLength == 0)
        return 1.0;

    return GetPixelScale(pCamera->GetCameraPixelSize(), m_focalLength, pCamera->Binning);
}

wxString MyFrame::PixelScaleSummary() const
{
    double pixelScale = GetCameraPixelScale();
    wxString scaleStr;
    if (pixelScale == 1.0)
        scaleStr = "unspecified";
    else
        scaleStr = wxString::Format("%.2f arc-sec/px", pixelScale);

    wxString focalLengthStr;
    if (m_focalLength == 0)
        focalLengthStr = "unspecified";
    else
        focalLengthStr = wxString::Format("%d mm", m_focalLength);

    return wxString::Format("Pixel scale = %s, Binning = %hu, Focal length = %s",
        scaleStr, pCamera->Binning, focalLengthStr);
}

bool MyFrame::GetBeepForLostStar()
{
    return m_beepForLostStar;
}

void MyFrame::SetBeepForLostStar(bool beep)
{
    m_beepForLostStar = beep;
    pConfig->Profile.SetBoolean("/BeepForLostStar", beep);
    Debug.Write(wxString::Format("Beep for lost star set to %s\n", beep ? "true" : "false"));
}

wxString MyFrame::GetSettingsSummary() const
{
    // return a loggable summary of current global configs managed by MyFrame
    return wxString::Format("Dither = %s, Dither scale = %.3f, Image noise reduction = %s, Guide-frame time lapse = %d, Server %s\n"
        "%s\n",
        m_ditherRaOnly ? "RA only" : "both axes",
        m_ditherScaleFactor,
        m_noiseReductionMethod == NR_NONE ? "none" : m_noiseReductionMethod == NR_2x2MEAN ? "2x2 mean" : "3x3 median",
        m_timeLapse,
        m_serverMode ? "enabled" : "disabled",
        PixelScaleSummary()
    );
}

void MyFrame::RegisterTextCtrl(wxTextCtrl *ctrl)
{
    // Text controls gaining focus need to disable the Bookmarks Menu accelerators
    ctrl->Bind(wxEVT_SET_FOCUS, &MyFrame::OnTextControlSetFocus, this);
    ctrl->Bind(wxEVT_KILL_FOCUS, &MyFrame::OnTextControlKillFocus, this);
}

// Reset the guiding parameters and the various graphical displays when image scale is changed outside the AD UI.  Goal is to restore basic guiding behavior
// until a fresh calibration is done.  Ratio of image scales is used because 1) info like mount guide speed may not be available and 2) the user may have already
// adjusted the calibration step-size in the profile to get the results he wants.
void MyFrame::HandleImageScaleChange(double NewToOldRatio)
{
    // Adjust the calibration step-size to facilitate a reasonable calibration of the real mount
    Scope *scope = TheScope();
    if (scope)
    {
        int stepSize = scope->GetCalibrationDuration();
        stepSize *= NewToOldRatio;              // Larger scale => larger step-size because calibration is targeted on fixed-size total pixel displacements
        scope->SetCalibrationDuration(stepSize);

    }

    // Leave the algo choices in place but force a reversion to default guiding params for in-use algos
    pAdvancedDialog->ResetGuidingParams();

    wxCommandEvent dummyEvt;
    if (pGraphLog)
    {
        pGraphLog->OnButtonClear(dummyEvt);
        pGraphLog->UpdateControls();
    }

    // Give the image-scale dependent windows a chance to reset
    if (pStepGuiderGraph)
        pStepGuiderGraph->OnButtonClear(dummyEvt);

    if (pTarget)
    {
        pTarget->OnButtonClear(dummyEvt);
        pTarget->UpdateControls();
    }

    Alert(_("Guiding parameters have been reset because the binning or pixel size changed unexpectedly. "
	    "You should use separate profiles for different image scales."));
}

MyFrameConfigDialogPane *MyFrame::GetConfigDialogPane(wxWindow *pParent)
{
    return new MyFrameConfigDialogPane(pParent, this);
}

MyFrameConfigDialogPane::MyFrameConfigDialogPane(wxWindow *pParent, MyFrame *pFrame)
    : ConfigDialogPane(_("Global Settings"), pParent)
{
}

void MyFrameConfigDialogPane::LayoutControls(BrainCtrlIdMap& CtrlMap)
{
    wxSizerFlags sizer_flags = wxSizerFlags(0).Border(wxALL, 5).Expand();
    wxSizerFlags grid_flags = wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL);
    wxFlexGridSizer *pTopGrid = new wxFlexGridSizer(2, 2, 15, 15);

    pTopGrid->Add(GetSizerCtrl(CtrlMap, AD_szLanguage), grid_flags);
    pTopGrid->Add(GetSingleCtrl(CtrlMap, AD_cbResetConfig), grid_flags);
    pTopGrid->Add(GetSingleCtrl(CtrlMap, AD_cbDontAsk), grid_flags);
    this->Add(pTopGrid, sizer_flags);
    this->Add(GetSizerCtrl(CtrlMap, AD_szSoftwareUpdate), sizer_flags);
    this->Add(GetSizerCtrl(CtrlMap, AD_szLogFileInfo), sizer_flags);
    this->Add(GetSingleCtrl(CtrlMap, AD_cbEnableImageLogging), sizer_flags);
    this->Add(GetSizerCtrl(CtrlMap, AD_szImageLoggingOptions), sizer_flags);
    this->Add(GetSizerCtrl(CtrlMap, AD_szDither), sizer_flags);
    Layout();
}

MyFrameConfigDialogCtrlSet *MyFrame::GetConfigDlgCtrlSet(MyFrame *pFrame, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
{
    return new MyFrameConfigDialogCtrlSet(pFrame, pAdvancedDialog, CtrlMap);
}

struct FocalLengthValidator : public wxIntegerValidator<int>
{
    typedef wxIntegerValidator<int> Super;
    AdvancedDialog *m_dlg;
    FocalLengthValidator(AdvancedDialog *dlg) : Super(nullptr, 0), m_dlg(dlg) { }
    wxObject *Clone() const override {
        return new FocalLengthValidator(*this);
    }
    bool Validate(wxWindow *parent) override {
        bool ok = false;
        if (Super::Validate(parent))
        {
            long val;
            wxTextCtrl *ctrl = static_cast<wxTextCtrl *>(GetWindow());
            if (ctrl->GetValue().ToLong(&val))
                ok = val <= AdvancedDialog::MAX_FOCAL_LENGTH && (val == 0 || val >= AdvancedDialog::MIN_FOCAL_LENGTH);
        }
        if (!ok)
        {
            m_dlg->ShowInvalid(GetWindow(),
                wxString::Format(_("Enter a focal length in millimeters, between %.f and %.f,\n"
                "or enter 0 if the focal length is not known"),
                AdvancedDialog::MIN_FOCAL_LENGTH, AdvancedDialog::MAX_FOCAL_LENGTH));
        }
        return ok;
    }
};

#if defined(__LINUX__) || defined(__FreeBSD__)
// ugly workaround for Issue 83 - link error on Linux
//  undefined reference to wxPluralFormsCalculatorPtr::~wxPluralFormsCalculatorPtr
wxPluralFormsCalculatorPtr::~wxPluralFormsCalculatorPtr() { }
#endif

// slow and iniefficient translation of a string to a given language
static wxString TranslateStrToLang(const wxString& s, int langid)
{
    if (langid == wxLANGUAGE_DEFAULT)
        langid = wxLocale::GetSystemLanguage();
    if (langid == wxLANGUAGE_ENGLISH_US)
        return s;
    for (const auto& tr : wxTranslations::Get()->GetAvailableTranslations(PHD_MESSAGES_CATALOG))
    {
        const wxLanguageInfo *info = wxLocale::FindLanguageInfo(tr);
        if (info && info->Language == langid)
        {
            std::unique_ptr<wxFileTranslationsLoader> loader(new wxFileTranslationsLoader());
            std::unique_ptr<wxMsgCatalog> msgcat(loader->LoadCatalog("messages", info->CanonicalName));
            if (msgcat)
            {
                const wxString *p = msgcat->GetString(s);
                if (p)
                    return *p;
            }
            break;
        }
    }
    return s;
}

class AvailableLanguages
{
    wxArrayString m_names;
    wxArrayInt m_ids;

    void Init()
    {
        wxArrayString availableTranslations =
                wxTranslations::Get()->GetAvailableTranslations(PHD_MESSAGES_CATALOG);

        availableTranslations.Sort();

        m_names.Add(_("System default"));
        m_names.Add("English");
        m_ids.Add(wxLANGUAGE_DEFAULT);
        m_ids.Add(wxLANGUAGE_ENGLISH_US);
        for (const auto& s : availableTranslations)
        {
            const wxLanguageInfo *info = wxLocale::FindLanguageInfo(s);
            const wxString *langDesc = &info->Description;
            std::unique_ptr<wxFileTranslationsLoader> loader(new wxFileTranslationsLoader());
            std::unique_ptr<wxMsgCatalog> msgcat(loader->LoadCatalog("messages", info->CanonicalName));
            if (msgcat)
            {
                const wxString *p = msgcat->GetString(wxTRANSLATE("Language-Name"));
                if (p)
                    langDesc = p;
            }
            m_names.Add(*langDesc);
            m_ids.Add(info->Language);
        }
    }

public:

    const wxArrayString& Names() {
        if (m_names.empty())
            Init();
        return m_names;
    }

    int Index(int langid) {
        if (m_names.empty())
            Init();
        return m_ids.Index(langid);
    }

    int LangId(int index) {
        if (m_names.empty())
            Init();
        return index >= 0 && index < m_names.size() ?
            m_ids[index] : wxLANGUAGE_DEFAULT;
    }
};
static AvailableLanguages PhdLanguages;

MyFrameConfigDialogCtrlSet::MyFrameConfigDialogCtrlSet(MyFrame *pFrame, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
    : ConfigDialogCtrlSet(pFrame, pAdvancedDialog, CtrlMap)
{
    int width;
    wxWindow *parent;

    m_pFrame = pFrame;
    m_pResetConfiguration = new wxCheckBox(GetParentWindow(AD_cbResetConfig), wxID_ANY, _("Reset Configuration"));
    AddCtrl(CtrlMap, AD_cbResetConfig, m_pResetConfiguration, _("Reset all configuration and program settings to fresh install status. This will require restarting PHD2"));
    m_pResetDontAskAgain = new wxCheckBox(GetParentWindow(AD_cbDontAsk), wxID_ANY, _("Reset \"Don't Show Again\" messages"));
    AddCtrl(CtrlMap, AD_cbDontAsk, m_pResetDontAskAgain, _("Restore any messages that were hidden when you checked \"Don't show this again\"."));

    wxString nralgo_choices[] =
    {
        _("None"), _("2x2 mean"), _("3x3 median")
    };

    width = StringArrayWidth(nralgo_choices, WXSIZEOF(nralgo_choices));
    parent = GetParentWindow(AD_szNoiseReduction);
    m_pNoiseReduction = new wxChoice(parent, wxID_ANY, wxPoint(-1, -1),
        wxSize(width + 35, -1), WXSIZEOF(nralgo_choices), nralgo_choices);
    AddLabeledCtrl(CtrlMap, AD_szNoiseReduction, _("Noise Reduction"), m_pNoiseReduction,
        _("Technique to reduce noise in images"));

    width = StringWidth(_T("00000"));
    parent = GetParentWindow(AD_szTimeLapse);
    m_pTimeLapse = pFrame->MakeSpinCtrl(parent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0, 10000, 0, _T("TimeLapse"));
    AddLabeledCtrl(CtrlMap, AD_szTimeLapse, _("Time Lapse (ms)"), m_pTimeLapse,
        _("How long should PHD wait between guide frames? Default = 0ms, useful when using very short exposures (e.g., using a video camera) but wanting to send guide commands less frequently"));

    parent = GetParentWindow(AD_szFocalLength);
    // Put a validator on this field to be sure that only digits are entered - avoids problem where
    // user face-plant on keyboard results in a focal length of zero
    m_pFocalLength = new wxTextCtrl(parent, wxID_ANY, _T(" "), wxDefaultPosition, wxSize(width + 30, -1), 0,
        FocalLengthValidator(pAdvancedDialog));
    AddLabeledCtrl(CtrlMap, AD_szFocalLength, _("Focal length (mm)"), m_pFocalLength,
        _("Guider telescope focal length, used with the camera pixel size to display guiding error in arc-sec."));

    width = StringWidth(_("System default"));
    parent = GetParentWindow(AD_szLanguage);
    m_pLanguage = new wxChoice(parent, wxID_ANY, wxPoint(-1, -1),
        wxSize(width + 35, -1), PhdLanguages.Names());
    AddLabeledCtrl(CtrlMap, AD_szLanguage, _("Language"), m_pLanguage,
        wxString::Format(_("%s Language. You'll have to restart PHD to take effect."), APPNAME));

    // Software Update
    {
        parent = GetParentWindow(AD_szSoftwareUpdate);
        wxStaticBoxSizer *sz = new wxStaticBoxSizer(wxHORIZONTAL, parent, _("Software Update"));
        m_updateEnabled = new wxCheckBox(parent, wxID_ANY, _("Automatically check for updates"));
        m_updateEnabled->SetToolTip(_("Check for software updates when PHD2 starts (recommended)"));
        sz->Add(m_updateEnabled, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxALL, 8));
        m_updateMajorOnly = new wxCheckBox(parent, wxID_ANY, _("Only check for major releases"));
        m_updateMajorOnly->SetToolTip(_("Ignore minor (development) releases when checking for updates"));
        sz->Add(m_updateMajorOnly, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxALL, 8));
        AddGroup(CtrlMap, AD_szSoftwareUpdate, sz);
    }

    // Log directory location - use a group box with a wide text edit control and a 'browse' button at the far right
    parent = GetParentWindow(AD_szLogFileInfo);
    wxStaticBoxSizer *pInputGroupBox = new wxStaticBoxSizer(wxHORIZONTAL, parent, _("Log File Location"));
    wxBoxSizer *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    m_pLogDir = new wxTextCtrl(parent, wxID_ANY, _T(""), wxDefaultPosition, wxSize(450, -1));
    m_pLogDir->SetToolTip(_("Folder for guide and debug logs; empty string to restore the default location"));
    m_pSelectDir = new wxButton(parent, wxID_OK, _("Browse..."));
    pButtonSizer->Add(m_pSelectDir, wxSizerFlags(0).Align(wxRIGHT));
    m_pSelectDir->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &MyFrameConfigDialogCtrlSet::OnDirSelect, this);

    pInputGroupBox->Add(m_pLogDir, wxSizerFlags(0).Expand());
    pInputGroupBox->Add(pButtonSizer, wxSizerFlags(0).Align(wxRIGHT).Border(wxTop, 20));
    AddGroup(CtrlMap, AD_szLogFileInfo, pInputGroupBox);

    const int PAD = 6;

    // Image logging controls
    width = StringWidth(_T("00.0"));
    parent = GetParentWindow(AD_cbEnableImageLogging);
    m_EnableImageLogging = new wxCheckBox(parent, wxID_ANY, _("Enable diagnostic image logging"));
    AddCtrl(CtrlMap, AD_cbEnableImageLogging, m_EnableImageLogging, _("Save guider images based on options below"));
    parent = GetParentWindow(AD_szImageLoggingOptions);
    m_EnableImageLogging->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MyFrameConfigDialogCtrlSet::OnImageLogEnableChecked, this);
    m_LoggingOptions = new wxStaticBoxSizer(wxVERTICAL, parent, _("Save Guider Images"));
    wxFlexGridSizer *pOptionsGrid = new wxFlexGridSizer(3, 2, 0, PAD);

    m_LogDroppedFrames = new wxCheckBox(parent, wxID_ANY, _("For all lost-star frames"));
    m_LogDroppedFrames->SetToolTip(_("Save guider image whenever a lost-star event occurs"));

    m_LogAutoSelectFrames = new wxCheckBox(parent, wxID_ANY, _("For all Auto-select Star frames"));
    m_LogAutoSelectFrames->SetToolTip(_("Save guider image when a star auto-selection is made. Note: the image is always saved when star auto-selection fails, regardless of this setting."));

    wxBoxSizer *pHzRel = new wxBoxSizer(wxHORIZONTAL);
    m_LogRelErrors = new wxCheckBox(parent, wxID_ANY, _("When relative error exceeds"));
    m_LogRelErrors->SetToolTip(_("Save guider images when the error for the current frame exceeds the average error by this factor. "
        "For example, if the average (RMS) error is 0.5 pixels, and the current frame's error is 1.5 pixels, the relative error is 3"));
    m_LogRelErrorThresh = pFrame->MakeSpinCtrlDouble(parent, wxID_ANY, _(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 1.0, 10.0, 4.0, 1.0);
    m_LogRelErrorThresh->SetToolTip(_("Relative error threshold. Relative error is the ratio between the current frame's error and the average error"));
    pHzRel->Add(m_LogRelErrors, wxSizerFlags().Border(wxALL, PAD).Align(wxALIGN_CENTER_VERTICAL));
    pHzRel->Add(m_LogRelErrorThresh, wxSizerFlags().Border(wxALL, PAD).Align(wxALIGN_CENTER_VERTICAL));

    wxBoxSizer *pHzAbs = new wxBoxSizer(wxHORIZONTAL);
    m_LogAbsErrors = new wxCheckBox(parent, wxID_ANY, _("When absolute error exceeds (pixels)"));
    m_LogAbsErrors->SetToolTip(_("Save guider images when the distance between the guide star and the lock position exceeds this many pixels"));
    width = StringWidth(_T("00.0"));
    m_LogAbsErrorThresh = pFrame->MakeSpinCtrlDouble(parent, wxID_ANY, _(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 1.0, 10.0, 4.0, 1.0);
    m_LogAbsErrorThresh->SetToolTip(_("Absolute error threshold in pixels"));
    pHzAbs->Add(m_LogAbsErrors, wxSizerFlags().Border(wxALL, PAD).Align(wxALIGN_CENTER_VERTICAL));
    pHzAbs->Add(m_LogAbsErrorThresh, wxSizerFlags().Border(wxALL, PAD).Align(wxALIGN_CENTER_VERTICAL));

    wxBoxSizer *pHzN = new wxBoxSizer(wxHORIZONTAL);
    m_LogNextNFrames = new wxCheckBox(parent, wxID_ANY, _("Until this count is reached"));
    m_LogNextNFrames->SetToolTip(_("Save each guider image until the specified number of images have been saved"));
    m_LogNextNFramesCount = pFrame->MakeSpinCtrl(parent, wxID_ANY, "1", wxDefaultPosition, wxSize(width, -1), wxSP_ARROW_KEYS, 1, 100, 1);
    m_LogNextNFramesCount->SetToolTip(_("Number of images to save"));
    pHzN->Add(m_LogNextNFrames, wxSizerFlags().Border(wxALL, PAD).Align(wxALIGN_CENTER_VERTICAL));
    pHzN->Add(m_LogNextNFramesCount, wxSizerFlags().Border(wxALL, PAD).Align(wxALIGN_CENTER_VERTICAL));

    pOptionsGrid->Add(m_LogDroppedFrames, wxSizerFlags().Border(wxALL, PAD));
    pOptionsGrid->Add(m_LogAutoSelectFrames, wxSizerFlags().Border(wxALL, PAD));
    pOptionsGrid->Add(pHzRel);
    pOptionsGrid->Add(pHzN);
    pOptionsGrid->Add(pHzAbs);
    m_LoggingOptions->Add(pOptionsGrid);

    AddGroup(CtrlMap, AD_szImageLoggingOptions, m_LoggingOptions);

    // Dither
    parent = GetParentWindow(AD_szDither);
    wxStaticBoxSizer *ditherGroupBox = new wxStaticBoxSizer(wxVERTICAL, parent, _("Dither Settings"));
    m_ditherRandom = new wxRadioButton(parent, wxID_ANY, _("Random"));
    m_ditherRandom->SetToolTip(_("Each dither command moves the lock position a random distance on each axis"));
    m_ditherSpiral = new wxRadioButton(parent, wxID_ANY, _("Spiral"));
    m_ditherSpiral->SetToolTip(_("Each dither command moves the lock position along a spiral path"));
    wxBoxSizer *sz = new wxBoxSizer(wxHORIZONTAL);
    sz->Add(new wxStaticText(parent, wxID_ANY, _("Mode: ")), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxALL, 8));
    sz->Add(m_ditherRandom, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxALL, 8));
    sz->Add(m_ditherSpiral, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxALL, 8));
    ditherGroupBox->Add(sz);

    m_ditherRaOnly = new wxCheckBox(parent, wxID_ANY, _("RA only"));
    m_ditherRaOnly->SetToolTip(_("Constrain dither to RA only"));

    width = StringWidth(_T("000.00"));
    m_ditherScaleFactor = pFrame->MakeSpinCtrlDouble(parent, wxID_ANY, _T(" "), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 0.1, 100.0, 0.0, 1.0);
    m_ditherScaleFactor->SetDigits(1);
    m_ditherScaleFactor->SetToolTip(_("Scaling for dither commands. Default = 1.0 (0.1-100.0)"));

    sz = new wxBoxSizer(wxHORIZONTAL);
    sz->Add(m_ditherRaOnly, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxLEFT, 8));
    sz->Add(new wxStaticText(parent, wxID_ANY, _("Scale") + _(": ")), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxLEFT, 40));
    sz->Add(m_ditherScaleFactor, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxLEFT, 10));
    ditherGroupBox->Add(sz);

    AddGroup(CtrlMap, AD_szDither, ditherGroupBox);

    parent = GetParentWindow(AD_cbAutoRestoreCal);
    m_pAutoLoadCalibration = new wxCheckBox(parent, wxID_ANY, _("Auto restore calibration"), wxDefaultPosition, wxDefaultSize);
    AddCtrl(CtrlMap, AD_cbAutoRestoreCal, m_pAutoLoadCalibration, _("For this equipment profile, automatically restore data from last successful calibration after gear is connected."));

    parent = GetParentWindow(AD_szAutoExposure);

    m_autoExpDurationMin = new wxComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxArrayString(), wxCB_READONLY);
    m_autoExpDurationMax = new wxComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxArrayString(), wxCB_READONLY);
    wxSize minsz(StringWidth(pFrame->ExposureDurationLabel(999990)) + 30, -1);
    m_autoExpDurationMin->SetMinSize(minsz);
    m_autoExpDurationMax->SetMinSize(minsz);

    width = StringWidth(_T("00.0"));
    m_autoExpSNR = pFrame->MakeSpinCtrlDouble(parent, wxID_ANY, _T(""), wxDefaultPosition,
        wxSize(width, -1), wxSP_ARROW_KEYS, 3.5, 99.9, 0.0, 1.0);

    wxFlexGridSizer *sz1 = new wxFlexGridSizer(1, 3, 10, 10);
    sz1->Add(MakeLabeledControl(AD_szAutoExposure, _("Min"), m_autoExpDurationMin, _("Auto exposure minimum duration")));
    sz1->Add(MakeLabeledControl(AD_szAutoExposure, _("Max"), m_autoExpDurationMax, _("Auto exposure maximum duration")), wxSizerFlags(0).Border(wxLEFT, 70));
    sz1->Add(MakeLabeledControl(AD_szAutoExposure, _("Target SNR"), m_autoExpSNR, _("Auto exposure target SNR value")), wxSizerFlags(0).Border(wxLEFT, 80));
    wxStaticBoxSizer *autoExp = new wxStaticBoxSizer(wxHORIZONTAL, parent, _("Auto Exposure"));
    autoExp->Add(sz1, wxSizerFlags(0).Expand());
    AddGroup(CtrlMap, AD_szAutoExposure, autoExp);

    wxFlexGridSizer *sz2 = new wxFlexGridSizer(1, 3, 10, 10);
    width = StringWidth(_T("600"));
    parent = GetParentWindow(AD_szVariableExposureDelay);
    m_varExposureDelayEnabled = new wxCheckBox(parent, wxID_ANY, _("Use Variable Exposure Delays"), wxDefaultPosition, wxDefaultSize);
    m_varExposureDelayEnabled->SetToolTip(_("Use \"short\" delay for calibration, looping, dithering, GA, \"long\" delay for normal guiding"));
    m_varExposureDelayEnabled->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MyFrameConfigDialogCtrlSet::OnVariableDelayChecked, this);
    sz2->Add(m_varExposureDelayEnabled, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxLEFT, 8));
    m_varExpDelayShort = pFrame->MakeSpinCtrl(parent, wxID_ANY, _T(" "), wxDefaultPosition, wxSize(width, -1), wxSP_ARROW_KEYS, 0, 10, 0, _T("ExpDelayShort"));
    m_varExpDelayLong = pFrame->MakeSpinCtrl(parent, wxID_ANY, _T(" "), wxDefaultPosition, wxSize(width, -1), wxSP_ARROW_KEYS, 0, 120, 0, _T("ExpDelayLong"));
    sz2->Add(MakeLabeledControl(AD_szVariableExposureDelay, _("Short delay (sec)"), m_varExpDelayShort, _("Short delay for calibration, looping, dithering, GA")));
    sz2->Add(MakeLabeledControl(AD_szVariableExposureDelay, _("Long delay (sec)"), m_varExpDelayLong, _("Long delay for normal guiding")));
    wxStaticBoxSizer* varDelayGrp = new wxStaticBoxSizer(wxHORIZONTAL, parent, _("Variable Exposure Delay (High-precision encoder mounts)"));
    varDelayGrp->Add(sz2, wxSizerFlags(0).Expand());
    AddGroup(CtrlMap, AD_szVariableExposureDelay, varDelayGrp);
}

void MyFrameConfigDialogCtrlSet::LoadValues()
{
    m_pResetConfiguration->SetValue(false);
    m_pResetConfiguration->Enable(!pFrame->CaptureActive);
    m_pResetDontAskAgain->SetValue(false);
    m_pNoiseReduction->SetSelection(pFrame->GetNoiseReductionMethod());
    if (m_pFrame->GetDitherMode() == DITHER_RANDOM)
        m_ditherRandom->SetValue(true);
    else
        m_ditherSpiral->SetValue(true);
    m_ditherRaOnly->SetValue(m_pFrame->GetDitherRaOnly());
    m_ditherScaleFactor->SetValue(m_pFrame->GetDitherScaleFactor());
    m_pTimeLapse->SetValue(m_pFrame->GetTimeLapse());
    VarDelayCfg delayCfg = m_pFrame->GetVariableDelayConfig();
    m_varExposureDelayEnabled->SetValue(delayCfg.enabled);
    m_varExpDelayShort->SetValue((int)delayCfg.shortDelay / 1000.);
    m_varExpDelayLong->SetValue((int)delayCfg.longDelay / 1000.);
    m_pTimeLapse->Enable(!delayCfg.enabled);
    m_varExpDelayShort->Enable(delayCfg.enabled);
    m_varExpDelayLong->Enable(delayCfg.enabled);

    SetFocalLength(m_pFrame->GetFocalLength());
    m_pFocalLength->Enable(!pFrame->CaptureActive);

    int language = wxGetApp().GetLocale().GetLanguage();
    m_oldLanguageChoice = PhdLanguages.Index(language);
    m_pLanguage->SetSelection(m_oldLanguageChoice);
    m_pLanguage->Enable(!pFrame->CaptureActive);

    m_pLogDir->SetValue(GuideLog.GetLogDir());
    m_pLogDir->Enable(!pFrame->CaptureActive);
    m_pSelectDir->Enable(!pFrame->CaptureActive);
    m_pAutoLoadCalibration->SetValue(m_pFrame->GetAutoLoadCalibration());

    const AutoExposureCfg& cfg = m_pFrame->GetAutoExposureCfg();

    std::vector<int> dur(pFrame->GetExposureDurations());
    std::sort(dur.begin(), dur.end());
    wxArrayString as;
    for (auto it = dur.begin(); it != dur.end(); ++it)
        as.Add(pFrame->ExposureDurationLabel(*it));

    m_autoExpDurationMin->Set(as);
    m_autoExpDurationMax->Set(as);

    auto pos = std::find(dur.begin(), dur.end(), cfg.minExposure);
    if (pos == dur.end())
        pos = std::find(dur.begin(), dur.end(), DefaultAutoExpMin);
    m_autoExpDurationMin->SetSelection(pos - dur.begin());

    pos = std::find(dur.begin(), dur.end(), cfg.maxExposure);
    if (pos == dur.end())
        pos = std::find(dur.begin(), dur.end(), DefaultAutoExpMax);
    m_autoExpDurationMax->SetSelection(pos - dur.begin());

    m_autoExpSNR->SetValue(cfg.targetSNR);

    ImageLoggerSettings imlSettings;
    ImageLogger::GetSettings(&imlSettings);

    m_EnableImageLogging->SetValue(imlSettings.loggingEnabled);
    m_LogDroppedFrames->SetValue(imlSettings.logFramesDropped);
    m_LogAutoSelectFrames->SetValue(imlSettings.logAutoSelectFrames);
    m_LogRelErrors->SetValue(imlSettings.logFramesOverThreshRel);
    m_LogRelErrorThresh->SetValue(imlSettings.guideErrorThreshRel);
    m_LogAbsErrors->SetValue(imlSettings.logFramesOverThreshPx);
    m_LogAbsErrorThresh->SetValue(imlSettings.guideErrorThreshPx);
    m_LogNextNFrames->SetValue(imlSettings.logNextNFrames);
    m_LogNextNFramesCount->SetValue(imlSettings.logNextNFramesCount);

    UpdaterSettings updSettings;
    PHD2Updater::GetSettings(&updSettings);

    m_updateEnabled->SetValue(updSettings.enabled);
    m_updateMajorOnly->SetValue(updSettings.series == UPD_SERIES_MAIN);

    wxCommandEvent dummy;
    OnImageLogEnableChecked(dummy);
}

void MyFrameConfigDialogCtrlSet::UnloadValues()
{
    try
    {
        if (m_pResetConfiguration->GetValue())
        {
            int choice = wxMessageBox(_("This will reset all PHD2 configuration values and restart the program.  Are you sure?"), _("Confirmation"), wxYES_NO);

            if (choice == wxYES)
            {
                wxGetApp().ResetConfiguration();
                wxGetApp().RestartApp();
            }
        }

        if (this->m_pResetDontAskAgain->GetValue())
        {
            ConfirmDialog::ResetAllDontAskAgain();
        }

        m_pFrame->SetNoiseReductionMethod(m_pNoiseReduction->GetSelection());
        m_pFrame->SetDitherMode(m_ditherRandom->GetValue() ? DITHER_RANDOM : DITHER_SPIRAL);
        m_pFrame->SetDitherRaOnly(m_ditherRaOnly->GetValue());
        m_pFrame->SetDitherScaleFactor(m_ditherScaleFactor->GetValue());
        m_pFrame->SetTimeLapse(m_pTimeLapse->GetValue());
        pFrame->SetVariableDelayConfig(m_varExposureDelayEnabled->GetValue(), m_varExpDelayShort->GetValue() * 1000, m_varExpDelayLong->GetValue() * 1000);
        int oldFL = m_pFrame->GetFocalLength();
        int newFL = GetFocalLength();               // From UI control
        if (oldFL != newFL)
            m_pFrame->pAdvancedDialog->MakeImageScaleAdjustments();
        m_pFrame->SetFocalLength(GetFocalLength());

        int idx = m_pLanguage->GetSelection();
        int langid = PhdLanguages.LangId(idx);
        pConfig->Global.SetInt("/wxLanguage", langid);
        if (m_oldLanguageChoice != idx)
        {
            wxString title = TranslateStrToLang(wxTRANSLATE("Restart PHD2"), langid);
            wxString msg = TranslateStrToLang(wxTRANSLATE("You must restart PHD2 for the language change to take effect.\n"
                "Would you like to restart PHD2 now?"), langid);
            int val = wxMessageBox(msg, title, wxYES_NO | wxCENTRE);
            if (val == wxYES)
                wxGetApp().RestartApp();
        }

        wxString newdir = m_pLogDir->GetValue();
        if (!newdir.IsSameAs(GuideLog.GetLogDir()))
        {
            GuideLog.ChangeDirLog(newdir);
            Debug.ChangeDirLog(newdir);
        }

        m_pFrame->SetAutoLoadCalibration(m_pAutoLoadCalibration->GetValue());

        std::vector<int> dur(m_pFrame->GetExposureDurations());
        std::sort(dur.begin(), dur.end());
        int durationMin = dur[m_autoExpDurationMin->GetSelection()];
        int durationMax = dur[m_autoExpDurationMax->GetSelection()];
        if (durationMax < durationMin)
            durationMax = durationMin;
        bool cfg_changed = m_pFrame->SetAutoExposureCfg(durationMin, durationMax, m_autoExpSNR->GetValue());
        if (m_pFrame->m_autoExp.enabled && cfg_changed)
            m_pFrame->NotifyExposureChanged();

        ImageLoggerSettings imlSettings;
        ImageLogger::GetSettings(&imlSettings);

        imlSettings.loggingEnabled = m_EnableImageLogging->GetValue();
        if (imlSettings.loggingEnabled)
        {
            imlSettings.logFramesOverThreshRel = m_LogRelErrors->GetValue();
            imlSettings.logFramesOverThreshPx = m_LogAbsErrors->GetValue();
            imlSettings.logFramesDropped = m_LogDroppedFrames->GetValue();
            imlSettings.logAutoSelectFrames = m_LogAutoSelectFrames->GetValue();
            imlSettings.guideErrorThreshRel = m_LogRelErrorThresh->GetValue();
            imlSettings.guideErrorThreshPx = m_LogAbsErrorThresh->GetValue();
            imlSettings.logNextNFrames = m_LogNextNFrames->GetValue();
            imlSettings.logNextNFramesCount = m_LogNextNFramesCount->GetValue();
        }

        ImageLogger::ApplySettings(imlSettings);
        SaveImageLoggerSettings(imlSettings);

        UpdaterSettings updSettings;
        updSettings.enabled = m_updateEnabled->GetValue();
        updSettings.series = m_updateMajorOnly->GetValue() ? UPD_SERIES_MAIN : UPD_SERIES_DEV;
        PHD2Updater::SetSettings(updSettings);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }
}

// Following are needed by step-size calculator to keep the UIs in-synch
int MyFrameConfigDialogCtrlSet::GetFocalLength() const
{
    long val = 0;
    m_pFocalLength->GetValue().ToLong(&val);
    return (int)val;
}

void MyFrameConfigDialogCtrlSet::SetFocalLength(int val)
{
    m_pFocalLength->SetValue(wxString::Format(_T("%d"), val));
}

void MyFrameConfigDialogCtrlSet::OnDirSelect(wxCommandEvent& evt)
{
    wxString sRtn = wxDirSelector("Choose a location", m_pLogDir->GetValue());

    if (sRtn.Len() > 0)
        m_pLogDir->SetValue(sRtn);
}

void MyFrameConfigDialogCtrlSet::OnImageLogEnableChecked(wxCommandEvent& event)
{
    // wxStaticBoxSizer doesn't have an Enable method :-(
    bool setIt = m_EnableImageLogging->IsChecked();
    m_LogRelErrors->Enable(setIt);
    m_LogRelErrorThresh->Enable(setIt);
    m_LogAbsErrors->Enable(setIt);
    m_LogAbsErrorThresh->Enable(setIt);
    m_LogDroppedFrames->Enable(setIt);
    m_LogAutoSelectFrames->Enable(setIt);
    m_LogNextNFrames->Enable(setIt);
    m_LogNextNFramesCount->Enable(setIt);
}

void MyFrameConfigDialogCtrlSet::OnVariableDelayChecked(wxCommandEvent& evt)
{
    m_pTimeLapse->Enable(!evt.IsChecked());
    m_varExpDelayShort->Enable(evt.IsChecked());
    m_varExpDelayLong->Enable(evt.IsChecked());
}

void MyFrame::PlaceWindowOnScreen(wxWindow *win, int x, int y)
{
    if (x < 0 || x > wxSystemSettings::GetMetric(wxSYS_SCREEN_X) - 20 ||
        y < 0 || y > wxSystemSettings::GetMetric(wxSYS_SCREEN_Y) - 20)
    {
        win->Centre(wxBOTH);
    }
    else
        win->Move(x, y);
}

inline static void AdjustSpinnerWidth(wxSize *sz)
{
#ifdef __APPLE__
    // GetSizeFromTextSize() not working on OSX, so we need to add more padding
    enum { SPINNER_WIDTH_PAD = 20 };
    sz->SetWidth(sz->GetWidth() + SPINNER_WIDTH_PAD);
#endif
}

// The spin control factories allow clients to specify a width based on the max width of the numeric values without having
// to make guesses about the additional space required by the other parts of the control
wxSpinCtrl* MyFrame::MakeSpinCtrl(wxWindow *parent, wxWindowID id, const wxString& value,
    const wxPoint& pos, const wxSize& size, long style,
    int min, int max, int initial, const wxString& name)
{
    wxSpinCtrl *ctrl = new wxSpinCtrl(parent, id, value, pos, size, style, min, max, initial, name);
    wxSize initsize(ctrl->GetSizeFromTextSize(size));
    AdjustSpinnerWidth(&initsize);
    ctrl->SetInitialSize(initsize);
    return ctrl;
}

wxSpinCtrlDouble* MyFrame::MakeSpinCtrlDouble(wxWindow *parent, wxWindowID id, const wxString& value,
    const wxPoint& pos, const wxSize& size, long style, double min, double max, double initial,
    double inc, const wxString& name)
{
    wxSpinCtrlDouble *ctrl = new wxSpinCtrlDouble(parent, id, value, pos, size, style, min, max, initial, inc, name);
    wxSize initsize(ctrl->GetSizeFromTextSize(size));
    AdjustSpinnerWidth(&initsize);
    ctrl->SetInitialSize(initsize);
    return ctrl;
}

template<typename T>
static void NotifyGuidingParam(const wxString& name, T val)
{
    GuideLog.SetGuidingParam(name, val);
    EvtServer.NotifyGuidingParam(name, val);
}

void MyFrame::NotifyGuidingParam(const wxString& name, double val)
{
    ::NotifyGuidingParam(name, val);
}

void MyFrame::NotifyGuidingParam(const wxString& name, int val)
{
    ::NotifyGuidingParam(name, val);
}

void MyFrame::NotifyGuidingParam(const wxString& name, bool val)
{
    ::NotifyGuidingParam(name, val);
}

void MyFrame::NotifyGuidingParam(const wxString& name, const wxString& val)
{
    ::NotifyGuidingParam(name, val);
}

// Interface to force logging if guiding is not active
void MyFrame::NotifyGuidingParam(const wxString& name, const wxString& val, bool ForceLog)
{
    GuideLog.SetGuidingParam(name, val, true);
    EvtServer.NotifyGuidingParam(name, val);
}
