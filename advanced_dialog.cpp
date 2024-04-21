/*
 *  advanced_dialog.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
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
#include "calstep_dialog.h"

#include <wx/tipwin.h>

#if defined(__WXOSX__)
# include <wx/choicebk.h>
#endif

const double AdvancedDialog::MIN_FOCAL_LENGTH = 50.0;
const double AdvancedDialog::MAX_FOCAL_LENGTH = 10000.0;

// a place to save id of selected panel so we can select the same panel next time the dialog is opened
static int s_selectedPage = -1;

void AdvancedDialog::BuildCtrlSets()
{
    m_pGlobalCtrlSet = m_pFrame->GetConfigDlgCtrlSet(m_pFrame, this, m_brainCtrls);
    if (pCamera)
        m_pCameraCtrlSet = pCamera->GetConfigDlgCtrlSet(m_pCameraSettingsPanel, pCamera, this, m_brainCtrls);
    else
        m_pCameraCtrlSet = nullptr;

    m_pGuiderCtrlSet = m_pFrame->pGuider->GetConfigDialogCtrlSet(m_pGuiderSettingsPanel, m_pFrame->pGuider, this, m_brainCtrls);

    if (TheAO())
        m_pAOCtrlSet = new AOConfigDialogCtrlSet(m_pDevicesSettingsPanel, pMount, this, m_brainCtrls);
    else
        m_pAOCtrlSet = nullptr;

    if (pRotator)
        m_pRotatorCtrlSet = pRotator->GetConfigDlgCtrlSet(m_pDevicesSettingsPanel, pRotator, this, m_brainCtrls);
    else
        m_pRotatorCtrlSet = nullptr;

    // Need a scope ctrl set even if pMount is null - it exports generic controls needed by other panes
    m_pScopeCtrlSet = new ScopeConfigDialogCtrlSet(m_pGuiderSettingsPanel, TheScope(), this, m_brainCtrls);
}

void AdvancedDialog::CleanupCtrlSets()
{
    delete m_pGlobalCtrlSet;
    m_pGlobalCtrlSet = nullptr;

    delete m_pCameraCtrlSet;
    m_pCameraCtrlSet = nullptr;

    delete m_pGuiderCtrlSet;
    m_pGuiderCtrlSet = nullptr;

    delete m_pScopeCtrlSet;
    m_pScopeCtrlSet = nullptr;

    delete m_pAOCtrlSet;
    m_pAOCtrlSet = nullptr;

    delete m_pRotatorCtrlSet;
    m_pRotatorCtrlSet = nullptr;
}

static wxString HelpLink(wxBookCtrlBase *nb)
{
    const wxString& txt = nb->GetPageText(nb->GetSelection());
    if (txt == _("Global"))
        return _T("Advanced_settings.htm#Global_Tab");
    else if (txt == _("Camera"))
        return _T("Advanced_settings.htm#Camera_Tab");
    else if (txt == _("Guiding"))
        return _T("Advanced_settings.htm#Guiding_Tab");
    else if (txt == _("Algorithms"))
        return _T("Advanced_settings.htm#Algorithms_Tab");
    else if (txt == _("Other Devices"))
        return _T("Advanced_settings.htm#Other_Devices_Tab");
    else
        return wxEmptyString;
}

static void EnableValidators(wxWindow *win)
{
    win->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
    for (auto kid : win->GetChildren())
        EnableValidators(kid);
}

AdvancedDialog::AdvancedDialog(MyFrame *pFrame) :
    wxDialog(pFrame, wxID_ANY, _("Advanced Settings"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX),
    m_tip(nullptr),
    m_tipTimer(nullptr)
{
    /*
     * The advanced dialog is made up of a number of "on the fly" generated panels that configure different things.
     *
     * pTopLevelSizer is a top level Box Sizer in wxVERTICAL mode that contains a wxNotebook object
     * and an unnamed button sizer with OK and CANCEL buttons.
     *
     * Each tab of the notebook contains one or more ConfigDialogPane(s) which are basically vertical
     * sizers to hold a bunch of UI controls.  The UI controls are constructed and managed by ConfigDialogCtrlSet
     * objects.  These reflect the internal organization of the app and generally bind one-to-one with the major internal
     * classes: MyFrame, Guider, Camera, Mount, Scope, AO, Rotator, etc.  The controls created by the ConfigDialogCtrlSet
     * objects are laid out on the various panes by the ConfigDialogPane instances.  So there is a level of indirection here
     * such that the controls can generally be placed anywhere, and the ConfigDialogCtrlSet objects don't care.  This means the
     * overall UI can be optimized for end-users while allowing the underlying controls to reside where they should from an
     * internal architecture perspective.
     * +------------------------------------+------------------------------------+
     * | |   Notebook tabs                                                    |  |
     * | + -------------------------------------------------------------------+  |
     * | |                                                                    |  |
     * | |                                                                    |  |
     * | |           One or more config dialog panes on each tab of the       |  |
     * | |           notebook, possibly nested                                |  |
     * | |                                                                    |  |
     * | |                                                                    |  |
     * | |                                                                    |  |
     * | |                                                                    |  |
     * | |                                                                    |  |
     * + |                                                                    |  |
     * | |                                                                    |  |
     * + +--------------------------------------------------------------------+  |
     * |                              OK and Cancel Buttons                      |
     * +-------------------------------------------------------------------------+
     *
     */

