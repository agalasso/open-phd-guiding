/*
 *  star_profile.cpp
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

BEGIN_EVENT_TABLE(ProfileWindow, wxWindow)
    EVT_PAINT(ProfileWindow::OnPaint)
    EVT_LEFT_DOWN(ProfileWindow::OnLClick)
END_EVENT_TABLE()

enum
{
    HALFW = 10,
    FULLW = 2 * HALFW + 1,
};

ProfileWindow::ProfileWindow(wxWindow *parent) :
    wxWindow(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize, wxFULL_REPAINT_ON_RESIZE,_("Profile"))
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    this->visible = false;
    this->mode = 0; // 2D profile
    rawMode = pConfig->Global.GetBoolean("/ProfileRawMode", false);
    this->SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    this->data = new unsigned short[FULLW * FULLW];  // 21x21 subframe

    memset(midrow_profile, 0, sizeof(midrow_profile));
    memset(vert_profile, 0, sizeof(vert_profile));
    memset(horiz_profile, 0, sizeof(horiz_profile));
    imageLeftMargin = 0;
    imageBottom = 0;

    m_inFocusingMode = false;
}

ProfileWindow::~ProfileWindow()
{
    delete[] data;
}

void ProfileWindow::OnLClick(wxMouseEvent& mevent)
{
    if (mevent.GetX() > imageLeftMargin && mevent.GetY() <= imageBottom)
    {
        rawMode = !rawMode;
        pConfig->Global.SetBoolean("/ProfileRawMode", rawMode);
    }
    else
    {
        // Check to see if point is within the metrics label area
        if ((pFrame->GetStarFindMode() == Star::FIND_PLANET) &&
            m_inFocusingMode && (mevent.GetX() <= m_labelX + m_labelWidth + 5) && (mevent.GetY() >= m_labelY - 5))
        {
            // Toggle between radius and sharpness metrics
            pFrame->pGuider->m_Planet.ToggleSharpness();
        }
        else if (mevent.GetX() < imageLeftMargin)
        {
            this->mode = this->mode + 1;
            if (this->mode > 2) this->mode = 0;
        }
    }
    Refresh();
}

void ProfileWindow::SetState(bool is_active)
{
    this->visible = is_active;
    if (is_active)
        Refresh();
}

void ProfileWindow::UpdateData(const usImage *img, float xpos, float ypos)
{
    if (this->data == NULL) return;

    Star::FindMode findMode = pFrame->GetStarFindMode();
    int radius = (findMode == Star::FIND_PLANET) ? pFrame->pGuider->m_Planet.m_radius * 5 / 4: HALFW;
    radius = wxMax(radius, (int) HALFW);

    int xstart = ROUNDF(xpos) - radius;
    int ystart = ROUNDF(ypos) - radius;
    if (xstart < 0) xstart = 0;
    if (ystart < 0) ystart = 0;

    int x,y;
    unsigned short *uptr = this->data;
    const int xrowsize = img->Size.GetWidth();
    const int yrowsize = img->Size.GetHeight();
    for (x = 0; x < FULLW; x++)
        horiz_profile[x] = vert_profile[x] = midrow_profile[x] = 0;
    if ((findMode == Star::FIND_PLANET) && (radius > HALFW))
    {
        for (y = 0; (y < radius * 2 + 1) && (ystart + y < yrowsize); y++) {
            int ys = y * (FULLW - 1) / (radius * 2);
            for (x = 0; (x < radius * 2 + 1) && (xstart + x < xrowsize); x++) {
                unsigned short sample = *(img->ImageData + xstart + x + (ystart + y) * xrowsize);
                int xs = x * (FULLW - 1) / (radius * 2);
                this->data[xs + ys * FULLW] = sample;
                horiz_profile[xs] += (int) sample;
                vert_profile[ys] += (int) sample;
            }
        }
    }
    else
    {
        for (y = 0; (y < FULLW) && (ystart + y < yrowsize); y++) {
            for (x = 0; (x < FULLW) && (xstart + x < xrowsize); x++, uptr++) {
                *uptr = *(img->ImageData + xstart + x + (ystart + y) * xrowsize);
                horiz_profile[x] += (int) *uptr;
                vert_profile[y] += (int) *uptr;
            }
        }
    }
    uptr = this->data + (FULLW * HALFW);
    for (x = 0; x < FULLW; x++, uptr++)
        midrow_profile[x] = (int) *uptr;
    if (this->visible)
        Refresh();
}

void ProfileWindow::OnPaint(wxPaintEvent& WXUNUSED(evt))
{
    wxAutoBufferedPaintDC dc(this);

    dc.SetBackground(wxColour(10, 30, 30));
    dc.Clear();

    if (!pFrame || !pFrame->pGuider || pFrame->pGuider->GetState() == STATE_UNINITIALIZED)
        return;

    const int xsize = this->GetSize().GetX();
    const int ysize = this->GetSize().GetY();

#if defined (__APPLE__)
    const wxFont& smallFont = *wxSMALL_FONT;
#else
    const wxFont& smallFont = *wxSWISS_FONT;
#endif
    dc.SetFont(smallFont);
    int smallFontHeight = dc.GetTextExtent("0").GetHeight();

    bool inFocusingMode = (ysize > xsize/2 + 20);

    wxFont largeFont;
    int largeFontHeight;
    int labelTextHeight;

    // Set label for displaying tracked object measured property, depending on star find mode
    wxString hfdLabel = pFrame->GetStarFindMode() == Star::FIND_PLANET ? pFrame->pGuider->m_Planet.GetHfdLabel() : _("HFD: ");

    const Star& star = pFrame->pGuider->PrimaryStar();
    float hfd = pFrame->GetStarFindMode() == Star::FIND_PLANET ? pFrame->pGuider->m_Planet.GetHFD() : star.HFD;
    if (inFocusingMode) {
        // To compute the scale factor, we use the following formula, which maximizes the use of all available
        // window width (xsize) while displaying HFD metrics in the exact format. The scaling value is calculated
        // on the premise that both large font digits are fixed-width, and that font scaling is linear.
        // The variable 'sfw' represents the width of a single digit displayed using the small font
        // and 'dotw' represents the width of a single '.' displayed using the small font.
        // xsize = 20 + smallFontTextWidth + scale * (sfw * strlen(largeFontTextWithoutDot) + dotw);
        // therefore, scale = (xsize - 10 - smallFontTextWidth) / (sfw * strlen(largeFontTextWithoutDot) + dotw)
        const Star& star = pFrame->pGuider->PrimaryStar();
        float hfd = pFrame->GetStarFindMode() == Star::FIND_PLANET ? pFrame->pGuider->m_Planet.GetHFD() : star.HFD;
        float sfw = (float)dc.GetTextExtent("0").GetWidth();
        float dotw = (float)dc.GetTextExtent(".").GetWidth();
        wxString smallFontText = hfdLabel;
        if (pFrame->pGuider->m_Planet.IsPixelMetrics() && !std::isnan(hfd))
        {
            float hfdArcSec = hfd * pFrame->GetCameraPixelScale();
            smallFontText += wxString::Format("  %.2f\"", hfdArcSec);
        }
        int smallFontTextWidth = dc.GetTextExtent(smallFontText).GetWidth();
        wxString largeDigitsText = wxString::Format("%.2f", hfd);
        int largeLenWithoutDot = largeDigitsText.Length() - 1;
        float scale = (xsize - 20 - smallFontTextWidth) / (sfw * largeLenWithoutDot + dotw);
        scale = wxMax(1.0, scale);

        // smallFontHeight * scale should be at most 1/2 of the window height
        // Note: the text extent of the large font based on ths scale factor is only an approximation,
        // but it's good enough for our purpose.
        scale = wxMin(scale, (float)ysize / (2.0 * smallFontHeight));
        largeFont = smallFont.Scaled(scale);

        dc.SetFont(largeFont);
        largeFontHeight = dc.GetTextExtent("0").GetHeight();
        dc.SetFont(smallFont);
        labelTextHeight = 5 + smallFontHeight + largeFontHeight + 5;
    }
    else
    {
        labelTextHeight = 5 + smallFontHeight + 5;
    }

    wxPen RedPen(wxColour(255,0,0));

    int i;
    int *profptr;
    wxString profileLabel;
    switch (this->mode)
    {  // Figure which profile to use
    case 0: // mid-row
    default:
        profptr = midrow_profile;
        profileLabel = _("Mid row");
        break;
    case 1: // avg row
        profptr = horiz_profile;
        profileLabel = _("Avg row");
        break;
    case 2:
        profptr = vert_profile;
        profileLabel = _("Avg col");
        break;
    }

    float fwhm = 0.f;

    // Figure max and min
    int Prof_Min, Prof_Max;
    Prof_Min = Prof_Max = *profptr;

    for (i = 1; i < FULLW; i++)
    {
        if (*(profptr + i) < Prof_Min)
            Prof_Min = *(profptr + i);
        else if (*(profptr + i) > Prof_Max)
            Prof_Max = *(profptr + i);
    }

    wxPoint Prof[FULLW];

    if (Prof_Min < Prof_Max)
    {
        int Prof_Mid = (Prof_Max - Prof_Min) / 2 + Prof_Min;
        // Figure the actual points in the window
        float Prof_Range = (float)(Prof_Max - Prof_Min) / (float)(ysize - labelTextHeight - 5);
        if (!Prof_Range) Prof_Range = 1;
        int wprof = (xsize - 15) / 2 - 5;
        wprof /= 20;
        for (i = 0; i < FULLW; i++)
            Prof[i] = wxPoint(5 + i * wprof, ysize - labelTextHeight - ((float)(*(profptr + i) - Prof_Min) / Prof_Range));

        if (pFrame->GetStarFindMode() != Star::FIND_PLANET)
        {
            // fwhm
            int x1 = 0;
            int x2 = 0;
            int profval;
            int profvalprec;
            for (i = 1; i < FULLW; i++)
            {
                profval = *(profptr + i);
                profvalprec = *(profptr + i - 1);
                if (profvalprec <= Prof_Mid && profval >= Prof_Mid)
                    x1 = i;
                else if (profvalprec >= Prof_Mid && profval <= Prof_Mid)
                    x2 = i;
            }
            profval = *(profptr + x1);
            profvalprec = *(profptr + x1 - 1);
            float f1 = (float)x1 - (float)(profval - Prof_Mid) / (float)(profval - profvalprec);
            profval = *(profptr + x2);
            profvalprec = *(profptr + x2 - 1);
            float f2 = (float)x2 - (float)(profvalprec - Prof_Mid) / (float)(profvalprec - profval);
            fwhm = f2 - f1;
        }
        else
        {
            // no fwhm in planetary mode, as we use other metrics
            fwhm = 0.0;
        }

        // Draw it
        dc.SetPen(RedPen);
        dc.DrawLines(FULLW, Prof);
    }

    // Prioritize rendering star image before rendering text
    dc.SetTextForeground(wxColour(255,0,0));

    // JBW: draw zoomed guidestar subframe (todo: make constants symbolic)
    wxImage* img = pFrame->pGuider->DisplayedImage();
    double scaleFactor = pFrame->pGuider->ScaleFactor();
    imageLeftMargin = (xsize - 15) / 2;
    if (img)
    {
        int width = xsize - imageLeftMargin - 5;
        if (width > ysize + 5) width = ysize - 5;
        int midwidth = width / 2;
        // grab width(30) px box around lock pos, scale by 2 & display next to profile
        double LockX = pFrame->pGuider->LockPosition().X * scaleFactor;
        double LockY = pFrame->pGuider->LockPosition().Y * scaleFactor;
        double dStarX = LockX - pFrame->pGuider->CurrentPosition().X * scaleFactor;
        double dStarY = LockY - pFrame->pGuider->CurrentPosition().Y * scaleFactor;
        // grab the subframe
        wxBitmap dBmp(*img);
        int planetRadius = pFrame->pGuider->m_Planet.m_radius;
        int radius = (pFrame->GetStarFindMode() == Star::FIND_PLANET) && (planetRadius > HALFW) ? planetRadius * 5 / 4 : 15;
        radius *= scaleFactor;
        int lkx = ROUND(LockX);
        int l = std::max(0, lkx - radius);
        int r = std::min(dBmp.GetWidth() - 1, lkx + radius);
        int w = std::min(lkx - l, r - lkx);
        int lky = ROUND(LockY);
        int t = std::max(0, lky - radius);
        int b = std::min(dBmp.GetHeight() - 1, lky + radius);
        int h = std::min(lky - t, b - lky);
        int sz = std::min(w, h);
        // scale by 2
        wxBitmap subDBmp = dBmp.GetSubBitmap(wxRect(lkx - sz, lky - sz, sz * 2, sz * 2));
        wxImage subDImg = subDBmp.ConvertToImage();
        wxMemoryDC tmpMdc;
        wxString toggleMsg;
        wxImageResizeQuality resizeQuality;
        // Build the temp DC with one of two scaling options
        if (!rawMode)
        {
            resizeQuality = wxIMAGE_QUALITY_HIGH;
            toggleMsg = _("Click image for raw view");
        }
        else
        {
            resizeQuality = wxIMAGE_QUALITY_NEAREST;
            toggleMsg = _("Click image for interpolated view");
        }
        wxBitmap zoomedDBmp(subDImg.Rescale(width, width, resizeQuality));
        tmpMdc.SelectObject(zoomedDBmp);
        int imgTop = 30;
        imageBottom = imgTop + width;
        // blit into profile DC
        dc.Blit(imageLeftMargin, imgTop, width, width, &tmpMdc, 0, 0, wxCOPY, false);
        // add text cue to allow switching between 'high quality' and 'nearest neighhbor' scaling
        dc.SetFont(smallFont);
        dc.DrawText(toggleMsg, imageLeftMargin, imgTop - smallFontHeight);

        // lines for the lock pos + red dot at star centroid
        dc.SetPen(wxPen(wxColor(0, 200, 0), 1, wxPENSTYLE_DOT));
        dc.DrawLine(imageLeftMargin, midwidth + imgTop, imageLeftMargin + width, midwidth + imgTop);
        dc.DrawLine(imageLeftMargin + midwidth, imgTop, imageLeftMargin + midwidth, width + imgTop);
        if (sz > 0)
        {
            // and a small cross at the centroid
            double starX = imageLeftMargin + midwidth - dStarX * (width / (sz * 2.0)) + 1, starY = midwidth - dStarY * (width / (sz * 2.0)) + 1 + imgTop;
            if (starX >= imageLeftMargin)
            {
                dc.SetPen(RedPen);
                dc.DrawLine(starX - 5, starY, starX + 5, starY);
                dc.DrawLine(starX, starY - 5, starX, starY + 5);
            }
        }
    }

    if (star.IsValid())
    {
        dc.DrawText(_("Peak"), 3, 3);
        dc.DrawText(wxString::Format("%u", star.PeakVal), 3, 3 + smallFontHeight);
    }

    wxString fwhmLine = (pFrame->GetStarFindMode() == Star::FIND_PLANET) ? profileLabel : wxString::Format(_("%s FWHM: %.2f"), profileLabel, fwhm);
    if (hfd != 0.f)
    {
        float hfdArcSec = hfd * pFrame->GetCameraPixelScale();
        if (inFocusingMode)
        {
            int fwhmLineWidth = dc.GetTextExtent(fwhmLine).GetWidth();
            dc.DrawText(fwhmLine, 5, ysize - labelTextHeight + 5);

            // Show X/Y of centroid if there's room
            if ((imageLeftMargin > fwhmLineWidth + 20) && (ysize - labelTextHeight + 5 > imageBottom))
                dc.DrawText(wxString::Format("X: %0.2f, Y: %0.2f", pFrame->pGuider->CurrentPosition().X, pFrame->pGuider->CurrentPosition().Y), imageLeftMargin, ysize - labelTextHeight + 5);

            // Show metrics label using small font
            m_labelX = 5;
            m_labelY = ysize - largeFontHeight / 2 - smallFontHeight / 2;
            m_labelWidth = dc.GetTextExtent(hfdLabel).GetWidth();
            m_labelHeight = smallFontHeight;
            dc.DrawText(hfdLabel, m_labelX, m_labelY);

            m_inFocusingMode = true;
            int x = m_labelX + m_labelWidth;
            wxString s = wxString::Format(_T("%.2f"), hfd);
            if (std::isnan(hfd))
            {
                dc.DrawText(_T("LOADING ..."), x, m_labelY);
            }
            else
            {
                dc.SetFont(largeFont);
                dc.DrawText(s, x, ysize - largeFontHeight);
                x += dc.GetTextExtent(s).GetWidth();

                if (pFrame->pGuider->m_Planet.IsPixelMetrics())
                {
                    dc.SetFont(smallFont);
                    s = wxString::Format(_T("  %.2f\""), hfdArcSec);
                    dc.DrawText(s, x, ysize - largeFontHeight / 2 - smallFontHeight / 2);
                }
            }
        }
        else
        {
            if (pFrame->pGuider->m_Planet.IsPixelMetrics())
                dc.DrawText(wxString::Format(_("%s FWHM: %.2f, %s%.2f (%.2f\")"), profileLabel, fwhm, hfdLabel, hfd, hfdArcSec), 5, ysize - smallFontHeight - 5);
            else
                dc.DrawText(wxString::Format(_("%s; %s%.2f"), profileLabel, hfdLabel, hfd), 5, ysize - smallFontHeight - 5);
            m_inFocusingMode = false;
        }
    }
    else
    {
        dc.DrawText(fwhmLine, 5, ysize - smallFontHeight - 5);
        m_inFocusingMode = false;
    }
}

