/*
 *  guider.cpp
 *  PHD Guiding
 *
 *  Created by Bret McKee
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
#include "nudge_lock.h"
#include "comet_tool.h"
#include "planetary_tool.h"
#include "polardrift_tool.h"
#include "staticpa_tool.h"
#include "guiding_assistant.h"

// un-comment to log star deflections to a file
//#define CAPTURE_DEFLECTIONS

struct DeflectionLogger
{
#ifdef CAPTURE_DEFLECTIONS
    wxFFile *m_file;
    PHD_Point m_lastPos;
#endif

    void Init();
    void Uninit();
    void Log(const PHD_Point& pos);
};
static DeflectionLogger s_deflectionLogger;

#ifdef CAPTURE_DEFLECTIONS

void DeflectionLogger::Init()
{
    m_file = new wxFFile();
    wxDateTime now = wxDateTime::UNow();
    wxString pathname = Debug.GetLogDir() + PATHSEPSTR + now.Format(_T("star_displacement_%Y-%m-%d_%H%M%S.csv"));
    m_file->Open(pathname, "w");
    m_lastPos.Invalidate();
}

void DeflectionLogger::Uninit()
{
    delete m_file;
    m_file = 0;
}

void DeflectionLogger::Log(const PHD_Point& pos)
{
    if (m_lastPos.IsValid())
    {
        PHD_Point mountpt;
        pMount->TransformCameraCoordinatesToMountCoordinates(pos - m_lastPos, mountpt);
        m_file->Write(wxString::Format("%0.2f,%0.2f\n", mountpt.X, mountpt.Y));
    }
    else
    {
        m_file->Write(wxString::Format("DeltaRA, DeltaDec, Scale=%0.2f\n", pFrame->GetCameraPixelScale()));
        if (pMount->GetGuidingEnabled())
            pFrame->Alert("GUIDING IS ACTIVE!!!  Star displacements will be useless!");
    }

    m_lastPos = pos;
}

#else // CAPTURE_DEFLECTIONS

inline void DeflectionLogger::Init() { }
inline void DeflectionLogger::Uninit() { }
inline void DeflectionLogger::Log(const PHD_Point&) { }

#endif // CAPTURE_DEFLECTIONS

static const int DefaultOverlayMode  = OVERLAY_NONE;
static const bool DefaultScaleImage  = true;

BEGIN_EVENT_TABLE(Guider, wxWindow)
    EVT_PAINT(Guider::OnPaint)
    EVT_CLOSE(Guider::OnClose)
    EVT_ERASE_BACKGROUND(Guider::OnErase)
END_EVENT_TABLE()

static void SaveBookmarks(const std::vector<wxRealPoint>& vec)
{
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(5);
    for (auto it = vec.begin(); it != vec.end(); ++it)
        os << it->x << ' ' << it->y << ' ';
    pConfig->Profile.SetString("/guider/bookmarks", os.str().c_str());
}

static void LoadBookmarks(std::vector<wxRealPoint> *vec)
{
    wxString s(pConfig->Profile.GetString("/guider/bookmarks", wxEmptyString));
    std::istringstream is(static_cast<const char *>(s.c_str()));

    vec->clear();

    while (true)
    {
        double x, y;
        is >> x >> y;
        if (!is.good())
            break;
        vec->push_back(wxRealPoint(x, y));
    }
}

static const wxStringCharType *StateStr(GUIDER_STATE st)
{
    switch (st) {
    case STATE_UNINITIALIZED: return wxS("UNINITIALIZED");
    case STATE_SELECTING: return wxS("SELECTING");
    case STATE_SELECTED: return wxS("SELECTED");
    case STATE_CALIBRATING_PRIMARY: return wxS("CALIBRATING_PRIMARY");
    case STATE_CALIBRATING_SECONDARY: return wxS("CALIBRATING_SECONDARY");
    case STATE_CALIBRATED: return wxS("CALIBRATED");
    case STATE_GUIDING: return wxS("GUIDING");
    case STATE_STOP: return wxS("STOP");
    default: return wxS("??");
    }
}

Guider::Guider(wxWindow *parent, int xSize, int ySize) :
    wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    m_state = STATE_UNINITIALIZED;
    Debug.Write(wxString::Format("guider state => %s\n", StateStr(m_state)));
    m_scaleFactor = 1.0;
    m_showBookmarks = true;
    m_displayedImage = new wxImage(XWinSize,YWinSize,true);
    m_paused = PAUSE_NONE;
    m_starFoundTimestamp = 0;
    m_avgDistanceNeedReset = false;
    m_avgDistanceCnt = 0;
    m_lockPosShift.shiftEnabled = false;
    m_lockPosShift.shiftRate.SetXY(0., 0.);
    m_lockPosShift.shiftUnits = UNIT_ARCSEC;
    m_lockPosShift.shiftIsMountCoords = true;
    m_lockPosIsSticky = false;
    m_ignoreLostStarLooping = false;
    m_forceFullFrame = false;
    m_measurementMode = false;
    m_searchRegion = 0;
    m_pCurrentImage = new usImage(); // so we always have one

    SetOverlayMode(DefaultOverlayMode);

    wxPoint center;
    center.x = pConfig->Profile.GetInt("/overlay/slit/center.x", 752 / 2);
    center.y = pConfig->Profile.GetInt("/overlay/slit/center.y", 580 / 2);
    wxSize size;
    size.x = pConfig->Profile.GetInt("/overlay/slit/width", 8);
    size.y = pConfig->Profile.GetInt("/overlay/slit/height", 100);
    int angle = pConfig->Profile.GetInt("/overlay/slit/angle", 0);
    SetOverlaySlitCoords(center, size, angle);

    m_defectMapPreview = 0;

    m_polarAlignCircleRadius = 0.0;
    m_polarAlignCircleCorrection = 1.0;

    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    SetBackgroundColour(wxColour((unsigned char) 30, (unsigned char) 30,(unsigned char) 30));

    s_deflectionLogger.Init();
}

Guider::~Guider()
{
    delete m_displayedImage;
    delete m_pCurrentImage;

    s_deflectionLogger.Uninit();
}

void Guider::LoadProfileSettings()
{
    bool enableFastRecenter = pConfig->Profile.GetBoolean("/guider/FastRecenter", true);
    EnableFastRecenter(enableFastRecenter);

    bool scaleImage = pConfig->Profile.GetBoolean("/guider/ScaleImage", DefaultScaleImage);
    SetScaleImage(scaleImage);

    double minHFD = pConfig->Profile.GetDouble("/guider/StarMinHFD", GetMinStarHFDDefault());
    // Handle upgrades from earlier releases that allowed zero MinHFD values.  Values below floor
    // are considered to be bogus and usually zero.  Set those to the default value (1.5).
    // Values between floor and default may be valid and would have come from specific user
    // configuration - so leave them alone.  Any values >= default are also acceptable and will be
    // left alone.  NOTE: first-generation LodeStar cameras create extremely low HFD values when binned 2x2
    if (minHFD < GetMinStarHFDFloor())
        minHFD = GetMinStarHFDFloor();
    SetMinStarHFD(minHFD);
    double maxHFD = pConfig->Profile.GetDouble("/guider/StarMaxHFD", 20.0);
    SetMaxStarHFD(maxHFD);

    double minSNR = pConfig->Profile.GetDouble("/guider/StarMinSNR", 6.0);
    SetAFMinStarSNR(minSNR);

    unsigned int autoSelDownsample = wxMax(0, pConfig->Profile.GetInt("/guider/AutoSelDownsample", 0));
    SetAutoSelDownsample(autoSelDownsample);

    LoadBookmarks(&m_bookmarks);

    // clear the display
    if (m_pCurrentImage->ImageData)
    {
        delete m_displayedImage;
        m_displayedImage = new wxImage(XWinSize, YWinSize, true);
        DisplayImage(new usImage());
    }
}

PauseType Guider::SetPaused(PauseType pause)
{
    Debug.Write(wxString::Format("Guider::SetPaused(%d)\n", pause));

    PauseType prev = m_paused;
    m_paused = pause;

    if (prev == PAUSE_FULL && pause != prev)
    {
        Debug.Write("Guider::SetPaused: resetting avg dist filter\n");
        m_avgDistanceNeedReset = true;
    }

    if (pause != prev)
    {
        Refresh();
        Update();
    }

    return prev;
}

void Guider::ForceFullFrame()
{
    if (!m_forceFullFrame)
    {
        Debug.Write("setting force full frames = true\n");
        m_forceFullFrame = true;
    }
}

void Guider::SetIgnoreLostStarLooping(bool ignore)
{
    if (m_ignoreLostStarLooping != ignore)
    {
        Debug.Write(wxString::Format("setting ignore lost star looping = %s\n", ignore ? "true" : "false"));
        m_ignoreLostStarLooping = ignore;
    }
}

bool Guider::SetOverlayMode(int overlayMode)
{
    bool bError = false;

    try
    {
        switch (overlayMode)
        {
            case OVERLAY_NONE:
            case OVERLAY_BULLSEYE:
            case OVERLAY_GRID_FINE:
            case OVERLAY_GRID_COARSE:
            case OVERLAY_RADEC:
            case OVERLAY_SLIT:
                break;
            default:
                throw ERROR_INFO("invalid overlayMode");
        }

        m_overlayMode = (OVERLAY_MODE) overlayMode;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        m_overlayMode = OVERLAY_NONE;
        bError = true;
    }

    Refresh();
    Update();

    return bError;
}

void Guider::GetOverlaySlitCoords(wxPoint *center, wxSize *size, int *angle)
{
    *center = m_overlaySlitCoords.center;
    *size = m_overlaySlitCoords.size;
    *angle = m_overlaySlitCoords.angle;
}

void Guider::EnableMeasurementMode(bool enable)
{
    if (enable)
    {
        if (m_state == STATE_GUIDING)
            m_measurementMode = true;
    }
    else
        m_measurementMode = false;
}

void Guider::SetOverlaySlitCoords(const wxPoint& center, const wxSize& size, int angle)
{
    m_overlaySlitCoords.center = center;
    m_overlaySlitCoords.size = size;
    m_overlaySlitCoords.angle = angle;

    pConfig->Profile.SetInt("/overlay/slit/center.x", center.x);
    pConfig->Profile.SetInt("/overlay/slit/center.y", center.y);
    pConfig->Profile.SetInt("/overlay/slit/width", size.x);
    pConfig->Profile.SetInt("/overlay/slit/height", size.y);
    pConfig->Profile.SetInt("/overlay/slit/angle", angle);

    if (size.GetWidth() > 0 && size.GetHeight() > 0)
    {
        if (angle != 0)
        {
            double a = -radians((double)angle);
            double s = sin(a);
            double c = cos(a);
            double cx = (double)center.x;
            double cy = (double)center.y;
            double x, y;

            x = +(double)size.GetWidth() / 2.0;
            y = +(double)size.GetHeight() / 2.0;
            m_overlaySlitCoords.corners[0].x = cx + (int)floor(x * c - y * s);
            m_overlaySlitCoords.corners[0].y = cy + (int)floor(x * s + y * c);

            x = -(double)size.GetWidth() / 2.0;
            y = +(double)size.GetHeight() / 2.0;
            m_overlaySlitCoords.corners[1].x = cx + (int)floor(x * c - y * s);
            m_overlaySlitCoords.corners[1].y = cy + (int)floor(x * s + y * c);

            x = -(double)size.GetWidth() / 2.0;
            y = -(double)size.GetHeight() / 2.0;
            m_overlaySlitCoords.corners[2].x = cx + (int)floor(x * c - y * s);
            m_overlaySlitCoords.corners[2].y = cy + (int)floor(x * s + y * c);

            x = +(double)size.GetWidth() / 2.0;
            y = -(double)size.GetHeight() / 2.0;
            m_overlaySlitCoords.corners[3].x = cx + (int)floor(x * c - y * s);
            m_overlaySlitCoords.corners[3].y = cy + (int)floor(x * s + y * c);
        }
        else
        {
            m_overlaySlitCoords.corners[0] = wxPoint(center.x + size.GetWidth() / 2, center.y + size.GetHeight() / 2);
            m_overlaySlitCoords.corners[1] = wxPoint(center.x - size.GetWidth() / 2, center.y + size.GetHeight() / 2);
            m_overlaySlitCoords.corners[2] = wxPoint(center.x - size.GetWidth() / 2, center.y - size.GetHeight() / 2);
            m_overlaySlitCoords.corners[3] = wxPoint(center.x + size.GetWidth() / 2, center.y - size.GetHeight() / 2);
        }

        m_overlaySlitCoords.corners[4] = m_overlaySlitCoords.corners[0];
    }

    Refresh();
    Update();
}

void Guider::EnableFastRecenter(bool enable)
{
    m_fastRecenterEnabled = enable;
    pConfig->Profile.SetInt("/guider/FastRecenter", m_fastRecenterEnabled);
}

void Guider::SetPolarAlignCircle(const PHD_Point& pt, double radius)
{
    m_polarAlignCircleRadius = radius;
    m_polarAlignCircleCenter = pt;
}

bool Guider::SetScaleImage(bool newScaleValue)
{
    bool bError = false;

    try
    {
        m_scaleImage = newScaleValue;
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    pConfig->Profile.SetBoolean("/guider/ScaleImage", m_scaleImage);
    return bError;
}

void Guider::OnErase(wxEraseEvent& evt)
{
    evt.Skip();
}

void Guider::OnClose(wxCloseEvent& evt)
{
    Destroy();
}

bool Guider::PaintHelper(wxAutoBufferedPaintDCBase& dc, wxMemoryDC& memDC)
{
    bool bError = false;

    try
    {
        GUIDER_STATE state = GetState();
        GetSize(&XWinSize, &YWinSize);

        if (m_pCurrentImage->ImageData)
        {
            int blevel = m_pCurrentImage->FiltMin;
            int wlevel = m_pCurrentImage->FiltMax;
            m_pCurrentImage->CopyToImage(&m_displayedImage, blevel, wlevel, pFrame->Stretch_gamma);
        }

        int imageWidth   = m_displayedImage->GetWidth();
        int imageHeight  = m_displayedImage->GetHeight();

        // scale the image if necessary

        if (imageWidth != XWinSize || imageHeight != YWinSize)
        {
            // The image is not the exact right size -- figure out what to do.
            double xScaleFactor = imageWidth / (double)XWinSize;
            double yScaleFactor = imageHeight / (double)YWinSize;
            int newWidth = imageWidth;
            int newHeight = imageHeight;

            double newScaleFactor = (xScaleFactor > yScaleFactor) ?
                                    xScaleFactor :
                                    yScaleFactor;

//            Debug.Write(wxString::Format("xScaleFactor=%.2f, yScaleFactor=%.2f, newScaleFactor=%.2f\n", xScaleFactor,
//                    yScaleFactor, newScaleFactor));

            // we rescale the image if:
            // - The image is either too big
            // - The image is so small that at least one dimension is less
            //   than half the width of the window or
            // - The user has requested rescaling

            if (xScaleFactor > 1.0 || yScaleFactor > 1.0 ||
                xScaleFactor < 0.45 || yScaleFactor < 0.45 || m_scaleImage)
            {

                newWidth /= newScaleFactor;
                newHeight /= newScaleFactor;

                newScaleFactor = 1.0 / newScaleFactor;

                m_scaleFactor = newScaleFactor;

                if (imageWidth != newWidth || imageHeight != newHeight)
                {
//                    Debug.Write(wxString::Format("Resizing image to %d,%d\n", newWidth, newHeight));

                    if (newWidth > 0 && newHeight > 0)
                    {
                        wxImageResizeQuality quality = (pFrame->GetStarFindMode() == Star::FIND_PLANET) ? wxIMAGE_QUALITY_NORMAL: wxIMAGE_QUALITY_BILINEAR;
                        m_displayedImage->Rescale(newWidth, newHeight, quality);
                    }
                }
            }
            else
            {
                m_scaleFactor = 1.0;
            }
        }

        // important to provide explicit color for r,g,b, optional args to Size().
        // If default args are provided wxWidgets performs some expensive histogram
        // operations.
        wxBitmap DisplayedBitmap(m_displayedImage->Size(wxSize(XWinSize, YWinSize), wxPoint(0, 0), 0, 0, 0));
        memDC.SelectObject(DisplayedBitmap);

        dc.Blit(0, 0, DisplayedBitmap.GetWidth(), DisplayedBitmap.GetHeight(), &memDC, 0, 0, wxCOPY, false);

        int XImgSize = m_displayedImage->GetWidth();
        int YImgSize = m_displayedImage->GetHeight();

        if (m_overlayMode)
        {
            dc.SetPen(wxPen(wxColor(200,50,50)));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);

            switch (m_overlayMode)
            {
                case OVERLAY_BULLSEYE:
                {
                    int cx = XImgSize / 2;
                    int cy = YImgSize / 2;
                    dc.DrawCircle(cx,cy,25);
                    dc.DrawCircle(cx,cy,50);
                    dc.DrawCircle(cx,cy,100);
                    dc.DrawLine(0, cy, XImgSize, cy);
                    dc.DrawLine(cx, 0, cx, YImgSize);
                    break;
                }
                case OVERLAY_GRID_FINE:
                case OVERLAY_GRID_COARSE:
                {
                    int i;
                    int size = (m_overlayMode - 1) * 20;
                    for (i=size; i<XImgSize; i+=size)
                        dc.DrawLine(i,0,i,YImgSize);
                    for (i=size; i<YImgSize; i+=size)
                        dc.DrawLine(0,i,XImgSize,i);
                    break;
                }

                case OVERLAY_RADEC:
                {
                    Mount *mount = TheScope();
                    if (mount)
                    {
                        double StarX = CurrentPosition().X;
                        double StarY = CurrentPosition().Y;

                        double r = 15.0;
                        double rlabel = r + 9.0;

                        double wAngle = mount->IsCalibrated() ? mount->xAngle() : 0.0;
                        double eAngle = wAngle + M_PI;
                        GuideParity raParity = mount->RAParity();
                        if (raParity == GUIDE_PARITY_ODD)
                        {
                            // odd parity => West calibration pulses move scope East
                            //   => star moves West
                            //   => East vector is opposite direction from X calibration vector (West calibration direction)
                            eAngle += M_PI;
                        }
                        double cos_eangle = cos(eAngle);
                        double sin_eangle = sin(eAngle);
                        dc.SetPen(wxPen(pFrame->pGraphLog->GetRaOrDxColor(), 2, wxPENSTYLE_DOT));
                        dc.DrawLine(ROUND(StarX * m_scaleFactor + r * cos_eangle), ROUND(StarY * m_scaleFactor + r * sin_eangle),
                            ROUND(StarX * m_scaleFactor - r * cos_eangle), ROUND(StarY * m_scaleFactor - r * sin_eangle));
                        if (raParity != GUIDE_PARITY_UNKNOWN)
                        {
                            dc.SetTextForeground(pFrame->pGraphLog->GetRaOrDxColor());
                            dc.DrawText(_("E"),
                                ROUND(StarX * m_scaleFactor + rlabel * cos_eangle) - 4, ROUND(StarY * m_scaleFactor + rlabel * sin_eangle) - 6);
                        }

                        double nAngle = mount->IsCalibrated() ? mount->yAngle() : M_PI / 2.0;
                        GuideParity decParity = mount->DecParity();
                        if (decParity == GUIDE_PARITY_EVEN)
                        {
                            // even parity => North calibration pulses move scope North
                            //   => star moves South
                            //   => North vector is opposite direction from Y calibration vector (North calibration direction)
                            nAngle += M_PI;
                        }
                        double cos_nangle = cos(nAngle);
                        double sin_nangle = sin(nAngle);
                        dc.SetPen(wxPen(pFrame->pGraphLog->GetDecOrDyColor(), 2, wxPENSTYLE_DOT));
                        dc.DrawLine(ROUND(StarX * m_scaleFactor + r * cos_nangle), ROUND(StarY * m_scaleFactor + r * sin_nangle),
                            ROUND(StarX * m_scaleFactor - r * cos_nangle), ROUND(StarY * m_scaleFactor - r * sin_nangle));
                        if (decParity != GUIDE_PARITY_UNKNOWN)
                        {
                            dc.SetTextForeground(pFrame->pGraphLog->GetDecOrDyColor());
                            dc.DrawText(_("N"),
                                ROUND(StarX * m_scaleFactor + rlabel * cos_nangle) - 4, ROUND(StarY * m_scaleFactor + rlabel * sin_nangle) - 6);
                        }

                        wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
                        gc->SetPen(wxPen(pFrame->pGraphLog->GetRaOrDxColor(), 1, wxPENSTYLE_DOT));
                        double step = (double) YImgSize / 10.0;

                        double MidX = (double) XImgSize / 2.0;
                        double MidY = (double) YImgSize / 2.0;
                        gc->Rotate(eAngle);
                        gc->GetTransform().TransformPoint(&MidX, &MidY);
                        gc->Rotate(-eAngle);
                        gc->Translate((double) XImgSize / 2.0 - MidX, (double) YImgSize / 2.0 - MidY);
                        gc->Rotate(eAngle);
                        for (int i = -2; i < 12; i++) {
                            gc->StrokeLine(0.0,step * (double) i,
                                (double) XImgSize, step * (double) i);
                        }

                        MidX = (double) XImgSize / 2.0;
                        MidY = (double) YImgSize / 2.0;
                        gc->Rotate(-eAngle);
                        gc->Rotate(nAngle);
                        gc->GetTransform().TransformPoint(&MidX, &MidY);
                        gc->Rotate(-nAngle);
                        gc->Translate((double) XImgSize / 2.0 - MidX, (double) YImgSize / 2.0 - MidY);
                        gc->Rotate(nAngle);
                        gc->SetPen(wxPen(pFrame->pGraphLog->GetDecOrDyColor(),1,wxPENSTYLE_DOT));
                        for (int i = -2; i < 12; i++) {
                            gc->StrokeLine(0.0,step * (double) i,
                                (double) XImgSize, step * (double) i);
                        }
                        delete gc;
                    }
                    break;
                }

                case OVERLAY_SLIT:
                    if (m_overlaySlitCoords.size.GetWidth() > 0 && m_overlaySlitCoords.size.GetHeight() > 0)
                    {
                        if (m_scaleFactor == 1.0)
                        {
                            dc.DrawLines(5, m_overlaySlitCoords.corners);
                        }
                        else
                        {
                            wxPoint pt[5];
                            for (int i = 0; i < 5; i++)
                            {
                                pt[i].x = (int) floor(m_overlaySlitCoords.corners[i].x * m_scaleFactor);
                                pt[i].y = (int) floor(m_overlaySlitCoords.corners[i].y * m_scaleFactor);
                            }
                            dc.DrawLines(5, pt);
                        }
                    }
                    break;

                case OVERLAY_NONE:
                    break;
            }
        }

        if (m_defectMapPreview)
        {
            dc.SetPen(wxPen(wxColor(255, 0, 0), 1, wxPENSTYLE_SOLID));
            for (DefectMap::const_iterator it = m_defectMapPreview->begin(); it != m_defectMapPreview->end(); ++it)
            {
                const wxPoint& pt = *it;
                dc.DrawPoint((int)(pt.x * m_scaleFactor), (int)(pt.y * m_scaleFactor));
            }
        }

        // draw the lockpoint of there is one
        if (state > STATE_SELECTED)
        {
            double LockX = LockPosition().X;
            double LockY = LockPosition().Y;

            switch (state)
            {
                case STATE_UNINITIALIZED:
                case STATE_SELECTING:
                case STATE_SELECTED:
                case STATE_STOP:
                    break;
                case STATE_CALIBRATING_PRIMARY:
                case STATE_CALIBRATING_SECONDARY:
                    dc.SetPen(wxPen(wxColor(255,255,0),1, wxPENSTYLE_DOT));
                    break;
                case STATE_CALIBRATED:
                case STATE_GUIDING:
                    dc.SetPen(wxPen(wxColor(0,255,0)));
                    break;
            }

            dc.DrawLine(0, int(LockY * m_scaleFactor), XImgSize, int(LockY * m_scaleFactor));
            dc.DrawLine(int(LockX * m_scaleFactor), 0, int(LockX * m_scaleFactor), YImgSize);
        }

        // draw a polar alignment circle
        if (m_polarAlignCircleRadius)
        {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            wxPenStyle penStyle = m_polarAlignCircleCorrection == 1.0 ? wxPENSTYLE_DOT : wxPENSTYLE_SOLID;
            dc.SetPen(wxPen(wxColor(255,0,255), 1, penStyle));
            int radius = ROUND(m_polarAlignCircleRadius * m_polarAlignCircleCorrection * m_scaleFactor);
            dc.DrawCircle(m_polarAlignCircleCenter.X * m_scaleFactor,
                m_polarAlignCircleCenter.Y * m_scaleFactor, radius);
        }

        // draw static polar align stuff
        PolarDriftTool::PaintHelper(dc, m_scaleFactor);
        StaticPaTool::PaintHelper(dc, m_scaleFactor);

        if (IsPaused())
        {
            dc.SetTextForeground(*wxYELLOW);
            dc.DrawText(_("PAUSED"), 10, YWinSize - 20);
        }
        else if (pMount && !pMount->GetGuidingEnabled())
        {
            dc.SetTextForeground(*wxYELLOW);
            dc.DrawText(_("Guide output DISABLED"), 10, YWinSize - 20);
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

void Guider::UpdateImageDisplay(usImage *pImage)
{
    if (!pImage)
    {
        pImage = m_pCurrentImage;
    }

    Debug.Write(wxString::Format("UpdateImageDisplay: Size=(%d,%d) min=%u, max=%u, med=%u, FiltMin=%u, FiltMax=%u, Gamma=%.3f\n",
                                 pImage->Size.x, pImage->Size.y, pImage->MinADU, pImage->MaxADU, pImage->MedianADU,
                                 pImage->FiltMin, pImage->FiltMax, pFrame->Stretch_gamma));

    Refresh();
    Update();
}

void Guider::SetDefectMapPreview(const DefectMap *defectMap)
{
    m_defectMapPreview = defectMap;
    Refresh();
    Update();
}

bool Guider::SaveCurrentImage(const wxString& fileName)
{
    return m_pCurrentImage->Save(fileName);
}

void Guider::InvalidateLockPosition()
{
    if (m_lockPosition.IsValid())
        EvtServer.NotifyLockPositionLost();
    m_lockPosition.Invalidate();
    NudgeLockTool::UpdateNudgeLockControls();
}

bool Guider::SetLockPosition(const PHD_Point& position)
{
    bool bError = false;

    try
    {
        if (!position.IsValid())
        {
            throw ERROR_INFO("Point is not valid");
        }

        double x = position.X;
        double y = position.Y;
        Debug.Write(wxString::Format("setting lock position to (%.2f, %.2f)\n", x, y));

        if ((x < 0.0) || (x >= m_pCurrentImage->Size.x))
        {
            throw ERROR_INFO("invalid x value");
        }

        if ((y < 0.0) || (y >= m_pCurrentImage->Size.y))
        {
            throw ERROR_INFO("invalid y value");
        }

        if (!m_lockPosition.IsValid() || position.X != m_lockPosition.X || position.Y != m_lockPosition.Y)
        {
            EvtServer.NotifySetLockPosition(position);
            if (m_state == STATE_GUIDING)
            {
                // let guide algorithms react to the updated lock pos
                pMount->NotifyGuidingDithered(position.X - m_lockPosition.X, position.Y - m_lockPosition.Y, false);
                GuideLog.NotifySetLockPosition(this);
            }
            NudgeLockTool::UpdateNudgeLockControls();
        }

        m_lockPosition.SetXY(x, y);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        bError = true;
    }

    return bError;
}

bool Guider::SetLockPosToStarAtPosition(const PHD_Point& starPosHint)
{
    bool error = SetCurrentPosition(m_pCurrentImage, starPosHint);

    if (!error && CurrentPosition().IsValid())
    {
        SetLockPosition(CurrentPosition());
    }

    return error;
}

// distance to nearest edge
static double edgeDist(const PHD_Point& pt, const wxSize& size)
{
    return wxMin(pt.X, wxMin(size.GetWidth() - pt.X, wxMin(pt.Y, size.GetHeight() - pt.Y)));
}

bool Guider::MoveLockPosition(const PHD_Point& mountDeltaArg)
{
    bool err = false;

    try
    {
        if (!mountDeltaArg.IsValid())
        {
            throw ERROR_INFO("Point is not valid");
        }

        if (!pMount || !pMount->IsCalibrated())
        {
            throw ERROR_INFO("No mount");
        }

        const usImage *image = CurrentImage();
        if (!image)
        {
            throw ERROR_INFO("cannot move lock pos without an image");
        }

        // This loop is to handle dithers when the star is near the edge of the frame. The strategy
        // is to try reflecting the requested dither in 4 directions along the RA/Dec axes; if any
        // of the projections results in a valid lock position, use it. Otherwise, choose the
        // direction that moves farthest from the edge of the frame.

        PHD_Point cameraDelta, mountDelta;
        double dbest;

        for (int q = 0; q < 4; q++)
        {
            int sx = 1 - ((q & 1) << 1);
            int sy = 1 - (q & 2);

            PHD_Point tmpMount(mountDeltaArg.X * sx, mountDeltaArg.Y * sy);
            PHD_Point tmpCamera;

            if (pMount->TransformMountCoordinatesToCameraCoordinates(tmpMount, tmpCamera))
            {
                throw ERROR_INFO("Transform failed");
            }

            PHD_Point tmpLockPosition = m_lockPosition + tmpCamera;

            if (IsValidLockPosition(tmpLockPosition))
            {
                cameraDelta = tmpCamera;
                mountDelta = tmpMount;
                break;
            }

            Debug.Write("dither produces an invalid lock position, try a variation\n");

            double d = edgeDist(tmpLockPosition, image->Size);
            if (q == 0 || d > dbest)
            {
                cameraDelta = tmpCamera;
                mountDelta = tmpMount;
                dbest = d;
            }
        }

        PHD_Point newLockPosition = m_lockPosition + cameraDelta;
        if (SetLockPosition(newLockPosition))
        {
            throw ERROR_INFO("SetLockPosition failed");
        }

        // update average distance right away so GetCurrentDistance reflects the increased distance from the dither
        double dist = cameraDelta.Distance(), distRA = fabs(mountDelta.X);
        m_avgDistance += dist;
        m_avgDistanceLong += dist;
        m_avgDistanceRA += distRA;
        m_avgDistanceLongRA += distRA;

        if (IsFastRecenterEnabled())
        {
            m_ditherRecenterRemaining.SetXY(fabs(mountDelta.X), fabs(mountDelta.Y));
            m_ditherRecenterDir.x = mountDelta.X < 0.0 ? 1 : -1;
            m_ditherRecenterDir.y = mountDelta.Y < 0.0 ? 1 : -1;
            // make each step a bit less than the full search region distance to avoid losing the star
            double f = ((double) GetMaxMovePixels() * 0.7) / m_ditherRecenterRemaining.Distance();
            m_ditherRecenterStep.SetXY(f * m_ditherRecenterRemaining.X, f * m_ditherRecenterRemaining.Y);
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        err = true;
    }

    return err;
}

void Guider::SetState(GUIDER_STATE newState)
{
    try
    {
        Debug.Write(wxString::Format("Changing from state %s to %s\n", StateStr(m_state), StateStr(newState)));

        if (newState == STATE_STOP)
        {
            // we are going to stop looping exposures.  We should put
            // ourselves into a good state to restart looping later
            switch (m_state)
            {
                case STATE_UNINITIALIZED:
                case STATE_SELECTING:
                case STATE_SELECTED:
                    newState = m_state;
                    break;
                case STATE_CALIBRATING_PRIMARY:
                case STATE_CALIBRATING_SECONDARY:
                    // because we have done some moving here, we need to just
                    // start over...
                    newState = STATE_UNINITIALIZED;
                    break;
                case STATE_CALIBRATED:
                case STATE_GUIDING:
                    newState = STATE_SELECTED;
                    break;
                case STATE_STOP:
                    break;
            }
        }

        assert(newState != STATE_STOP);

        if (newState > m_state + 1)
        {
            Debug.Write(wxString::Format("Cannot transition from %s to newState=%s\n", StateStr(m_state), StateStr(newState)));
            throw ERROR_INFO("Illegal state transition");
        }

        GUIDER_STATE requestedState = newState;

        switch (requestedState)
        {
            case STATE_UNINITIALIZED:
                InvalidateLockPosition();
                InvalidateCurrentPosition();
                newState = STATE_SELECTING;
                break;
            case STATE_SELECTED:
                break;
            case STATE_CALIBRATING_PRIMARY:
                if (!pMount->IsCalibrated())
                {
                    pMount->ResetErrorCount();
                    if (pMount->BeginCalibration(CurrentPosition()))
                    {
                        newState = STATE_UNINITIALIZED;
                        Debug.Write(ERROR_INFO("pMount->BeginCalibration failed"));
                    }
                    else
                    {
                        GuideLog.StartCalibration(pMount);
                        EvtServer.NotifyStartCalibration(pMount);
                    }
                    break;
                }
                // fall through
            case STATE_CALIBRATING_SECONDARY:
                if (!pSecondaryMount || !pSecondaryMount->IsConnected())
                {
                    newState = STATE_CALIBRATED;
                }
                else if (!pSecondaryMount->IsCalibrated())
                {
                    pSecondaryMount->ResetErrorCount();
                    if (pSecondaryMount->BeginCalibration(CurrentPosition()))
                    {
                        newState = STATE_UNINITIALIZED;
                        Debug.Write(ERROR_INFO("pSecondaryMount->BeginCalibration failed"));
                    }
                    else
                    {
                        GuideLog.StartCalibration(pSecondaryMount);
                        EvtServer.NotifyStartCalibration(pSecondaryMount);
                    }
                }
                break;
            case STATE_GUIDING:
                assert(pMount);

                m_ditherRecenterRemaining.Invalidate();  // reset dither fast recenter state

                pMount->AdjustCalibrationForScopePointing();
                if (pSecondaryMount)
                {
                    pSecondaryMount->AdjustCalibrationForScopePointing();
                }

                pFrame->UpdateStatusBarCalibrationStatus();

                if (m_lockPosition.IsValid() && m_lockPosIsSticky)
                {
                    Debug.Write("keeping sticky lock position\n");
                }
                else
                {
                    SetLockPosition(CurrentPosition());
                }
                break;

            case STATE_SELECTING:
            case STATE_CALIBRATED:
            case STATE_STOP:
                break;
        }

        if (newState >= requestedState)
        {
            m_state = newState;
            Debug.Write(wxString::Format("guider state => %s\n", StateStr(m_state)));
        }
        else
        {
            Debug.Write(wxString::Format("SetState recurses newState %s\n", StateStr(newState)));
            SetState(newState);
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }
}

void Guider::UpdateCurrentDistance(double distance, double distanceRA)
{
    m_starFoundTimestamp = wxDateTime::GetTimeNow();

    if (IsGuiding())
    {
        // update moving average distance
        static double const alpha = .3; // moderately high weighting for latest sample
        m_avgDistance += alpha * (distance - m_avgDistance);
        m_avgDistanceRA += alpha * (distanceRA - m_avgDistanceRA);

        ++m_avgDistanceCnt;

        if (m_avgDistanceCnt < 10)
        {
            // initialize smoothed running avg with mean of first 10 pts
            m_avgDistanceLong += (distance - m_avgDistanceLong) / m_avgDistanceCnt;
            m_avgDistanceLongRA += (distanceRA - m_avgDistanceLongRA) / m_avgDistanceCnt;
        }
        else
        {
            static double const alpha_long = .045; // heavy smoothing, low weighting for latest sample .045 => 15 frame half-life
            m_avgDistanceLong += alpha_long * (distance - m_avgDistanceLong);
            m_avgDistanceLongRA += alpha_long * (distanceRA - m_avgDistanceLongRA);
        }
    }
    else
    {
        // not yet guiding, reinitialize average distance
        m_avgDistance = m_avgDistanceLong = distance;
        m_avgDistanceRA = m_avgDistanceLongRA = distanceRA;
        m_avgDistanceCnt = 1;
    }

    if (m_avgDistanceNeedReset)
    {
        // avg distance history invalidated
        m_avgDistance = m_avgDistanceLong = distance;
        m_avgDistanceRA = m_avgDistanceLongRA = distanceRA;
        m_avgDistanceCnt = 1;
        m_avgDistanceNeedReset = false;
    }
}

inline static double CurrentError(time_t starFoundTimestamp, double avgDist)
{
    enum { THRESHOLD_SECONDS = 20 };
    static double const LARGE_DISTANCE = 100.0;

    if (!starFoundTimestamp)
    {
        return LARGE_DISTANCE;
    }

    if (wxDateTime::GetTimeNow() - starFoundTimestamp > THRESHOLD_SECONDS)
    {
        return LARGE_DISTANCE;
    }

    return avgDist;
}

double Guider::CurrentError(bool raOnly)
{
    return ::CurrentError(m_starFoundTimestamp, raOnly ? m_avgDistanceRA : m_avgDistance);
}

double Guider::CurrentErrorSmoothed(bool raOnly)
{
    return ::CurrentError(m_starFoundTimestamp, raOnly ? m_avgDistanceLongRA : m_avgDistanceLong);
}

void Guider::StartGuiding()
{
    // we set the state to calibrating.  The state machine will
    // automatically move from calibrating->calibrated->guiding
    // when it can
    SetState(STATE_CALIBRATING_PRIMARY);
}

void Guider::StopGuiding()
{
    // first, send a notification that we stopped
    switch (m_state)
    {
        case STATE_UNINITIALIZED:
        case STATE_SELECTING:
        case STATE_SELECTED:
            break;
        case STATE_CALIBRATING_PRIMARY:
        case STATE_CALIBRATING_SECONDARY:
        case STATE_CALIBRATED:
            EvtServer.NotifyCalibrationFailed(m_state == STATE_CALIBRATING_SECONDARY ? pSecondaryMount : pMount,
                _("Calibration manually stopped"));
            // fall through to notify guiding stopped
        case STATE_GUIDING:
            if ((!pMount || !pMount->IsBusy()) && (!pSecondaryMount || !pSecondaryMount->IsBusy()))
            {
                // Notify guiding stopped if there are no outstanding guide steps.  The Guiding
                // Stopped notification must come after the final GuideStep notification otherwise
                // event server clients and the guide log will show the guide step happening after
                // guiding stopped.

                pFrame->NotifyGuidingStopped();
            }
            break;
        case STATE_STOP:
            break;
    }

    SetState(STATE_STOP);
}

void Guider::Reset(bool fullReset)
{
    SetState(STATE_UNINITIALIZED);
    if (fullReset)
    {
        InvalidateCurrentPosition(true);
    }
}

// Called from the alert to offer auto-restore calibration
static void SetAutoLoad(long param)
{
    pFrame->SetAutoLoadCalibration(true);
    pFrame->m_infoBar->Dismiss();
}

// Generate an alert if the user is likely to be missing the opportunity for auto-restore of
// the just-completed calibration
static void CheckCalibrationAutoLoad()
{
    int autoLoadProfileVal = pConfig->Profile.GetInt("/AutoLoadCalibration", -1);
    bool shouldAutoLoad = pPointingSource && pPointingSource->CanReportPosition();

    if (autoLoadProfileVal == -1)
    {
        // new profile, assert appropriate choice as default
        pFrame->SetAutoLoadCalibration(shouldAutoLoad);
    }
    else if (autoLoadProfileVal == 0 && shouldAutoLoad)
    {
        bool alreadyAsked = pConfig->Profile.GetBoolean("/AlreadyAskedCalibAutoload", false);

        if (!alreadyAsked)
        {
            wxString msg = wxString::Format(_("Do you want to automatically restore this calibration whenever the profile is used?"));
            pFrame->Alert(msg, 0, "Auto-restore", &SetAutoLoad, 0, false, 0);
            pConfig->Profile.SetBoolean("/AlreadyAskedCalibAutoload", true);
        }
    }
}

void Guider::DisplayImage(usImage *img)
{
    if (IsCalibratingOrGuiding())
        return;

    // switch in the new image
    usImage *prev = m_pCurrentImage;
    m_pCurrentImage = img;

    ImageLogger::SaveImage(prev);

    UpdateImageDisplay();
}

inline static bool
IsLoopingState(GUIDER_STATE state)
{
    // returns true for looping, but non-guiding states
    switch (state)
    {
    case STATE_UNINITIALIZED:
    case STATE_SELECTING:
    case STATE_SELECTED:
        return true;

    case STATE_CALIBRATING_PRIMARY:
    case STATE_CALIBRATING_SECONDARY:
    case STATE_CALIBRATED:
    case STATE_GUIDING:
    case STATE_STOP:
        return false;
    }

    return false;
}

/*************  A new image is ready ************************/

