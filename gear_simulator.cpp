/*
 *  gear_simulator.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Reimplemented for PHD2 by Andy Galasso.
 *  Copyright (c) 2006-2010 Craig Stark
 *  Copyright (c) 2015-2018 Andy Galasso
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

#ifdef SIMULATOR

#include "camera.h"
#include "gear_simulator.h"
#include "image_math.h"

#include <wx/dir.h>
#include <wx/gdicmn.h>
#include <wx/stopwatch.h>
#include <wx/radiobut.h>

#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <wx/tokenzr.h>

#define SIMMODE 3   // 1=FITS, 2=BMP, 3=Generate
// #define SIMDEBUG

/* simulation parameters for SIMMODE = 3*/
// #define SIM_FILE_DISPLACEMENTS          // subset of SIMMODE = 3, reading raw star displacements from a file

struct SimCamParams
{
    static unsigned int width;
    static unsigned int height;
    static unsigned int border;
    static unsigned int nr_stars;
    static unsigned int nr_hot_pixels;
    static double noise_multiplier;
    static double dec_backlash;
    static double pe_scale;
    static double dec_drift_rate;
    static double seeing_scale;
    static double cam_angle;
    static double guide_rate;
    static PierSide pier_side;
    static bool reverse_dec_pulse_on_west_side;
    static unsigned int clouds_inten;
    static double clouds_opacity;                   // UI has percentage, internally 0-1.0
    static double image_scale; // arc-sec per pixel
    static bool use_pe;
    static bool use_stiction;
    static bool use_default_pe_params;
    static double custom_pe_amp;
    static double custom_pe_period;
    static bool show_comet;
    static double comet_rate_x;
    static double comet_rate_y;
    static bool allow_async_st4;
    static unsigned int frame_download_ms;
};

unsigned int SimCamParams::width = 752;          // simulated camera image width
unsigned int SimCamParams::height = 580;         // simulated camera image height
unsigned int SimCamParams::border = 12;          // do not place any stars within this size border
unsigned int SimCamParams::nr_stars;             // number of stars to generate
unsigned int SimCamParams::nr_hot_pixels;        // number of hot pixels to generate
double SimCamParams::noise_multiplier;           // noise factor, increase to increase noise
double SimCamParams::dec_backlash;               // dec backlash amount (pixels)
double SimCamParams::pe_scale;                   // scale factor controlling magnitude of simulated periodic error
double SimCamParams::dec_drift_rate;             // dec drift rate (pixels per second)
double SimCamParams::seeing_scale;               // simulated seeing scale factor
double SimCamParams::cam_angle;                  // simulated camera angle (degrees)
double SimCamParams::guide_rate;                 // guide rate, pixels per second
PierSide SimCamParams::pier_side;                // side of pier
bool SimCamParams::reverse_dec_pulse_on_west_side; // reverse dec pulse on west side of pier, like ASCOM pulse guided equatorial mounts
unsigned int SimCamParams::clouds_inten = 50;           // seed brightness for cloud contribution
double SimCamParams::clouds_opacity;
double SimCamParams::image_scale;                // arc-sec per pixel
bool SimCamParams::use_pe;
bool SimCamParams::use_stiction;
bool SimCamParams::use_default_pe_params;
double SimCamParams::custom_pe_amp;
double SimCamParams::custom_pe_period;
bool SimCamParams::show_comet;
double SimCamParams::comet_rate_x;
double SimCamParams::comet_rate_y;
bool SimCamParams::allow_async_st4 = true;
unsigned int SimCamParams::frame_download_ms;    // frame download time, ms

// Note: these are all in units appropriate for the UI
#define NR_STARS_DEFAULT 20
#define NR_HOT_PIXELS_DEFAULT 8
#define NOISE_DEFAULT 2.0
#define NOISE_MAX 5.0
#define DEC_BACKLASH_DEFAULT 5.0                  // arc-sec
#define DEC_BACKLASH_MAX 100.0
#define DEC_DRIFT_DEFAULT 5.0                     // arc-sec per minute
#define DEC_DRIFT_MAX 30.0
#define SEEING_DEFAULT 2.0                        // arc-sec FWHM
#define SEEING_MAX 5.0
#define CAM_ANGLE_DEFAULT 15.0
#define CAM_ANGLE_MAX 360.0
#define GUIDE_RATE_DEFAULT (1.0 * 15.0)           // multiples of sidereal rate, a-s/sec
#define GUIDE_RATE_MAX (1.0 * 15.0)
#define PIER_SIDE_DEFAULT PIER_SIDE_EAST
#define REVERSE_DEC_PULSE_ON_WEST_SIDE_DEFAULT true
#define CLOUDS_OPACITY_DEFAULT 0
#define USE_PE_DEFAULT true
#define USE_STICTION_DEFAULT false
#define PE_SCALE_DEFAULT 5.0                    // amplitude arc-sec
#define PE_SCALE_MAX 30.0
#define USE_PE_DEFAULT_PARAMS true
#define PE_CUSTOM_AMP_DEFAULT 2.0               // Give them a trivial 2 a-s 4 min smooth curve
#define PE_CUSTOM_PERIOD_DEFAULT 240.0
#define SHOW_COMET_DEFAULT false
#define COMET_RATE_X_DEFAULT 555.0              // pixels per hour
#define COMET_RATE_Y_DEFAULT -123.4              // pixels per hour
#define SIM_FILE_DISPLACEMENTS_DEFAULT "star_displacements.csv"

// Needed to handle legacy registry values that may no longer be in correct units or range
static double range_check(double thisval, double minval, double maxval)
{
    return wxMin(wxMax(thisval, minval), maxval);
}

static void load_sim_params()
{
    SimCamParams::image_scale = pFrame->GetCameraPixelScale();

    SimCamParams::nr_stars = pConfig->Profile.GetInt("/SimCam/nr_stars", NR_STARS_DEFAULT);
    SimCamParams::nr_hot_pixels = pConfig->Profile.GetInt("/SimCam/nr_hot_pixels", NR_HOT_PIXELS_DEFAULT);
    SimCamParams::noise_multiplier = pConfig->Profile.GetDouble("/SimCam/noise", NOISE_DEFAULT);
    SimCamParams::use_pe = pConfig->Profile.GetBoolean("/SimCam/use_pe", USE_PE_DEFAULT);
    SimCamParams::use_stiction = pConfig->Profile.GetBoolean("/SimCam/use_stiction", USE_STICTION_DEFAULT);
    SimCamParams::use_default_pe_params = pConfig->Profile.GetBoolean("/SimCam/use_default_pe", USE_PE_DEFAULT_PARAMS);
    SimCamParams::custom_pe_amp = pConfig->Profile.GetDouble("/SimCam/pe_cust_amp", PE_CUSTOM_AMP_DEFAULT);
    SimCamParams::custom_pe_period = pConfig->Profile.GetDouble("/SimCam/pe_cust_period", PE_CUSTOM_PERIOD_DEFAULT);

    double dval = pConfig->Profile.GetDouble("/SimCam/dec_drift", DEC_DRIFT_DEFAULT);
    SimCamParams::dec_drift_rate = range_check(dval, -DEC_DRIFT_MAX, DEC_DRIFT_MAX) / (SimCamParams::image_scale * 60.0);  //a-s per min is saved
    // backlash is in arc-secs in UI - map to px for internal use
    dval = pConfig->Profile.GetDouble("/SimCam/dec_backlash", DEC_BACKLASH_DEFAULT);
    SimCamParams::dec_backlash = range_check(dval, 0, DEC_BACKLASH_MAX) / SimCamParams::image_scale;
    SimCamParams::pe_scale = range_check(pConfig->Profile.GetDouble("/SimCam/pe_scale", PE_SCALE_DEFAULT), 0, PE_SCALE_MAX);

    SimCamParams::seeing_scale = range_check(pConfig->Profile.GetDouble("/SimCam/seeing_scale", SEEING_DEFAULT), 0, SEEING_MAX);       // FWHM a-s
    SimCamParams::cam_angle = pConfig->Profile.GetDouble("/SimCam/cam_angle", CAM_ANGLE_DEFAULT);
    SimCamParams::clouds_opacity = pConfig->Profile.GetDouble("/SimCam/clouds_opacity", CLOUDS_OPACITY_DEFAULT);
    SimCamParams::guide_rate = range_check(pConfig->Profile.GetDouble("/SimCam/guide_rate", GUIDE_RATE_DEFAULT), 0, GUIDE_RATE_MAX);
    SimCamParams::pier_side = (PierSide) pConfig->Profile.GetInt("/SimCam/pier_side", PIER_SIDE_DEFAULT);
    SimCamParams::reverse_dec_pulse_on_west_side = pConfig->Profile.GetBoolean("/SimCam/reverse_dec_pulse_on_west_side", REVERSE_DEC_PULSE_ON_WEST_SIDE_DEFAULT);

    SimCamParams::show_comet = pConfig->Profile.GetBoolean("/SimCam/show_comet", SHOW_COMET_DEFAULT);
    SimCamParams::comet_rate_x = pConfig->Profile.GetDouble("/SimCam/comet_rate_x", COMET_RATE_X_DEFAULT);
    SimCamParams::comet_rate_y = pConfig->Profile.GetDouble("/SimCam/comet_rate_y", COMET_RATE_Y_DEFAULT);

    SimCamParams::frame_download_ms = pConfig->Profile.GetInt("/SimCam/frame_download_ms", 50);
}

