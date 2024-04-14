/*
 *  target.cpp
 *  PHD Guiding
 *
 *  Created by Sylvain Girard
 *  Copyright (c) 2013 Sylvain Girard
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
 *    Neither the name of Bret McKee, Dad Dog Development Ltd, nor the names of its
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
 *
 */

#include "phd.h"

static double const MIN_ZOOM = 0.25;

BEGIN_EVENT_TABLE(TargetWindow, wxWindow)
    EVT_BUTTON(BUTTON_GRAPH_LENGTH,TargetWindow::OnButtonLength)
    EVT_MENU_RANGE(MENU_LENGTH_BEGIN, MENU_LENGTH_END, TargetWindow::OnMenuLength)
    EVT_BUTTON(BUTTON_GRAPH_CLEAR,TargetWindow::OnButtonClear)
    EVT_BUTTON(BUTTON_GRAPH_ZOOMIN,TargetWindow::OnButtonZoomIn)
    EVT_BUTTON(BUTTON_GRAPH_ZOOMOUT,TargetWindow::OnButtonZoomOut)
    EVT_CHECKBOX(TARGET_ENABLE_REF_CIRCLE, TargetWindow::OnCheckBoxRefCircle)
    EVT_SPINCTRLDOUBLE(TARGET_REF_CIRCLE_RADIUS, TargetWindow::OnRefCircleRadius)
END_EVENT_TABLE()

TargetWindow::TargetWindow(wxWindow *parent) :
    wxWindow(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize, 0,_("Target"))
{
    //SetFont(wxFont(8,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL));
    SetBackgroundColour(*wxBLACK);

    m_visible = false;
    m_pClient = new TargetClient(this);

    wxBoxSizer *pMainSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *pLeftSizer = new wxBoxSizer(wxVERTICAL);

    pMainSizer->Add(pLeftSizer);

    wxString label = wxString::Format("%3d", m_pClient->m_length);
    m_lengthButton = new OptionsButton(this,BUTTON_GRAPH_LENGTH,label,wxDefaultPosition,wxSize(40/*80*/,-1),wxALIGN_CENTER_HORIZONTAL);
    m_lengthButton->SetToolTip(_("Select the number of frames of history to display"));

    wxBoxSizer *pZoomSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton *zoomInButton = new wxButton(this, BUTTON_GRAPH_ZOOMIN, _T("+"), wxDefaultPosition, wxSize(40, -1));
    zoomInButton->SetToolTip(_("Zoom in"));

    wxButton *zoomOutButton = new wxButton(this, BUTTON_GRAPH_ZOOMOUT, _T("-"), wxDefaultPosition, wxSize(40, -1));
    zoomOutButton->SetToolTip(_("Zoom out"));

    pZoomSizer->Add(zoomInButton, wxSizerFlags(1).Expand());
    pZoomSizer->Add(zoomOutButton, wxSizerFlags(1).Expand());

    wxButton *clearButton = new wxButton(this,BUTTON_GRAPH_CLEAR,_("Clear"),wxDefaultPosition,wxSize(80,-1));
    clearButton->SetToolTip(_("Clear graph data"));

    m_enableRefCircle = new wxCheckBox(this, TARGET_ENABLE_REF_CIRCLE, _("Reference Circle"));
    m_enableRefCircle->SetToolTip(_("Check to display a reference circle"));
#if defined(__WXOSX__)
    // workaround inability to set checkbox foreground color
    m_enableRefCircle->SetBackgroundColour(wxColor(200, 200, 200));
#else
    m_enableRefCircle->SetForegroundColour(*wxLIGHT_GREY);
#endif

    wxStaticText *lbl = new wxStaticText(this, wxID_ANY, _("Radius:"));
    lbl->SetForegroundColour(*wxLIGHT_GREY);
    lbl->SetBackgroundColour(*wxBLACK);

    int w, h;
    GetTextExtent(_T("88.8"), &w, &h);
    m_refCircleRadius = pFrame->MakeSpinCtrlDouble(this, TARGET_REF_CIRCLE_RADIUS, wxEmptyString, wxDefaultPosition, wxSize(w, -1));
    m_refCircleRadius->SetToolTip(_("Reference circle radius"));
    m_refCircleRadius->SetRange(0.1, 10.0);
    m_refCircleRadius->SetIncrement(0.1);
    m_refCircleRadius->SetDigits(1);
    wxBoxSizer *sizer1 = new wxBoxSizer(wxHORIZONTAL);
    sizer1->Add(lbl, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxRIGHT, 5));
    sizer1->Add(m_refCircleRadius, wxSizerFlags(1).Align(wxALIGN_CENTER_VERTICAL).Expand());

    pLeftSizer->Add(m_lengthButton, wxSizerFlags().Center().Border(wxTOP | wxRIGHT | wxLEFT, 5).Expand());
    pLeftSizer->Add(pZoomSizer, wxSizerFlags().Border(wxRIGHT | wxLEFT, 5).Expand());
    pLeftSizer->Add(clearButton, wxSizerFlags().Border(wxRIGHT | wxLEFT,5).Expand());
    pLeftSizer->Add(m_enableRefCircle, wxSizerFlags().Center().Border(wxALL, 3).Expand());
    pLeftSizer->Add(sizer1, wxSizerFlags().Center().Border(wxRIGHT | wxLEFT, 5).Expand());

    pMainSizer->Add(m_pClient, wxSizerFlags().Border(wxALL,3).Expand().Proportion(1));

    SetSizer(pMainSizer);
    pMainSizer->SetSizeHints(this);

    UpdateControls();
}

