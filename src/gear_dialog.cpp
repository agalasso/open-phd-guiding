/*
 *  gear_dialog.cpp
 *  PHD Guiding
 *
 *  Created by Bret McKee
 *  Copyright (c) 2013 Bret McKee
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
 *    Neither the name of Bret McKee, Dad Dog Development, Ltd. nor the names of its
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
#include "profile_wizard.h"

#include <wx/gbsizer.h>
#include <functional>

// clang-format off
wxBEGIN_EVENT_TABLE(GearDialog, wxDialog)
    EVT_CHOICE(GEAR_PROFILES, GearDialog::OnProfileChoice)
    EVT_BUTTON(GEAR_PROFILE_MANAGE, GearDialog::OnButtonProfileManage)
    EVT_MENU(GEAR_PROFILE_NEW, GearDialog::OnProfileNew)
    EVT_MENU(GEAR_PROFILE_DELETE, GearDialog::OnProfileDelete)
    EVT_MENU(GEAR_PROFILE_RENAME, GearDialog::OnProfileRename)
    EVT_MENU(GEAR_PROFILE_LOAD, GearDialog::OnProfileLoad)
    EVT_MENU(GEAR_PROFILE_SAVE, GearDialog::OnProfileSave)
    EVT_MENU(BUTTON_ADVANCED, GearDialog::OnAdvanced)
    EVT_MENU(GEAR_PROFILE_WIZARD, GearDialog::OnButtonWizard)

    EVT_BUTTON(GEAR_BUTTON_CONNECT_ALL, GearDialog::OnButtonConnectAll)
    EVT_BUTTON(GEAR_BUTTON_DISCONNECT_ALL, GearDialog::OnButtonDisconnectAll)

    EVT_CHOICE(GEAR_CHOICE_CAMERA, GearDialog::OnChoiceCamera)
    EVT_COMMAND_RANGE(MENU_SELECT_CAMERA_BEGIN, MENU_SELECT_CAMERA_END, SELECT_CAMERA_EVENT, GearDialog::OnCommandSelectCamera)
    EVT_BUTTON(GEAR_BUTTON_SETUP_CAMERA, GearDialog::OnButtonSetupCamera)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_CONNECT_CAMERA, GearDialog::OnButtonConnectCamera)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_DISCONNECT_CAMERA, GearDialog::OnButtonDisconnectCamera)

    EVT_CHOICE(GEAR_CHOICE_SCOPE, GearDialog::OnChoiceScope)
    EVT_BUTTON(GEAR_BUTTON_SETUP_SCOPE, GearDialog::OnButtonSetupScope)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_CONNECT_SCOPE, GearDialog::OnButtonConnectScope)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_DISCONNECT_SCOPE, GearDialog::OnButtonDisconnectScope)

    EVT_CHOICE(GEAR_CHOICE_AUXSCOPE, GearDialog::OnChoiceAuxScope)
    EVT_BUTTON(GEAR_BUTTON_SETUP_AUXSCOPE, GearDialog::OnButtonSetupAuxScope)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_CONNECT_AUXSCOPE, GearDialog::OnButtonConnectAuxScope)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_DISCONNECT_AUXSCOPE, GearDialog::OnButtonDisconnectAuxScope)

    EVT_BUTTON(GEAR_BUTTON_MORE, GearDialog::OnButtonMore)

    EVT_CHOICE(GEAR_CHOICE_STEPGUIDER, GearDialog::OnChoiceStepGuider)
    EVT_BUTTON(GEAR_BUTTON_SETUP_STEPGUIDER, GearDialog::OnButtonSetupStepGuider)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_CONNECT_STEPGUIDER, GearDialog::OnButtonConnectStepGuider)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_DISCONNECT_STEPGUIDER, GearDialog::OnButtonDisconnectStepGuider)

    EVT_CHOICE(GEAR_CHOICE_ROTATOR, GearDialog::OnChoiceRotator)
    EVT_BUTTON(GEAR_BUTTON_SETUP_ROTATOR, GearDialog::OnButtonSetupRotator)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_CONNECT_ROTATOR, GearDialog::OnButtonConnectRotator)
    EVT_TOGGLEBUTTON(GEAR_BUTTON_DISCONNECT_ROTATOR, GearDialog::OnButtonDisconnectRotator)

    EVT_CHAR_HOOK(GearDialog::OnChar)
wxEND_EVENT_TABLE();
// clang-format on

/*
 * The Gear Dialog allows the user to select and connect to their hardware.
 *
 * The dialog looks something like this:
 *
 * +--------------------------------------------------------------------------+
 * |                                                                          |
 * |                               Help text                                  |
 * |                                                                          |
 * +--------------------------------------------------------------------------+
 * |                                   |    +------------------------+        |
 * |  Camera Selection                 |    |Camera Connection Button|        |
 * |                                   |    +------------------------+        |
 * +--------------------------------------------------------------------------+
 * |                                   |    +-----------------------+         |
 * |  Mount Selection                  |    |Mount Connection Button|         |
 * |                                   |    +-----------------------+         |
 * +--------------------------------------------------------------------------+
 * +--------------------------------------------------------------------------+
 * |                                   |    +--------------------------+      |
 * |  Aux Mount Selection              |    |AuxMount Connection Button|      |
 * |                                   |    +--------------------------+      |
 * +--------------------------------------------------------------------------+
 * |                                   |    +---------------------+           |
 * |  AO Selection                     |    | AO Connection Button|           |
 * |                                   |    +---------------------+           |
 * +--------------------------------------------------------------------------+
 * |             +-------------------+   +-------------------+                |
 * |             |    Connect All    |   |  Disconnect All   |                |
 * |             +-------------------+   +-------------------+                |
 * +--------------------------------------------------------------------------+
 */
