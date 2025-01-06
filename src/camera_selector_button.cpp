/*
 *  camera_selector_button.cpp
 *  PHD Guiding
 *
 *  Copyright (c) 2025 PHD2 Developers
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

#include "camera_selector_button.h"
#include "icons/select.png.h"

wxDEFINE_EVENT(SELECT_CAMERA_EVENT, wxCommandEvent);

// clang-format off
wxBEGIN_EVENT_TABLE(CameraSelectorButton, wxBitmapButton)
	EVT_BUTTON(GEAR_BUTTON_SELECT_CAMERA, CameraSelectorButton::OnButtonSelectCamera)
    EVT_MENU_RANGE(MENU_SELECT_CAMERA_BEGIN, MENU_SELECT_CAMERA_END, CameraSelectorButton::OnMenuSelectCamera)
wxEND_EVENT_TABLE();
// clang-format on

CameraSelectorButton::CameraSelectorButton(wxWindow *pParent, wxWindowID id)
    : wxBitmapButton(pParent, id, wxBitmap(wxBITMAP_PNG_FROM_DATA(select)))
{
    SetToolTip(_("Select which camera to connect to when there are multiple cameras of the same type."));

    m_pCamera = nullptr;
}

CameraSelectorButton::~CameraSelectorButton()
{
    delete m_pCamera;
    m_pCamera = nullptr;
}

void CameraSelectorButton::SetCamera(wxString cam)
{
    delete m_pCamera;
    m_pCamera = nullptr;

    m_pCamera = GuideCamera::Factory(cam);
    m_lastCamera = cam;
}

void CameraSelectorButton::OnButtonSelectCamera(wxCommandEvent& event)
{
    if (!m_pCamera || !m_pCamera->CanSelectCamera())
        return;

    if (m_pCamera->HandleSelectCameraButtonClick(event))
        return;

    wxArrayString names;
    m_cameraIds.clear(); // otherwise camera selection only works randomly as EnumCameras tends to append to the camera Ids
    bool error = m_pCamera->EnumCameras(names, m_cameraIds);
    if (error || names.size() == 0)
    {
        names.clear();
        names.Add(_("No cameras found"));
        m_cameraIds.clear();
    }

    wxString selectedId = SelectedCameraId(m_lastCamera);

    wxMenu *menu = new wxMenu();
    int id = MENU_SELECT_CAMERA_BEGIN;
    for (unsigned int idx = 0; idx < names.size(); idx++)
    {
        wxMenuItem *item = menu->AppendRadioItem(id, names.Item(idx));
        if (idx < m_cameraIds.size())
        {
            const wxString& camId = m_cameraIds[idx];
            if (camId == selectedId || (idx == 0 && selectedId == GuideCamera::DEFAULT_CAMERA_ID))
                item->Check(true);
        }
        if (++id > MENU_SELECT_CAMERA_END)
        {
            Debug.AddLine("Truncating camera list!");
            break;
        }
    }

    PopupMenu(menu, 0, this->GetSize().GetHeight());

    delete menu;
}

void CameraSelectorButton::OnMenuSelectCamera(wxCommandEvent& event)
{
    unsigned int idx = event.GetId() - MENU_SELECT_CAMERA_BEGIN;
    if (idx < m_cameraIds.size())
    {
        wxString key = CameraSelectionKey(m_lastCamera);
        const wxString& id = m_cameraIds[idx];
        if (pConfig->Profile.GetString(key, wxEmptyString) != id)
        {
            pConfig->Profile.SetString(key, id);

            wxCommandEvent event(SELECT_CAMERA_EVENT, idx + MENU_SELECT_CAMERA_BEGIN);
            wxPostEvent(GetParent(), event);
        }
    }
}

wxString CameraSelectorButton::CameraSelectionKey(const wxString& camName)
{
    std::hash<std::string> hash_fn;
    std::string name(camName.c_str());
    return wxString::Format("/cam_hash/%lx/whichCamera", (unsigned long) hash_fn(name));
}

wxString CameraSelectorButton::SelectedCameraId(const wxString& camName)
{
    wxString key = CameraSelectionKey(camName);
    return pConfig->Profile.GetString(key, GuideCamera::DEFAULT_CAMERA_ID);
}