TargetWindow::~TargetWindow()
{
    delete m_pClient;
}

void TargetWindow::UpdateControls(void)
{
    m_enableRefCircle->SetValue(pConfig->Profile.GetBoolean("/target/refCircleEnabled", false));
    m_refCircleRadius->SetValue(pConfig->Profile.GetDouble("/target/refCircleRadius", 2.0));
    m_pClient->m_refCircleRadius = m_enableRefCircle->GetValue() ? m_refCircleRadius->GetValue() : 0.0;
    m_pClient->Refresh();
}

void TargetWindow::SetState(bool is_active)
{
    m_visible = is_active;
    if (is_active)
        Refresh();
}

void TargetWindow::AppendData(const GuideStepInfo& step)
{
    m_pClient->AppendData(step);

    if (this->m_visible)
    {
        Refresh();
    }
}

void TargetWindow::OnButtonLength(wxCommandEvent& WXUNUSED(evt))
{
    wxMenu *menu = new wxMenu();

    unsigned int val = m_pClient->m_minLength;
    for (int id = MENU_LENGTH_BEGIN; id <= MENU_LENGTH_END; id++)
    {
        wxMenuItem *item = menu->AppendRadioItem(id, wxString::Format("%d", val));
        if (val == m_pClient->m_length)
            item->Check(true);
        val *= 2;
        if (val > m_pClient->m_maxLength)
            break;
    }

    PopupMenu(menu, m_lengthButton->GetPosition().x,
        m_lengthButton->GetPosition().y + m_lengthButton->GetSize().GetHeight());

    delete menu;
}

void TargetWindow::OnMenuLength(wxCommandEvent& evt)
{
    unsigned int val = m_pClient->m_minLength;
    for (int id = MENU_LENGTH_BEGIN; id < evt.GetId(); id++)
        val *= 2;

    m_pClient->m_length = val;

    pConfig->Global.SetInt("/target/length", val);

    m_lengthButton->SetLabel(wxString::Format(_T("%3d"), val));
    Refresh();
}

void TargetWindow::OnButtonClear(wxCommandEvent& WXUNUSED(evt))
{
    m_pClient->m_nItems = 0;
    Refresh();
}