#if defined(__WXOSX__)
    m_pNotebook = new wxChoicebook(this, wxID_ANY);
#else
    m_pNotebook = new wxNotebook(this, wxID_ANY);
#endif
    m_pFrame = pFrame;      // We get called before global var is initialized

    wxSizerFlags sizer_flags = wxSizerFlags(0).Align(wxALIGN_TOP|wxALIGN_CENTER_HORIZONTAL).Border(wxALL,2).Expand();

    // Build all the panels first - these are needed to create the various ConfigCtrlSets
    // Each panel gets a vertical sizer attached to it
    m_pGlobalSettingsPanel = new wxPanel(m_pNotebook);
    wxBoxSizer *pGlobalTabSizer = new wxBoxSizer(wxVERTICAL);
    m_pGlobalSettingsPanel->SetSizer(pGlobalTabSizer);
    m_pNotebook->AddPage(m_pGlobalSettingsPanel, _("Global"), true);
    // Camera pane
    m_pCameraSettingsPanel = new wxPanel(m_pNotebook);
    wxBoxSizer *pCameraTabSizer = new wxBoxSizer(wxVERTICAL);
    m_pCameraSettingsPanel->SetSizer(pCameraTabSizer);
    m_pNotebook->AddPage(m_pCameraSettingsPanel, _("Camera"), false);
    // Guiding pane
    m_pGuiderSettingsPanel = new wxPanel(m_pNotebook);
    wxBoxSizer *pGuidingTabSizer = new wxBoxSizer(wxVERTICAL);
    m_pGuiderSettingsPanel->SetSizer(pGuidingTabSizer);
    m_pNotebook->AddPage(m_pGuiderSettingsPanel, _("Guiding"));
    // Guiding Algorithms pane
    m_pScopeSettingsPanel = new wxPanel(m_pNotebook);
    wxBoxSizer *pScopeTabSizer = new wxBoxSizer(wxVERTICAL);
    m_pScopeSettingsPanel->SetSizer(pScopeTabSizer);
    m_pNotebook->AddPage(m_pScopeSettingsPanel, _("Algorithms"));
    // Devices pane - home for AO and rotator - won't be shown if neither device is used
    m_pDevicesSettingsPanel = new wxPanel(m_pNotebook);
    wxBoxSizer *pDevicesTabSizer = new wxBoxSizer(wxVERTICAL);
    m_pDevicesSettingsPanel->SetSizer(pDevicesTabSizer);
    m_pNotebook->AddPage(m_pDevicesSettingsPanel, _("Other Devices"));

    BuildCtrlSets();        // Populates the m_brainCtrls map with all UI controls

    // Pane contruction now pulls controls from the map and places them where they make sense to a user
    // Populate global pane
    m_pGlobalPane = pFrame->GetConfigDialogPane(m_pGlobalSettingsPanel);
    m_pGlobalPane->LayoutControls(m_brainCtrls);
    pGlobalTabSizer->Add(m_pGlobalPane, sizer_flags);

    // Populate the camera pane
    AddCameraPage();

    // Populate the guiding pane
    m_pGuiderPane = pFrame->pGuider->GetConfigDialogPane(m_pGuiderSettingsPanel);
    m_pGuiderPane->LayoutControls(pFrame->pGuider, m_brainCtrls);
    pGuidingTabSizer->Add(m_pGuiderPane, sizer_flags);

    // Build Mount tab
    AddMountPage();

    // Build AO tab
    AddAoPage();

    // Add page for rotator
    AddRotatorPage();

    // Ok and cancel buttons for the entire dialog box
    wxBoxSizer *pTopLevelSizer = new wxBoxSizer(wxVERTICAL);
    pTopLevelSizer->Add(m_pNotebook, wxSizerFlags(0).Expand().Border(wxALL, 5));
    wxSizer *bsz = CreateButtonSizer(wxOK | wxCANCEL);
    bsz->PrependStretchSpacer();
    wxButton *helpbtn = new wxButton(this, wxID_ANY, _("Help"));
