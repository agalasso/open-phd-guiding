/*
 *  guidinglog.cpp
 *  PHD Guiding
 *
 *  Created by Bret McKee
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

#include "phd.h"

#include <wx/wfstream.h>
#include <wx/txtstrm.h>

#define GUIDELOG_VERSION _T("2.5")

const int RetentionPeriod = 60;

GuidingLog::GuidingLog()
    :
    m_enabled(false),
    m_keepFile(false),
    m_isGuiding(false)
{
}

GuidingLog::~GuidingLog()
{
}

static wxString PierSideStr(PierSide p)
{
    switch (p)
    {
    case PIER_SIDE_EAST: return "East";
    case PIER_SIDE_WEST: return "West";
    default:             return "Unknown";
    }
}

static wxString ParityStr(int p)
{
    switch (p)
    {
    case GUIDE_PARITY_EVEN: return "Even";
    case GUIDE_PARITY_ODD:  return "Odd";
    default:                return "N/A";
    }
}

static double HourAngle(double ra, double lst)
{
    return norm(lst - ra, -12.0, 12.0);
}

static wxString RotatorPosStr()
{
    if (!pRotator)
        return "N/A";
    double pos = Rotator::RotatorPosition();
    if (pos == Rotator::POSITION_UNKNOWN)
        return "Unknown";
    else
        return wxString::Format("%.1f", norm(pos, 0.0, 360.0));
}

// Angles in degrees, HA in hours, results in degrees
static void GetAltAz(double Latitude, double HA, double Dec, double& Altitude, double& Azimuth)
{
    const double HrsToRad = 0.2617993881;
    // Get altitude first
    // sin(Alt) = cos(HA)cos(Dec)cos(Lat) + sin(Dec)sin(Lat)
    double decRadians = radians(Dec);
    double latRadians = radians(Latitude);
    double haRadians = HA * HrsToRad;
    double altRadians;
    double sinAlt = cos(haRadians) * cos(decRadians) * cos(latRadians) + sin(decRadians) * sin(latRadians);
    altRadians = asin(sinAlt);
    Altitude = degrees(asin(sinAlt));
    // Special handling if we're very close to zenith (very small zenith angle)
    double zenDist = acos(sinAlt);
    double zSin = sin(zenDist);
    if (fabs(zSin) < 1e-5)
    {
        Altitude = 90.0;
        // Azimuth values are arbitrary in this situation
        if (sin(haRadians) >= 0)
            Azimuth = 270;
        else
            Azimuth = 90;
        return;
    }

    // Now get azimuth - alternative formula using zSin avoids div-by-zero conditions
    double As = cos(decRadians) * sin(haRadians) / zSin;
    double Ac = (sin(latRadians) * cos(decRadians) * cos(haRadians) - cos(latRadians) *
        sin(decRadians)) / zSin;
    // atan2 doesn't want both params = 0
    if (Ac == 0.0 && As == 0.0)
    {
        if (Dec > 0)
            Azimuth = 180.0;
        else
            Azimuth = 0.0;
        return;
    }
    // arctan2 handles quadrants automatically but returns result in the range of -180 to +180 - we want 0 to 360
    double altPrime = atan2(As, Ac);
    Azimuth = degrees(altPrime) + 180.0;
}

static wxString PointingInfo()
{
    double cur_ra, cur_dec, cur_st;
    double latitude, longitude;
    double alt;
    double az;
    double ha;
    wxString rslt;
    bool pointingError = false;
    if (pPointingSource && !pPointingSource->GetCoordinates(&cur_ra, &cur_dec, &cur_st))
    {
        ha = HourAngle(cur_ra, cur_st);
        rslt = wxString::Format("RA = %0.2f hr, Dec = %0.1f deg, Hour angle = %0.2f hr, Pier side = %s, Rotator pos = %s, ",
            cur_ra, cur_dec, ha, PierSideStr(pPointingSource->SideOfPier()), RotatorPosStr());
    }
    else
    {
        rslt = wxString::Format("RA/Dec = Unknown, Hour angle = Unknown, Pier side = Unknown, Rotator pos = %s, ", RotatorPosStr());
        pointingError = true;
    }
    if (pPointingSource && !pointingError && !pPointingSource->GetSiteLatLong(&latitude, &longitude))
    {
        GetAltAz(latitude, ha, cur_dec, alt, az);
    }
    if (!pointingError)
        rslt += wxString::Format("Alt = %0.1f deg, Az = %0.1f deg", alt, az);
    else
        rslt += "Alt = Unknown, Az = Unknown";

    return rslt;
}

static void GuidingHeader(wxFFile& file)
// output guiding header to log file
{
    file.Write("Equipment Profile = " + pConfig->GetCurrentProfile() + "\n");

    file.Write(pFrame->GetSettingsSummary());
    file.Write(pFrame->pGuider->GetSettingsSummary());

    if (pCamera)
    {
        file.Write(pCamera->GetSettingsSummary());
        file.Write("Exposure = " + pFrame->ExposureDurationSummary() + "\n");
    }

    if (pMount)
        file.Write(pMount->GetSettingsSummary());

    if (pSecondaryMount)
        file.Write(pSecondaryMount->GetSettingsSummary());

    file.Write(PointingInfo());
    file.Write("\n");

    const Star& star = pFrame->pGuider->PrimaryStar();

    file.Write(wxString::Format("Lock position = %.3f, %.3f, Star position = %.3f, %.3f, HFD = %.2f px\n",
                                pFrame->pGuider->LockPosition().X,
                                pFrame->pGuider->LockPosition().Y,
                                pFrame->pGuider->CurrentPosition().X,
                                pFrame->pGuider->CurrentPosition().Y,
                                star.HFD));

    file.Write("Frame,Time,mount,dx,dy,RARawDistance,DECRawDistance,RAGuideDistance,DECGuideDistance,"
               "RADuration,RADirection,DECDuration,DECDirection,XStep,YStep,StarMass,SNR,ErrorCode\n");
}

static void WriteSummaryInfo(wxFFile& file, const GuideLogSummaryInfo& summary)
{
    if (!summary.valid)
        return;

    file.Write(wxString::Format("Log Summary: calcnt:%u gcnt:%u gdur:%.f gacnt:%u\n",
                                summary.cal_cnt, summary.guide_cnt, summary.guide_dur,
                                summary.ga_cnt));
}

void GuideLogSummaryInfo::LoadSummaryInfo(wxFFile& file)
{
    Clear();

    wxFFileInputStream is(file);
    if (is.IsOk())
    {
        struct RestorePos {
            wxFFile& f;
            wxFileOffset o;
            RestorePos(wxFFile& f_) : f(f_) { o = f.Tell(); }
            ~RestorePos() { try { f.Seek(o); } catch (...) {} }
        } restore(file);
        wxFileOffset ofs = wxMax(file.Length() - 128, 0LL);
        is.SeekI(ofs);
        wxTextInputStream tis(is, wxS(" "), wxMBConvUTF8());
        while (!is.Eof())
        {
            wxString s = tis.ReadLine();
            if (s.StartsWith(wxS("Log Summary: ")))
            {
                size_t pos;
                wxStringCharType *e;
                if ((pos = s.find(wxS("calcnt:"))) != wxString::npos)
                    cal_cnt = wxStrtoul(s.substr(pos + 7), &e, 10);
                if ((pos = s.find(wxS("gcnt:"))) != wxString::npos)
                    guide_cnt = wxStrtoul(s.substr(pos + 5), &e, 10);
                if ((pos = s.find(wxS("gdur:"))) != wxString::npos)
                    guide_dur = wxStrtod(s.substr(pos + 5), &e);
                if ((pos = s.find(wxS("gacnt:"))) != wxString::npos)
                    ga_cnt = wxStrtoul(s.substr(pos + 6), &e, 10);
                valid = true;
                return;
            }
        }
    }
}

void GuidingLog::EnableLogging()
{
    if (m_enabled)
        return;

    try
    {
        const wxDateTime& logFileTime = wxGetApp().GetLogFileTime();
        if (!m_file.IsOpened())
        {
            m_fileName = GetLogDir() + PATHSEPSTR + logFileTime.Format(_T("PHD2_GuideLog_%Y-%m-%d_%H%M%S.txt"));

            if (!m_file.Open(m_fileName, "a+"))
            {
                throw ERROR_INFO("unable to open file");
            }
            if (m_file.Length() > 0)
            {
                m_keepFile = true;
                m_summary.LoadSummaryInfo(m_file);
            }
            else
            {
                // starting a new log, don't keep it until something meaningful is logged
                m_keepFile = false;
                m_summary.valid = true;
            }
        }

        assert(m_file.IsOpened());

        m_file.Write(_T("PHD2 version ") FULLVER _T(" [") PHD_OSNAME _T("]") _T(", Log version ") GUIDELOG_VERSION _T(". Log enabled at ") +
            logFileTime.Format(_T("%Y-%m-%d %H:%M:%S")) + "\n");

        m_enabled = true;

        // persist state
        pConfig->Global.SetBoolean("/LoggingMode", m_enabled);

        // dump guiding header if logging enabled during guide
        if (pFrame && pFrame->pGuider->IsGuiding())
            GuidingHeader(m_file);

        Flush();
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }
}

void GuidingLog::EnableLogging(bool enable)
{
    if (enable)
        EnableLogging();
    else
        DisableLogging();
}

void GuidingLog::DisableLogging()
{
    if (!m_enabled)
        return;

    if (m_file.IsOpened())
    {
        wxDateTime now = wxDateTime::Now();

        m_file.Write("\n");
        m_file.Write("Log disabled at " + now.Format(_T("%Y-%m-%d %H:%M:%S")) + "\n");
        Flush();
    }

    m_enabled = false;

    // persist state
    pConfig->Global.SetBoolean("/LoggingMode", m_enabled);
}

bool GuidingLog::ChangeDirLog(const wxString& newdir)
{
    bool enabled = IsEnabled();
    bool ok = true;

    CloseGuideLog();

    if (!SetLogDir(newdir))
    {
        wxMessageBox(wxString::Format("invalid folder name %s, log folder unchanged", newdir));
        ok = false;
    }

    if (enabled)             // if SetLogDir failed, no harm no foul, stay with original. Otherwise
    {
        EnableLogging();     // start fresh...
    }

    return ok;
}

void GuidingLog::RemoveOldFiles()
{
    Logger::RemoveMatchingFiles("PHD2_GuideLog*.txt", RetentionPeriod);
}

bool GuidingLog::Flush()
{
    if (!m_enabled)
        return false;

    bool error = false;

    try
    {
        assert(m_file.IsOpened());

        if (!m_file.Flush())
        {
            throw ERROR_INFO("unable to flush file");
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        error = true;
    }

    return error;
}

void GuidingLog::CloseGuideLog()
{
    if (m_file.IsOpened())
    {
        if (m_keepFile)
        {
            wxDateTime now = wxDateTime::Now();

            m_file.Write("\n");
            WriteSummaryInfo(m_file, m_summary);
            m_file.Write("Log closed at " + now.Format(_T("%Y-%m-%d %H:%M:%S")) + "\n");
            Flush();
        }

        m_file.Close();
    }

    m_enabled = false;

    if (!m_keepFile)            // Delete the file if nothing useful was logged
    {
        wxRemove(m_fileName);
    }
}

void GuidingLog::StartCalibration(const Mount *pCalibrationMount)
{
    m_isGuiding = true;

    if (!m_enabled)
        return;

    assert(m_file.IsOpened());
    wxDateTime now = wxDateTime::Now();

    m_file.Write("\n");
    m_file.Write("Calibration Begins at " + now.Format(_T("%Y-%m-%d %H:%M:%S")) + "\n");
    m_file.Write("Equipment Profile = " + pConfig->GetCurrentProfile() + "\n");

    m_file.Write(pFrame->GetSettingsSummary());
    m_file.Write(pFrame->pGuider->GetSettingsSummary());

    if (pCamera)
    {
        m_file.Write(pCamera->GetSettingsSummary());
        m_file.Write("Exposure = " + pFrame->ExposureDurationSummary() + "\n");
    }

    assert(pCalibrationMount && pCalibrationMount->IsConnected());

    m_file.Write("Mount = " + pCalibrationMount->Name());
    wxString calSettings = pCalibrationMount->CalibrationSettingsSummary();
    if (!calSettings.IsEmpty())
        m_file.Write(", " + calSettings);
    m_file.Write("\n");

    m_file.Write(PointingInfo());
    m_file.Write("\n");

    const Star& star = pFrame->pGuider->PrimaryStar();

    m_file.Write(wxString::Format("Lock position = %.3f, %.3f, Star position = %.3f, %.3f, HFD = %.2f px\n",
                pFrame->pGuider->LockPosition().X,
                pFrame->pGuider->LockPosition().Y,
                pFrame->pGuider->CurrentPosition().X,
                pFrame->pGuider->CurrentPosition().Y,
                star.HFD));

    m_file.Write("Direction,Step,dx,dy,x,y,Dist\n");

    Flush();

    m_keepFile = true;
}

void GuidingLog::CalibrationFailed(const Mount *pCalibrationMount, const wxString& msg)
{
    m_isGuiding = false;

    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    m_file.Write(msg); m_file.Write("\n");
    Flush();
}

void GuidingLog::CalibrationStep(const CalibrationStepInfo& info)
{
    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    // Direction,Step,dx,dy,x,y,Dist
    m_file.Write(wxString::Format("%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n",
        info.direction,
        info.stepNumber,
        info.dx, info.dy,
        info.pos.X, info.pos.Y,
        info.dist));

    Flush();
}

void GuidingLog::CalibrationDirectComplete(const Mount *pCalibrationMount, const wxString& direction, double angle, double rate, int parity)
{
    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    m_file.Write(wxString::Format("%s calibration complete. Angle = %.1f deg, Rate = %.3f px/sec, Parity = %s\n",
        direction, degrees(angle), rate * 1000.0, ParityStr(parity)));

    Flush();
}

void GuidingLog::CalibrationComplete(const Mount *pCalibrationMount)
{
    m_isGuiding = false;

    if (!m_enabled)
        return;

    ++m_summary.cal_cnt;

    assert(m_file.IsOpened());

    m_file.Write(wxString::Format("Calibration complete, mount = %s.\n", pCalibrationMount->Name()));

    Flush();
}

void GuidingLog::GuidingStarted()
{
    m_isGuiding = true;

    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    m_file.Write("\n");
    m_file.Write("Guiding Begins at " + pFrame->m_guidingStarted.Format(_T("%Y-%m-%d %H:%M:%S")) + "\n");

    // add common guiding header
    GuidingHeader(m_file);

    Flush();

    m_keepFile = true;
}

void GuidingLog::GuidingStopped()
{
    m_isGuiding = false;

    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    ++m_summary.guide_cnt;
    m_summary.guide_dur += pFrame->TimeSinceGuidingStarted();

    m_file.Write("Guiding Ends at " + wxDateTime::Now().Format(_T("%Y-%m-%d %H:%M:%S")) + "\n");
    Flush();
}

void GuidingLog::GuideStep(const GuideStepInfo& step)
{
    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    m_file.Write(wxString::Format("%d,%.3f,\"%s\",%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,",
        step.frameNumber, step.time,
        step.mount->IsStepGuider() ? "AO" : "Mount",
        step.cameraOffset.X, step.cameraOffset.Y,
        step.mountOffset.X, step.mountOffset.Y,
        step.guideDistanceRA, step.guideDistanceDec));

    if (step.mount->IsStepGuider())
    {
        int xSteps = step.directionRA == LEFT ? -step.durationRA : step.durationRA;
        int ySteps = step.directionDec == DOWN ? -step.durationDec : step.durationDec;
        m_file.Write(wxString::Format(",,,,%d,%d,", xSteps, ySteps));
    }
    else
    {
        m_file.Write(wxString::Format("%d,%s,%d,%s,,,",
            step.durationRA, step.durationRA > 0 ? step.mount->DirectionChar((GUIDE_DIRECTION)step.directionRA) : "",
            step.durationDec, step.durationDec > 0 ? step.mount->DirectionChar((GUIDE_DIRECTION)step.directionDec): ""));
    }

    m_file.Write(wxString::Format("%.f,%.2f,%d\n",
            step.starMass, step.starSNR, step.starError));

    Flush();
}

void GuidingLog::FrameDropped(const FrameDroppedInfo& info)
{
    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    m_file.Write(wxString::Format("%d,%.3f,\"DROP\",,,,,,,,,,,,,%.f,%.2f,%d,\"%s\"\n",
        info.frameNumber, info.time, info.starMass, info.starSNR, info.starError, info.status));

    Flush();
}

void GuidingLog::CalibrationFrameDropped(const FrameDroppedInfo& info)
{
    if (!m_enabled)
        return;

    assert(m_file.IsOpened());

    m_file.Write(wxString::Format("INFO: STAR LOST during calibration, Mass= %.f, SNR= %.2f, Error= %d, Status=%s\n",
        info.starMass, info.starSNR, info.starError, info.status));

    Flush();
}

void GuidingLog::NotifyGuidingDithered(Guider *guider, double dx, double dy)
{
    if (!m_enabled || !m_isGuiding)
        return;

    m_file.Write(wxString::Format("INFO: DITHER by %.3f, %.3f, new lock pos = %.3f, %.3f\n",
        dx, dy, guider->LockPosition().X, guider->LockPosition().Y));

    Flush();
}

void GuidingLog::NotifySettlingStateChange(const wxString& msg)
{
    if (!m_enabled)
        return;
    m_file.Write(wxString::Format("INFO: SETTLING STATE CHANGE, %s\n", msg));
    Flush();
}

void GuidingLog::NotifyGACompleted()
{
    if (!m_enabled)
        return;

    ++m_summary.ga_cnt;
}

void GuidingLog::NotifyGAResult(const wxString& msg)
{
    if (!m_enabled)
        return;

    // Client needs to handle end-of-line formatting
    m_file.Write(wxString::Format("INFO: GA Result - %s", msg));
    Flush();
}

void GuidingLog::NotifySetLockPosition(Guider *guider)
{
    if (!m_enabled || !m_isGuiding)
        return;

    m_file.Write(wxString::Format("INFO: SET LOCK POSITION, new lock pos = %.3f, %.3f\n",
        guider->LockPosition().X, guider->LockPosition().Y));

    Flush();

    m_keepFile = true;
}

void GuidingLog::NotifyLockShiftParams(const LockPosShiftParams& shiftParams, const PHD_Point& cameraRate)
{
    if (!m_enabled || !m_isGuiding)
        return;

    wxString details;
    if (shiftParams.shiftEnabled)
    {
        details = wxString::Format("%s rate (%.2f,%.2f) %s/hr (%.2f,%.2f) px/hr",
                                    shiftParams.shiftIsMountCoords ? "RA,Dec" : "X,Y",
                                    shiftParams.shiftRate.IsValid() ? shiftParams.shiftRate.X : 0.0,
                                    shiftParams.shiftRate.IsValid() ? shiftParams.shiftRate.Y : 0.0,
                                    shiftParams.shiftUnits == UNIT_ARCSEC ? "arc-sec" : "pixels",
                                    cameraRate.IsValid() ? cameraRate.X * 3600.0 : 0.0,
                                    cameraRate.IsValid() ? cameraRate.Y * 3600.0 : 0.0);
    }

    m_file.Write(wxString::Format("INFO: LOCK SHIFT, enabled = %d %s\n", shiftParams.shiftEnabled, details));
    Flush();

    m_keepFile = true;
}

void GuidingLog::ServerCommand(Guider *guider, const wxString& cmd)
{
    if (!m_enabled || !m_isGuiding)
        return;

    m_file.Write(wxString::Format("INFO: Server received %s\n", cmd));
    Flush();

    m_keepFile = true;
}

void GuidingLog::NotifyManualGuide(const Mount *mount, int direction, int duration)
{
    if (!m_enabled || !m_isGuiding)
        return;

    m_file.Write(wxString::Format("INFO: Manual guide (%s) %s %d %s\n",
                                  mount->IsStepGuider() ? "AO" : "Mount",
                                  mount->DirectionStr(static_cast<GUIDE_DIRECTION>(direction)), duration,
                                  mount->IsStepGuider() ? (duration != 1 ? "steps" : "step") : "ms"));
    Flush();

    m_keepFile = true;
}

void GuidingLog::SetGuidingParam(const wxString& name, double val)
{
    SetGuidingParam(name, wxString::Format("%.2f", val));
}

void GuidingLog::SetGuidingParam(const wxString& name, int val)
{
    SetGuidingParam(name, wxString::Format("%d", val));
}

void GuidingLog::SetGuidingParam(const wxString& name, bool val)
{
    SetGuidingParam(name, wxString(val ? "true" : "false"));
}

void GuidingLog::SetGuidingParam(const wxString& name, const wxString& val)
{
    if (!m_enabled || !m_isGuiding)
        return;

    m_file.Write(wxString::Format("INFO: Guiding parameter change, %s = %s\n", name, val));
    Flush();

    m_keepFile = true;
}

void GuidingLog::SetGuidingParam(const wxString& name, const wxString& val, bool AlwaysLog)
{
    m_file.Write(wxString::Format("INFO: Guiding parameter change, %s = %s\n", name, val));
    Flush();

    m_keepFile = true;
}