static void save_sim_params()
{
    pConfig->Profile.SetInt("/SimCam/nr_stars", SimCamParams::nr_stars);
    pConfig->Profile.SetInt("/SimCam/nr_hot_pixels", SimCamParams::nr_hot_pixels);
    pConfig->Profile.SetDouble("/SimCam/noise", SimCamParams::noise_multiplier);
    pConfig->Profile.SetDouble("/SimCam/dec_backlash", SimCamParams::dec_backlash * SimCamParams::image_scale);
    pConfig->Profile.SetBoolean("/SimCam/use_pe", SimCamParams::use_pe);
    pConfig->Profile.SetBoolean("/SimCam/use_stiction", SimCamParams::use_stiction);
    pConfig->Profile.SetBoolean("/SimCam/use_default_pe", SimCamParams::use_default_pe_params);
    pConfig->Profile.SetDouble("/SimCam/pe_scale", SimCamParams::pe_scale);
    pConfig->Profile.SetDouble("/SimCam/pe_cust_amp", SimCamParams::custom_pe_amp);
    pConfig->Profile.SetDouble("/SimCam/pe_cust_period", SimCamParams::custom_pe_period);
    pConfig->Profile.SetDouble("/SimCam/dec_drift", SimCamParams::dec_drift_rate * 60.0 * SimCamParams::image_scale);
    pConfig->Profile.SetDouble("/SimCam/seeing_scale", SimCamParams::seeing_scale);
    pConfig->Profile.SetDouble("/SimCam/clouds_opacity", SimCamParams::clouds_opacity);
    pConfig->Profile.SetDouble("/SimCam/cam_angle", SimCamParams::cam_angle);
    pConfig->Profile.SetDouble("/SimCam/guide_rate", SimCamParams::guide_rate);
    pConfig->Profile.SetInt("/SimCam/pier_side", (int) SimCamParams::pier_side);
    pConfig->Profile.SetBoolean("/SimCam/reverse_dec_pulse_on_west_side", SimCamParams::reverse_dec_pulse_on_west_side);
    pConfig->Profile.SetBoolean("/SimCam/show_comet", SimCamParams::show_comet);
    pConfig->Profile.SetDouble("/SimCam/comet_rate_x", SimCamParams::comet_rate_x);
    pConfig->Profile.SetDouble("/SimCam/comet_rate_y", SimCamParams::comet_rate_y);
    pConfig->Profile.SetInt("/SimCam/frame_download_ms", SimCamParams::frame_download_ms);
}

#ifdef STEPGUIDER_SIMULATOR

struct SimAoParams
{
    static unsigned int max_position; // max position in steps
    static double scale;           // arcsec per step
    static double angle;           // angle relative to camera (degrees)
};

unsigned int SimAoParams::max_position = 45;
double SimAoParams::scale = 0.10;
double SimAoParams::angle = 35.0;

class StepGuiderSimulator : public StepGuider
{
public:
    StepGuiderSimulator();
    virtual ~StepGuiderSimulator();

    bool Connect() override;
    bool Disconnect() override;

private:
    bool Center() override;
    STEP_RESULT Step(GUIDE_DIRECTION direction, int steps) override;
    int MaxPosition(GUIDE_DIRECTION direction) const override;
    bool SetMaxPosition(int steps) override;
    bool HasNonGuiMove() override;
};

static StepGuiderSimulator *s_sim_ao;

StepGuiderSimulator::StepGuiderSimulator()
{
    m_Name = _("AO-Simulator");
    SimAoParams::max_position = pConfig->Profile.GetInt("/SimAo/max_steps", 45);
}

StepGuiderSimulator::~StepGuiderSimulator()
{
}

bool StepGuiderSimulator::Connect()
{
    if (StepGuider::Connect())
        return true;

    ZeroCurrentPosition();

    s_sim_ao = this;

    Debug.AddLine("AO Simulator Connected");

    if (!pCamera || pCamera->Name != _T("Simulator"))
    {
        pFrame->Alert(_("The AO Simulator only works with the Camera Simulator. You should either disconnect the AO Simulator or connect the Camera Simulator."));
    }

    return false;
}

bool StepGuiderSimulator::Disconnect()
{
    if (StepGuider::Disconnect())
        return true;

    if (s_sim_ao == this) {
        Debug.AddLine("AO Simulator Disconnected");
        s_sim_ao = 0;
    }

    return false;
}

bool StepGuiderSimulator::Center()
{
    ZeroCurrentPosition();
    return false;
}

StepGuider::STEP_RESULT StepGuiderSimulator::Step(GUIDE_DIRECTION direction, int steps)
{
#if 0 // enable to test step failure
    wxPoint pos = GetAoPos();
    if (direction == LEFT && pos.x - steps < -35)
    {
        Debug.Write("simulate step failure\n");
        return STEP_LIMIT_REACHED;
    }
#endif

    // parent class maintains x/y offsets, so nothing to do here. Just simulate a delay.
    enum { LATENCY_MS_PER_STEP = 5 };
    wxMilliSleep(steps * LATENCY_MS_PER_STEP);
    return STEP_OK;
}

int StepGuiderSimulator::MaxPosition(GUIDE_DIRECTION direction) const
{
    return SimAoParams::max_position;
}

bool StepGuiderSimulator::SetMaxPosition(int steps)
{
    SimAoParams::max_position = (unsigned int) steps;
    pConfig->Profile.SetInt("/SimAo/max_steps", steps);
    return false;
}

bool StepGuiderSimulator::HasNonGuiMove()
{
    return true;
}

#endif // STEPGUIDER_SIMULATOR

#ifdef ROTATOR_SIMULATOR

class RotatorSimulator : public Rotator
{
public:
    RotatorSimulator();
    virtual ~RotatorSimulator();

    bool Connect() override;
    bool Disconnect() override;

    // get the display name of the rotator device
    wxString Name() const override;

    // get the rotator position in degrees
    float Position() const override;
};

RotatorSimulator::RotatorSimulator()
{
}

RotatorSimulator::~RotatorSimulator()
{
}

bool RotatorSimulator::Connect()
{
    if (!pCamera || pCamera->Name != _T("Simulator"))
    {
        pFrame->Alert(_("The Rotator Simulator only works with the Camera Simulator. You must either disconnect the Rotator Simulator or connect the Camera Simulator."));
        return true;
    }

    Rotator::Connect();
    return false;
}

bool RotatorSimulator::Disconnect()
{
    Rotator::Disconnect();
    return false;
}

wxString RotatorSimulator::Name() const
{
    return _T("Simulator");
}

float RotatorSimulator::Position() const
{
    assert(IsConnected());
    return SimCamParams::cam_angle;
}

#endif // ROTATOR_SIMULATOR

// value with backlash
//   There is an index value, and a lower and upper limit separated by the
//   backlash amount. When the index moves past the upper limit, it carries
//   both limits along, likewise for the lower limit. The current value is
//   the value of the upper limit.
struct BacklashVal
{
    double cur;    // current index value
    double upper;  // upper limit
    double amount; // backlash amount (lower limit is upper - amount)

    BacklashVal() { }

    BacklashVal(double backlash_amount)
        : cur(0), upper(backlash_amount), amount(backlash_amount) { }

    double val() const { return upper; }

    void incr(double d) {
        cur += d;
        if (d > 0.) {
            if (cur > upper)
                upper = cur;
        }
        else if (d < 0.) {
            if (cur < upper - amount)
                upper = cur + amount;
        }
    }
};

struct SimStar
{
    wxRealPoint pos;
    double inten;
};

struct StictionSim
{
    int lastDirection;
    bool pending;
    double adjustment;

    StictionSim()
    {
        lastDirection = NONE;
        pending = false;
        adjustment = 0.;
    }

    double GetAdjustment(int Direction, int Duration, double Distance)
    {
        double rslt = 0;
        if (lastDirection != NONE)
        {
            if (Duration > 300 && Direction != lastDirection && !pending)
            {
                adjustment = Distance / 3.0;
                pending = true;
                Debug.Write(wxString::Format("Stiction: reduced distance by %0.2f\n", adjustment));
                rslt = -adjustment;
            }
            else
            {
                if (pending)
                {
                    if (Direction == lastDirection)
                    {
                        rslt = adjustment;
                        Debug.Write(wxString::Format("Stiction: increased distance by %0.2f\n", adjustment));
                        adjustment = 0;
                    }
                    pending = false;
                }
            }
        }
        lastDirection = Direction;
        return rslt;
    }
};

static const double AMBIENT_TEMP = 15.;
static const double MIN_COOLER_TEMP = -15.;

struct Cooler
{
    bool on;
    double startTemp;
    double endTemp;
    double setTemp;
    time_t endTime;
    double rate; // degrees per second
    double direction; // -1 = cooling, +1 = warming

    Cooler() : on(false), startTemp(AMBIENT_TEMP), endTemp(AMBIENT_TEMP), setTemp(AMBIENT_TEMP), endTime(0), rate(1. / 8.), direction(0.) { }

    double CurrentTemp() const
    {
        time_t now = wxDateTime::GetTimeNow();

        if (now >= endTime)
            return endTemp;

        return endTemp - rate * direction * (double)(endTime - now);
    }

    void TurnOn()
    {
        if (!on)
        {
            _SetTemp(CurrentTemp());
            on = true;
        }
    }

    void TurnOff()
    {
        if (on)
        {
            _SetTemp(AMBIENT_TEMP);
            on = false;
        }
    }

    void _SetTemp(double newtemp)
    {
        startTemp = CurrentTemp();
        endTemp = std::max(std::min(newtemp, AMBIENT_TEMP), MIN_COOLER_TEMP);
        double dt = ceil(fabs(endTemp - startTemp) / rate);
        endTime = wxDateTime::GetTimeNow() + (time_t) dt;
        direction = endTemp < startTemp ? -1. : +1.;
    }

    void SetTemp(double newtemp)
    {
        assert(on);
        _SetTemp(newtemp);
        setTemp = newtemp;
    }
};