#   include "icons/help22.png.h"
    wxBitmap help_bmp(wxBITMAP_PNG_FROM_DATA(help22));
    helpbtn->SetBitmap(help_bmp, wxLEFT);
    helpbtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& evt) {
            ::pFrame->help->Display(HelpLink(m_pNotebook));
        });
    bsz->Prepend(helpbtn);
    pTopLevelSizer->Add(bsz, wxSizerFlags(0).Expand().Border(wxALL, 5));
    SetSizerAndFit(pTopLevelSizer);

    EnableValidators(this);

    m_rebuildPanels = false;
}

AdvancedDialog::~AdvancedDialog()
{
    CleanupCtrlSets();
    delete m_tipTimer;
}

// Let a client(GearDialog) ask to preload the UI elements - prevents any visible delay when the AdvancedDialog is shown for the first time
void AdvancedDialog::Preload()
{
    if (m_rebuildPanels)
        RebuildPanels();
}

// Internal debugging function to be sure all controls are hosted on a panel somewhere
void AdvancedDialog::ConfirmLayouts()
{
    int orphan_controls = 0;
    for (BrainCtrlIdMap::const_iterator it = m_brainCtrls.begin(); it != m_brainCtrls.end(); ++it)
    {
        BRAIN_CTRL_IDS id = it->first;
        const BrainCtrlInfo& info = it->second;
        if (!info.isPositioned)
        {
            Debug.AddLine(wxString::Format("AdvancedDialog internal error: Controlid %d is not positioned", id));
            ++orphan_controls;
        }
    }
    assert(orphan_controls == 0);
}

// Perform a from-scratch initialization and layout of all the tabs
void AdvancedDialog::RebuildPanels()
{
    CleanupCtrlSets();

    m_pGlobalPane->Clear(true);
    m_pCameraPane->Clear(true);

    m_pCameraSettingsPanel->GetSizer()->Clear(true);
    m_pGuiderPane->Clear(true);

    if (m_pMountPane)
    {
        m_pMountPane->Clear(true);
        m_pScopeSettingsPanel->GetSizer()->Clear(true);
    }
    if (m_pAOPane)
    {
        m_pAOPane->Clear(true);
    }
    if (m_pRotatorPane)
    {
        m_pRotatorPane->Clear(true);
    }
    if (m_pRotatorPane != nullptr || m_pAOPane != nullptr)
        m_pDevicesSettingsPanel->GetSizer()->Clear(true);

    m_brainCtrls.clear();

    BuildCtrlSets();

    m_pGlobalPane->LayoutControls(m_brainCtrls);
    m_pGlobalPane->Layout();

    AddCameraPage();

    m_pGuiderPane->LayoutControls(m_pFrame->pGuider, m_brainCtrls);     // Guider pane doesn't have specific device dependencies
    m_pGuiderPane->Layout();

    AddMountPage();

    AddAoPage();            // Will handle no AO case
    AddRotatorPage();       // Will handle no Rotator case

    if (m_pAOPane == nullptr && m_pRotatorPane == nullptr)        // Dump the Other Devices tab if not needed
    {
        int idx = m_pNotebook->FindPage(m_pDevicesSettingsPanel);
        if (idx != wxNOT_FOUND)
            m_pNotebook->RemovePage(idx);
    }
    else
    {
        int idx = m_pNotebook->FindPage(m_pDevicesSettingsPanel);
        if (idx == wxNOT_FOUND)
            m_pNotebook->AddPage(m_pDevicesSettingsPanel, _("Other Devices"));
    }

    GetSizer()->Layout();
    GetSizer()->Fit(this);
    m_rebuildPanels = false;

    ConfirmLayouts();             // maybe should be under compiletime option
}

