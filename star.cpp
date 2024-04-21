/*
 *  star.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Refactored by Bret McKee
 *  Copyright (c) 2006-2010 Craig Stark.
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
 *    Neither the name of Bret McKee, Dad Dog Development,
 *     Craig Stark, Stark Labs nor the names of its
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
#include <algorithm>

Star::Star()
{
    Invalidate();
    // Star is a bit quirky in that we use X and Y after the star is Invalidate()ed.
    X = Y = 0.0;
}

bool Star::WasFound(FindResult result)
{
    return result == STAR_OK || result == STAR_SATURATED;
}

bool Star::WasFound() const
{
    return IsValid() && WasFound(m_lastFindResult);
}

void Star::Invalidate()
{
    Mass = 0.0;
    SNR = 0.0;
    HFD = 0.0;
    m_lastFindResult = STAR_ERROR;
    PHD_Point::Invalidate();
}

void Star::SetError(FindResult error)
{
    m_lastFindResult = error;
}

// helper struct for HFR calculation
struct R2M
{
    double r2;
    wxPoint p;
    double m;
    R2M() { }
    R2M(int x, int y, double m_) : p(x, y), m(m_) { }
    bool operator<(const R2M& rhs) const { return r2 < rhs.r2; }
};

static double hfr(std::vector<R2M>& vec, double cx, double cy, double mass)
{
    if (vec.size() == 1) // hot pixel?
        return 0.25;

    // compute Half Flux Radius (HFR)
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        double dx = (double) it->p.x - cx;
        double dy = (double) it->p.y - cy;
        it->r2 = dx * dx + dy * dy;
    }
    std::sort(vec.begin(), vec.end()); // sort by ascending radius^2

    // find radius of half-mass
    double r20, r21, m0, m1;
    r20 = r21 = m0 = m1 = 0.0;
    double halfm = 0.5 * mass;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        const R2M& rm = *it;
        r20 = r21;
        m0 = m1;
        r21 = rm.r2;
        m1 += rm.m;
        if (m1 > halfm)
            break;
    }

    // interpolate
    double hfr;
    if (m1 > m0)
    {
        double r0 = sqrt(r20), r1 = sqrt(r21);
        double s = (r1 - r0) / (m1 - m0);
        hfr = r0 + s * (halfm - m0);
    }
    else
        hfr = 0.25;

    return hfr;
}

double Star::CalcPlanetMetrics(const usImage* pImg, int center_x, int center_y, int r, int annulusWidth)
{
    const double sigma_factor = 1.0;
    int scopeOuter = r + annulusWidth * 3;
    int scopeInner = r + annulusWidth;
    int scopeOuter2 = scopeOuter * scopeOuter;
    int scopeInner2 = scopeInner * scopeInner;
    int start_x = wxMax(0, center_x - scopeOuter);
    int end_x = wxMin(center_x + scopeOuter, pImg->Size.GetWidth() - 1);
    int start_y = wxMax(0, center_y - scopeOuter);
    int end_y = wxMin(center_y + scopeOuter, pImg->Size.GetHeight() - 1);

    const unsigned short* imgdata = pImg->ImageData;
    const int rowsize = pImg->Size.GetWidth();

    // Calculate the statistics within the larger annulus
    double snr = 0.0;
    double mass = 0;
    double sum = 0.0;
    double sq_sum = 0.0;
    int count = 0;
    const unsigned short* row = imgdata + rowsize * start_y;
    for (int y = start_y; y <= end_y; y++, row += rowsize)
    {
        for (int x = start_x; x <= end_x; x++)
        {
            int r2 = (x - center_x) * (x - center_x) + (y - center_y) * (y - center_y);
            if ((r2 < scopeOuter2) && (r2 > scopeInner2))
            {
                double pixel = (double) row[x];
                sum += pixel;
                sq_sum += pixel * pixel;
                count++;
            }
        }
    }

    // Calculate mean, variance and standard deviation in the annulus
    double mean = (count > 0) ? sum / count : 0.0;
    double variance = (count > 0) ? (sq_sum / count) - (mean * mean) : 0.0;
    double stdDev = sqrt(variance);
    int signalThreshold = mean + stdDev * sigma_factor;

    // Calculate signal and noise within the circle
    unsigned short peak_val = 0;
    double meanSignal = 0.0;
    int signalCount = 0;
    double meanNoise = 0.0;
    int noiseCount = 0;
    double noiseVariance = 0.0;
    row = imgdata + rowsize * start_y;
    for (int y = start_y; y <= end_y; y++, row += rowsize)
    {
        for (int x = start_x; x <= end_x; x++)
        {
            int r2 = (x - center_x) * (x - center_x) + (y - center_y) * (y - center_y);
            if (r2 < scopeInner2)
            {
                double pixel = (double) row[x];
                if (pixel > peak_val)
                    peak_val = pixel;
                if (pixel > signalThreshold)
                {
                    meanSignal += pixel;
                    signalCount++;
                }
            }
            else if (r2 < scopeOuter2)
            {
                double pixel = (double) row[x];
                if (pixel <= signalThreshold)
                {
                    meanNoise += pixel;
                    noiseVariance += pixel * pixel;
                    noiseCount++;
                }
            }
        }
    }

    // Use sum of all pixels considered as signal as the mass metric
    mass = meanSignal;
    meanSignal = (signalCount > 0) ? meanSignal / signalCount : 0.0;

    double noiseStdDev = 0.0;
    if (noiseCount > 0)
    {
        meanNoise /= noiseCount;
        noiseVariance = noiseVariance / noiseCount - meanNoise * meanNoise;
        noiseStdDev = sqrt(noiseVariance);
        snr = (noiseStdDev > 1) ? (meanSignal - meanNoise) / noiseStdDev : 0.0;
    }

    PeakVal = peak_val;
    Mass = mass;
    Debug.Write(wxString::Format("Star::CalcPlanetMetrics: signalThreshold=%d, meanSignal=%.1f, meanNoise=%.1f (stddev=%.1f), SNR=%.1f\n", signalThreshold, meanSignal, meanNoise, noiseStdDev, snr));
    return snr;
}
bool Star::Find(const usImage *pImg, int searchRegion, double base_x, double base_y, FindMode mode, double minHFD, double maxHFD, unsigned short maxADU, StarFindLogType loggingControl)
{
    FindResult Result = STAR_OK;
    double newX = (int) base_x;
    double newY = (int) base_y;

    try
    {
        if (loggingControl == FIND_LOGGING_VERBOSE)
            Debug.Write(wxString::Format("Star::Find(%d, %.2f, %.2f, %d, (%d,%d,%d,%d), %.1f, %0.1f, %hu) frame %u\n", searchRegion, base_x, base_y, mode,
            pImg->Subframe.x, pImg->Subframe.y, pImg->Subframe.width, pImg->Subframe.height, minHFD, maxHFD, maxADU, pImg->FrameNum));

        int minx, miny, maxx, maxy;

        if (pImg->Subframe.IsEmpty())
        {
            minx = miny = 0;
            maxx = pImg->Size.GetWidth() - 1;
            maxy = pImg->Size.GetHeight() - 1;
        }
        else
        {
            minx = pImg->Subframe.GetLeft();
            maxx = pImg->Subframe.GetRight();
            miny = pImg->Subframe.GetTop();
            maxy = pImg->Subframe.GetBottom();
        }

        // search region bounds
        int start_x = wxMax(base_x - searchRegion, minx);
        int end_x   = wxMin(base_x + searchRegion, maxx);
        int start_y = wxMax(base_y - searchRegion, miny);
        int end_y   = wxMin(base_y + searchRegion, maxy);

        if (end_x <= start_x || end_y <= start_y)
        {
            throw ERROR_INFO("coordinates are invalid");
        }

        const unsigned short *imgdata = pImg->ImageData;
        int rowsize = pImg->Size.GetWidth();

        int peak_x = 0, peak_y = 0;
        unsigned int peak_val = 0;
        unsigned short max3[3] = { 0, 0, 0 };

        if (mode == FIND_PEAK)
        {
            for (int y = start_y; y <= end_y; y++)
            {
                for (int x = start_x; x <= end_x; x++)
                {
                    unsigned short val = imgdata[y * rowsize + x];

                    if (val > peak_val)
                    {
                        peak_val = val;
                        peak_x = x;
                        peak_y = y;
                    }
                }
            }

            PeakVal = peak_val;
        }
        else if (mode == FIND_CENTROID)
        {
            // find the peak value within the search region using a smoothing function
            // also check for saturation

            for (int y = start_y + 1; y <= end_y - 1; y++)
            {
                for (int x = start_x + 1; x <= end_x - 1; x++)
                {
                    unsigned short p = imgdata[y * rowsize + x];
                    unsigned int val =
                        4 * (unsigned int) p +
                        imgdata[(y - 1) * rowsize + (x - 1)] +
                        imgdata[(y - 1) * rowsize + (x + 1)] +
                        imgdata[(y + 1) * rowsize + (x - 1)] +
                        imgdata[(y + 1) * rowsize + (x + 1)] +
                        2 * imgdata[(y - 1) * rowsize + (x + 0)] +
                        2 * imgdata[(y + 0) * rowsize + (x - 1)] +
                        2 * imgdata[(y + 0) * rowsize + (x + 1)] +
                        2 * imgdata[(y + 1) * rowsize + (x + 0)];

                    if (val > peak_val)
                    {
                        peak_val = val;
                        peak_x = x;
                        peak_y = y;
                    }

                    if (p > max3[0])
                        std::swap(p, max3[0]);
                    if (p > max3[1])
                        std::swap(p, max3[1]);
                    if (p > max3[2])
                        std::swap(p, max3[2]);
                }
            }

            PeakVal = max3[0];   // raw peak val
            peak_val /= 16; // smoothed peak value
        }
        else // FIND_PLANET
        {
            // Use exact position for planet tracking
            newX = base_x;
            newY = base_y;
            SNR = CalcPlanetMetrics(pImg, base_x, base_y, searchRegion, 15);
            HFD = pFrame->pGuider->m_Planet.GetHFD();
            goto done;
        }

        // measure noise in the annulus with inner radius A and outer radius B
        int const A = 7;   // inner radius
        int const B = 12;  // outer radius
        int const A2 = A * A;
        int const B2 = B * B;

        // center window around peak value
        start_x = wxMax(peak_x - B, minx);
        end_x = wxMin(peak_x + B, maxx);
        start_y = wxMax(peak_y - B, miny);
        end_y = wxMin(peak_y + B, maxy);

        // find the mean and stdev of the background

        unsigned int nbg;
        double mean_bg = 0., prev_mean_bg;
        double sigma2_bg = 0.;
        double sigma_bg = 0.;

        for (int iter = 0; iter < 9; iter++)
        {
            double sum = 0.0;
            double a = 0.0;
            double q = 0.0;
            nbg = 0;

            const unsigned short *row = imgdata + rowsize * start_y;
            for (int y = start_y; y <= end_y; y++, row += rowsize)
            {
                int dy = y - peak_y;
                int dy2 = dy * dy;
                for (int x = start_x; x <= end_x; x++)
                {
                    int dx = x - peak_x;
                    int r2 = dx * dx + dy2;

                    // exclude points not in annulus
                    if (r2 <= A2 || r2 > B2)
                        continue;

                    double const val = (double) row[x];

                    if (iter > 0 && (val < mean_bg - 2.0 * sigma_bg || val > mean_bg + 2.0 * sigma_bg))
                        continue;

                    sum += val;
                    ++nbg;
                    double const k = (double) nbg;
                    double const a0 = a;
                    a += (val - a) / k;
                    q += (val - a0) * (val - a);
                }
            }

            if (nbg < 10) // only possible after the first iteration
            {
                Debug.Write(wxString::Format("Star::Find: too few background points! nbg=%u mean=%.1f sigma=%.1f\n",
                    nbg, mean_bg, sigma_bg));
                break;
            }

            prev_mean_bg = mean_bg;
            mean_bg = sum / (double) nbg;
            sigma2_bg = q / (double) (nbg - 1);
            sigma_bg = sqrt(sigma2_bg);

            if (iter > 0 && fabs(mean_bg - prev_mean_bg) < 0.5)
                break;
        }

        unsigned short thresh;

        double cx = 0.0;
        double cy = 0.0;
        double mass = 0.0;
        unsigned int n;

        std::vector<R2M> hfrvec;

        if (mode == FIND_PEAK)
        {
            mass = peak_val;
            n = 1;
            thresh = 0;
        }
        else
        {
            thresh = (unsigned short)(mean_bg + 3.0 * sigma_bg + 0.5);

            // find pixels over threshold within aperture; compute mass and centroid

            start_x = wxMax(peak_x - A, minx);
            end_x = wxMin(peak_x + A, maxx);
            start_y = wxMax(peak_y - A, miny);
            end_y = wxMin(peak_y + A, maxy);

            n = 0;

            const unsigned short *row = imgdata + rowsize * start_y;
            for (int y = start_y; y <= end_y; y++, row += rowsize)
            {
                int dy = y - peak_y;
                int dy2 = dy * dy;
                if (dy2 > A2)
                    continue;

                for (int x = start_x; x <= end_x; x++)
                {
                    int dx = x - peak_x;

                    // exclude points outside aperture
                    if (dx * dx + dy2 > A2)
                        continue;

                    // exclude points below threshold
                    unsigned short val = row[x];
                    if (val < thresh)
                        continue;

                    double const d = (double) val - mean_bg;

                    cx += dx * d;
                    cy += dy * d;
                    mass += d;
                    ++n;

                    hfrvec.push_back(R2M(x, y, d));
                }
            }
        }

        Mass = mass;

        // SNR estimate from: Measuring the Signal-to-Noise Ratio S/N of the CCD Image of a Star or Nebula, J.H.Simonetti, 2004 January 8
        //     http://www.phys.vt.edu/~jhs/phys3154/snr20040108.pdf
        double const gain = .5; // electrons per ADU, nominal
        SNR = (n > 0 && nbg > 0) ? mass / sqrt(mass / gain + sigma2_bg * (double)n * (1.0 + 1.0 / (double)nbg)) : 0.0;

        double const LOW_SNR = 3.0;

        // a few scattered pixels over threshold can give a false positive
        // avoid this by requiring the smoothed peak value to be above the threshold
        if (peak_val <= thresh && SNR >= LOW_SNR)
        {
            Debug.Write(wxString::Format("Star::Find false star n=%u nbg=%u bg=%.1f sigma=%.1f thresh=%u peak=%u\n", n, nbg, mean_bg, sigma_bg, thresh, peak_val));
            SNR = LOW_SNR - 0.1;
        }

        if (mass < 10.0)
        {
            HFD = 0.;
            Result = STAR_LOWMASS;
            goto done;
        }

        if (SNR < LOW_SNR)
        {
            HFD = 0.;
            Result = STAR_LOWSNR;
            goto done;
        }

        newX = peak_x + cx / mass;
        newY = peak_y + cy / mass;

        HFD = 2.0 * hfr(hfrvec, newX, newY, mass);
        // Check for constraints on HFD value
        if (mode != FIND_PEAK)
        {
            if (HFD < minHFD)
            {
                Result = STAR_LOWHFD;
                goto done;
            }
            if (HFD > maxHFD)
            {
                Result = STAR_HIHFD;
                goto done;
            }
        }

        // check for saturation

        unsigned int mx = (unsigned int) max3[0];

        // remove pedestal
        if (mx >= pImg->Pedestal)
            mx -= pImg->Pedestal;
        else
            mx = 0; // unlikely

        if (maxADU > 0)
        {
            // maxADU is known
            if (mx >= maxADU)
                Result = STAR_SATURATED;
            goto done;
        }

        // maxADU not known, use the "flat-top" heuristic
        //
        // even at saturation, the max values may vary a bit due to noise
        // Call it saturated if the the top three values are within 32 parts per 65535 of max for 16-bit cameras,
        // or within 1 part per 191 for 8-bit cameras
        unsigned int d = (unsigned int) (max3[0] - max3[2]);

        if (pImg->BitsPerPixel < 12)
        {
            if (d * 191U < 1U * mx)
                Result = STAR_SATURATED;
        }
        else
        {
            if (d * 65535U < 32U * mx)
                Result = STAR_SATURATED;
        }
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);

        if (Result == STAR_OK)
        {
            Result = STAR_ERROR;
        }
    }

done:
    // update state
    SetXY(newX, newY);
    m_lastFindResult = Result;

    bool wasFound = WasFound(Result);

    if (Result == STAR_ERROR)
    {
        Mass = 0.0;
        SNR = 0.0;
        HFD = 0.0;
    }

    if (loggingControl == FIND_LOGGING_VERBOSE)
        Debug.Write(wxString::Format("Star::Find returns %d (%d), X=%.2f, Y=%.2f, Mass=%.f, SNR=%.1f, Peak=%hu HFD=%.1f\n",
        wasFound, Result, newX, newY, Mass, SNR, PeakVal, HFD));

    return wasFound;
}

bool Star::Find(const usImage *pImg, int searchRegion, FindMode mode, double minHFD, double maxHFD, unsigned short saturation, StarFindLogType loggingControl)
{
    return Find(pImg, searchRegion, X, Y, mode, minHFD, maxHFD, saturation, loggingControl);
}

struct FloatImg
{
    float *px;
    wxSize Size;
    unsigned int NPixels;

    FloatImg() : px(0) { }
    FloatImg(const wxSize& size) : px(0) { Init(size); }
    FloatImg(const usImage& img) : px(0) {
        Init(img.Size);
        for (unsigned int i = 0; i < NPixels; i++)
            px[i] = (float) img.ImageData[i];
    }
    ~FloatImg() { delete[] px; }
    void Init(const wxSize& sz) { delete[] px;  Size = sz; NPixels = Size.GetWidth() * Size.GetHeight(); px = new float[NPixels]; }
    void Swap(FloatImg& other) { std::swap(px, other.px); std::swap(Size, other.Size); std::swap(NPixels, other.NPixels); }
};

static void GetStats(double *mean, double *stdev, const FloatImg& img, const wxRect& win)
{
    // Determine the mean and standard deviation
    double sum = 0.0;
    double a = 0.0;
    double q = 0.0;
    double k = 1.0;
    double km1 = 0.0;

    const int width = img.Size.GetWidth();
    const float *p0 = &img.px[win.GetTop() * width + win.GetLeft()];
    for (int y = 0; y < win.GetHeight(); y++)
    {
        const float *end = p0 + win.GetWidth();
        for (const float *p = p0; p < end; p++)
        {
            double const x = (double) *p;
            sum += x;
            double const a0 = a;
            a += (x - a) / k;
            q += (x - a0) * (x - a);
            km1 = k;
            k += 1.0;
        }
        p0 += width;
    }

    *mean = sum / km1;
    *stdev = sqrt(q / km1);
}

// un-comment to save the intermediate autofind image
//#define SAVE_AUTOFIND_IMG

static void SaveImage(const FloatImg& img, const char *name)
{
#ifdef SAVE_AUTOFIND_IMG
    float maxv = img.px[0];
    float minv = img.px[0];

    for (unsigned int i = 1; i < img.NPixels; i++)
    {
        if (img.px[i] > maxv)
            maxv = img.px[i];
        if (img.px[i] < minv)
            minv = img.px[i];
    }

    usImage tmp;
    tmp.Init(img.Size);
    for (unsigned int i = 0; i < tmp.NPixels; i++)
    {
        tmp.ImageData[i] = (unsigned short)(((double) img.px[i] - minv) * 65535.0 / (maxv - minv));
    }

    tmp.Save(wxFileName(Debug.GetLogDir(), name).GetFullPath());
#endif // SAVE_AUTOFIND_IMG
}

static void psf_conv(FloatImg& dst, const FloatImg& src)
{
    dst.Init(src.Size);

    //                       A      B1     B2    C1     C2    C3     D1     D2     D3
    const double PSF[] = { 0.906, 0.584, 0.365, .117, .049, -0.05, -.064, -.074, -.094 };

    int const width = src.Size.GetWidth();
    int const height = src.Size.GetHeight();

    memset(dst.px, 0, src.NPixels * sizeof(float));

    /* PSF Grid is:
    D3 D3 D3 D3 D3 D3 D3 D3 D3
    D3 D3 D3 D2 D1 D2 D3 D3 D3
    D3 D3 C3 C2 C1 C2 C3 D3 D3
    D3 D2 C2 B2 B1 B2 C2 D2 D3
    D3 D1 C1 B1 A  B1 C1 D1 D3
    D3 D2 C2 B2 B1 B2 C2 D2 D3
    D3 D3 C3 C2 C1 C2 C3 D3 D3
    D3 D3 D3 D2 D1 D2 D3 D3 D3
    D3 D3 D3 D3 D3 D3 D3 D3 D3

    1@A
    4@B1, B2, C1, C3, D1
    8@C2, D2
    44 * D3
    */

    int psf_size = 4;

    for (int y = psf_size; y < height - psf_size; y++)
    {
        for (int x = psf_size; x < width - psf_size; x++)
        {
            float A, B1, B2, C1, C2, C3, D1, D2, D3;

#define PX(dx, dy) *(src.px + width * (y + (dy)) + x + (dx))
            A =  PX(+0, +0);
            B1 = PX(+0, -1) + PX(+0, +1) + PX(+1, +0) + PX(-1, +0);
            B2 = PX(-1, -1) + PX(+1, -1) + PX(-1, +1) + PX(+1, +1);
            C1 = PX(+0, -2) + PX(-2, +0) + PX(+2, +0) + PX(+0, +2);
            C2 = PX(-1, -2) + PX(+1, -2) + PX(-2, -1) + PX(+2, -1) + PX(-2, +1) + PX(+2, +1) + PX(-1, +2) + PX(+1, +2);
            C3 = PX(-2, -2) + PX(+2, -2) + PX(-2, +2) + PX(+2, +2);
            D1 = PX(+0, -3) + PX(-3, +0) + PX(+3, +0) + PX(+0, +3);
            D2 = PX(-1, -3) + PX(+1, -3) + PX(-3, -1) + PX(+3, -1) + PX(-3, +1) + PX(+3, +1) + PX(-1, +3) + PX(+1, +3);
            D3 = PX(-4, -2) + PX(-3, -2) + PX(+3, -2) + PX(+4, -2) + PX(-4, -1) + PX(+4, -1) + PX(-4, +0) + PX(+4, +0) + PX(-4, +1) + PX(+4, +1) + PX(-4, +2) + PX(-3, +2) + PX(+3, +2) + PX(+4, +2);
#undef PX
            int i;
            const float *uptr;

            uptr = src.px + width * (y - 4) + (x - 4);
            for (i = 0; i < 9; i++)
                D3 += *uptr++;

            uptr = src.px + width * (y - 3) + (x - 4);
            for (i = 0; i < 3; i++)
                D3 += *uptr++;
            uptr += 3;
            for (i = 0; i < 3; i++)
                D3 += *uptr++;

            uptr = src.px + width * (y + 3) + (x - 4);
            for (i = 0; i < 3; i++)
                D3 += *uptr++;
            uptr += 3;
            for (i = 0; i < 3; i++)
                D3 += *uptr++;

            uptr = src.px + width * (y + 4) + (x - 4);
            for (i = 0; i < 9; i++)
                D3 += *uptr++;

            double mean = (A + B1 + B2 + C1 + C2 + C3 + D1 + D2 + D3) / 81.0;
            double PSF_fit = PSF[0] * (A - mean) + PSF[1] * (B1 - 4.0 * mean) + PSF[2] * (B2 - 4.0 * mean) +
                PSF[3] * (C1 - 4.0 * mean) + PSF[4] * (C2 - 8.0 * mean) + PSF[5] * (C3 - 4.0 * mean) +
                PSF[6] * (D1 - 4.0 * mean) + PSF[7] * (D2 - 8.0 * mean) + PSF[8] * (D3 - 44.0 * mean);

            dst.px[width * y + x] = (float) PSF_fit;
        }
    }
}

