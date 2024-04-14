/*
*  aui_controls.cpp
*  PHD Guiding
*
*  Created by Bruce Waddington
*  Copyright (c) 2016 Bruce Waddington and Andy Galasso
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
#include "aui_controls.h"

#include <algorithm>
#include <unordered_set>

//#define ICON_DEV
#ifndef ICON_DEV
#include "icons/sb_led_green.png.h"
#include "icons/sb_led_yellow.png.h"
#include "icons/sb_led_red.png.h"
#include "icons/sb_arrow_left_16.png.h"
#include "icons/sb_arrow_right_16.png.h"
#include "icons/sb_arrow_up_16.png.h"
#include "icons/sb_arrow_down_16.png.h"
#endif

wxBEGIN_EVENT_TABLE(PHDStatusBar, wxStatusBar)
  EVT_SIZE(PHDStatusBar::OnSize)
wxEND_EVENT_TABLE()

// Types of fields in the statusbar
enum SBFieldTypes
{
    Field_StatusMsg,
    Field_Sat,
    Field_SNR,
    Field_RAInfo,
    Field_DecInfo,
    Field_Darks,
    Field_Calib,
    Field_Gear,
    Field_Max
};

class PHDStatusBar;

// Self-drawn panel for hosting controls in the wxStatusBar
class SBPanel : public wxPanel
{
    std::vector<int> m_fieldOffsets;
    wxString m_overlayText;
    std::unordered_set<wxWindow *> m_hidden;   // controls that are hidden by the overlay text

    enum { OVERLAY_HPADDING = 10 };

    int OverlayWidth() const {
        wxSize sz = GetTextExtent(m_overlayText);
        return OVERLAY_HPADDING + sz.GetWidth() + OVERLAY_HPADDING;
    }

#ifdef __APPLE__
    // OSX needs a timer to clear the overlay text since no events are
    // delivered when menu items are de-selected :(
    wxTimer m_timer;
    void OnTimer(wxTimerEvent&);
#endif

public:
    int emWidth;

    SBPanel(wxStatusBar *parent, const wxSize& panelSize);
    void ShowControl(wxWindow *ctrl, bool show);
    void SetOverlayText(const wxString& text);
    void OnPaint(wxPaintEvent& evt);
    void BuildFieldOffsets(const std::vector<int>& fldWidths);
    wxPoint FieldLoc(int fieldId);
    int GetMinPanelWidth();

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(SBPanel, wxPanel)
  EVT_PAINT(SBPanel::OnPaint)
#ifdef __APPLE__
  EVT_TIMER(wxID_ANY, SBPanel::OnTimer)
#endif
wxEND_EVENT_TABLE()

// Classes for color-coded state indicators
class SBStateIndicatorItem;

class SBStateIndicators
{
    std::vector <SBStateIndicatorItem *> m_stateItems;
    SBPanel *m_parentPanel;

public:
    wxIcon icoGreenLed;
    wxIcon icoYellowLed;
    wxIcon icoRedLed;

    SBStateIndicators(SBPanel *panel, std::vector<int>& fldWidths);
    ~SBStateIndicators();
    void PositionControls();
    void UpdateState();
};

class SBStateIndicatorItem
{
public:
    SBFieldTypes m_type;
    int txtWidth;
    int fieldId;
    int lastState;
    SBPanel *m_parentPanel;
    SBStateIndicators *container;
    wxStaticText *ctrl;
    wxStaticBitmap *pic;
    wxString otherInfo;

public:
    SBStateIndicatorItem(SBPanel *panel, SBStateIndicators *container,
        int indField, const wxString& indLabel, SBFieldTypes indType, std::vector<int>& fldWidths);
    void PositionControl();
    void UpdateState();
    wxString GearToolTip(int quadState);
    wxString DarksToolTip(int quadState);
};

class SBGuideIndicators
{
    wxStaticBitmap *bitmapRA;
    wxStaticBitmap *bitmapDec;
    wxStaticText *txtRaAmounts;
    wxStaticText *txtDecAmounts;
    wxBitmap arrowLeft;
    wxBitmap arrowRight;
    wxBitmap arrowUp;
    wxBitmap arrowDown;
    SBPanel *m_parentPanel;

public:
    SBGuideIndicators(SBPanel *panel, std::vector<int>& fldWidths);
    void PositionControls();
    void UpdateState(int raDirection, int decDirection, double raPx, int raPulse, double decPx, int decPulse);
    void ClearState() { UpdateState(LEFT, UP, 0, 0, 0, 0); }
};

class SBStarIndicators
{
    wxStaticText *txtSNRLabel;
    wxStaticText *txtSNRValue;
    wxStaticText *txtStarInfo;
    int snrLabelWidth;
    SBPanel *m_parentPanel;

public:
    SBStarIndicators(SBPanel *panel, std::vector<int>& fldWidths);
    void PositionControls();
    void UpdateState(double MassPct, double SNR, bool Saturated);

};

// How this works:
// PHDStatusBar is a child of wxStatusBar and is composed of various control groups - properties of the guide star, info about
// current guide commands, and state information about the current app session.  Each group is managed by its own class, and that class
// is responsible for building, positioning, and updating its controls. The various controls are positioned (via the OnSize event) on top of the SBPanel that
// is the single underlying field in the base-class statusbar.  The SBPanel class handles its own Paint event in order to render
// the borders and field separators the way we want.

// ----------------------------------------------------------------------------
// SBPanel - parent control is the parent for all the status bar items
//
SBPanel::SBPanel(wxStatusBar *parent, const wxSize& panelSize)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, panelSize)
{
    m_fieldOffsets.reserve(12);
    int txtHeight;
    parent->GetTextExtent("M", &emWidth, &txtHeight);       // Horizontal spacer used by various controls
    SetBackgroundStyle(wxBG_STYLE_PAINT);

#ifdef __APPLE__
    m_timer.SetOwner(this);
#endif

#ifndef __APPLE__
    SetDoubleBuffered(true);
#endif
}

// wxWidgets does not support controls that overlap
//   our workaround is to hide the controls that are overlapped by the overlay text
//   m_hidden contains the set of controls that are hidden, but would otherwise be visible if the overlay text were not present
//
void SBPanel::ShowControl(wxWindow *ctrl, bool show)
{
    if (m_overlayText.empty() || ctrl->GetPosition().x >= OverlayWidth())
    {
        ctrl->Show(show);
        return;
    }

    // ctrl is overlapped by overlap text
    //   show=true
    //      ctrl already in hidden - do nothing
    //      ctrl not in hidden - show(false), add to hidden
    //   show=false
    //      ctrl in hidden - remove it
    //      ctrl not in hidden - show(false)
    if (show)
    {
        if (m_hidden.find(ctrl) == m_hidden.end())
        {
            ctrl->Show(false);
            m_hidden.insert(ctrl);
        }
    }
    else
    {
        ctrl->Show(false);
        auto it = m_hidden.find(ctrl);
        if (it != m_hidden.end())
        {
            m_hidden.erase(it);
        }
    }
}

void SBPanel::SetOverlayText(const wxString& s)
{
    m_overlayText = s;

    if (s.empty())
    {
        // un-hide overlapped controls
        std::for_each(m_hidden.begin(), m_hidden.end(), [](wxWindow *p) { p->Show(true); });
        m_hidden.clear();
#ifdef __APPLE__
        m_timer.Stop();
#endif
    }
    else
    {
        int const width = OverlayWidth();

        // hide overlapped controls and un-hide hidden controls that are no longer overlapped
        const wxWindowList& children = GetChildren();
        for (auto it = children.begin(); it != children.end(); ++it)
        {
            wxWindow *w = *it;
            if (w->IsShown())
            {
                if (w->GetPosition().x < width)
                {
                    w->Show(false);
                    m_hidden.insert(w);
                }
            }
            else
            {
                if (w->GetPosition().x >= width)
                {
                    auto it = m_hidden.find(w);
                    if (it != m_hidden.end())
                    {
                        m_hidden.erase(it);
                        w->Show(true);
                    }
                }
            }
        }
#ifdef __APPLE__
        m_timer.StartOnce(5000);
#endif
    }

    Refresh();
}

#ifdef __APPLE__
void SBPanel::OnTimer(wxTimerEvent& evt)
{
    SetOverlayText(wxEmptyString);
}
#endif // __APPLE__

void SBPanel::OnPaint(wxPaintEvent& evt)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();

    wxPen pen(*wxWHITE, 1);
    wxSize panelSize = GetClientSize();
    int x;

    dc.SetPen(pen);
    // Draw vertical white lines slightly in front of each field
    for (auto it = m_fieldOffsets.begin() + 1; it != m_fieldOffsets.end(); it++)
    {
        x = panelSize.x - *it - 4;
        dc.DrawLine(wxPoint(x, 0), wxPoint(x, panelSize.y));
    }
    // Put a border on the top of the panel
    dc.DrawLine(wxPoint(0, 0), wxPoint(panelSize.x, 0));
    dc.SetPen(wxNullPen);

    if (!m_overlayText.empty())
    {
        dc.SetBrush(wxColor(0xe5, 0xdc, 0x62));
        dc.DrawRectangle(0, 0, OverlayWidth(), GetSize().GetHeight());
        dc.SetTextForeground(*wxBLACK);
        dc.DrawText(m_overlayText, OVERLAY_HPADDING, 1);
    }
}

// We want a vector with the integer offset of each field relative to the righthand end of the panel
void SBPanel::BuildFieldOffsets(const std::vector<int>& fldWidths)
{
    int cum = 0;
    // Add up field widths starting at right end of panel
    for (auto it = fldWidths.rbegin(); it != fldWidths.rend(); it++)
    {
        cum += *it;
        m_fieldOffsets.push_back(cum);
    }
    // Reverse it because the fields are indexed from left to right
    std::reverse(m_fieldOffsets.begin(), m_fieldOffsets.end());
}

int SBPanel::GetMinPanelWidth()
{
    return m_fieldOffsets.at(0);
}

wxPoint SBPanel::FieldLoc(int fieldId)
{
    wxSize panelSize = GetClientSize();
    int x = panelSize.x - m_fieldOffsets.at(fieldId);
    return wxPoint(x, 3);
}

//-----------------------------------------------------------------------------
// SBStarIndicators - properties of the guide star
//
SBStarIndicators::SBStarIndicators(SBPanel *panel, std::vector<int>& fldWidths)
{
    int snrValueWidth;
    int satWidth;

    int txtHeight;
    panel->GetTextExtent(_("SNR"), &snrLabelWidth, &txtHeight);
    panel->GetTextExtent("999.9", &snrValueWidth, &txtHeight);
    panel->GetTextExtent(_("SAT"), &satWidth, &txtHeight);
    fldWidths.push_back(satWidth + 1 * panel->emWidth);
    fldWidths.push_back(snrLabelWidth + snrValueWidth + 2 * panel->emWidth);

    // Use default positions for control creation - positioning is handled explicitly in PositionControls()
    txtStarInfo = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(satWidth, -1));
    txtStarInfo->SetBackgroundColour(*wxBLACK);
    txtStarInfo->SetForegroundColour(*wxWHITE);
    // Label and value fields separated to allow different foreground colors for each
    txtSNRLabel = new wxStaticText(panel, wxID_ANY, _("SNR"), wxDefaultPosition, wxDefaultSize);
    txtSNRValue = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(snrValueWidth, 3), wxALIGN_RIGHT);
    txtSNRLabel->SetBackgroundColour(*wxBLACK);
    txtSNRLabel->SetForegroundColour(*wxWHITE);
    txtSNRLabel->Show(false);
    txtSNRValue->SetBackgroundColour(*wxBLACK);
    txtSNRValue->SetForegroundColour(*wxGREEN);
    txtSNRValue->SetToolTip(_("Signal-to-noise ratio of guide star\nGreen means SNR >= 10\nYellow means  4 <= SNR < 10\nRed means SNR < 4"));

    m_parentPanel = panel;
}

void SBStarIndicators::PositionControls()
{
    int fieldNum = (int)Field_Sat;
    wxPoint snrPos;
    wxPoint satPos;

    satPos = m_parentPanel->FieldLoc(fieldNum++);
    txtStarInfo->SetPosition(wxPoint(satPos.x + 1, satPos.y));
    snrPos = m_parentPanel->FieldLoc(fieldNum++);
    txtSNRLabel->SetPosition(wxPoint(snrPos.x + 3, snrPos.y));
    txtSNRValue->SetPosition(wxPoint(snrPos.x + 3 + snrLabelWidth + 6, snrPos.y));
}

void SBStarIndicators::UpdateState(double MassPct, double SNR, bool Saturated)
{
    if (SNR >= 0)
    {
        if (SNR >= 10.0)
            txtSNRValue->SetForegroundColour(*wxGREEN);
        else
        {
            if (SNR >= 4.0)
                txtSNRValue->SetForegroundColour(*wxYELLOW);
            else
                txtSNRValue->SetForegroundColour(*wxRED);
        }
        m_parentPanel->ShowControl(txtSNRLabel, true);
        txtSNRValue->SetLabelText(wxString::Format("%3.1f", SNR));
        m_parentPanel->ShowControl(txtStarInfo, true);
        m_parentPanel->ShowControl(txtSNRValue, true);
        if (pFrame->pGuider->GetMultiStarMode())
        {
            wxString txtCount = pFrame->pGuider->GetStarCount();
            txtStarInfo->SetLabelText(txtCount);
        }
        else
        {
            if (Saturated)
                txtStarInfo->SetLabelText("SAT");
            else
                txtStarInfo->SetLabelText(wxEmptyString);
        }
    }
    else
    {
        m_parentPanel->ShowControl(txtSNRLabel, false);
        m_parentPanel->ShowControl(txtSNRValue, false);
        m_parentPanel->ShowControl(txtStarInfo, false);
    }
}

//-----------------------------------------------------------------------------
// SBGuideIndicators - info about the most recent guide commands
//
SBGuideIndicators::SBGuideIndicators(SBPanel *panel, std::vector<int>& fldWidths)
{
#ifdef ICON_DEV
    wxIcon arrow = wxIcon("SB_arrow_left_16.png", wxBITMAP_TYPE_PNG, 16, 16);
    arrowLeft.CopyFromIcon(arrow);
    arrow = wxIcon("SB_arrow_right_16.png", wxBITMAP_TYPE_PNG, 16, 16);
    arrowRight.CopyFromIcon(arrow);
    arrow = wxIcon("SB_arrow_up_16.png", wxBITMAP_TYPE_PNG, 16, 16);
    arrowUp.CopyFromIcon(arrow);
    arrow = wxIcon("SB_arrow_down_16.png", wxBITMAP_TYPE_PNG, 16, 16);
    arrowDown.CopyFromIcon(arrow);
#else
    arrowLeft = (wxBITMAP_PNG_FROM_DATA(sb_arrow_left_16));
    arrowRight = wxBitmap(wxBITMAP_PNG_FROM_DATA(sb_arrow_right_16));
    arrowUp = wxBitmap(wxBITMAP_PNG_FROM_DATA(sb_arrow_up_16));
    arrowDown = wxBitmap(wxBITMAP_PNG_FROM_DATA(sb_arrow_down_16));
#endif
    wxColor fgColor(200, 200, 200);           // reduced brightness
    int guideAmtWidth;
    int txtHeight;
    panel->GetTextExtent("5555 ms, 555 px", &guideAmtWidth, &txtHeight);

    // Use default positions for control creation - positioning is handled explicitly in PositionControls()
    bitmapRA = new wxStaticBitmap(panel, wxID_ANY, arrowLeft);
    wxSize bitmapSize = bitmapRA->GetSize();
    bitmapRA->Show(false);
    txtRaAmounts = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(guideAmtWidth, bitmapSize.y), wxALIGN_CENTER);
    txtRaAmounts->SetBackgroundColour(*wxBLACK);
    txtRaAmounts->SetForegroundColour(fgColor);
    txtDecAmounts = new wxStaticText(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(guideAmtWidth, bitmapSize.y), wxALIGN_RIGHT);
    txtDecAmounts->SetBackgroundColour(*wxBLACK);
    txtDecAmounts->SetForegroundColour(fgColor);
    bitmapDec = new wxStaticBitmap(panel, wxID_ANY, arrowUp);
    bitmapDec->Show(false);
    m_parentPanel = panel;
    // Since we don't want separators between the arrows and the text info, we lump the two together and treat them as one field for the purpose
    // of positioning
    fldWidths.push_back(bitmapSize.x + guideAmtWidth + 2 * panel->emWidth);          // RA info
    fldWidths.push_back(bitmapSize.x + guideAmtWidth + 2 * panel->emWidth);          // Dec info
}

void SBGuideIndicators::PositionControls()
{
    int fieldNum = (int)Field_RAInfo;
    int txtWidth;
    int txtHeight;
    wxPoint loc;

    loc = m_parentPanel->FieldLoc(fieldNum);
    bitmapRA->SetPosition(wxPoint(loc.x, loc.y - 1));
    wxPoint raPosition = m_parentPanel->FieldLoc(fieldNum);
    raPosition.x += 20;
    txtRaAmounts->SetPosition(raPosition);

    fieldNum++;
    wxString txtSizer =  wxString::Format(_("%d ms, %0.1f px"), 120, 4.38);
    m_parentPanel->GetTextExtent(txtSizer, &txtWidth, &txtHeight);
    wxPoint decPosition = m_parentPanel->FieldLoc(fieldNum);
    txtDecAmounts->SetPosition(decPosition);

    decPosition.x += txtWidth + 8;
    decPosition.y -= 1;
    bitmapDec->SetPosition(decPosition);
}

void SBGuideIndicators::UpdateState(int raDirection, int decDirection, double raPx, int raPulse, double decPx, int decPulse)
{
    wxString raInfo;
    wxString decInfo;

    if (raPulse > 0)
    {
        if (raDirection == RIGHT)
            bitmapRA->SetBitmap(arrowRight);
        else
            bitmapRA->SetBitmap(arrowLeft);

        m_parentPanel->ShowControl(bitmapRA, true);
        raInfo = wxString::Format(_("%d ms, %0.1f px"), raPulse, raPx);
    }
    else
    {
        m_parentPanel->ShowControl(bitmapRA, false);
    }

    if (decPulse > 0)
    {
        if (decDirection == UP)
            bitmapDec->SetBitmap(arrowUp);
        else
            bitmapDec->SetBitmap(arrowDown);
        m_parentPanel->ShowControl(bitmapDec, true);
        decInfo = wxString::Format(_("%d ms, %0.1f px"), decPulse, decPx);
    }
    else
    {
        m_parentPanel->ShowControl(bitmapDec, false);
    }

    txtRaAmounts->SetLabelText(raInfo);
    txtDecAmounts->SetLabelText(decInfo);
}

//------------------------------------------------------------------------------------------
// ---SBStateIndicatorItem - individual state indicators
//
SBStateIndicatorItem::SBStateIndicatorItem(SBPanel *panel, SBStateIndicators *host, int indField, const wxString& indLabel, SBFieldTypes indType, std::vector<int>& fldWidths)
{
    m_type = indType;
    lastState = -2;
    m_parentPanel = panel;
    container = host;
    fieldId = indField;
    otherInfo = wxEmptyString;
    int txtHeight;
    m_parentPanel->GetTextExtent(indLabel, &txtWidth, &txtHeight);
    // Use default positions for control creation - positioning is handled explicitly in PositionControls()
    if (indType != Field_Gear)
    {
        ctrl = new wxStaticText(m_parentPanel, wxID_ANY, indLabel, wxDefaultPosition, wxSize(txtWidth + m_parentPanel->emWidth, -1), wxALIGN_CENTER);
        fldWidths.push_back(txtWidth + 2 * m_parentPanel->emWidth);
    }
    else
    {
        pic = new wxStaticBitmap(m_parentPanel, wxID_ANY, container->icoGreenLed, wxDefaultPosition, wxSize(16, 16));
        fldWidths.push_back(20 + 1 * m_parentPanel->emWidth);
    }
}

void SBStateIndicatorItem::PositionControl()
{
    wxPoint loc;
    if (m_type == Field_Gear)
    {
        loc = m_parentPanel->FieldLoc(fieldId);
        pic->SetPosition(wxPoint(loc.x + 7, loc.y));
    }
    else
        ctrl->SetPosition(m_parentPanel->FieldLoc(fieldId));
}

static int CalibrationQuadState(wxString *tip)
{
    // For calib quad state: -1 => no cal, 0 => cal but no pointing compensation, 1 => golden

    bool calibrated = (pMount || pSecondaryMount) &&
        (!pMount || pMount->IsCalibrated()) && (!pSecondaryMount || pSecondaryMount->IsCalibrated());

    if (!calibrated)
    {
        *tip = _("Not calibrated");
        return -1;
    }

    const Scope *scope = TheScope();
    if (!scope && TheAO())
    {
        *tip = _("Calibrated"); // just an AO, we don't care about dec compensation
        return 1;
    }

    if (!pPointingSource || !pPointingSource->IsConnected() || !pPointingSource->CanReportPosition())
    {
        *tip = _("Calibrated, scope pointing information not available");
        return 0;
    }

    if (!scope->DecCompensationEnabled())
    {
        *tip = _("Calibrated, declination compensation disabled");
        return 0;
    }

    if (scope->MountCal().declination == UNKNOWN_DECLINATION)
    {
        *tip = _("Calibrated, but a new calibration is required to activate declination compensation");
        return 0;
    }

    *tip = _("Calibrated, scope pointing info in use");
    return 1;
}

static wxString Join(const wxString& delim, const std::vector<wxString>& vec)
{
    if (vec.size() == 0)
        return wxEmptyString;

    size_t l = 0;
    std::for_each(vec.begin(), vec.end(), [&l](const wxString& s) { l += s.length(); });
    wxString buf;
    buf.reserve(l + delim.length() * (vec.size() - 1));
    std::for_each(vec.begin(), vec.end(),
                  [&buf, &delim](const wxString& s) {
                      if (!buf.empty())
                          buf += delim;
                      buf += s;
                  });
    return buf;
}

void SBStateIndicatorItem::UpdateState()
{
    int quadState = -1;
    wxString cal_tooltip;

    switch (m_type)
    {
    case Field_Gear: {
        std::vector<wxString> MIAs;
        bool cameraOk = true;
        bool problems = false;
        bool partials = false;

        if (pCamera && pCamera->Connected)
        {
            partials = true;
        }
        else
        {
            MIAs.push_back(_("Camera"));
            cameraOk = false;
            problems = true;
        }

        if ((pMount && pMount->IsConnected()) || (pSecondaryMount && pSecondaryMount->IsConnected()))
            partials = true;
        else
        {
            MIAs.push_back(_("Mount"));
            problems = true;
        }

        if (pPointingSource && pPointingSource->IsConnected())
            partials = true;
        else
        {
            MIAs.push_back(_("Aux Mount"));
            problems = true;
        }

        if (pMount && pMount->IsStepGuider())
        {
            if (pMount->IsConnected())
                partials = true;
            else
            {
                MIAs.push_back(_("AO"));
                problems = true;
            }
        }

        if (pRotator)
        {
            if (pRotator->IsConnected())
                partials = true;
            else
            {
                MIAs.push_back(_("Rotator"));
                problems = true;
            }
        }

        if (partials)
        {
            if (!problems)
            {
                pic->SetIcon(wxIcon(container->icoGreenLed));
                quadState = 1;
                otherInfo.clear();
            }
            else
            {
                if (cameraOk)
                    pic->SetIcon(container->icoYellowLed);
                else
                    pic->SetIcon(container->icoRedLed);     // What good are we without a camera
                quadState = 0;
                otherInfo = Join(_(", "), MIAs);
                pic->SetToolTip(GearToolTip(quadState));
            }
        }
        else
        {
            pic->SetIcon(container->icoRedLed);
            quadState = -1;
        }

        break;
    }

    case Field_Darks:
        if (pFrame)
        {
            if (pFrame->m_useDarksMenuItem->IsChecked() || pFrame->m_useDefectMapMenuItem->IsChecked())
            {
                quadState = 1;
                wxString lastLabel = ctrl->GetLabelText();
                wxString currLabel = (pFrame->m_useDefectMapMenuItem->IsChecked() ? _("BPM") : _("Dark"));
                if (lastLabel != currLabel)
                {
                    ctrl->SetLabelText(currLabel);
                    ctrl->SetToolTip(DarksToolTip(quadState));
                }
            }
        }

        break;

    case Field_Calib: {
        quadState = CalibrationQuadState(&cal_tooltip);
        lastState = -2; // force tool-tip update even if state did not change
        break;
      }

    default:
        break;
    }

    // Don't flog the status icons unless something has changed
    if (lastState != quadState)
    {
        if (m_type != Field_Gear)
        {
            switch (quadState)
            {
            case -2:
                ctrl->SetForegroundColour(*wxLIGHT_GREY);
                break;
            case -1:
                ctrl->SetForegroundColour(*wxRED);
                break;
            case 0:
                ctrl->SetForegroundColour(*wxYELLOW);
                break;
            case 1:
                ctrl->SetForegroundColour(*wxGREEN);
                break;
            }
            ctrl->Refresh();

            if (quadState != -2)
            {
                if (m_type == Field_Darks)
                    ctrl->SetToolTip(DarksToolTip(quadState));
                else if (m_type == Field_Calib)
                    ctrl->SetToolTip(cal_tooltip);
            }
        }
        else if (quadState != -2)
        {
            // m_type == Field_Gear
            pic->SetToolTip(GearToolTip(quadState));
        }

        lastState = quadState;
    }
}

wxString SBStateIndicatorItem::GearToolTip(int quadState)
{
    if (quadState == 1)
        return _("All devices connected");
    else if (quadState == -1)
        return _("No devices connected");
    else
        return wxString::Format(_("Devices not connected: %s"), otherInfo);
}

wxString SBStateIndicatorItem::DarksToolTip(int quadState)
{
    if (ctrl->GetLabelText() == _("Dark"))
        return quadState == 1 ? _("Dark library in use") : _("Dark library not in use");
    else
        return quadState == 1 ? _("Bad-pixel map in use") : _("Bad-pixel map not in use");
}

//--------------------------------------------------------------------------------------
// ---SBStateIndicators - the group of all app/session state controls, a mix of static text and bitmap controls
//
SBStateIndicators::SBStateIndicators(SBPanel *panel, std::vector<int>& fldWidths)
{
    m_parentPanel = panel;
    wxString labels[] = { _("Dark"), _("Cal"), wxEmptyString };

#ifdef ICON_DEV
    icoGreenLed = wxIcon("SB_led_green.ico", wxBITMAP_TYPE_ICO, 16, 16);
    icoYellowLed = wxIcon("SB_led_yellow.ico", wxBITMAP_TYPE_ICO, 16, 16);
    icoRedLed = wxIcon("SB_led_red.ico", wxBITMAP_TYPE_ICO, 16, 16);
#else
    wxBitmap led(wxBITMAP_PNG_FROM_DATA(sb_led_green));
    icoGreenLed.CopyFromBitmap(led);
    led = wxBitmap(wxBITMAP_PNG_FROM_DATA(sb_led_yellow));
    icoYellowLed.CopyFromBitmap(led);
    led = wxBitmap(wxBITMAP_PNG_FROM_DATA(sb_led_red));
    icoRedLed.CopyFromBitmap(led);
#endif
    for (int inx = (int)Field_Darks; inx <= (int)Field_Gear; inx++)
    {
        SBStateIndicatorItem *item = new SBStateIndicatorItem(m_parentPanel, this, inx, labels[inx - Field_Darks], (SBFieldTypes)(inx), fldWidths);
        m_stateItems.push_back(item);
        item->UpdateState();
    }
}

SBStateIndicators::~SBStateIndicators()
{
    for (unsigned int inx = 0; inx < m_stateItems.size(); inx++)
        delete m_stateItems.at(inx);
}

void SBStateIndicators::PositionControls()
{
    for (auto it = m_stateItems.begin(); it != m_stateItems.end(); it++)
    {
        (*it)->PositionControl();
    }
}

void SBStateIndicators::UpdateState()
{
    for (auto it = m_stateItems.begin(); it != m_stateItems.end(); it++)
    {
        (*it)->UpdateState();
    }
}

enum {
    SB_HEIGHT = 16
};

// -----------  PHDStatusBar Class
//
PHDStatusBar::PHDStatusBar(wxWindow *parent, long style)
    : wxStatusBar(parent, wxID_ANY, wxSTB_SHOW_TIPS | wxSTB_ELLIPSIZE_END | wxFULL_REPAINT_ON_RESIZE, "PHDStatusBar")
{
    std::vector<int> fieldWidths;

    // Set up the only field the wxStatusBar base class will know about
    int widths[] = {-1};

    SetFieldsCount(1);
    SetStatusWidths(1, widths);
    this->SetBackgroundColour(*wxBLACK);

    m_ctrlPanel = new SBPanel(this, wxSize(500, SB_HEIGHT));
    m_ctrlPanel->SetPosition(wxPoint(1, 2));

    // Build the leftmost text status field, the only field managed at this level
    m_Msg1 = new wxStaticText(m_ctrlPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(150, -1));
    int txtWidth, txtHeight;
    GetTextExtent(_("Selected star at (999.9, 999.9)"), &txtWidth, &txtHeight);         // only care about the width
    m_Msg1->SetBackgroundColour(*wxBLACK);
    m_Msg1->SetForegroundColour(*wxWHITE);
    fieldWidths.push_back(txtWidth);                    // Doesn't matter but we need to occupy the position in fieldWidths

    // Build the star status fields
    m_StarIndicators = new SBStarIndicators(m_ctrlPanel, fieldWidths);

    // Build the guide indicators
    m_GuideIndicators = new SBGuideIndicators(m_ctrlPanel, fieldWidths);

    // Build the state indicator controls
    m_StateIndicators = new SBStateIndicators(m_ctrlPanel, fieldWidths);

    m_ctrlPanel->BuildFieldOffsets(fieldWidths);
}

// Helper function - not safe to call SetMinHeight in the constructor
PHDStatusBar *PHDStatusBar::CreateInstance(wxWindow *parent, long style)
{
    PHDStatusBar *sb = new PHDStatusBar(parent, style);
    sb->SetMinHeight(SB_HEIGHT);
    return sb;
}

// Destructor
PHDStatusBar::~PHDStatusBar()
{
    this->DestroyChildren();        // any wxWidgets objects will be deleted
    delete m_StateIndicators;
    delete m_GuideIndicators;
    delete m_StarIndicators;
}

void PHDStatusBar::OverlayMsg(const wxString& text)
{
    m_ctrlPanel->SetOverlayText(text);
}

void PHDStatusBar::ClearOverlayMsg()
{
    m_ctrlPanel->SetOverlayText(wxEmptyString);
}

void PHDStatusBar::OnSize(wxSizeEvent& event)
{
    wxRect fldRect;
    GetFieldRect(0, fldRect);
    int fldWidth = fldRect.GetWidth();
    m_ctrlPanel->SetSize(fldWidth - 1, fldRect.GetHeight());
    m_Msg1->SetPosition(wxPoint(2, 3));
    m_StarIndicators->PositionControls();
    m_GuideIndicators->PositionControls();
    m_StateIndicators->PositionControls();

    event.Skip();
}

// Let client force updates to various statusbar components
void PHDStatusBar::UpdateStates()
{
    m_StateIndicators->UpdateState();
}

void PHDStatusBar::UpdateStarInfo(double SNR, bool Saturated)
{
    m_StarIndicators->UpdateState(0, SNR, Saturated);
}

void PHDStatusBar::UpdateGuiderInfo(const GuideStepInfo& info)
{
    m_GuideIndicators->UpdateState(info.directionRA, info.directionDec, fabs(info.mountOffset.X),
        info.durationRA, fabs(info.mountOffset.Y), info.durationDec);
}

void PHDStatusBar::ClearGuiderInfo()
{
    m_GuideIndicators->ClearState();
}

int PHDStatusBar::GetMinSBWidth()
{
    return m_ctrlPanel->GetMinPanelWidth();
}

void PHDStatusBar::StatusMsg(const wxString& text)
{
    m_Msg1->SetLabelText(text);
    m_Msg1->Update();
}

// Trivial class to handle the background color on the toolbar control
void PHDToolBarArt::DrawBackground(wxDC& dc, wxWindow *parent, const wxRect& rect)
{
    dc.SetBrush(wxColour(100, 100, 100));
    dc.DrawRectangle(rect);
}
