/*
*  statswindow.cpp
*  PHD Guiding
*
*  Created by Andy Galasso
*  Copyright (c) 2014 Andy Galasso
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

#include "phd.h"
#include "statswindow.h"

enum {
    TIMER_ID_COOLER = 101,
};

wxBEGIN_EVENT_TABLE(StatsWindow, wxWindow)
    EVT_BUTTON(BUTTON_GRAPH_LENGTH, StatsWindow::OnButtonLength)
    EVT_MENU_RANGE(MENU_LENGTH_BEGIN, MENU_LENGTH_END, StatsWindow::OnMenuLength)
    EVT_BUTTON(BUTTON_GRAPH_CLEAR, StatsWindow::OnButtonClear)
    EVT_TIMER(TIMER_ID_COOLER, StatsWindow::OnTimerCooler)
wxEND_EVENT_TABLE()

StatsWindow::StatsWindow(wxWindow *parent)
    : wxWindow(parent, wxID_ANY),
    m_visible(false),
    m_coolerTimer(this, TIMER_ID_COOLER),
    m_lastFrameSize(wxDefaultSize)
{
    SetBackgroundColour(*wxBLACK);

    m_grid1 = new wxGrid(this, wxID_ANY);

    m_grid1->CreateGrid(4, 3);
    m_grid1->SetRowLabelSize(1);
    m_grid1->SetColLabelSize(1);
    m_grid1->EnableEditing(false);
    m_grid1->SetDefaultCellBackgroundColour(*wxBLACK);
    m_grid1->SetDefaultCellTextColour(*wxLIGHT_GREY);
    m_grid1->SetGridLineColour(wxColour(40, 40, 40));

    int col = 0;
    int row = 0;
    m_grid1->SetCellValue(row, col++, "");
    m_grid1->SetCellValue(row, col++, _("RMS [px]"));
    m_grid1->SetCellValue(row, col++, _("Peak [px]"));
    ++row, col = 0;
    m_grid1->SetCellValue(row, col++, _("RA"));
    m_grid1->SetCellValue(row, col++, _T(" 99.99 (99.99'')"));
    m_grid1->SetCellValue(row, col++, _(" 99.99 (99.99'')"));
    ++row, col = 0;
    m_grid1->SetCellValue(row, col++, _("Dec"));
    ++row, col = 0;
    m_grid1->SetCellValue(row, col++, _("Total"));

    m_grid1->AutoSize();
    m_grid1->SetCellValue(1, 1, "");
    m_grid1->SetCellValue(1, 2, "");
    m_grid1->ClearSelection();
    m_grid1->DisableDragGridSize();

    m_grid2 = new wxGrid(this, wxID_ANY);
    m_grid2->CreateGrid(12, 2);
    m_grid2->SetRowLabelSize(1);
    m_grid2->SetColLabelSize(1);
    m_grid2->EnableEditing(false);
    m_grid2->SetDefaultCellBackgroundColour(*wxBLACK);
    m_grid2->SetDefaultCellTextColour(*wxLIGHT_GREY);
    m_grid2->SetGridLineColour(wxColour(40, 40, 40));

    row = 0, col = 0;
    m_grid2->SetCellValue(row, col++, _("RA Osc"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("RA Limited"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Dec Limited"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Star lost"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Declination"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Pier Side"));
    m_grid2->SetCellValue(3, 1, _T("MMMMMM"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Rotator Pos"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Camera binning"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Image size"));
    m_frameSizeRow = row;
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Pixel scale"));
    m_pixelScaleRow = row;
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Field of View"));
    // Make sure the column is big enough
    m_grid2->SetCellValue(row, col, _("nnn.n x nnn.n arc-min"));
    ++row, col = 0;
    m_grid2->SetCellValue(row, col++, _("Camera cooler"));
    m_grid2->SetCellValue(row, col, "-99" DEGREES_SYMBOL " / -99" DEGREES_SYMBOL ", 999%");
    m_coolerRow = row;

    m_grid2->AutoSize();
    m_grid2->SetCellValue(3, 1, wxEmptyString);
    m_grid2->SetCellValue(m_pixelScaleRow + 1, 1, wxEmptyString); // field of view row
    m_grid2->ClearSelection();
    m_grid2->DisableDragGridSize();

    // Planetary detection stats
    const int grid3Rows = 4;
    m_grid3 = new wxGrid(this, wxID_ANY);
    m_grid3->CreateGrid(grid3Rows, 2);
    m_grid3->SetRowLabelSize(1);
    m_grid3->SetColLabelSize(1);
    m_grid3->EnableEditing(false);
    m_grid3->SetDefaultCellBackgroundColour(*wxBLACK);
    m_grid3->SetDefaultCellTextColour(*wxLIGHT_GREY);
    m_grid3->SetGridLineColour(wxColour(40, 40, 40));

    row = 0, col = 0;
    m_grid3->SetCellValue(row, col++, _("Detection time"));
    m_grid3->SetCellValue(row, col, _("000000 ms"));
    ++row, col = 0;
    m_grid3->SetCellValue(row, col++, _("Contours/points"));
    m_grid3->SetCellValue(row, col, _("9999/9999"));
    ++row, col = 0;
    m_grid3->SetCellValue(row, col++, _("Fitting score"));
    m_grid3->SetCellValue(row, col, _("1.00"));
    ++row, col = 0;
    m_grid3->SetCellValue(row, col++, _("Detection error"));
    m_grid3->SetCellValue(row, col, _("9999999"));
    m_grid3->AutoSize();

    m_grid3->SetCellValue(0, 1, wxEmptyString);
    m_grid3->SetCellValue(1, 0, wxEmptyString);
    m_grid3->SetCellValue(1, 1, wxEmptyString);
    m_grid3->SetCellValue(2, 0, wxEmptyString);
    m_grid3->SetCellValue(2, 1, wxEmptyString);
    m_grid3->SetCellValue(3, 0, wxEmptyString);
    m_grid3->SetCellValue(3, 1, wxEmptyString);
    m_grid3->ClearSelection();
    m_grid3->DisableDragGridSize();
    m_grid3->HideRow(3);
    ShowPlanetStats(false);

    wxSizer *sizer1 = new wxBoxSizer(wxHORIZONTAL);

    wxButton *clearButton = new wxButton(this, BUTTON_GRAPH_CLEAR, _("Clear"));
    clearButton->SetToolTip(_("Clear graph data and stats"));
    clearButton->SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
    sizer1->Add(clearButton, 0, wxALL, 10);

    m_pLengthButton = new OptionsButton(this, BUTTON_GRAPH_LENGTH, _T("XXXXXXX:888888"), wxDefaultPosition, wxSize(220, -1));
    m_pLengthButton->SetToolTip(_("Select the number of frames of history for stats and the graph"));
    m_length = dynamic_cast<MyFrame *>(parent)->pGraphLog->GetLength();
    m_pLengthButton->SetLabel(wxString::Format(_T("x:%3d"), m_length));
    sizer1->Add(m_pLengthButton, 0, wxALL, 10);

    wxSizer *sizer2 = new wxBoxSizer(wxVERTICAL);

    sizer2->Add(sizer1, 0, wxEXPAND, 10);
    sizer2->Add(m_grid1, wxSizerFlags(0).Border(wxALL, 10));
    sizer2->Add(m_grid2, wxSizerFlags(0).Border(wxALL, 10));
    sizer2->Add(m_grid3, wxSizerFlags(0).Border(wxALL, 10));

    SetSizerAndFit(sizer2);
}

StatsWindow::~StatsWindow(void)
{
}

void StatsWindow::SetState(bool is_active)
{
    m_visible = is_active;
    if (m_visible)
    {
        UpdateStats();
        UpdateCooler();
    }
}

static wxString arcsecs(double px, double sampling)
{
    if (sampling != 1.0)
        return wxString::Format("% 4.2f (%.2f'')", px, px * sampling);
    else
        return wxString::Format("% 4.2f", px);
}

void StatsWindow::UpdateStats(void)
{
    if (!m_visible)
        return;

    if (!pFrame || !pFrame->pGraphLog)
        return;

    int length = pFrame->pGraphLog->GetLength();
    if (m_length != length)
    {
        m_pLengthButton->SetLabel(wxString::Format(_T("x:%3d"), length));
        m_length = length;
    }

    const SummaryStats& stats = pFrame->pGraphLog->Stats();
    const double sampling = pFrame->GetCameraPixelScale();

    m_grid1->BeginBatch();
    m_grid2->BeginBatch();

    int row = 1, col = 1;
    m_grid1->SetCellValue(row++, col, arcsecs(stats.rms_ra, sampling));
    m_grid1->SetCellValue(row++, col, arcsecs(stats.rms_dec, sampling));
    m_grid1->SetCellValue(row++, col, arcsecs(stats.rms_tot, sampling));

    row = 1, col = 2;
    m_grid1->SetCellValue(row++, col, arcsecs(stats.ra_peak, sampling));
    m_grid1->SetCellValue(row++, col, arcsecs(stats.dec_peak, sampling));

    row = 0, col = 1;
    if (stats.osc_alert)
        m_grid2->SetCellTextColour(row, col, wxColour(185, 20, 0));
    else
        m_grid2->SetCellTextColour(row, col, *wxLIGHT_GREY);
    m_grid2->SetCellValue(row++, col, wxString::Format("% .02f", stats.osc_index));

    unsigned int historyItems = wxMax(pFrame->pGraphLog->GetHistoryItemCount(), 1); // avoid divide-by-zero
    if (stats.ra_limit_cnt > 0)
        m_grid2->SetCellTextColour(row, col, wxColour(185, 20, 0));
    else
        m_grid2->SetCellTextColour(row, col, *wxLIGHT_GREY);
    m_grid2->SetCellValue(row++, col, wxString::Format(" %u (%.f%%)", stats.ra_limit_cnt, stats.ra_limit_cnt * 100. / historyItems));

    if (stats.dec_limit_cnt > 0)
        m_grid2->SetCellTextColour(row, col, wxColour(185, 20, 0));
    else
        m_grid2->SetCellTextColour(row, col, *wxLIGHT_GREY);
    m_grid2->SetCellValue(row++, col, wxString::Format(" %u (%.f%%)", stats.dec_limit_cnt, stats.dec_limit_cnt * 100. / historyItems));

    m_grid2->SetCellValue(row++, col, wxString::Format(" %u", stats.star_lost_cnt));

    m_grid1->EndBatch();
    m_grid2->EndBatch();
}

static wxString CamCoolerStatus()
{
    bool on;
    double setpt, power, temp;
    bool err = pCamera->GetCoolerStatus(&on, &setpt, &power, &temp);
    if (err)
        return _("Camera error");
    else if (on)
        return wxString::Format(_("%.f%s / %.f%s, %.f%%"), temp, DEGREES_SYMBOL, setpt, DEGREES_SYMBOL, power);
    else
        return wxString::Format(_("%.f%s, Off"), temp, DEGREES_SYMBOL);
}

void StatsWindow::UpdateCooler()
{
    m_coolerTimer.Stop();

    if (!m_visible)
        return;

    wxString s;
    if (pCamera && pCamera->Connected)
    {
        if (pCamera->HasCooler)
        {
            s = CamCoolerStatus();
            enum { COOLER_POLL_INTERVAL_MS = 10000 };
            m_coolerTimer.StartOnce(COOLER_POLL_INTERVAL_MS);
        }
        else
            s = _("None");
    }
    m_grid2->SetCellValue(m_coolerRow, 1, s);
}

static wxString fov(const wxSize& sensorFormat, double sampling)
{
    if (sampling != 1.0)
        return wxString::Format("%4.1f x %4.1f %s", sensorFormat.x * sampling / 60.0, sensorFormat.y * sampling / 60.0, _("arc-min"));
    else
        return " ";
}

void StatsWindow::UpdateImageSize(const wxSize& frameSize)
{
    if (m_visible && frameSize != m_lastFrameSize)
    {
        wxString sensorStr = wxString::Format("%d x %d %s", frameSize.x, frameSize.y, _("px"));
        m_grid2->SetCellValue(m_frameSizeRow, 1, sensorStr);
        const double sampling = pFrame ? pFrame->GetCameraPixelScale() : 1.0;
        m_grid2->SetCellValue(m_pixelScaleRow, 1, wxString::Format(_("%.1f\"/%s"), sampling, _("px")));
        m_grid2->SetCellValue(m_pixelScaleRow + 1, 1, fov(frameSize, sampling));
        m_lastFrameSize = frameSize;
    }
}

void StatsWindow::ResetImageSize()
{
    m_lastFrameSize = wxDefaultSize;
    m_grid2->SetCellValue(m_frameSizeRow, 1, wxEmptyString);
    m_grid2->SetCellValue(m_pixelScaleRow, 1, wxEmptyString);
    m_grid2->SetCellValue(m_pixelScaleRow + 1, 1, wxEmptyString);
}

void StatsWindow::ClearPlanetStats()
{
    m_grid3->SetCellValue(0, 1, wxEmptyString);
    m_grid3->SetCellValue(1, 0, wxEmptyString);
    m_grid3->SetCellValue(1, 1, wxEmptyString);
    m_grid3->SetCellValue(2, 0, wxEmptyString);
    m_grid3->SetCellValue(2, 1, wxEmptyString);
    m_grid3->SetCellValue(3, 0, wxEmptyString);
    m_grid3->SetCellValue(3, 1, wxEmptyString);
}

void StatsWindow::ShowPlanetStats(bool show)
{
    if (show)
        m_grid3->Show();
    else
        m_grid3->Hide();
    Layout();
}

void StatsWindow::ShowSimulatorStats(bool show)
{
    if (show)
        m_grid3->ShowRow(3);
    else
        m_grid3->HideRow(3);
}

void StatsWindow::UpdatePlanetDetectionTime(int msec)
{
    wxString timeStr = wxString::Format(_T("%d ms"), msec);
    m_grid3->SetCellValue(0, 1, timeStr);
}

void StatsWindow::UpdatePlanetFeatureCount(wxString label, int count)
{
    wxString valueStr = wxString::Format(_T("%d"), count);
    m_grid3->SetCellValue(1, 0, label);
    m_grid3->SetCellValue(1, 1, valueStr);
}

void StatsWindow::UpdatePlanetFeatureCount(wxString label, int count1, int count2)
{
    wxString valueStr = wxString::Format(_T("%d/%d"), count1, count2);
    m_grid3->SetCellValue(1, 0, label);
    m_grid3->SetCellValue(1, 1, valueStr);
}

void StatsWindow::UpdatePlanetScore(wxString label, float score)
{
    if (label == wxEmptyString)
    {
        m_grid3->SetCellValue(2, 0, wxEmptyString);
        m_grid3->SetCellValue(2, 1, wxEmptyString);
    }
    else
    {
        m_grid3->SetCellValue(2, 0, label);
        m_grid3->SetCellValue(2, 1, wxString::Format(_T("%.2f"), score));
    }
}

void StatsWindow::UpdatePlanetError(wxString label, float error)
{
    if (pCamera && pCamera->Name == "Simulator")
    {
        wxString valueStr = error >= 0 ? wxString::Format(_T("%.2f px"), error) : _("loading");
        m_grid3->SetCellValue(3, 0, label);
        m_grid3->SetCellValue(3, 1, valueStr);
    }
}

void StatsWindow::OnTimerCooler(wxTimerEvent&)
{
    UpdateCooler();
}

static wxString RotatorPosStr()
{
    if (!pRotator)
        return _("N/A");
    double pos = Rotator::RotatorPosition();
    if (pos == Rotator::POSITION_UNKNOWN)
        return _("Unknown");
    else
        return wxString::Format("%5.1f", norm(pos, 0.0, 360.0));
}

void StatsWindow::UpdateScopePointing()
{
    if (pPointingSource)
    {
        double declination = pPointingSource->GetDeclinationRadians();
        PierSide pierSide = pPointingSource->SideOfPier();

        m_grid2->BeginBatch();
        int row = 4, col = 1;
        m_grid2->SetCellValue(row++, col, Mount::DeclinationStrTr(declination, "% .1f" DEGREES_SYMBOL));
        m_grid2->SetCellValue(row++, col, Mount::PierSideStrTr(pierSide));
        m_grid2->SetCellValue(row++, col, RotatorPosStr());
        m_grid2->SetCellValue(row++, col, pCamera ? wxString::Format("%hu", pCamera->Binning) : _("N/A"));
        m_grid2->EndBatch();
    }
}

void StatsWindow::OnButtonLength(wxCommandEvent& WXUNUSED(evt))
{
    wxMenu *menu = pFrame->pGraphLog->GetLengthMenu();

    PopupMenu(menu, m_pLengthButton->GetPosition().x,
        m_pLengthButton->GetPosition().y + m_pLengthButton->GetSize().GetHeight());

    delete menu;
}

void StatsWindow::OnMenuLength(wxCommandEvent& evt)
{
    pFrame->pGraphLog->OnMenuLength(evt);
}

void StatsWindow::OnButtonClear(wxCommandEvent& evt)
{
    pFrame->pGraphLog->OnButtonClear(evt);
}
