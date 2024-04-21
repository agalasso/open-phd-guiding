/*
 *  scope_ascom.h
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
 *  All rights reserved.
 *
 *  Modified by Bret McKee
 *  Copyright (c) 2012-2013 Bret McKee
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

#ifndef SCOPE_ASCOM_INCLUDED
#define SCOPE_ASCOM_INCLUDED

#ifdef GUIDE_ASCOM

#include "comdispatch.h"

class ScopeASCOM : public Scope
{
    GITEntry m_gitEntry;

    // DISPIDs we reuse
    DISPID dispid_connected;
    DISPID dispid_ispulseguiding;
    DISPID dispid_isslewing;
    DISPID dispid_pulseguide;
    DISPID dispid_declination;
    DISPID dispid_rightascension;
    DISPID dispid_siderealtime;
    DISPID dispid_sitelatitude;
    DISPID dispid_sitelongitude;
    DISPID dispid_slewtocoordinates;
    DISPID dispid_raguiderate;
    DISPID dispid_decguiderate;
    DISPID dispid_sideofpier;
    DISPID dispid_abortslew;
    DISPID dispid_tracking;
    DISPID dispid_trackingrate;

    // other private variables
    bool m_canCheckPulseGuiding;
    bool m_canGetCoordinates;
    bool m_canGetGuideRates;
    bool m_canSlew;
    bool m_canSlewAsync;
    bool m_canPulseGuide;

    bool m_abortSlewWhenGuidingStuck;
    bool m_checkForSyncPulseGuide;

    wxString m_choice; // name of chosen scope

    // private functions
    bool Create(DispatchObj& obj);
    bool IsGuiding(DispatchObj *pScopeDriver);
    bool IsSlewing(DispatchObj *pScopeDriver);
    void AbortSlew(DispatchObj *pScopeDriver);

public:
    ScopeASCOM(const wxString& choice);
    virtual ~ScopeASCOM();
    static wxArrayString EnumAscomScopes();

    bool Connect() override;
    bool Disconnect() override;

    bool HasSetupDialog() const override;
    void SetupDialog() override;

    bool HasNonGuiMove() override;

    MOVE_RESULT Guide(GUIDE_DIRECTION direction, int durationMs) override;

    double GetDeclinationRadians() override;
    void EnumerateTrackingRates() override;
    bool GetTracking(bool* tracking, bool verbose) override;
    bool SetTracking(bool tracking) override;
    bool CanSetTracking() override;
    bool GetTrackingRate(enum DriveRates* rate, bool verbose) override;
    bool GetTrackingRate(enum DriveRates* rate, double *ra_rate, double *dec_rate, bool verbose) override;
    bool SetTrackingRate(enum DriveRates rate) override;
    bool SetTrackingRateOffsets(double raRateOffset, double decRateOffset) override;
    bool GetGuideRates(double *pRAGuideRate, double *pDecGuideRate) override;
    bool GetCoordinates(double *ra, double *dec, double *siderealTime) override;
    bool GetSiteLatLong(double *latitude, double *longitude) override;
    bool CanSlew() override;
    bool CanSlewAsync() override;
    bool CanReportPosition() override;
    bool CanPulseGuide() override;
    bool SlewToCoordinates(double ra, double dec) override;
    bool SlewToCoordinatesAsync(double ra, double dec) override;
    void AbortSlew() override;
    bool CanCheckSlewing() override;
    bool Slewing() override;
    PierSide SideOfPier() override;
};

#endif // GUIDE_ASCOM
#endif // SCOPE_ASCOM_INCLUDED