struct SimCamState
{
    unsigned int width;
    unsigned int height;
    wxVector<SimStar> stars; // star positions and intensities (ra, dec)
    wxVector<wxPoint> hotpx; // hot pixels
    double ra_ofs;           // assume no backlash in RA
    BacklashVal dec_ofs;     // simulate backlash in DEC
    double cum_dec_drift;    // cumulative dec drift
    wxStopWatch timer;       // platform-independent timer
    long last_exposure_time; // last exposure time, milliseconds
    Cooler cooler;           // simulated cooler
    StictionSim stictionSim;

#ifdef SIMDEBUG
    wxFFile DebugFile;
    double last_ra_move;
    double last_dec_move;
#endif

#ifdef SIM_FILE_DISPLACEMENTS
    wxFileInputStream* pIStream;
    wxTextInputStream* pText;
    double scaleConversion;
    void ReadDisplacements(double& cumX, double& cumY);
#endif

#if SIMMODE == 1
    wxDir dir;
    bool dirStarted;
    bool ReadNextImage(usImage& img, const wxRect& subframe);
#endif

    void Initialize();
    void FillImage(usImage& img, const wxRect& subframe, int exptime, int gain, int offset);
};

void SimCamState::Initialize()
{
    width = SimCamParams::width;
    height = SimCamParams::height;
    // generate stars at random positions but no closer than 12 pixels from any edge
    unsigned int const nr_stars = SimCamParams::nr_stars;
    stars.resize(nr_stars);
    unsigned int const border = SimCamParams::border;

    srand(2); // always generate the same stars
    for (unsigned int i = 0; i < nr_stars; i++)
    {
        // generate stars in ra/dec coordinates
        stars[i].pos.x = (double)(rand() % (width - 2 * border)) - 0.5 * width;
        stars[i].pos.y = (double)(rand() % (height - 2 * border)) - 0.5 * height;
        double r = (double) (rand() % 90) / 3.0; // 0..30
        if (i == 10)
            stars[i].inten = 30.1;                              // Always have one saturated star
        else
            stars[i].inten = 0.1 + (double) (r * r * r) / 9000.0;

        // force a couple stars to be close together. This is a useful test for Star::AutoFind
        if (i == 3)
        {
            stars[i].pos.x = stars[i - 1].pos.x + 8;
            stars[i].pos.y = stars[i - 1].pos.y + 8;
            stars[i].inten = stars[i - 1].inten;
        }
    }

    // generate hot pixels
    unsigned int const nr_hot = SimCamParams::nr_hot_pixels;
    hotpx.resize(nr_hot);
    for (unsigned int i = 0; i < nr_hot; i++) {
        hotpx[i].x = rand() % width;
        hotpx[i].y = rand() % height;
    }
    srand(clock());
    ra_ofs = 0.;
    dec_ofs = BacklashVal(SimCamParams::dec_backlash);
    cum_dec_drift = 0.;
    last_exposure_time = 0;

#if SIMMODE == 1
    dirStarted = false;
#endif

#ifdef SIM_FILE_DISPLACEMENTS
    pIStream = nullptr;
    wxString csvName = Debug.GetLogDir() + PATHSEPSTR + SIM_FILE_DISPLACEMENTS_DEFAULT;
    if (wxFile::Exists(csvName))
        pIStream = new wxFileInputStream(csvName);
    else
    {
        wxFileDialog dlg(pFrame, _("Choose a star displacements file"), wxEmptyString, wxEmptyString,
            wxT("Comma-separated files (*.csv)|*.csv"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        dlg.SetDirectory(Debug.GetLogDir());
        if (dlg.ShowModal() == wxID_OK)
        {
            pIStream = new wxFileInputStream(dlg.GetPath());
            if (!pIStream->IsOk())
            {
                wxMessageBox(_("Can't use this file for star displacements"));
            }
        }
        else
            wxMessageBox(_("Can't simulate any star movement without a displacement file"));
    }
    if (pIStream && pIStream->IsOk())
        pText = new wxTextInputStream(*pIStream);
    else
        pText = nullptr;                   // User cancelled open dialog or file is useless
    scaleConversion = 1.0;          // safe default
#endif

#ifdef SIMDEBUG
    DebugFile.Open ("Sim_Debug.txt", "w");
#ifdef SIM_FILE_DISPLACEMENTS
    DebugFile.Write ("Total_X, Total_Y, RA_Ofs, Dec_Ofs \n");
#else
    DebugFile.Write ("PE, Drift, RA_Seeing, Dec_Seeing, Total_X, Total_Y, RA_Ofs, Dec_Ofs, \n");
#endif
#endif
}

#if SIMMODE == 1
bool SimCamState::ReadNextImage(usImage& img, const wxRect& subframe)
{
    wxString filename;

    if (!dir.IsOpened())
    {
        dir.Open(wxFileName(Debug.GetLogDir(), "sim_images").GetFullPath());
    }
    if (dir.IsOpened())
    {
        if (!dirStarted)
        {
            dir.GetFirst(&filename, "*.fit", wxDIR_FILES);
            dirStarted = true;
        }
        else
        {
            if (!dir.GetNext(&filename))
                dir.GetFirst(&filename, "*.fit", wxDIR_FILES);
        }
    }
    if (filename.IsEmpty())
    {
        return true;
    }

    Debug.Write("Sim file opened: " + filename + "\n");
    fitsfile *fptr;  // FITS file pointer
    int status = 0;  // CFITSIO status value MUST be initialized to zero!

    if (PHD_fits_open_diskfile(&fptr, wxFileName(dir.GetName(), filename).GetFullPath(), READONLY, &status))
        return true;

    int hdutype;
    if (fits_get_hdu_type(fptr, &hdutype, &status) || hdutype != IMAGE_HDU)
    {
        pFrame->Alert(_("FITS file is not of an image"));
        PHD_fits_close_file(fptr);
        return true;
    }

    int naxis;
    fits_get_img_dim(fptr, &naxis, &status);

    int nhdus;
    fits_get_num_hdus(fptr, &nhdus, &status);
    if ((nhdus != 1) || (naxis != 2)) {
        pFrame->Alert(_("Unsupported type or read error loading FITS file"));
        PHD_fits_close_file(fptr);
        return true;
    }

    long fits_size[2];
    fits_get_img_size(fptr, 2, fits_size, &status);

    int xsize = (int) fits_size[0];
    int ysize = (int) fits_size[1];

    if (img.Init(xsize, ysize)) {
        pFrame->Alert(_("Memory allocation error"));
        PHD_fits_close_file(fptr);
        return true;
    }

    unsigned short *buf = new unsigned short[img.NPixels];
    bool useSubframe = !subframe.IsEmpty();
    wxRect frame;
    if (useSubframe)
        frame = subframe;
    else
        frame = wxRect(0, 0, xsize, ysize);

    long inc[] = { 1, 1 };
    long fpixel[] = { frame.GetLeft() + 1, frame.GetTop() + 1 };
    long lpixel[] = { frame.GetRight() + 1, frame.GetBottom() + 1 };
    if (fits_read_subset(fptr, TUSHORT, fpixel, lpixel, inc, nullptr, buf, nullptr, &status))
    {
        pFrame->Alert(_("Error reading data"));
        PHD_fits_close_file(fptr);
        return true;
    }

    if (useSubframe)
    {
        img.Subframe = subframe;

        // Clear out the image
        img.Clear();

        int i = 0;
        for (int y = 0; y < subframe.height; y++)
        {
            unsigned short *dst = img.ImageData + (y + subframe.y) * xsize + subframe.x;
            for (int x = 0; x < subframe.width; x++, i++)
                *dst++ = (unsigned short) buf[i];
        }
    }
    else
    {
        for (unsigned int i = 0; i < img.NPixels; i++)
            img.ImageData[i] = (unsigned short) buf[i];
    }

    delete[] buf;

    PHD_fits_close_file(fptr);

    return false;
}
#endif // SIMMODE == 1

// get a pair of normally-distributed independent random values - Box-Muller algorithm, sigma=1
static void rand_normal(double r[2])
{
    double u = (double) rand() / RAND_MAX;
    double v = (double) rand() / RAND_MAX;
    double const a = sqrt(-2.0 * log(u));
    double const p = 2 * M_PI * v;
    r[0] = a * cos(p);
    r[1] = a * sin(p);
}

inline static unsigned short *pixel_addr(usImage& img, int x, int y)
{
    if (x < 0 || x >= img.Size.x)
        return 0;
    if (y < 0 || y >= img.Size.y)
        return 0;
    return &img.Pixel(x, y);
}

inline static void set_pixel(usImage& img, int x, int y, unsigned short val)
{
    unsigned short *const addr = pixel_addr(img, x, y);
    if (addr)
        *addr = val;
}

inline static void incr_pixel(usImage& img, int x, int y, unsigned int val)
{
    unsigned short *const addr = pixel_addr(img, x, y);
    if (addr) {
        unsigned int t = *addr;
        t += val;
        if (t > (unsigned int)(unsigned short)-1)
            *addr = (unsigned short)-1;
        else
            *addr = (unsigned short)t;
    }
}

static void render_comet(usImage& img, int binning, const wxRect& subframe, const wxRealPoint& p, double inten)
{
    enum { WIDTH = 5 };
    double STAR[][WIDTH] = { { 0.0, 0.8, 2.2, 0.8, 0.0, },
                             { 0.8, 16.6, 46.1, 16.6, 0.8, },
                             { 2.2, 46.1, 128.0, 46.1, 2.2, },
                             { 0.8, 16.6, 46.1, 16.6, 0.8, },
                             { 0.0, 0.8, 2.2, 0.8, 0.0, },
                            };

    wxRealPoint intpart;
    double fx = modf(p.x / (double) binning, &intpart.x);
    double fy = modf(p.y / (double) binning, &intpart.y);
    double f00 = (1.0 - fx) * (1.0 - fy);
    double f01 = (1.0 - fx) * fy;
    double f10 = fx * (1.0 - fy);
    double f11 = fx * fy;

    double d[WIDTH + 1][WIDTH + 1] = { { 0.0 } };
    for (unsigned int i = 0; i < WIDTH; i++)
    for (unsigned int j = 0; j < WIDTH; j++)
    {
        double s = STAR[i][j];
        if (s > 0.0)
        {
            s *= inten / 256.0;
            d[i][j] += f00 * s;
            d[i + 1][j] += f10 * s;
            d[i][j + 1] += f01 * s;
            d[i + 1][j + 1] += f11 * s;
        }
    }

    wxPoint c((int)intpart.x - (WIDTH - 1) / 2,
        (int)intpart.y - (WIDTH - 1) / 2);

    for (unsigned int x_inc = 0; x_inc < 10; x_inc++)
    {
        for (double y = -1; y < 1.5; y += 0.5)
        {
            int const cx = c.x + x_inc;
            int const cy = c.y + y * x_inc;
            if (cx < subframe.GetRight() && cy < subframe.GetBottom() && cy > subframe.GetTop())
                incr_pixel(img, cx, cy, (int)d[2][2]);
        }

    }

}

static void render_star(usImage& img, int binning, const wxRect& subframe, const wxRealPoint& p, double inten)
{
    enum { WIDTH = 5 };
    double STAR[][WIDTH] = {{ 0.0,  0.8,   2.2,  0.8, 0.0, },
                            { 0.8, 16.6,  46.1, 16.6, 0.8, },
                            { 2.2, 46.1, 128.0, 46.1, 2.2, },
                            { 0.8, 16.6,  46.1, 16.6, 0.8, },
                            { 0.0,  0.8,   2.2,  0.8, 0.0, },
                           };

    wxRealPoint intpart;
    double fx = modf(p.x / (double) binning, &intpart.x);
    double fy = modf(p.y / (double) binning, &intpart.y);
    double f00 = (1.0 - fx) * (1.0 - fy);
    double f01 = (1.0 - fx) * fy;
    double f10 = fx * (1.0 - fy);
    double f11 = fx * fy;

    double d[WIDTH + 1][WIDTH + 1] = { { 0.0 } };
    for (unsigned int i = 0; i < WIDTH; i++)
        for (unsigned int j = 0; j < WIDTH; j++)
        {
            double s = STAR[i][j];
            if (s > 0.0)
            {
                s *= inten / 256.0;
                d[i][j] += f00 * s;
                d[i+1][j] += f10 * s;
                d[i][j+1] += f01 * s;
                d[i+1][j+1] += f11 * s;
            }
        }

    wxPoint c((int) intpart.x - (WIDTH - 1) / 2,
              (int) intpart.y - (WIDTH - 1) / 2);

    for (unsigned int i = 0; i < WIDTH + 1; i++)
    {
        int const cx = c.x + i;
        if (cx < subframe.GetLeft() || cx > subframe.GetRight())
            continue;
        for (unsigned int j = 0; j < WIDTH + 1; j++)
        {
            int const cy = c.y + j;
            if (cy < subframe.GetTop() || cy > subframe.GetBottom())
                continue;
            int incr = (int) d[i][j];
            if (incr > (unsigned short)-1)
                incr = (unsigned short)-1;
            incr_pixel(img, cx, cy, incr);
        }
    }
}

static void render_clouds(usImage& img, const wxRect& subframe, int exptime, int gain, int offset)
{
    unsigned short cloud_amt;
    unsigned short *p0 = &img.Pixel(subframe.GetLeft(), subframe.GetTop());
    for (int r = 0; r < subframe.GetHeight(); r++, p0 += img.Size.GetWidth())
    {
        unsigned short *const end = p0 + subframe.GetWidth();
        for (unsigned short *p = p0; p < end; p++)
        {
            // Compute a randomized brightness contribution from clouds, then overlay that on the guide frame
             cloud_amt = (unsigned short)(SimCamParams::clouds_inten * ((double)gain / 10.0 * offset * exptime / 100.0 + ((rand() % (gain * 100)) / 30.0)));
             *p = (unsigned short) (SimCamParams::clouds_opacity * cloud_amt + (1 - SimCamParams::clouds_opacity) * *p);
        }
    }
}

#ifdef SIM_FILE_DISPLACEMENTS
// Get raw star displacements from a file generated by using the CAPTURE_DEFLECTIONS
// compile-time option in guider.cpp to record them
void SimCamState::ReadDisplacements(double& incX, double& incY)
{
    wxStringTokenizer tok;

    // If we reach the EOF, just start over - we don't want to suddenly reverse direction on linear drifts, and the
    // underlying seeing behavior is sufficiently random that a simple replay is warranted
    if (pIStream->Eof())
        pIStream->SeekI(wxFileOffset(0));

    if (!pIStream->Eof())
    {
        wxString line = pText->ReadLine();
        line.Trim(false); // trim leading whitespace

        if (line.StartsWith("DeltaRA"))
        {
            // Get the image scale of the underlying raw data stream
            tok.SetString(line, ", =");
            wxString tk = tok.GetNextToken();
            while (tk != "Scale")
                tk = tok.GetNextToken();
            tk = tok.GetNextToken();            // numeric image scale a-s/p
            double realImageScale;
            if (tk.ToDouble(&realImageScale))
            {
                // Will use this to scale subsequent raw star displacements to match simulator image scale
                scaleConversion = realImageScale / SimCamParams::image_scale;
            }
            line = pText->ReadLine();
            line.Trim(false);
        }

        tok.SetString(line, ", ");
        wxString s1 = tok.GetNextToken();
        wxString s2 = tok.GetNextToken();
        double x, y;
        if (s1.ToDouble(&x) && s2.ToDouble(&y))
        {
            incX = x * scaleConversion;
            incY = y * scaleConversion;
        }
        else
        {
            Debug.AddLine(wxString::Format("Star_deflections file: bad input starting with %s", line));
        }
    }
}
#endif

void SimCamState::FillImage(usImage& img, const wxRect& subframe, int exptime, int gain, int offset)
{
    unsigned int const nr_stars = stars.size();

#ifdef SIMDEBUG
    static int CountUp (0);
    if (CountUp == 0)
    {
        // Changes in the setup dialog are hard to track - just make sure we are using the params we think we are
        Debug.AddLine (wxString::Format("SimDebug: img_scale: %.3f, seeing_scale: %.3f", SimCamParams::image_scale, SimCamParams::seeing_scale));
    }
    CountUp++;
#endif

    // start with original star positions
    wxVector<wxRealPoint> pos(nr_stars);
    for (unsigned int i = 0; i < nr_stars; i++)
        pos[i] = stars[i].pos;

    double total_shift_x = 0;
    double total_shift_y = 0;

#ifdef SIM_FILE_DISPLACEMENTS

    double inc_x;
    double inc_y;
    if (pText)
    {
        ReadDisplacements(inc_x, inc_y);
        total_shift_x = ra_ofs + inc_x;
        total_shift_y = dec_ofs.val() + inc_y;
        // If user has disabled guiding, let him see the raw behavior of the displacement data - the
        // ra_ofs and dec_ofs variables are normally updated in the ST-4 guide function
        if (!pMount->GetGuidingEnabled())
        {
            ra_ofs += inc_x;
            dec_ofs.incr(inc_y);
        }
    }

#else // SIM_FILE_DISPLACEMENTS

    long const cur_time = timer.Time();
    long const delta_time_ms = last_exposure_time - cur_time;
    last_exposure_time = cur_time;

    // simulate worm phase changing with RA slew
    double dec, st, ra = 0.;
    if (pPointingSource)
        pPointingSource->GetCoordinates(&ra, &dec, &st);

    static double s_prev_ra;
    double dra = norm(ra - s_prev_ra, -12.0, 12.0);
    s_prev_ra = ra;

    // convert RA hours to SI seconds
    const double SECONDS_PER_HOUR = 60. * 60.;
    const double SIDEREAL_SECONDS_PER_SEC = 0.9973;
    dra *= SECONDS_PER_HOUR / SIDEREAL_SECONDS_PER_SEC;
    static double s_ra_offset;
    s_ra_offset += dra;

    // an increase in RA means the worm moved backwards
    double const now = cur_time / 1000. - s_ra_offset;

    // Compute PE - canned PE terms create some "steep" sections of the curve
    static double const max_amp = 4.85;         // max amplitude of canned PE
    double pe = 0.;

    if (SimCamParams::use_pe)
    {
        if (SimCamParams::use_default_pe_params)
        {
            static double const period[] = { 230.5, 122.0, 49.4, 9.56, 76.84, };
            static double const amp[] =    {2.02, 0.69, 0.22, 0.137, 0.14};   // in a-s
            static double const phase[] =  { 0.0,     1.4, 98.8, 35.9, 150.4, };

            for (unsigned int i = 0; i < WXSIZEOF(period); i++)
                pe += amp[i] * cos((now - phase[i]) / period[i] * 2. * M_PI);

            pe *= (SimCamParams::pe_scale / (max_amp * SimCamParams::image_scale));      // modulated PE in px
        }
        else
        {
            pe = SimCamParams::custom_pe_amp * cos(now / SimCamParams::custom_pe_period * 2.0 * M_PI) / SimCamParams::image_scale;
        }
    }

    // simulate drift in DEC
    cum_dec_drift += (double) delta_time_ms * SimCamParams::dec_drift_rate / 1000.;

    // Compute total movements from all sources - ra_ofs and dec_ofs are cumulative sums of all guider movements relative to zero-point
    total_shift_x = pe + ra_ofs;
    total_shift_y = cum_dec_drift + dec_ofs.val();

    double seeing[2] = { 0.0 };

    // simulate seeing
    if (SimCamParams::seeing_scale > 0.0)
    {
        rand_normal(seeing);
        static const double seeing_adjustment = (2.345 * 1.4 * 2.4);        //FWHM, geometry, empirical
        double sigma = SimCamParams::seeing_scale / (seeing_adjustment * SimCamParams::image_scale);
        seeing[0] *= sigma;
        seeing[1] *= sigma;
        total_shift_x += seeing[0];
        total_shift_y += seeing[1];
    }

#endif // SIM_FILE_DISPLACEMENTS

    for (unsigned int i = 0; i < nr_stars; i++)
    {
        pos[i].x += total_shift_x;
        pos[i].y += total_shift_y;
    }

#ifdef SIMDEBUG
#ifdef SIM_FILE_DISPLACEMENTS
    DebugFile.Write(wxString::Format("%.3f, %.3f, %.3f, %.3f\n", total_shift_x, total_shift_y,
        ra_ofs, dec_ofs.val()));
#else
    DebugFile.Write(wxString::Format( "%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n",
        pe, drift, seeing[0], seeing[1], total_shift_x, total_shift_y,
        ra_ofs, dec_ofs.val()));
#endif
#endif

    // check for pier-flip
    if (pPointingSource)
    {
        PierSide new_side = pPointingSource->SideOfPier();
        if (new_side != SimCamParams::pier_side)
        {
            Debug.Write(wxString::Format("Cam simulator: pointing source pier side changed from %d to %d\n", SimCamParams::pier_side, new_side));
            SimCamParams::pier_side = new_side;
        }
    }

    // convert to camera coordinates
    wxVector<wxRealPoint> cc(nr_stars);
    double angle = radians(SimCamParams::cam_angle);

    if (SimCamParams::pier_side == PIER_SIDE_WEST)
        angle += M_PI;
    double const cos_t = cos(angle);
    double const sin_t = sin(angle);
    for (unsigned int i = 0; i < nr_stars; i++) {
        cc[i].x = pos[i].x * cos_t - pos[i].y * sin_t + width / 2.0;
        cc[i].y = pos[i].x * sin_t + pos[i].y * cos_t + height / 2.0;
    }

#ifdef STEPGUIDER_SIMULATOR
    // add-in AO offset
    if (s_sim_ao) {
        double const ao_angle = radians(SimAoParams::angle);
        double const cos_a = cos(ao_angle);
        double const sin_a = sin(ao_angle);
        double const ao_x = (double) s_sim_ao->CurrentPosition(RIGHT) * SimAoParams::scale;
        double const ao_y = (double) s_sim_ao->CurrentPosition(UP) * SimAoParams::scale;
        double const dx = ao_x * cos_a - ao_y * sin_a;
        double const dy = ao_x * sin_a + ao_y * cos_a;
        for (unsigned int i = 0; i < nr_stars; i++) {
            cc[i].x += dx;
            cc[i].y += dy;
        }
    }
#endif // STEPGUIDER_SIMULATOR

    // render each star
    if (!pCamera->ShutterClosed)
    {
        for (unsigned int i = 0; i < nr_stars; i++)
        {
            double star = stars[i].inten * exptime * gain;
            double dark = (double) gain / 10.0 * offset * exptime / 100.0;
            double noise = (double)(rand() % (gain * 100));
            double inten = star + dark + noise;

            render_star(img, pCamera->Binning, subframe, cc[i], inten);
        }

#ifndef SIM_FILE_DISPLACEMENTS
        if (SimCamParams::show_comet)
        {
            double x = total_shift_x + now * SimCamParams::comet_rate_x / 3600.;
            double y = total_shift_y + now * SimCamParams::comet_rate_y / 3600.;
            double cx = x * cos_t - y * sin_t + width / 2.0;
            double cy = x * sin_t + y * cos_t + height / 2.0;

            double inten = 3.0;
            double star = inten * exptime * gain;
            double dark = (double) gain / 10.0 * offset * exptime / 100.0;
            double noise = (double)(rand() % (gain * 100));
            inten = star + dark + noise;

            render_comet(img, pCamera->Binning, subframe, wxRealPoint(cx, cy), inten);
        }
#endif
    }

    if (SimCamParams::clouds_opacity > 0)
        render_clouds(img, subframe, exptime, gain, offset);

    // render hot pixels
    for (unsigned int i = 0; i < hotpx.size(); i++)
    {
        wxPoint p(hotpx[i]);
        p.x /= pCamera->Binning;
        p.y /= pCamera->Binning;
        if (subframe.Contains(p))
            set_pixel(img, p.x, p.y, (unsigned short)-1);
    }
}

class CameraSimulator : public GuideCamera
{
    SimCamState sim;
public:
    CameraSimulator();
    ~CameraSimulator();
    bool     Capture(int duration, usImage& img, int options, const wxRect& subframe) override;
    bool     Connect(const wxString& camId) override;
    bool     Disconnect() override;
    void     ShowPropertyDialog() override;
    bool     HasNonGuiCapture() override { return true; }
    wxByte   BitsPerPixel() override;
    bool     SetCoolerOn(bool on) override;
    bool     SetCoolerSetpoint(double temperature) override;
    bool     GetCoolerStatus(bool *on, double *setpoint, double *power, double *temperature) override;
    bool     GetSensorTemperature(double *temperature) override;
    bool     ST4HasNonGuiMove() override { return true; }
    bool     ST4SynchronousOnly() override;
    bool     ST4PulseGuideScope(int direction, int duration) override;
    PierSide SideOfPier() const;
    void     FlipPierSide();
};

CameraSimulator::CameraSimulator()
{
    Connected = false;
    Name = _T("Simulator");
    m_hasGuideOutput = true;
    HasShutter = true;
    HasGainControl = true;
    HasSubframes = true;
    PropertyDialogType = PROPDLG_WHEN_CONNECTED;
    MaxBinning = 3;
    HasCooler = true;
}

wxByte CameraSimulator::BitsPerPixel()
{
#if 1
    return 16;
#else
    return 8;
#endif
}

bool CameraSimulator::Connect(const wxString& camId)
{
    load_sim_params();
    sim.Initialize();

    struct ConnectInBg : public ConnectCameraInBg
    {
        CameraSimulator *cam;
        ConnectInBg(CameraSimulator *cam_) : cam(cam_) { }
        bool Entry()
        {
//#define TEST_SLOW_CONNECT
#ifdef TEST_SLOW_CONNECT
            for (int i = 0; i < 100; i++)
            {
                wxMilliSleep(100);
                if (IsCanceled())
                    return true;
            }
#endif
            return false;
        }
    };

    bool err = ConnectInBg(this).Run();
    if (!err)
        Connected = true;

    return err;
}

bool CameraSimulator::Disconnect()
{
    Connected = false;
    return false;
}

CameraSimulator::~CameraSimulator()
{
#ifdef SIMDEBUG
    sim.DebugFile.Close();
#endif
#ifdef SIM_FILE_DISPLACEMENTS
    if (sim.pText)
        delete sim.pText;
    if (sim.pIStream)
        delete sim.pIStream;
#endif
}

#if SIMMODE==2
bool CameraSimulator::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    int xsize, ysize;
    wxImage disk_image;
    unsigned short *dataptr;
    unsigned char *imgptr;

    bool retval = disk_image.LoadFile("/Users/stark/dev/PHD/simimage.bmp");
    if (!retval) {
        pFrame->Alert(_("Cannot load simulated image"));
        return true;
    }
    xsize = disk_image.GetWidth();
    ysize = disk_image.GetHeight();
    if (img.Init(xsize,ysize)) {
        pFrame->Alert(_("Memory allocation error"));
        return true;
    }

    dataptr = img.ImageData;
    imgptr = disk_image.GetData();
    for (unsigned int i = 0; i < img.NPixels; i++, dataptr++, imgptr++) {
        *dataptr = (unsigned short) *imgptr;
        imgptr++; imgptr++;
    }
    QuickLRecon(img);
    return false;

}
#endif

#if SIMMODE == 3
static void fill_noise(usImage& img, const wxRect& subframe, int exptime, int gain, int offset)
{
    unsigned short *p0 = &img.Pixel(subframe.GetLeft(), subframe.GetTop());
    for (int r = 0; r < subframe.GetHeight(); r++, p0 += img.Size.GetWidth())
    {
        unsigned short *const end = p0 + subframe.GetWidth();
        for (unsigned short *p = p0; p < end; p++)
            *p = (unsigned short) (SimCamParams::noise_multiplier * ((double) gain / 10.0 * offset * exptime / 100.0 + (rand() % (gain * 100))));
    }
}
#endif // SIMMODE == 3

bool CameraSimulator::Capture(int duration, usImage& img, int options, const wxRect& subframeArg)
{
    wxRect subframe(subframeArg);
    CameraWatchdog watchdog(duration, GetTimeoutMs());

    // sleep before rendering the image so that any changes made in the middle of a long exposure (e.g. manual guide pulse) shows up in the image

    if (duration > 5)
    {
        if (WorkerThread::MilliSleep(duration - 5, WorkerThread::INT_ANY))
            return true;
        if (watchdog.Expired())
        {
            DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
            return true;
        }
    }

#if SIMMODE == 1

    if (!UseSubframes)
        subframe = wxRect();

    if (sim.ReadNextImage(img, subframe))
        return true;

    FullSize = img.Size;

#else

    int width = sim.width / Binning;
    int height = sim.height / Binning;
    FullSize = wxSize(width, height);

    bool usingSubframe = UseSubframes;
    if (subframe.width <= 0 || subframe.height <= 0 || subframe.GetRight() >= width || subframe.GetBottom() >= height)
        usingSubframe = false;
    if (!usingSubframe)
        subframe = wxRect(0, 0, FullSize.GetWidth(), FullSize.GetHeight());

    int const exptime = duration;
    int const gain = 30;
    int const offset = 100;

    if (img.Init(FullSize))
    {
        pFrame->Alert(_("Memory allocation error"));
        return true;
    }

    if (usingSubframe)
        img.Clear();

    fill_noise(img, subframe, exptime, gain, offset);

    sim.FillImage(img, subframe, exptime, gain, offset);

    if (usingSubframe)
        img.Subframe = subframe;

    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);

#endif // SIMMODE == 1

    unsigned int tot_dur = duration + SimCamParams::frame_download_ms;
    long elapsed = watchdog.Time();
    if (elapsed < tot_dur)
    {
        if (WorkerThread::MilliSleep(tot_dur - elapsed, WorkerThread::INT_ANY))
            return true;
        if (watchdog.Expired())
        {
            DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
            return true;
        }
    }

    return false;
}