void TargetWindow::OnButtonZoomIn(wxCommandEvent& evt)
{
    if (m_pClient->m_zoom < 3)
    {
        m_pClient->m_zoom *= 2;
        pConfig->Global.SetDouble("/target/zoom", m_pClient->m_zoom);
    }
    Refresh();
}

void TargetWindow::OnButtonZoomOut(wxCommandEvent& evt)
{
    if (m_pClient->m_zoom > MIN_ZOOM)
    {
        m_pClient->m_zoom /= 2.0;
        pConfig->Global.SetDouble("/target/zoom", m_pClient->m_zoom);
    }
    Refresh();
}

void TargetWindow::OnCheckBoxRefCircle(wxCommandEvent& event)
{
    m_pClient->m_refCircleRadius = event.IsChecked() ? m_refCircleRadius->GetValue() : 0.0;
    pConfig->Profile.SetBoolean("/target/refCircleEnabled", event.IsChecked());
    m_pClient->Refresh();
}

void TargetWindow::OnRefCircleRadius(wxSpinDoubleEvent& event)
{
    pConfig->Profile.SetDouble("/target/refCircleRadius", event.GetValue());
    if (m_enableRefCircle->GetValue())
    {
        m_pClient->m_refCircleRadius = event.GetValue();
        m_pClient->Refresh();
    }
}

BEGIN_EVENT_TABLE(TargetClient, wxWindow)
    EVT_PAINT(TargetClient::OnPaint)
END_EVENT_TABLE()

TargetClient::TargetClient(wxWindow *parent) :
    wxWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(201,201), wxFULL_REPAINT_ON_RESIZE )
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    m_minLength = 50;
    m_maxLength = 400;

    m_refCircleRadius = 0.0;
    m_nItems = 0;
    m_length = pConfig->Global.GetInt("/target/length", 100);
    m_zoom = pConfig->Global.GetDouble("/target/zoom", 1.0);
    if (m_zoom < MIN_ZOOM)
        m_zoom = MIN_ZOOM;
}

TargetClient::~TargetClient(void)
{
}

void TargetClient::AppendData(const GuideStepInfo& step)
{
    memmove(&m_history, &m_history[1], sizeof(m_history[0])*(m_maxHistorySize-1));

    m_history[m_maxHistorySize-1].ra = step.mountOffset.X;
    m_history[m_maxHistorySize-1].dec = step.mountOffset.Y;

    if (m_nItems < m_maxHistorySize)
    {
        m_nItems++;
    }
}