static void Downsample(FloatImg& dst, const FloatImg& src, int downsample)
{
    int width = src.Size.GetWidth();
    int dw = src.Size.GetWidth() / downsample;
    int dh = src.Size.GetHeight() / downsample;

    dst.Init(wxSize(dw, dh));

    float const d2 = downsample * downsample;

    for (int yy = 0; yy < dh; yy++)
    {
        for (int xx = 0; xx < dw; xx++)
        {
            float sum = 0.0;
            for (int j = 0; j < downsample; j++)
                for (int i = 0; i < downsample; i++)
                    sum += src.px[(yy * downsample + j) * width + xx * downsample + i];
            float val = sum / d2;
            dst.px[yy * dw + xx] = val;
        }
    }
}

struct Peak
{
    int x;
    int y;
    float val;

    Peak() { }
    Peak(int x_, int y_, float val_) : x(x_), y(y_), val(val_) { }
    bool operator<(const Peak& rhs) const { return val < rhs.val; }
};

static void RemoveItems(std::set<Peak>& stars, const std::set<int>& to_erase)
{
    int n = 0;
    for (std::set<Peak>::iterator it = stars.begin(); it != stars.end(); n++)
    {
        if (to_erase.find(n) != to_erase.end())
        {
            std::set<Peak>::iterator next = it;
            ++next;
            stars.erase(it);
            it = next;
        }
        else
            ++it;
    }
}