bool CameraSimulator::ST4PulseGuideScope(int direction, int duration)
{
    // Following must take into account how the render_star function works.  Render_star uses camera binning explicitly, so
    // relying only on image scale in computing d creates distances that are too small by a factor of <binning>
    double d = SimCamParams::guide_rate * Binning * duration / (1000.0 * SimCamParams::image_scale);

    // simulate RA motion scaling according to declination
    if (direction == WEST || direction == EAST)
    {
        double dec = pPointingSource->GetDeclinationRadians();
        if (dec == UNKNOWN_DECLINATION)
            dec = radians(25.0); // some arbitrary declination
        d *= cos(dec);
    }

    // simulate stiction if option selected
    if (SimCamParams::use_stiction && (direction == NORTH || direction == SOUTH))
        d += sim.stictionSim.GetAdjustment(direction, duration, d);

    if (SimCamParams::pier_side == PIER_SIDE_WEST && SimCamParams::reverse_dec_pulse_on_west_side)
    {
        // after pier flip, North/South have opposite affect on declination
        switch (direction) {
        case NORTH: direction = SOUTH; break;
        case SOUTH: direction = NORTH; break;
        }
    }

    switch (direction) {
    case WEST:    sim.ra_ofs += d;      break;
    case EAST:    sim.ra_ofs -= d;      break;
    case NORTH:   sim.dec_ofs.incr(d);  break;
    case SOUTH:   sim.dec_ofs.incr(-d); break;
    default: return true;
    }
    WorkerThread::MilliSleep(duration, WorkerThread::INT_ANY);
    return false;
}

