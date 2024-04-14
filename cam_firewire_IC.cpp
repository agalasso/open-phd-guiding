/*
 *  cam_firewire_IC.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2007-2010 Craig Stark.
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

#if defined (__WINDOWS__) && defined (FIREWIRE_CAMERA)
#include "camera.h"
#include "time.h"
#include "image_math.h"
#include <wx/textdlg.h>
#include <wx/stdpaths.h>
#include <wx/textfile.h>

// Deal with 8-bit vs. 16-bit??
// Take care of gain capability

// Start stream in InitCapture()?

#include "cam_firewire.h"
using namespace _DSHOWLIB_NAMESPACE;

CameraFirewire::CameraFirewire()
{
    Connected = false;
    Name = _T("The Imaging Source");
    FullSize = wxSize(1280,1024);
    HasGainControl = true;
    m_hasGuideOutput = false;
}

wxByte CameraFirewire::BitsPerPixel()
{
    return 8;
}

//CameraFirewire::~CameraFirewire () {
//  ;
//}
bool CameraFirewire::Connect(const wxString& camId)
{
    int CamNum, ModeNum;
    bool retval;
    wxArrayString Names;
    std::string str;
    bool debug = false;
    wxTextFile *debugfile;
    int debugstep = 0;

    if (debug) {
        debugfile = new wxTextFile(Debug.GetLogDir() + PATHSEPSTR + wxString::Format("PHD_debug_%ld.txt",wxGetLocalTime()));
        if (debugfile->Exists()) debugfile->Open();
        else debugfile->Create();
        wxDateTime now = wxDateTime::Now();
        debugfile->AddLine(wxString::Format("DEBUG %s %s  -- ", APPNAME, FULLVER) + now.FormatDate() + now.FormatTime());
    }

    try {
        if (debug) { debugfile->AddLine(wxString::Format("1: Init library")); debugfile->Write(); }
        // Init the TIS library
        if( ! DShowLib::InitLibrary( "ISB3200016679" ) ) {  // license key check
            return CamConnectFailed(_("Cannot initialize ImageCapture library"));
        }

        if (debug) { debugfile->AddLine(wxString::Format("2: Create grabber")); debugfile->Write(); }
        m_pGrabber = new DShowLib::Grabber();

        if (debug) { debugfile->AddLine(wxString::Format("3: Find cameras")); debugfile->Write(); }
        Grabber::tVidCapDevListPtr pVidCapDevList = m_pGrabber->getAvailableVideoCaptureDevices();
        if( pVidCapDevList == 0 || pVidCapDevList->empty() )
            return CamConnectFailed(_("No camera found"));

        int NCams = (int) pVidCapDevList->size();
        if (debug) { debugfile->AddLine(wxString::Format("4: Found %d cams",NCams)); debugfile->Write(); }

        // deal with > 1 cam
        CamNum = 0;
        if (NCams > 1) {
            for( Grabber::tVidCapDevList::iterator it = pVidCapDevList->begin();
                it != pVidCapDevList->end(); ++it ) {
                Names.Add(it->toString());
            }
            CamNum = wxGetSingleChoiceIndex(_("Select Camera"),_("Camera"),Names);
            if (CamNum == -1)
                return true;
        }

        // Open camera
        if (debug) { debugfile->AddLine(wxString::Format("5: Open Camera")); debugfile->Write(); }
        retval = m_pGrabber->openDev( pVidCapDevList->at( CamNum ) );
        if (!retval)
            return CamConnectFailed(_("Cannot open camera"));

        if (debug) { debugfile->AddLine(wxString(pVidCapDevList->at(CamNum).toString())); debugfile->Write(); }

        if (debug) { debugfile->AddLine(wxString::Format("6: Get Video formats")); debugfile->Write(); }
        // Get video formats
        Grabber::tVidFmtListPtr pVidFmtList = m_pGrabber->getAvailableVideoFormats();
        if ((pVidFmtList == 0) || pVidFmtList->empty()) {
            m_pGrabber->closeDev();
            return CamConnectFailed(_("Cannot get list of video modes"));
        }
        int NModes = pVidFmtList->size();
        if (debug) { debugfile->AddLine(wxString::Format("7: Found %d formats",NModes)); debugfile->Write(); }

    //  Names.Clear();
    //  ModeNum = 0;
        wxString Name;
    //  if (NModes > 1) {
            ModeNum = -1;
            int i = 0;
            for( Grabber::tVidFmtList::iterator it = pVidFmtList->begin();
                it != pVidFmtList->end(); ++it, i++ ) {
    //          Names.Add(it->toString());
                Name = wxString(it->toString());
                if (debug) {debugfile->AddLine(wxString(it->toString())); debugfile->Write(); }
                if (Name.Find("Y800") != wxNOT_FOUND) {
                    ModeNum = i;
                    break;
                }
            }
            //ModeNum = wxGetSingleChoiceIndex(_T("Select Mode"),_T("Mode"),Names);
            if (ModeNum == -1) {
                return CamConnectFailed(_("Cannot find a Y800 mode"));
            }
    //  }

        if (debug) { debugfile->AddLine(wxString::Format("8: Set format %d",ModeNum)); debugfile->Write(); }
        // Set the video format
        m_pGrabber->setVideoFormat(pVidFmtList->at(ModeNum));

        // Set some more format things
        if (debug) { debugfile->AddLine(wxString::Format("9: Set FPS")); debugfile->Write(); }
    //  retval = m_pGrabber->setFPS(7.5);  // No need to run higher than this
    //  if (!retval) wxMessageBox (_T("Could not set to 7.5 FPS"));
        if (debug) { debugfile->AddLine(wxString::Format("10: Turn off auto-exposure")); debugfile->Write(); }
        retval = m_pGrabber->setProperty(CameraControl_Exposure,false);
        if (!retval) {
            return CamConnectFailed(_("Could not turn off auto-exposure"));
        }

        // Setup the frame handler
        if (debug) { debugfile->AddLine(wxString::Format("11: Setup frame handler")); debugfile->Write(); }
        pSink = FrameHandlerSink::create(eY800, 4 );  // not sure why I even need 4...
        if (!pSink) {
            return CamConnectFailed(_("Cannot setup frame handler"));
        }

        if (debug) { debugstep = 1; debugfile->AddLine(wxString::Format("12: Set snap mode")); debugfile->Write(); }
        pSink->setSnapMode( true );

        if (debug) { debugstep = 2; debugfile->AddLine(wxString::Format("12a: Setting SinkType")); debugfile->Write(); }
        retval = m_pGrabber->setSinkType(pSink);
        if (!retval) {
            Debug.Write("cam_firewire_IC: Could not set sink type\n");
            return CamConnectFailed(_("FireWire camera setup failed"));
        }

        // Get info I need
        if (debug) { debugstep = 3; debugfile->AddLine(wxString::Format("12b: Getting name for mode %d",ModeNum)); debugfile->Write(); }
        Name = wxString(pVidCapDevList->at(CamNum).toString());
        if (debug) { debugstep = 4; debugfile->AddLine(_T(" Name: " + Name)); debugfile->Write(); }
        if (debug) { debugstep = 5; debugfile->AddLine(wxString::Format("12c: Getting size for mode %d",ModeNum)); debugfile->Write(); }
        SIZE sz = pVidFmtList->at(ModeNum).getSize();
        if (debug) {debugstep = 6; debugfile->AddLine(_T("Size found - setting FullSize")); debugfile->Write(); }
        FullSize = wxSize((int)sz.cx, (int)sz.cy);

        if (debug) { debugstep = 7; debugfile->AddLine(wxString::Format("Image: %d %d Camera: ",FullSize.GetWidth(),FullSize.GetHeight()) + Name); debugfile->Write(); }

        // Get the stream prepared, but don't start yet - going to start/stop on each frame grab
        if (debug) { debugstep = 8; debugfile->AddLine(wxString::Format("13: Prepare Live")); debugfile->Write(); }
        retval = m_pGrabber->prepareLive(false); // not using their renderer
        if (!retval) {
            return CamConnectFailed(_("Could not start Live view"));
        }

        // Get pointer to the exposure duration functin needed
        if (debug) { debugfile->AddLine(wxString::Format("14: Get VCD properties")); debugfile->Write(); }
        tIVCDPropertyItemsPtr pItems = m_pGrabber->getAvailableVCDProperties();
        if( pItems != 0 ) {
            tIVCDPropertyItemPtr pExposureItem = pItems->findItem( VCDID_Exposure );
            tIVCDPropertyElementPtr pExposureValueElement = pExposureItem->findElement( VCDElement_Value );
            if (pExposureValueElement != 0) {
                pExposureValueElement->getInterfacePtr(m_pExposureAbs);
                if (m_pExposureAbs == 0) {
                    pFrame->Alert(_("Warning - cannot directly control exposure duration - running in auto-exposure"));
                    m_pGrabber->setProperty(CameraControl_Exposure,true);
                }
                else
                    m_pExposureAbs->setValue(0.2);
            }

            tIVCDPropertyItemPtr pGainItem = pItems->findItem( VCDID_Gain );
            tIVCDPropertyElementPtr pGainValueElement = pGainItem->findElement( VCDElement_Value );
            if (pGainValueElement != 0) {
                pGainValueElement->getInterfacePtr(m_pGain);
                if (m_pGain == 0) {
                    pFrame->Alert(_T("Warning - cannot directly control gain - running in auto-gain"));
    //              m_pGrabber->setProperty(CameraControl_Exposure,true);
                }
                else {
                    GainMax = m_pGain->getRangeMax();
                    long lval = (long) GuideCameraGain * GainMax / 100;
                    if (lval > m_pGain->getRangeMax())
                        lval = m_pGain->getRangeMax();
                    else if (lval < m_pGain->getRangeMin())
                        lval = m_pGain->getRangeMin();

                    m_pGain->setValue(lval);
                }
            }
        }
    } // try
    catch (...) {
        if (debug) {  debugfile->AddLine(wxString::Format("Failed at %d",debugstep)); debugfile->Write(); debugfile->Close(); }
        return CamConnectFailed(wxString::Format(_("Fatal error at step %d connecting to TIS camera"),debugstep));
    }
    if (debug) {debugfile->Write(); debugfile->Close(); delete debugfile; }
    Connected = true;
    return false;
}

bool CameraFirewire::Disconnect()
{
    m_pGrabber->stopLive();
    m_pGrabber->closeDev();
    Connected = false;
    delete m_pGrabber;
    return false;
}

void CameraFirewire::InitCapture()
{
    // Set gain
    if (m_pGain != 0) {
        long lval = (long) GuideCameraGain * GainMax / 100;
        if (lval > m_pGain->getRangeMax())
            lval = m_pGain->getRangeMax();
        else if (lval < m_pGain->getRangeMin())
            lval = m_pGain->getRangeMin();

        m_pGain->setValue(lval);
    }
}

bool CameraFirewire::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    static int programmed_dur = 200;

    int xsize = FullSize.GetWidth();
    int ysize = FullSize.GetHeight();

    if (img.Init(FullSize))
    {
        pFrame->Alert(_("Memory allocation error"));
        return true;
    }
    unsigned short *dataptr = img.ImageData;

    if ((duration != programmed_dur) && (m_pExposureAbs != 0)) {
        m_pExposureAbs->setValue(duration / 1000.0);
        programmed_dur = duration;
    }

    bool retval = m_pGrabber->startLive(false);
    if (!retval) {
        pFrame->Alert(_("Could not start video stream"));
        return true;
    }

    // Flush

    // grab the next frame

    Error err = pSink->snapImages( 1,15000 );
    if (err.getVal() ==  eTIMEOUT_PREMATURLY_ELAPSED) {
        wxMilliSleep(200);
        err = pSink->snapImages( 1,15000 );
    }
    if (err.getVal() ==  eTIMEOUT_PREMATURLY_ELAPSED) {
        wxMilliSleep(200);
        err = pSink->snapImages( 1,15000 );
    }

    if (err.isError())
    {
        DisconnectWithAlert(wxString::Format(_("Error capturing image: %d (%d) %s"),
            (int) err.getVal(), (int) eTIMEOUT_PREMATURLY_ELAPSED, wxString(err.c_str())), NO_RECONNECT);
        return true;
    }
    unsigned char *imgptr = (unsigned char *) pSink->getLastAcqMemBuffer()->getPtr();

    for (unsigned int i = 0; i < img.NPixels; i++, dataptr++, imgptr++)
        *dataptr = (unsigned short) *imgptr;

/*  if (dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_WAIT, &vframe)!=DC1394_SUCCESS) {
        DisconnectWithAlert(_("Cannot get a frame from the queue"));
        return true;
    }
    imgptr = vpFrame->image;
    for (unsigned int i = 0; i < img.NPixels; i++, dataptr++, imgptr++)
        *dataptr = (unsigned short) *imgptr;
    dc1394_capture_enqueue(camera, vframe);  // release this frame
    */

    m_pGrabber->suspendLive();

    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);

    return false;
}

bool CameraFirewire::HasNonGuiCapture(void)
{
    return true;
}

#endif