static bool CloseToReference(const GuideStar& referencePoint, const GuideStar& other)
{
    // test whether star is close to the reference star for purposes of detecting duplicates and improving spacial sampling
    const double minSeparation = 25.0;
    return other.Distance(referencePoint) < minSeparation;
}

// Multi-star version of AutoFind.
bool GuideStar::AutoFind(const usImage& image, int extraEdgeAllowance, int searchRegion, const wxRect& roi,
    std::vector<GuideStar>& foundStars, int maxStars)
{
    if (!image.Subframe.IsEmpty())
    {
        Debug.AddLine("AutoFind called on subframe, returning error");
        return false; // not found
    }

    wxBusyCursor busy;

    Debug.Write(wxString::Format("Star::AutoFind called with edgeAllowance = %d "
        "searchRegion = %d roi = %dx%d@%d,%d\n",
        extraEdgeAllowance, searchRegion, roi.width, roi.height,
        roi.x, roi.y));

    // run a 3x3 median first to eliminate hot pixels
    usImage smoothed;
    smoothed.CopyFrom(image);
    if (!roi.IsEmpty())
    {
        // set the subrame to the roi so that the Median3
        // operation blanks pixels outside the ROI.
        smoothed.Subframe = roi;
        smoothed.Subframe.Intersect(wxRect(smoothed.Size));

        Debug.Write(wxString::Format("AutoFind: using ROI %dx%d@%d,%d\n",
            smoothed.Subframe.width, smoothed.Subframe.height,
            smoothed.Subframe.x, smoothed.Subframe.y));

        if (smoothed.Subframe.width < searchRegion ||
            smoothed.Subframe.height < searchRegion)
        {
            Debug.Write(wxString::Format("AutoFind: bad ROI %dx%d\n",
                smoothed.Subframe.width,
                smoothed.Subframe.height));
            return false;
        }
    }
    Median3(smoothed);

    // convert to floating point
    FloatImg conv(smoothed);

    // downsample the source image
    int downsample = pFrame->pGuider->GetAutoSelDownsample();
    if (downsample == 0 /* "Auto" */)
    {
        double const DOWNSAMPLE_SCALE_THRESH = 0.6;
        double scale = pFrame->GetCameraPixelScale();

        if (scale > DOWNSAMPLE_SCALE_THRESH)
            downsample = 1;
        else
            downsample = 2;

        Debug.Write(wxString::Format("AutoFind: auto downsample for scale %.2f => %dx\n", scale, downsample));
    }
    if (downsample > 1)
    {
        Debug.Write(wxString::Format("AutoFind: downsample %dx\n", downsample));
        FloatImg tmp;
        Downsample(tmp, conv, downsample);
        conv.Swap(tmp);
    }

    // run the PSF convolution
    {
        FloatImg tmp;
        psf_conv(tmp, conv);
        conv.Swap(tmp);
    }

    enum { CONV_RADIUS = 4 };
    int dw = conv.Size.GetWidth();      // width of the downsampled image
    int dh = conv.Size.GetHeight();     // height of the downsampled image
    wxRect convRect(CONV_RADIUS, CONV_RADIUS, dw - 2 * CONV_RADIUS, dh - 2 * CONV_RADIUS);  // region containing valid data

    SaveImage(conv, "PHD2_AutoFind.fit");

    enum { TOP_N = 100 };  // keep track of the brightest stars
    std::set<Peak> stars;  // sorted by ascending intensity

    double global_mean, global_stdev;
    GetStats(&global_mean, &global_stdev, conv, convRect);

    Debug.Write(wxString::Format("AutoFind: global mean = %.1f, stdev %.1f\n", global_mean, global_stdev));

    const double threshold = 0.1;
    Debug.Write(wxString::Format("AutoFind: using threshold = %.1f\n", threshold));

    // find each local maximum
    int srch = 4;
    for (int y = convRect.GetTop() + srch; y <= convRect.GetBottom() - srch; y++)
    {
        for (int x = convRect.GetLeft() + srch; x <= convRect.GetRight() - srch; x++)
        {
            float val = conv.px[dw * y + x];
            bool ismax = false;
            if (val > 0.0)
            {
                ismax = true;
                for (int j = -srch; j <= srch; j++)
                {
                    for (int i = -srch; i <= srch; i++)
                    {
                        if (i == 0 && j == 0)
                            continue;
                        if (conv.px[dw * (y + j) + (x + i)] > val)
                        {
                            ismax = false;
                            break;
                        }
                    }
                }
            }
            if (!ismax)
                continue;

            // compare local maximum to mean value of surrounding pixels
            const int local = 7;
            double local_mean, local_stdev;
            wxRect localRect(x - local, y - local, 2 * local + 1, 2 * local + 1);
            localRect.Intersect(convRect);
            GetStats(&local_mean, &local_stdev, conv, localRect);

            // this is our measure of star intensity
            double h = (val - local_mean) / global_stdev;

            if (h < threshold)
            {
                //  Debug.Write(wxString::Format("AG: local max REJECT [%d, %d] PSF %.1f SNR %.1f\n", imgx, imgy, val, SNR));
                continue;
            }

            // coordinates on the original image
            int imgx = x * downsample + downsample / 2;
            int imgy = y * downsample + downsample / 2;

            stars.insert(Peak(imgx, imgy, h));
            if (stars.size() > TOP_N)
                stars.erase(stars.begin());
        }
    }

    for (std::set<Peak>::const_reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
        Debug.Write(wxString::Format("AutoFind: local max [%d, %d] %.1f\n", it->x, it->y, it->val));

    // merge stars that are very close into a single star
    {
        const int minlimitsq = 5 * 5;
    repeat:
        for (std::set<Peak>::const_iterator a = stars.begin(); a != stars.end(); ++a)
        {
            std::set<Peak>::const_iterator b = a;
            ++b;
            for (; b != stars.end(); ++b)
            {
                int dx = a->x - b->x;
                int dy = a->y - b->y;
                int d2 = dx * dx + dy * dy;
                if (d2 < minlimitsq)
                {
                    // very close, treat as single star
                    Debug.Write(wxString::Format("AutoFind: merge [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                    // erase the dimmer one
                    stars.erase(a);
                    goto repeat;
                }
            }
        }
    }

    // exclude stars that would fit within a single searchRegion box
    {
        // build a list of stars to be excluded
        std::set<int> to_erase;
        const int extra = 5; // extra safety margin
        const int fullw = searchRegion + extra;
        for (std::set<Peak>::const_iterator a = stars.begin(); a != stars.end(); ++a)
        {
            std::set<Peak>::const_iterator b = a;
            ++b;
            for (; b != stars.end(); ++b)
            {
                int dx = abs(a->x - b->x);
                int dy = abs(a->y - b->y);
                if (dx <= fullw && dy <= fullw)
                {
                    // stars closer than search region, exclude them both
                    // but do not let a very dim star eliminate a very bright star
                    if (b->val / a->val >= 5.0)
                    {
                        Debug.Write(wxString::Format("AutoFind: close dim-bright [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                    }
                    else
                    {
                        Debug.Write(wxString::Format("AutoFind: too close [%d, %d] %.1f - [%d, %d] %.1f\n", a->x, a->y, a->val, b->x, b->y, b->val));
                        to_erase.insert(std::distance(stars.begin(), a));
                        to_erase.insert(std::distance(stars.begin(), b));
                    }
                }
            }
        }
        RemoveItems(stars, to_erase);
    }

    // exclude stars too close to the edge
    {
        int edgeDist = searchRegion + extraEdgeAllowance;

        std::set<Peak>::iterator it = stars.begin();
        while (it != stars.end())
        {
            std::set<Peak>::iterator next = it;
            ++next;
            if (it->x <= edgeDist || it->x >= image.Size.GetWidth() - edgeDist ||
                it->y <= edgeDist || it->y >= image.Size.GetHeight() - edgeDist)
            {
                Debug.Write(wxString::Format("AutoFind: too close to edge [%d, %d] %.1f\n", it->x, it->y, it->val));
                stars.erase(it);
            }
            it = next;
        }
    }

    // At first I tried running Star::Find on the survivors to find the best
    // star. This had the unfortunate effect of locating hot pixels which
    // the psf convolution so nicely avoids. So, don't do that!  -ag

    unsigned int sat_level; // saturation level, including pedestal

    if (pCamera->IsSaturationByADU())
    {
        // known saturation level ... easy
        sat_level = pCamera->GetSaturationADU() + image.Pedestal;
    }
    else
    {
        // try to identify the saturation point

        //  first, find the peak pixel overall
        unsigned short maxVal = 0;
        for (unsigned int i = 0; i < image.NPixels; i++)
        if (image.ImageData[i] > maxVal)
            maxVal = image.ImageData[i];

        // next see if any of the stars has a flat-top
        bool foundSaturated = false;
        for (std::set<Peak>::reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
        {
            Star tmp;
            tmp.Find(&image, searchRegion, it->x, it->y, FIND_CENTROID, pFrame->pGuider->GetMinStarHFD(), pFrame->pGuider->GetMaxStarHFD(), pCamera->GetSaturationADU(), FIND_LOGGING_VERBOSE);
            if (tmp.WasFound() && tmp.GetError() == STAR_SATURATED)
            {
                if ((maxVal - tmp.PeakVal) * 255U > maxVal)
                {
                    // false positive saturation, flat top but below maxVal
                    Debug.Write(wxString::Format("AutoFind: false positive saturation peak = %hu, max = %hu\n", tmp.PeakVal, maxVal));
                }
                else
                {
                    // a saturated star was found
                    foundSaturated = true;
                    break;
                }
            }
        }

        if (foundSaturated)
        {
            // use the peak overall pixel value as the saturation limit
            Debug.Write(wxString::Format("AutoFind: using saturation level peakVal = %hu\n", maxVal));
            sat_level = maxVal; // includes pedestal
        }
        else
        {
            // no saturated stars found, can't make any assumption about whether the max val is saturated

            Debug.Write(wxString::Format("AutoFind: using saturation level from BPP %u and pedestal %hu\n",
                image.BitsPerPixel, image.Pedestal));

            sat_level = ((1U << image.BitsPerPixel) - 1) + image.Pedestal;
        }
    }

    unsigned int range = sat_level > image.Pedestal ? sat_level - image.Pedestal : 0U;
    // "near-saturation" threshold at 90% saturation
    unsigned int tmp = (unsigned int)image.Pedestal + 9 * range / 10;
    if (tmp > 65535) tmp = 65535;
    unsigned short sat_thresh = (unsigned short)tmp;

    Debug.Write(wxString::Format("AutoFind: BPP = %u, saturation at %u, pedestal %hu, thresh = %hu\n",
        image.BitsPerPixel, sat_level, image.Pedestal, sat_thresh));

    // Before sifting for the best star, collect all the viable candidates
    double minSNR = pFrame->pGuider->GetAFMinStarSNR();
    double maxHFD = pFrame->pGuider->GetMaxStarHFD();
    foundStars.clear();
    for (std::set<Peak>::reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
    {
        GuideStar tmp;
        tmp.Find(&image, searchRegion, it->x, it->y, FIND_CENTROID, pFrame->pGuider->GetMinStarHFD(), maxHFD, pCamera->GetSaturationADU(), FIND_LOGGING_VERBOSE);
        // We're repeating the find, so we're vulnerable to hot pixels and creation of unwanted duplicates
        if (tmp.WasFound() && tmp.SNR >= minSNR)
        {
            bool duplicate = std::find_if(foundStars.begin(), foundStars.end(),
                [&tmp](const GuideStar& other) { return CloseToReference(tmp, other); }) != foundStars.end();

            if (!duplicate)
            {
                tmp.referencePoint.X = tmp.X;
                tmp.referencePoint.Y = tmp.Y;
                foundStars.push_back(tmp);
            }
        }
    }

    // Final star selection - either the only star or the primary one for multi-star mode
    //   pass 1: find brightest star with peak value < 90% saturation AND SNR >= MinSNR (defaults to 6)
    //       this pass will reject saturated and nearly-saturated stars
    //   pass 2: find brightest non-saturated star with SNR >= MinSNR
    //   pass 3: find brightest star, even if saturated or below MinSNR

    for (int pass = 1; pass <= 3; pass++)
    {
        Debug.Write(wxString::Format("AutoFind: finding best star pass %d\n", pass));

        for (std::set<Peak>::reverse_iterator it = stars.rbegin(); it != stars.rend(); ++it)
        {
            GuideStar tmp;
            tmp.Find(&image, searchRegion, it->x, it->y, FIND_CENTROID, pFrame->pGuider->GetMinStarHFD(), maxHFD, pCamera->GetSaturationADU(), FIND_LOGGING_VERBOSE);
            if (tmp.WasFound())
            {
                if (pass == 1)
                {
                    if (tmp.PeakVal > sat_thresh)
                    {
                        Debug.Write(wxString::Format("AutoFind: near-saturated [%d, %d] %.1f Mass %.f SNR %.1f Peak %hu\n", it->x, it->y, it->val, tmp.Mass, tmp.SNR, tmp.PeakVal));
                        continue;
                    }
                    if (tmp.GetError() == STAR_SATURATED || tmp.SNR < minSNR)
                        continue;
                }
                else if (pass == 2)
                {
                    if (tmp.GetError() == STAR_SATURATED || tmp.SNR < minSNR)
                    {
                        Debug.Write(wxString::Format("AutoFind: star saturated or too dim [%d, %d] %.1f Mass %.f SNR %.1f\n", it->x, it->y, it->val, tmp.Mass, tmp.SNR));
                        continue;
                    }
                }

                // star accepted
                SetXY(it->x, it->y);
                Debug.Write(wxString::Format("AutoFind returns star at [%d, %d] %.1f Mass %.f SNR %.1f\n", it->x, it->y, it->val, tmp.Mass, tmp.SNR));
                if (maxStars > 1)
                {
                    // Find the chosen star in the list and compute the offsetFromPrimary for all secondary stars
                    int primaryLoc = -1;
                    PHD_Point primaryRef(it->x, it->y);
                    for (auto pGS = foundStars.begin(); pGS != foundStars.end(); ++pGS)
                    {
                        if (pGS->X == tmp.X && pGS->Y == tmp.Y)
                        {
                            primaryLoc = pGS - foundStars.begin();
                        }
                        else
                        {
                            pGS->offsetFromPrimary = pGS->referencePoint - primaryRef;
                        }
                    }

                    if (primaryLoc >= 0)
                    {
                        // Delete saturated stars ahead of chosen star, likely saturated or otherwise flawed
                        foundStars.erase(foundStars.begin(), foundStars.begin() + primaryLoc);
                        // Prune total list size to match maxStars parameter
                        if (foundStars.size() > maxStars)
                            foundStars.erase(foundStars.begin() + maxStars, foundStars.end());
                    }
                    else if (primaryLoc == -1)
                    {
                        // Secondary stars are presumably degraded, just put primary star at head of list
                        foundStars.clear();
                        tmp.referencePoint.X = tmp.X;
                        tmp.referencePoint.Y = tmp.Y;
                        foundStars.push_back(tmp);
                        Debug.Write("MultiStar: primary star forcibly inserted in list\n");
                    }
                }
                return true;
            }
        }

        if (pass == 1)
            Debug.Write("AutoFind: could not find a star on Pass 1\n");
        else if (pass == 2)
            Debug.Write("AutoFind: could not find a non-saturated star!\n");
    }

    Debug.Write("AutoFind: no star found\n");
    return false;
}