bool CameraSimulator::SetCoolerOn(bool on)
{
    if (on)
        sim.cooler.TurnOn();
    else
        sim.cooler.TurnOff();

    return false; // no error
}

bool CameraSimulator::SetCoolerSetpoint(double temperature)
{
    if (!sim.cooler.on)
        return true; // error

    sim.cooler.SetTemp(temperature);
    return false;
}

bool CameraSimulator::GetCoolerStatus(bool *onp, double *setpoint, double *power, double *temperature)
{
    bool on = sim.cooler.on;
    double cur = sim.cooler.CurrentTemp();

    *onp = on;

    if (on)
    {
        *setpoint = sim.cooler.setTemp;
        *power = cur < MIN_COOLER_TEMP ? 100. : cur >= AMBIENT_TEMP ? 0. : (AMBIENT_TEMP - cur) * 100. / (AMBIENT_TEMP - MIN_COOLER_TEMP);
        *temperature = cur;
    }
    else
    {
        *temperature = cur;
    }

    return false;
}

bool CameraSimulator::GetSensorTemperature(double *temperature)
{
    bool on;
    double setpt, powr;
    return GetCoolerStatus(&on, &setpt, &powr, temperature);
}

PierSide CameraSimulator::SideOfPier() const
{
    return SimCamParams::pier_side;
}

