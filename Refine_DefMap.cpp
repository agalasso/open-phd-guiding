/*
 *  Refine_DefMap.cpp
 *  PHD Guiding
 *
 *  Created by Bruce Waddington in collaboration with Andy Galasso and David Ault
 *  Copyright(c) 2014 Bruce Waddington
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
#include "Refine_DefMap.h"
#include "darks_dialog.h"

enum {
    ID_PREVIEW = 10001,
};

wxBEGIN_EVENT_TABLE(RefineDefMap, wxDialog)
    EVT_CHECKBOX(ID_PREVIEW, RefineDefMap::OnPreview)
    EVT_CLOSE(RefineDefMap::OnClose)
wxEND_EVENT_TABLE()

static const double DefDMSigmaX = 75;

inline static void StartRow(int& row, int& column)
{
    ++row;
    column = 0;
}

// Utility function to add the <label, ctrl> pairs to a flexgrid
static void AddTableEntryPair(wxWindow *parent, wxFlexGridSizer *pTable, const wxString& label, wxWindow *pControl)
{
    wxStaticText *pLabel = new wxStaticText(parent, wxID_ANY, label + _(": "), wxPoint(-1, -1), wxSize(-1, -1));
    pTable->Add(pLabel, 1, wxALL, 5);
    pTable->Add(pControl, 1, wxALL, 5);
}

RefineDefMap::RefineDefMap(wxWindow *parent) :
    wxDialog(parent, wxID_ANY, _("Refine Bad-pixel Map"), wxDefaultPosition, wxSize(900, 400), wxCAPTION | wxCLOSE_BOX), m_profileId(-1)
{
    SetSize(wxSize(900, 400));

    // Create the vertical sizer and first group box we're going to need
    wxSizer *pVSizer;
    pVSizer = new wxBoxSizer(wxVERTICAL);

    pRebuildDarks = new wxCheckBox(this, wxID_ANY, _("Rebuild Master Dark Frame"));
    pRebuildDarks->SetToolTip(_("Click to re-acquire the master dark frames needed to compute an initial bad-pixel map"));
    pVSizer->Add(pRebuildDarks, wxSizerFlags(0).Border(wxALL, 10));

    wxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

    pShowDetails = new wxCheckBox(this, wxID_ANY, _("Show Master Dark Details"));
    pShowDetails->SetToolTip(_("Click to display detailed statistics of master dark frame used to compute bad-pixel map"));
    pShowDetails->Bind(wxEVT_CHECKBOX, &RefineDefMap::OnDetails, this);
    hsizer->Add(pShowDetails, wxSizerFlags(0).Border(wxALL, 10));

    pShowPreview = new wxCheckBox(this, ID_PREVIEW, _("Show defect pixels"));
    pShowPreview->SetToolTip(_("Check to show hot/cold pixels in the main image window."));
    hsizer->Add(pShowPreview, wxSizerFlags(0).Border(wxALL, 10));

    pVSizer->Add(hsizer);

    pInfoGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("General Information"));
    pInfoGrid = new wxGrid(this, wxID_ANY);
    pInfoGrid->CreateGrid(5, 4);
    pInfoGrid->SetRowLabelSize(1);
    pInfoGrid->SetColLabelSize(1);
    pInfoGrid->EnableEditing(false);
    pInfoGrid->SetDefaultColSize(150);

    int col = 0;
    int row = 0;
    pInfoGrid->SetCellValue(row, col++, _("Time:"));
    createTimeLoc.Set(row, col++);
    pInfoGrid->SetCellValue(row, col++, _("Camera:"));
    cameraLoc.Set(row, col++);

    StartRow(row, col);
    pInfoGrid->SetCellValue(row, col++, _("Master Dark Exposure Time:"));
    expTimeLoc.Set(row, col++);
    pInfoGrid->SetCellValue(row, col++, _("Master Dark Exposure Count:"));
    expCntLoc.Set(row, col++);
    // Not convenient to use AutoSize() method because some columns are populated later
    pInfoGrid->SetColSize(0, StringWidth(this, _("Master Dark Exposure Time:")) + 5);
    pInfoGrid->SetColSize(2, StringWidth(this, _("Master Dark Exposure Count:")) + 5);

    StartRow(row, col);
    pInfoGrid->SetCellValue(row, col++, _("Aggressiveness, hot pixels:"));
    hotFactorLoc.Set(row, col++);
    pInfoGrid->SetCellValue(row, col++, _("Aggressiveness, cold pixels:"));
    coldFactorLoc.Set(row, col++);

    StartRow(row, col);
    pInfoGrid->SetCellValue(row, col++, _("Mean:"));
    meanLoc.Set(row, col++);
    pInfoGrid->SetCellValue(row, col++, _("Standard Deviation:"));
    stdevLoc.Set(row, col++);

    StartRow(row, col);
    pInfoGrid->SetCellValue(row, col++, _("Median:"));
    medianLoc.Set(row, col++);
    pInfoGrid->SetCellValue(row, col++, _("Median Absolute Deviation:"));
    madLoc.Set(row, col++);

    pInfoGroup->Add(pInfoGrid);
    pVSizer->Add(pInfoGroup, wxSizerFlags(0).Border(wxALL, 15));

    // Build the stats group controls
    wxStaticBoxSizer *pStatsGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Results"));
    pStatsGrid = new wxGrid(this, wxID_ANY);
    pStatsGrid->CreateGrid(2, 4);
    pStatsGrid->SetRowLabelSize(1);
    pStatsGrid->SetColLabelSize(1);
    pStatsGrid->EnableEditing(false);
    pStatsGrid->SetDefaultColSize(150);

    row = 0;
    col = 0;
    pStatsGrid->SetCellValue(row, col++, _("Hot pixel count:"));
    hotPixelLoc.Set(row, col++);
    pStatsGrid->SetCellValue(row, col++, _("Cold pixel count:"));
    coldPixelLoc.Set(row, col++);

    StartRow(row, col);
    pStatsGrid->SetCellValue(row, col++, _("Manually added pixels"));
    manualPixelLoc.Set(row, col++);
    pStatsGroup->Add(pStatsGrid);
    pVSizer->Add(pStatsGroup, wxSizerFlags(0).Border(wxALL, 10));

    // Put the aggressiveness sliders in
    wxStaticBoxSizer *pAggressivenessGrp = new wxStaticBoxSizer(wxVERTICAL, this, _("Aggressiveness"));
    pAdjustmentGrid = new wxFlexGridSizer(1, 4, 5, 15);
    pHotSlider = new wxSlider(this, wxID_ANY, 0, 0, 100, wxPoint(-1, -1), wxSize(200, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
    pHotSlider->Bind(wxEVT_SCROLL_CHANGED, &RefineDefMap::OnHotChange, this);
    pHotSlider->Bind(wxEVT_SCROLL_THUMBTRACK, &RefineDefMap::OnHotChange, this);
    pHotSlider->SetToolTip(_("Move this slider to increase or decrease the number of pixels that will be treated as 'hot', then click on 'generate' to build and load the new bad-pixel map"));
    pColdSlider = new wxSlider(this, wxID_ANY, 0, 0, 100, wxPoint(-1, -1), wxSize(200, -1), wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
    pColdSlider->Bind(wxEVT_SCROLL_CHANGED, &RefineDefMap::OnColdChange, this);
    pColdSlider->Bind(wxEVT_SCROLL_THUMBTRACK, &RefineDefMap::OnColdChange, this);
    pColdSlider->SetToolTip(_("Move this slider to increase or decrease the number of pixels that will be treated as 'cold', then click on 'generate' to build and load the new bad-pixel map"));
    AddTableEntryPair(this, pAdjustmentGrid, _("Hot pixels"), pHotSlider);
    AddTableEntryPair(this, pAdjustmentGrid, _("Cold pixels"), pColdSlider);
    pAggressivenessGrp->Add(pAdjustmentGrid);
    pVSizer->Add(pAggressivenessGrp, wxSizerFlags(0).Border(wxALL, 10));

    // Buttons
    wxBoxSizer *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    pResetBtn = new wxButton(this, wxID_ANY, _("Reset"));
    pResetBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &RefineDefMap::OnReset, this);
    pResetBtn->SetToolTip(_("Reset parameters to starting point"));

    pApplyBtn = new wxButton(this, wxID_ANY, _("Generate"));
    pApplyBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &RefineDefMap::OnGenerate, this);
    pApplyBtn->SetToolTip(_("Use the current aggressiveness settings to build and load a new bad-pixel map; this will discard any manually added bad pixels"));

    pAddDefectBtn = new wxButton(this, wxID_ANY, _("Add Bad Pixel"));
    pAddDefectBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &RefineDefMap::OnAddDefect, this);
    pAddDefectBtn->SetToolTip(_("Click on a bad pixel in the image display; then click on this button to add it to the in-use bad-pixel map"));

    pButtonSizer->Add(
        pResetBtn,
        wxSizerFlags(0).Align(0).Border(wxALL, 10));
    pButtonSizer->Add(
        pApplyBtn,
        wxSizerFlags(0).Align(0).Border(wxALL, 10));
    pButtonSizer->Add(
        pAddDefectBtn,
        wxSizerFlags(0).Align(0).Border(wxALL, 10));

    pVSizer->Add(pButtonSizer, wxSizerFlags().Center().Border(wxALL, 10));

    // status bar
    pStatusBar = new wxStatusBar(this, -1);
    pStatusBar->SetFieldsCount(1);
    pVSizer->Add(pStatusBar, 0, wxGROW);

    SetSizerAndFit(pVSizer);
    ShowStatus(_("Adjust sliders to increase/decrease pixels marked as bad"), false);
}

void RefineDefMap::InitCameraMode()
{
    // change the star finding mode to select peaks, not centroids
    m_saveStarFindMode = pFrame->SetStarFindMode(Star::FIND_PEAK);
    pFrame->SetRawImageMode(true); // no "recon" (debayer/deinterlace)
    // disable subframes
    m_saveUseSubframes = pCamera->UseSubframes;
    pCamera->UseSubframes = false;
}

void RefineDefMap::RestoreCameraMode()
{
    pFrame->SetRawImageMode(false); // raw images not needed any more
    pFrame->SetStarFindMode(m_saveStarFindMode);
    pCamera->UseSubframes = m_saveUseSubframes;
}

bool RefineDefMap::InitUI()
{
    InitCameraMode();

    if (pConfig->GetCurrentProfileId() == m_profileId)
    {
        RefreshPreview();
        return true;
    }

    bool firstTime = false;
    m_profileId = pConfig->GetCurrentProfileId();
    manualPixelCount = 0;
    if (!DefectMap::DefectMapExists(pConfig->GetCurrentProfileId(), false))
    {
        if (RebuildMasterDarks())
            firstTime = true;       // Need to get the UI built before finishing up
    }
    if (DefectMap::DefectMapExists(m_profileId, false) || firstTime)
    {
        LoadFromProfile();
        if (firstTime)
            ApplyNewMap();
        RefreshPreview();
        return true;
    }
    else
    {
        RestoreCameraMode();
        return false;      // No master dark files to work with, user didn't build them
    }
}

// Do the initial layout of the UI controls
void RefineDefMap::LoadFromProfile()
{
    // Let the user know this might take some time...
    ShowStatus(_("Please wait while image statistics are being computed..."), false);

    m_darks.LoadDarks();
    m_builder.Init(m_darks);

    const ImageStats& stats = m_builder.GetImageStats();

    MiscInfo info;
    GetMiscInfo(info);

    info.lastHotFactor.ToLong(&initHotFactor);
    info.lastColdFactor.ToLong(&initColdFactor);

    pHotSlider->SetValue(initHotFactor);
    pColdSlider->SetValue(initColdFactor);

    pShowDetails->SetValue(pConfig->Profile.GetBoolean("/camera/dmap_show_details", true));
    pInfoGroup->Show(pShowDetails->GetValue());

    pInfoGrid->SetCellValue(createTimeLoc, DefectMapTimeString());
    pInfoGrid->SetCellValue(cameraLoc, info.cameraName);

    pInfoGrid->SetCellValue(expTimeLoc, info.darkExposureTime);
    pInfoGrid->SetCellValue(expCntLoc, info.darkCount);

    pInfoGrid->SetCellValue(hotFactorLoc, info.lastHotFactor);
    pInfoGrid->SetCellValue(coldFactorLoc, info.lastColdFactor);

    pInfoGrid->SetCellValue(meanLoc, wxString::Format("%.2f", stats.mean));
    pInfoGrid->SetCellValue(stdevLoc, wxString::Format("%.2f", stats.stdev));
    pInfoGrid->SetCellValue(medianLoc, wxString::Format("%d", stats.median));
    pInfoGrid->SetCellValue(madLoc, wxString::Format("%d", stats.mad));

    GetBadPxCounts();
    ShowStatus(_("Statistics completed..."), false);
    LoadPreview();

}

bool RefineDefMap::RebuildMasterDarks()
{
    bool rslt = false;
    DarksDialog dlg(this, false);

    if (dlg.ShowModal() == wxOK)
    {
        m_darks.LoadDarks();
        if (m_darks.filteredDark.ImageData && m_darks.masterDark.ImageData)
        {
            m_builder.Init(m_darks);
            rslt = true;
        }
    }
    return rslt;
}

void RefineDefMap::ShowStatus(const wxString& msg, bool appending)
{
    static wxString preamble;

    if (appending)
        pStatusBar->SetStatusText(preamble + " " + msg);
    else
    {
        pStatusBar->SetStatusText(msg);
        preamble = msg;
    }
    pStatusBar->Update();
}

// Build a new defect map based on current aggressiveness params; load it and update the UI
void RefineDefMap::ApplyNewMap()
{
    m_builder.SetAggressiveness(pColdSlider->GetValue(), pHotSlider->GetValue());
    // This can take a bit of time...
    pHotSlider->Enable(false);
    pColdSlider->Enable(false);
    ShowStatus(_("Building new bad-pixel map"), false);
    m_builder.BuildDefectMap(m_defectMap, true);
    ShowStatus(_("Saving new bad-pixel map file"), false);
    m_defectMap.Save(m_builder.GetMapInfo());
    ShowStatus(_("Loading new bad-pixel map"), false);
    pFrame->LoadDefectMapHandler(true);
    ShowStatus(_("New bad-pixel map now being used"), false);
    pConfig->Profile.SetInt("/camera/dmap_hot_factor", pHotSlider->GetValue());
    pConfig->Profile.SetInt("/camera/dmap_cold_factor", pColdSlider->GetValue());
    // Since we've saved the defect map, update the baseline info about aggressiveness settings
    pInfoGrid->SetCellValue(hotFactorLoc, wxString::Format("%d", pHotSlider->GetValue()));
    pInfoGrid->SetCellValue(coldFactorLoc, wxString::Format("%d", pColdSlider->GetValue()));
    pInfoGrid->SetCellValue(createTimeLoc, DefectMapTimeString());
    pStatsGrid->SetCellValue(manualPixelLoc, "0");          // Manual pixels will always be discarded
    pHotSlider->Enable(true);
    pColdSlider->Enable(true);
    pFrame->SetDarkMenuState();                     // Get enabled states straightened out
}

void RefineDefMap::OnGenerate(wxCommandEvent& evt)
{
    if (pRebuildDarks->GetValue())
    {
        if (RebuildMasterDarks())
        {
            pRebuildDarks->SetValue(false);
            LoadFromProfile();
        }
        else
        {
            ShowStatus(_("Master dark frames NOT rebuilt"), false);
            return;         // Couldn't do what we were asked
        }
    }
    ApplyNewMap();
}

// Get the timestamp from the file modification timestamp of the defectmap .txt file
wxString RefineDefMap::DefectMapTimeString()
{
    wxString dfFileName = DefectMap::DefectMapFileName(pConfig->GetCurrentProfileId());
    if (wxFileExists(dfFileName))
    {
        wxDateTime when = wxFileModificationTime(dfFileName);
        return(when.FormatDate() + "  " + when.FormatTime());
    }
    else
        return "";
}

// Gather the background info for the last constructed defect map
void RefineDefMap::GetMiscInfo(MiscInfo& info)
{
    info.creationTime = DefectMapTimeString();
    info.cameraName = pCamera->Name;
    info.darkExposureTime = wxString::Format("%.1f", m_darks.masterDark.ImgExpDur / 1000.0);
    info.darkCount = wxString::Format("%d", m_darks.masterDark.ImgStackCnt);
    info.lastHotFactor = wxString::Format("%d", pConfig->Profile.GetInt("/camera/dmap_hot_factor", DefDMSigmaX));
    info.lastColdFactor = wxString::Format("%d", pConfig->Profile.GetInt("/camera/dmap_cold_factor", DefDMSigmaX));
}

// Recompute hot/cold pixel counts based on current aggressiveness settings
void RefineDefMap::Recalc()
{
    if (manualPixelCount != 0)
    {
        manualPixelCount = 0;
        pStatsGrid->SetCellValue(manualPixelLoc, "0");          // Manual pixels will always be discarded
    }
    GetBadPxCounts();
    m_builder.BuildDefectMap(m_defectMap, false);
}

void RefineDefMap::OnHotChange(wxScrollEvent& evt)
{
    Recalc();
    pStatsGrid->SetCellBackgroundColour(hotPixelLoc.GetRow(), hotPixelLoc.GetCol(), "light blue");
    RefreshPreview();
}

void RefineDefMap::OnColdChange(wxScrollEvent& evt)
{
    Recalc();
    pStatsGrid->SetCellBackgroundColour(coldPixelLoc.GetRow(), coldPixelLoc.GetCol(), "light blue");
    RefreshPreview();
}

// Manually add a bad pixel to the currently loaded (in-memory) defect map - does NOT affect any future map generations
void RefineDefMap::OnAddDefect(wxCommandEvent& evt)
{
    PHD_Point pixelLoc = pFrame->pGuider->CurrentPosition();

    if (pFrame->pGuider->IsLocked())
    {
        wxPoint badspot((int)(pixelLoc.X + 0.5), (int)(pixelLoc.Y + 0.5));
        Debug.AddLine(wxString::Format("Current position returned as %.1f,%.1f", pixelLoc.X, pixelLoc.Y));
        Debug.AddLine(wxString::Format("User adding bad pixel at %d,%d", badspot.x, badspot.y));

        bool needLoadPreview = false;

        { // lock around changes to defect map
            wxCriticalSectionLocker lck(pCamera->DarkFrameLock);
            DefectMap *pCurrMap = pCamera->CurrentDefectMap;
            if (pCurrMap)
            {
                if (!pCurrMap->FindDefect(badspot))
                {
                    pCurrMap->AddDefect(badspot);           // Changes both in-memory instance and disk file
                    manualPixelCount++;
                    pStatsGrid->SetCellValue(manualPixelLoc, wxString::Format("%d", manualPixelCount));
                    needLoadPreview = true;
                }
            }
            else
                ShowStatus(_("You must first load a bad-pixel map"), false);
        } // lock defect map

        if (needLoadPreview)
        {
            ShowStatus(wxString::Format(_("Bad pixel marked at %d,%d"), badspot.x, badspot.y), false);
            LoadPreview();
            RefreshPreview();
        }
    }
    else
        ShowStatus(_("Pixel position not added - no star-like object recognized there"), false);
}

// Re-generate a defect map based on settings seen at app-startup; manually added pixels will be lost
void RefineDefMap::OnReset(wxCommandEvent& evt)
{
    pHotSlider->SetValue(initHotFactor);
    pColdSlider->SetValue(initColdFactor);
    Recalc();
    RefreshPreview();
    ShowStatus(_("Settings restored to original values"), false);
}

void RefineDefMap::LoadPreview()
{
    m_defectMap.clear();

    wxCriticalSectionLocker lck(pCamera->DarkFrameLock);
    DefectMap *curMap = pCamera->CurrentDefectMap;
    if (curMap)
    {
        m_defectMap = *curMap;
    }
}

void RefineDefMap::RefreshPreview()
{
    if (pShowPreview->IsChecked())
        pFrame->pGuider->SetDefectMapPreview(&m_defectMap);
    else
        pFrame->pGuider->SetDefectMapPreview(0);
}

void RefineDefMap::OnPreview(wxCommandEvent& evt)
{
    RefreshPreview();
}

// Get a recalculation of the bad-pixel counts based on current user aggressiveness settings
void RefineDefMap::GetBadPxCounts()
{
    m_builder.SetAggressiveness(pColdSlider->GetValue(), pHotSlider->GetValue());
    pStatsGrid->SetCellValue(hotPixelLoc, wxString::Format("%d", m_builder.GetHotPixelCnt()));
    pStatsGrid->SetCellValue(coldPixelLoc, wxString::Format("%d", m_builder.GetColdPixelCnt()));
}

void RefineDefMap::OnDetails(wxCommandEvent& ev)
{
    pInfoGroup->Show(pShowDetails->GetValue());
    Layout();
    GetSizer()->Fit(this);
}

// Hook the close event to tweak setting of 'build defect map' menu - mutual exclusion for now
void RefineDefMap::OnClose(wxCloseEvent& evt)
{
    RestoreCameraMode();
    pFrame->pGuider->SetDefectMapPreview(0);
    pFrame->darks_menu->FindItem(MENU_TAKEDARKS)->Enable(!pFrame->CaptureActive);
    pConfig->Profile.SetBoolean("/camera/dmap_show_details", pShowDetails->GetValue());
    evt.Skip();
}

// We're modeless, so we need to clean up the global pointer to our dialog
RefineDefMap::~RefineDefMap()
{
    pFrame->pRefineDefMap = NULL;
}
