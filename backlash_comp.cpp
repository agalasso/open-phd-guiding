/*
*  backlash_comp.cpp
*  PHD Guiding
*
*  Created by Bruce Waddington
*  Copyright (c) 2015 Bruce Waddington and Andy Galasso
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
#include "backlash_comp.h"

#include <algorithm>

static const unsigned int MIN_COMP_AMOUNT = 20;               // min pulse in ms, must be small enough to effectively disable blc
static const unsigned int MAX_COMP_AMOUNT = 8000;             // max pulse in ms

class CorrectionTuple
{
public:
    long timeSeconds;
    double miss;

    CorrectionTuple(long TimeInSecs, double Amount)
    {
        timeSeconds = TimeInSecs;
        miss = Amount;
    }
};

class BLCEvent
{
public:
    std::vector<CorrectionTuple> corrections;
    bool initialOvershoot;
    bool initialUndershoot;
    bool stictionSeen;

    BLCEvent() {};

    BLCEvent(long TimeSecs, double Amount)
    {
        corrections.push_back(CorrectionTuple(TimeSecs, Amount));
        initialOvershoot = false;
        initialUndershoot = false;
        stictionSeen = false;
    }

    size_t InfoCount() const
    {
        return corrections.size();
    }

    void AddEventInfo(long TimeSecs, double Amount, double minMove)
    {
        // Correction[0] is the deflection that triggered the BLC in the first place.  Correction[1] is the first delta after the pulse was issued,
        // Correction[2] is the (optional) subsequent delta, needed to detect stiction
        if (InfoCount() < 3)
        {
            corrections.push_back(CorrectionTuple(TimeSecs, Amount));               // Regardless of size relative to min-move
            if (fabs(Amount) > minMove)
            {
                if (InfoCount() == 2)
                {
                    if (Amount > 0)
                        initialUndershoot = true;
                    else
                        initialOvershoot = true;

                }
                else
                {
                    if (InfoCount() == 3)
                    {
                        stictionSeen = Amount < 0 && corrections[1].miss > 0;           // 2nd follow-on miss was an over-shoot
                    }
                }
            }
        }
    }

};

// Basic operation
// Keep a record of the last <HISTORY_DEPTH> BLC events.  Each event entry holds the initial BLC
// deflection and either one or two immediate follow-on deflections.  The deflections are raw Dec
// amounts, unfiltered by min-move or guiding algorithm decisions. Recording of follow-on
// deflections is determined by windowOpen.  The window is opened when the BLC is initially
// triggered and is then closed if a) A pulse-size adjustment is made based on the first follow-on
// deflection or b) 2 follow-on events have been recorded
// Algorithm behavior for adjusting BLC pulse size is in AdjustmentNeeded()

class BLCHistory
{
    std::vector<BLCEvent> blcEvents;
    int blcIndex = 0;
    const int ENTRY_CAPACITY = 3;
    const unsigned int HISTORY_DEPTH = 10;
    bool windowOpen;
    long timeBase;
    int lastIncrease;

public:
    struct RecentStats
    {
        int shortCount;
        int longCount;
        int stictionCount;
        double avgInitialMiss;
        double avgStictionAmount;

        RecentStats() : shortCount(0), longCount(0), stictionCount(0), avgInitialMiss(0), avgStictionAmount(0)
        {
        }
    };

    bool WindowOpen() const
    {
        return windowOpen;
    }

    BLCHistory()
    {
        windowOpen = false;
        lastIncrease = 0;
        timeBase = wxGetCurrentTime();
    }

    static void LogStatus(const wxString& Msg)
    {
        Debug.Write(wxString::Format("BLC: %s\n", Msg));
    }

    void CloseWindow()
    {
        windowOpen = false;
        Debug.Write("BLC: window closed\n");
    }

    void RecordNewBLCPulse(long when, double triggerDeflection)
    {
        if (blcEvents.size() >= HISTORY_DEPTH)
        {
            blcEvents.erase(blcEvents.begin());
            LogStatus("Oldest BLC event removed");
        }
        blcEvents.push_back(BLCEvent((when - timeBase), triggerDeflection));
        blcIndex = blcEvents.size() - 1;
        windowOpen = true;
    }

    bool AddDeflection(long When, double Amt, double MinMove)
    {
        bool added = false;
        if (blcIndex >= 0 && blcEvents[blcIndex].InfoCount() < ENTRY_CAPACITY)
        {
            blcEvents[blcIndex].AddEventInfo(When-timeBase, Amt, MinMove);
            added = true;
            //LogStatus("Deflection entry added for event " + std::to_string(blcIndex));
        }
        else
        {
            CloseWindow();
        }
        return added;
    }

    void RemoveOldestOvershoots(int howMany)
    {
        for (int ct = 1; ct <= howMany; ct++)
        {
            for (unsigned int inx = 0; inx < blcEvents.size() - 1; inx++)
            {
                if (blcEvents[inx].initialOvershoot)
                {
                    blcEvents.erase(blcEvents.begin() + inx);
                    blcIndex = blcEvents.size() - 1;
                    break;
                }
            }
        }
    }

    void RemoveOldestStictions(int howMany)
    {
        for (int ct = 1; ct <= howMany; ct++)
        {
            for (unsigned int inx = 0; inx < blcEvents.size() - 1; inx++)
            {
                if (blcEvents[inx].stictionSeen)
                {
                    blcEvents.erase(blcEvents.begin() + inx);
                    blcIndex = blcEvents.size() - 1;
                    break;
                }
            }
        }
    }

    void ClearHistory()
    {
        blcEvents.clear();
        CloseWindow();
        LogStatus("History cleared");
    }

    // Stats over some number of recent events, returns the average initial miss
    double GetStats(int numEvents, RecentStats* Results) const
    {
        int bottom = std::max(0, blcIndex - (numEvents - 1));
        double sum = 0;
        double stictionSum = 0;
        int ct = 0;
        for (int inx = blcIndex; inx >= bottom; inx--)
        {
            const BLCEvent& evt = blcEvents[inx];
            if (evt.initialOvershoot)
                Results->longCount++;
            else
                Results->shortCount++;
            if (evt.stictionSeen)
            {
                Results->stictionCount++;
                stictionSum += evt.corrections[2].miss;
            }
            // Average only the initial misses immediately following the blcs
            if (evt.InfoCount() > 1)
            {
                sum += evt.corrections[1].miss;
                ct++;
            }
        }
        if (ct > 0)
            Results->avgInitialMiss = sum / ct;
        else
            Results->avgInitialMiss = 0;
        if (Results->stictionCount > 0)
            Results->avgStictionAmount = stictionSum / Results->stictionCount;
        else
            Results->avgStictionAmount = 0;
        return Results->avgInitialMiss;
    }

    bool AdjustmentNeeded(double miss, double minMove, double yRate, double* correction)
    {
        bool adjust = false;
        const BLCEvent *currEvent;
        RecentStats stats;
        *correction = 0;
        double avgInitMiss = 0;
        if (blcIndex >= 0)
        {
            avgInitMiss = GetStats(HISTORY_DEPTH, &stats);
            currEvent = &blcEvents[blcIndex];
            wxString deflections = " Deflections: 0=" + std::to_string(currEvent->corrections[0].miss) + ", 1:" +
                wxString(std::to_string(currEvent->corrections[1].miss));
            if (currEvent->InfoCount() > 2)
                deflections += ", 2:" + std::to_string(currEvent->corrections[2].miss);
            LogStatus(wxString::Format("History state: CurrMiss=%0.2f, AvgInitMiss=%0.2f, ShCount=%d, LgCount=%d, SticCount=%d, %s",
                miss, stats.avgInitialMiss, stats.shortCount, stats.longCount, stats.stictionCount, deflections));
        }
        else
            return false;

        if (fabs(miss) >= minMove)                      // Most recent miss was big enough to look at
        {
            int corr;
            corr = (int)(floor(abs(avgInitMiss) / yRate) + 0.5);                          // unsigned correction value
            if (miss > 0)
                // UNDER-SHOOT-------------------------------
            {
                if (avgInitMiss > 0)
                {
                    // Might want to increase the blc value - but check for stiction and history of over-corrections
                    // Don't make any changes before getting two follow-on displacements after last BLC
                    if (currEvent->InfoCount() == ENTRY_CAPACITY)
                    {
                        // Check for stiction history
                        if (stats.stictionCount > 2)
                            LogStatus("Under-shoot, no adjustment because of stiction history");
                        else
                        {
                            // Check for over-shoot history
                            if (stats.longCount >= 2)             // 2 or more over-shoots in window
                                LogStatus("Under-shoot; no adjustment because of over-shoot history");
                            else
                            {
                                adjust = true;
                                *correction = corr;
                                lastIncrease = corr;
                                LogStatus("Under-shoot: nominal increase by " + std::to_string(corr));
                            }
                        }
                    }
                    else
                        LogStatus("Under-shoot, no adjustment, waiting for more data");
                }
                else
                {
                    LogStatus("Under-shoot, no adjustment, avgInitialMiss <= 0");
                    CloseWindow();
                }
            }
            else
                // OVER-SHOOT, miss < 0--------------------------------------
            {
                std::string msg = "";
                if (currEvent->stictionSeen)
                {
                    if (stats.stictionCount > 1)          // Seeing and low min-move can look like stiction, don't over-react
                    {
                        msg = "Over-shoot, stiction seen, ";
                        double stictionCorr = (int)(floor(abs(stats.avgStictionAmount) / yRate) + 0.5);
                        *correction = -stictionCorr;
                        RemoveOldestStictions(1);
                        adjust = true;
                        LogStatus(msg + "nominal decrease by " + std::to_string(*correction));
                    }
                    else
                        LogStatus("Over-shoot, first stiction event, no adjustment");
                }
                else if (stats.longCount > stats.shortCount && blcIndex >= 4)
                {
                    msg = "Recent history of over-shoots, ";
                    *correction = -corr;
                    RemoveOldestOvershoots(2);
                    adjust = true;
                    LogStatus(msg + "nominal decrease by " + std::to_string(*correction));
                }
                else if (avgInitMiss <= -0.1)
                {
                    msg = "Average miss indicates over-shooting, ";
                    *correction = -corr;
                    adjust = true;
                    LogStatus(msg + "nominal decrease by " + std::to_string(*correction));
                }
                else
                {
                    correction = 0;
                    std::string msg = "Over-shoot, no adjustment based on avgInitialMiss";
                    LogStatus(msg);
                    CloseWindow();
                }
            }
        }
        else
        {
            LogStatus("No correction, Miss < min_move");
        }

        if (adjust)
            CloseWindow();
        return adjust;
    }
};

BacklashComp::BacklashComp(Scope *scope)
{
    m_pScope = scope;
    m_pHistory = new BLCHistory();
    int lastAmt = pConfig->Profile.GetInt("/" + m_pScope->GetMountClassName() + "/DecBacklashPulse", 0);
    int lastFloor = pConfig->Profile.GetInt("/" + m_pScope->GetMountClassName() + "/DecBacklashFloor", 0);
    int lastCeiling = pConfig->Profile.GetInt("/" + m_pScope->GetMountClassName() + "/DecBacklashCeiling", 0);
    if (lastAmt > 0)
        m_compActive = pConfig->Profile.GetBoolean("/" + m_pScope->GetMountClassName() + "/BacklashCompEnabled", false);
    else
        m_compActive = false;
    SetCompValues(lastAmt, lastFloor, lastCeiling);
    m_lastDirection = NONE;
    if (m_compActive)
        Debug.Write(wxString::Format("BLC: Enabled with correction = %d ms, Floor = %d, Ceiling = %d, %s\n",
        m_pulseWidth, m_adjustmentFloor, m_adjustmentCeiling, m_fixedSize ? "Fixed" : "Adjustable"));
    else
        Debug.Write("BLC: Backlash compensation is disabled\n");
}

BacklashComp::~BacklashComp()
{
    delete m_pHistory;
}

int BacklashComp::GetBacklashPulseMaxValue()
{
    return MAX_COMP_AMOUNT;
}

int BacklashComp::GetBacklashPulseMinValue()
{
    return MIN_COMP_AMOUNT;
}

void BacklashComp::GetBacklashCompSettings(int* pulseWidth, int* floor, int* ceiling) const
{
    *pulseWidth = m_pulseWidth;
    *floor = m_adjustmentFloor;
    *ceiling = m_adjustmentCeiling;
}

// Private method to be sure all comp values are rational and comply with limits
// May change max-move value for Dec depending on the context
void BacklashComp::SetCompValues(int requestedSize, int floor, int ceiling)
{
    m_pulseWidth = wxMax(0, wxMin(requestedSize, MAX_COMP_AMOUNT));
    if (floor > m_pulseWidth || floor < MIN_COMP_AMOUNT)                        // Coming from GA or user input makes no sense
        m_adjustmentFloor = MIN_COMP_AMOUNT;
    else
        m_adjustmentFloor = floor;
    if (ceiling < m_pulseWidth)
        m_adjustmentCeiling = wxMin(1.50 * m_pulseWidth, MAX_COMP_AMOUNT);
    else
        m_adjustmentCeiling = wxMin(ceiling, MAX_COMP_AMOUNT);
    m_fixedSize = abs(m_adjustmentCeiling - m_adjustmentFloor) < MIN_COMP_AMOUNT;
    if (m_pulseWidth > m_pScope->GetMaxDecDuration() && m_compActive)
        m_pScope->SetMaxDecDuration(m_pulseWidth);
}

// Public method to ask for a set of backlash comp settings.  Ceiling == 0 implies compute a default
void BacklashComp::SetBacklashPulseWidth(int ms, int floor, int ceiling)
{
    if (m_pulseWidth != ms || m_adjustmentFloor != floor || m_adjustmentCeiling != ceiling)
    {
        int oldBLC = m_pulseWidth;
        SetCompValues(ms, floor, ceiling);
        pFrame->NotifyGuidingParam("Backlash comp amount", m_pulseWidth);
        Debug.Write(wxString::Format("BLC: Comp pulse set to %d ms, Floor = %d ms, Ceiling = %d ms, %s\n",
            m_pulseWidth, m_adjustmentFloor, m_adjustmentCeiling, m_fixedSize ? "Fixed" : "Adjustable"));
        if (abs(m_pulseWidth - oldBLC) > 100)
        {
            m_pHistory->ClearHistory();
            m_pHistory->CloseWindow();
        }
    }

    pConfig->Profile.SetInt("/" + m_pScope->GetMountClassName() + "/DecBacklashPulse", m_pulseWidth);
    pConfig->Profile.SetInt("/" + m_pScope->GetMountClassName() + "/DecBacklashFloor", m_adjustmentFloor);
    pConfig->Profile.SetInt("/" + m_pScope->GetMountClassName() + "/DecBacklashCeiling", m_adjustmentCeiling);
}

void BacklashComp::EnableBacklashComp(bool enable)
{
    if (m_compActive != enable)
    {
        pFrame->NotifyGuidingParam("Backlash comp enabled", enable);
        if (enable)
            ResetBLCState();
    }

    m_compActive = enable;
    pConfig->Profile.SetBoolean("/" + m_pScope->GetMountClassName() + "/BacklashCompEnabled", m_compActive);
    Debug.Write(wxString::Format("BLC: Backlash comp %s, Comp pulse = %d ms\n", m_compActive ? "enabled" : "disabled", m_pulseWidth));
}

void BacklashComp::ResetBLCState()
{
    if (m_compActive)
    {
        m_lastDirection = NONE;
        m_pHistory->CloseWindow();
        Debug.Write("BLC: Last direction was reset\n");
    }
}

void BacklashComp::TrackBLCResults(unsigned int moveOptions, double yRawOffset)
{
    if (!m_compActive)
        return;

    if (!(moveOptions & MOVEOPT_USE_BLC))
    {
        // Calibration-type move that can move mount in Dec w/out notifying blc about direction
        ResetBLCState();
        return;
    }

    // only track algorithm result moves, do not track "fast
    // recovery after dither" moves or deduced moves or AO bump
    // moves
    bool isAlgoResultMove = (moveOptions & MOVEOPT_ALGO_RESULT) != 0;
    if (!isAlgoResultMove)
    {
        // non-algo blc move occurred before follow-up data were acquired for previous blc
        m_pHistory->CloseWindow();
        return;
    }

    if (!m_pHistory->WindowOpen() || m_fixedSize)
        return;

    // An earlier BLC was applied and we're tracking follow-up results

    // Record the history even if residual error is zero. Sign convention has nothing to do with N or S direction - only whether we
    // needed more correction (+) or less (-)
    GUIDE_DIRECTION dir = yRawOffset > 0.0 ? DOWN : UP;
    double yDistance = fabs(yRawOffset);
    double miss;
    if (dir == m_lastDirection)
        miss = yDistance;                           // + => we needed more of the same, under-shoot
    else
        miss = -yDistance;                         // over-shoot

    double minMove = fmax(m_pScope->GetYGuideAlgorithm()->GetMinMove(), 0.); // Algo w/ no min-move returns -1

    m_pHistory->AddDeflection(wxGetCurrentTime(), miss, minMove);

    double adjustment;
    if (!m_pHistory->AdjustmentNeeded(miss, minMove, m_pScope->MountCal().yRate, &adjustment))
        return;

    int newBLC;
    double nominalBLC = m_pulseWidth + adjustment;
    if (nominalBLC > m_pulseWidth)
    {
        newBLC = ROUND(fmin(m_pulseWidth * 1.1, nominalBLC));
        if (newBLC > m_adjustmentCeiling)
        {
            Debug.Write(wxString::Format("BLC: Pulse increase limited by ceiling of %d\n", m_adjustmentCeiling));
            newBLC = m_adjustmentCeiling;
        }
    }
    else
    {
        newBLC = ROUND(fmax(0.8 * m_pulseWidth, nominalBLC));
        if (newBLC < m_adjustmentFloor)
        {
            Debug.Write(wxString::Format("BLC: Pulse decrease limited by floor of %d\n", m_adjustmentFloor));
            newBLC = m_adjustmentFloor;
        }
    }
    Debug.Write(wxString::Format("BLC: Pulse adjusted to %d\n", newBLC));
    pConfig->Profile.SetInt("/" + m_pScope->GetMountClassName() + "/DecBacklashPulse", newBLC);
    SetCompValues(newBLC, m_adjustmentFloor, m_adjustmentCeiling);
}

void BacklashComp::ApplyBacklashComp(unsigned int moveOptions, double yGuideDistance, int *yAmount)
{
    if (!(moveOptions & MOVEOPT_USE_BLC))
        return;
    if (!m_compActive || m_pulseWidth <= 0 || yGuideDistance == 0.0)
        return;

    GUIDE_DIRECTION dir = yGuideDistance > 0.0 ? DOWN : UP;
    bool isAlgoResultMove = (moveOptions & MOVEOPT_ALGO_RESULT) != 0;

    if (m_lastDirection != NONE && dir != m_lastDirection)
    {
        *yAmount += m_pulseWidth;

        if (isAlgoResultMove)
        {
            // Only track results or make adjustments for algorithm-controlled blc's
            m_pHistory->RecordNewBLCPulse(wxGetCurrentTime(), yGuideDistance);
        }
        else
        {
            m_pHistory->CloseWindow();
            Debug.Write("BLC: Compensation needed for non-algo type move\n");
        }

        Debug.Write(wxString::Format("BLC: Dec direction reversal from %s to %s, backlash comp pulse of %d applied\n",
            m_pScope->DirectionStr(m_lastDirection), m_pScope->DirectionStr(dir), m_pulseWidth));
    }
    else if (!isAlgoResultMove)
    {
        Debug.Write("BLC: non-algo type move will not reverse Dec direction, no blc applied\n");
    }

    m_lastDirection = dir;
}

// Class for implementing the backlash graph dialog
class BacklashGraph : public wxDialog
{
public:
    BacklashGraph(wxDialog *parent, const std::vector<double> &northSteps, const std::vector<double> &southSteps, int PulseSize);
    wxBitmap CreateGraph(int graphicWidth, int graphicHeight, const std::vector<double> &northSteps, const std::vector<double> &southSteps, int PulseSize);
};

BacklashGraph::BacklashGraph(wxDialog *parent, const std::vector<double> &northSteps, const std::vector<double> &southSteps, int PulseSize)
    : wxDialog(parent, wxID_ANY, wxGetTranslation(_("Backlash Results")), wxDefaultPosition, wxSize(500, 400))
{
    // Just but a big button area for the graph with a button below it
    wxBoxSizer *vSizer = new wxBoxSizer(wxVERTICAL);
    // Use a bitmap button so we don't waste cycles in paint events
    wxBitmap graph_bitmap = CreateGraph(450, 300, northSteps, southSteps, PulseSize);
    wxStaticBitmap *graph = new wxStaticBitmap(this, wxID_ANY, graph_bitmap, wxDefaultPosition, wxDefaultSize, 0);
    vSizer->Add(graph, 0, wxALIGN_CENTER_HORIZONTAL | wxALL | wxFIXED_MINSIZE, 5);

    // ok button because we're modal
    vSizer->Add(
        CreateButtonSizer(wxOK),
        wxSizerFlags(0).Expand().Border(wxALL, 10));

    SetSizerAndFit(vSizer);
}

wxBitmap BacklashGraph::CreateGraph(int bmpWidth, int bmpHeight, const std::vector<double> &northSteps, const std::vector<double> &southSteps, int PulseSize)
{
    wxMemoryDC dc;
    wxBitmap bmp(bmpWidth, bmpHeight, -1);
    wxColour decColor = pFrame->pGraphLog->GetDecOrDyColor();
    wxColour idealColor("WHITE");
    wxPen axisPen("GREY", 3, wxPENSTYLE_CROSS_HATCH);
    wxPen decPen(decColor, 3, wxPENSTYLE_SOLID);
    wxPen idealPen(idealColor, 3, wxPENSTYLE_SOLID);
    wxBrush decBrush(decColor, wxBRUSHSTYLE_SOLID);
    wxBrush idealBrush(idealColor, wxBRUSHSTYLE_SOLID);

    double xScaleFactor;
    double yScaleFactor;
    int xOrigin;
    int yOrigin;
    int ptRadius;
    int graphWindowWidth;
    int graphWindowHeight;
    int numNorth;
    double northInc;
    int numSouth;

    // Find the max excursion from the origin in order to scale the points to fit the bitmap
    double maxDec = -9999.0;
    double minDec = 9999.0;
    for (auto it = northSteps.begin(); it != northSteps.end(); ++it)
    {
        maxDec = wxMax(maxDec, *it);
        minDec = wxMin(minDec, *it);
    }

    for (auto it = southSteps.begin(); it != southSteps.end(); ++it)
    {
        maxDec = wxMax(maxDec, *it);
        minDec = wxMin(minDec, *it);
    }

    graphWindowWidth = bmpWidth;
    graphWindowHeight = 0.7 * bmpHeight;
    yScaleFactor = (graphWindowHeight) / (maxDec - minDec + 1);
    xScaleFactor = (graphWindowWidth) / (northSteps.size() + southSteps.size());

    // Since we get mount coordinates, north steps will always be in ascending order
    numNorth = northSteps.size();
    northInc = (northSteps.at(numNorth - 1) - northSteps.at(0)) / numNorth;
    numSouth = southSteps.size();       // May not be the same as numNorth if some sort of problem occurred

    dc.SelectObject(bmp);
    dc.SetBackground(*wxBLACK_BRUSH);

    dc.SetFont(wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    dc.Clear();

    // Bottom and top labels
    xOrigin = graphWindowWidth / 2;
    yOrigin = graphWindowHeight + 40;           // Leave room at the top for labels and such
    dc.SetTextForeground(idealColor);
    dc.DrawText(_("Ideal"), 0.7 * graphWindowWidth, bmpHeight - 25);
    dc.SetTextForeground(decColor);
    dc.DrawText(_("Measured"), 0.2 * graphWindowWidth, bmpHeight - 25);
    dc.DrawText(_("North"), 0.1 * graphWindowWidth, 10);
    dc.DrawText(_("South"), 0.8 * graphWindowWidth, 10);
    if (PulseSize > 0)
    {
        wxString pulseSzTxt = wxString::Format(_("Pulse size = %d ms"), PulseSize);
        int w, h;
        GetTextExtent(pulseSzTxt, &w, &h);

        dc.DrawText(pulseSzTxt, wxMax(0, graphWindowWidth / 2 - w / 2 - 10), yOrigin - (h + 10));
    }
    // Draw the axes
    dc.SetPen(axisPen);
    dc.DrawLine(0, yOrigin, graphWindowWidth, yOrigin);    // x
    dc.DrawLine(xOrigin, yOrigin, xOrigin, 0);             // y

    // Draw the north steps
    dc.SetPen(decPen);
    dc.SetBrush(decBrush);
    ptRadius = 1;

    for (int i = 0; i < numNorth; i++)
    {
        dc.DrawCircle(wxPoint(i * xScaleFactor, round(yOrigin - (northSteps.at(i) - minDec) * yScaleFactor)), ptRadius);
    }

    // Draw the south steps
    for (int i = 0; i < numSouth; i++)
    {
        dc.DrawCircle(wxPoint((i + numNorth) * xScaleFactor, round(yOrigin - (southSteps.at(i) - minDec) * yScaleFactor)), ptRadius);
    }

    // Now show an ideal south recovery line
    dc.SetPen(idealPen);
    dc.SetBrush(idealBrush);

    double peakSouth = southSteps.at(0);
    for (int i = 1; i <= numNorth; i++)
    {
        wxPoint where = wxPoint((i + numNorth)* xScaleFactor, round(yOrigin - (peakSouth - i * northInc - minDec) * yScaleFactor));
        dc.DrawCircle(where, ptRadius);
    }

    dc.SelectObject(wxNullBitmap);
    return bmp;
}

// -------------------  BacklashTool Implementation

BacklashTool::BacklashTool()
{
    m_scope = TheScope();

    m_lastDecGuideRate = GetLastDecGuideRate();     // -1 if we aren't calibrated
    if (m_lastDecGuideRate > 0)
        m_bltState = BLT_STATE_INITIALIZE;
    else
    {
        m_bltState = BLT_STATE_ABORTED;
        m_lastStatus = _("Backlash measurement cannot be run - please re-run your mount calibration");
        Debug.Write("BLT: Could not get calibration data\n");
    }
    m_backlashResultPx = 0;
    m_backlashResultMs = 0;
    m_cumClearingDistance = 0;
    m_backlashExemption = false;
}

BacklashTool::~BacklashTool()
{

}

bool BacklashTool::IsGraphable()
{
    return m_southBLSteps.size() > 0;
}

double BacklashTool::GetLastDecGuideRate()
{
    double rtnVal;
    Calibration lastCalibration;
    m_scope->GetLastCalibration(&lastCalibration);

    if (lastCalibration.isValid)
    {
        rtnVal = lastCalibration.yRate;
    }
    else
    {
        rtnVal = -1;
    }
    return rtnVal;
}

void BacklashTool::StartMeasurement(double DriftPerMin)
{
    m_bltState = BLT_STATE_INITIALIZE;
    m_driftPerSec = DriftPerMin / 60.0;
    m_northBLSteps.clear();
    m_southBLSteps.clear();
    m_northStats.ClearAll();
    DecMeasurementStep(pFrame->pGuider->CurrentPosition());
}

void BacklashTool::StopMeasurement()
{
    m_bltState = BLT_STATE_ABORTED;
    DecMeasurementStep(pFrame->pGuider->CurrentPosition());
}

static bool OutOfRoom(const wxSize& frameSize, double camX, double camY, int margin)
{
    return camX < margin ||
        camY < margin ||
        camX >= frameSize.GetWidth() - margin ||
        camY >= frameSize.GetHeight() - margin;
}

// Measure the apparent backlash by looking at the first south moves, looking to see when the mount moves consistently at the expected rate
// Goal is to establish a good seed value for backlash compensation, not to accurately measure the hardware performance
BacklashTool::MeasurementResults BacklashTool::ComputeBacklashPx(double* bltPx, int* bltMs, double* northRate)
{
    double expectedAmount;
    double expectedMagnitude;
    double earlySouthMoves = 0;
    double blPx = 0;
    double northDelta = 0;
    double driftPxPerFrame;
    double nRate = 0.;
    BacklashTool::MeasurementResults rslt;

    *bltPx = 0;
    *bltMs = 0;
    *northRate = m_lastDecGuideRate;
    if (m_northBLSteps.size() > 3)
    {
        // figure out the drift-related corrections
        double driftAmtPx = m_driftPerSec * (m_msmtEndTime - m_msmtStartTime) / 1000;               // amount of drift in px for entire north measurement period
        int stepCount = m_northStats.GetCount();
        northDelta = m_northStats.GetSum();
        nRate = fabs((northDelta - driftAmtPx) / (stepCount * m_pulseWidth));                       // drift-corrected empirical measure of north rate
        driftPxPerFrame = driftAmtPx / stepCount;
        Debug.Write(wxString::Format("BLT: Drift correction of %0.2f px applied to total north moves of %0.2f px, %0.3f px/frame\n", driftAmtPx, northDelta, driftPxPerFrame));
        Debug.Write(wxString::Format("BLT: Empirical north rate = %.2f px/s \n", nRate * 1000));

        // Compute an expected movement of 90% of the median delta north moves (px).  Use the 90% tolerance to accept situations where the south rate
        // never matches the north rate yet the mount is moving consistently. Allow smoothing for odd mounts that produce sequences of short-long-short-long...
        expectedAmount = 0.9 * m_northStats.GetMedian();
        expectedMagnitude = fabs(expectedAmount);
        int goodSouthMoves = 0;
        double lastSouthMove = 0;
        bool smoothing = false;
        for (int step = 1; step < m_southBLSteps.size(); step++)
        {
            double southMove = m_southBLSteps[step] - m_southBLSteps[step-1];
            earlySouthMoves += southMove;
            if (southMove < 0 && (fabs(southMove) >= expectedMagnitude || fabs(southMove + lastSouthMove / 2.0) > expectedMagnitude))     // Big enough move and in the correct (south) direction
            {
                if (fabs(southMove) < expectedMagnitude)
                    smoothing = true;
                goodSouthMoves++;
                // We want two consecutive south moves that meet or exceed the expected magnitude.  This sidesteps situations where the mount shows a "false start" south
                if (goodSouthMoves == 2)
                {
                    if (smoothing)
                        Debug.Write("BLT: Smoothing applied to south data points\n");
                    // bl = sum(expected moves) - sum(actual moves) - (drift correction for that period)
                    blPx = step * expectedMagnitude - fabs(earlySouthMoves - step * driftPxPerFrame);               // drift-corrected backlash amount
                    if (blPx * nRate < -200)
                        rslt = MEASUREMENT_SANITY;              // large negative number
                    else
                    if (blPx >= 0.7 * northDelta)
                        rslt = MEASUREMENT_TOO_FEW_NORTH;       // bl large compared to total north moves
                    else
                        rslt = MEASUREMENT_VALID;
                    if (blPx < 0)
                    {
                        Debug.Write(wxString::Format("BLT: Negative measurement = %0.2f px, forcing to zero\n", blPx));
                        blPx = 0;
                    }
                    break;
                }
            }
            else
            {
                if (goodSouthMoves > 0)
                    goodSouthMoves--;
            }
            lastSouthMove = southMove;
        }
        if (goodSouthMoves < 2)
            rslt = MEASUREMENT_TOO_FEW_SOUTH;
    }
    else
        rslt = MEASUREMENT_TOO_FEW_NORTH;
    // Update the ref variables
    *bltPx = blPx;
    *bltMs = (int)(blPx / nRate);
    *northRate = nRate;
    return rslt;
}

void BacklashTool::DecMeasurementStep(const PHD_Point& currentCamLoc)
{
    double decDelta = 0.;
    double amt = 0;
    // double fakeDeltas []= {0, -5, -2, 2, 4, 5, 5, 5, 5 };
    PHD_Point currMountLocation;
    double tol;
    try
    {
        if (m_scope->TransformCameraCoordinatesToMountCoordinates(currentCamLoc, currMountLocation))
            throw ERROR_INFO("BLT: CamToMount xForm failed");
        if (m_bltState != BLT_STATE_INITIALIZE)
        {
            decDelta = currMountLocation.Y - m_markerPoint.Y;
            m_cumClearingDistance += decDelta;                                    // use signed value
            //if (m_bltState == BLT_STATE_CLEAR_NORTH)                            // DEBUG ONLY
            //    decDelta = fakeDeltas[wxMin(m_stepCount, 7)];
        }
        Debug.Write("BLT: Entering DecMeasurementStep, state = " + std::to_string(m_bltState) + "\n");
        switch (m_bltState)
        {
        case BLT_STATE_INITIALIZE:
            m_stepCount = 0;
            m_markerPoint = currMountLocation;
            m_startingPoint = currMountLocation;
            // Compute pulse size for clearing backlash - just use the last known guide rate
            if (m_lastDecGuideRate <= 0)
                m_lastDecGuideRate = GetLastDecGuideRate();             // try it again, maybe the user has since calibrated
            if (m_lastDecGuideRate > 0)
            {
                m_pulseWidth = BACKLASH_EXPECTED_DISTANCE * 1.25 / m_lastDecGuideRate;      // px/px_per_ms, bump it to sidestep near misses
                m_acceptedMoves = 0;
                m_lastClearRslt = 0;
                m_cumClearingDistance = 0;
                m_backlashExemption = false;
                m_Rslt = MEASUREMENT_VALID;
                // Get this state machine in synch with the guider state machine - let it drive us, starting with backlash clearing step
                m_bltState = BLT_STATE_CLEAR_NORTH;
                m_scope->SetGuidingEnabled(true);
                pFrame->pGuider->EnableMeasurementMode(true);                   // Measurement results now come to us
            }
            else
            {
                m_bltState = BLT_STATE_ABORTED;
                m_lastStatus = _("Backlash measurement cannot be run - Dec guide rate not available");
                Debug.Write("BLT: Could not get calibration data\n");
            }
            break;

        case BLT_STATE_CLEAR_NORTH:
            // Want to see the mount moving north for 3 consecutive moves of >= expected distance pixels
            if (m_stepCount == 0)
            {
                // Get things moving with the first clearing pulse
                Debug.Write(wxString::Format("BLT starting North backlash clearing using pulse width of %d,"
                    " looking for moves >= %d px\n", m_pulseWidth, BACKLASH_EXPECTED_DISTANCE));
                pFrame->ScheduleAxisMove(m_scope, NORTH, m_pulseWidth, MOVEOPTS_CALIBRATION_MOVE);
                m_stepCount = 1;
                m_lastStatus = wxString::Format(_("Clearing North backlash, step %d"), m_stepCount);
                m_lastStatusDebug = wxString::Format("Clearing North backlash, step %d", m_stepCount);
                break;
            }
            if (fabs(decDelta) >= BACKLASH_EXPECTED_DISTANCE)
            {
                if (m_acceptedMoves == 0 || (m_lastClearRslt * decDelta) > 0)    // Just starting or still moving in same direction
                {
                    m_acceptedMoves++;
                    Debug.Write(wxString::Format("BLT accepted clearing move of %0.2f\n", decDelta));
                }
                else
                {
                    m_acceptedMoves = 0;            // Reset on a direction reversal
                    Debug.Write(wxString::Format("BLT rejected clearing move of %0.2f, direction reversal\n", decDelta));
                }
            }
            else
                Debug.Write(wxString::Format("BLT backlash clearing move of %0.2f px was not large enough\n", decDelta));
            if (m_acceptedMoves < BACKLASH_MIN_COUNT)                    // More work to do
            {
                if (m_stepCount < MAX_CLEARING_STEPS)
                {
                    if (fabs(m_cumClearingDistance) > BACKLASH_EXEMPTION_DISTANCE)
                    {
                        // We moved the mount a substantial distance north but the individual moves were too small - probably a bad calibration,
                        // so let the user proceed with backlash measurement before we push the star too far
                        Debug.Write(wxString::Format("BLT: Cum backlash of %0.2f px is at least half of expected, continue with backlash measurement\n", m_cumClearingDistance));
                        m_backlashExemption = true;
                    }
                    else
                    {
                        if (!OutOfRoom(pCamera->FullSize, currentCamLoc.X, currentCamLoc.Y, pFrame->pGuider->GetMaxMovePixels()))
                        {
                            pFrame->ScheduleAxisMove(m_scope, NORTH, m_pulseWidth, MOVEOPTS_CALIBRATION_MOVE);
                            m_stepCount++;
                            m_markerPoint = currMountLocation;
                            m_lastClearRslt = decDelta;
                            m_lastStatus = wxString::Format(_("Clearing North backlash, step %d (up to limit of %d)"), m_stepCount, MAX_CLEARING_STEPS);
                            m_lastStatusDebug = wxString::Format("Clearing North backlash, step %d (up to limit of %d)", m_stepCount, MAX_CLEARING_STEPS);
                            Debug.Write(wxString::Format("BLT: %s, LastDecDelta = %0.2f px\n", m_lastStatusDebug, decDelta));
                            break;
                        }
                    }
                }
                else
                {
                    m_lastStatus = _("Could not clear North backlash - test failed");
                    m_Rslt = MEASUREMENT_BL_NOT_CLEARED;
                    throw (wxString("BLT: Could not clear north backlash"));
                }
            }
            if (m_acceptedMoves >= BACKLASH_MIN_COUNT || m_backlashExemption || OutOfRoom(pCamera->FullSize, currentCamLoc.X, currentCamLoc.Y, pFrame->pGuider->GetMaxMovePixels()))    // Ok to go ahead with actual backlash measurement
            {
                m_bltState = BLT_STATE_STEP_NORTH;
                double totalBacklashCleared = m_stepCount * m_pulseWidth;
                // Want to move the mount North at >=500 ms, regardless of image scale. But reduce pulse width if it would exceed 80% of the tracking rectangle -
                // need to leave some room for seeing deflections and dec drift
                m_pulseWidth = wxMax((int)NORTH_PULSE_SIZE, m_scope->GetCalibrationDuration());
                m_pulseWidth = wxMin(m_pulseWidth, (int)floor(0.7 * (double)pFrame->pGuider->GetMaxMovePixels() / m_lastDecGuideRate));
                m_stepCount = 0;
                // Move 50% more than the backlash we cleared or >=8 secs, whichever is greater.  We want to leave plenty of room
                // for giving South moves time to clear backlash and actually get moving
                m_northPulseCount = wxMax((MAX_NORTH_PULSES + m_pulseWidth - 1) / m_pulseWidth,
                                          totalBacklashCleared * 1.5 / m_pulseWidth);  // Up to 8 secs

                Debug.Write(wxString::Format("BLT: Starting North moves at Dec=%0.2f\n", currMountLocation.Y));
                m_msmtStartTime = ::wxGetUTCTimeMillis().GetValue();
                // falling through to start moving North
            }

        case BLT_STATE_STEP_NORTH:
            if (m_stepCount < m_northPulseCount && !OutOfRoom(pCamera->FullSize, currentCamLoc.X, currentCamLoc.Y, pFrame->pGuider->GetMaxMovePixels()))
            {
                m_lastStatus = wxString::Format(_("Moving North for %d ms, step %d / %d"), m_pulseWidth, m_stepCount + 1, m_northPulseCount);
                m_lastStatusDebug = wxString::Format("Moving North for %d ms, step %d / %d", m_pulseWidth, m_stepCount + 1, m_northPulseCount);
                double deltaN;
                if (m_stepCount >= 1)
                {
                    deltaN = currMountLocation.Y - m_northBLSteps.back();
                    m_northStats.AddGuideInfo(m_stepCount, deltaN, 0);
                }
                else
                {
                    deltaN = 0;
                    m_markerPoint = currMountLocation;            // Marker point at start of Dec moves North
                }
                Debug.Write(wxString::Format("BLT: %s, DecLoc = %0.2f, DeltaDec = %0.2f\n", m_lastStatusDebug, currMountLocation.Y, deltaN));
                m_northBLSteps.push_back(currMountLocation.Y);
                pFrame->ScheduleAxisMove(m_scope, NORTH, m_pulseWidth, MOVEOPTS_CALIBRATION_MOVE);
                m_stepCount++;
                break;
            }
            else
            {
                // Either got finished or ran out of room
                m_msmtEndTime = ::wxGetUTCTimeMillis().GetValue();
                double deltaN = 0;
                if (m_stepCount >= 1)
                {
                    deltaN = currMountLocation.Y - m_northBLSteps.back();
                    m_northStats.AddGuideInfo(m_stepCount, deltaN, 0);
                }
                Debug.Write(wxString::Format("BLT: North pulses ended at Dec location %0.2f, TotalDecDelta=%0.2f px, LastDeltaDec = %0.2f\n", currMountLocation.Y, decDelta, deltaN));
                m_northBLSteps.push_back(currMountLocation.Y);
                if (m_stepCount < m_northPulseCount)
                {
                    if (m_stepCount < 0.5 * m_northPulseCount)
                    {
                        m_lastStatus = _("Star too close to edge for accurate measurement of backlash. Choose a star farther from the edge.");
                        m_Rslt = MEASUREMENT_TOO_FEW_NORTH;
                        throw (wxString("BLT: Too few north moves"));
                    }
                    Debug.Write("BLT: North pulses truncated, too close to frame edge\n");
                }
                m_northPulseCount = m_stepCount;
                m_stepCount = 0;
                m_bltState = BLT_STATE_STEP_SOUTH;
                // falling through to moving back South
            }

        case BLT_STATE_STEP_SOUTH:
            if (m_stepCount < m_northPulseCount)
            {
                m_lastStatus = wxString::Format(_("Moving South for %d ms, step %d / %d"), m_pulseWidth, m_stepCount + 1, m_northPulseCount);
                m_lastStatusDebug = wxString::Format("Moving South for %d ms, step %d / %d", m_pulseWidth, m_stepCount + 1, m_northPulseCount);
                Debug.Write(wxString::Format("BLT: %s, DecLoc = %0.2f\n", m_lastStatusDebug, currMountLocation.Y));
                m_southBLSteps.push_back(currMountLocation.Y);
                pFrame->ScheduleAxisMove(m_scope, SOUTH, m_pulseWidth, MOVEOPTS_CALIBRATION_MOVE);
                m_stepCount++;
                break;
            }

            // Now see where we ended up - fall through to computing and testing a correction
            Debug.Write(wxString::Format("BLT: South pulses ended at Dec location %0.2f\n", currMountLocation.Y));
            m_southBLSteps.push_back(currMountLocation.Y);
            m_endSouth = currMountLocation;
            m_bltState = BLT_STATE_TEST_CORRECTION;
            m_stepCount = 0;
            // fall through

        case BLT_STATE_TEST_CORRECTION:
            if (m_stepCount == 0)
            {
                m_Rslt = ComputeBacklashPx(&m_backlashResultPx, &m_backlashResultMs, &m_northRate);
                if (m_Rslt != MEASUREMENT_VALID)
                {
                    // Abort the test and show an explanatory status in the GA dialog
                    switch (m_Rslt)
                    {
                    case MEASUREMENT_SANITY:
                        m_lastStatus = _("Dec movements too erratic - test failed");
                        throw (wxString("BLT: Calculation failed sanity check"));
                        break;
                    case MEASUREMENT_TOO_FEW_NORTH:
                        // Don't throw an exception - the test was completed but the bl result is not accurate - handle it in the GA UI
                        break;
                    case MEASUREMENT_TOO_FEW_SOUTH:
                        m_lastStatus = _("Mount never established consistent south moves - test failed");
                        throw (wxString("BLT: Too few acceptable south moves"));
                        break;
                    default:
			            break;
                    }
                }

                double sigmaPx;
                double sigmaMs;
                GetBacklashSigma(&sigmaPx, &sigmaMs);
                Debug.Write(wxString::Format("BLT: Trial backlash amount is %0.2f px, %d ms, sigma = %0.1f px\n", m_backlashResultPx, m_backlashResultMs,
                    sigmaPx));
                if (m_backlashResultMs > 0)
                {
                    // Don't push the guide star outside the tracking region
                    if (m_backlashResultPx < pFrame->pGuider->GetMaxMovePixels())
                    {
                        m_lastStatus = wxString::Format(_("Issuing test backlash correction of %d ms"), m_backlashResultMs);
                        Debug.Write(m_lastStatus + "\n");
                        // This should put us back roughly to where we issued the big North pulse unless the backlash is very large
                        pFrame->ScheduleAxisMove(m_scope, SOUTH, m_backlashResultMs, MOVEOPTS_CALIBRATION_MOVE);
                        m_stepCount++;
                        break;
                    }
                    else
                    {
                        int maxFrameMove = (int)floor((double)0.8 * pFrame->pGuider->GetMaxMovePixels() / m_northRate);
                        Debug.Write(wxString::Format("BLT: Clearing pulse is very large, issuing max S move of %d\n", maxFrameMove));
                        pFrame->ScheduleAxisMove(m_scope, SOUTH, maxFrameMove, MOVEOPTS_CALIBRATION_MOVE); // One more pulse to cycle the state machine
                        m_stepCount = 0;
                        // Can't fine-tune the pulse size, just try to restore the star to < MaxMove of error
                        m_bltState = BLT_STATE_RESTORE;
                        break;
                    }
                }
                else
                {
                    m_bltState = BLT_STATE_RESTORE;
                    m_stepCount = 0;
                    // fall through, no need for test pulse
                }

            }
            // See how close we came, maybe fine-tune a bit
            if (m_bltState == BLT_STATE_TEST_CORRECTION)
            {
                Debug.Write(wxString::Format("BLT: Trial backlash pulse resulted in net DecDelta = %0.2f px, Dec Location %0.2f\n", decDelta, currMountLocation.Y));
                tol = TRIAL_TOLERANCE_AS / pFrame->GetCameraPixelScale();                           // tolerance in units of px
                if (fabs(decDelta) > tol)                                                           // decDelta = (current - markerPoint)
                {
                    double pulse_delta = fabs(currMountLocation.Y - m_endSouth.Y);                  // How far we moved with the test pulse
                    double target_delta = fabs(m_markerPoint.Y - m_endSouth.Y);                     // How far we needed to go
                    if ((m_endSouth.Y - m_markerPoint.Y) * decDelta < 0)                            // Sign change, went too far
                    {
                        //m_backlashResultMs *= target_delta / pulse_delta;
                        Debug.Write(wxString::Format("BLT: Nominal backlash value over-shot by %0.2f X\n", target_delta / pulse_delta));
                    }
                    else
                    {
                        Debug.Write(wxString::Format("BLT: Nominal backlash value under-shot by %0.2f X\n", target_delta / pulse_delta));
                    }
                }
                else
                    Debug.Write(wxString::Format("BLT: Nominal backlash pulse resulted in final delta of %0.1f a-s\n", fabs(decDelta) * pFrame->GetCameraPixelScale()));
            }

            m_bltState = BLT_STATE_RESTORE;
            Debug.Write("BLT: normal result, moving to state=restore\n");
            m_stepCount = 0;
            // fall through

        case BLT_STATE_RESTORE:
            // We could be a considerable distance from where we started, so get back close to the starting point without losing the star
            if (m_stepCount == 0)
            {
                Debug.Write(wxString::Format("BLT: Starting Dec position at %0.2f, Ending Dec position at %0.2f\n", m_markerPoint.Y, currMountLocation.Y));
                amt = fabs(currMountLocation.Y - m_startingPoint.Y);
                if (amt > pFrame->pGuider->GetMaxMovePixels())      // Too big, try to move guide star closer to starting position
                {
                    m_restoreCount = (int)floor((amt / m_northRate) / m_pulseWidth);
                    m_restoreCount = wxMin(m_restoreCount, 10);             // Don't spend forever at it, something probably went wrong
                    Debug.Write(wxString::Format("BLT: Final restore distance is %0.1f px, approx %d steps\n", amt, m_restoreCount));
                    m_stepCount = 0;
                }
                else
                    m_bltState = BLT_STATE_WRAPUP;
            }
            if (m_stepCount < m_restoreCount)
            {

                pFrame->ScheduleAxisMove(m_scope, SOUTH, m_pulseWidth, MOVEOPTS_CALIBRATION_MOVE);
                m_stepCount++;
                m_lastStatus = _("Restoring star position");
                Debug.Write(wxString::Format("BLT: Issuing restore pulse count %d of %d ms\n", m_stepCount, m_pulseWidth));
                break;
            }
            m_bltState = BLT_STATE_WRAPUP;
            Debug.Write("BLT: normal result, moving to state=wrap-up\n");
            // fall through

        case BLT_STATE_WRAPUP:
            m_lastStatus = _("Measurement complete");
            CleanUp();
            m_bltState = BLT_STATE_COMPLETED;
            break;                          // This will cycle the guider state machine and get normal guiding going

        case BLT_STATE_COMPLETED:                           // Shouldn't happen
            break;

        case BLT_STATE_ABORTED:
            m_lastStatus = _("Measurement halted");
            Debug.Write("BLT: measurement process halted by user or by error\n");
            CleanUp();
            break;
        }                       // end of switch on state
    }
    catch (const wxString& msg)
    {
        POSSIBLY_UNUSED(msg);
        Debug.Write(wxString::Format("BLT: Exception thrown in logical state %d\n", (int)m_bltState));
        m_bltState = BLT_STATE_ABORTED;
        Debug.Write("BLT: " + m_lastStatus + "\n");
        CleanUp();
    }

    Debug.Write("BLT: Exiting DecMeasurementStep\n");
}

void BacklashTool::GetBacklashSigma(double* SigmaPx, double* SigmaMs)
{
    if ((m_Rslt == MEASUREMENT_VALID || m_Rslt == BacklashTool::MEASUREMENT_TOO_FEW_NORTH) && m_northStats.GetCount() > 1)
    {
        // Sigma of mean for north moves + sigma of two measurements going south, added in quadrature
        double variance = m_northStats.GetVariance();
        int count = m_northStats.GetCount();
        *SigmaPx = sqrt((variance / count) + (2 * variance / (count - 1)));
        *SigmaMs = *SigmaPx / m_northRate;
    }
    else
    {
        *SigmaPx = 0;
        *SigmaMs = 0;
    }
}

// Launch modal dlg to show backlash test
void BacklashTool::ShowGraph(wxDialog *pGA, const std::vector<double> &northSteps, const std::vector<double> &southSteps, int PulseSize)
{
    BacklashGraph dlg(pGA, northSteps, southSteps, PulseSize);
    dlg.ShowModal();
}

void BacklashTool::CleanUp()
{
    m_scope->GetBacklashComp()->ResetBLCState();        // Normal guiding will start, don't want old BC state applied
    pFrame->pGuider->EnableMeasurementMode(false);
    Debug.Write("BLT: Cleanup completed\n");
}

//------------------------------  End of BacklashTool implementation
