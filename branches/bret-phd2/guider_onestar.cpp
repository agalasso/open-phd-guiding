/*
 *  guider_onestar.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
 *  All rights reserved.
 *
 *  Greatly expanded by Bret McKee
 *  Copyright (c) 2012 Bret McKee
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

#if defined (__APPLE__)
#include "../cfitsio/fitsio.h"
#else
#include <fitsio.h>
#endif

#define SCALE_UP_SMALL  // Currently problematic as the box for the star is drawn in the wrong spot.
#if ((wxMAJOR_VERSION < 3) && (wxMINOR_VERSION < 9))
#define wxPENSTYLE_DOT wxDOT
#endif

static const double DefaultMassChangeThreshold = 0.5;
static const int DefaultSearchRegion = 15;

BEGIN_EVENT_TABLE(GuiderOneStar, Guider)
    EVT_PAINT(GuiderOneStar::OnPaint)
    EVT_LEFT_DOWN(GuiderOneStar::OnLClick)
END_EVENT_TABLE()

// Define a constructor for the guide canvas
GuiderOneStar::GuiderOneStar(wxWindow *parent):
    Guider(parent, XWinSize, YWinSize)
{
    double massChangeThreshold  = PhdConfig.GetDouble((char *) "/guider/onestar/MassChangeThreshold",
            DefaultMassChangeThreshold);
    SetMassChangeThreshold(massChangeThreshold);

    int searchRegion = PhdConfig.GetInt((char *) "/guider/onestar/SearchRegion", DefaultSearchRegion);
    SetSearchRegion(searchRegion);

    SetState(STATE_UNINITIALIZED);
}

GuiderOneStar::~GuiderOneStar()
{
}

double GuiderOneStar::GetMassChangeThreshold(void)
{
    return m_massChangeThreshold;
}

bool GuiderOneStar::SetMassChangeThreshold(double massChangeThreshold)
{
    bool bError = false;

    try
    {
        if (massChangeThreshold < 0)
        {
            throw ERROR_INFO("massChangeThreshold < 0");
        }

        m_massChangeThreshold = massChangeThreshold;
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);

        bError = true;
        m_massChangeThreshold = DefaultMassChangeThreshold;
    }

    m_badMassCount = 0;
    PhdConfig.SetDouble((char *) "/guider/onestar/MassChangeThreshold", m_massChangeThreshold);

    return bError;
}

int GuiderOneStar::GetSearchRegion(void)
{
    return m_searchRegion;
}

bool GuiderOneStar::SetSearchRegion(int searchRegion)
{
    bool bError = false;

    try
    {
        if (searchRegion <= 0)
        {
            throw ERROR_INFO("searchRegion <= 0");
        }
        m_searchRegion = searchRegion;
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
        m_searchRegion = DefaultSearchRegion;
    }

    PhdConfig.SetInt((char *) "/guider/onestar/SearchRegion", m_searchRegion);

    return bError;
}


bool GuiderOneStar::SetCurrentPosition(usImage *pImage, const PHD_Point& position)
{
    bool bError = true;

    try
    {
        if (!position.IsValid())
        {
            throw ERROR_INFO("position is invalid");
        }

        double x = position.X;
        double y = position.Y;

        Debug.AddLine(wxString::Format("SetCurrentPostion(%d,%d)", x, y ));

        if ((x <= 0) || (x >= pImage->Size.x))
        {
            throw ERROR_INFO("invalid y value");
        }

        if ((y <= 0) || (y >= pImage->Size.x))
        {
            throw ERROR_INFO("invalid x value");
        }

        bError = m_star.Find(pImage, m_searchRegion, x, y);
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }

    return bError;
}

bool GuiderOneStar::AutoSelect(usImage *pImage)
{
    bool bError = false;

    try
    {
        Star newStar;

        if (pImage == NULL)
        {
            pImage = CurrentImage();
        }

        if (!newStar.AutoFind(pImage))
        {
            throw ERROR_INFO("Uable to AutoFind");
        }

        if (!m_star.Find(pImage, m_searchRegion, newStar.X, newStar.Y))
        {
            throw ERROR_INFO("Uable to find");
        }

        if (SetLockPosition(m_star , true))
        {
            throw ERROR_INFO("Unable to set Lock Position");
        }
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool GuiderOneStar::IsLocked(void)
{
    return m_star.WasFound();
}

PHD_Point &GuiderOneStar::CurrentPosition(void)
{
    return m_star;
}

wxRect GuiderOneStar::GetBoundingBox(void)
{
    wxRect box(0, 0, 0, 0);

    if (GetState() == STATE_GUIDING)
    {
        box = wxRect(LockPosition().X - 3*m_searchRegion, LockPosition().Y - 3*m_searchRegion,
                6*m_searchRegion, 6*m_searchRegion);
        box.Intersect(wxRect(0, 0, pCamera->FullSize.x, pCamera->FullSize.y));
    }

    return box;
}

void GuiderOneStar::InvalidateCurrentPosition(void)
{
    m_star.Invalidate();
    m_autoSelectTries = 0;
}

bool GuiderOneStar::UpdateCurrentPosition(usImage *pImage, wxString &statusMessage)
{
    bool bError = false;

    try
    {
        GUIDER_STATE state = GetState();

        if (state == STATE_SELECTING && m_autoSelectTries++ == 0)
        {
            Debug.Write("UpdateGuideState(): Autoselecting\n");

            if (AutoSelect(pImage))
            {
                statusMessage = _T("No Star selected");
                throw THROW_INFO("No Star Autoselected");
            }

            pFrame->SetStatusText(wxString::Format(_T("Auto Selected star at (%.1f, %.1f)"),m_star.X, m_star.Y), 1);
        }

        Star newStar(m_star);

        if (!newStar.Find(pImage, m_searchRegion))
        {
            statusMessage = _T("No Star found");
            throw ERROR_INFO("UpdateGuideState():newStar not found");
        }

        if (m_massChangeThreshold < 0.99 &&
            m_star.Mass > 0.0 &&
            newStar.Mass > 0.0 &&
            m_badMassCount++ < 2)
        {
            // check to see if it seems like the star we just found was the
            // same as the orignial star.  We do this by comparing the
            // mass
            double massRatio;

            if (newStar.Mass > m_star.Mass)
            {
                massRatio = m_star.Mass/newStar.Mass;
            }
            else
            {
                massRatio = newStar.Mass/m_star.Mass;
            }

            massRatio = 1.0 - massRatio;

            assert(massRatio >= 0 && massRatio < 1.0);

            if (massRatio > m_massChangeThreshold)
            {
                m_star.SetError(Star::STAR_MASSCHANGE);
                pFrame->SetStatusText(wxString::Format(_("Mass: %.0f vs %.0f"), newStar.Mass, m_star.Mass), 1);
                Debug.Write(wxString::Format("UpdateGuideState(): star mass ratio=%.1f, thresh=%.1f new=%.1f, old=%.1f\n", massRatio, m_massChangeThreshold, newStar.Mass, m_star.Mass));
                throw THROW_INFO("massChangeThreshold error");
            }
        }

        // update the star position, mass, etc.
        m_star = newStar;
        m_badMassCount = 0;

        pFrame->Profile->UpdateData(pImage, m_star.X, m_star.Y);
        statusMessage.Printf(_T("m=%.0f SNR=%.1f"), m_star.Mass, m_star.SNR);
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void GuiderOneStar::OnLClick(wxMouseEvent &mevent)
{
    try
    {
        if (GetState() > STATE_SELECTED)
        {
            mevent.Skip();
            throw THROW_INFO("Skipping event because state > STATE_SELECTED");
        }

        if (mevent.m_shiftDown)
        {
            // clear them out
            SetState(STATE_UNINITIALIZED);
        }
        else
        {
            if ((mevent.m_x <= m_searchRegion) || (mevent.m_x >= (XWinSize+m_searchRegion)) || (mevent.m_y <= m_searchRegion) || (mevent.m_y >= (XWinSize+m_searchRegion)))
            {
                mevent.Skip();
                throw THROW_INFO("Skipping event because click outside of search region");
            }

            usImage *pImage = CurrentImage();

            if (pImage->NPixels == 0)
            {
                mevent.Skip();
                throw ERROR_INFO("Skipping event m_pCurrentImage->NPixels == 0");
            }

            double scaleFactor = ScaleFactor();
            double StarX = (double) mevent.m_x / scaleFactor;
            double StarY = (double) mevent.m_y / scaleFactor;

            SetCurrentPosition(pImage, PHD_Point(StarX, StarY));

            if (!m_star.IsValid())
            {
                pFrame->SetStatusText(wxString::Format(_("No star found")));
            }
            else
            {
                SetLockPosition(m_star, true);
                pFrame->SetStatusText(wxString::Format(_("Selected star at (%.1f, %.1f)"),m_star.X, m_star.Y), 1);
                pFrame->SetStatusText(wxString::Format(_T("m=%.0f SNR=%.1f"),m_star.Mass,m_star.SNR));
            }

            Refresh();
            Update();
        }
    }
    catch (wxString Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }
}

// Define the repainting behaviour
void GuiderOneStar::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    wxMemoryDC memDC;

    if (!PaintHelper(dc, memDC))
    {
        // PaintHelper drew the image and any overlays
        // now decorate the image to show the selection

        GUIDER_STATE state = GetState();
        bool FoundStar = m_star.WasFound();
        double StarX = m_star.X;
        double StarY = m_star.Y;

        if (state == STATE_SELECTED || IsPaused()) {

            if (FoundStar)
                dc.SetPen(wxPen(wxColour(100,255,90),1,wxSOLID ));  // Draw the box around the star
            else
                dc.SetPen(wxPen(wxColour(230,130,30),1,wxDOT ));
            dc.SetBrush(* wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(ROUND(StarX*m_scaleFactor)-m_searchRegion,ROUND(StarY*m_scaleFactor)-m_searchRegion,m_searchRegion*2+1,m_searchRegion*2+1);
        }
        else if (state == STATE_CALIBRATING_PRIMARY || state == STATE_CALIBRATING_SECONDARY) {  // in the cal process
            dc.SetPen(wxPen(wxColour(32,196,32),1,wxSOLID ));  // Draw the box around the star
            dc.SetBrush(* wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(ROUND(StarX*m_scaleFactor)-m_searchRegion,ROUND(StarY*m_scaleFactor)-m_searchRegion,m_searchRegion*2+1,m_searchRegion*2+1);

        }
        else if (state == STATE_CALIBRATED || state == STATE_GUIDING) { // locked and guiding
            if (FoundStar)
                dc.SetPen(wxPen(wxColour(32,196,32),1,wxSOLID ));  // Draw the box around the star
            else
                dc.SetPen(wxPen(wxColour(230,130,30),1,wxDOT ));
            dc.SetBrush(* wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(ROUND(StarX*m_scaleFactor)-m_searchRegion,ROUND(StarY*m_scaleFactor)-m_searchRegion,m_searchRegion*2+1,m_searchRegion*2+1);

        }

        if ((Log_Images==1) && (state >= STATE_SELECTED)) {  // Save star image as a JPEG
            double LockX = LockPosition().X;
            double LockY = LockPosition().Y;

            wxBitmap SubBmp(60,60,-1);
            wxMemoryDC tmpMdc;
            tmpMdc.SelectObject(SubBmp);
            memDC.SetPen(wxPen(wxColor(0,255,0),1,wxDOT));
            memDC.DrawLine(0, LockY*m_scaleFactor, XWinSize, LockY*m_scaleFactor);
            memDC.DrawLine(LockX*m_scaleFactor, 0, LockX*m_scaleFactor, YWinSize);
#ifdef __APPLEX__
            tmpMdc.Blit(0,0,60,60,&memDC,ROUND(StarX*m_scaleFactor)-30,Displayed_Image->GetHeight() - ROUND(StarY*m_scaleFactor)-30,wxCOPY,false);
#else
            tmpMdc.Blit(0,0,60,60,&memDC,ROUND(StarX*m_scaleFactor)-30,ROUND(StarY*m_scaleFactor)-30,wxCOPY,false);
#endif

            //          tmpMdc.Blit(0,0,200,200,&Cdc,0,0,wxCOPY);
            wxString fname = LogFile->GetName();
            wxDateTime CapTime;
            CapTime=wxDateTime::Now();
            //full_fname = base_name + CapTime.Format("_%j_%H%M%S.fit");
            fname = fname.BeforeLast('.') + CapTime.Format(_T("_%j_%H%M%S")) + _T(".jpg");
            SubBmp.SaveFile(fname,wxBITMAP_TYPE_JPEG);
            tmpMdc.SelectObject(wxNullBitmap);
        }
        else if ((Log_Images==2) && (state >= STATE_SELECTED)) { // Save star image as a FITS
            SaveStarFITS();
        }
        memDC.SelectObject(wxNullBitmap);
    }
}

void GuiderOneStar::SaveStarFITS() {
    double StarX = m_star.X;
    double StarY = m_star.Y;
    usImage *pImage = CurrentImage();
    usImage tmpimg;

    tmpimg.Init(60,60);
    int start_x = ROUND(StarX)-30;
    int start_y = ROUND(StarY)-30;
    if ((start_x + 60) > pImage->Size.GetWidth())
        start_x = pImage->Size.GetWidth() - 60;
    if ((start_y + 60) > pImage->Size.GetHeight())
        start_y = pImage->Size.GetHeight() - 60;
    int x,y, width;
    width = pImage->Size.GetWidth();
    unsigned short *usptr = tmpimg.ImageData;
    for (y=0; y<60; y++)
        for (x=0; x<60; x++, usptr++)
            *usptr = *(pImage->ImageData + (y+start_y)*width + (x+start_x));
    wxString fname = LogFile->GetName();
    wxDateTime CapTime;
    CapTime=wxDateTime::Now();
    fname = fname.BeforeLast('.') + CapTime.Format(_T("_%j_%H%M%S")) + _T(".fit");

    fitsfile *fptr;  // FITS file pointer
    int status = 0;  // CFITSIO status value MUST be initialized to zero!
    long fpixel[3] = {1,1,1};
    long fsize[3];
    char keyname[9]; // was 9
    char keycomment[100];
    char keystring[100];
    int output_format=USHORT_IMG;

    fsize[0] = 60;
    fsize[1] = 60;
    fsize[2] = 0;
    fits_create_file(&fptr,(const char*) fname.mb_str(wxConvUTF8),&status);
    if (!status) {
        fits_create_img(fptr,output_format, 2, fsize, &status);

        time_t now;
        struct tm *timestruct;
        time(&now);
        timestruct=gmtime(&now);
        sprintf(keyname,"DATE");
        sprintf(keycomment,"UTC date that FITS file was created");
        sprintf(keystring,"%.4d-%.2d-%.2d %.2d:%.2d:%.2d",timestruct->tm_year+1900,timestruct->tm_mon+1,timestruct->tm_mday,timestruct->tm_hour,timestruct->tm_min,timestruct->tm_sec);
        if (!status) fits_write_key(fptr, TSTRING, keyname, keystring, keycomment, &status);

        sprintf(keyname,"DATE-OBS");
        sprintf(keycomment,"YYYY-MM-DDThh:mm:ss observation start, UT");
        sprintf(keystring,"%s",(const char*) pImage->ImgStartDate.c_str());
        if (!status) fits_write_key(fptr, TSTRING, keyname, keystring, keycomment, &status);

        sprintf(keyname,"EXPOSURE");
        sprintf(keycomment,"Exposure time [s]");
        float dur = (float) pImage->ImgExpDur / 1000.0;
        if (!status) fits_write_key(fptr, TFLOAT, keyname, &dur, keycomment, &status);

        unsigned int tmp = 1;
        sprintf(keyname,"XBINNING");
        sprintf(keycomment,"Camera binning mode");
        fits_write_key(fptr, TUINT, keyname, &tmp, keycomment, &status);
        sprintf(keyname,"YBINNING");
        sprintf(keycomment,"Camera binning mode");
        fits_write_key(fptr, TUINT, keyname, &tmp, keycomment, &status);

        sprintf(keyname,"XORGSUB");
        sprintf(keycomment,"Subframe x position in binned pixels");
        tmp = start_x;
        fits_write_key(fptr, TINT, keyname, &tmp, keycomment, &status);
        sprintf(keyname,"YORGSUB");
        sprintf(keycomment,"Subframe y position in binned pixels");
        tmp = start_y;
        fits_write_key(fptr, TINT, keyname, &tmp, keycomment, &status);


        if (!status) fits_write_pix(fptr,TUSHORT,fpixel,tmpimg.NPixels,tmpimg.ImageData,&status);

    }
    fits_close_file(fptr,&status);
}

ConfigDialogPane *GuiderOneStar::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuiderOneStarConfigDialogPane(pParent, this);
}

GuiderOneStar::GuiderOneStarConfigDialogPane::GuiderOneStarConfigDialogPane(wxWindow *pParent, GuiderOneStar *pGuider)
    : GuiderConfigDialogPane(pParent, pGuider)
{
    int width;

    m_pGuiderOneStar = pGuider;

    width = StringWidth(_T("0000"));
    m_pSearchRegion = new wxSpinCtrl(pParent, wxID_ANY, _T("foo2"), wxPoint(-1,-1),
            wxSize(width+30, -1), wxSP_ARROW_KEYS, 10, 50, 15, _T("Search"));
    DoAdd(_("Search region (pixels)"), m_pSearchRegion,
          _("How many pixels (up/down/left/right) do we examine to find the star? Default = 15"));

    width = StringWidth(_T("0000"));
    m_pMassChangeThreshold = new wxSpinCtrlDouble(pParent, wxID_ANY,_T("foo2"), wxPoint(-1,-1),
            wxSize(width+30, -1), wxSP_ARROW_KEYS, 0.1, 100.0, 0.0, 1.0,_T("MassChangeThreshold"));
    m_pMassChangeThreshold->SetDigits(1);
    DoAdd(_("Star mass tolerance"), m_pMassChangeThreshold,
          _("Tolerance for change in star mass b/n frames. Default = 0.3 (0.1-1.0)"));
}

GuiderOneStar::GuiderOneStarConfigDialogPane::~GuiderOneStarConfigDialogPane(void)
{
}

void GuiderOneStar::GuiderOneStarConfigDialogPane::LoadValues(void)
{
    GuiderConfigDialogPane::LoadValues();
    m_pMassChangeThreshold->SetValue(100.0*m_pGuiderOneStar->GetMassChangeThreshold());
    m_pSearchRegion->SetValue(m_pGuiderOneStar->GetSearchRegion());
}

void GuiderOneStar::GuiderOneStarConfigDialogPane::UnloadValues(void)
{
    m_pGuiderOneStar->SetMassChangeThreshold(m_pMassChangeThreshold->GetValue()/100.0);

    m_pGuiderOneStar->SetSearchRegion(m_pSearchRegion->GetValue());
    GuiderConfigDialogPane::UnloadValues();
}