bool CameraSimulator::ST4SynchronousOnly()
{
    return !SimCamParams::allow_async_st4;
}

static PierSide OtherSide(PierSide side)
{
    return side == PIER_SIDE_EAST ? PIER_SIDE_WEST : PIER_SIDE_EAST;
}

void CameraSimulator::FlipPierSide()
{
    SimCamParams::pier_side = OtherSide(SimCamParams::pier_side);
    Debug.Write(wxString::Format("CamSimulator FlipPierSide: side = %d  cam_angle = %.1f\n", SimCamParams::pier_side, SimCamParams::cam_angle));
}

#if SIMMODE == 4
bool CameraSimulator::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    int xsize, ysize;
    //  unsigned short *dataptr;
    //  int i;
    fitsfile *fptr;  // FITS file pointer
    int status = 0;  // CFITSIO status value MUST be initialized to zero!
    int hdutype, naxis;
    int nhdus=0;
    long fits_size[2];
    long fpixel[3] = {1,1,1};
    //  char keyname[15];
    //  char keystrval[80];
    static int frame = 0;
    static int step = 1;
    char fname[256];
    snprintf(fname, sizeof(fname), "/Users/stark/dev/PHD/simimg/DriftSim_%d.fit", frame);
    if (!PHD_fits_open_diskfile(&fptr, fname, READONLY, &status))
    {
        if (fits_get_hdu_type(fptr, &hdutype, &status) || hdutype != IMAGE_HDU) {
            pFrame->Alert(_("FITS file is not of an image"));
            PHD_fits_close_file(fptr);
            return true;
        }

        // Get HDUs and size
        fits_get_img_dim(fptr, &naxis, &status);
        fits_get_img_size(fptr, 2, fits_size, &status);
        xsize = (int) fits_size[0];
        ysize = (int) fits_size[1];
        fits_get_num_hdus(fptr,&nhdus,&status);
        if ((nhdus != 1) || (naxis != 2)) {
            pFrame->Alert(wxString::Format(_("Unsupported type or read error loading FITS file %d %d"),nhdus,naxis));
            PHD_fits_close_file(fptr);
            return true;
        }
        if (img.Init(xsize,ysize)) {
            pFrame->Alert(_("Memory allocation error"));
            PHD_fits_close_file(fptr);
            return true;
        }
        if (fits_read_pix(fptr, TUSHORT, fpixel, xsize*ysize, nullptr, img.ImageData, nullptr, &status) ) { // Read image
            pFrame->Alert(_("Error reading data"));
            PHD_fits_close_file(fptr);
            return true;
        }
        PHD_fits_close_file(fptr);
        frame = frame + step;
        if (frame > 440) {
            step = -1;
            frame = 439;
        }
        else if (frame < 0) {
            step = 1;
            frame = 1;
        }

    }
    return false;

}
#endif // SIMMODE == 4

struct SimCamDialog : public wxDialog
{
    wxSlider *pStarsSlider;
    wxSlider *pHotpxSlider;
    wxSlider *pNoiseSlider;
    wxSlider *pCloudSlider;
    wxSpinCtrlDouble *pBacklashSpin;
    wxSpinCtrlDouble *pDriftSpin;
    wxSpinCtrlDouble *pGuideRateSpin;
    wxSpinCtrlDouble *pCameraAngleSpin;
    wxSpinCtrlDouble *pSeeingSpin;
    wxCheckBox* showComet;
    wxCheckBox *pUsePECbx;
    wxCheckBox *pUseStiction;
    wxCheckBox *pReverseDecPulseCbx;
    PierSide pPierSide;
    wxStaticText *pPiersideLabel;
    wxRadioButton *pPEDefaultRb;
    wxSpinCtrlDouble *pPEDefScale;
    wxRadioButton *pPECustomRb;
    wxTextCtrl *pPECustomAmp;
    wxTextCtrl *pPECustomPeriod;
    wxButton *pPierFlip;
    wxButton *pResetBtn;

