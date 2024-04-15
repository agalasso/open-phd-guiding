/*
 *  planetary_tool.h
 *  PHD Guiding
 *
 *  Created by Leo Shatz
 *  Copyright (c) 2023-2024 Leo Shatz, openphdguiding.org
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

#pragma once

// Default planetary detection parameters values
#define PT_MIN_RADIUS_DEFAULT  100
#define PT_MAX_RADIUS_DEFAULT  200
#define PT_RADIUS_MIN          1
#define PT_RADIUS_MAX          2000

#define PT_HIGH_THRESHOLD_DEFAULT 200
#define PT_THRESHOLD_MIN          1
#define PT_HIGH_THRESHOLD_MAX     400
#define PT_LOW_THRESHOLD_MAX      200

#define PT_CAMERA_EXPOSURE_MIN    1
#define PT_CAMERA_EXPOSURE_MAX    30000

static inline wxString PausePlanetDetectionAlertEnabledKey()
{
    // we want the key to be under "/Confirm" so ConfirmDialog::ResetAllDontAskAgain() resets it, but we also want the setting to be per-profile
    return wxString::Format("/Confirm/%d/PausePlanetDetectionAlertEnabled", pConfig->GetCurrentProfileId());
}

class PlanetTool
{
    PlanetTool();
public:
    static wxWindow *CreatePlanetToolWindow();
};
