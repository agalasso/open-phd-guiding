/*
 *  camcal_import_dialog.cpp
 *  PHD Guiding
 *
 *  Created by Bruce Waddington
 *  Copyright (c) 2015 Bruce Waddington
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

// Handles import of camera calibration files (dark library, bad-pix map files) from user-selected profile to the current profile
// Source profile choices are limited to camera data with compatible geometry

#include "phd.h"
#include "camcal_import_dialog.h"
#include "wx/file.h"

// Utility function to add the <label, input> pairs to a flexgrid
static void AddTableEntryPair(wxWindow *parent, wxFlexGridSizer *pTable, const wxString& label, wxWindow *pControl)
{
    wxStaticText *pLabel = new wxStaticText(parent, wxID_ANY, label + _(": "), wxPoint(-1, -1), wxSize(-1, -1));
    pTable->Add(pLabel, 1, wxALL, 5);
    pTable->Add(pControl, 1, wxALL, 5);
}

CamCalImportDialog::CamCalImportDialog(wxWindow *parent) :
    wxDialog(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);

    SetTitle(wxString::Format(_("Import Darks to Profile %s"), pConfig->GetCurrentProfile()));

    m_thisProfileId = pConfig->GetCurrentProfileId();
    m_profileNames = pConfig->ProfileNames();

    // Start with the dark library
    wxStaticBoxSizer* darksGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Dark Library"));

    wxStaticText* darksLabel = new wxStaticText(this, wxID_STATIC, _("Choose the profile with the dark library you want to use:"), wxDefaultPosition, wxDefaultSize, 0);
    darksGroup->Add(darksLabel, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);
    wxFlexGridSizer* drkGrid = new wxFlexGridSizer(2, 2, 0, 0);

    wxArrayString drkChoices;
    drkChoices.Add(_("None"));

    FindCompatibleDarks(&drkChoices);

    if (drkChoices.Count() > 1)
    {
        m_darksChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, drkChoices, 0, wxDefaultValidator, _("Darks Profiles"));
        m_darksChoice->SetSelection(0);
        m_darksChoice->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &CamCalImportDialog::OnDarkProfileChoice, this);
        AddTableEntryPair(this, drkGrid, _("Import from profile"), m_darksChoice);
        m_darkCameraChoice = new wxStaticText(this, wxID_ANY, wxEmptyString, wxPoint(-1, -1), wxSize(-1, -1));
        AddTableEntryPair(this, drkGrid, _("Camera in profile"), m_darkCameraChoice);
        darksGroup->Add(drkGrid, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);
    }
    else
        darksLabel->SetLabelText(_("There are no compatible dark libraries available"));
    vSizer->Add(darksGroup, 0, wxALIGN_LEFT | wxALL, 10);

    // Now add the bad-pix map controls
    wxStaticBoxSizer* bpmGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Bad-pixel Map"));

    wxStaticText* bpmLabel = new wxStaticText(this, wxID_STATIC, _("Choose the profile with the bad-pixel map you want to use:"), wxDefaultPosition, wxDefaultSize, 0);
    bpmGroup->Add(bpmLabel, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

    wxArrayString bpmChoices;
    bpmChoices.Add(_("None"));

    FindCompatibleBPMs(&bpmChoices);

    if (bpmChoices.Count() > 1)
    {
        m_bpmChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, bpmChoices, 0, wxDefaultValidator, _("Bad-pix Map Profiles"));
        m_bpmChoice->SetSelection(0);
        m_bpmChoice->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &CamCalImportDialog::OnBPMProfileChoice, this);
        wxFlexGridSizer *bpmGrid = new wxFlexGridSizer(2, 2, 0, 0);
        AddTableEntryPair(this, bpmGrid, _("Import from profile"), m_bpmChoice);
        m_bpmCameraChoice = new wxStaticText(this, wxID_ANY, wxEmptyString, wxPoint(-1, -1), wxSize(-1, -1));
        AddTableEntryPair(this, bpmGrid, _("Camera in profile"), m_bpmCameraChoice);
        bpmGroup->Add(bpmGrid, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);
    }
    else
        bpmLabel->SetLabelText(_("There are no compatible bad-pixel maps available"));

    vSizer->Add(bpmGroup, 0, wxALIGN_LEFT | wxALL, 10);

    // Add the buttons
    wxBoxSizer* btnHSizer = new wxBoxSizer(wxHORIZONTAL);
    vSizer->Add(btnHSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

    wxButton* btnOk = new wxButton(this, wxID_ANY, _("OK"), wxDefaultPosition, wxDefaultSize, 0);
    btnOk->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &CamCalImportDialog::OnOk, this);
    btnOk->SetDefault();
    btnHSizer->Add(btnOk, 0, wxALIGN_CENTER_VERTICAL | wxALL, 10);

    wxButton* btnCancel = new wxButton(this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0);
    btnHSizer->Add(btnCancel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 10);

    //PopulateLBs(darksLB, bpmLB);
    SetSizerAndFit(vSizer);

    m_activeProfileName = pConfig->GetCurrentProfile();
    m_sourceDarksProfileId = -1;
    m_sourceBpmProfileId = -1;
}
CamCalImportDialog::~CamCalImportDialog(void)
{
}

void CamCalImportDialog::FindCompatibleDarks(wxArrayString* pResults)
{

    for (unsigned int i = 0; i < m_profileNames.GetCount(); i++)
    {
        int profileId = pConfig->GetProfileId(m_profileNames[i]);
        if (profileId != m_thisProfileId)
            if (pFrame->DarkLibExists(profileId, false))
                pResults->Add(m_profileNames[i]);
    }

}

void CamCalImportDialog::FindCompatibleBPMs(wxArrayString* pResults)
{
    for (unsigned int i = 0; i < m_profileNames.GetCount(); i++)
    {
        int profileId = pConfig->GetProfileId(m_profileNames[i]);
        if (profileId != m_thisProfileId)
            if (DefectMap::DefectMapExists(profileId, false))
                pResults->Add(m_profileNames[i]);
    }
}

void CamCalImportDialog::OnDarkProfileChoice(wxCommandEvent& evt)
{
    wxString selProfile = m_darksChoice->GetString(m_darksChoice->GetSelection());
    if (selProfile != _("None"))
    {
        pConfig->SetCurrentProfile(selProfile);
        m_sourceDarksProfileId = pConfig->GetCurrentProfileId();
        wxString thatCamera = pConfig->Profile.GetString("/camera/LastMenuchoice", _("None"));
        m_darkCameraChoice->SetLabelText(thatCamera);
        pConfig->SetCurrentProfile(m_activeProfileName);
    }
    else
    {
        m_darkCameraChoice->SetLabelText(wxEmptyString);
        m_sourceDarksProfileId = -1;
    }

}

void CamCalImportDialog::OnBPMProfileChoice(wxCommandEvent& evt)
{
    wxString selProfile = m_bpmChoice->GetString(m_bpmChoice->GetSelection());
    if (selProfile != _("None"))
    {
        pConfig->SetCurrentProfile(selProfile);
        m_sourceBpmProfileId = pConfig->GetCurrentProfileId();
        wxString thatCamera = pConfig->Profile.GetString("/camera/LastMenuchoice", _("None"));
        m_bpmCameraChoice->SetLabelText(thatCamera);
        pConfig->SetCurrentProfile(m_activeProfileName);
    }
    else
    {
        m_bpmCameraChoice->SetLabelText(wxEmptyString);
        m_sourceBpmProfileId = -1;
    }
}

void CamCalImportDialog::OnOk(wxCommandEvent& evt)
{
    bool bpmLoaded = false;
    wxString sourceName;
    wxString destName;

    if (m_sourceBpmProfileId != -1)
    {
        if (DefectMap::ImportFromProfile(m_sourceBpmProfileId, m_thisProfileId))
        {
            Debug.Write(wxString::Format("Defect map files imported and loaded from profile %d to profile %d\n", m_sourceBpmProfileId, m_thisProfileId));
            pFrame->LoadDefectMapHandler(true);
            bpmLoaded = true;
        }
        else
        {
            // ImportFromProfile already logs errors
            wxMessageBox(_("Bad-pixel map could not be imported because of errors in file/copy"));
        }
    }

    if (m_sourceDarksProfileId != -1)
    {
        sourceName = MyFrame::DarkLibFileName(m_sourceDarksProfileId);
        destName = MyFrame::DarkLibFileName(m_thisProfileId);
        if (wxCopyFile(sourceName, destName, true))
        {
            Debug.Write(wxString::Format("Dark library imported from profile %d to profile %d\n", m_sourceDarksProfileId, m_thisProfileId));
            if (!bpmLoaded)
            {
                pFrame->LoadDarkHandler(true);
            }
        }
        else
        {
            Debug.Write(wxString::Format("Dark lib import failed on file copy of %s to %s\n", sourceName, destName));
            wxMessageBox(_("Dark library could not be imported because of errors in file/copy"));
        }
    }
    pFrame->SetDarkMenuState();                     // Get enabled states straightened out
    EndModal(wxID_OK);
}
