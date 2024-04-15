/*
 *  ascom.cpp
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
 *  Modified by Leo Shatz
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

#ifdef GUIDE_ASCOM

#include "comdispatch.h"

#include <wx/msw/ole/oleutils.h>
#include <comdef.h>
#include <objbase.h>
#include <ole2ver.h>
#include <initguid.h>
#include <wx/textfile.h>
#include <wx/stdpaths.h>
#include <wx/stopwatch.h>

ScopeASCOM::ScopeASCOM(const wxString& choice)
{
    m_choice = choice;
    m_canPulseGuide = false;                           // will get updated in Connect()

    dispid_connected = DISPID_UNKNOWN;
    dispid_ispulseguiding = DISPID_UNKNOWN;
    dispid_isslewing = DISPID_UNKNOWN;
    dispid_pulseguide = DISPID_UNKNOWN;
    dispid_declination = DISPID_UNKNOWN;
    dispid_rightascension = DISPID_UNKNOWN;
    dispid_siderealtime = DISPID_UNKNOWN;
    dispid_sitelatitude = DISPID_UNKNOWN;
    dispid_sitelongitude = DISPID_UNKNOWN;
    dispid_slewtocoordinates = DISPID_UNKNOWN;
    dispid_raguiderate = DISPID_UNKNOWN;
    dispid_decguiderate = DISPID_UNKNOWN;
    dispid_sideofpier = DISPID_UNKNOWN;
    dispid_abortslew = DISPID_UNKNOWN;
    dispid_tracking = DISPID_UNKNOWN;
    dispid_trackingrate = DISPID_UNKNOWN;
}

ScopeASCOM::~ScopeASCOM()
{
}

static wxString displayName(const wxString& ascomName)
{
    if (ascomName.Find(_T("ASCOM")) != wxNOT_FOUND)
        return ascomName;
    return ascomName + _T(" (ASCOM)");
}

// map descriptive name to progid
static std::map<wxString, wxString> s_progid;

wxArrayString ScopeASCOM::EnumAscomScopes()
{
    wxArrayString list;

    try
    {
        DispatchObj profile;
        if (!profile.Create(L"ASCOM.Utilities.Profile"))
            throw ERROR_INFO("ASCOM Scope: could not instantiate ASCOM profile class ASCOM.Utilities.Profile. Is ASCOM installed?");

        Variant res;
        if (!profile.InvokeMethod(&res, L"RegisteredDevices", L"Telescope"))
            throw ERROR_INFO("ASCOM Scope: could not query registered telescope devices: " + ExcepMsg(profile.Excep()));

        DispatchClass ilist_class;
        DispatchObj ilist(res.pdispVal, &ilist_class);

        Variant vcnt;
        if (!ilist.GetProp(&vcnt, L"Count"))
            throw ERROR_INFO("ASCOM Scope: could not query registered telescopes: " + ExcepMsg(ilist.Excep()));

        unsigned int const count = vcnt.intVal;
        DispatchClass kvpair_class;

        for (unsigned int i = 0; i < count; i++)
        {
            Variant kvpres;
            if (ilist.GetProp(&kvpres, L"Item", i))
            {
                DispatchObj kvpair(kvpres.pdispVal, &kvpair_class);
                Variant vkey, vval;
                if (kvpair.GetProp(&vkey, L"Key") && kvpair.GetProp(&vval, L"Value"))
                {
                    wxString ascomName = vval.bstrVal;
                    wxString displName = displayName(ascomName);
                    wxString progid = vkey.bstrVal;
                    s_progid[displName] = progid;
                    list.Add(displName);
                }
            }
        }
    }
    catch (const wxString& msg)
    {
        POSSIBLY_UNUSED(msg);
    }

    return list;
}

bool ScopeASCOM::Create(DispatchObj& obj)
{
    try
    {
        // is there already an instance registered in the global interface table?
        IDispatch *idisp = m_gitEntry.Get();
        if (idisp)
        {
            obj.Attach(idisp, NULL);
            return true;
        }

        Debug.Write(wxString::Format("Create ASCOM Scope: choice '%s' progid %s\n", m_choice, s_progid[m_choice]));

        wxBasicString progid(s_progid[m_choice]);

        if (!obj.Create(progid))
        {
            throw ERROR_INFO("Could not establish instance of " + wxString(progid));
        }

        Debug.Write(wxString::Format("pScopeDriver = 0x%p\n", obj.IDisp()));

        // store the driver interface in the global table for access by other threads
        m_gitEntry.Register(obj);
    }
    catch (const wxString& msg)
    {
        Debug.Write(msg + "\n");
        return false;
    }

    return true;
}

bool ScopeASCOM::HasSetupDialog() const
{
    return true;
}

void ScopeASCOM::SetupDialog()
{
    bool prevRegistered = m_gitEntry.IsRegistered();
    DispatchObj scope;
    if (Create(scope))
    {
        Variant res;
        if (!scope.InvokeMethod(&res, L"SetupDialog"))
        {
            wxString msg(scope.Excep().bstrSource);
            if (scope.Excep().bstrDescription)
                msg += ":\n" + wxString(scope.Excep().bstrDescription);
            wxMessageBox(msg, _("Error"), wxOK | wxICON_ERROR);
        }
    }

    // We may need to de-register from the GIT to reduce the likelhood of getting into a
    // state where the user has killed the ASCOM local server instance and PHD2 is
    // holding a reference to the defunct driver instance in the global interface
    // table
    if (!prevRegistered)
        // But only de-register from the GIT if we happened to have done the registration here
        // in this function call. Otherwise, if the object was already registered at the time
        // we came into this function, de-registration will be taken care of elsewhere.
        m_gitEntry.Unregister();
}

bool ScopeASCOM::Connect()
{
    bool bError = false;
    m_canSetTracking = false;
    for (int i = 0; i < driveMaxRate; i++)
    {
        m_mountRates[i].name = wxEmptyString;
        m_mountRates[i].canSet = false;
    }

    try
    {
        Debug.Write("ASCOM Scope: Connecting\n");

        if (IsConnected())
        {
            wxMessageBox("Scope already connected",_("Error"));
            throw ERROR_INFO("ASCOM Scope: Connected - Already Connected");
        }

        DispatchObj pScopeDriver;
        if (!Create(pScopeDriver))
        {
            wxMessageBox(_T("Could not establish instance of ") + m_choice, _("Error"), wxOK | wxICON_ERROR);
            throw ERROR_INFO("ASCOM Scope: Could not establish ASCOM Scope instance");
        }

        // --- get the dispatch IDs we need ...

        // ... get the dispatch ID for the Connected property ...
        if (!pScopeDriver.GetDispatchId(&dispid_connected, L"Connected"))
        {
            wxMessageBox(_T("ASCOM driver problem -- cannot connect"),_("Error"), wxOK | wxICON_ERROR);
            throw ERROR_INFO("ASCOM Scope: Could not get the dispatch id for the Connected property");
        }

        // ... get the dispatch ID for the "IsPulseGuiding" property ....
        m_canCheckPulseGuiding = true;
        if (!pScopeDriver.GetDispatchId(&dispid_ispulseguiding, L"IsPulseGuiding"))
        {
            m_canCheckPulseGuiding = false;
            Debug.Write("cannot get dispid_ispulseguiding\n");
            // don't fail if we can't get the status on this - can live without it as it's really a safety net for us
        }

        // ... get the dispatch ID for the "Slewing" property ....
        if (!pScopeDriver.GetDispatchId(&dispid_isslewing, L"Slewing"))
        {
            wxMessageBox(_T("ASCOM driver missing the Slewing property"),_("Error"), wxOK | wxICON_ERROR);
            throw ERROR_INFO("ASCOM Scope: Could not get the dispatch id for the Slewing property");
        }

        // ... get the dispatch ID for the "PulseGuide" property ....
        if (!pScopeDriver.GetDispatchId(&dispid_pulseguide, L"PulseGuide"))
        {
            wxMessageBox(_T("ASCOM driver missing the PulseGuide property"),_("Error"), wxOK | wxICON_ERROR);
            throw ERROR_INFO("ASCOM Scope: Could not get the dispatch id for the PulseGuide property");
        }

        // ... get the dispatch ID for the "Declination" property ....
        m_canGetCoordinates = true;
        if (!pScopeDriver.GetDispatchId(&dispid_declination, L"Declination"))
        {
            m_canGetCoordinates = false;
            Debug.Write("cannot get dispid_declination\n");
        }
        else if (!pScopeDriver.GetDispatchId(&dispid_rightascension, L"RightAscension"))
        {
            Debug.Write("cannot get dispid_rightascension\n");
            m_canGetCoordinates = false;
        }
        else if (!pScopeDriver.GetDispatchId(&dispid_siderealtime, L"SiderealTime"))
        {
            Debug.Write("cannot get dispid_siderealtime\n");
            m_canGetCoordinates = false;
        }

        if (!pScopeDriver.GetDispatchId(&dispid_sitelatitude, L"SiteLatitude"))
        {
            Debug.Write("cannot get dispid_sitelatitude\n");
        }
        if (!pScopeDriver.GetDispatchId(&dispid_sitelongitude, L"SiteLongitude"))
        {
            Debug.Write("cannot get dispid_sitelongitude\n");
        }

        m_canSlew = true;
        if (!pScopeDriver.GetDispatchId(&dispid_slewtocoordinates, L"SlewToCoordinates"))
        {
            m_canSlew = false;
            Debug.Write("cannot get dispid_slewtocoordinates\n");
        }

        // ... get the dispatch IDs for the two guide rate properties - if we can't get them, no sweat, doesn't matter for actual guiding
        // Used for things like calibration sanity checking, backlash clearing, etc.
        m_canGetGuideRates = true;         // Likely case, required for any ASCOM driver at V2 or later
        if (!pScopeDriver.GetDispatchId(&dispid_decguiderate, L"GuideRateDeclination"))
        {
            Debug.Write("cannot get dispid_decguiderate\n");
            m_canGetGuideRates = false;
            // don't throw if we can't get this one
        }
        else if (!pScopeDriver.GetDispatchId(&dispid_raguiderate, L"GuideRateRightAscension"))
        {
            Debug.Write("cannot get dispid_raguiderate\n");
            m_canGetGuideRates = false;
            // don't throw if we can't get this one
        }

        if (!pScopeDriver.GetDispatchId(&dispid_sideofpier, L"SideOfPier"))
        {
            Debug.Write("cannot get dispid_sideofpier\n");
            dispid_sideofpier = DISPID_UNKNOWN;
        }

        if (!pScopeDriver.GetDispatchId(&dispid_abortslew, L"AbortSlew"))
        {
            Debug.Write("cannot get dispid_abortslew\n");
            dispid_abortslew = DISPID_UNKNOWN;
        }

        struct ConnectInBg : public ConnectMountInBg
        {
            ScopeASCOM *sa;
            ConnectInBg(ScopeASCOM *sa_) : sa(sa_) { }
            bool Entry()
            {
                GITObjRef scope(sa->m_gitEntry);
                // ... set the Connected property to true....
                if (!scope.PutProp(sa->dispid_connected, true))
                {
                    SetErrorMsg(ExcepMsg(scope.Excep()));
                    return true;
                }
                return false;
            }
        };
        ConnectInBg bg(this);

        // set the Connected property to true in a background thread
        if (bg.Run())
        {
            wxMessageBox(_T("ASCOM driver problem during connection: ") + bg.GetErrorMsg(),
                _("Error"), wxOK | wxICON_ERROR);
            throw ERROR_INFO("ASCOM Scope: Could not set Connected property to true");
        }

        // get the scope name
        Variant vRes;
        if (!pScopeDriver.GetProp(&vRes, L"Name"))
        {
            wxMessageBox(_T("ASCOM driver problem getting Name property"), _("Error"), wxOK | wxICON_ERROR);
            throw ERROR_INFO("ASCOM Scope: Could not get the scope name: " + ExcepMsg(pScopeDriver.Excep()));
        }

        m_Name = displayName(vRes.bstrVal);

        Debug.Write(wxString::Format("Scope reports its name as %s\n", m_Name));

        m_abortSlewWhenGuidingStuck = false;

        if (m_Name.Find(_T("Gemini Telescope .NET")) != wxNOT_FOUND)
        {
            // Gemini2 firmware (2013 Oct 13 version, perhaps others) has been found to contain a
            // bug where a pulse guide command can fail to complete, with the Guiding property
            // returning true forever. The firmware developer suggests that PHD2 should issue an
            // AbortSlew when this condition is detected.
            Debug.Write("ASCOM scope: enabling stuck guide pulse workaround\n");
            m_abortSlewWhenGuidingStuck = true;
        }

        m_checkForSyncPulseGuide = false;

        if (m_Name.Find(_T("AstroPhysicsV2")) != wxNOT_FOUND)
        {
            // The Astro-Physics VB6 driver apparently uses timing functions that fire its COM message pump at inappropriate times.
            // This can cause unpredictable delays in the execution of pulse-guide commands when running on low-end dual-core
            // PCs with other clients also connected to the driver. The problem is exacerbated if the user has switched to "synchronous
            // pulse guiding" (not the default driver setting).  Since the problems can occur regardless of this setting, it just confuses
            // users if we fire an alert - so we will simply log the situation for support purposes.  Fortunately, it happens pretty rarely.
            Debug.Write("ASCOM scope: enabling sync pulse guide check\n");
            m_checkForSyncPulseGuide = true;
        }

        // see if we can pulse guide
        m_canPulseGuide = true;
        if (!pScopeDriver.GetProp(&vRes, L"CanPulseGuide") || vRes.boolVal != VARIANT_TRUE)
        {
            Debug.Write("Connecting to ASCOM scope that does not support PulseGuide\n");
            m_canPulseGuide = false;
        }

        // see if scope can slew
        m_canSlewAsync = false;
        if (m_canSlew)
        {
            if (!pScopeDriver.GetProp(&vRes, L"CanSlew"))
            {
                Debug.Write(wxString::Format("ASCOM scope got error invoking CanSlew: %s\n", ExcepMsg(pScopeDriver.Excep())));
                m_canSlew = false;
            }
            else if (vRes.boolVal != VARIANT_TRUE)
            {
                Debug.Write("ASCOM scope reports CanSlew = false\n");
                m_canSlew = false;
            }

            m_canSlewAsync = pScopeDriver.GetProp(&vRes, L"CanSlewAsync") && vRes.boolVal == VARIANT_TRUE;
            Debug.Write(wxString::Format("ASCOM scope CanSlewAsync is %s\n", m_canSlewAsync ? "true" : "false"));
        }

        if (!pScopeDriver.GetProp(&vRes, L"CanSetTracking"))
        {
            Debug.Write("cannot get CanSetTracking\n");
            m_canSetTracking = false;
        }
        else
        {
            m_canSetTracking = (vRes.boolVal == VARIANT_TRUE);
        }

        if (!pScopeDriver.GetDispatchId(&dispid_trackingrate, L"TrackingRate"))
        {
            Debug.Write("cannot get dispid_trackingrate\n");
            dispid_trackingrate = DISPID_UNKNOWN;
        }

        if (!pScopeDriver.GetDispatchId(&dispid_tracking, L"Tracking"))
        {
            Debug.Write("cannot get dispid_tracking\n");
            dispid_trackingrate = DISPID_UNKNOWN;
        }

        Debug.Write(wxString::Format("%s connected\n", Name()));

        Scope::Connect();

        Debug.Write("ASCOM Scope: Connect success\n");
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    // Enumerate the tracking rates
    EnumerateTrackingRates();

    return bError;
}

bool ScopeASCOM::Disconnect()
{
    bool bError = false;

    try
    {
        Debug.Write("ASCOM Scope: Disconnecting\n");

        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: attempt to disconnect when not connected");
        }

        // Setting the Connected property to false will cause the scope to be disconnected for all
        // ASCOM clients that are connected to the scope, and we do not want this!
        bool const disconnectAscomDriver = false;
        if (disconnectAscomDriver)
        {
            GITObjRef scope(m_gitEntry);

            // Set the Connected property to false
            if (!scope.PutProp(dispid_connected, false))
            {
                pFrame->Alert(_("ASCOM driver problem during disconnect, check the debug log for more information"));
                throw ERROR_INFO("ASCOM Scope: Could not set Connected property to false: " + ExcepMsg(scope.Excep()));
            }
        }

        m_gitEntry.Unregister();

        Debug.Write("ASCOM Scope: Disconnected Successfully\n");
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    Scope::Disconnect();

    return bError;
}

// Enumerate all supported tracking rates
void ScopeASCOM::EnumerateTrackingRates()
{
    // Get all supported tracking rates
    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: attempt to enumeration tracking rates when not connected");
        }

        GITObjRef scope(m_gitEntry);
        Variant vTrackingRates;
        if (scope.GetProp(&vTrackingRates, L"TrackingRates"))
        {
            // Assuming varTrackingRates now holds the IDispatch* to the TrackingRates collection
            IDispatch* pTrackingRates = vTrackingRates.pdispVal;
            DispatchClass iListClass;
            DispatchObj iList(pTrackingRates, &iListClass);

            Variant vCount;
            if (iList.GetProp(&vCount, L"Count"))
            {
                unsigned int const ratesCount = vCount.intVal;
                Debug.Write(wxString::Format("ASCOM scope: reports count=%d of tracking rates\n", ratesCount));
                for (unsigned int i = 1; i <= ratesCount; ++i)
                {
                    Variant vRate;
                    if (iList.GetProp(&vRate, L"Item", i))
                    {
                        enum DriveRates driveRate = (enum DriveRates)vRate.intVal;
                        switch (driveRate)
                        {
                        case driveSidereal:
                            m_mountRates[driveRate].name = _("Sidereal");
                            m_mountRates[driveRate].canSet = true;
                            break;
                        case driveLunar:
                            m_mountRates[driveRate].name = _("Lunar");
                            m_mountRates[driveRate].canSet = true;
                            break;
                        case driveSolar:
                            m_mountRates[driveRate].name = _("Solar");
                            m_mountRates[driveRate].canSet = true;
                            break;
                        case driveKing:
                            m_mountRates[driveRate].name = _("King");
                            m_mountRates[driveRate].canSet = true;
                            break;
                        }
                        Debug.Write(wxString::Format("ASCOM scope: supports tracking rate: %d\n", driveRate));
                    }
                }
            }
            else
            {
                Debug.Write("ASCOM scope: cannot get count of TrackingRates\n");
            }
        }
        else
        {
            Debug.Write("ASCOM scope: cannot get list of TrackingRates\n");
        }

        enum DriveRates savedRate = driveSidereal;
        Debug.Write("ASCOM scope: testing if mount supports setting Sidereal/Lunar/Solar tracking rates\n");
        if (GetTrackingRate(&savedRate, true))
        {
            throw ("ASCOM scope: could not get current tracking rate");
        }

        // If mount doesn't enumerate supported rates, try setting them one by one
        for (int i = driveSidereal; i < driveMaxRate; i++)
        {
            enum DriveRates currRate, testRate;
            bool bErr;
            const int timeout = 3000;
            wxStopWatch sWatch;

            testRate = (enum DriveRates) i;
            Debug.Write(wxString::Format("ASCOM scope: testing tracking rate %d\n", testRate));
            if (!SetTrackingRate(testRate))
            {
                // Wait up to 3 seconds for mount to change tracking rate
                sWatch.Start();
                do
                {
                    wxMilliSleep(100);
                    bErr = GetTrackingRate(&currRate, true);
                } while ((bErr || (currRate != testRate)) && (sWatch.Time() <= timeout));

                if (!bErr && (currRate == testRate))
                {
                    Debug.Write(wxString::Format("ASCOM scope: can get/set tracking rate: %d [t=%d msec]\n", testRate, (int) sWatch.Time()));
                    m_mountRates[i].canSet = true;
                }
            }
            else
            {
                m_mountRates[i].canSet = false;
            }

            switch (testRate)
            {
            case driveSidereal:
                m_mountRates[testRate].name = _("Sidereal");
                break;
            case driveLunar:
                m_mountRates[testRate].name = _("Lunar");
                break;
            case driveSolar:
                m_mountRates[testRate].name = _("Solar");
                break;
            case driveKing:
                if (m_mountRates[testRate].name == wxEmptyString)
                    m_mountRates[testRate].name = _("Custom");
                break;
            }
        }

        // Restore original rate
        Debug.Write(wxString::Format("ASCOM mount: restore saved tracking rate %d\n", savedRate));
        SetTrackingRate(savedRate);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }
}

#define CheckSlewing(dispobj, result) \
    do { \
        if (IsStopGuidingWhenSlewingEnabled() && IsSlewing(dispobj)) \
        { \
            *(result) = MOVE_ERROR_SLEWING; \
            throw ERROR_INFO("attempt to guide while slewing"); \
        } \
    } while (0)

static wxString SlewWarningEnabledKey()
{
    // we want the key to be under "/Confirm" so ConfirmDialog::ResetAllDontAskAgain() resets it, but we also want the setting to be per-profile
    return wxString::Format("/Confirm/%d/SlewWarningEnabled", pConfig->GetCurrentProfileId());
}

static void SuppressSlewAlert(long)
{
    //If the user doesn't want to see these, we shouldn't be checking for the condition
    TheScope()->EnableStopGuidingWhenSlewing(false);
}

static wxString PulseGuideFailedAlertEnabledKey()
{
    // we want the key to be under "/Confirm" so ConfirmDialog::ResetAllDontAskAgain() resets it, but we also want the setting to be per-profile
    return wxString::Format("/Confirm/%d/PulseGuideFailedAlertEnabled", pConfig->GetCurrentProfileId());
}

static void SuppressPulseGuideFailedAlert(long)
{
    pConfig->Global.SetBoolean(PulseGuideFailedAlertEnabledKey(), false);
}

static wxString SyncPulseGuideAlertEnabledKey()
{
    // we want the key to be under "/Confirm" so
    // ConfirmDialog::ResetAllDontAskAgain() resets it, but we also want the
    // setting to be per-profile
    return wxString::Format("/Confirm/%d/SyncPulseGuideAlertEnabled", pConfig->GetCurrentProfileId());
}

static void SuppressSyncPulseGuideAlert(long)
{
    pConfig->Global.SetBoolean(SyncPulseGuideAlertEnabledKey(), false);
}

Mount::MOVE_RESULT ScopeASCOM::Guide(GUIDE_DIRECTION direction, int duration)
{
    MOVE_RESULT result = MOVE_OK;

    try
    {
        Debug.Write(wxString::Format("Guiding  Dir = %d, Dur = %d\n", direction, duration));

        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: attempt to guide when not connected");
        }

        if (!m_canPulseGuide)
        {
            // Could happen if move command is issued on the Aux mount or CanPulseGuide property got changed on the fly
            pFrame->Alert(_("ASCOM driver does not support PulseGuide. Check your ASCOM driver settings."));
            throw ERROR_INFO("ASCOM scope: guide command issued but PulseGuide not supported");
        }

        GITObjRef scope(m_gitEntry);

        // First, check to see if already moving

        CheckSlewing(&scope, &result);

        if (IsGuiding(&scope))
        {
            Debug.Write("Entered PulseGuideScope while moving\n");
            int i;
            for (i = 0; i < 20; i++)
            {
                wxMilliSleep(50);

                CheckSlewing(&scope, &result);

                if (!IsGuiding(&scope))
                    break;

                Debug.Write("Still moving\n");
            }
            if (i == 20)
            {
                Debug.Write("Still moving after 1s - aborting\n");
                throw ERROR_INFO("ASCOM Scope: scope is still moving after 1 second");
            }
            else
            {
                Debug.Write("Movement stopped - continuing\n");
            }
        }

        // Do the move

        VARIANTARG rgvarg[2];
        rgvarg[1].vt = VT_I2;
        rgvarg[1].iVal = direction;
        rgvarg[0].vt = VT_I4;
        rgvarg[0].lVal = (long) duration;

        DISPPARAMS dispParms;
        dispParms.cArgs = 2;
        dispParms.rgvarg = rgvarg;
        dispParms.cNamedArgs = 0;
        dispParms.rgdispidNamedArgs = NULL;

        wxStopWatch swatch;

        HRESULT hr;
        ExcepInfo excep;
        Variant vRes;

        if (FAILED(hr = scope.IDisp()->Invoke(dispid_pulseguide, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
            &dispParms, &vRes, &excep, NULL)))
        {
            Debug.Write(wxString::Format("pulseguide: [%x] %s\n", hr, _com_error(hr).ErrorMessage()));

            // Make sure nothing got by us and the mount can really handle pulse guide - HIGHLY unlikely
            if (scope.GetProp(&vRes, L"CanPulseGuide") && vRes.boolVal != VARIANT_TRUE)
            {
                Debug.Write("Tried to guide mount that has no PulseGuide support\n");
                // This will trigger a nice alert the next time through Guide
                m_canPulseGuide = false;
            }
            throw ERROR_INFO("ASCOM Scope: pulseguide command failed: " + ExcepMsg(excep));
        }

        long elapsed = swatch.Time();

        if (m_checkForSyncPulseGuide)
        {
            // check for a long pulse and an elapsed time close to or longer
            // than the pulse duration
            if (duration >= 250 && elapsed >= duration - 30)
            {
                Debug.Write(wxString::Format("SyncPulseGuide alert: sync pulseguide or slow thread dispatch detected. "
                                             "Duration = %d Elapsed = %ld\n", duration, elapsed));
                // only log the event once
                m_checkForSyncPulseGuide = false;
            }
        }

        if (elapsed < (long)duration)
        {
            unsigned long rem = (unsigned long)((long)duration - elapsed);

            Debug.Write(wxString::Format("PulseGuide returned control before completion, sleep %lu\n", rem + 10));

            if (WorkerThread::MilliSleep(rem + 10))
                throw ERROR_INFO("ASCOM Scope: thread terminate requested");
        }

        if (IsGuiding(&scope))
        {
            Debug.Write("scope still moving after pulse duration time elapsed\n");

            // try waiting a little longer. If scope does not stop moving after 1 second, try doing AbortSlew
            // if it still does not stop after 2 seconds, bail out with an error

            enum { GRACE_PERIOD_MS = 1000,
                   TIMEOUT_MS = GRACE_PERIOD_MS + 1000, };

            bool timeoutExceeded = false;
            bool didAbortSlew = false;

            while (true)
            {
                ::wxMilliSleep(20);

                if (WorkerThread::InterruptRequested())
                    throw ERROR_INFO("ASCOM Scope: thread interrupt requested");

                CheckSlewing(&scope, &result);

                if (!IsGuiding(&scope))
                {
                    Debug.Write(wxString::Format("scope move finished after %ld + %ld ms\n", (long)duration, swatch.Time() - (long)duration));
                    break;
                }

                long now = swatch.Time();

                if (!didAbortSlew && now > duration + GRACE_PERIOD_MS && m_abortSlewWhenGuidingStuck)
                {
                    Debug.Write(wxString::Format("scope still moving after %ld + %ld ms, try aborting slew\n", (long)duration, now - (long)duration));
                    AbortSlew(&scope);
                    didAbortSlew = true;
                    continue;
                }

                if (now > duration + TIMEOUT_MS)
                {
                    timeoutExceeded = true;
                    break;
                }
            }

            if (timeoutExceeded && IsGuiding(&scope))
            {
                throw ERROR_INFO("timeout exceeded waiting for guiding pulse to complete");
            }
        }
    }
    catch (const wxString& msg)
    {
        POSSIBLY_UNUSED(msg);

        if (result == MOVE_OK)
        {
            result = MOVE_ERROR;

            if (!WorkerThread::InterruptRequested())
            {
                pFrame->SuppressableAlert(PulseGuideFailedAlertEnabledKey(), _("PulseGuide command to mount has failed - guiding is likely to be ineffective."),
                    SuppressPulseGuideFailedAlert, 0);
            }
        }
    }

    if (result == MOVE_ERROR_SLEWING)
    {
        pFrame->SuppressableAlert(SlewWarningEnabledKey(), _("Guiding stopped: the scope started slewing."),
            SuppressSlewAlert, 0);
    }

    return result;
}

bool ScopeASCOM::IsGuiding(DispatchObj *scope)
{
    bool bReturn = true;

    try
    {
        if (!m_canCheckPulseGuiding)
        {
            // Assume all is good - best we can do as this is really a fail-safe check.  If we can't call this property (lame driver) guides will have to
            // enforce the wait.  But, enough don't support this that we can't throw an error.
            throw ERROR_INFO("ASCOM Scope: IsGuiding - !m_canCheckPulseGuiding");
        }

        // First, check to see if already moving
        Variant vRes;
        if (!scope->GetProp(&vRes, dispid_ispulseguiding))
        {
            pFrame->Alert(_("ASCOM driver failed checking IsPulseGuiding. See the debug log for more information."));
            throw ERROR_INFO("ASCOM Scope: IsGuiding - IsPulseGuiding failed: " + ExcepMsg(scope->Excep()));
        }

        bReturn = vRes.boolVal == VARIANT_TRUE;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bReturn = false;
    }

    Debug.Write(wxString::Format("IsGuiding returns %d\n", bReturn));

    return bReturn;
}

bool ScopeASCOM::IsSlewing(DispatchObj *scope)
{
    Variant vRes;
    if (!scope->GetProp(&vRes, dispid_isslewing))
    {
        Debug.Write(wxString::Format("ScopeASCOM::IsSlewing failed: %s\n", ExcepMsg(scope->Excep())));
        pFrame->Alert(_("ASCOM driver failed checking for slewing, see the debug log for more information."));
        return false;
    }

    bool result = vRes.boolVal == VARIANT_TRUE;

    Debug.Write(wxString::Format("IsSlewing returns %d\n", result));

    return result;
}

void ScopeASCOM::AbortSlew(DispatchObj *scope)
{
    Debug.Write("ScopeASCOM: AbortSlew\n");
    Variant vRes;
    if (!scope->InvokeMethod(&vRes, dispid_abortslew))
    {
        pFrame->Alert(_("ASCOM driver failed calling AbortSlew, see the debug log for more information."));
    }
}

bool ScopeASCOM::CanCheckSlewing()
{
    return true;
}

bool ScopeASCOM::Slewing()
{
    bool bReturn = true;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: Cannot check Slewing when not connected to mount");
        }

        GITObjRef scope(m_gitEntry);
        bReturn = IsSlewing(&scope);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bReturn = false;
    }

    return bReturn;
}

bool ScopeASCOM::HasNonGuiMove()
{
    return true;
}

// return the declination in radians, or UNKNOWN_DECLINATION
double ScopeASCOM::GetDeclinationRadians()
{
    double dReturn = UNKNOWN_DECLINATION;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get Declination when not connected to mount");
        }

        if (!m_canGetCoordinates)
        {
            throw THROW_INFO("!m_canGetCoordinates");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRes;
        if (!scope.GetProp(&vRes, dispid_declination))
        {
            throw ERROR_INFO("GetDeclinationRadians() fails: " + ExcepMsg(scope.Excep()));
        }

        dReturn = radians(vRes.dblVal);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        m_canGetCoordinates = false;
    }

    Debug.Write(wxString::Format("ScopeASCOM::GetDeclinationRadians() returns %s\n", DeclinationStr(dReturn)));

    return dReturn;
}

bool ScopeASCOM::GetTrackingRate(enum DriveRates* rate, bool verbose)
{
    return GetTrackingRate(rate, nullptr, nullptr, verbose);
}

bool ScopeASCOM::GetTrackingRate(enum DriveRates* rate, double *ra_rate, double *dec_rate, bool verbose)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get tracking rate when not connected");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRes;

        if (!scope.GetProp(&vRes, dispid_trackingrate))
        {
            throw ERROR_INFO("ASCOM Scope: GetTrackingRate() failed: " + ExcepMsg(scope.Excep()));
        }

        *rate = (enum DriveRates) vRes.iVal;

        // EQMOD is known to return bogus rates, so we use offsets to correct them
        if (*rate == driveSidereal)
        {
            if (ra_rate && scope.GetProp(&vRes, L"RightAscensionRate"))
            {
                *ra_rate = vRes.dblVal;
                if (verbose)
                    Debug.Write(wxString::Format("ScopeASCOM::GetTrackingRate() RightAscensionRate=%.9f\n", *ra_rate));
            }
            if (dec_rate && scope.GetProp(&vRes, L"DeclinationRate"))
            {
                *dec_rate = vRes.dblVal;
                if (verbose)
                    Debug.Write(wxString::Format("ScopeASCOM::GetTrackingRate() DeclinationRate=%.9f\n", *dec_rate));
            }
        }
    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    if (verbose)
        Debug.Write(wxString::Format("ScopeASCOM::GetTrackingRate() returns %s, tracking rate = %d\n", bError ? "error" : "success", bError ? 0 : *rate));

    return bError;
}

bool ScopeASCOM::SetTrackingRate(enum DriveRates rate)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot set tracking rate when not connected");
        }

        GITObjRef scope(m_gitEntry);

        if (!scope.PutProp(dispid_trackingrate, (LONG) rate))
        {
            throw ERROR_INFO("ASCOM Scope: SetTrackingRate() failed: " + ExcepMsg(scope.Excep()));
        }
    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    Debug.Write(wxString::Format("ScopeASCOM::SetTrackingRate() returns %s, tracking rate = %d\n", bError ? "error" : "success", bError ? 0 : rate));

    return bError;
}

bool ScopeASCOM::SetTrackingRateOffsets(double raRateOffset, double decRateOffset)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot set tracking rate when not connected");
        }

        GITObjRef scope(m_gitEntry);

        if (!scope.PutProp(L"RightAscensionRate", raRateOffset))
        {
            throw ERROR_INFO("ASCOM Scope: SetTrackingRateOffsets(RightAscensionRate) failed: " + ExcepMsg(scope.Excep()));
        }
        if (!scope.PutProp(L"DeclinationRate", decRateOffset))
        {
            throw ERROR_INFO("ASCOM Scope: SetTrackingRateOffsets(DeclinationRate) failed: " + ExcepMsg(scope.Excep()));
        }
    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    Debug.Write(wxString::Format("ScopeASCOM::SetTrackingRateOffsets() returns %s\n", bError ? "error" : "success"));

    return bError;
}

bool ScopeASCOM::GetTracking(bool* tracking, bool verbose)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get tracking when not connected");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRes;

        if (!scope.GetProp(&vRes, dispid_tracking))
        {
            throw ERROR_INFO("ASCOM Scope: GetTracking() failed: " + ExcepMsg(scope.Excep()));
        }

        *tracking = vRes.bVal;
    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    if (verbose)
        Debug.Write(wxString::Format("ScopeASCOM::GetTracking() returns %s, tracking = %d\n", bError ? "error" : "success", bError ? 0 : *tracking));

    return bError;
}

bool ScopeASCOM::SetTracking(bool tracking)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot set tracking when not connected");
        }

        if (!m_canSetTracking)
        {
            throw THROW_INFO("!m_canSetTracking");
        }

        GITObjRef scope(m_gitEntry);

        if (!scope.PutProp(dispid_tracking, tracking))
        {
            throw ERROR_INFO("ASCOM Scope: SetTracking() failed: " + ExcepMsg(scope.Excep()));
        }
    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    Debug.Write(wxString::Format("ScopeASCOM::SetTracking() returns %s (tracking = %d)\n", bError ? "error" : "success", tracking));

    return bError;
}

bool ScopeASCOM::CanSetTracking()
{
    return m_canSetTracking;
}

// Return RA and Dec guide rates in native ASCOM units, degrees/sec.
// Convention is to return true on an error
bool ScopeASCOM::GetGuideRates(double *pRAGuideRate, double *pDecGuideRate)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get guide rates when not connected");
        }

        if (!m_canGetGuideRates)
        {
            throw THROW_INFO("ASCOM Scope: not capable of getting guide rates");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRes;

        if (!scope.GetProp(&vRes, dispid_decguiderate))
        {
            throw ERROR_INFO("ASCOM Scope: GuideRateDec() failed: " + ExcepMsg(scope.Excep()));
        }

        *pDecGuideRate = vRes.dblVal;

        if (!scope.GetProp(&vRes, dispid_raguiderate))
        {
            throw ERROR_INFO("ASCOM Scope: GuideRateRA() failed: " + ExcepMsg(scope.Excep()));
        }

        *pRAGuideRate = vRes.dblVal;

        if (!ValidGuideRates(*pRAGuideRate, *pDecGuideRate))
        {
            if (!m_bogusGuideRatesFlagged)
            {
                pFrame->Alert(_("The mount's ASCOM driver is reporting invalid guide speeds. Some guiding functions including PPEC will be impaired. Contact the ASCOM driver provider or mount vendor for support."),
                    0, wxEmptyString, 0, 0, true);
                m_bogusGuideRatesFlagged = true;
            }
            throw THROW_INFO("ASCOM Scope: mount reporting invalid guide speeds");
        }

    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    Debug.Write(wxString::Format("ScopeASCOM::GetGuideRates returns %u %.3f %.3f a-s/sec\n", bError,
        bError ? 0.0 : *pDecGuideRate * 3600., bError ? 0.0 : *pRAGuideRate * 3600.));

    return bError;
}

bool ScopeASCOM::GetCoordinates(double *ra, double *dec, double *siderealTime)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get coordinates when not connected");
        }

        if (!m_canGetCoordinates)
        {
            throw THROW_INFO("ASCOM Scope: not capable of getting coordinates");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRA;

        if (!scope.GetProp(&vRA, dispid_rightascension))
        {
            throw ERROR_INFO("ASCOM Scope: get right ascension failed: " + ExcepMsg(scope.Excep()));
        }

        Variant vDec;

        if (!scope.GetProp(&vDec, dispid_declination))
        {
            throw ERROR_INFO("ASCOM Scope: get declination failed: " + ExcepMsg(scope.Excep()));
        }

        Variant vST;

        if (!scope.GetProp(&vST, dispid_siderealtime))
        {
            throw ERROR_INFO("ASCOM Scope: get sidereal time failed: " + ExcepMsg(scope.Excep()));
        }

        *ra = vRA.dblVal;
        *dec = vDec.dblVal;
        *siderealTime = vST.dblVal;
    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    return bError;
}

bool ScopeASCOM::GetSiteLatLong(double *latitude, double *longitude)
{
    if (dispid_sitelatitude == DISPID_UNKNOWN || dispid_sitelongitude == DISPID_UNKNOWN)
        return true;

    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get site latitude/longitude when not connected");
        }

        GITObjRef scope(m_gitEntry);

        Variant vLat;

        if (!scope.GetProp(&vLat, dispid_sitelatitude))
        {
            throw ERROR_INFO("ASCOM Scope: get site latitude failed: " + ExcepMsg(scope.Excep()));
        }

        Variant vLong;

        if (!scope.GetProp(&vLong, dispid_sitelongitude))
        {
            throw ERROR_INFO("ASCOM Scope: get site longitude failed: " + ExcepMsg(scope.Excep()));
        }

        *latitude = vLat.dblVal;
        *longitude = vLong.dblVal;
    }
    catch (const wxString& Msg)
    {
        bError = true;
        POSSIBLY_UNUSED(Msg);
    }

    return bError;
}

bool ScopeASCOM::CanSlew()
{
    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get CanSlew property when not connected to mount");
        }

        return m_canSlew;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        return false;
    }
}

bool ScopeASCOM::CanSlewAsync()
{
    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get CanSlewAsync property when not connected to mount");
        }

        return m_canSlewAsync;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        return false;
    }
}

bool ScopeASCOM::CanReportPosition()
{
    return true;
}

bool ScopeASCOM::CanPulseGuide()
{
    return m_canPulseGuide;
}

bool ScopeASCOM::SlewToCoordinates(double ra, double dec)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot slew when not connected");
        }

        if (!m_canSlew)
        {
            throw THROW_INFO("ASCOM Scope: not capable of slewing");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRes;

        if (!scope.InvokeMethod(&vRes, dispid_slewtocoordinates, ra, dec))
        {
            throw ERROR_INFO("ASCOM Scope: slew to coordinates failed");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool ScopeASCOM::SlewToCoordinatesAsync(double ra, double dec)
{
    bool bError = false;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot slew when not connected");
        }

        if (!m_canSlewAsync)
        {
            throw THROW_INFO("ASCOM Scope: not capable of async slewing");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRes;

        if (!scope.InvokeMethod(&vRes, L"SlewToCoordinatesAsync", ra, dec))
        {
            throw ERROR_INFO("ASCOM Scope: async slew to coordinates failed");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void ScopeASCOM::AbortSlew()
{
    GITObjRef scope(m_gitEntry);
    Variant vRes;
    scope.InvokeMethod(&vRes, L"AbortSlew");
}

PierSide ScopeASCOM::SideOfPier()
{
    PierSide pierSide = PIER_SIDE_UNKNOWN;

    try
    {
        if (!IsConnected())
        {
            throw ERROR_INFO("ASCOM Scope: cannot get side of pier when not connected");
        }

        if (dispid_sideofpier == DISPID_UNKNOWN)
        {
            throw THROW_INFO("ASCOM Scope: not capable of getting side of pier");
        }

        GITObjRef scope(m_gitEntry);

        Variant vRes;

        if (!scope.GetProp(&vRes, dispid_sideofpier))
        {
            throw ERROR_INFO("ASCOM Scope: SideOfPier failed: " + ExcepMsg(scope.Excep()));
        }

        switch (vRes.intVal) {
        case 0: pierSide = PIER_SIDE_EAST; break;
        case 1: pierSide = PIER_SIDE_WEST; break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    Debug.Write(wxString::Format("ScopeASCOM::SideOfPier() returns %d\n", pierSide));

    return pierSide;
}

#endif /* GUIDE_ASCOM */
