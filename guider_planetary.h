/*
 *  guider_planetary.h
 *  PHD Guiding
 *
 *  Planetary detection extensions by Leo Shatz
 *  Copyright (c) 2023-2024 Leo Shatz
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

#pragma once

#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

// Planetary guiding/tracking state and control class
class GuiderPlanet
{
private:
    // Planetary guiding parameters
    bool m_Planetary_enabled;
    bool m_PlanetaryDetectionPaused;
    bool m_RoiEnabled;
    bool m_prevCaptureActive;

    double m_Planetary_minRadius;
    double m_Planetary_maxRadius;
    int    m_Planetary_lowThreshold;
    int    m_Planetary_highThreshold;
    bool   m_Planetary_ShowElementsButtonState;
    bool   m_Planetary_ShowElementsVisual;

    bool   m_measuringSharpnessMode;
    bool   m_unknownHFD;
    double m_focusSharpness;
    int    m_starProfileSize;

    float  m_PlanetEccentricity;
    float  m_PlanetAngle;

    wxMutex m_syncLock;
    cv::Point2f m_prevClickedPoint;

    std::vector<cv::Point2f> m_diskContour;
    int m_centoid_x;
    int m_centoid_y;
    int m_sm_circle_x;
    int m_sm_circle_y;
    int m_frameWidth;
    int m_frameHeight;

    cv::Point2f m_origPoint;
    cv::Point2f m_cameraSimulationMove;
    cv::Point2f m_cameraSimulationRefPoint;

public:
    wxString m_statusMsg;
    bool m_detected;
    float m_center_x;
    float m_center_y;
    int m_radius;
    int m_searchRegion;
    float m_prevSearchRegion;

    bool m_roiActive;
    cv::Rect m_roiRect;
    bool m_roiClicked;
    int m_clicked_x;
    int m_clicked_y;

    int m_detectionCounter;
    bool m_simulationZeroOffset;
    bool m_cameraSimulationRefPointValid;

public:
    GuiderPlanet();
    ~GuiderPlanet();

    bool FindPlanet(const usImage* pImage, bool autoSelect = false);
    void RestartSimulatorErrorDetection();

    double GetHFD();
    wxString GetHfdLabel();
    bool IsPixelMetrics();
    void ToggleSharpness();
    void GetDetectionStatus(wxString& statusMsg);
    void NotifyCameraConnect(bool connected);
    bool UpdateCaptureState(bool CaptureActive);
    void SaveCameraSimulationMove(double rx, double ry);

    bool GetPlanetaryEnableState() { return m_Planetary_enabled; }
    void SetPlanetaryEnableState(bool enabled) { m_Planetary_enabled = enabled; }
    bool GetDetectionPausedState() { return m_PlanetaryDetectionPaused; }
    void SetDetectionPausedState(bool paused) { m_PlanetaryDetectionPaused = paused; }
    void   SetPlanetaryParam_minRadius(double val) { m_Planetary_minRadius = val; }
    double GetPlanetaryParam_minRadius() { return m_Planetary_minRadius; }
    void   SetPlanetaryParam_maxRadius(double val) { m_Planetary_maxRadius = val; }
    double GetPlanetaryParam_maxRadius() { return m_Planetary_maxRadius; }
    bool GetRoiEnableState() { return m_RoiEnabled; }
    void SetRoiEnableState(bool enabled) { m_RoiEnabled = enabled; }
    void SetPlanetaryParam_lowThreshold(int value) { m_Planetary_lowThreshold = value; }
    int  GetPlanetaryParam_lowThreshold() { return m_Planetary_lowThreshold; }
    void SetPlanetaryParam_highThreshold(int value) { m_Planetary_highThreshold = value; }
    int  GetPlanetaryParam_highThreshold() { return m_Planetary_highThreshold; }

    void SetPlanetaryElementsVisual(bool state);
    bool GetPlanetaryElementsVisual() { return m_Planetary_ShowElementsVisual; }
    void SetPlanetaryElementsButtonState(bool state) { m_Planetary_ShowElementsButtonState = state; }
    bool GetPlanetaryElementsButtonState() { return m_Planetary_ShowElementsButtonState; }

public:
    // Displaying visual aid for planetary parameter tuning
    bool m_draw_PlanetaryHelper;
    void PlanetVisualRefresh() { m_draw_PlanetaryHelper = true; }
    void PlanetVisualHelper(wxDC& dc, Star primaryStar, double scaleFactor);

private:
    wxStopWatch m_PlanetWatchdog;
    typedef struct {
        float x;
        float y;
        float radius;
    } CircleDescriptor;
    struct LineParameters {
        bool  valid;
        bool  vertical;
        float slope;
        float b;
    } m_DiameterLineParameters;
    typedef struct WeightedCircle {
        float x;
        float y;
        float r;
        float score;
    } WeightedCircle;

private:
    double  ComputeSobelSharpness(const cv::Mat& img);
    double  CalcSharpness(cv::Mat& FullFrame, cv::Point2f& clickedPoint, bool detectionResult);
    void    CalcLineParams(CircleDescriptor p1, CircleDescriptor p2);
    int     RefineDiskCenter(float& bestScore, CircleDescriptor& diskCenter, std::vector<cv::Point2f>& diskContour, int minRadius, int maxRadius, float searchRadius, float resolution = 1.0);
    float   FindContourCenter(CircleDescriptor& diskCenter, CircleDescriptor& smallestCircle, std::vector<cv::Point2f>& bestContourVector, cv::Moments& mu, int minRadius, int maxRadius);
    void    FindCenters(cv::Mat image, const std::vector<cv::Point>& contour, CircleDescriptor& bestCentroid, CircleDescriptor& smallestCircle, std::vector<cv::Point2f>& bestContour, cv::Moments& mu, int minRadius, int maxRadius);
    bool    FindPlanetCenter(cv::Mat img8, int minRadius, int maxRadius, bool roiActive, cv::Point2f& clickedPoint, cv::Rect& roiRect, bool activeRoiLimits, float distanceRoiMax);
    void    UpdateDetectionErrorInSimulator(cv::Point2f& clickedPoint);
};