GearDialog::GearDialog(wxWindow *pParent)
    : wxDialog(pParent, wxID_ANY, _("Connect Equipment"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX),
      m_cameraUpdated(false), m_mountUpdated(false), m_stepGuiderUpdated(false), m_rotatorUpdated(false),
      m_showDarksDialog(false), m_camWarningIssued(false), m_camChanged(false), m_imageScaleRatio(1.0), m_flushConfig(false)
{
    m_pCamera = nullptr;
    m_pScope = nullptr;
    m_pAuxScope = nullptr;
    m_pStepGuider = nullptr;
    m_pRotator = nullptr;

    m_pCameras = nullptr;
    m_pScopes = nullptr;
    m_pAuxScopes = nullptr;
    m_pStepGuiders = nullptr;
    m_pRotators = nullptr;

    m_pConnectCameraButton = nullptr;
    m_pConnectScopeButton = nullptr;
    m_pConnectAuxScopeButton = nullptr;
    m_pConnectStepGuiderButton = nullptr;
    m_pConnectRotatorButton = nullptr;

    m_pConnectAllButton = nullptr;
    m_pDisconnectAllButton = nullptr;

    m_profiles = nullptr;
    m_btnProfileManage = nullptr;
    m_menuProfileManage = nullptr;

    Initialize();

    Centre(wxBOTH);
}

GearDialog::~GearDialog()
{
    delete m_pCamera;
    delete m_pScope;
    if (m_pAuxScope != m_pScope)
        delete m_pAuxScope;

    delete m_pStepGuider;
    delete m_pRotator;

    // prevent double frees
    pCamera = nullptr;
    pMount = nullptr;
    pSecondaryMount = nullptr;
    pPointingSource = nullptr;
    pRotator = nullptr;

    delete m_menuProfileManage;
}

static wxToggleButton *MakeConnectBtn(wxWindow *parent, wxWindowID id)
{
#include "icons/connected.png.h"
    wxBitmap connected_bmp(wxBITMAP_PNG_FROM_DATA(connected));
#include "icons/disconnected.png.h"
    wxBitmap disconnected_bmp(wxBITMAP_PNG_FROM_DATA(disconnected));

    wxToggleButton *btn = new wxToggleButton(parent, id, _("Disconnect"), wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);

    btn->SetBitmap(disconnected_bmp);
    btn->SetBitmapPressed(connected_bmp);

    // Layout the button now with the wider Disconnected label
    btn->Layout();
    wxSize sz(btn->GetSize());
#ifdef __WXGTK__
    // WXGTK bug? button layout excludes bitmap size?!
    sz.SetWidth(sz.GetWidth() + connected_bmp.GetWidth());
#endif
    btn->SetMinSize(sz);

    return btn;
}

void GearDialog::Initialize()
{
    wxSizerFlags sizerFlags = wxSizerFlags().Align(wxALIGN_CENTER).Border(wxALL, 2).Expand();
    wxSizerFlags sizerTextFlags = wxSizerFlags().Align(wxALIGN_CENTER).Border(wxALL, 2).Expand();
    wxSizerFlags sizerLabelFlags = wxSizerFlags().Align(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL).Border(wxALL, 2);
    wxSizerFlags sizerButtonFlags = wxSizerFlags().Align(wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL).Border(wxALL, 2).Expand();

    wxBoxSizer *pTopLevelSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *profilesSizer = new wxBoxSizer(wxHORIZONTAL);
    profilesSizer->Add(new wxStaticText(this, wxID_ANY, _("Equipment profile")), sizerLabelFlags);
    m_profiles = new wxChoice(this, GEAR_PROFILES, wxDefaultPosition, wxDefaultSize, pConfig->ProfileNames());
    m_profiles->SetToolTip(_("Select the Equipment Profile you would like to use. PHD stores all of your settings and "
                             "equipment selections in an Equipment Profile. "
                             "You can create multiple profiles and switch back and forth between them."));
    m_profiles->SetStringSelection(pConfig->GetCurrentProfile());
    profilesSizer->Add(m_profiles, sizerButtonFlags);

    m_menuProfileManage = new wxMenu();
    m_menuProfileManage->Append(GEAR_PROFILE_WIZARD, _("New using Wizard..."),
                                _("Run the first-light wizard to create a new profile"));
    m_menuProfileManage->Append(GEAR_PROFILE_NEW, _("New"), _("Create a new profile, optionally copying from another profile"));
    m_menuProfileManage->Append(GEAR_PROFILE_DELETE, _("Delete"), _("Delete the selected profile"));
    m_menuProfileManage->Append(GEAR_PROFILE_RENAME, _("Rename"), _("Rename the selected profile"));
    m_menuProfileManage->Append(GEAR_PROFILE_LOAD, _("Import..."), _("Load a profile from a file"));
    m_menuProfileManage->Append(GEAR_PROFILE_SAVE, _("Export..."), _("Save the selected profile to a file"));
    m_menuProfileManage->Append(BUTTON_ADVANCED, _("Settings..."), _("Open the advanced settings dialog"));

    m_btnProfileManage = new OptionsButton(this, GEAR_PROFILE_MANAGE, _("Manage Profiles"));
    m_btnProfileManage->SetToolTip(_("Create a new Equipment Profile, or delete or rename the selected Equipment Profile"));
    profilesSizer->Add(m_btnProfileManage, sizerButtonFlags);

    pTopLevelSizer->Add(profilesSizer, wxSizerFlags().Align(wxALIGN_CENTER).Border(wxALL, 2));
    pTopLevelSizer->AddSpacer(10);

    // text at the top.  I tried (really really hard) to get it to resize/Wrap()
    // with the rest of the sizer, but it just didn't want to work, and I needed
    // to get the rest of the dialog working.
    wxStaticText *pText = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                           wxALIGN_CENTER | wxALIGN_CENTER_VERTICAL);
    int width, height;
    pText->SetLabel(
        _("Select your equipment below and click Connect All to connect, or click Disconnect All to disconnect. You can also "
          "connect or disconnect individual equipment items by clicking the button next to the item."));
    pText->GetTextExtent(_T("MMMMMMMMMM"), &width, &height);
    pText->Wrap(4 * width);
    pTopLevelSizer->Add(pText, sizerTextFlags.Align(wxALIGN_CENTER));

    // The Gear grid in the middle of the screen
    m_gearSizer = new wxGridBagSizer();
    pTopLevelSizer->Add(m_gearSizer, wxSizerFlags().Align(wxALIGN_CENTER).Border(wxALL, 2));

    // Camera
    m_gearSizer->Add(new wxStaticText(this, wxID_ANY, _("Camera"), wxDefaultPosition, wxDefaultSize), wxGBPosition(0, 0),
                     wxGBSpan(1, 1), wxALL | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);
    m_pCameras = new wxChoice(this, GEAR_CHOICE_CAMERA, wxDefaultPosition, wxDefaultSize, 0, nullptr, 0, wxDefaultValidator,
                              _("Camera"));
    m_gearSizer->Add(m_pCameras, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);

#include "icons/setup.png.h"
    wxBitmap setup_bmp(wxBITMAP_PNG_FROM_DATA(setup));

    m_selectCameraButton = new CameraSelectorButton(this, GEAR_BUTTON_SELECT_CAMERA);
    m_gearSizer->Add(m_selectCameraButton, wxGBPosition(0, 2), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pSetupCameraButton = new wxBitmapButton(this, GEAR_BUTTON_SETUP_CAMERA, setup_bmp);
    m_pSetupCameraButton->SetToolTip(_("Camera Setup"));
    m_gearSizer->Add(m_pSetupCameraButton, wxGBPosition(0, 3), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pConnectCameraButton = MakeConnectBtn(this, GEAR_BUTTON_CONNECT_CAMERA);
    m_gearSizer->Add(m_pConnectCameraButton, wxGBPosition(0, 4), wxGBSpan(1, 1),
                     wxBOTTOM | wxTOP | wxRIGHT | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);

    // mount
    m_gearSizer->Add(new wxStaticText(this, wxID_ANY, _("Mount"), wxDefaultPosition, wxDefaultSize), wxGBPosition(1, 0),
                     wxGBSpan(1, 1), wxALL | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);
    m_pScopes =
        new wxChoice(this, GEAR_CHOICE_SCOPE, wxDefaultPosition, wxDefaultSize, 0, nullptr, 0, wxDefaultValidator, _("Mount"));
    m_pScopes->SetToolTip(
        _("Specify how guide commands will be sent to the mount - via an ASCOM or INDI driver, directly from the camera or AO, "
          "or via one of the GPxxx devices. An ASCOM connection is recommended."));
    m_gearSizer->Add(m_pScopes, wxGBPosition(1, 1), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pSetupScopeButton = new wxBitmapButton(this, GEAR_BUTTON_SETUP_SCOPE, setup_bmp);
    m_pSetupScopeButton->SetToolTip(_("Mount Setup"));
    m_gearSizer->Add(m_pSetupScopeButton, wxGBPosition(1, 3), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pConnectScopeButton = MakeConnectBtn(this, GEAR_BUTTON_CONNECT_SCOPE);
    m_gearSizer->Add(m_pConnectScopeButton, wxGBPosition(1, 4), wxGBSpan(1, 1),
                     wxBOTTOM | wxTOP | wxRIGHT | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);

    // aux mount - used for position/state information when not guiding through ASCOM interface
    m_gearSizer->Add(new wxStaticText(this, wxID_ANY, _("Aux Mount"), wxDefaultPosition, wxDefaultSize), wxGBPosition(2, 0),
                     wxGBSpan(1, 1), wxALL | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);
    m_pAuxScopes = new wxChoice(this, GEAR_CHOICE_AUXSCOPE, wxDefaultPosition, wxDefaultSize, 0, nullptr, 0, wxDefaultValidator,
                                _("Aux Mount"));

#if defined(GUIDE_ASCOM) || defined(GUIDE_INDI)
# ifdef GUIDE_ASCOM
    wxString driverName = _T("ASCOM");
# else
    wxString driverName = _T("INDI");
# endif
    m_pAuxScopes->SetToolTip(
        wxString::Format(_("If you are using a guide port (On-camera or GPXXX) interface  for guiding, you can also use an "
                           "'aux' connection to your %s-compatible mount. This will "
                           "be used to make automatic calibration adjustments based on declination and side-of-pier.  If you "
                           "have already selected an %s driver for your 'mount', the 'aux' mount "
                           "parameter will not be used."),
                         driverName, driverName));
#endif // ASCOM or INDI

    m_gearSizer->Add(m_pAuxScopes, wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pSetupAuxScopeButton = new wxBitmapButton(this, GEAR_BUTTON_SETUP_AUXSCOPE, setup_bmp);
    m_pSetupAuxScopeButton->SetToolTip(_("Aux Mount Setup"));
    m_gearSizer->Add(m_pSetupAuxScopeButton, wxGBPosition(2, 3), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pConnectAuxScopeButton = MakeConnectBtn(this, GEAR_BUTTON_CONNECT_AUXSCOPE);
    m_gearSizer->Add(m_pConnectAuxScopeButton, wxGBPosition(2, 4), wxGBSpan(1, 1),
                     wxBOTTOM | wxTOP | wxRIGHT | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);

    m_moreButton = new wxButton(this, GEAR_BUTTON_MORE, wxEmptyString);
    m_gearSizer->Add(m_moreButton, wxGBPosition(3, 0), wxGBSpan(1, 4), wxALL | /*wxALIGN_CENTER*/ wxALIGN_LEFT, 5);

    // ao
    m_gearSizer->Add(new wxStaticText(this, wxID_ANY, _("AO"), wxDefaultPosition, wxDefaultSize), wxGBPosition(4, 0),
                     wxGBSpan(1, 1), wxALL | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);
    m_pStepGuiders = new wxChoice(this, GEAR_CHOICE_STEPGUIDER, wxDefaultPosition, wxDefaultSize, 0, nullptr, 0,
                                  wxDefaultValidator, _("AO"));
    m_gearSizer->Add(m_pStepGuiders, wxGBPosition(4, 1), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pSetupStepGuiderButton = new wxBitmapButton(this, GEAR_BUTTON_SETUP_STEPGUIDER, setup_bmp);
    m_pSetupStepGuiderButton->SetToolTip(_("AO Setup"));
    m_gearSizer->Add(m_pSetupStepGuiderButton, wxGBPosition(4, 3), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL,
                     5);
    m_pConnectStepGuiderButton = MakeConnectBtn(this, GEAR_BUTTON_CONNECT_STEPGUIDER);
    m_gearSizer->Add(m_pConnectStepGuiderButton, wxGBPosition(4, 4), wxGBSpan(1, 1),
                     wxBOTTOM | wxTOP | wxRIGHT | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);

    // rotator
    m_gearSizer->Add(new wxStaticText(this, wxID_ANY, _("Rotator"), wxDefaultPosition, wxDefaultSize), wxGBPosition(5, 0),
                     wxGBSpan(1, 1), wxALL | wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);
    m_pRotators = new wxChoice(this, GEAR_CHOICE_ROTATOR, wxDefaultPosition, wxDefaultSize, 0, nullptr, 0, wxDefaultValidator,
                               _("Rotator"));
    m_gearSizer->Add(m_pRotators, wxGBPosition(5, 1), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pSetupRotatorButton = new wxBitmapButton(this, GEAR_BUTTON_SETUP_ROTATOR, setup_bmp);
    m_pSetupRotatorButton->SetToolTip(_("Rotator Setup"));
    m_gearSizer->Add(m_pSetupRotatorButton, wxGBPosition(5, 3), wxGBSpan(1, 1), wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);
    m_pConnectRotatorButton = MakeConnectBtn(this, GEAR_BUTTON_CONNECT_ROTATOR);
    m_gearSizer->Add(m_pConnectRotatorButton, wxGBPosition(5, 4), wxGBSpan(1, 1),
                     wxBOTTOM | wxTOP | wxRIGHT | wxEXPAND | wxALIGN_CENTER_VERTICAL, 5);

    // Setup the bottom row of buttons

    wxBoxSizer *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    m_pConnectAllButton = new wxButton(this, GEAR_BUTTON_CONNECT_ALL, _("Connect All"));
    m_pConnectAllButton->SetToolTip(_("Connect all equipment and close the equipment selection window"));
    pButtonSizer->Add(m_pConnectAllButton, sizerFlags);

    m_pDisconnectAllButton = new wxButton(this, GEAR_BUTTON_DISCONNECT_ALL, _("Disconnect All"));
    m_pDisconnectAllButton->SetToolTip(_("Disconnect all equipment"));
    pButtonSizer->Add(m_pDisconnectAllButton, sizerFlags);

    wxButton *closeBtn = new wxButton(this, wxID_CANCEL, _("Close"));
    pButtonSizer->Add(closeBtn, sizerFlags);

    pTopLevelSizer->Add(pButtonSizer, wxSizerFlags().Align(wxALIGN_TOP | wxALIGN_CENTER_HORIZONTAL).Border(wxALL, 2));

    // preselect the choices
    LoadGearChoices();

    m_showMoreGear = m_pStepGuider || m_pRotator;
    ShowMoreGear();

    // fit everything with the sizers
    SetSizerAndFit(pTopLevelSizer);

    UpdateAdvancedDialog(false);
}

static bool DeviceSelectionMatches(const wxString& val, const wxString& item)
{
    if (val.Contains("INDI"))
        return item.Contains("INDI");
    return val == item;
}

static void SetMatchingSelection(wxChoice *ctrl, const wxString& val)
{
    if (ctrl->SetStringSelection(val))
        return;

    // special case for INDI:  INDI device selection will be "INDI Camera" or "INDI Camera [indi driver name]"
    // allow either one to match
    for (int i = 0; i < ctrl->GetCount(); ++i)
    {
        const wxString& item = ctrl->GetString(i);
        if (DeviceSelectionMatches(val, item))
        {
            ctrl->SetSelection(i);
            return;
        }
    }
}

static void LoadChoices(wxChoice *ctl, const wxArrayString& ary)
{
    ctl->Freeze();
    ctl->Clear();
    ctl->Append(ary);
    ctl->Thaw();
}

static void LoadCameras(wxChoice *cameras)
{
    LoadChoices(cameras, GuideCamera::GuideCameraList());
}

static void LoadMounts(wxChoice *mounts)
{
    LoadChoices(mounts, Scope::MountList());
}

static void LoadAuxMounts(wxChoice *auxMounts)
{
    LoadChoices(auxMounts, Scope::AuxMountList());
}

static void LoadAOs(wxChoice *aos)
{
    LoadChoices(aos, StepGuider::AOList());
}

static void LoadRotators(wxChoice *rots)
{
    LoadChoices(rots, Rotator::RotatorList());
}

#if 1 // TODO: remove after a couple releases - added 2019/02/19
static wxString NewAoName(const wxString& oldname)
{
    // we renamed the AOs when we added the INDI SBIG AO
    // temporary code to convert the old names to the new
    // this should be removed after a release or two
    if (oldname == _T("sxAO"))
        return _T("SX AO");
    if (oldname == _T("INDI sxAO"))
        return _T("SX AO (INDI)");
    return oldname;
}
#endif

void GearDialog::LoadGearChoices()
{
    LoadCameras(m_pCameras);
    LoadMounts(m_pScopes);
    LoadAuxMounts(m_pAuxScopes);
    LoadAOs(m_pStepGuiders);
    LoadRotators(m_pRotators);

    wxCommandEvent dummyEvent;
    m_lastCamera = pConfig->Profile.GetString("/camera/LastMenuChoice", _("None"));
    SetMatchingSelection(m_pCameras, m_lastCamera);
    OnChoiceCamera(dummyEvent);

    wxString lastScope = pConfig->Profile.GetString("/scope/LastMenuChoice", _("None"));
    SetMatchingSelection(m_pScopes, lastScope);
    OnChoiceScope(dummyEvent);

    wxString lastAuxScope = pConfig->Profile.GetString("/scope/LastAuxMenuChoice", _("None"));
    SetMatchingSelection(m_pAuxScopes, lastAuxScope);
    OnChoiceAuxScope(dummyEvent);

    wxString lastStepGuider = pConfig->Profile.GetString("/stepguider/LastMenuChoice", _("None"));
    SetMatchingSelection(m_pStepGuiders, NewAoName(lastStepGuider));
    OnChoiceStepGuider(dummyEvent);

    wxString lastRotator = pConfig->Profile.GetString("/rotator/LastMenuChoice", _("None"));
    SetMatchingSelection(m_pRotators, lastRotator);
    OnChoiceRotator(dummyEvent);
}

int GearDialog::ShowGearDialog(bool autoConnect)
{
    int ret = wxID_OK;
    int callSuper = true;

    assert(pCamera == nullptr || pCamera == m_pCamera);

    m_camChanged = false;
    m_camWarningIssued = false;

    if (m_pStepGuider)
    {
        assert(pMount == nullptr || pMount == m_pStepGuider);
        assert(pSecondaryMount == nullptr || pSecondaryMount == m_pScope);
    }
    else
    {
        assert(pMount == nullptr || pMount == m_pScope);
        assert(pSecondaryMount == nullptr);
    }

    if (autoConnect)
    {
        Debug.Write("gear_dialog: connect all (auto) calls OnButtonConnectAll\n");

        wxCommandEvent dummyEvent;
        OnButtonConnectAll(dummyEvent);

        if (m_pCamera && m_pCamera->Connected && (!m_pScope || m_pScope->IsConnected()) &&
            (!m_pAuxScope || m_pAuxScope->IsConnected()) && (!m_pStepGuider || m_pStepGuider->IsConnected()) &&
            (!m_pRotator || m_pRotator->IsConnected()))
        {
            callSuper = false;
        }
    }

    if (callSuper)
    {
        m_imageScaleRatio = 1.0;
        UpdateButtonState();

        GetSizer()->Fit(this);
        CenterOnParent();

        wxWindow *top = wxGetApp().GetTopWindow();
        wxGetApp().SetTopWindow(this);
        ret = wxDialog::ShowModal();
        wxGetApp().SetTopWindow(top);
    }
    else
    {
        EndModal(ret);
    }

    if (m_flushConfig)
    {
        pConfig->Flush();
        m_flushConfig = false;
    }

    return ret;
}

void GearDialog::EndModal(int retCode)
{
    assert(pCamera == m_pCamera);

    if (m_pStepGuider)
    {
        assert(pMount == m_pStepGuider);
        assert(pSecondaryMount == m_pScope);
    }
    else
    {
        assert(pMount == m_pScope);
        assert(pSecondaryMount == nullptr);
    }

    pFrame->UpdateButtonsStatus();
    pFrame->pGraphLog->UpdateControls();
    pFrame->pTarget->UpdateControls();

    if (pFrame->GetAutoLoadCalibration() && !m_camChanged) // ok to reload calibration
    {
        if (pMount && pMount->IsConnected() && (!pSecondaryMount || pSecondaryMount->IsConnected()))
        {
            Debug.Write("Auto-loading calibration data\n");
            pFrame->LoadCalibration();
        }
    }
    else
    {
        // Force a recalibration if the camera has changed
        if (m_camChanged)
        {
            Debug.Write("Clearing calibration data because camera was changed\n");

            if (m_pStepGuider)
            {
                if (m_pStepGuider->IsConnected())
                    m_pStepGuider->ClearCalibration();
                if (pSecondaryMount && pSecondaryMount->IsConnected())
                    pSecondaryMount->ClearCalibration();
            }
            else
            {
                if (pMount && pMount->IsConnected())
                    pMount->ClearCalibration();
            }
        }
    }

    wxDialog::EndModal(retCode);

    UpdateAdvancedDialog(true);

    if (fabs(m_imageScaleRatio - 1.0) >= 0.01)
    {
        Debug.Write("GearDialog::EndModal: imageScaleRatio changed\n");
        pFrame->HandleImageScaleChange();
    }

    if (m_showDarksDialog)
    {
        m_showDarksDialog = false;
        if (pCamera && pCamera->Connected)
        {
            wxCommandEvent dummy;
            pFrame->OnDark(dummy);
        }
    }
}

void GearDialog::UpdateCameraButtonState()
{
    // Now set up the buttons to match our current state
    if (!m_pCamera)
    {
        m_selectCameraButton->Enable(false);
        m_pSetupCameraButton->Enable(false);
        m_pConnectCameraButton->Enable(false);
        m_pConnectCameraButton->SetLabel(_("Connect"));
        m_pConnectCameraButton->SetValue(false);
        m_pConnectCameraButton->SetToolTip(_("Connect to camera"));
        m_pConnectCameraButton->SetId(GEAR_BUTTON_CONNECT_CAMERA);
        m_pCameras->Enable(true);
    }
    else
    {
        bool enablePropDlg = ((m_pCamera->PropertyDialogType & PROPDLG_WHEN_CONNECTED) != 0 && m_pCamera->Connected) ||
            ((m_pCamera->PropertyDialogType & PROPDLG_WHEN_DISCONNECTED) != 0 && !m_pCamera->Connected);
        m_pSetupCameraButton->Enable(enablePropDlg);

        m_pConnectCameraButton->Enable(true);

        if (m_pCamera->Connected)
        {
            m_selectCameraButton->Enable(false);
            m_pConnectCameraButton->SetLabel(_("Disconnect"));
            m_pConnectCameraButton->SetValue(true);
            m_pConnectCameraButton->SetToolTip(_("Disconnect from camera"));
            m_pConnectCameraButton->SetId(GEAR_BUTTON_DISCONNECT_CAMERA);
            m_pCameras->Enable(false);
        }
        else
        {
            m_selectCameraButton->Enable(m_pCamera->CanSelectCamera());
            m_pConnectCameraButton->SetLabel(_("Connect"));
            m_pConnectCameraButton->SetValue(false);
            m_pConnectCameraButton->SetToolTip(_("Connect to camera"));
            m_pConnectCameraButton->SetId(GEAR_BUTTON_CONNECT_CAMERA);
            m_pCameras->Enable(true);
        }
    }
}

void GearDialog::UpdateScopeButtonState()
{
    // Now set up the buttons to match our current state
    if (!m_pScope)
    {
        m_pSetupScopeButton->Enable(false);
        m_pConnectScopeButton->Enable(false);
        m_pConnectScopeButton->SetLabel(_("Connect"));
        m_pConnectScopeButton->SetValue(false);
        m_pConnectScopeButton->SetToolTip(_("Connect to mount"));
        m_pConnectScopeButton->SetId(GEAR_BUTTON_CONNECT_SCOPE);
        m_pScopes->Enable(true);
    }
    else
    {
        m_pSetupScopeButton->Enable(m_pScope->HasSetupDialog());
        m_pConnectScopeButton->Enable(true);

        if (m_pScope->IsConnected())
        {
            m_pConnectScopeButton->SetLabel(_("Disconnect"));
            m_pConnectScopeButton->SetValue(true);
            m_pConnectScopeButton->SetToolTip(_("Disconnect from mount"));
            m_pConnectScopeButton->SetId(GEAR_BUTTON_DISCONNECT_SCOPE);
            m_pScopes->Enable(false);
        }
        else
        {
            m_pConnectScopeButton->SetLabel(_("Connect"));
            m_pConnectScopeButton->SetValue(false);
            m_pConnectScopeButton->SetToolTip(_("Connect to mount"));
            m_pConnectScopeButton->SetId(GEAR_BUTTON_CONNECT_SCOPE);
            m_pScopes->Enable(true);

            if (m_pScope->RequiresCamera() && (!m_pCamera || !m_pCamera->ST4HasGuideOutput() || !m_pCamera->Connected))
            {
                m_pConnectScopeButton->Enable(false);
            }
            else if (m_pScope->RequiresStepGuider() &&
                     (!m_pStepGuider || !m_pStepGuider->ST4HasGuideOutput() || !m_pStepGuider->IsConnected()))
            {
                m_pConnectScopeButton->Enable(false);
            }
            else
            {
                m_pConnectScopeButton->Enable(true);
            }
        }
    }
}

void GearDialog::UpdateAuxScopeButtonState()
{
    // Now set up the buttons to match our current state
    if (m_pScope && m_pScope->CanReportPosition())
    {
        int noneInx = m_pAuxScopes->FindString(_("None")); // Should always be first in list
        m_pAuxScopes->SetSelection(noneInx);
        m_pAuxScopes->Enable(false);
        m_pSetupAuxScopeButton->Enable(false);
        m_pConnectAuxScopeButton->Enable(false);

        if (m_pAuxScope && m_pAuxScope != m_pScope)
            delete m_pAuxScope;
        m_pAuxScope = nullptr;
    }
    else
    {
        m_pAuxScopes->Enable(true);
        if (!m_pAuxScope)
        {
            m_pSetupAuxScopeButton->Enable(false);
            m_pConnectAuxScopeButton->Enable(false);
            m_pConnectAuxScopeButton->SetLabel(_("Connect"));
            m_pConnectAuxScopeButton->SetValue(false);
            m_pConnectAuxScopeButton->SetToolTip(_("Connect to aux mount"));
            m_pConnectAuxScopeButton->SetId(GEAR_BUTTON_CONNECT_AUXSCOPE);
            m_pAuxScopes->Enable(true);
        }
        else
        {
            m_pSetupAuxScopeButton->Enable(m_pAuxScope->HasSetupDialog());
            m_pConnectAuxScopeButton->Enable(true);

            if (m_pAuxScope->IsConnected())
            {
                m_pConnectAuxScopeButton->SetLabel(_("Disconnect"));
                m_pConnectAuxScopeButton->SetValue(true);
                m_pConnectAuxScopeButton->SetToolTip(_("Disconnect from aux mount"));
                m_pConnectAuxScopeButton->SetId(GEAR_BUTTON_DISCONNECT_AUXSCOPE);
                m_pAuxScopes->Enable(false);
            }
            else
            {
                m_pConnectAuxScopeButton->SetLabel(_("Connect"));
                m_pConnectAuxScopeButton->SetValue(false);
                m_pConnectAuxScopeButton->SetToolTip(_("Connect to aux mount"));
                m_pConnectAuxScopeButton->SetId(GEAR_BUTTON_CONNECT_AUXSCOPE);
                m_pAuxScopes->Enable(true);
            }
        }
    }
}

void GearDialog::UpdateStepGuiderButtonState()
{
    // Now set up the buttons to match our current state
    if (!m_pStepGuider)
    {
        m_pSetupStepGuiderButton->Enable(false);
        m_pConnectStepGuiderButton->Enable(false);
        m_pConnectStepGuiderButton->SetLabel(_("Connect"));
        m_pConnectStepGuiderButton->SetValue(false);
        m_pConnectStepGuiderButton->SetToolTip(_("Connect to AO"));
        m_pConnectStepGuiderButton->SetId(GEAR_BUTTON_CONNECT_STEPGUIDER);
        m_pStepGuiders->Enable(true);
    }
    else
    {
        m_pConnectStepGuiderButton->Enable(true);

        if (m_pStepGuider->IsConnected())
        {
            m_pConnectStepGuiderButton->SetLabel(_("Disconnect"));
            m_pConnectStepGuiderButton->SetValue(true);
            m_pConnectStepGuiderButton->SetToolTip(_("Disconnect from AO"));
            m_pConnectStepGuiderButton->SetId(GEAR_BUTTON_DISCONNECT_STEPGUIDER);
            m_pStepGuiders->Enable(false);
            m_pSetupStepGuiderButton->Enable(false);
        }
        else
        {
            m_pConnectStepGuiderButton->SetLabel(_("Connect"));
            m_pConnectStepGuiderButton->SetValue(false);
            m_pConnectStepGuiderButton->SetToolTip(_("Connect to AO"));
            m_pConnectStepGuiderButton->SetId(GEAR_BUTTON_CONNECT_STEPGUIDER);
            m_pStepGuiders->Enable(true);
            m_pSetupStepGuiderButton->Enable(true);
        }
    }
}

void GearDialog::UpdateRotatorButtonState()
{
    // Now set up the buttons to match our current state
    if (!m_pRotator)
    {
        m_pSetupRotatorButton->Enable(false);
        m_pConnectRotatorButton->Enable(false);
        m_pConnectRotatorButton->SetLabel(_("Connect"));
        m_pConnectRotatorButton->SetValue(false);
        m_pConnectRotatorButton->SetToolTip(_("Connect to Rotator"));
        m_pConnectRotatorButton->SetId(GEAR_BUTTON_CONNECT_ROTATOR);
        m_pRotators->Enable(true);
    }
    else
    {
        m_pConnectRotatorButton->Enable(true);

        if (m_pRotator->IsConnected())
        {
            m_pConnectRotatorButton->SetLabel(_("Disconnect"));
            m_pConnectRotatorButton->SetValue(true);
            m_pConnectRotatorButton->SetToolTip(_("Disconnect from Rotator"));
            m_pConnectRotatorButton->SetId(GEAR_BUTTON_DISCONNECT_ROTATOR);
            m_pRotators->Enable(false);
            m_pSetupRotatorButton->Enable(false);
        }
        else
        {
            m_pConnectRotatorButton->SetLabel(_("Connect"));
            m_pConnectRotatorButton->SetValue(false);
            m_pConnectRotatorButton->SetToolTip(_("Connect to Rotator"));
            m_pConnectRotatorButton->SetId(GEAR_BUTTON_CONNECT_ROTATOR);
            m_pRotators->Enable(true);
            m_pSetupRotatorButton->Enable(true);
        }
    }
}

void GearDialog::UpdateConnectAllButtonState()
{
    if ((m_pCamera && !m_pCamera->Connected) || (m_pScope && !m_pScope->IsConnected()) ||
        (m_pAuxScope && !m_pAuxScope->IsConnected()) || (m_pStepGuider && !m_pStepGuider->IsConnected()) ||
        (m_pRotator && !m_pRotator->IsConnected()))
    {
        m_pConnectAllButton->Enable(true);
    }
    else
    {
        m_pConnectAllButton->Enable(false);
    }
}

void GearDialog::UpdateDisconnectAllButtonState()
{
    if ((m_pCamera && m_pCamera->Connected) || (m_pScope && m_pScope->IsConnected()) ||
        (m_pAuxScope && m_pAuxScope->IsConnected()) || (m_pStepGuider && m_pStepGuider->IsConnected()) ||
        (m_pRotator && m_pRotator->IsConnected()))
    {
        m_pDisconnectAllButton->Enable(true);
        m_profiles->Enable(false);
        m_btnProfileManage->Enable(false);
    }
    else
    {
        m_pDisconnectAllButton->Enable(false);
        // allow profiles to be selected / modified only when everything is disconnected.
        m_profiles->Enable(true);
        m_btnProfileManage->Enable(true);
    }
}

void GearDialog::UpdateButtonState()
{
    UpdateGearPointers();

    UpdateCameraButtonState();
    UpdateScopeButtonState();
    UpdateAuxScopeButtonState();
    UpdateStepGuiderButtonState();
    UpdateRotatorButtonState();
    UpdateConnectAllButtonState();
    UpdateDisconnectAllButtonState();
}

void GearDialog::OnButtonConnectAll(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonConnectAll\n");

    bool canceled = DoConnectCamera(false);
    if (canceled)
        return;
    OnButtonConnectStepGuider(event);
    OnButtonConnectScope(event);
    OnButtonConnectAuxScope(event);
    OnButtonConnectRotator(event);

    bool done = true;
    if (m_pCamera && !m_pCamera->Connected)
        done = false;
    if (m_pScope && !m_pScope->IsConnected())
        done = false;
    if (m_pAuxScope && !m_pAuxScope->IsConnected())
        done = false;
    if (m_pStepGuider && !m_pStepGuider->IsConnected())
        done = false;
    if (m_pRotator && !m_pRotator->IsConnected())
        done = false;

    if (done)
        EndModal(0);
}

void GearDialog::OnButtonDisconnectAll(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonDisconnectAll\n");

    OnButtonDisconnectScope(event);
    OnButtonDisconnectAuxScope(event);
    OnButtonDisconnectCamera(event);
    OnButtonDisconnectStepGuider(event);
    OnButtonDisconnectRotator(event);
}

// Handle 'Esc' char as close trigger
void GearDialog::OnChar(wxKeyEvent& evt)
{
    if (evt.GetKeyCode() == WXK_ESCAPE && !evt.HasModifiers())
    {
        EndModal(0);
    }
    else
    {
        evt.Skip();
    }
}

void GearDialog::OnChoiceCamera(wxCommandEvent& event)
{
    try
    {
        wxString choice = m_pCameras->GetStringSelection();

        delete m_pCamera;
        m_pCamera = nullptr;

        UpdateGearPointers();

        m_pCamera = GuideCamera::Factory(choice);

        Debug.AddLine(wxString::Format("Created new camera of type %s = %p", choice, m_pCamera));

        if (pConfig->Profile.GetString("/camera/LastMenuChoice", wxEmptyString) != choice)
        {
            pConfig->Profile.SetString("/camera/LastMenuChoice", choice);
            m_flushConfig = true;
        }

        m_selectCameraButton->SetCamera(choice);
        m_selectCameraButton->Enable(m_pCamera && m_pCamera->CanSelectCamera());
        m_lastCamera = choice;

        if (!m_pCamera)
        {
            throw THROW_INFO("OnChoiceCamera: m_pCamera == NULL");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();

    m_cameraUpdated = true;
}

static void AutoLoadDefectMap()
{
    if (pConfig->Profile.GetBoolean("/camera/AutoLoadDefectMap", true))
    {
        Debug.AddLine("auto-loading defect map");
        pFrame->LoadDefectMapHandler(true);
    }
}

static void AutoLoadDarks()
{
    if (pConfig->Profile.GetBoolean("/camera/AutoLoadDarks", true))
    {
        Debug.AddLine("Auto-loading dark library");
        pFrame->LoadDarkHandler(true);
    }
}

wxString GearDialog::SelectedCameraId() const
{
    return CameraSelectorButton::SelectedCameraId(m_lastCamera);
}

void GearDialog::OnCommandSelectCamera(wxCommandEvent& event)
{
    m_flushConfig = true;
}

void GearDialog::OnButtonSetupCamera(wxCommandEvent& event)
{
    m_pCamera->ShowPropertyDialog();

    // camera setup may have changed camera name so re-load the camera list
    wxString selection = m_pCameras->GetStringSelection();
    LoadCameras(m_pCameras);
    SetMatchingSelection(m_pCameras, selection);
}

bool GearDialog::DoConnectCamera(bool autoReconnecting)
{
    bool canceled = false;

    try
    {
        if (!m_pCamera)
        {
            throw ERROR_INFO("DoConnectCamera called with m_pCamera == NULL");
        }

        if (m_pCamera->Connected)
        {
            throw THROW_INFO("DoConnectCamera: called when connected");
        }

        wxString newCam = m_pCameras->GetStringSelection();

        Debug.Write(wxString::Format(_T("gear_dialog: DoConnectCamera [%s]\n"), newCam));

        pFrame->StatusMsgNoTimeout(_("Connecting to Camera ..."));

        wxString cameraId = SelectedCameraId();

        Debug.Write(wxString::Format("Connecting to camera [%s] id = [%s]\n", newCam, cameraId));

        int profileBinning = m_pCamera->Binning;
        if (m_pCamera->Connect(cameraId))
        {
            throw THROW_INFO("DoConnectCamera: connect failed");
        }

        // update camera pixel size from the driver, cam must be connected for reliable results
        double prevPixelSize = m_pCamera->GetProfilePixelSize();
        double pixelSize;
        bool err = m_pCamera->GetDevicePixelSize(&pixelSize);
        if (!err)
        {
            m_pCamera->SetCameraPixelSize(pixelSize);
            if (prevPixelSize != GuideCamera::UnknownPixelSize)
                m_imageScaleRatio *= pixelSize / prevPixelSize;
        }

        Debug.Write(wxString::Format("DoConnectCamera: reconnecting=%d warningIssued=%d lastCam=[%s] scaleRatio=%.3f\n",
                                     autoReconnecting, m_camWarningIssued, m_lastCamera, m_imageScaleRatio));

        // No very reliable way to know if cam selection has changed - id's and name strings may be the same for different cams
        // from same mfr so do what we can here including consideration of image scale change Purpose is to warn user of
        // potential loss of dark/bpm files and later, to adjust guide params as best we can
        if (!m_camWarningIssued && !autoReconnecting)
        {
            if ((m_lastCamera != _("None") && newCam != _("None") && !DeviceSelectionMatches(m_lastCamera, newCam)) ||
                (fabs(m_imageScaleRatio - 1.0) >= 0.01))
            {
                int currProfileId = pConfig->GetCurrentProfileId();
                wxString darkName = MyFrame::DarkLibFileName(currProfileId);
                wxString bpmName = DefectMap::DefectMapFileName(currProfileId);

                m_camChanged = true;

                // Can't use standard checks because we don't want to consider sensor-size
                if (wxFileExists(darkName) || wxFileExists(bpmName))
                {
                    Debug.Write("DoConnectCamera: displaying camera-change warning\n");

                    wxString msg = _("By changing cameras in this profile, you won't be able to use the existing dark library "
                                     "or bad-pixel maps. You should consider"
                                     " creating a new profile for this set-up.  Do you want to connect to this camera anyway?");
                    if (wxMessageBox(msg, _("Camera Change Warning"), wxYES_NO, this) == wxYES)
                    {
                        m_camWarningIssued = true;
                        m_lastCamera = newCam; // make consistent with what's in the UI
                    }
                    else
                    {
                        m_pCamera->Disconnect();
                        SetMatchingSelection(m_pCameras, m_lastCamera);
                        wxCommandEvent dummy;
                        OnChoiceCamera(dummy);
                        canceled = true;
                        m_camChanged = false;
                        throw THROW_INFO("DoConnectCamera: user cancelled after camera-change warning");
                    }
                }

                Debug.Write("DoConnectCamera: camera changed\n");
            }
        }

        // update default gain setting from the driver
        if (m_pCamera->HasGainControl && !pConfig->Profile.HasEntry("/camera/gain"))
        {
            int defaultGain = m_pCamera->GetDefaultCameraGain();
            Debug.Write(wxString::Format("Initializing camera gain to %d%%\n", defaultGain));
            m_pCamera->SetCameraGain(defaultGain);
        }

        // See if the profile was created with a binning level that isn't supported by the camera (user mistake) - if so, reset
        // binning to 1 Must be done here because orig binning level is not saved
        if (profileBinning > m_pCamera->MaxBinning)
        {
            int rslt;
            if (TheScope())
            {
                rslt = TheScope()->GetCalibrationDuration() / profileBinning;
                TheScope()->SetCalibrationDuration(rslt);
            }
            m_pCamera->SetBinning(1);
            Debug.Write(wxString::Format("CamConfigDlg correcting bogus user binning value from %d to 1\n", profileBinning));
        }

        // force re-build of camera tab in case Connect updated any of
        // the camera properties that influence the camera tab. For
        // example, binning options.
        m_cameraUpdated = true;

        Debug.AddLine("Connected Camera: " + m_pCamera->Name);
        Debug.Write(wxString::Format("FullSize=(%d,%d)\n", m_pCamera->FullSize.x, m_pCamera->FullSize.y));
        Debug.Write(wxString::Format("PixelSize=%.2f\n", m_pCamera->GetCameraPixelSize()));
        Debug.Write(wxString::Format("BitsPerPixel=%u\n", m_pCamera->BitsPerPixel()));
        Debug.Write(wxString::Format("HasGainControl=%d\n", m_pCamera->HasGainControl));

        if (m_pCamera->HasGainControl)
        {
            Debug.Write(wxString::Format("GuideCameraGain=%d\n", m_pCamera->GuideCameraGain));
        }

        Debug.Write(wxString::Format("HasShutter=%d\n", m_pCamera->HasShutter));
        Debug.Write(wxString::Format("HasSubFrames=%d\n", m_pCamera->HasSubframes));
        Debug.Write(wxString::Format("ST4HasGuideOutput=%d\n", m_pCamera->ST4HasGuideOutput()));

        if (!autoReconnecting) // On a reconnect, this stuff is already established
        {
            AutoLoadDefectMap();
            if (!pCamera->CurrentDefectMap)
            {
                AutoLoadDarks();
            }
            pFrame->SetDarkMenuState();
        }

        pFrame->StatusMsg(_("Camera Connected"));

        pFrame->UpdateStatusBarStateLabels();
        pFrame->pStatsWin->UpdateCooler();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        pFrame->StatusMsg(_("Camera Connect Failed"));
        pFrame->UpdateStatusBarStateLabels();
    }

    UpdateButtonState();

    return canceled;
}

void GearDialog::OnButtonConnectCamera(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonConnectCamera\n");
    DoConnectCamera(false);
}

bool GearDialog::ReconnectCamera()
{
    Debug.Write("gear_dialog: ReconnectCamera\n");
    DoConnectCamera(true);
    bool err = !m_pCamera || !m_pCamera->Connected;
    return err;
}

void GearDialog::OnButtonDisconnectCamera(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonDisconnectCamera\n");

    try
    {
        if (!m_pCamera)
        {
            throw ERROR_INFO("OnButtonDisconnectCamera called with m_pCamera == NULL");
        }

        if (!m_pCamera->Connected)
        {
            throw THROW_INFO("OnButtonDisconnectCamera: called when not connected");
        }

        m_pCamera->Disconnect();

        if (m_pScope && m_pScope->RequiresCamera() && m_pScope->IsConnected())
        {
            Debug.Write("gear_dialog: scope requires camera so disconnecting scope\n");
            OnButtonDisconnectScope(event);
        }

        pFrame->StatusMsg(_("Camera Disconnected"));
        pFrame->UpdateStatusBarStateLabels();
        pFrame->pStatsWin->UpdateCooler();
        pFrame->pStatsWin->ResetImageSize();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();
}

void GearDialog::UpdateGearPointers()
{
    pCamera = m_pCamera;

    if (m_pStepGuider)
    {
        pMount = m_pStepGuider;
        pSecondaryMount = m_pScope;
    }
    else
    {
        pMount = m_pScope;
        pSecondaryMount = nullptr;
    }

    pPointingSource = m_pScope && (!m_pAuxScope || m_pScope->CanReportPosition()) ? m_pScope : m_pAuxScope;

    pRotator = m_pRotator;
}

void GearDialog::OnChoiceScope(wxCommandEvent& event)
{
    try
    {
        wxString choice = m_pScopes->GetStringSelection();

        delete m_pScope;
        m_pScope = nullptr;
        UpdateGearPointers();

        m_pScope = Scope::Factory(choice);
        Debug.AddLine(wxString::Format("Created new scope of type %s = %p", choice, m_pScope));

        if (pConfig->Profile.GetString("/scope/LastMenuChoice", wxEmptyString) != choice)
        {
            pConfig->Profile.SetString("/scope/LastMenuChoice", choice);
            m_flushConfig = true;
        }

        if (!m_pScope)
        {
            throw THROW_INFO("OnChoiceScope: m_pScope == NULL");
        }

        m_ascomScopeSelected = choice.Contains("ASCOM");
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();

    m_mountUpdated = true;
}

void GearDialog::OnChoiceAuxScope(wxCommandEvent& event)
{
    try
    {
        wxString choice = m_pAuxScopes->GetStringSelection();

        if (m_pAuxScope != m_pScope)
            delete m_pAuxScope;
        m_pAuxScope = nullptr;
        UpdateGearPointers();

        m_pAuxScope = Scope::Factory(choice);
        Debug.AddLine(wxString::Format("Created new aux scope of type %s = %p", choice, m_pAuxScope));

        if (pConfig->Profile.GetString("/scope/LastAuxMenuChoice", wxEmptyString) != choice)
        {
            pConfig->Profile.SetString("/scope/LastAuxMenuChoice", choice);
            m_flushConfig = true;
        }

        if (!m_pAuxScope)
        {
            throw THROW_INFO("OnAuxChoiceScope: m_pAuxScope == NULL");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();
}

void GearDialog::OnButtonSetupScope(wxCommandEvent& event)
{
    m_pScope->SetupDialog();

    // scope setup may have changed the scope name so re-load the scope list
    wxString selection = m_pScopes->GetStringSelection();
    LoadMounts(m_pScopes);
    SetMatchingSelection(m_pScopes, selection);
}

void GearDialog::OnButtonSetupAuxScope(wxCommandEvent& event)
{
    m_pAuxScope->SetupDialog();

    // scope setup may have changed scope name so re-load the aux scope list
    wxString selection = m_pAuxScopes->GetStringSelection();
    LoadAuxMounts(m_pAuxScopes);
    SetMatchingSelection(m_pAuxScopes, selection);
}

void GearDialog::OnButtonConnectScope(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonConnectScope\n");

    try
    {
        // m_pScope is NULL when scope selection is "None"

        if (m_pScope && m_pScope->IsConnected())
        {
            throw THROW_INFO("OnButtonConnectScope: called when connected");
        }

        if (m_pScope)
        {
            pFrame->StatusMsgNoTimeout(_("Connecting to Mount ..."));

            Debug.Write(wxString::Format("Connecting to mount [%s]\n", m_pScopes->GetStringSelection()));

            if (m_pScope->Connect())
            {
                throw THROW_INFO("OnButtonConnectScope: connect failed");
            }

            if (m_pScope && m_ascomScopeSelected && !m_pScope->CanPulseGuide())
            {
                m_pScope->Disconnect();
                wxMessageBox(wxString::Format(_("Mount does not support the required PulseGuide interface"), _("Error")));
                throw THROW_INFO("OnButtonConnectScope: PulseGuide commands not supported");
            }

            pFrame->StatusMsg(_("Mount Connected"));
            pFrame->UpdateStatusBarStateLabels();
        }
        else
        {
            pFrame->UpdateStatusBarStateLabels();
        }

        Debug.AddLine("Connected Scope:" + (m_pScope ? m_pScope->Name() : "None"));
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        pFrame->StatusMsg(_("Mount Connect Failed"));
        pFrame->UpdateStatusBarStateLabels();
    }

    UpdateButtonState();
}

void GearDialog::OnButtonConnectAuxScope(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonConnectAuxScope\n");

    try
    {
        // m_pAuxScope is NULL when scope selection is "None"

        if (m_pAuxScope && m_pAuxScope->IsConnected())
        {
            throw THROW_INFO("OnButtonConnectAuxScope: called when connected");
        }

        if (m_pAuxScope)
        {
            pFrame->StatusMsgNoTimeout(_("Connecting to Aux Mount ..."));

            Debug.Write(wxString::Format("Connecting to aux mount [%s]\n", m_pAuxScopes->GetStringSelection()));

            if (m_pAuxScope->Connect())
            {
                throw THROW_INFO("OnButtonConnectAuxScope: connect failed");
            }

            pFrame->StatusMsg(_("Aux Mount Connected"));
        }

        Debug.AddLine("Connected AuxScope:" + (m_pAuxScope ? m_pAuxScope->Name() : "None"));
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        pFrame->StatusMsg(_("Aux Mount Connect Failed"));
    }

    UpdateButtonState();
}

void GearDialog::OnButtonDisconnectScope(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonDisconnectScope\n");

    try
    {
        if (!m_pScope)
        {
            throw ERROR_INFO("OnButtonDisconnectScope called with m_pScope == NULL");
        }

        if (!m_pScope->IsConnected())
        {
            throw THROW_INFO("OnButtonDisconnectScope: called when not connected");
        }

        m_pScope->Disconnect();

        pFrame->StatusMsg(_("Mount Disconnected"));
        pFrame->UpdateStatusBarStateLabels();

        if (pFrame->pManualGuide)
        {
            pFrame->pManualGuide->Destroy();
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();
}

void GearDialog::OnButtonDisconnectAuxScope(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonDisconnectAuxScope\n");

    try
    {
        if (!m_pAuxScope)
        {
            throw ERROR_INFO("OnButtonDisconnectAuxScope called with m_pAuxScope == NULL");
        }

        if (!m_pAuxScope->IsConnected())
        {
            throw THROW_INFO("OnButtonDisconnectAuxScope: called when not connected");
        }

        m_pAuxScope->Disconnect();
        pFrame->StatusMsg(_("Aux Mount Disconnected"));
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();
}

void GearDialog::ShowMoreGear()
{
    if (m_showMoreGear)
    {
        for (int i = 14; i <= 14 + 8; i++)
            m_gearSizer->Show(i, true);
        m_moreButton->SetLabel(_("Hide"));
    }
    else
    {
        for (int i = 14; i <= 14 + 8; i++)
            m_gearSizer->Hide(i);
        m_moreButton->SetLabel(_("More Equipment ..."));
    }
}

void GearDialog::OnButtonMore(wxCommandEvent& event)
{
    m_showMoreGear = !m_showMoreGear;
    ShowMoreGear();
    Layout();
    GetSizer()->Fit(this);
}

void GearDialog::OnChoiceStepGuider(wxCommandEvent& event)
{
    try
    {
        wxString choice = m_pStepGuiders->GetStringSelection();

        delete m_pStepGuider;
        m_pStepGuider = NULL;
        UpdateGearPointers();

        m_pStepGuider = StepGuider::Factory(choice);
        Debug.AddLine(wxString::Format("Created new stepguider of type %s = %p", choice, m_pStepGuider));

        if (pConfig->Profile.GetString("/stepguider/LastMenuChoice", wxEmptyString) != choice)
        {
            pConfig->Profile.SetString("/stepguider/LastMenuChoice", choice);
            m_flushConfig = true;
        }

        if (!m_pStepGuider)
        {
            throw THROW_INFO("OnChoiceStepGuider: m_pStepGuider == NULL");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();

    m_stepGuiderUpdated = true;
}

void GearDialog::OnButtonSetupStepGuider(wxCommandEvent& event)
{
    m_pStepGuider->ShowPropertyDialog();

    // setup dialog may have changed device name so re-load the list
    wxString selection = m_pStepGuiders->GetStringSelection();
    LoadAOs(m_pStepGuiders);
    SetMatchingSelection(m_pStepGuiders, selection);
}

void GearDialog::OnButtonConnectStepGuider(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonConnectStepGuider\n");

    try
    {
        // m_pStepGuider is NULL when stepguider selection is "None"

        if (m_pStepGuider && m_pStepGuider->IsConnected())
        {
            throw THROW_INFO("OnButtonConnectStepGuider: called when connected");
        }

        if (m_pStepGuider)
        {
            pFrame->StatusMsgNoTimeout(_("Connecting to AO ..."));

            Debug.Write(wxString::Format("Connecting to AO [%s]\n", m_pStepGuiders->GetStringSelection()));

            if (m_pStepGuider->Connect())
            {
                throw THROW_INFO("OnButtonConnectStepGuider: connect failed");
            }

            pFrame->StatusMsgNoTimeout(_("Centering AO ..."));
            if (m_pStepGuider->Center())
            {
                m_pStepGuider->Disconnect();
                throw ERROR_INFO("StepGuider unable to center");
            }
        }

        if (m_pStepGuider)
        {
            pFrame->StatusMsg(_("AO Connected"));
            pFrame->UpdateStatusBarStateLabels();
        }
        else
        {
            pFrame->UpdateStatusBarStateLabels();
        }

        Debug.AddLine("Connected AO:" + (m_pStepGuider ? m_pStepGuider->Name() : "None"));
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        pFrame->StatusMsg(_("AO Connect Failed"));
        pFrame->UpdateStatusBarStateLabels();
    }

    UpdateButtonState();
}

void GearDialog::OnButtonDisconnectStepGuider(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonDisconnectStepGuider\n");

    try
    {
        if (m_pStepGuider == NULL)
        {
            throw ERROR_INFO("OnButtonDisconnectStepGuider called with m_pStepGuider == NULL");
        }

        if (!m_pStepGuider->IsConnected())
        {
            throw THROW_INFO("OnButtonDisconnectStepGuider: called when not connected");
        }

        m_pStepGuider->Disconnect();

        if (m_pScope && m_pScope->RequiresStepGuider() && m_pScope->IsConnected())
        {
            Debug.Write("gear_dialog: scope requires stepguider so disconnecting scope\n");
            OnButtonDisconnectScope(event);
        }

        pFrame->StatusMsg(_("AO Disconnected"));
        pFrame->UpdateStatusBarStateLabels();

        if (pFrame->pManualGuide)
        {
            pFrame->pManualGuide->Destroy();
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();
}

void GearDialog::OnChoiceRotator(wxCommandEvent& event)
{
    try
    {
        wxString choice = m_pRotators->GetStringSelection();

        delete m_pRotator;
        m_pRotator = NULL;
        UpdateGearPointers();

        m_pRotator = Rotator::Factory(choice);
        Debug.AddLine(wxString::Format("Created new Rotator of type %s = %p", choice, m_pRotator));

        if (pConfig->Profile.GetString("/rotator/LastMenuChoice", wxEmptyString) != choice)
        {
            pConfig->Profile.SetString("/rotator/LastMenuChoice", choice);
            m_flushConfig = true;
        }

        if (!m_pRotator)
        {
            throw THROW_INFO("OnChoiceRotator: m_pRotator == NULL");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();

    m_rotatorUpdated = true;
}

void GearDialog::OnButtonSetupRotator(wxCommandEvent& event)
{
    m_pRotator->ShowPropertyDialog();

    // setup dialog may have changed device name so re-load the list
    wxString selection = m_pRotators->GetStringSelection();
    LoadRotators(m_pRotators);
    SetMatchingSelection(m_pRotators, selection);
}

void GearDialog::OnButtonConnectRotator(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonConnectRotator\n");

    try
    {
        // m_pRotator is NULL when stepguider selection is "None"

        if (m_pRotator && m_pRotator->IsConnected())
        {
            throw THROW_INFO("OnButtonConnectRotator: called when connected");
        }

        if (m_pRotator)
        {
            pFrame->StatusMsgNoTimeout(_("Connecting to Rotator ..."));

            Debug.Write(wxString::Format("Connecting to rotator [%s]\n", m_pRotators->GetStringSelection()));

            if (m_pRotator->Connect())
            {
                throw THROW_INFO("OnButtonConnectRotator: connect failed");
            }
        }

        if (m_pRotator)
        {
            pFrame->StatusMsg(_("Rotator Connected"));
            pFrame->UpdateStatusBarStateLabels();
        }
        else
        {
            pFrame->UpdateStatusBarStateLabels();
        }

        Debug.AddLine("Connected Rotator:" + (m_pRotator ? m_pRotator->Name() : "None"));
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        pFrame->StatusMsg(_("Rotator Connect Failed"));
        pFrame->UpdateStatusBarStateLabels();
    }

    UpdateButtonState();
}

void GearDialog::OnButtonDisconnectRotator(wxCommandEvent& event)
{
    Debug.Write("gear_dialog: OnButtonDisconnectRotator\n");

    try
    {
        if (m_pRotator == NULL)
        {
            throw ERROR_INFO("OnButtonDisconnectRotator called with m_pRotator == NULL");
        }

        if (!m_pRotator->IsConnected())
        {
            throw THROW_INFO("OnButtonDisconnectRotator: called when not connected");
        }

        m_pRotator->Disconnect();

        pFrame->StatusMsg(_("Rotator Disconnected"));
        pFrame->UpdateStatusBarStateLabels();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    UpdateButtonState();
}

void GearDialog::OnButtonProfileManage(wxCommandEvent& event)
{
    PopupMenu(m_menuProfileManage, m_btnProfileManage->GetPosition().x,
              m_btnProfileManage->GetPosition().y + m_btnProfileManage->GetSize().GetHeight());
}

void GearDialog::OnButtonWizard(wxCommandEvent& event)
{
    // these variables must be initialized before instantiating the ProfileWizard, since
    // the instantiation has a side-effect of installing a temporary empty profile
    bool firstLight = IsEmptyProfile();
    wxString current = m_profiles->GetStringSelection();

    bool showGreeting =
        event.GetId() == 0; // Event id of 0 comes from "first light" launch; show first light UI panel only then
    bool darks_requested;
    if (EquipmentProfileWizard::ShowModal(this, showGreeting, &darks_requested))
    {
        // a new profile was created and set as the current profile
        // if this was a first-light run, we may have left an empty "My Equipment" profile behind - if so, delete it
        if (firstLight)
        {
            pConfig->DeleteProfile(current); // Can't be any dark/bpm files associated with it
        }

        wxArrayString profiles = pConfig->ProfileNames();
        m_profiles->Set(profiles);
        m_profiles->SetStringSelection(pConfig->GetCurrentProfile());
        Layout();

        wxCommandEvent dummy;
        OnProfileChoice(dummy);

        if (darks_requested)
        {
            m_showDarksDialog = true;
            // if wizard was launched from dialog and darks are requested, connect-all and close dialog
            if (IsVisible())
            {
                Debug.Write("gear_dialog: wizard connecting all gear\n");
                wxCommandEvent dummyEvent;
                OnButtonConnectAll(dummyEvent);
            }
        }
    }
}

void GearDialog::ShowProfileWizard()
{
    wxCommandEvent dummy;
    OnButtonWizard(dummy);
    if (m_showDarksDialog)
    {
        ShowGearDialog(true); // connect equipment and launch darks dialog
    }
}

void GearDialog::ShowProfileWizard(wxCommandEvent& evt)
{
    OnButtonWizard(evt);
    if (!IsEmptyProfile()) // User didn't just cancel out
    {
        ShowGearDialog(m_showDarksDialog); // Maybe auto-connect, close, launch darks; or just display dialog
    }
}

bool GearDialog::IsEmptyProfile()
{
    wxString lastCamera = pConfig->Profile.GetString("/camera/LastMenuChoice", _("None"));
    wxString lastScope = pConfig->Profile.GetString("/scope/LastMenuChoice", _("None"));
    return lastCamera == _("None") && lastScope == _("None");
}

void GearDialog::OnProfileChoice(wxCommandEvent& event)
{
    wxString prev = pConfig->GetCurrentProfile();
    wxString selection = m_profiles->GetStringSelection();
    if (selection != prev)
        m_flushConfig = true;
    pConfig->SetCurrentProfile(selection);
    LoadGearChoices();
    pFrame->LoadProfileSettings();
    pFrame->pGuider->LoadProfileSettings();
    pFrame->UpdateTitle();
    pFrame->pGraphLog->ResetData();
}

bool GearDialog::SetProfile(int profileId, wxString *error)
{
    if (profileId == pConfig->GetCurrentProfileId())
        return false;

    if (IsModal())
    {
        // these error messages are internal to the event server and are not translated
        *error = "cannot set profile when Connect Equipment dialog is open";
        return true;
    }

    if ((m_pCamera && m_pCamera->Connected) || (m_pScope && m_pScope->IsConnected()) ||
        (m_pAuxScope && m_pAuxScope->IsConnected()) || (m_pStepGuider && m_pStepGuider->IsConnected()) ||
        (m_pRotator && m_pRotator->IsConnected()))
    {
        *error = "cannot set profile when equipment is connected";
        return true;
    }

    if (!pConfig->ProfileExists(profileId))
    {
        *error = "invalid profile id";
        return true;
    }

    wxString profile = pConfig->GetProfileName(profileId);

    if (!m_profiles->SetStringSelection(profile))
    {
        *error = "invalid profile id";
        return true;
    }

    // need the side-effects for making the selection
    wxCommandEvent dummy;
    OnProfileChoice(dummy);

    // need the side-effects of closing the dialog
    EndModal(0);

    return false;
}

bool GearDialog::ConnectAll(wxString *error)
{
    if (m_pCamera && m_pCamera->Connected && (!m_pScope || m_pScope->IsConnected()) &&
        (!m_pAuxScope || m_pAuxScope->IsConnected()) && (!m_pStepGuider || m_pStepGuider->IsConnected()) &&
        (!m_pRotator || m_pRotator->IsConnected()))
    {
        // everything already connected
        return false;
    }

    if (pFrame->CaptureActive)
    {
        // these error messages are internal to the event server and are not translated
        *error = "cannot connect equipment when capture is active";
        return true;
    }

    if (IsModal())
    {
        *error = "cannot connect equipment when Connect Equipment dialog is open";
        return true;
    }

    Debug.Write("gear_dialog: ConnectAll calls OnButtonConnectAll\n");

    wxCommandEvent dummyEvent;
    OnButtonConnectAll(dummyEvent);

    // need the side-effects of closing the dialog
    EndModal(0);

    wxString fail;
    if (!m_pCamera || !m_pCamera->Connected)
        fail += " camera";
    if (m_pScope && !m_pScope->IsConnected())
        fail += " mount";
    if (m_pAuxScope && !m_pAuxScope->IsConnected())
        fail += " aux mount";
    if (m_pStepGuider && !m_pStepGuider->IsConnected())
        fail += " AO";
    if (m_pRotator && !m_pRotator->IsConnected())
        fail += " Rotator";

    if (fail.IsEmpty())
    {
        return false;
    }
    else
    {
        *error = "equipment failed to connect:" + fail;
        return true;
    }
}

bool GearDialog::DisconnectAll(wxString *error)
{
    if ((!m_pCamera || !m_pCamera->Connected) && (!m_pScope || !m_pScope->IsConnected()) &&
        (!m_pAuxScope || !m_pAuxScope->IsConnected()) && (!m_pStepGuider || !m_pStepGuider->IsConnected()) &&
        (!m_pRotator || !m_pRotator->IsConnected()))
    {
        // nothing connected
        return false;
    }

    if (pFrame->CaptureActive)
    {
        // these error messages are internal to the event server and are not translated
        *error = "cannot disconnect equipment while capture active";
        return true;
    }

    if (IsModal())
    {
        *error = "cannot disconnect equipment when Connect Equipment dialog is open";
        return true;
    }

    Debug.Write("gear_dialog: DisconnectAll calls OnButtonDisconnectAll\n");

    wxCommandEvent dummy;
    OnButtonDisconnectAll(dummy);

    EndModal(0); // need the side effects

    return false;
}

void GearDialog::Shutdown(bool forced)
{
    Debug.Write(wxString::Format("Shutdown: forced=%d\n", forced));

    if (!forced && m_pScope && m_pScope->IsConnected())
    {
        Debug.AddLine("Shutdown: disconnect scope");
        m_pScope->Disconnect();
    }

    if (m_pAuxScope && m_pAuxScope->IsConnected())
    {
        Debug.AddLine("Shutdown: disconnect aux scope");
        m_pAuxScope->Disconnect();
    }

    if (!forced && m_pCamera && m_pCamera->Connected)
    {
        Debug.AddLine("Shutdown: disconnect camera");
        m_pCamera->Disconnect();
    }

    if (!forced && m_pStepGuider && m_pStepGuider->IsConnected())
    {
        Debug.AddLine("Shutdown: disconnect stepguider");
        m_pStepGuider->Disconnect();
    }

    if (m_pRotator && m_pRotator->IsConnected())
    {
        Debug.AddLine("Shutdown: disconnect rotator");
        m_pRotator->Disconnect();
    }

    Debug.AddLine("Shutdown complete");
}

struct NewProfileDialog : public wxDialog
{
    wxTextCtrl *m_name;
    wxChoice *m_copyFrom;

    NewProfileDialog(wxWindow *parent);
};

NewProfileDialog::NewProfileDialog(wxWindow *parent) : wxDialog(parent, wxID_ANY, _("New Equipment Profile"))
{
    wxSizerFlags sizerLabelFlags = wxSizerFlags().Align(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL).Border(wxALL, 2).Expand();
    wxSizerFlags sizerTextFlags = wxSizerFlags().Align(wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL).Border(wxALL, 2).Expand();
    wxSizerFlags sizerButtonFlags = wxSizerFlags().Align(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL).Border(wxALL, 2).Expand();

    wxSizer *sizer1 = new wxBoxSizer(wxHORIZONTAL);
    sizer1->Add(
        new wxStaticText(this, wxID_ANY, _("Name"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL),
        sizerLabelFlags);
    wxSize size = GetTextExtent("MMMMMMMMMMMMMMMMMMMMMMMMMMMM");
    size.SetHeight(-1);
    m_name = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, size);
    m_name->SetToolTip(_("Enter the name of the new equipment profile"));
    sizer1->Add(m_name, sizerTextFlags);

    wxArrayString choices = pConfig->ProfileNames();
    choices.Insert(_("PHD Defaults"), 0);

    wxSizer *sizer2 = new wxBoxSizer(wxHORIZONTAL);
    sizer2->Add(new wxStaticText(this, wxID_ANY, _("Profile initial settings"), wxDefaultPosition, wxDefaultSize,
                                 wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL),
                sizerLabelFlags);
    m_copyFrom = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
    m_copyFrom->SetSelection(0);
    m_copyFrom->SetToolTip(_("Select PHD Defaults to create a profile with default PHD settings, or select an existing "
                             "Equipment Profile to copy its settings into your new profile."));
    sizer2->Add(m_copyFrom, sizerTextFlags);

    wxSizer *sizer3 = new wxBoxSizer(wxVERTICAL);
    sizer3->Add(sizer1);
    sizer3->Add(sizer2);
    sizer3->Add(CreateButtonSizer(wxOK | wxCANCEL), sizerButtonFlags);

    sizer3->SetSizeHints(this);
    SetSizerAndFit(sizer3);
}

void GearDialog::OnProfileNew(wxCommandEvent& event)
{
    NewProfileDialog dlg(this);
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString newname = dlg.m_name->GetValue();
    if (newname.IsEmpty())
        return;

    if (pConfig->GetProfileId(newname) > 0)
    {
        wxMessageBox(wxString::Format(_("Cannot create profile %s, there is already a profile with that name"), newname),
                     _("Error"));
        return;
    }

    if (dlg.m_copyFrom->GetSelection() != 0)
    {
        wxString copyFrom = dlg.m_copyFrom->GetStringSelection();
        if (pConfig->CloneProfile(newname, copyFrom))
        {
            wxMessageBox(wxString::Format(_("Could not create profile %s from profile %s"), newname, copyFrom), _("Error"));
            return;
        }
    }

    if (pConfig->SetCurrentProfile(newname))
    {
        wxMessageBox(wxString::Format(_("Could not create profile %s"), newname), _("Error"));
        return;
    }

    wxArrayString profiles = pConfig->ProfileNames();
    m_profiles->Set(profiles);
    m_profiles->SetStringSelection(pConfig->GetCurrentProfile());
    Layout();

    wxCommandEvent dummy;
    OnProfileChoice(dummy);
}

void GearDialog::OnProfileDelete(wxCommandEvent& event)
{
    wxString current = m_profiles->GetStringSelection();
    int result = wxMessageBox(wxString::Format(_("Delete profile %s?"), current), _("Delete Equipment Profile"),
                              wxOK | wxCANCEL | wxCENTRE);
    if (result != wxOK)
        return;
    int id = pConfig->GetProfileId(current);
    if (id > 0)
        pFrame->DeleteDarkLibraryFiles(id);
    pConfig->DeleteProfile(current);
    wxArrayString profiles = pConfig->ProfileNames();
    m_profiles->Set(profiles);
    m_profiles->SetStringSelection(pConfig->GetCurrentProfile());
    Layout();

    wxCommandEvent dummy;
    OnProfileChoice(dummy);
}

void GearDialog::OnProfileRename(wxCommandEvent& event)
{
    wxString current = m_profiles->GetStringSelection();
    wxTextEntryDialog dlg(this, wxString::Format(_("Rename %s"), current), _("Rename Equipment Profile"), current);
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString newname = dlg.GetValue();
    if (newname.IsEmpty())
        return;

    if (pConfig->GetProfileId(newname) > 0)
    {
        wxMessageBox(_(wxString::Format("Cannot not rename profile to %s, there is already a profile with that name", newname)),
                     _("Error"));
        return;
    }

    if (pConfig->RenameProfile(current, newname))
    {
        wxMessageBox(_("Could not rename profile"), _("Error"));
        return;
    }

    int sel = m_profiles->GetSelection();
    m_profiles->SetString(sel, newname);
    pFrame->UpdateTitle();
    Layout();
}

void GearDialog::OnProfileLoad(wxCommandEvent& event)
{
    wxString default_path = pConfig->Global.GetString("/profileFilePath", wxEmptyString);

    wxFileDialog dlg(this, _("Import PHD Equipment Profiles"), default_path, wxEmptyString,
                     wxT("PHD profile files (*.phd)|*.phd"), wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

    if (dlg.ShowModal() == wxID_CANCEL)
    {
        return;
    }

    wxArrayString paths;
    dlg.GetPaths(paths);

    for (size_t i = 0; i < paths.GetCount(); i++)
    {
        wxString path = paths[i];

        if (i == 0)
            pConfig->Global.SetString("/profileFilePath", wxFileName(path).GetPath());

        pConfig->ReadProfile(path);
    }

    wxArrayString profiles = pConfig->ProfileNames();
    m_profiles->Set(profiles);
    m_profiles->SetStringSelection(pConfig->GetCurrentProfile());
    Layout();

    wxCommandEvent dummy;
    OnProfileChoice(dummy);
}

void GearDialog::OnProfileSave(wxCommandEvent& event)
{
    wxString default_path = pConfig->Global.GetString("/profileFilePath", wxEmptyString);
    wxString fname =
        wxFileSelector(_("Export PHD Equipment Profile"), default_path, pConfig->GetCurrentProfile() + wxT(".phd"), wxT("phd"),
                       wxT("PHD profile files (*.phd)|*.phd"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT, this);

    if (fname.IsEmpty())
    {
        // dialog canceled
        return;
    }

    pConfig->Global.SetString("/profileFilePath", wxFileName(fname).GetPath());
    if (!fname.EndsWith(_T(".phd")))
    {
        fname.Append(_T(".phd"));
    }

    if (pConfig->WriteProfile(fname))
    {
        wxLogError("Cannot write file '%s'.", fname);
    }
}

void GearDialog::UpdateAdvancedDialog(bool preLoad)
{
    MyFrame *frame = static_cast<MyFrame *>(GetParent()); // global pFrame may not have initialized yet

    if (m_cameraUpdated)
    {
        frame->pAdvancedDialog->UpdateCameraPage();
        m_cameraUpdated = false;
    }

    if (m_mountUpdated)
    {
        frame->pAdvancedDialog->UpdateMountPage();
        m_mountUpdated = false;
    }

    if (m_stepGuiderUpdated)
    {
        frame->pAdvancedDialog->UpdateAoPage();
        m_stepGuiderUpdated = false;
    }

    if (m_rotatorUpdated)
    {
        frame->pAdvancedDialog->UpdateRotatorPage();
        m_rotatorUpdated = false;
    }

    if (preLoad)
    {
        frame->pAdvancedDialog->Preload();
    }
}

void GearDialog::OnAdvanced(wxCommandEvent& event)
{
    UpdateAdvancedDialog(false);
    pFrame->OnAdvanced(event);
}