void Guider::UpdateGuideState(usImage *pImage, bool bStopping)
{
    wxString statusMessage;
    bool someException = false;

    try
    {
        Debug.Write(wxString::Format("UpdateGuideState(): m_state=%d\n", m_state));

        if (pImage)
        {
            // switch in the new image

            usImage *pPrevImage = m_pCurrentImage;
            m_pCurrentImage = pImage;

            ImageLogger::SaveImage(pPrevImage);
        }
        else
        {
            pImage = m_pCurrentImage;
        }

        if (pFrame && pFrame->pStatsWin)
            pFrame->pStatsWin->UpdateImageSize(pImage->Size);

        if (bStopping)
        {
            StopGuiding();
            statusMessage = _("Stopped Guiding");
            throw THROW_INFO("Stopped Guiding");
        }

        assert(!pMount || !pMount->IsBusy());

        // shift lock position
        if (LockPosShiftEnabled() && IsGuiding())
        {
            if (ShiftLockPosition())
            {
                EvtServer.NotifyLockShiftLimitReached();
                pFrame->Alert(_("Shifted lock position outside allowable area. Lock Position Shift disabled."));
                EnableLockPosShift(false);
            }
            NudgeLockTool::UpdateNudgeLockControls();
        }

        GuiderOffset ofs;
        FrameDroppedInfo info;

        if (UpdateCurrentPosition(pImage, &ofs, &info))           // true means error
        {
            info.frameNumber = pImage->FrameNum;
            info.time = pFrame->TimeSinceGuidingStarted();
            info.avgDist = pFrame->CurrentGuideError();

            switch (m_state)
            {
                case STATE_UNINITIALIZED:
                case STATE_SELECTING:
                    EvtServer.NotifyLooping(pImage->FrameNum, nullptr, &info);
                    break;
                case STATE_SELECTED:
                    // we had a current position and lost it
                    EvtServer.NotifyLooping(pImage->FrameNum, nullptr, &info);
                    if (!m_ignoreLostStarLooping)
                    {
                        SetState(STATE_UNINITIALIZED);
                        EvtServer.NotifyStarLost(info);
                    }
                    StaticPaTool::NotifyStarLost();
                    break;
                case STATE_CALIBRATING_PRIMARY:
                case STATE_CALIBRATING_SECONDARY:
                    GuideLog.CalibrationFrameDropped(info);
                    Debug.Write("Star lost during calibration... blundering on\n");
                    EvtServer.NotifyStarLost(info);
                    pFrame->StatusMsg(_("star lost"));
                    break;
                case STATE_GUIDING:
                {
                    GuideLog.FrameDropped(info);
                    EvtServer.NotifyStarLost(info);
                    GuidingAssistant::NotifyFrameDropped(info);
                    pFrame->pGraphLog->AppendData(info);

                    // allow guide algorithms to attempt dead reckoning
                    static GuiderOffset ZERO_OFS;
                    pFrame->SchedulePrimaryMove(pMount, ZERO_OFS, MOVEOPTS_DEDUCED_MOVE);

                    // Don't blink and beep during planetary guiding pause state
                    if (!m_Planet.GetDetectionPausedState())
                    {
                        wxColor prevColor = GetBackgroundColour();
                        SetBackgroundColour(wxColour(64, 0, 0));
                        ClearBackground();
                        if (pFrame->GetBeepForLostStar())
                            wxBell();
                        wxMilliSleep(100);
                        SetBackgroundColour(prevColor);
                    }
                    break;
                }

                case STATE_CALIBRATED:
                case STATE_STOP:
                    break;
            }

            statusMessage = info.status;
            throw THROW_INFO("unable to update current position");
        }

        statusMessage = info.status;

        if (IsLoopingState(m_state))
            EvtServer.NotifyLooping(pImage->FrameNum, &PrimaryStar(), nullptr);

        // we have a star selected, so re-enable subframes
        if (m_forceFullFrame)
        {
            Debug.Write("setting force full frames = false\n");
            m_forceFullFrame = false;
        }

        if (IsPaused())
        {
            if (m_state == STATE_GUIDING)
            {
                // allow guide algorithms to attempt dead reckoning
                static GuiderOffset ZERO_OFS;
                pFrame->SchedulePrimaryMove(pMount, ZERO_OFS, MOVEOPTS_DEDUCED_MOVE);
            }

            statusMessage = _("Paused") + (GetPauseType() == PAUSE_FULL ? _("/full") : _("/looping"));
            throw THROW_INFO("Skipping frame - guider is paused");
        }

        switch (m_state)
        {
            case STATE_SELECTING:
                assert(CurrentPosition().IsValid());
                SetLockPosition(CurrentPosition());
                Debug.Write("CurrentPosition() valid, moving to STATE_SELECTED\n");
                EvtServer.NotifyStarSelected(CurrentPosition());
                SetState(STATE_SELECTED);
                break;
            case STATE_SELECTED:
                if (!StaticPaTool::UpdateState())
                {
                    SetState(STATE_UNINITIALIZED);
                    statusMessage = _("Static PA rotation failed");
                    throw ERROR_INFO("Static PA rotation failed");
                }
                if (!PolarDriftTool::UpdateState())
                {
                    SetState(STATE_UNINITIALIZED);
                    statusMessage = _("Polar Drift PA drift failed");
                    throw ERROR_INFO("Polar Drift PA drift failed");
                }
                break;
            case STATE_CALIBRATING_PRIMARY:
                if (!pMount->IsCalibrated())
                {
                    if (pMount->UpdateCalibrationState(CurrentPosition()))
                    {
                        SetState(STATE_UNINITIALIZED);
                        statusMessage = pMount->IsStepGuider() ? _("AO calibration failed") : _("calibration failed");
                        throw ERROR_INFO("Calibration failed");
                    }

                    if (!pMount->IsCalibrated())
                    {
                        break;
                    }
                }

                SetState(STATE_CALIBRATING_SECONDARY);

                if (m_state == STATE_CALIBRATING_SECONDARY)
                {
                    // if we really have a secondary mount, and it isn't calibrated,
                    // we need to take another exposure before falling into the code
                    // below.  If we don't have one, or it is calibrated, we can fall
                    // through.  If we don't fall through, we end up displaying a frame
                    // which has the lockpoint in the wrong place, and while I thought I
                    // could live with it when I originally wrote the code, it bothered
                    // me so I did this.  Ick.
                    break;
                }

                // Fall through
            case STATE_CALIBRATING_SECONDARY:
                if (pSecondaryMount && pSecondaryMount->IsConnected())
                {
                    if (!pSecondaryMount->IsCalibrated())
                    {
                        if (pSecondaryMount->UpdateCalibrationState(CurrentPosition()))
                        {
                            SetState(STATE_UNINITIALIZED);
                            statusMessage = _("calibration failed");
                            throw ERROR_INFO("Calibration failed");
                        }
                    }

                    if (!pSecondaryMount->IsCalibrated())
                    {
                        break;
                    }
                }
                assert(!pSecondaryMount || !pSecondaryMount->IsConnected() || pSecondaryMount->IsCalibrated());

                SetState(STATE_CALIBRATED);
                // fall through
            case STATE_CALIBRATED:
                assert(m_state == STATE_CALIBRATED);
                SetState(STATE_GUIDING);

                pFrame->NotifyGuidingStarted();

                // camera angle is known, so ok to calculate shift rate camera coords
                UpdateLockPosShiftCameraCoords();
                if (LockPosShiftEnabled())
                    GuideLog.NotifyLockShiftParams(m_lockPosShift, m_lockPosition.ShiftRate());

                CheckCalibrationAutoLoad();
                break;
            case STATE_GUIDING:
                if (m_ditherRecenterRemaining.IsValid())
                {
                    // fast recenter after dither taking large steps and bypassing
                    // guide algorithms

                    PHD_Point step(wxMin(m_ditherRecenterRemaining.X, m_ditherRecenterStep.X),
                                   wxMin(m_ditherRecenterRemaining.Y, m_ditherRecenterStep.Y));

                    Debug.Write(wxString::Format("dither recenter: remaining=(%.1f,%.1f) step=(%.1f,%.1f)\n",
                        m_ditherRecenterRemaining.X * m_ditherRecenterDir.x,
                        m_ditherRecenterRemaining.Y * m_ditherRecenterDir.y,
                        step.X * m_ditherRecenterDir.x, step.Y * m_ditherRecenterDir.y));

                    m_ditherRecenterRemaining -= step;
                    if (m_ditherRecenterRemaining.X < 0.5 && m_ditherRecenterRemaining.Y < 0.5)
                    {
                        // fast recenter is done
                        m_ditherRecenterRemaining.Invalidate();
                        // reset distance tracker
                        m_avgDistanceNeedReset = true;
                    }

                    ofs.mountOfs.SetXY(step.X * m_ditherRecenterDir.x, step.Y * m_ditherRecenterDir.y);
                    pMount->TransformMountCoordinatesToCameraCoordinates(ofs.mountOfs, ofs.cameraOfs);
                    pFrame->SchedulePrimaryMove(pMount, ofs, MOVEOPTS_RECOVERY_MOVE);
                    // let guide algorithms know about the direct move
                    pMount->NotifyDirectMove(ofs.mountOfs);
                }
                else if (m_measurementMode)
                {
                    GuidingAssistant::NotifyBacklashStep(CurrentPosition());
                }
                else
                {
                    // ordinary guide step
                    s_deflectionLogger.Log(CurrentPosition());
                    pFrame->SchedulePrimaryMove(pMount, ofs, MOVEOPTS_GUIDE_STEP);
                }
                break;

            case STATE_UNINITIALIZED:
            case STATE_STOP:
                break;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
        someException = true;
    }

    // during calibration, the mount is responsible for updating the status message
    if (someException && m_state != STATE_CALIBRATING_PRIMARY && m_state != STATE_CALIBRATING_SECONDARY)
    {
        pFrame->StatusMsg(statusMessage);
    }

    if (m_measurementMode && m_state != STATE_GUIDING)
    {
        GuidingAssistant::NotifyBacklashError();
        m_measurementMode = false;
    }

    pFrame->UpdateButtonsStatus();

    UpdateImageDisplay(pImage);

    Debug.AddLine("UpdateGuideState exits: " + statusMessage);
}

bool Guider::ShiftLockPosition()
{
    m_lockPosition.UpdateShift();
    bool isValid = IsValidLockPosition(m_lockPosition);
    Debug.Write(wxString::Format("ShiftLockPos: new pos = %.2f, %.2f valid=%d\n",
                  m_lockPosition.X, m_lockPosition.Y, isValid));
    return !isValid;
}

void Guider::SetLockPosShiftRate(const PHD_Point& rate, GRAPH_UNITS units, bool isMountCoords, bool updateToolWin)
{
    Debug.Write(wxString::Format("SetLockPosShiftRate: rate = %.2f,%.2f units = %d isMountCoords = %d\n",
        rate.X, rate.Y, units, isMountCoords));

    m_lockPosShift.shiftRate = rate;
    m_lockPosShift.shiftUnits = units;
    m_lockPosShift.shiftIsMountCoords = isMountCoords;

    CometTool::UpdateCometToolControls(updateToolWin);

    if (m_state == STATE_CALIBRATED || m_state == STATE_GUIDING)
    {
        UpdateLockPosShiftCameraCoords();
        if (LockPosShiftEnabled())
        {
            GuideLog.NotifyLockShiftParams(m_lockPosShift, m_lockPosition.ShiftRate());
        }
    }
}

void Guider::EnableLockPosShift(bool enable)
{
    if (enable != m_lockPosShift.shiftEnabled)
    {
        Debug.Write(wxString::Format("EnableLockPosShift: enable = %d\n", enable));
        m_lockPosShift.shiftEnabled = enable;
        if (enable)
        {
            m_lockPosition.BeginShift();
        }
        if (m_state == STATE_CALIBRATED || m_state == STATE_GUIDING)
        {
            GuideLog.NotifyLockShiftParams(m_lockPosShift, m_lockPosition.ShiftRate());
        }

        CometTool::UpdateCometToolControls(false);
    }
}

void Guider::UpdateLockPosShiftCameraCoords()
{
    if (!m_lockPosShift.shiftRate.IsValid())
    {
        Debug.Write("UpdateLockPosShiftCameraCoords: no shift rate set\n");
        m_lockPosition.DisableShift();
        return;
    }

    PHD_Point rate(0., 0.);

    // convert shift rate to camera coordinates
    if (m_lockPosShift.shiftIsMountCoords)
    {
        PHD_Point radec_rates = m_lockPosShift.shiftRate;

        Debug.Write(wxString::Format("UpdateLockPosShiftCameraCoords: shift rate mount coords = %.2f,%.2f\n",
            radec_rates.X, radec_rates.Y));

        Mount *scope = TheScope();
        if (scope)
        {
            if (m_lockPosShift.shiftUnits == UNIT_ARCSEC)
            {
                // if rates are RA/Dec arc-seconds, assume they are ephemeris rates

                // account for parity if known
                GuideParity raParity = scope->RAParity();
                GuideParity decParity = scope->DecParity();
                if (raParity != GUIDE_PARITY_UNKNOWN || decParity != GUIDE_PARITY_UNKNOWN)
                {
                    if (raParity == GUIDE_PARITY_ODD)
                        radec_rates.X = -radec_rates.X;
                    if (decParity == GUIDE_PARITY_ODD)
                        radec_rates.Y = -radec_rates.Y;

                    Debug.Write(wxString::Format("UpdateLockPosShiftCameraCoords: after parity adjustment: %.2f,%.2f\n", radec_rates.X, radec_rates.Y));
                }

                // account for scope declination
                if (pPointingSource)
                {
                    double dec = pPointingSource->GetDeclinationRadians();
                    if (dec != UNKNOWN_DECLINATION)
                    {
                        radec_rates.X *= cos(dec);
                        Debug.Write(wxString::Format("UpdateLockPosShiftCameraCoords: RA shift rate adjusted for declination %.1f\n", degrees(dec)));
                    }
                }
            }

            scope->TransformMountCoordinatesToCameraCoordinates(radec_rates, rate);
        }
    }
    else
    {
        rate = m_lockPosShift.shiftRate;
    }

    Debug.Write(wxString::Format("UpdateLockPosShiftCameraCoords: shift rate camera coords = %.2f,%.2f %s/hr\n",
                  rate.X, rate.Y, m_lockPosShift.shiftUnits == UNIT_ARCSEC ? "arcsec" : "pixels"));

    // convert arc-seconds to pixels
    if (m_lockPosShift.shiftUnits == UNIT_ARCSEC)
    {
        rate /= pFrame->GetCameraPixelScale();
    }
    rate /= 3600.0;  // per hour => per second

    Debug.Write(wxString::Format("UpdateLockPosShiftCameraCoords: shift rate %.2g,%.2g px/sec\n",
        rate.X, rate.Y));

    m_lockPosition.SetShiftRate(rate.X, rate.Y);
}

wxString Guider::GetSettingsSummary() const
{
    // return a loggable summary of current global configs managed by MyFrame
    return wxEmptyString;
}

Guider::GuiderConfigDialogPane *Guider::GetConfigDialogPane(wxWindow *pParent)
{
    return new GuiderConfigDialogPane(pParent, this);
}

Guider::GuiderConfigDialogPane::GuiderConfigDialogPane(wxWindow *pParent, Guider *pGuider)
: ConfigDialogPane(_("Guider Settings"), pParent)
{
    m_pGuider = pGuider;
}

void Guider::GuiderConfigDialogPane::LayoutControls(Guider *pGuider, BrainCtrlIdMap& CtrlMap)
{
    wxSizerFlags def_flags = wxSizerFlags(0).Border(wxALL, 5).Expand();

    wxStaticBoxSizer *pStarTrack = new wxStaticBoxSizer(wxVERTICAL, m_pParent, _("Guide star tracking"));
    wxStaticBoxSizer *pCalib = new wxStaticBoxSizer(wxVERTICAL, m_pParent, _("Calibration"));
    wxStaticBoxSizer *pShared = new wxStaticBoxSizer(wxVERTICAL, m_pParent, _("Shared Parameters"));
    wxFlexGridSizer *pCalibSizer = new wxFlexGridSizer(3, 2, 10, 10);
    wxFlexGridSizer *pSharedSizer = new wxFlexGridSizer(2, 2, 10, 10);

    pStarTrack->Add(GetSizerCtrl(CtrlMap, AD_szStarTracking), def_flags);
    pStarTrack->Layout();

    pCalibSizer->Add(GetSizerCtrl(CtrlMap, AD_szFocalLength));
    pCalibSizer->Add(GetSizerCtrl(CtrlMap, AD_szCalibrationDuration), wxSizerFlags(0).Border(wxLEFT, 90));
    pCalibSizer->Add(GetSingleCtrl(CtrlMap, AD_cbAutoRestoreCal));
    pCalibSizer->Add(GetSingleCtrl(CtrlMap, AD_cbAssumeOrthogonal), wxSizerFlags(0).Border(wxLEFT, 90));
    CondAddCtrl(pCalibSizer, CtrlMap, AD_cbClearCalibration);
    CondAddCtrl(pCalibSizer, CtrlMap, AD_cbUseDecComp, wxSizerFlags(0).Border(wxLEFT, 90));
    pCalib->Add(pCalibSizer, def_flags);
    pCalib->Layout();

    // Minor ordering to have "no-mount" condition look ok
    pSharedSizer->Add(GetSingleCtrl(CtrlMap, AD_cbScaleImages));
    pSharedSizer->Add(GetSingleCtrl(CtrlMap, AD_cbFastRecenter), wxSizerFlags(0).Border(wxLEFT, 35));
    CondAddCtrl(pSharedSizer, CtrlMap, AD_cbReverseDecOnFlip);
    CondAddCtrl(pSharedSizer, CtrlMap, AD_cbEnableGuiding, wxSizerFlags(0).Border(wxLEFT, 35));
    CondAddCtrl(pSharedSizer, CtrlMap, AD_cbSlewDetection);
    pShared->Add(pSharedSizer, def_flags);
    pShared->Layout();

    this->Add(pStarTrack, def_flags);
    this->Add(pCalib, def_flags);
    this->Add(pShared, def_flags);
    Fit(m_pParent);

}

GuiderConfigDialogCtrlSet *Guider::GetConfigDialogCtrlSet(wxWindow *pParent, Guider *pGuider, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap)
{
    return new GuiderConfigDialogCtrlSet(pParent, pGuider, pAdvancedDialog, CtrlMap);
}

GuiderConfigDialogCtrlSet::GuiderConfigDialogCtrlSet(wxWindow *pParent, Guider *pGuider, AdvancedDialog* pAdvancedDialog, BrainCtrlIdMap& CtrlMap) :
ConfigDialogCtrlSet(pParent, pAdvancedDialog, CtrlMap)
{
    assert(pGuider);

    m_pGuider = pGuider;

    m_pScaleImage = new wxCheckBox(GetParentWindow(AD_cbScaleImages), wxID_ANY, _("Always scale images"));
    AddCtrl(CtrlMap, AD_cbScaleImages, m_pScaleImage, _("Always scale images to fill window"));

    m_pEnableFastRecenter = new wxCheckBox(GetParentWindow(AD_cbFastRecenter), wxID_ANY, _("Fast recenter after calibration or dither"));
    AddCtrl(CtrlMap, AD_cbFastRecenter, m_pEnableFastRecenter, _("Speed up calibration and dithering by using larger guide pulses to return the star to the center position. Un-check to use the old, slower method of recentering after calibration or dither."));
}

void GuiderConfigDialogCtrlSet::LoadValues()
{
    m_pEnableFastRecenter->SetValue(m_pGuider->IsFastRecenterEnabled());
    m_pScaleImage->SetValue(m_pGuider->GetScaleImage());
}

void GuiderConfigDialogCtrlSet::UnloadValues()
{
    m_pGuider->EnableFastRecenter(m_pEnableFastRecenter->GetValue());
    m_pGuider->SetScaleImage(m_pScaleImage->GetValue());
}

EXPOSED_STATE Guider::GetExposedState()
{
    EXPOSED_STATE rval;
    Guider *guider = pFrame->pGuider;

    if (!guider)
        rval = EXPOSED_STATE_NONE;

    else if (guider->IsPaused())
        rval = EXPOSED_STATE_PAUSED;

    else if (!pFrame->CaptureActive)
        rval = EXPOSED_STATE_NONE;

    else
    {
        // map the guider internal state into a server reported state

        switch (guider->GetState())
        {
            case STATE_UNINITIALIZED:
            case STATE_STOP:
            default:
                rval = EXPOSED_STATE_NONE;
                break;

            case STATE_SELECTING:
                // only report "looping" if no star is selected
                if (guider->CurrentPosition().IsValid())
                    rval = EXPOSED_STATE_SELECTED;
                else
                    rval = EXPOSED_STATE_LOOPING;
                break;

            case STATE_SELECTED:
            case STATE_CALIBRATED:
                rval = EXPOSED_STATE_SELECTED;
                break;

            case STATE_CALIBRATING_PRIMARY:
            case STATE_CALIBRATING_SECONDARY:
                rval = EXPOSED_STATE_CALIBRATING;
                break;

            case STATE_GUIDING:
                if (guider->IsLocked())
                    rval = EXPOSED_STATE_GUIDING_LOCKED;
                else
                    rval = EXPOSED_STATE_GUIDING_LOST;
        }

        Debug.Write(wxString::Format("case statement mapped state %d to %d\n", guider->GetState(), rval));
    }

    return rval;
}

void Guider::SetMinStarHFD(double val)
{
    Debug.Write(wxString::Format("Setting StarMinHFD = %.2f\n", val));
    pConfig->Profile.SetDouble("/guider/StarMinHFD", val);
    m_minStarHFD = val;
}

// Minimum star SNR for auto-find; minimum SNR for normal star tracking is hard-wired to 3
void Guider::SetAFMinStarSNR(double val)
{
    Debug.Write(wxString::Format("Setting StarMinSNR = %0.1f\n", val));
    pConfig->Profile.SetDouble("/guider/StarMinSNR", val);
    m_minAFStarSNR = val;
}

// Maximum star HFD
void Guider::SetMaxStarHFD(double val)
{
    Debug.Write(wxString::Format("Setting MaxHFD = %0.1f\n", val));
    pConfig->Profile.SetDouble("/guider/StarMaxHFD", val);
    m_maxStarHFD = val;
}

void Guider::SetAutoSelDownsample(unsigned int val)
{
    Debug.Write(wxString::Format("Setting AutoSelDownsample = %u\n", val));
    pConfig->Profile.SetInt("/guider/AutoSelDownsample", val);
    m_autoSelDownsample = val;
}

void Guider::SetBookmarksShown(bool show)
{
    bool prev = m_showBookmarks;
    m_showBookmarks = show;
    if (prev != show && m_bookmarks.size())
    {
        Refresh();
        Update();
    }
}

void Guider::ToggleShowBookmarks()
{
    SetBookmarksShown(!m_showBookmarks);
}

void Guider::DeleteAllBookmarks()
{
    if (m_bookmarks.size())
    {
        bool confirmed = ConfirmDialog::Confirm(_("Are you sure you want to delete all Bookmarks?"),
            "/delete_all_bookmarks_ok");
        if (confirmed)
        {
            m_bookmarks.clear();
            SaveBookmarks(m_bookmarks);
            if (m_showBookmarks)
            {
                Refresh();
                Update();
            }
        }
    }
}

static bool IsClose(const wxRealPoint& p1, const wxRealPoint& p2, double tolerance)
{
    return fabs(p1.x - p2.x) <= tolerance &&
        fabs(p1.y - p2.y) <= tolerance;
}

static std::vector<wxRealPoint>::iterator FindBookmark(const wxRealPoint& pos, std::vector<wxRealPoint>& vec)
{
    static const double TOLERANCE = 6.0;
    std::vector<wxRealPoint>::iterator it;
    for (it = vec.begin(); it != vec.end(); ++it)
        if (IsClose(*it, pos, TOLERANCE))
            break;
    return it;
}

void Guider::ToggleBookmark(const wxRealPoint& pos)
{
    std::vector<wxRealPoint>::iterator it = FindBookmark(pos, m_bookmarks);

    if (it == m_bookmarks.end())
        m_bookmarks.push_back(pos);
    else
        m_bookmarks.erase(it);

    SaveBookmarks(m_bookmarks);
}

static bool BookmarkPos(const PHD_Point& pos, std::vector<wxRealPoint>& vec)
{
    if (pos.IsValid())
    {
        wxRealPoint pt(pos.X, pos.Y);
        std::vector<wxRealPoint>::iterator it = FindBookmark(pt, vec);
        if (it != vec.end())
            vec.erase(it);
        vec.push_back(pt);
        SaveBookmarks(vec);
        return true;
    }
    return false;
}

void Guider::BookmarkLockPosition()
{
    if (BookmarkPos(LockPosition(), m_bookmarks) && m_showBookmarks)
    {
        Refresh();
        Update();
    }
}

void Guider::BookmarkCurPosition()
{
    if (BookmarkPos(CurrentPosition(), m_bookmarks) && m_showBookmarks)
    {
        Refresh();
        Update();
    }
}