// Needed by ConfigDialogCtrlSets to know what parent to use when creating a control
wxWindow *AdvancedDialog::GetTabLocation(BRAIN_CTRL_IDS id)
{
    if (id < AD_GLOBAL_TAB_BOUNDARY)
        return m_pGlobalSettingsPanel;
    else if (id < AD_CAMERA_TAB_BOUNDARY)
        return m_pCameraSettingsPanel;
    else if (id < AD_GUIDER_TAB_BOUNDARY)
        return m_pGuiderSettingsPanel;
    else if (id < AD_MOUNT_TAB_BOUNDARY)
        return m_pScopeSettingsPanel;
    else if (id < AD_DEVICES_TAB_BOUNDARY)
        return m_pDevicesSettingsPanel;
    else
    {
        assert(false);          // Fundamental problem
        return nullptr;
    }
}

void AdvancedDialog::AddCameraPage()
{
    // Even if pCamera is null, the pane hosts other controls
    if (pCamera)
        m_pCameraPane = pCamera->GetConfigDialogPane(m_pCameraSettingsPanel);
    else
        m_pCameraPane = new CameraConfigDialogPane(m_pCameraSettingsPanel, pCamera);
    m_pCameraPane->LayoutControls(pCamera, m_brainCtrls);
    m_pCameraPane->Layout();

    m_pCameraSettingsPanel->GetSizer()->Add(m_pCameraPane);
    m_pScopeSettingsPanel->Layout();
}

void AdvancedDialog::AddMountPage()
{
    const long ID_NOMOUNT = 99999;
    Mount *mount = pMount;

    if (mount)
    {
        wxWindow *noMsgWindow = m_pScopeSettingsPanel->FindWindow(ID_NOMOUNT);
        if (noMsgWindow)
            noMsgWindow->Destroy();
        m_pMountPane = mount->GetConfigDialogPane(m_pScopeSettingsPanel);
        m_pMountPane->LayoutControls(m_pScopeSettingsPanel, m_brainCtrls);
        m_pMountPane->Layout();
    }
    else
    {
        m_pMountPane = new Mount::MountConfigDialogPane(m_pScopeSettingsPanel, _("Mount"), mount);
        wxStaticText *pNoMount = new wxStaticText(m_pScopeSettingsPanel, ID_NOMOUNT, _("No mount specified"));
        m_pMountPane->Add(pNoMount);
    }

    m_pScopeSettingsPanel->GetSizer()->Add(m_pMountPane, wxSizerFlags(0).Border(wxTOP, 10).Expand());
    m_pScopeSettingsPanel->Layout();
}

void AdvancedDialog::AddAoPage()
{
    if (TheAO())
    {
        m_pAOPane = new AOConfigDialogPane(m_pDevicesSettingsPanel, TheAO());
        m_pAOPane->LayoutControls(m_pDevicesSettingsPanel, m_brainCtrls);
        m_pAOPane->Layout();

        m_pDevicesSettingsPanel->GetSizer()->Add(m_pAOPane, wxSizerFlags(0).Border(wxTOP, 10).Expand());
        m_pDevicesSettingsPanel->Layout();
    }
    else
    {
        m_pAOPane = nullptr;
    }
}