void TargetClient::OnPaint(wxPaintEvent& WXUNUSED(evt))
{
    wxAutoBufferedPaintDC dc(this);

    dc.SetBackground(*wxBLACK_BRUSH);
    //dc.SetBackground(wxColour(10,0,0));
    dc.Clear();

    wxColour Grey(128,128,128);
    wxPen GreySolidPen = wxPen(Grey,1, wxPENSTYLE_SOLID);
    wxPen GreyDashPen = wxPen(Grey,1, wxPENSTYLE_DOT);

    dc.SetTextForeground(wxColour(200,200,200));
    dc.SetFont(wxFont(8,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL));
    dc.SetPen(GreySolidPen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    wxSize size = GetClientSize();
    wxPoint center(size.x/2, size.y/2);
    int radius_max = ((size.x < size.y ? size.x : size.y) - 6) / 2;

    int leftEdge = center.x - radius_max;
    int topEdge = center.y - radius_max;
    radius_max -= 18;

    if (radius_max < 10)
        radius_max = 10;

    const double sampling = pFrame ? pFrame->GetCameraPixelScale() : 1.0;
    double scale = radius_max / 2 * sampling;

    // Draw reference circle
    if (m_refCircleRadius > 0.0)
    {
        wxDCBrushChanger b(dc, wxBrush(wxColor(55,55,55)));
        wxDCPenChanger p(dc, *wxTRANSPARENT_PEN);
        dc.DrawCircle(center, m_refCircleRadius * scale * m_zoom / sampling);
    }

    // Draw circles
    for (int i = 1 ; i <= 4 ; i++)
    {
        int rr = radius_max * i / 4;
        dc.DrawCircle(center, rr);
        wxString l = wxString::Format(_T("%g%s"), i/2.0 / m_zoom, sampling != 1.0 ? "''" : "");
        wxSize sl = dc.GetTextExtent(l);
        dc.DrawText(l, center.x - sl.x - 1, center.y - rr - sl.y);
    }

    // Draw axes
    dc.DrawLine(3, center.y , size.x - 3, center.y);
    dc.DrawLine(center.x, 3, center.x, size.y - 3);

    double r = radius_max / (2 / m_zoom);
    int g = size.x / 100;
    for (double x = 0 ; x < size.x ; x += r/4)
    {
        if (x != radius_max && x != r)
        {
            dc.DrawLine(center.x + x, center.y - g, center.x + x, center.y + g);
            dc.DrawLine(center.x - x, center.y - g, center.x - x, center.y + g);
        }
    }
    for (double y = 0 ; y < size.y ; y += r/4)
    {
        if (y != radius_max && y != r)
        {
            dc.DrawLine(center.x - g, center.y + y, center.x + g, center.y + y);
            dc.DrawLine(center.x - g, center.y - y, center.x + g, center.y - y);
        }
    }

    // Draw labels
    int d_width = 10, d_height = 10;
    dc.GetTextExtent(_("0"), &d_width, &d_height);
    dc.DrawText(_("RA"), leftEdge, center.y - 3 - d_height);
    dc.DrawText(_("Dec"), center.x + 5, center.y - radius_max - d_height);

    // Draw impacts
    unsigned int startPoint = m_maxHistorySize - m_length;

    if (m_nItems < m_length)
    {
        startPoint = m_maxHistorySize - m_nItems;
    }

    // plot guide star offsets in mount coordinates:
    //   RA offset is distance W of lock pos
    //        => plot -dRA for East = positive
    //   Dec offset is distance S of lock pos
    //        => plot -dDec for North = positive
    double const raSign = -1.0;
    double const decSign = -1.0;

    // label sky coordinate directions

    GuideParity raParity = pMount ? pMount->RAParity() : GUIDE_PARITY_UNKNOWN;
    if (raParity == GUIDE_PARITY_EVEN)
        dc.DrawText(_("SkyE"), size.x - d_width * 5, center.y + 5);  // sky E = mount E
    else if (raParity == GUIDE_PARITY_ODD)
        dc.DrawText(_("SkyE"), leftEdge, center.y + 5);              // sky E = mount W

    GuideParity decParity = pMount ? pMount->DecParity() : GUIDE_PARITY_UNKNOWN;
    if (decParity == GUIDE_PARITY_EVEN)
        dc.DrawText(_("SkyN"), center.x + 5, center.y + 5 - radius_max);    // sky N = mount N
    else if (decParity == GUIDE_PARITY_ODD)
        dc.DrawText(_("SkyN"), center.x + 5, center.y + 5 + radius_max);    // sky N = mount S

    dc.SetPen(wxPen(wxColour(127, 127, 255), 1, wxPENSTYLE_SOLID));
    for (unsigned int i = startPoint; i < m_maxHistorySize; i++)
    {
        int ximpact = center.x + m_history[i].ra * scale * m_zoom * raSign;
        int yimpact = center.y - m_history[i].dec * scale * m_zoom * decSign;
        if (i == m_maxHistorySize - 1)
        {
            const int lcrux = 4;
            dc.SetPen(*wxRED_PEN);
            dc.DrawLine(ximpact + lcrux, yimpact + lcrux, ximpact - lcrux - 1, yimpact - lcrux - 1);
            dc.DrawLine(ximpact + lcrux, yimpact - lcrux, ximpact - lcrux - 1, yimpact + lcrux + 1);
        }
        else
            dc.DrawCircle(ximpact, yimpact, 1);
    }
}