    SimCamDialog(wxWindow *parent);
    ~SimCamDialog() { }
    void OnReset(wxCommandEvent& event);
    void OnPierFlip(wxCommandEvent& event);
    void UpdatePierSideLabel();
    void OnRbDefaultPE(wxCommandEvent& evt);
    void OnRbCustomPE(wxCommandEvent& evt);
    void OnOkClick(wxCommandEvent& evt);

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(SimCamDialog, wxDialog)
    EVT_BUTTON(wxID_RESET, SimCamDialog::OnReset)
    EVT_BUTTON(wxID_CONVERT, SimCamDialog::OnPierFlip)
END_EVENT_TABLE()

// Utility functions for adding controls with specified properties
static wxSlider *NewSlider(wxWindow *parent, int val, int minval, int maxval, const wxString& tooltip)
{
    wxSlider *pNewCtrl = new wxSlider(parent, wxID_ANY, val, minval, maxval, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_VALUE_LABEL);
    pNewCtrl->SetToolTip(tooltip);
    return pNewCtrl;
}

static wxSpinCtrlDouble *NewSpinner(wxWindow *parent, double val, double minval, double maxval, double inc,
    const wxString& tooltip)
{
    wxSize sz = pFrame->GetTextExtent(wxString::Format("%.2f", maxval * 10.));
    wxSpinCtrlDouble *pNewCtrl = pFrame->MakeSpinCtrlDouble(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
        sz, wxSP_ARROW_KEYS, minval, maxval, val, inc);
    pNewCtrl->SetDigits(2);
    pNewCtrl->SetToolTip(tooltip);
    return pNewCtrl;
}

static wxCheckBox *NewCheckBox(wxWindow *parent, bool val, const wxString& label, const wxString& tooltip)
{
    wxCheckBox *pNewCtrl = new wxCheckBox(parent, wxID_ANY, label);
    pNewCtrl->SetValue(val);
    pNewCtrl->SetToolTip(tooltip);
    return pNewCtrl;
}

// Utility function to add the <label, input> pairs to a grid including tool-tips
static void AddTableEntryPair(wxWindow *parent, wxFlexGridSizer *pTable, const wxString& label, wxWindow *pControl)
{
    wxStaticText *pLabel = new wxStaticText(parent, wxID_ANY, label + _(": "), wxPoint(-1,-1), wxSize(-1,-1));
    pTable->Add(pLabel, 1, wxALL, 5);
    pTable->Add(pControl, 1, wxALL, 5);
}

static wxTextCtrl *AddCustomPEField(wxWindow *parent, wxFlexGridSizer *pTable, const wxString& label, const wxString& tip, double val)
{
    int width;
    int height;

    parent->GetTextExtent(_T("999.9"), &width, &height);
    wxTextCtrl *pCtrl = new wxTextCtrl(parent, wxID_ANY, _T("    "), wxDefaultPosition, wxSize(width+30, -1));
    pCtrl->SetValue(wxString::Format("%.1f", val));
    pCtrl->SetToolTip(tip);
    AddTableEntryPair(parent, pTable, label, pCtrl);
    return pCtrl;
}

static void SetRBState(SimCamDialog *dlg, bool using_defaults)
{
    dlg->pPEDefScale->Enable(using_defaults);
    dlg->pPECustomAmp->Enable(!using_defaults);
    dlg->pPECustomPeriod->Enable(!using_defaults);
}

static void SetControlStates(SimCamDialog *dlg, bool captureActive)
{
    bool enable = !captureActive;

    dlg->pBacklashSpin->Enable(enable);
    dlg->pGuideRateSpin->Enable(enable);
    dlg->pCameraAngleSpin->Enable(enable);
    dlg->pPEDefaultRb->Enable(enable);
    dlg->pPEDefScale->Enable(enable);
    dlg->pPECustomAmp->Enable(enable);
    dlg->pPECustomPeriod->Enable(enable);
    dlg->pPECustomRb->Enable(enable);
    dlg->pUsePECbx->Enable(enable);
    dlg->pUseStiction->Show(false);                          // no good for end-users
    dlg->pPierFlip->Enable(enable);
    dlg->pReverseDecPulseCbx->Enable(enable);
    dlg->pResetBtn->Enable(enable);
}

// Event handlers
void SimCamDialog::OnRbDefaultPE(wxCommandEvent& evt)
{
    SetRBState(this, true);
}

void SimCamDialog::OnRbCustomPE(wxCommandEvent& evt)
{
    SetRBState(this, false);
}

// Need to enforce semantics on free-form user input
void SimCamDialog::OnOkClick(wxCommandEvent& evt)
{
    bool bOk = true;

    if (pPECustomRb->GetValue())
    {
        wxString sAmp = pPECustomAmp->GetValue();
        wxString sPeriod = pPECustomPeriod->GetValue();
        double amp;
        double period;
        if (sAmp.ToDouble(&amp) && sPeriod.ToDouble(&period))
        {
            if (amp <= 0.0 || period <= 0.0)
            {
                wxMessageBox(_("PE amplitude and period must be > 0"), "Error", wxOK | wxICON_ERROR);
                bOk = false;
            }
        }
        else
        {
            wxMessageBox(_("PE amplitude and period must be numbers > 0"), "Error", wxOK | wxICON_ERROR);
            bOk = false;
        }
    }

    if (bOk)
        wxDialog::EndModal(wxID_OK);
}

SimCamDialog::SimCamDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _("Camera Simulator"))
{
    wxBoxSizer *pVSizer = new wxBoxSizer(wxVERTICAL);
    double imageScale = pFrame->GetCameraPixelScale();

    SimCamParams::image_scale = imageScale;

    // Camera group controls
    wxStaticBoxSizer *pCamGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Camera"));
    wxFlexGridSizer *pCamTable = new wxFlexGridSizer(1, 6, 15, 15);
    pStarsSlider = NewSlider(this, SimCamParams::nr_stars, 1, 100, _("Number of simulated stars"));
    AddTableEntryPair(this, pCamTable, _("Stars"), pStarsSlider);
    pHotpxSlider = NewSlider(this, SimCamParams::nr_hot_pixels, 0, 50, _("Number of hot pixels"));
    AddTableEntryPair(this, pCamTable, _("Hot pixels"), pHotpxSlider);
    pNoiseSlider = NewSlider(this, (int)floor(SimCamParams::noise_multiplier * 100 / NOISE_MAX), 0, 100,
        /* xgettext:no-c-format */ _("% Simulated noise"));
    AddTableEntryPair(this, pCamTable, _("Noise"), pNoiseSlider);
    pCamGroup->Add(pCamTable);

    // Mount group controls
    wxStaticBoxSizer *pMountGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Mount"));
    wxFlexGridSizer *pMountTable = new wxFlexGridSizer(2, 6, 5, 15);
    pBacklashSpin = NewSpinner(this, SimCamParams::dec_backlash * imageScale, 0, DEC_BACKLASH_MAX, 0.1, _("Dec backlash, arc-secs"));
    AddTableEntryPair(this, pMountTable, _("Dec backlash"), pBacklashSpin);
    pDriftSpin = NewSpinner(this, SimCamParams::dec_drift_rate * 60.0 * imageScale, -DEC_DRIFT_MAX, DEC_DRIFT_MAX, 0.5, _("Dec drift, arc-sec/min"));
    AddTableEntryPair(this, pMountTable, _("Dec drift"), pDriftSpin);
    pGuideRateSpin = NewSpinner(this, SimCamParams::guide_rate / 15.0, 0.25, GUIDE_RATE_MAX, 0.25, _("Guide rate, x sidereal"));
    AddTableEntryPair(this, pMountTable, _("Guide rate"), pGuideRateSpin);
    pUseStiction = NewCheckBox(this, SimCamParams::use_stiction, _("Apply stiction"), _("Simulate dec axis stiction"));
    pUseStiction->Enable(false);                            // too crude to put in hands of users
    pMountTable->Add(pUseStiction, 1, wxBOTTOM, 15);
    pMountGroup->Add(pMountTable);

    // Add embedded group for PE info (still within mount group)
    wxStaticBoxSizer *pPEGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("PE"));
    pUsePECbx = NewCheckBox(this, SimCamParams::use_pe, _("Apply PE"), _("Simulate periodic error"));
    wxBoxSizer *pPEHorSizer = new wxBoxSizer(wxHORIZONTAL);
    // Default PE parameters
    wxFlexGridSizer *pPEDefaults = new wxFlexGridSizer(1, 3, 10, 10);
    pPEDefaultRb = new wxRadioButton(this, wxID_ANY, _("Default curve"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    pPEDefaultRb->SetValue(SimCamParams::use_default_pe_params);
    pPEDefaultRb->SetToolTip(_("Use a built-in PE curve that has some steep and smooth sections."));
    pPEDefaultRb->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &SimCamDialog::OnRbDefaultPE, this);                // Event handler binding
    wxStaticText *pSliderLabel = new wxStaticText(this, wxID_ANY, _("Amplitude: "),wxPoint(-1,-1),wxSize(-1,-1));
    pPEDefScale = NewSpinner(this, SimCamParams::pe_scale, 0, PE_SCALE_MAX, 0.5, _("PE Amplitude, arc-secs"));

    int hor_spacing = StringWidth(this, "9");
    pPEDefaults->Add(pPEDefaultRb);
    pPEDefaults->Add(pSliderLabel, wxSizerFlags().Border(wxLEFT, hor_spacing));
    pPEDefaults->Add(pPEDefScale, wxSizerFlags().Border(wxLEFT, hor_spacing + 1));
    // Custom PE parameters
    wxFlexGridSizer *pPECustom = new wxFlexGridSizer(1, 5, 10, 10);
    pPECustomRb = new wxRadioButton(this, wxID_ANY, _("Custom curve"), wxDefaultPosition, wxDefaultSize);
    pPECustomRb->SetValue(!SimCamParams::use_default_pe_params);
    pPECustomRb->SetToolTip(_("Use a simple sinusoidal curve. You can specify the amplitude and period."));
    pPECustomRb->Bind(wxEVT_COMMAND_RADIOBUTTON_SELECTED, &SimCamDialog::OnRbCustomPE, this);              // Event handler binding
    pPECustom->Add(pPECustomRb, wxSizerFlags().Border(wxTOP, 4));
    pPECustomAmp = AddCustomPEField(this, pPECustom, _("Amplitude"), _("Amplitude, arc-secs"), SimCamParams::custom_pe_amp);
    pPECustomPeriod = AddCustomPEField(this, pPECustom, _("Period"), _("Period, seconds"), SimCamParams::custom_pe_period);
    // VSizer for default and custom controls
    wxBoxSizer *pPEVSizer = new wxBoxSizer(wxVERTICAL);
    pPEVSizer->Add(pPEDefaults, wxSizerFlags().Border(wxLEFT, 60));
    pPEVSizer->Add(pPECustom, wxSizerFlags().Border(wxLEFT, 60));
    // Finish off the whole PE group
    pPEHorSizer->Add(pUsePECbx);
    pPEHorSizer->Add(pPEVSizer);
    pPEGroup->Add(pPEHorSizer);

    // Now add some miscellaneous mount-related stuff (still within mount group)
    wxBoxSizer *pMiscSizer = new wxBoxSizer(wxHORIZONTAL);
    pReverseDecPulseCbx = NewCheckBox(this, SimCamParams::reverse_dec_pulse_on_west_side, _("Reverse Dec pulse on West side of pier"),
        _("Simulate a mount that reverses guide pulse direction after a meridian flip, like an ASCOM pulse-guided mount."));
    pPierSide = SimCamParams::pier_side;
    pPiersideLabel = new wxStaticText(this, wxID_ANY, _("Side of Pier: MMMMM"));
    pMiscSizer->Add(pReverseDecPulseCbx, wxSizerFlags().Border(10).Expand());
    pPierFlip = new wxButton(this, wxID_CONVERT, _("Pier Flip"));
    pMiscSizer->Add(pPierFlip, wxSizerFlags().Border(wxLEFT, 30).Expand());
    pMiscSizer->Add(pPiersideLabel , wxSizerFlags().Border(wxLEFT, 30).Expand());
    pMountGroup->Add(pPEGroup, wxSizerFlags().Center().Border(10).Expand());
    pMountGroup->Add(pMiscSizer, wxSizerFlags().Border(wxTOP, 10).Expand());

    // Session group controls
    wxStaticBoxSizer *pSessionGroup = new wxStaticBoxSizer(wxVERTICAL, this, _("Session"));
    wxFlexGridSizer *pSessionTable = new wxFlexGridSizer(1, 6, 15, 15);
    pCameraAngleSpin = NewSpinner(this, SimCamParams::cam_angle, 0, CAM_ANGLE_MAX, 10, _("Camera angle, degrees"));
    AddTableEntryPair(this, pSessionTable, _("Camera angle"), pCameraAngleSpin);
    pSeeingSpin = NewSpinner(this, SimCamParams::seeing_scale, 0, SEEING_MAX, 0.5, _("Seeing, FWHM arc-sec"));
    AddTableEntryPair(this, pSessionTable, _("Seeing"), pSeeingSpin);
    pCloudSlider = NewSlider(this, (int)(100 * SimCamParams::clouds_opacity), 0, 100, _("% cloud opacity"));
    AddTableEntryPair(this, pSessionTable, _("Cloud %"), pCloudSlider);
    showComet = new wxCheckBox(this, wxID_ANY, _("Comet"));
    showComet->SetValue(SimCamParams::show_comet);
    pSessionGroup->Add(pSessionTable);
    pSessionGroup->Add(showComet);

    pVSizer->Add(pCamGroup, wxSizerFlags().Border(wxALL, 10).Expand());
    pVSizer->Add(pMountGroup, wxSizerFlags().Border(wxRIGHT | wxLEFT, 10));
    pVSizer->Add(pSessionGroup, wxSizerFlags().Border(wxRIGHT | wxLEFT, 10).Expand());

    // Now deal with the buttons
    wxBoxSizer *pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pResetBtn = new wxButton(this, wxID_RESET, _("Reset"));
    pResetBtn->SetToolTip(_("Reset all values to application defaults"));
    pButtonSizer->Add(
        pResetBtn,
        wxSizerFlags(0).Align(0).Border(wxALL, 10));
    // Need to handle the OK event ourselves to validate text input fields
    wxButton *pBtn = new wxButton(this, wxID_OK, _("OK"));
    pBtn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &SimCamDialog::OnOkClick, this);
    pButtonSizer->Add(
        pBtn,
        wxSizerFlags(0).Align(0).Border(wxALL, 10));
    pButtonSizer->Add(
        new wxButton( this, wxID_CANCEL, _("Cancel") ),
        wxSizerFlags(0).Align(0).Border(wxALL, 10));

    // position the buttons centered with no border
    pVSizer->Add(
        pButtonSizer,
        wxSizerFlags(0).Center() );

    SetSizerAndFit(pVSizer);
    SetControlStates(this, pFrame->CaptureActive);
    if (!pFrame->CaptureActive)
        SetRBState(this, pPEDefaultRb->GetValue());        // Enable matching PE-related controls
    UpdatePierSideLabel();
}

