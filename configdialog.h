/*
 *  configdialog.h
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

#ifndef CONFIG_DIALOG_H_INCLUDED
#define CONFIG_DIALOG_H_INCLUDED

// Design Notes:  The goal here is to separate the ownership of various controls from where they are displayed in the AD.  A class that "owns" a control
// will create it and handle all its behavior - loading, unloading, all the semantics.  This part is handled by the ConfigDlgControlSet.  Where those controls
// are displayed is determined by the BrainIDControlMap, a dictionary that maps the control ids to the AD panel where they will be displayed.  The owner of the
// AD panel - the "host" - is responsible for creating the panel UI and rendering all the controls that belong on that panel.  This will be handled in the
// LayoutControls() method of the hosting class.  Beyond that, the host class has no involvement with controls that are owned by a different class.
// Example: the focal length control (AD_szFocalLength) is owned by MyFrame but is displayed on the guiding tab

// Segmented by the tab page location seen in the UI
// "sz" => element is a sizer
enum BRAIN_CTRL_IDS
{
    AD_UNASSIGNED,

    AD_cbResetConfig,
    AD_cbDontAsk,
    AD_szLanguage,
    AD_szSoftwareUpdate,
    AD_szLogFileInfo,
    AD_cbEnableImageLogging,
    AD_szImageLoggingOptions,
    AD_szDither,
    AD_GLOBAL_TAB_BOUNDARY,        //-----end of global tab controls

    AD_cbUseSubFrames,
    AD_szNoiseReduction,
    AD_szAutoExposure,
    AD_szVariableExposureDelay,
    AD_szSaturationOptions,
    AD_szCameraTimeout,
    AD_szTimeLapse,
    AD_szPixelSize,
    AD_szGain,
    AD_szDelay,
    AD_szPort,
    AD_szBinning,
    AD_szCooler,
    AD_CAMERA_TAB_BOUNDARY,        // ------ end of camera tab controls

    AD_cbScaleImages,
    AD_szFocalLength,
    AD_cbAutoRestoreCal,
    AD_cbFastRecenter,
    AD_szStarTracking,
    AD_cbClearCalibration,
    AD_cbEnableGuiding,
    AD_szCalibrationDuration,
    AD_cbReverseDecOnFlip,
    AD_cbAssumeOrthogonal,
    AD_cbSlewDetection,
    AD_cbUseDecComp,
    AD_cbBeepForLostStar,
    AD_GUIDER_TAB_BOUNDARY,        // --------------- end of guiding tab controls

    AD_szBLCompCtrls,
    AD_szMaxRAAmt,
    AD_szMaxDecAmt,
    AD_szDecGuideMode,
    AD_MOUNT_TAB_BOUNDARY,          // ----------- end of mount tab controls

    AD_AOTravel,
    AD_szCalStepsPerIteration,
    AD_szSamplesToAverage,
    AD_szBumpPercentage,
    AD_szBumpSteps,
    AD_cbBumpOnDither,
    AD_szBumpBLCompCtrls,
    AD_cbClearAOCalibration,
    AD_cbEnableAOGuiding,
    AD_cbRotatorReverse,
    AD_DEVICES_TAB_BOUNDARY         // ----------- end of devices tab controls
};

struct BrainCtrlInfo
{
    wxObject *panelCtrl;
    bool isPositioned;           // debug only

    BrainCtrlInfo()
        :
        panelCtrl(0),
        isPositioned(false)
    {
    }

    BrainCtrlInfo(BRAIN_CTRL_IDS id, wxObject *ctrl)
        :
        panelCtrl(ctrl),
        isPositioned(false)
    {
    }
};

typedef std::map<BRAIN_CTRL_IDS, BrainCtrlInfo> BrainCtrlIdMap;

class ConfigDialogPane : public wxStaticBoxSizer
{
protected:
    wxWindow *m_pParent;
public:
    ConfigDialogPane(const wxString& heading, wxWindow *pParent);
    virtual ~ConfigDialogPane(void) {};

    virtual void LoadValues(void) = 0;
    virtual void UnloadValues(void) = 0;
    virtual void Undo();

    wxWindow *GetSingleCtrl(BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS id);
    wxSizer *GetSizerCtrl(BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS id);
    void CondAddCtrl(wxSizer *szr, BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS id, const wxSizerFlags& flags = 0);
    virtual void OnImageScaleChange();         // Only for adjustments made within the AD panels
    virtual void EnableDecControls(bool enable);        // Needed for guide algo ConfigDialogPanes which inherit directly from this class

protected:
    wxSizer *MakeLabeledControl(const wxString& label, wxWindow *pControl, const wxString& toolTip, wxWindow *pControl2 = nullptr);
    void DoAdd(wxSizer *pSizer);
    void DoAdd(wxWindow *pWindow);
    void DoAdd(wxWindow *pWindow, const wxString& toolTip);
    void DoAdd(const wxString& Label, wxWindow *pControl, const wxString& toolTip, wxWindow *pControl2 = nullptr);

    int StringWidth(const wxString& string);
    int StringArrayWidth(wxString string[], int nElements);
};

class MyFrame;
class AdvancedDialog;

class ConfigDialogCtrlSet
{
protected:
    wxWindow *m_pParent;
    AdvancedDialog *m_pAdvDlg;

public:
    ConfigDialogCtrlSet(wxWindow *pParent, AdvancedDialog *pAdvancedDialog, BrainCtrlIdMap& CtrlMap);
    virtual ~ConfigDialogCtrlSet(void) {};

    virtual void LoadValues(void) = 0;
    virtual void UnloadValues(void) = 0;

public:
    wxSizer *MakeLabeledControl(BRAIN_CTRL_IDS id, const wxString& label, wxWindow *pControl, const wxString& toolTip);
    void AddMapElement(BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS, wxObject *pElem);
    void AddGroup(BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS id, wxSizer *pSizer);      // Sizer
    void AddCtrl(BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS id, wxControl *pCtrl);      // Bare control
    void AddLabeledCtrl(BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS id, const wxString& Label, wxControl *pCtrl, const wxString& toolTip);
    void AddCtrl(BrainCtrlIdMap& CtrlMap, BRAIN_CTRL_IDS id, wxControl *pCtrl, const wxString& toolTip);     // Control with tooltip

    wxWindow *GetParentWindow(BRAIN_CTRL_IDS id);

    int StringWidth(const wxString& string);
    int StringArrayWidth(wxString string[], int nElements);
    int StringArrayWidth(const wxArrayString& ary);
};
#endif // CONFIG_DIALOG_H_INCLUDED