void AdvancedDialog::AddRotatorPage()
{
    if (pRotator)
    {
        // We have a rotator selected
        m_pRotatorPane = new RotatorConfigDialogPane(m_pDevicesSettingsPanel);
        m_pRotatorPane->LayoutControls(m_pDevicesSettingsPanel, m_brainCtrls);
        m_pRotatorPane->Layout();

        m_pDevicesSettingsPanel->GetSizer()->Add(m_pRotatorPane, wxSizerFlags(0).Border(wxTOP, 10).Expand());
        m_pDevicesSettingsPanel->Layout();
    }
    else
    {
        m_pRotatorPane = nullptr;
    }
}

// All update options for devices are handled by simply forcing a RebuildPanels before the Advanced Dialog is displayed

void AdvancedDialog::UpdateCameraPage()
{
    m_rebuildPanels = true;
}

void AdvancedDialog::UpdateMountPage()
{
    m_rebuildPanels = true;
}

void AdvancedDialog::UpdateAoPage()
{
    m_rebuildPanels = true;
}

void AdvancedDialog::UpdateRotatorPage()
{
    m_rebuildPanels = true;
}

void AdvancedDialog::LoadValues()
{
    // Late-binding rebuild of all the panels
    if (m_rebuildPanels)
        RebuildPanels();

    // Load all the current params
    m_pGlobalCtrlSet->LoadValues();
    if (m_pCameraCtrlSet)
        m_pCameraCtrlSet->LoadValues();
    if (m_pGuiderCtrlSet)
        m_pGuiderCtrlSet->LoadValues();
    if (m_pRotatorCtrlSet)
        m_pRotatorCtrlSet->LoadValues();

    // Mount sub-classes use a hybrid approach involving both CtrlSets and Panes

    if (TheAO())
    {
        m_pAOCtrlSet->LoadValues();
    }
    if (TheScope())
    {
        m_pScopeCtrlSet->LoadValues();
        m_pMountPane->LoadValues();
    }

    if (s_selectedPage != -1)
        m_pNotebook->ChangeSelection(s_selectedPage);
}

void AdvancedDialog::UnloadValues()
{
    // Unload all the current params
    m_pGlobalCtrlSet->UnloadValues();
    if (m_pCameraCtrlSet)
        m_pCameraCtrlSet->UnloadValues();
    if (m_pGuiderCtrlSet)
        m_pGuiderCtrlSet->UnloadValues();
    if (m_pRotatorCtrlSet)
        m_pRotatorCtrlSet->UnloadValues();

    // Mount sub-classes use a hybrid approach involving both CtrlSets and Panes

    if (TheAO())
    {
        m_pAOCtrlSet->UnloadValues();
    }
    if (TheScope())
    {
        m_pScopeCtrlSet->UnloadValues();
        m_pMountPane->UnloadValues();
    }
}

// Any un-do ops need to be handled at the ConfigDialogPane level
void AdvancedDialog::Undo()
{
    ConfigDialogPane *const panes[] =
        { m_pGlobalPane, m_pGuiderPane, m_pCameraPane, m_pMountPane, m_pAOPane, m_pRotatorPane };

    for (unsigned int i = 0; i < WXSIZEOF(panes); i++)
    {
        ConfigDialogPane *const pane = panes[i];
        if (pane)
            pane->Undo();
    }
}

void AdvancedDialog::EndModal(int retCode)
{
    s_selectedPage = m_pNotebook->GetSelection();
    wxDialog::EndModal(retCode);
}

// Properties and methods needed by step-size calculator dialog
int AdvancedDialog::GetFocalLength()
{
    return m_pGlobalCtrlSet->GetFocalLength();
}

void AdvancedDialog::SetFocalLength(int val)
{
    m_pGlobalCtrlSet->SetFocalLength(val);
}

double AdvancedDialog::GetPixelSize()
{
    return m_pCameraCtrlSet ? m_pCameraCtrlSet->GetPixelSize() : 0.0;
}

void AdvancedDialog::SetPixelSize(double val)
{
    if (m_pCameraCtrlSet)
        m_pCameraCtrlSet->SetPixelSize(val);
}

// Needed to handle reset if the image scale changes on the fly
void AdvancedDialog::ResetGuidingParams()
{
    LoadValues();           // Insure that control values reflect actual device properties and any changes made outside the AD
    m_pMountPane->ResetRAGuidingParams();
    m_pMountPane->ResetDecGuidingParams();
    UnloadValues();
}