void SimCamDialog::OnReset(wxCommandEvent& event)
{
    pStarsSlider->SetValue(NR_STARS_DEFAULT);
    pHotpxSlider->SetValue(NR_HOT_PIXELS_DEFAULT);
    pNoiseSlider->SetValue((int)floor(NOISE_DEFAULT * 100.0 / NOISE_MAX));
    pBacklashSpin->SetValue(DEC_BACKLASH_DEFAULT);
    pCloudSlider->SetValue(0);

    pDriftSpin->SetValue(DEC_DRIFT_DEFAULT);
    pSeeingSpin->SetValue(SEEING_DEFAULT);
    pCameraAngleSpin->SetValue(CAM_ANGLE_DEFAULT);
    pGuideRateSpin->SetValue(GUIDE_RATE_DEFAULT / GUIDE_RATE_MAX);
    pReverseDecPulseCbx->SetValue(REVERSE_DEC_PULSE_ON_WEST_SIDE_DEFAULT);
    pUsePECbx->SetValue(USE_PE_DEFAULT);
    pUseStiction->SetValue(USE_STICTION_DEFAULT);
    pPEDefaultRb->SetValue(USE_PE_DEFAULT_PARAMS);
    pPECustomRb->SetValue(!USE_PE_DEFAULT_PARAMS);
    pPEDefScale->SetValue(PE_SCALE_DEFAULT);
    pPECustomAmp->SetValue(wxString::Format("%0.1f",PE_CUSTOM_AMP_DEFAULT));
    pPECustomPeriod->SetValue(wxString::Format("%0.1f", PE_CUSTOM_PERIOD_DEFAULT));
    pPierSide = PIER_SIDE_DEFAULT;
    SetRBState( this, USE_PE_DEFAULT_PARAMS);
    UpdatePierSideLabel();
    showComet->SetValue(SHOW_COMET_DEFAULT);
}

void SimCamDialog::OnPierFlip(wxCommandEvent& event)
{
    int angle = pCameraAngleSpin->GetValue();
    angle += 180;
    if (angle >= 360)
        angle -= 360;
    pCameraAngleSpin->SetValue(angle);
    pPierSide= OtherSide(pPierSide);
    UpdatePierSideLabel();
}

void SimCamDialog::UpdatePierSideLabel()
{
    pPiersideLabel->SetLabel(wxString::Format(_("Side of pier: %s"), pPierSide == PIER_SIDE_EAST ? _("East") : _("West")));
}

struct UpdateChecker
{
    bool updated;
    UpdateChecker() : updated(false) { }
    template<typename T, typename U>
    void Update(T& val, const U& newval) {
        if (val != newval)
        {
            val = newval;
            updated = true;
        }
    }
    bool WasModified() const { return updated; }
};

void CameraSimulator::ShowPropertyDialog()
{
    SimCamDialog dlg(pFrame);
    double imageScale = pFrame->GetCameraPixelScale();              // arc-sec/pixel, defaults to 1.0 if no user specs
    SimCamParams::image_scale = imageScale;                         // keep current - might have gotten changed in brain dialog
    if (dlg.ShowModal() == wxID_OK)
    {
        UpdateChecker upd; // keep track of whether any values changed
        upd.Update(SimCamParams::nr_stars, dlg.pStarsSlider->GetValue());
        upd.Update(SimCamParams::nr_hot_pixels, dlg.pHotpxSlider->GetValue());
        SimCamParams::noise_multiplier = (double) dlg.pNoiseSlider->GetValue() * NOISE_MAX / 100.0;
        upd.Update(SimCamParams::dec_backlash, dlg.pBacklashSpin->GetValue() / imageScale);    // a-s -> px

        bool use_pe = dlg.pUsePECbx->GetValue();
        SimCamParams::use_pe = use_pe;
        SimCamParams::use_stiction = dlg.pUseStiction->GetValue();
        bool use_default_pe_params = dlg.pPEDefaultRb->GetValue();
        SimCamParams::use_default_pe_params = use_default_pe_params;
        if (SimCamParams::use_default_pe_params)
        {
            SimCamParams::pe_scale = dlg.pPEDefScale->GetValue();
        }
        else
        {
            dlg.pPECustomAmp->GetValue().ToDouble(&SimCamParams::custom_pe_amp);
            dlg.pPECustomPeriod->GetValue().ToDouble(&SimCamParams::custom_pe_period);
        }
        SimCamParams::dec_drift_rate =   dlg.pDriftSpin->GetValue() / (imageScale * 60.0);  // a-s per min to px per second
        SimCamParams::seeing_scale =     dlg.pSeeingSpin->GetValue();                      // already in a-s
        upd.Update(SimCamParams::cam_angle, dlg.pCameraAngleSpin->GetValue());
        SimCamParams::guide_rate =       dlg.pGuideRateSpin->GetValue() * 15.0;
        SimCamParams::pier_side = dlg.pPierSide;
        SimCamParams::reverse_dec_pulse_on_west_side = dlg.pReverseDecPulseCbx->GetValue();
        SimCamParams::show_comet = dlg.showComet->GetValue();
        SimCamParams::clouds_opacity = dlg.pCloudSlider->GetValue() / 100.0;
        save_sim_params();

        if (upd.WasModified())
            sim.Initialize();
    }
}

GuideCamera *GearSimulator::MakeCamSimulator()
{
    return new CameraSimulator();
}

void GearSimulator::FlipPierSide(GuideCamera *camera)
{
    if (camera && camera->Name == _T("Simulator"))
    {
        CameraSimulator *simcam = static_cast<CameraSimulator *>(camera);
        simcam->FlipPierSide();
    }
}

StepGuider *GearSimulator::MakeAOSimulator()
{
    return new StepGuiderSimulator();
}

Rotator *GearSimulator::MakeRotatorSimulator()
{
    return new RotatorSimulator();
}

#endif // SIMULATOR
