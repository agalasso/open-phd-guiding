/*
 *  staticpa_toolwin.h
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
#ifndef STATICPA_TOOLWIN_H
#define STATICPA_TOOLWIN_H

#include "phd.h"

#include <wx/gbsizer.h>
#include <wx/valnum.h>
#include <wx/textwrapper.h>

//==================================
struct StaticPaToolWin : public wxFrame
{
    StaticPaToolWin();
    ~StaticPaToolWin();
    /*
    Tool window controls
    */
    wxHtmlWindow *m_instructionsText;
    wxTextCtrl *m_camScaleText; // Text box for camera pixel scale
    wxTextCtrl *m_camRotText;   // Text box for camera rotation
    wxSpinCtrlDouble *m_hourAngleSpin; // Spinner to manually set HA
    wxCheckBox *m_manualCheck;   // Checkbox for auto/manual slewing
    wxCheckBox *m_flipCheck;     // Checkbox to flip camera
    wxCheckBox *m_orbitCheck;    // Checkbox t show/hide orbits
    wxButton *m_instrButton;      // Button to toggle instructions/map
    wxButton *m_star1Button;      // Button for manual get of point 1
    wxButton *m_star2Button;      // Button for manual get of point 2
    wxButton *m_star3Button;      // Button for manual get of point 3
    wxStaticText *m_notesLabel;
    wxTextCtrl *m_notesText;
    wxButton *m_gotoButton;     // Button to clear display
    wxButton *m_clearButton;     // Button to clear display
    wxButton *m_closeButton;     // Close button
    wxStatusBar *m_statusBar;
    wxChoice *m_refStarChoice;  // Listbox for reference stars
    wxChoice *m_hemiChoice;     // Listbox for manual hemisphere choice

    class PolePanel : public wxPanel
    {
    public:
        wxPoint m_origPt, m_currPt;
        PolePanel(StaticPaToolWin* parent);
        StaticPaToolWin *paParent;
        void OnClick(wxMouseEvent &evt);
        void OnPaint(wxPaintEvent &evt);
        void Paint();
        DECLARE_EVENT_TABLE()
    };
    PolePanel *m_polePanel; // Panel for drawing of pole stars

    /*
    Constants used in the tool window controls
    */
    wxString c_autoInstr;
    wxString c_manualInstr;

    class Star
    {
    public:
        std::string name;
        double ra2000, dec2000, mag, ra, dec;
        Star(const char* a, double b, double c, double d) :name(a), ra2000(b), dec2000(c), mag(d), ra(-1), dec(-1) {};
    };
    std::vector<Star> c_SthStars, c_NthStars; // Stars around the poles
    std::vector<Star> *m_poleStars;

    enum StaticPaCtrlIds
    {
        ID_HEMI = 10001,
        ID_HA,
        ID_INSTR,
        ID_MANUAL,
        ID_FLIP,
        ID_ORBIT,
        ID_REFSTAR,
        ID_ROTATE,
        ID_STAR2,
        ID_STAR3,
        ID_GOTO,
        ID_CLEAR,
        ID_CLOSE,
    };
    bool m_canSlew;     // Mount can slew
    double m_pxScale;   // Camera pixel scale
    double m_camAngle;  // Camera angle to RA
    double m_camWidth;  // Camera width

    bool m_instr;       // Instructions displayed va Map displayed
    double m_devpx;     // Number of pixels deviation needed to detect an arc
    int m_refStar;      // Selected reference star
    bool m_auto;        // Auto slewing - must be manual if mount cannot slew
    int m_hemi;         // Hemisphere of the observer
    double m_ha;        // Manual hour angle
    bool m_drawOrbit;   // Draw the star orbits
    bool m_flip;        // Flip the camera angle

    bool m_aligning;        // Indicates that alignment points are being collected
    unsigned int m_state;   // state of the alignment process
    int m_numPos;           // Number of alignment points
    double m_reqRot;        // Amount of rotation required
    int m_reqStep;          // Number of steps needed
    double m_totRot;        // Rotation so far
    int m_nStep;            // Number of steps taken so far

    double m_raPos[3];      // RA readings at each point
    PHD_Point m_pxPos[3];   // Alignment points - in pixels
    PHD_Point m_pxCentre;   // Centre of Rotation in pixels
    double m_radius;        // Radius of centre of rotation to reference star

    double m_dispSz[2];     // Display size (dynamic)
    PHD_Point m_AzCorr, m_AltCorr; // Calculated Alt and Az corrections
    PHD_Point m_ConeCorr, m_DecCorr;  //Calculated Dec and Cone offsets

    void FillPanel();

    void OnInstr(wxCommandEvent& evt);
    void OnHemi(wxCommandEvent& evt);
    void OnHa(wxSpinDoubleEvent& evt);
    void OnRefStar(wxCommandEvent& evt);
    void OnManual(wxCommandEvent& evt);
    void OnFlip(wxCommandEvent& evt);
    void OnOrbit(wxCommandEvent& evt);
    void OnNotes(wxCommandEvent& evt);
    void OnRotate(wxCommandEvent& evt);
    void OnStar2(wxCommandEvent& evt);
    void OnStar3(wxCommandEvent& evt);
    void OnGoto(wxCommandEvent& evt);
    void OnClear(wxCommandEvent& evt);
    void OnCloseBtn(wxCommandEvent& evt);
    void OnClose(wxCloseEvent& evt);

    void CreateStarTemplate(wxDC &dc, const wxPoint& m_currPt);
    bool IsAligning(){ return m_aligning; };
    bool RotateMount();
    bool RotateFail(const wxString& msg);
    bool SetParams(double newoffset);
    bool MoveWestBy(double thetadeg);
    bool SetStar(int idx);
    bool IsAligned(){ return m_auto ? ((m_state>>1) & 3) ==3 : ((m_state>>1) & 7)==7; }
    bool IsCalced(){ return HasState(0); }
    void CalcRotationCentre(void);
    void CalcAdjustments(void);
    bool HasState(int ipos) { return (m_state & (1 << ipos)) > 0;  }
    void SetState(int ipos) { m_state = m_state | (1 << ipos); }
    void UnsetState(int ipos) { m_state = m_state & ~(1 << ipos) & 15; }
    void ClearState() { m_state = 0; }
    void PaintHelper(wxAutoBufferedPaintDCBase& dc, double scale);
    PHD_Point Radec2Px(const PHD_Point& radec);
    PHD_Point J2000Now(const PHD_Point& radec);

    DECLARE_EVENT_TABLE()
};

#endif

