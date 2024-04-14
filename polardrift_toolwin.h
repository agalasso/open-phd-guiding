/*
 *  polardrift_toolwin.h
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
#ifndef POLARDRIFT_TOOLWIN_H
#define POLARDRIFT_TOOLWIN_H

#include "phd.h"

#include <wx/gbsizer.h>
#include <wx/valnum.h>
#include <wx/textwrapper.h>

//==================================
struct PolarDriftToolWin : public wxFrame
{
    PolarDriftToolWin();
    ~PolarDriftToolWin();
    /*
    Tool window controls
    */
    wxStaticText *m_instructionsText;
    wxButton *m_startButton;      // Button to start or stop drift
    wxStaticText *m_notesLabel;
    wxTextCtrl *w_notesText;
    wxButton *m_closeButton;      // Close button
    wxStatusBar *m_statusBar;
    wxChoice *m_hemiChoice;     // Listbox for manual hemisphere choice
    wxCheckBox *m_mirrorCheck;     // Checkbox for mirrored camera
    bool m_savePrimaryMountEnabled;
    bool m_saveSecondaryMountEnabled;
    bool m_guideOutputDisabled;

    /*
    Constants used in the tool window controls
    */
    wxString c_instr;

    enum PolarDriftCtrlIds
    {
        ID_HEMI = 10001,
        ID_MIRROR,
        ID_START,
        ID_CLOSE,
    };

    double m_pxScale;   // Camera pixel scale

    int m_hemi;         // Hemisphere of the observer
    int m_mirror;       // -1 if the image is mirrored e.g. due to OAG

    bool m_drifting;        // Indicates that alignment points are being collected
    double m_t0;
    double m_sumt, m_sumt2, m_sumx, m_sumx2, m_sumy, m_sumy2, m_sumtx, m_sumty, m_sumxy;
    long m_num;
    double m_offset, m_alpha;
    PHD_Point m_current, m_target;

    void FillPanel();

    void OnHemi(wxCommandEvent& evt);
    void OnMirror(wxCommandEvent& evt);
    void OnStart(wxCommandEvent& evt);
    void OnCloseBtn(wxCommandEvent& evt);
    void OnClose(wxCloseEvent& evt);

    bool IsDrifting(){ return m_drifting; };
    bool WatchDrift();
    void PaintHelper(wxAutoBufferedPaintDCBase& dc, double scale);

    DECLARE_EVENT_TABLE()
};

#endif

