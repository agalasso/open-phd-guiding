/*
 *  staticpa_toolwin.cpp
 *  PHD Guiding
 *
 *  Created by Ken Self
 *  Copyright (c) 2017 Ken Self
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
 *    Neither the name of openphdguiding.org nor the names of its
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
#include "staticpa_tool.h"
#include "staticpa_toolwin.h"

#include <wx/gbsizer.h>
#include <wx/valnum.h>
#include <wx/textwrapper.h>

//==================================
BEGIN_EVENT_TABLE(StaticPaToolWin, wxFrame)
EVT_BUTTON(ID_INSTR, StaticPaToolWin::OnInstr)
EVT_CHOICE(ID_HEMI, StaticPaToolWin::OnHemi)
EVT_SPINCTRLDOUBLE(ID_HA, StaticPaToolWin::OnHa)
EVT_CHECKBOX(ID_MANUAL, StaticPaToolWin::OnManual)
EVT_CHECKBOX(ID_FLIP, StaticPaToolWin::OnFlip)
EVT_CHECKBOX(ID_ORBIT, StaticPaToolWin::OnOrbit)
EVT_CHOICE(ID_REFSTAR, StaticPaToolWin::OnRefStar)
EVT_BUTTON(ID_ROTATE, StaticPaToolWin::OnRotate)
EVT_BUTTON(ID_STAR2, StaticPaToolWin::OnStar2)
EVT_BUTTON(ID_STAR3, StaticPaToolWin::OnStar3)
EVT_BUTTON(ID_GOTO, StaticPaToolWin::OnGoto)
EVT_BUTTON(ID_CLEAR, StaticPaToolWin::OnClear)
EVT_BUTTON(ID_CLOSE, StaticPaToolWin::OnCloseBtn)
EVT_CLOSE(StaticPaToolWin::OnClose)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(StaticPaToolWin::PolePanel, wxPanel)
EVT_PAINT(StaticPaToolWin::PolePanel::OnPaint)
EVT_LEFT_DCLICK(StaticPaToolWin::PolePanel::OnClick)
END_EVENT_TABLE()

StaticPaToolWin::PolePanel::PolePanel(StaticPaToolWin* parent):
    wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(320, 240), wxBU_AUTODRAW | wxBU_EXACTFIT),
    paParent(parent)
{
    m_origPt.x = 160;
    m_origPt.y = 120;
    m_currPt.x = 0;
    m_currPt.y = 0;
}

void StaticPaToolWin::PolePanel::OnPaint(wxPaintEvent& evt)
{
    wxPaintDC dc(this);
    paParent->CreateStarTemplate(dc, m_currPt);
}
void StaticPaToolWin::PolePanel::Paint()
{
    wxClientDC dc(this);
    paParent->CreateStarTemplate(dc, m_currPt);
}
void StaticPaToolWin::PolePanel::OnClick(wxMouseEvent& evt)
{
    const wxPoint pt = wxGetMousePosition();
    const wxPoint mpt = GetScreenPosition();
    wxPoint mousePt = pt - mpt - m_origPt; // Distance fron centre
    m_currPt = m_currPt + mousePt; // Distance from origin
    paParent->FillPanel();
    Paint();
}

wxWindow *StaticPaTool::CreateStaticPaToolWindow()
{
    if (!pCamera || !pCamera->Connected)
    {
        wxMessageBox(_("Please connect a camera first."));
        return 0;
    }

    // confirm that image scale is specified
    if (pFrame->GetCameraPixelScale() == 1.0)
    {
        bool confirmed = ConfirmDialog::Confirm(_(
            "The Static Align tool is most effective when PHD2 knows your guide\n"
            "scope focal length and camera pixel size.\n"
            "\n"
            "Enter your guide scope focal length on the Global tab in the Brain.\n"
            "Enter your camera pixel size on the Camera tab in the Brain.\n"
            "\n"
            "Would you like to run the tool anyway?"),
            "/rotate_tool_without_pixscale");

        if (!confirmed)
        {
            return 0;
        }
    }
    if (pFrame->pGuider->IsCalibratingOrGuiding())
    {
        wxMessageBox(_("Please wait till Calibration is done and stop guiding"));
        return 0;
    }

    return new StaticPaToolWin();
}
void StaticPaTool::PaintHelper(wxAutoBufferedPaintDCBase& dc, double scale)
{
    StaticPaToolWin *win = static_cast<StaticPaToolWin *>(pFrame->pStaticPaTool);
    if (win)
    {
        win->PaintHelper(dc, scale);
    }
}

void StaticPaTool::NotifyStarLost()
{
    // See if a Static PA is underway
    StaticPaToolWin *win = static_cast<StaticPaToolWin *>(pFrame->pStaticPaTool);
    if (win && win->IsAligning())
    {
        win->RotateFail(_("Static PA rotation failed - star lost"));
    }
}
bool StaticPaTool::UpdateState()
{
    StaticPaToolWin *win = static_cast<StaticPaToolWin *>(pFrame->pStaticPaTool);
    if (win && win->IsAligning())
    {
        // Rotate the mount in RA a bit
        if (!win->RotateMount())
        {
            return false;
        }
    }
    return true;
}

StaticPaToolWin::StaticPaToolWin()
: wxFrame(pFrame, wxID_ANY, _("Static Polar Alignment"), wxDefaultPosition, wxDefaultSize,
wxCAPTION | wxCLOSE_BOX | wxMINIMIZE_BOX | wxSYSTEM_MENU | wxTAB_TRAVERSAL | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR)
{
    m_numPos = 0;
    m_devpx = 5;
    ClearState();
    m_aligning = false;

    // Fairly convoluted way to get the camera size in pixels
    wxImage *pDispImg = pFrame->pGuider->DisplayedImage();
    double scalefactor = pFrame->pGuider->ScaleFactor();
    double xpx = pDispImg->GetWidth() / scalefactor;
    double ypx = pDispImg->GetHeight() / scalefactor;
    m_pxCentre.X = xpx/2;
    m_pxCentre.Y = ypx/2;
    m_pxScale = pFrame->GetCameraPixelScale();
    // Fullsize is easier but the camera simulator does not set this.
//    wxSize camsize = pCamera->FullSize;
    m_camWidth = pCamera->FullSize.GetWidth() == 0 ? xpx: pCamera->FullSize.GetWidth();

    m_camAngle = 0.0;
    double camAngle_rad = 0.0;
    m_flip = false;
    if (pMount && pMount->IsConnected() && pMount->IsCalibrated())
    {
        camAngle_rad = pMount->xAngle();
        Debug.AddLine(wxString::Format("StaticPA: Camera angle %.1f", degrees(camAngle_rad)));
        wxString prefix = "/" + pMount->GetMountClassName() + "/calibration/";
        int ipier = pConfig->Profile.GetInt(prefix + "pierSide", PIER_SIDE_UNKNOWN);
        PierSide calPierSide = ipier == PIER_SIDE_EAST ? PIER_SIDE_EAST : ipier == PIER_SIDE_WEST ? PIER_SIDE_WEST : PIER_SIDE_UNKNOWN;
        PierSide currPierSide = pPointingSource->SideOfPier();
        Debug.AddLine(wxString::Format("StaticPA: calPierSide %s; currPierSide %s", pMount->PierSideStr(calPierSide), pMount->PierSideStr(currPierSide)));
        if (currPierSide != calPierSide  && currPierSide != PIER_SIDE_UNKNOWN)
        {
            m_flip =  true;
            Debug.AddLine(wxString::Format("StaticPA: Flipped Camera angle"));
        }
        m_camAngle = degrees(camAngle_rad);
    }
    // RA and Dec in J2000.0
    c_SthStars = {
        Star("A: sigma Oct", 317.19908, -88.9564, 4.3),
        Star("B: HD99828", 165.91797, -89.2392, 7.5),
        Star("C: HD125371", 241.45949, -89.3087, 7.8),
        Star("D: HD92239", 142.27856, -89.3471, 8.0),
        Star("E: HD90105", 130.52896, -89.4606, 7.2),
        Star("F: BQ Oct", 218.86418, -89.7718, 6.8),
        Star("G: HD99685", 149.13626, -89.7824, 7.8),
        Star("H: HD98784", 134.64254, -89.8312, 8.9),
    };
    for (int is = 0; is < c_SthStars.size(); is++) {
        PHD_Point radec_now = J2000Now(PHD_Point(c_SthStars.at(is).ra2000, c_SthStars.at(is).dec2000));
        c_SthStars.at(is).ra = radec_now.X;
        c_SthStars.at(is).dec = radec_now.Y;
    }

    c_NthStars = {
        Star("A: HD5914", 23.48114, 89.0155, 6.45),
        Star("B: HD14369", 55.20640, 89.1048, 8.05),
        Star("C: Polaris", 37.96089, 89.2643, 1.95),
        Star("D: HD211455", 309.69879, 89.4065, 8.9),
        Star("E: TYC-4629-33-1", 75.97399, 89.4207, 9.25),
        Star("F: HD21070", 146.59109, 89.5695, 9.0),
        Star("G: HD1687", 9.92515, 89.4443, 8.1),
        Star("H: TYC-4629-37-1", 70.70722, 89.6301, 9.15),
    };
    for (int is = 0; is < c_NthStars.size(); is++) {
        PHD_Point radec_now = J2000Now(PHD_Point(c_NthStars.at(is).ra2000, c_NthStars.at(is).dec2000));
        c_NthStars.at(is).ra = radec_now.X;
        c_NthStars.at(is).dec = radec_now.Y;
    }

    // get site lat/long from scope to determine hemisphere.
    m_refStar = pConfig->Profile.GetInt("/StaticPaTool/RefStar", 0);
    m_hemi = pConfig->Profile.GetInt("/StaticPaTool/Hemisphere", 1);
    if (pPointingSource)
    {
        double lat, lon;
        if (!pPointingSource->GetSiteLatLong(&lat, &lon))
        {
            m_hemi = lat >= 0 ? 1 : -1;
        }
    }
    if (!pFrame->CaptureActive)
    {
        // loop exposures
        SetStatusText(_("Start Looping..."));
        pFrame->StartLoopingInteractive(_T("StaticPA:start"));
    }
    m_instr = false;
    c_autoInstr = _(
        "Slew to near the Celestial Pole.<br/>"
        "Choose a Reference Star from the list.<br/>"
        "Use the Star Map to help identify a Reference Star.<br/>"
        "Select it as the guide star on the main display.<br/>"
        "Click Rotate to start the alignment.<br/>"
        "Wait for the adjustments to display.<br/>"
        "Adjust your mount's altitude and azimuth as displayed.<br/>"
        "Red=Altitude; Blue=Azimuth<br/>"
        );
    c_manualInstr = _(
        "Slew to near the Celestial Pole.<br/>"
        "Choose a Reference Star from the list.<br/>"
        "Use the Star Map to help identify a Reference Star.<br/>"
        "Select it as the guide star on the main display.<br/>"
        "Click Get first position.<br/>"
        "Slew at least 0h20m west in RA.<br/>"
        "Ensure the Reference Star is still selected.<br/>"
        "Click Get second position.<br/>"
        "Repeat for the third position.<br/>"
        "Wait for the adjustments to display.<br/>"
        "Adjust your mount's altitude and azimuth to place "
        "three reference stars on their orbits\n"
        );

    // can mount slew?
    m_auto = true;
    m_drawOrbit = true;
    m_canSlew = pPointingSource && pPointingSource->CanSlewAsync();
    m_ha = 0.0;
    if (!m_canSlew){
        m_auto = false;
        m_ha = 270.0;
    }

    // Start window definition
    SetSizeHints(wxDefaultSize, wxDefaultSize);

    // a vertical sizer holding everything
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    // a horizontal box sizer for the bitmap and the instructions
    wxBoxSizer *instrSizer = new wxBoxSizer(wxHORIZONTAL);
    m_instructionsText = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxSize(320, 240), wxHW_DEFAULT_STYLE);
    m_instructionsText->SetStandardFonts(8);
    m_instructionsText->Hide();
    /*
#ifdef __WXOSX__
    m_instructionsText->SetFont(*wxSMALL_FONT);
#endif
    */
    instrSizer->Add(m_instructionsText, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    m_polePanel = new PolePanel(this);
    instrSizer->Add(m_polePanel, 0, wxALIGN_CENTER_HORIZONTAL | wxALL | wxFIXED_MINSIZE, 5);

    m_instrButton = new wxButton(this, ID_INSTR, _("Instructions"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    instrSizer->Add(m_instrButton, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 0);

    topSizer->Add(instrSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    // static box sizer holding the scope pointing controls
    wxStaticBoxSizer *sbSizer = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, _("Alignment Parameters")), wxVERTICAL);

    // a grid box sizer for laying out scope pointing the controls
    wxGridBagSizer *gbSizer = new wxGridBagSizer(0, 0);
    gbSizer->SetFlexibleDirection(wxBOTH);
    gbSizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

    wxStaticText *txt;

    // First row of grid
    int gridRow = 0;
    // Hour angle
    txt = new wxStaticText(this, wxID_ANY, _("Hour Angle"));
    txt->Wrap(-1);
    gbSizer->Add(txt, wxGBPosition(gridRow, 0), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);

    txt = new wxStaticText(this, wxID_ANY, _("Hemisphere"));
    txt->Wrap(-1);
    gbSizer->Add(txt, wxGBPosition(gridRow, 1), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);

    // Alignment star specification
    txt = new wxStaticText(this, wxID_ANY, _("Reference Star"));
    txt->Wrap(-1);
    gbSizer->Add(txt, wxGBPosition(gridRow, 2), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);

    // Next row of grid
    gridRow++;
    m_hourAngleSpin = new wxSpinCtrlDouble(this, ID_HA, wxEmptyString, wxDefaultPosition, wxSize(10, -1), wxSP_ARROW_KEYS | wxSP_WRAP, 0.0, 24.0, m_ha/15.0, 0.1);
    m_hourAngleSpin->SetToolTip(_("Set your scope hour angle"));
    gbSizer->Add(m_hourAngleSpin, wxGBPosition(gridRow, 0), wxGBSpan(1, 1), wxEXPAND | wxALL | wxFIXED_MINSIZE, 5);
    m_hourAngleSpin->SetDigits(1);

    wxArrayString hemi;
    hemi.Add(_("North"));
    hemi.Add(_("South"));
    m_hemiChoice = new wxChoice(this, ID_HEMI, wxDefaultPosition, wxDefaultSize, hemi);
    m_hemiChoice->SetToolTip(_("Select your hemisphere"));
    gbSizer->Add(m_hemiChoice, wxGBPosition(gridRow, 1), wxGBSpan(1, 1), wxALL, 5);

    wxBoxSizer *refSizer = new wxBoxSizer(wxHORIZONTAL);

    m_refStarChoice = new wxChoice(this, ID_REFSTAR, wxDefaultPosition, wxDefaultSize);
    m_refStarChoice->SetToolTip(_("Select the star used for checking alignment."));
    refSizer->Add(m_refStarChoice, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 0);

    m_gotoButton = new wxButton(this, ID_GOTO, _(">"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
    refSizer->Add(m_gotoButton, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 0);

    gbSizer->Add(refSizer, wxGBPosition(gridRow, 2), wxGBSpan(1, 1), wxALL, 5);

    // Next row of grid
    gridRow++;
    txt = new wxStaticText(this, wxID_ANY, _("Camera Angle"));
    txt->Wrap(-1);
    gbSizer->Add(txt, wxGBPosition(gridRow, 0), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);

    txt = new wxStaticText(this, wxID_ANY, _("Arcsec/pixel"));
    txt->Wrap(-1);
    gbSizer->Add(txt, wxGBPosition(gridRow, 1), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);

    m_manualCheck = new wxCheckBox(this, ID_MANUAL, _("Manual Slew"));
    gbSizer->Add(m_manualCheck, wxGBPosition(gridRow, 2), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);
    m_manualCheck->SetValue(false);
    m_manualCheck->SetToolTip(_("Manually slew the mount to three alignment positions"));

    // Next row of grid
    gridRow++;

    m_camRotText = new wxTextCtrl(this, wxID_ANY, _T("--"), wxDefaultPosition, wxSize(10, -1), wxTE_READONLY);
    m_camRotText->SetMinSize(wxSize(10, -1));
    gbSizer->Add(m_camRotText, wxGBPosition(gridRow, 0), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);

    m_camScaleText = new wxTextCtrl(this, wxID_ANY, _T("--"), wxDefaultPosition, wxSize(10, -1), wxTE_READONLY);
    m_camRotText->SetMinSize(wxSize(10, -1));
    gbSizer->Add(m_camScaleText, wxGBPosition(gridRow, 1), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);

    m_star1Button = new wxButton(this, ID_ROTATE, _("Rotate"));
    gbSizer->Add(m_star1Button, wxGBPosition(gridRow, 2), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);

    // Next row of grid
    gridRow++;

    m_flipCheck = new wxCheckBox(this, ID_FLIP, _("Flip camera"));
    gbSizer->Add(m_flipCheck, wxGBPosition(gridRow, 0), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);
    m_flipCheck->SetValue(m_flip);
    m_flipCheck->SetToolTip(_("Invert the camera angle"));

    m_star2Button = new wxButton(this, ID_STAR2, _("Get second position"));
    gbSizer->Add(m_star2Button, wxGBPosition(gridRow, 2), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);

    // Next row of grid
    gridRow++;

    m_orbitCheck = new wxCheckBox(this, ID_ORBIT, _("Show Orbits"));
    gbSizer->Add(m_orbitCheck, wxGBPosition(gridRow, 0), wxGBSpan(1, 1), wxALL | wxALIGN_BOTTOM, 5);
    m_orbitCheck->SetValue(m_drawOrbit);
    m_orbitCheck->SetToolTip(_("Show or hide the star orbits"));

    m_star3Button = new wxButton(this, ID_STAR3, _("Get third position"));
    gbSizer->Add(m_star3Button, wxGBPosition(gridRow, 2), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);

    // Next row of grid
    gridRow++;

    m_clearButton = new wxButton(this, ID_CLEAR, _("Clear"), wxDefaultPosition, wxDefaultSize, 0);
    gbSizer->Add(m_clearButton, wxGBPosition(gridRow, 1), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);

    m_closeButton = new wxButton(this, ID_CLOSE, _("Close"), wxDefaultPosition, wxDefaultSize, 0);
    gbSizer->Add(m_closeButton, wxGBPosition(gridRow, 2), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);

    // add grid bag sizer to static sizer
    sbSizer->Add(gbSizer, 1, wxALIGN_CENTER, 5);

    // add static sizer to top-level sizer
    topSizer->Add(sbSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    // add some padding below the static sizer
    topSizer->Add(0, 3, 0, wxEXPAND, 3);

    m_notesLabel = new wxStaticText(this, wxID_ANY, _("Adjustment notes"));
    m_notesLabel->Wrap(-1);
    topSizer->Add(m_notesLabel, 0, wxEXPAND | wxTOP | wxLEFT, 8);

    m_notesText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 54), wxTE_MULTILINE);
    pFrame->RegisterTextCtrl(m_notesText);
    topSizer->Add(m_notesText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    m_notesText->Bind(wxEVT_COMMAND_TEXT_UPDATED, &StaticPaToolWin::OnNotes, this);
    m_notesText->SetValue(pConfig->Profile.GetString("/StaticPaTool/Notes", wxEmptyString));

    SetSizer(topSizer);

    m_statusBar = CreateStatusBar(1, wxST_SIZEGRIP, wxID_ANY);

    Layout();
    topSizer->Fit(this);

    int xpos = pConfig->Global.GetInt("/StaticPaTool/pos.x", -1);
    int ypos = pConfig->Global.GetInt("/StaticPaTool/pos.y", -1);
    MyFrame::PlaceWindowOnScreen(this, xpos, ypos);

    FillPanel();
}

StaticPaToolWin::~StaticPaToolWin()
{
    pFrame->pStaticPaTool = NULL;
}

void StaticPaToolWin::OnInstr(wxCommandEvent& evt)
{
    m_instr = !m_instr;
    FillPanel();
    return;
}

void StaticPaToolWin::OnHemi(wxCommandEvent& evt)
{
    int i_hemi = m_hemiChoice->GetSelection() <= 0 ? 1 : -1;
    pConfig->Profile.SetInt("/StaticPaTool/Hemisphere", i_hemi);
    if (i_hemi != m_hemi)
    {
        m_refStar = 0;
        m_hemi = i_hemi;
    }
    FillPanel();
}
void StaticPaToolWin::OnHa(wxSpinDoubleEvent& evt)
{
    m_ha = evt.GetValue() * 15.0;
    m_polePanel->Paint();
}

void StaticPaToolWin::OnManual(wxCommandEvent& evt)
{
    m_auto = !m_manualCheck->IsChecked();
    FillPanel();
}

void StaticPaToolWin::OnFlip(wxCommandEvent& evt)
{
    bool newFlipCheck = m_flipCheck->IsChecked();
    if (newFlipCheck != m_flip)
    {
        m_polePanel->m_currPt = wxPoint(0, 0) - m_polePanel->m_currPt;
    }
    m_flip = newFlipCheck;
    FillPanel();
}

void StaticPaToolWin::OnOrbit(wxCommandEvent& evt)
{
    m_drawOrbit = m_orbitCheck->IsChecked();
    FillPanel();
}

void StaticPaToolWin::OnRefStar(wxCommandEvent& evt)
{
    int i_refStar = m_refStarChoice->GetSelection();
    pConfig->Profile.SetInt("/StaticPaTool/RefStar", m_refStarChoice->GetSelection());

    m_refStar = i_refStar;
}

void StaticPaToolWin::OnNotes(wxCommandEvent& evt)
{
    pConfig->Profile.SetString("/StaticPaTool/Notes", m_notesText->GetValue());
}

void StaticPaToolWin::OnRotate(wxCommandEvent& evt)
{
    if (m_aligning && m_auto){ // STOP rotating
        pPointingSource->AbortSlew();
        m_aligning = false;
        m_numPos = 0;
        ClearState();
        SetStatusText(_("Static alignment stopped"));
        Debug.AddLine(wxString::Format("Static alignment stopped"));
        FillPanel();
        return;
    }
    if (pFrame->pGuider->IsCalibratingOrGuiding())
    {
        SetStatusText(_("Please wait till Calibration is done and/or stop guiding"));
        return;
    }
    if (!pFrame->pGuider->IsLocked())
    {
        SetStatusText(_("Please select a star"));
        return;
    }
    m_numPos = 1;
    if(m_auto) ClearState();
    m_aligning = true;
    FillPanel();
    return;
}

void StaticPaToolWin::OnStar2(wxCommandEvent& evt)
{
    m_numPos = 2;
    m_aligning = true;
}

void StaticPaToolWin::OnStar3(wxCommandEvent& evt)
{
    m_numPos = 3;
    m_aligning = true;
}

void StaticPaToolWin::OnGoto(wxCommandEvent& evt)
{
    // Get current star
    // Convert current star Ra Dec to pixels
    int is = m_refStar;
    double scale = 320.0 / m_camWidth;

    PHD_Point stardeg = PHD_Point(m_poleStars->at(is).ra, m_poleStars->at(is).dec);
    PHD_Point starpx = Radec2Px(stardeg);
    m_polePanel->m_currPt = wxPoint(starpx.X*scale, starpx.Y*scale);
    FillPanel();
    return;
}

void StaticPaToolWin::OnClear(wxCommandEvent& evt)
{
    if (IsCalced())
    {
        m_aligning = false;
        m_numPos = 0;
        ClearState();
        SetStatusText(_("Static Polar alignment display cleared"));
        Debug.AddLine(wxString::Format("Static PA display cleared"));
        FillPanel();
        return;
    }
}

void StaticPaToolWin::OnCloseBtn(wxCommandEvent& evt)
{
    wxCloseEvent dummy;
    OnClose(dummy);
}

void StaticPaToolWin::OnClose(wxCloseEvent& evt)
{
    if (IsAligning())
    {
        m_aligning = false;
    }
    // save the window position
    int x, y;
    GetPosition(&x, &y);
    pConfig->Global.SetInt("/StaticPaTool/pos.x", x);
    pConfig->Global.SetInt("/StaticPaTool/pos.y", y);
    Debug.AddLine("Close StaticPaTool");
    Destroy();
}

void StaticPaToolWin::FillPanel()
{
    if (m_instr)
    {
        m_instructionsText->Show();
        m_polePanel->Hide();
        m_instrButton->SetLabel(_("Star Map"));
    }
    else
    {
        m_instructionsText->Hide();
        m_polePanel->Show();
        m_instrButton->SetLabel(_("Instructions"));
    }

    m_hourAngleSpin->Enable(true);

    if (!m_canSlew){
        m_manualCheck->Hide();
    }
    m_manualCheck->SetValue(!m_auto);

    wxString html = wxString::Format("<html><body style=\"background-color:#cccccc;\">%s</body></html>", m_auto ? c_autoInstr : c_manualInstr);
    m_instructionsText->SetPage(html);

    m_star1Button->SetLabel(_("Rotate"));
    if (m_aligning) {
        m_star1Button->SetLabel(_("Stop"));
    }
    m_star2Button->Hide();
    m_star3Button->Hide();
    m_hemiChoice->Enable(false);
    m_hemiChoice->SetSelection(m_hemi > 0 ? 0 : 1);
    if (!m_auto)
    {
        m_star1Button->SetLabel(_("Get first position"));
        m_star2Button->Show();
        m_star3Button->Show();
        m_hemiChoice->Enable(true);
    }
    m_poleStars = m_hemi >= 0 ? &c_NthStars : &c_SthStars;
    m_refStarChoice->Clear();
    std::string starname;
    for (int is = 0; is < m_poleStars->size(); is++) {
        m_refStarChoice->AppendString(m_poleStars->at(is).name);
    }
    m_refStarChoice->SetSelection(m_refStar);
    m_camScaleText->SetValue(wxString::Format("%.1f", m_pxScale));
    m_camRotText->SetValue(wxString::Format("%.1f", m_camAngle));

    m_polePanel->Paint();
    Layout();
}

void StaticPaToolWin::CalcRotationCentre(void)
{
    double x1, y1, x2, y2, x3, y3;
    double cx, cy, cr;
    x1 = m_pxPos[0].X;
    y1 = m_pxPos[0].Y;
    x2 = m_pxPos[1].X;
    y2 = m_pxPos[1].Y;
    UnsetState(0);

    if (!m_auto)
    {
        double a, b, c, e, f, g, i, j, k;
        double m11, m12, m13, m14;
        x3 = m_pxPos[2].X;
        y3 = m_pxPos[2].Y;
        Debug.AddLine(wxString::Format("StaticPA: Manual CalcCoR: P1(%.1f,%.1f); P2(%.1f,%.1f); P3(%.1f,%.1f)", x1, y1, x2, y2, x3, y3));
        // |A| = aei + bfg + cdh -ceg -bdi -afh
        // a b c
        // d e f
        // g h i
        // a= x1^2+y1^2; b=x1; c=y1; d=1
        // e= x2^2+y2^2; f=x2; g=y2; h=1
        // i= x3^2+y3^2; j=x3; k=y3; l=1
        //
        // x0 = 1/2.|M12|/|M11|
        // y0 = -1/2.|M13|/|M11|
        // r = x0^2 + y0^2 + |M14| / |M11|
        //
        a = x1 * x1 + y1 * y1;
        b = x1;
        c = y1;
        e = x2 * x2 + y2 * y2;
        f = x2;
        g = y2;
        i = x3 * x3 + y3 * y3;
        j = x3;
        k = y3;
        m11 = b * g + c * j + f * k - g * j - c * f - b * k;
        m12 = a * g + c * i + e * k - g * i - c * e - a * k;
        m13 = a * f + b * i + e * j - f * i - b * e - a * j;
        m14 = a * f * k + b * g * i + c * e * j - c * f * i - b * e * k - a * g * j;
        cx = (1. / 2.) * m12 / m11;
        cy = (-1. / 2.) * m13 / m11;
        cr = sqrt(cx * cx + cy *cy + m14 / m11);
    }
    else
    {
        Debug.AddLine(wxString::Format("StaticPA Auto CalcCoR: P1(%.1f,%.1f); P2(%.1f,%.1f); RA: %.1f %.1f", x1, y1, x2, y2, m_raPos[0] * 15., m_raPos[1] * 15.));
        // Alternative algorithm based on two points and angle rotated
        double radiff, theta2;
        // Get RA change. For westward movement RA decreases.
        // Invert to get image rotation (mult by -1)
        // Convert to radians radians(mult by 15)
        // Convert to RH system (mult by m_hemi)
        // normalise to +/- PI
        radiff = norm_angle(radians((m_raPos[0] - m_raPos[1])*15.0*m_hemi));

        theta2 = radiff / 2.0; // Half the image rotation for midpoint of chord
        double lenchord = hypot((x1 - x2), (y1 - y2));
        cr = fabs(lenchord / 2.0 / sin(theta2));
        double lenbase = fabs(cr*cos(theta2));
        // Calculate the slope of the chord in pixels
        // We know the image is moving clockwise in NH and anti-clockwise in SH
        // So subtract PI/2 in NH or add PI/2 in SH to get the slope to the CoR
        // Invert y values as pixels are +ve downwards
        double slopebase = atan2(y1 - y2, x2 - x1) - m_hemi* M_PI / 2.0;
        cx = (x1 + x2) / 2.0 + lenbase * cos(slopebase);
        cy = (y1 + y2) / 2.0 - lenbase * sin(slopebase); // subtract for pixels
        Debug.AddLine(wxString::Format("StaticPA CalcCoR: radiff(deg): %.1f; cr: %.1f; slopebase(deg) %.1f", degrees(radiff), cr, degrees(slopebase)));
    }
    m_pxCentre.X = cx;
    m_pxCentre.Y = cy;
    m_radius = cr;

    wxImage *pDispImg = pFrame->pGuider->DisplayedImage();
    double scalefactor = pFrame->pGuider->ScaleFactor();
    int xpx = pDispImg->GetWidth() / scalefactor;
    int ypx = pDispImg->GetHeight() / scalefactor;
    m_dispSz[0] = xpx;
    m_dispSz[1] = ypx;

    Debug.AddLine(wxString::Format("StaticPA CalcCoR: W:H:scale:angle %d: %d: %.1f %.1f", xpx, ypx, scalefactor, m_camAngle));

    // Distance and angle of CoR from centre of sensor
    double cor_r = hypot(xpx / 2 - cx, ypx / 2 - cy);
    double cor_a = degrees(atan2((ypx / 2 - cy), (xpx / 2 - cx)));
    double rarot = -m_camAngle;
    // Cone and Dec components of CoR vector
    double dec_r = cor_r * sin(radians(cor_a - rarot));
    m_DecCorr.X = -dec_r * sin(radians(rarot));
    m_DecCorr.Y = dec_r * cos(radians(rarot));
    double cone_r = cor_r * cos(radians(cor_a - rarot));
    m_ConeCorr.X = cone_r * cos(radians(rarot));
    m_ConeCorr.Y = cone_r * sin(radians(rarot));
    SetState(0);
    FillPanel();
    return;
}

void StaticPaToolWin::CalcAdjustments(void)
{
    // Caclulate pixel values for the alignment stars relative to the CoR
    PHD_Point starpx, stardeg;
    stardeg = PHD_Point(m_poleStars->at(m_refStar).ra, m_poleStars->at(m_refStar).dec);
    starpx = Radec2Px(stardeg);
    // Convert to display pixel values by adding the CoR pixel coordinates
    double xt = starpx.X + m_pxCentre.X;
    double yt = starpx.Y + m_pxCentre.Y;

    int idx = m_auto ? 1 : 2;
    double xs = m_pxPos[idx].X;
    double ys = m_pxPos[idx].Y;

    // Calculate the camera rotation from the Azimuth axis
    // HA = LST - RA
    // In NH HA decreases clockwise; RA increases clockwise
    // "Up" is HA=0
    // Sensor "up" is 90deg counterclockwise from mount RA plus rotation
    // Star rotation is RAstar - RAmount

    // Alt angle aligns to HA=0, Azimuth (East) to HA = -90
    // In home position Az aligns with Dec
    // So at HA +/-90 (home pos) Alt rotation is 0 (HA+90)
    // At the meridian, HA=0 Alt aligns with dec so Rotation is +/-90
    // Let harot =  camera rotation from Alt axis
    // Alt axis is at HA+90
    // This is camera rotation from RA minus(?) LST angle
    double    hcor_r = hypot((xt - xs), (yt - ys)); //xt,yt: target, xs,ys: measured
    double    hcor_a = degrees(atan2((yt - ys), (xt - xs)));
    double ra_hrs, dec_deg, st_hrs, ha_deg;
    ha_deg = m_ha;
    if (pPointingSource && !pPointingSource->GetCoordinates(&ra_hrs, &dec_deg, &st_hrs))
    {
        ha_deg = norm((st_hrs - ra_hrs)*15.0 + m_ha, 0, 360);
    }
    double rarot = -m_camAngle;
    double harot = norm(rarot - (90 + ha_deg), 0, 360);
    double hrot = norm(hcor_a - harot, 0, 360);

    double az_r = hcor_r * sin(radians(hrot));
    double alt_r = hcor_r * cos(radians(hrot));

    m_AzCorr.X = -az_r * sin(radians(harot));
    m_AzCorr.Y = az_r * cos(radians(harot));
    m_AltCorr.X = alt_r * cos(radians(harot));
    m_AltCorr.Y = alt_r * sin(radians(harot));
    Debug.AddLine(wxString::Format("StaticPA CalcAdjust: Angles: rarot %.1f; ha_deg %.1f; m_ha %.1f; hcor_a %.1f; harot: %.1f", rarot, ha_deg, m_ha, hcor_a, harot));
    Debug.AddLine(wxString::Format("StaticPA CalcAdjust: Errors(px): alt %.1f; az %.1f; tot %.1f", alt_r, az_r, hcor_r));
    SetStatusText(wxString::Format(_("Polar Alignment Error (arcmin): Alt %.1f; Az %.1f Tot %.1f"),
        fabs(alt_r)*m_pxScale/60, fabs(az_r)*m_pxScale/60, fabs(hcor_r)*m_pxScale/60));
}

PHD_Point StaticPaToolWin::Radec2Px(const PHD_Point& radec)
{
    // Convert dec to pixel radius
    double r = (90.0 - fabs(radec.Y)) * 3600 / m_pxScale;

    // Rotate by calibration angle and HA f object taking into account mount rotation (HA)
    double ra_hrs, dec_deg, ra_deg, st_hrs;
    ra_deg = 0.0;
    if (pPointingSource && !pPointingSource->GetCoordinates(&ra_hrs, &dec_deg, &st_hrs))
    {
        ra_deg = norm(ra_hrs * 15.0 + m_ha, 0, 360);
    }
    else
    {
        // If not a goto mount calculate ra_deg from LST assuming mount is in the home position (HA=18h)
        tm j2000_info;
        j2000_info.tm_year = 100;
        j2000_info.tm_mon = 0;  // January is month 0
        j2000_info.tm_mday = 1;
        j2000_info.tm_hour = 12;
        j2000_info.tm_min = 0;
        j2000_info.tm_sec = 0;
        j2000_info.tm_isdst = 0;
        time_t j2000 = mktime(&j2000_info);
        time_t nowutc = time(NULL);
        tm *nowinfo = localtime(&nowutc);
        time_t now = mktime(nowinfo);
        double since = difftime(now, j2000) / 86400.0;
        double hadeg = m_ha;
        ra_deg = norm((280.46061837 + 360.98564736629 * since - hadeg),0,360);
    }

    // Target hour angle - or rather the rotation needed to correct.
    // HA = LST - RA
    // In NH HA decreases clockwise; RA increases clockwise
    // "Up" is HA=0
    // Sensor "up" is 90deg counterclockwise from mount RA plus rotation
    // Star rotation is RAstar - RAmount
    double a1 = radec.X - (ra_deg - 90.0);
    a1 = norm(a1, 0, 360);

    double l_camAngle;
    l_camAngle = norm((m_flip ? m_camAngle + 180.0 : m_camAngle), 0, 360);
    double a = l_camAngle - a1 * m_hemi;

    PHD_Point px(r * cos(radians(a)), -r * sin(radians(a)));
    return px;
}

PHD_Point StaticPaToolWin::J2000Now(const PHD_Point& radec)
{
    tm j2000_info;
    j2000_info.tm_year = 100;
    j2000_info.tm_mon = 0;  // January is month 0
    j2000_info.tm_mday = 1;
    j2000_info.tm_hour = 12;
    j2000_info.tm_min = 0;
    j2000_info.tm_sec = 0;
    j2000_info.tm_isdst = 0;
    time_t j2000 = mktime(&j2000_info);
    time_t nowutc = time(NULL);
    double JDnow = difftime(nowutc, j2000) / 86400.0;

    /*
    This code is adapted from paper
    Improvement of the IAU 2000 precession model
    N. Capitaine, P. T. Wallace, J. Chapront
    https://www.aanda.org/articles/aa/full/2005/10/aa1908/aa1908.html
    The order of polynomial to be used was found to be t^5 and the precision of the coefficients 0.1 uas. The following series with
    a 0.1 uas level of precision matches the canonical 4-rotation series to sub-microarcsecond accuracy over 4 centuries:
    zetaA = 2.5976176 + 2306.0809506 t + 0.3019015 t^2 + 0.0179663 t^3 - 0.0000327 t^4 - 0.0000002 t^5
    zedA = -2.5976176 + 2306.0803226 t + 1.0947790 t^2 + 0.0182273 t^3 + 0.0000470 t^4 - 0.0000003 t^5
    thetaA = 2004.1917476 t - 0.4269353 t^2 - 0.0418251 t^3 - 0.0000601 t^4 - 0.0000001 t^5

    In this implementation we use coefficients up to t^3
    */
    double tnow = JDnow / 36525;  // JDNow is days since J2000.0 so no need to subtract JD2000
    double t2 = pow(tnow, 2);
    double t3 = pow(tnow, 3);
    double zed, zeta, theta;            // arcseconds
    double zedrad, zetarad, thetarad;
    zeta = 2.5976176 + 2306.0809506*tnow + 0.3019015*t2 + 0.0179663*t3;
    zetarad = radians(zeta / 3600);
    zed = -2.5976176 + 2306.0803226*tnow + 1.0947790*t2 + 0.0182273*t3;
    zedrad = radians(zed / 3600);
    theta = 2004.1917476*tnow - 0.4269353*t2 - 0.0418251*t3;
    thetarad = radians(theta / 3600);

//  Build the transformation matrix
    double Xx, Xy, Xz, Yx, Yy, Yz, Zx, Zy, Zz;
    Xx = cos(zedrad)*cos(thetarad)*cos(zetarad) - sin(zedrad)*sin(zetarad);
    Yx = -cos(zedrad)*cos(thetarad)*sin(zetarad) - sin(zedrad)*cos(zetarad);
    Zx = -cos(zedrad)*sin(thetarad);
    Xy = sin(zedrad)*cos(thetarad)*cos(zetarad) + cos(zedrad)*sin(zetarad);
    Yy = -sin(zedrad)*cos(thetarad)*sin(zetarad) + cos(zedrad)*cos(zetarad);
    Zy = -sin(zedrad)*sin(thetarad);
    Xz = sin(thetarad)*cos(zetarad);
    Yz = -sin(thetarad)*sin(zetarad);
    Zz = cos(thetarad);

    // Transform coordinates;
    double x0, y0, z0;
    double x, y, z;
    x0 = cos(radians(radec.Y)) * cos(radians(radec.X));
    y0 = cos(radians(radec.Y)) * sin(radians(radec.X));
    z0 = sin(radians(radec.Y));
    x = Xx * x0 + Yx * y0 + Zx*z0;
    y = Xy * x0 + Yy * y0 + Zy*z0;
    z = Xz * x0 + Yz * y0 + Zz*z0;
    double radeg, decdeg;
    radeg = norm(degrees(atan2(y, x)), 0, 360);
    decdeg = degrees(atan2(z, sqrt(1 - z * z)));
    return PHD_Point(radeg, decdeg);
}

void StaticPaToolWin::PaintHelper(wxAutoBufferedPaintDCBase& dc, double scale)
{
    double intens = 255;
    dc.SetPen(wxPen(wxColour(0, intens, intens), 1, wxPENSTYLE_SOLID));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    for (int i = 0; i < 3; i++)
    {
        if (HasState(i+1))
        {
            dc.DrawCircle(m_pxPos[i].X*scale, m_pxPos[i].Y*scale, 12 * scale);
        }
    }
    if (!IsCalced())
    {
        return;
    }
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(wxColor(intens, 0, intens), 1, wxPENSTYLE_DOT));
    if (m_drawOrbit)
    {
        dc.DrawCircle(m_pxCentre.X*scale, m_pxCentre.Y*scale, m_radius*scale);
    }

    // draw the centre of the circle as a red cross
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(wxColor(255, 0, 0), 1, wxPENSTYLE_SOLID));
    int region = 10;
    dc.DrawLine((m_pxCentre.X - region)*scale, m_pxCentre.Y*scale, (m_pxCentre.X + region)*scale, m_pxCentre.Y*scale);
    dc.DrawLine(m_pxCentre.X*scale, (m_pxCentre.Y - region)*scale, m_pxCentre.X*scale, (m_pxCentre.Y + region)*scale);

    // Show the centre of the display wth a grey cross
    double xsc = m_dispSz[0] / 2;
    double ysc = m_dispSz[1] / 2;
    dc.SetPen(wxPen(wxColor(intens, intens, intens), 1, wxPENSTYLE_SOLID));
    dc.DrawLine((xsc - region)*scale, ysc*scale, (xsc + region)*scale, ysc*scale);
    dc.DrawLine(xsc*scale, (ysc - region)*scale, xsc*scale, (ysc + region)*scale);

    // Draw orbits for each reference star
    // Caclulate pixel values for the reference stars
    PHD_Point starpx, stardeg;
    double radpx;
    const std::string alpha = "ABCDEFGHIJKL";
    const wxFont& SmallFont =
#if defined(__WXOSX__)
        *wxSMALL_FONT;
#else
        *wxSWISS_FONT;
#endif
    dc.SetFont(SmallFont);
    for (int is = 0; is < m_poleStars->size(); is++)
    {
        stardeg = PHD_Point(m_poleStars->at(is).ra, m_poleStars->at(is).dec);
        starpx = Radec2Px(stardeg);
        radpx = hypot(starpx.X, starpx.Y);
        wxColor line_color = (is == m_refStar) ? wxColor(0, intens, 0) : wxColor(intens, intens, 0);
        dc.SetPen(wxPen(line_color, 1, wxPENSTYLE_DOT));
        if (m_drawOrbit)
        {
            dc.DrawCircle(m_pxCentre.X * scale, m_pxCentre.Y * scale, radpx * scale);
        }
        dc.SetPen(wxPen(line_color, 1, wxPENSTYLE_SOLID));
        dc.DrawCircle((m_pxCentre.X + starpx.X) * scale, (m_pxCentre.Y + starpx.Y) * scale, region*scale);
        dc.SetTextForeground(line_color);
        dc.DrawText(wxString::Format("%c", alpha[is]), (m_pxCentre.X + starpx.X + region) * scale, (m_pxCentre.Y + starpx.Y) * scale);
    }
    // Draw adjustment lines for centring the CoR on the display in blue (dec) and red (cone error)
    bool drawCone = false;
    if (drawCone){
        double xr = m_pxCentre.X * scale;
        double yr = m_pxCentre.Y * scale;
        dc.SetPen(wxPen(wxColor(intens, 0, 0), 1, wxPENSTYLE_SOLID));
        dc.DrawLine(xr, yr, xr + m_ConeCorr.X * scale, yr + m_ConeCorr.Y * scale);
        dc.SetPen(wxPen(wxColor(0, 0, intens), 1, wxPENSTYLE_SOLID));
        dc.DrawLine(xr + m_ConeCorr.X * scale, yr + m_ConeCorr.Y * scale,
            xr + m_DecCorr.X * scale + m_ConeCorr.X * scale,
            yr + m_DecCorr.Y * scale + m_ConeCorr.Y * scale);
        dc.SetPen(wxPen(wxColor(intens, intens, intens), 1, wxPENSTYLE_SOLID));
        dc.DrawLine(xr, yr, xr + m_DecCorr.X * scale + m_ConeCorr.X * scale,
            yr + m_DecCorr.Y * scale + m_ConeCorr.Y * scale);
    }
    // Draw adjustment lines for placing the guide star in its correct position relative to the CoR
    // Blue (azimuth) and Red (altitude)
    CalcAdjustments();
    bool drawCorr = true;
    if (drawCorr)
    {
        int idx;
        idx = m_auto ? 1 : 2;
        double xs = m_pxPos[idx].X * scale;
        double ys = m_pxPos[idx].Y * scale;
        dc.SetPen(wxPen(wxColor(intens, 0, 0), 1, wxPENSTYLE_DOT));
        dc.DrawLine(xs, ys, xs + m_AltCorr.X * scale, ys + m_AltCorr.Y * scale);
        dc.SetPen(wxPen(wxColor(0, 188.0*intens / 255.0, intens), 1, wxPENSTYLE_DOT));
        dc.DrawLine(xs + m_AltCorr.X * scale, ys + m_AltCorr.Y * scale,
            xs + m_AltCorr.X * scale + m_AzCorr.X * scale, ys + m_AzCorr.Y * scale + m_AltCorr.Y * scale);
        dc.SetPen(wxPen(wxColor(intens*2/3, intens*2/3, intens*2/3), 1, wxPENSTYLE_DOT));
        dc.DrawLine(xs, ys, xs + m_AltCorr.X * scale + m_AzCorr.X * scale,
            ys + m_AltCorr.Y * scale + m_AzCorr.Y * scale);
    }
}

bool StaticPaToolWin::RotateMount()
{
    // Initially, assume an offset of 5.0 degrees of the camera from the CoR
    // Calculate how far to move in RA to get a detectable arc
    // Calculate the tangential ditance of that movement
    // Mark the starting position then rotate the mount
    SetStatusText(wxString::Format(_("Reading Star Position #%d"), m_numPos));
    Debug.AddLine(wxString::Format("StaticPA: Reading Star Pos#%d", m_numPos));
    if (m_numPos == 1)
    {
        //   Initially offset is 5 degrees;
        if (!SetParams(5.0)) // m_reqRot, m_reqStep for assumed 5.0 degree PA error
        {
            Debug.AddLine(wxString::Format("StaticPA: Error from SetParams"));
            return RotateFail(wxString::Format(_("Error setting rotation parameters: Stopping")));
        }
        Debug.AddLine(wxString::Format("StaticPA: Pos#1 m_reqRot=%.1f m_reqStep=%d", m_reqRot, m_reqStep));
        if (!SetStar(m_numPos))
        {
            Debug.AddLine(wxString::Format("StaticPA: Error from SetStar %d", m_numPos));
            return RotateFail(wxString::Format(_("Error reading star position #%d: Stopping"), m_numPos));
        }
        m_numPos++;
        if (!m_auto)
        {
            m_aligning = false;
            if (IsAligned())
            {
                CalcRotationCentre();
            }
        }
        m_totRot = 0.0;
        m_nStep = 0;
        return true;
    }
    if (m_numPos == 2)
    {
        double theta = m_reqRot - m_totRot;
        // Once the mount has rotated theta degrees (as indicated by prevtheta);
        if (!m_auto)
        {
            if (!SetStar(m_numPos))
            {
                Debug.AddLine(wxString::Format("StaticPA: Error from SetStar %d", m_numPos));
                return RotateFail(wxString::Format(_("Error reading star position #%d: Stopping"), m_numPos));
            }
            m_numPos++;
            m_aligning = false;
            if (IsAligned()){
                CalcRotationCentre();
            }
            return true;
        }
        SetStatusText(wxString::Format(_("Star Pos#2 Step=%d / %d Rotated=%.1f / %.1f deg"), m_nStep, m_reqStep, m_totRot, m_reqRot));
        Debug.AddLine(wxString::Format("StaticPA: Star Pos#2 m_nStep=%d / %d m_totRot=%.1f / %.1f deg", m_nStep, m_reqStep, m_totRot, m_reqRot));
        if (pPointingSource->Slewing())  // Wait till the mount has stopped
        {
            return true;
        }
        if (m_totRot < m_reqRot)
        {
            double newtheta = theta / (m_reqStep - m_nStep);
            if (!MoveWestBy(newtheta))
            {
                Debug.AddLine(wxString::Format("StaticPA: Error from MoveWestBy at step %d", m_nStep));
                return RotateFail(wxString::Format(_("Error moving west step %d: Stopping"), m_nStep));
            }
            m_totRot += newtheta;
        }
        else
        {
            if (!SetStar(m_numPos))
            {
                Debug.AddLine(wxString::Format("StaticPA: Error from SetStar %d", m_numPos));
                return RotateFail(wxString::Format(_("Error reading star position #%d: Stopping"), m_numPos));
            }

            // Calclate how far the mount moved compared to the expected movement;
            // This assumes that the mount was rotated through theta degrees;
            // So theta is the total rotation needed for the current offset;
            // And prevtheta is how we have already moved;
            // Recalculate the offset based on the actual movement;
            // CAUTION: This might end up in an endless loop.
            double actpix = hypot((m_pxPos[1].X - m_pxPos[0].X), (m_pxPos[1].Y - m_pxPos[0].Y));
            double actsec = actpix * m_pxScale;
            double actoffsetdeg = 90 - degrees(acos(actsec / 3600 / m_reqRot));
            Debug.AddLine(wxString::Format("StaticPA: Star Pos#2 actpix=%.1f actsec=%.1f m_pxScale=%.1f", actpix, actsec, m_pxScale));

            if (actoffsetdeg == 0)
            {
                Debug.AddLine(wxString::Format("StaticPA: Star Pos#2 Mount did not move actoffsetdeg=%.1f", actoffsetdeg));
                return RotateFail(wxString::Format(_("Star Pos#2 Mount did not move. Calculated polar offset=%.1f deg"), actoffsetdeg));
            }
            double prev_rotdg = m_reqRot;
            if (!SetParams(actoffsetdeg))
            {
                Debug.AddLine(wxString::Format("StaticPA: Error from SetParams"));
                return RotateFail(wxString::Format(_("Error setting rotation parameters: Stopping")));
            }
            if (m_reqRot <= prev_rotdg) // Moved far enough: show the adjustment chart
            {
                m_numPos++;
                m_nStep = 0;
                m_totRot = 0.0;
                m_aligning = false;
                CalcRotationCentre();
            }
            else if (m_reqRot > 45)
            {
                Debug.AddLine(wxString::Format("StaticPA: Pos#2 Too close to CoR actoffsetdeg=%.1f m_reqRot=%.1f", actoffsetdeg, m_reqRot));
                return RotateFail(wxString::Format(_("Star is too close to CoR (%.1f deg) - try another reference star"), actoffsetdeg ));
            }
            else
            {
                m_nStep = int(m_reqStep * m_totRot/m_reqRot);
                Debug.AddLine(wxString::Format("StaticPA: Star Pos#2 m_nStep=%d / %d m_totRot=%.1f / %.1f", m_nStep, m_reqStep, m_totRot, m_reqRot));
            }
        }
        return true;
    }
    if (m_numPos == 3)
    {
        if (!m_auto)
        {
            if (!SetStar(m_numPos))
            {
                Debug.AddLine(wxString::Format("StaticPA: Error from SetStar %d", m_numPos));
                return RotateFail(wxString::Format(_("Error reading star position #%d: Stopping"), m_numPos));
            }
            m_numPos++;
            m_aligning = false;
            if (IsAligned()){
                CalcRotationCentre();
            }
            return true;
        }
        m_numPos++;
        return true;
    }
    return true;
}

bool StaticPaToolWin::RotateFail(const wxString& msg)
{
    SetStatusText(msg);
    m_aligning = false;
    if (m_auto) // STOP rotating
    {
        pPointingSource->AbortSlew();
        m_numPos = 0;
        ClearState();
        FillPanel();
    }
    return false;
}

bool StaticPaToolWin::SetStar(int npos)
{
    int idx = npos - 1;
    // Get X and Y coords from PHD2;
    // idx: 0=B, 1/2/3 = A[idx];
    double cur_dec, cur_st;
    UnsetState(npos);
    if (m_auto && pPointingSource->GetCoordinates(&m_raPos[idx], &cur_dec, &cur_st))
    {
        Debug.AddLine("StaticPA: SetStar failed to get scope coordinates");
        return false;
    }
    m_pxPos[idx].X = -1;
    m_pxPos[idx].Y = -1;
    PHD_Point star = pFrame->pGuider->CurrentPosition();
    if (!star.IsValid())
    {
        return false;
    }
    m_pxPos[idx] = star;
    SetState(npos);
    Debug.AddLine(wxString::Format("StaticPA: Setstar #%d %.0f, %.0f", npos, m_pxPos[idx].X, m_pxPos[idx].Y));
    SetStatusText(wxString::Format(_("Read Position #%d: %.0f, %.0f"), npos, m_pxPos[idx].X, m_pxPos[idx].Y));
    FillPanel();
    return true;
}

bool StaticPaToolWin::SetParams(double newoffset)
{
    double offsetdeg = newoffset;
    double m_offsetpx = offsetdeg * 3600 / m_pxScale;
    Debug.AddLine(wxString::Format("StaticPA:SetParams(newoffset=%.1f) m_pxScale=%.1f m_offsetpx=%.1f m_devpx=%.1f", newoffset, m_pxScale, m_offsetpx, m_devpx));
    if (m_offsetpx < m_devpx)
    {
        Debug.AddLine(wxString::Format("StaticPA: SetParams() Too close to CoR: m_offsetpx=%.1f m_devpx=%.1f", m_offsetpx, m_devpx));
        return false;
    }
    m_reqRot = degrees(acos(1 - m_devpx / m_offsetpx));
    double m_rotpx = m_reqRot * 3600.0 / m_pxScale * sin(radians(offsetdeg));

    int region = pFrame->pGuider->GetSearchRegion();
    m_reqStep = 1;
    if (m_rotpx > region)
    {
        m_reqStep = int(ceil(m_rotpx / region));
    }
    Debug.AddLine(wxString::Format("StaticPA: SetParams() m_reqRot=%.1f m_rotpx=%.1f m_reqStep=%d region=%d", m_reqRot, m_rotpx, m_reqStep, region));
    return true;
}
bool StaticPaToolWin::MoveWestBy(double thetadeg)
{
    double cur_ra, cur_dec, cur_st;
    if (pPointingSource->GetCoordinates(&cur_ra, &cur_dec, &cur_st))
    {
        Debug.AddLine("StaticPA: MoveWestBy failed to get scope coordinates");
        return false;
    }
    double slew_ra = norm_ra(cur_ra - thetadeg * 24.0 / 360.0);
//    slew_ra = slew_ra - 24.0 * floor(slew_ra / 24.0);
    Debug.AddLine(wxString::Format("StaticPA: Slewing from RA hrs: %.3f to:%.3f", cur_ra, slew_ra));
    if (pPointingSource->SlewToCoordinatesAsync(slew_ra, cur_dec))
    {
        Debug.AddLine("StaticPA: MoveWestBy: async slew failed");
        return false;
    }

    m_nStep++;
    PHD_Point lockpos = pFrame->pGuider->CurrentPosition();
    if (pFrame->pGuider->SetLockPosToStarAtPosition(lockpos))
    {
        Debug.AddLine("StaticPA: MoveWestBy: Failed to lock star position");
        return false;
    }
    return true;
}

void StaticPaToolWin::CreateStarTemplate(wxDC& dc, const wxPoint& m_currPt)
{
    dc.SetBackground(*wxGREY_BRUSH);
    dc.Clear();

    double scale = 320.0 / m_camWidth;
    int region = 5;

    dc.SetTextForeground(*wxYELLOW);
    const wxFont& SmallFont =
#if defined(__WXOSX__)
        *wxSMALL_FONT;
#else
        *wxSWISS_FONT;
#endif
    dc.SetFont(SmallFont);

    const std::string alpha = "ABCDEFGHIJKL";
    PHD_Point starpx, stardeg;
    double starsz, starmag;
    // Draw position of each alignment star
    for (int is = 0; is < m_poleStars->size(); is++)
        {
        stardeg = PHD_Point(m_poleStars->at(is).ra, m_poleStars->at(is).dec);
        starmag = m_poleStars->at(is).mag;

        starsz = 356.0*exp(-0.3*starmag) / m_pxScale;
        starpx = Radec2Px(stardeg);
        dc.SetPen(*wxYELLOW_PEN);
        dc.SetBrush(*wxYELLOW_BRUSH);
        wxPoint starPt = wxPoint(starpx.X*scale, starpx.Y*scale) - m_currPt + wxPoint(160, 120);
        dc.DrawCircle(starPt.x, starPt.y, starsz*scale);
        dc.DrawText(wxString::Format("%c", alpha[is]), starPt.x + starsz*scale, starPt.y);
    }
    // draw the pole as a red cross
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(wxColor(255, 0, 0), 1, wxPENSTYLE_SOLID));
    dc.DrawLine(160- region*scale, 120, 160 + region*scale, 120);
    dc.DrawLine(160, 120 - region*scale, 160, 120 + region*scale);
    dc.DrawLine(160, 120, 160-m_currPt.x, 120-m_currPt.y);
    return;
}
