/*
 *  camera_selector_button.h
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

#ifndef CAMERA_SELECTOR_BUTTON_H_INCLUDED
#define CAMERA_SELECTOR_BUTTON_H_INCLUDED

#include "phd.h"

wxDECLARE_EVENT(SELECT_CAMERA_EVENT, wxCommandEvent);

class CameraSelectorButton : public wxBitmapButton
{
public:
    CameraSelectorButton(wxWindow *pParent, wxWindowID id);
    ~CameraSelectorButton();
    void SetCamera(wxString cam);

    static wxString CameraSelectionKey(const wxString& camName);
    static wxString SelectedCameraId(const wxString& camName);

private:
    GuideCamera *m_pCamera;
    wxString m_lastCamera;
    wxArrayString m_cameraIds;
    void OnButtonSelectCamera(wxCommandEvent& event);
    void OnMenuSelectCamera(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};

#endif // CAMERA_SELECTOR_BUTTON_H_INCLUDED