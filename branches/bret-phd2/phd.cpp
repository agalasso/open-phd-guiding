/*
 *  phd.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
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
#include "graph.h"


//#define WINICONS

//#define DEVBUILD

// Globals`

PhdConfig *pConfig=NULL;
Mount *pMount = NULL;
Mount *pSecondaryMount = NULL;
MyFrame *pFrame = NULL;
GuideCamera *pCamera = NULL;

DebugLog Debug;
GuidingLog GuideLog;

wxTextFile *LogFile;
bool Log_Data = false;
int Log_Images = 0;

int AdvDlg_fontsize = 0;
int XWinSize = 640;
int YWinSize = 512;

bool RandomMotionMode = false;

IMPLEMENT_APP(PhdApp)

// ------------------------  Phd App stuff -----------------------------
bool PhdApp::OnInit() {
    GuideLog.EnableLogging();
#ifndef DEBUG
    #if (wxMAJOR_VERSION > 2 || wxMINOR_VERSION > 8)
    wxDisableAsserts();
    #endif
#endif

#if defined(_DEBUG)
    Debug.Init("debug", true);
#else
    Debug.Init("debug", false);
#endif
    SetVendorName(_T("StarkLabs"));
    pConfig = new PhdConfig(_T("PHDGuidingV2"));

    pMount = new ScopeNone();

    wxLocale locale;

    locale.Init(wxLANGUAGE_ENGLISH_US);
//  wxMessageBox(wxString::Format("%f",1.23));
    pFrame = new MyFrame(wxString::Format(_T("PHD Guiding %s  -  www.stark-labs.com"),VERSION));
    wxImage::AddHandler(new wxJPEGHandler);

    pFrame->Show(true);

    return true;
}

int PhdApp::OnExit(void)
{
    delete pMount;
    pMount = NULL;
    delete pCamera;
    pCamera = NULL;
    delete pConfig;
    pConfig = NULL;

    return wxApp::OnExit();
}

bool PhdApp::Yield(bool onlyIfNeeded)
{
    bool bReturn = !onlyIfNeeded;

    if (wxThread::IsMain())
    {
        bReturn = wxApp::Yield(onlyIfNeeded);
    }

    return bReturn;
}