// This function only affects UI elements in the various AD panes and is intended for use only by the various ConfigCtrl classes
void AdvancedDialog::MakeImageScaleAdjustments()
{
    double origImageScale = pFrame->GetPixelScale(pCamera->GetCameraPixelSize(), pFrame->GetFocalLength(), pCamera->Binning);       // Profile values
    double newImageScale = pFrame->GetPixelScale(GetPixelSize(), GetFocalLength(), GetBinning());                                   // Current UI ctrl values
    if (fabs((origImageScale - newImageScale) / newImageScale) >= 0.01)
    {
        // Scale the UI cal step size based on image scale ratio - may get refined at start of calibration if actual guiding rates are known
        Debug.Write("Image scale has changed via AD UI - step-size and algo adjustments made\n");
        Debug.Write(wxString::Format("  fl %d => %d, px %.3fu => %.3fu, bin %d => %d\n",
                                     pFrame->GetFocalLength(), GetFocalLength(),
                                     pCamera->GetCameraPixelSize(), GetPixelSize(),
                                     pCamera->Binning, GetBinning()));

        ScopeConfigDialogCtrlSet *cfgset = static_cast<ScopeConfigDialogCtrlSet *>(m_pScopeCtrlSet);
        int oldStepSize = cfgset->GetCalStepSizeCtrlValue();
        int newCalStep = (int)((double)oldStepSize * (newImageScale / origImageScale));
        cfgset->SetCalStepSizeCtrlValue(newCalStep);

        // but let the current guide algos make their own adjustments for stuff like min-moves
        if (m_pMountPane->IsValid())
        {
            m_pMountPane->OnImageScaleChange();
        }
        if (pMount)
        {
            pMount->ClearCalibration();
            if (pMount->IsStepGuider() && pSecondaryMount)
                pSecondaryMount->ClearCalibration();
            Debug.Write("Calibrations cleared because of image scale change\n");
        }
    }
}

int AdvancedDialog::GetBinning()
{
    return m_pCameraCtrlSet ? m_pCameraCtrlSet->GetBinning() : 1;
}

void AdvancedDialog::SetBinning(int binning)
{
    if (m_pCameraCtrlSet)
        m_pCameraCtrlSet->SetBinning(binning);
}

size_t AdvancedDialog::FindPage(wxWindow *ctrl)
{
    wxWindow *a[] = {
        m_pGlobalSettingsPanel,
        m_pCameraSettingsPanel,
        m_pGuiderSettingsPanel,
        m_pScopeSettingsPanel,
        m_pDevicesSettingsPanel,
    };

    for (wxWindow *p = ctrl; p; p = p->GetParent())
        for (size_t i = 0; i < WXSIZEOF(a); i++)
            if (p == a[i])
                return i;

    return wxNOT_FOUND;
}

struct ClearTipTimer : public wxTimer
{
    wxTipWindow **m_w;
    ClearTipTimer(wxTipWindow **w) : m_w(w) { }
    void Notify() override
    {
        if (*m_w)
            (*m_w)->Destroy();
    }
};

void AdvancedDialog::ShowInvalid(wxWindow *ctrl, const wxString& message)
{
    if (m_tip)
        return; // already another error shown

    // focus the notebook page with the error
    size_t sel = FindPage(ctrl);
    if (sel != wxNOT_FOUND)
        m_pNotebook->ChangeSelection(sel);

    ctrl->SetFocus();
    m_tip = new wxTipWindow(ctrl, message, 100, &m_tip);
    m_tip->SetPosition(ctrl->GetScreenPosition() + wxPoint(ctrl->GetSize().GetWidth(), 0));

    if (!m_tipTimer)
        m_tipTimer = new ClearTipTimer(&m_tip);
    enum { TIP_TIMER_MILLISECONDS = 9000 };
    m_tipTimer->StartOnce(TIP_TIMER_MILLISECONDS);
}

bool AdvancedDialog::Validate()
{
    if (m_tip)
    {
        wxWindow *t = m_tip;
        m_tip = nullptr;
        delete t;
    }
    return wxDialog::Validate();
}
