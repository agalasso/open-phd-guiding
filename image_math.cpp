/*
 *  image_math.cpp
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
#include "image_math.h"

#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <wx/tokenzr.h>

#include <algorithm>

int dbl_sort_func (double *first, double *second)
{
    if (*first < *second)
        return -1;
    else if (*first > *second)
        return 1;
    return 0;
}

double CalcSlope(const ArrayOfDbl& y)
{
    // Does a linear regression to calculate the slope

    int nn = (int) y.GetCount();

    if (nn < 2)
        return 0.;

    double s_xy = 0.0;
    double s_y = 0.0;

    for (int x = 0; x < nn; x++)
    {
        s_xy += (double)(x + 1) * y[x];
        s_y += y[x];
    }

    int sx = (nn * (nn + 1)) / 2;
    int sxx = sx * (2 * nn + 1) / 3;
    double s_x = (double) sx;
    double s_xx = (double) sxx;
    double n = (double) nn;
    return (n * s_xy - (s_x * s_y)) / (n * s_xx - (s_x * s_x));
}

bool QuickLRecon(usImage& img)
{
    // Does a simple debayer of luminance data only -- sliding 2x2 window
    usImage tmp;
    if (tmp.Init(img.Size))
    {
        pFrame->Alert(_("Memory allocation error"));
        return true;
    }

    int const W = img.Size.GetWidth();
    int RX, RY, RW, RH;
    if (img.Subframe.IsEmpty())
    {
        RX = RY = 0;
        RW = img.Size.GetWidth();
        RH = img.Size.GetHeight();
    }
    else
    {
        RX = img.Subframe.GetX();
        RY = img.Subframe.GetY();
        RW = img.Subframe.GetWidth();
        RH = img.Subframe.GetHeight();
        tmp.Clear();
    }

#define IX(x_, y_) ((RY + (y_)) * W + RX + (x_))

    unsigned short *d;
    unsigned int t;

    for (int y = 0; y <= RH - 2; y++)
    {
        d = &tmp.ImageData[IX(0, y)];

        for (int x = 0; x <= RW - 2; x++)
        {
            t  = img.ImageData[IX(x    , y    )];
            t += img.ImageData[IX(x + 1, y    )];
            t += img.ImageData[IX(x    , y + 1)];
            t += img.ImageData[IX(x + 1, y + 1)];
            *d++ = (unsigned short)(t >> 2);
        }

        // last col
        t  = img.ImageData[IX(RW - 1, y    )];
        t += img.ImageData[IX(RW - 1, y + 1)];
        *d = (unsigned short)(t >> 1);
    }

    // last row

    d = &tmp.ImageData[IX(0, RH - 1)];

    for (int x = 0; x <= RW - 2; x++)
    {
        t  = img.ImageData[IX(x    , RH - 1)];
        t += img.ImageData[IX(x + 1, RH - 1)];
        *d++ = (unsigned short)(t >> 1);
    }

    // bottom-right pixel
    *d = img.ImageData[IX(RW - 1, RH - 1)];

#undef IX

    img.SwapImageData(tmp);
    return false;
}

bool Median3(usImage& img)
{
    usImage tmp;

    if (tmp.Init(img.Size))
    {
        Debug.Write("Median3: ERROR: memory allocation failure!\n");
        return true;
    }

    if (img.Subframe.IsEmpty())
    {
        Median3(tmp.ImageData, img.ImageData, img.Size, wxRect(img.Size));
    }
    else
    {
        tmp.Clear();
        Median3(tmp.ImageData, img.ImageData, img.Size, img.Subframe);
    }

    img.SwapImageData(tmp);

    return false;
}

inline static void swap(unsigned short& a, unsigned short& b)
{
    unsigned short const t = a;
    a = b;
    b = t;
}

inline static unsigned short median9(const unsigned short l[9])
{
    unsigned short l0 = l[0], l1 = l[1], l2 = l[2], l3 = l[3], l4 = l[4];
    unsigned short x;
    x = l[5];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    if (x < l4) swap(x, l4);
    x = l[6];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    if (x < l4) swap(x, l4);
    x = l[7];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    if (x < l4) swap(x, l4);
    x = l[8];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    if (x < l4) swap(x, l4);

    if (l1 > l0) l0 = l1;
    if (l2 > l0) l0 = l2;
    if (l3 > l0) l0 = l3;
    if (l4 > l0) l0 = l4;

    return l0;
}

inline static unsigned short median8(const unsigned short l[8])
{
    unsigned short l0 = l[0], l1 = l[1], l2 = l[2], l3 = l[3], l4 = l[4];
    unsigned short x;

    x = l[5];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    if (x < l4) swap(x, l4);
    x = l[6];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    if (x < l4) swap(x, l4);
    x = l[7];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    if (x < l4) swap(x, l4);

    if (l2 > l0) swap(l2, l0);
    if (l2 > l1) swap(l2, l1);

    if (l3 > l0) swap(l3, l0);
    if (l3 > l1) swap(l3, l1);

    if (l4 > l0) swap(l4, l0);
    if (l4 > l1) swap(l4, l1);

    return (unsigned short)(((unsigned int) l0 + (unsigned int) l1) / 2);
}

inline static unsigned short median6(const unsigned short l[6])
{
    unsigned short l0 = l[0], l1 = l[1], l2 = l[2], l3 = l[3];
    unsigned short x;

    x = l[4];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);
    x = l[5];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    if (x < l3) swap(x, l3);

    if (l2 > l0) swap(l2, l0);
    if (l2 > l1) swap(l2, l1);

    if (l3 > l0) swap(l3, l0);
    if (l3 > l1) swap(l3, l1);

    return (unsigned short)(((unsigned int) l0 + (unsigned int) l1) / 2);
}

inline static unsigned short median5(const unsigned short l[5])
{
    unsigned short l0 = l[0], l1 = l[1], l2 = l[2];
    unsigned short x;
    x = l[3];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);
    x = l[4];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);

    if (l1 > l0) l0 = l1;
    if (l2 > l0) l0 = l2;

    return l0;
}

inline static unsigned short median4(const unsigned short l[4])
{
    unsigned short l0 = l[0], l1 = l[1], l2 = l[2];
    unsigned short x;
    x = l[3];
    if (x < l0) swap(x, l0);
    if (x < l1) swap(x, l1);
    if (x < l2) swap(x, l2);

    if (l2 > l0) swap(l2, l0);
    if (l2 > l1) swap(l2, l1);

    return (unsigned short)(((unsigned int) l0 + (unsigned int) l1) / 2);
}

inline static unsigned short median3(const unsigned short l[3])
{
    unsigned short l0 = l[0], l1 = l[1], l2 = l[2];
    if (l2 < l0) swap(l2, l0);
    if (l2 < l1) swap(l2, l1);
    if (l1 > l0) l0 = l1;
    return l0;
}

void Median3(unsigned short *dst, const unsigned short *src, const wxSize& size, const wxRect& rect)
{
    int const W = size.GetWidth();
    int const RX = rect.GetX();
    int const RY = rect.GetY();
    int const RW = rect.GetWidth();
    int const RH = rect.GetHeight();

    unsigned short a[9];
    unsigned short *d;

#define IX(x_, y_) ((RY + (y_)) * W + RX + (x_))

    // top row
    d = &dst[IX(0, 0)];

    // top-left corner
    a[0] = src[IX(0, 0)];
    a[1] = src[IX(1, 0)];
    a[2] = src[IX(0, 1)];
    a[3] = src[IX(1, 1)];
    *d++ = median4(a);

    // top row middle pixels
    for (int x = 1; x <= RW - 2; x++)
    {
        a[0] = src[IX(x - 1, 0)];
        a[1] = src[IX(x,     0)];
        a[2] = src[IX(x + 1, 0)];
        a[3] = src[IX(x - 1, 1)];
        a[4] = src[IX(x,     1)];
        a[5] = src[IX(x + 1, 1)];
        *d++ = median6(a);
    }

    // top-right corner
    a[0] = src[IX(RW - 2, 0)];
    a[1] = src[IX(RW - 1, 0)];
    a[2] = src[IX(RW - 2, 1)];
    a[3] = src[IX(RW - 1, 1)];
    *d = median4(a);

    for (int y = 1; y <= RH - 2; y++)
    {
        d = &dst[IX(0, y)];

        // leftmost pixel
        a[0] = src[IX(0, y - 1)];
        a[1] = src[IX(1, y - 1)];
        a[2] = src[IX(0, y    )];
        a[3] = src[IX(1, y    )];
        a[4] = src[IX(0, y + 1)];
        a[5] = src[IX(1, y + 1)];
        *d++ = median6(a);

        for (int x = 1; x <= RW - 2; x++)
        {
            a[0] = src[IX(x - 1, y - 1)];
            a[1] = src[IX(x    , y - 1)];
            a[2] = src[IX(x + 1, y - 1)];
            a[3] = src[IX(x - 1, y    )];
            a[4] = src[IX(x    , y    )];
            a[5] = src[IX(x + 1, y    )];
            a[6] = src[IX(x - 1, y + 1)];
            a[7] = src[IX(x    , y + 1)];
            a[8] = src[IX(x + 1, y + 1)];
            *d++ = median9(a);
        }

        // rightmost pixel
        a[0] = src[IX(RW - 2, y - 1)];
        a[1] = src[IX(RW - 1, y - 1)];
        a[2] = src[IX(RW - 2, y    )];
        a[3] = src[IX(RW - 1, y    )];
        a[4] = src[IX(RW - 2, y + 1)];
        a[5] = src[IX(RW - 1, y + 1)];
        *d++ = median6(a);
    }

    // bottom row
    d = &dst[IX(0, RH - 1)];

    // bottom-left corner
    a[0] = src[IX(0, RH - 2)];
    a[1] = src[IX(1, RH - 2)];
    a[2] = src[IX(0, RH - 1)];
    a[3] = src[IX(1, RH - 1)];
    *d++ = median4(a);

    // bottom row middle pixels
    for (int x = 1; x <= RW - 2; x++)
    {
        a[0] = src[IX(x - 1, RH - 2)];
        a[1] = src[IX(x    , RH - 2)];
        a[2] = src[IX(x + 1, RH - 2)];
        a[3] = src[IX(x - 1, RH - 1)];
        a[4] = src[IX(x    , RH - 1)];
        a[5] = src[IX(x + 1, RH - 1)];
        *d++ = median6(a);
    }

    // bottom-right corner
    a[0] = src[IX(RW - 2, RH - 2)];
    a[1] = src[IX(RW - 1, RH - 2)];
    a[2] = src[IX(RW - 2, RH - 1)];
    a[3] = src[IX(RW - 1, RH - 1)];
    *d = median4(a);

#undef IX
}

static unsigned short MedianBorderingPixels(const usImage& img, int x, int y)
{
    unsigned short array[8];
    int const xsize = img.Size.GetWidth();
    int const ysize = img.Size.GetHeight();

    if (x > 0 && y > 0 && x < xsize - 1 && y < ysize - 1)
    {
        array[0] = img.ImageData[(x-1) + (y-1) * xsize];
        array[1] = img.ImageData[(x)   + (y-1) * xsize];
        array[2] = img.ImageData[(x+1) + (y-1) * xsize];
        array[3] = img.ImageData[(x-1) + (y)   * xsize];
        array[4] = img.ImageData[(x+1) + (y)   * xsize];
        array[5] = img.ImageData[(x-1) + (y+1) * xsize];
        array[6] = img.ImageData[(x)   + (y+1) * xsize];
        array[7] = img.ImageData[(x+1) + (y+1) * xsize];
        return median8(array);
    }

    if (x == 0 && y > 0 && y < ysize - 1)
    {
        // On left edge
        array[0] = img.ImageData[(x)     + (y - 1) * xsize];
        array[1] = img.ImageData[(x)     + (y + 1) * xsize];
        array[2] = img.ImageData[(x + 1) + (y - 1) * xsize];
        array[3] = img.ImageData[(x + 1) + (y)     * xsize];
        array[4] = img.ImageData[(x + 1) + (y + 1) * xsize];
        return median5(array);
    }

    if (x == xsize - 1 && y > 0 && y < ysize - 1)
    {
        // On right edge
        array[0] = img.ImageData[(x)     + (y - 1) * xsize];
        array[1] = img.ImageData[(x)     + (y + 1) * xsize];
        array[2] = img.ImageData[(x - 1) + (y - 1) * xsize];
        array[3] = img.ImageData[(x - 1) + (y)     * xsize];
        array[4] = img.ImageData[(x - 1) + (y + 1) * xsize];
        return median5(array);
    }

    if (y == 0 && x > 0 && x < xsize - 1)
    {
        // On bottom edge
        array[0] = img.ImageData[(x - 1) + (y)     * xsize];
        array[1] = img.ImageData[(x - 1) + (y + 1) * xsize];
        array[2] = img.ImageData[(x)     + (y + 1) * xsize];
        array[3] = img.ImageData[(x + 1) + (y)     * xsize];
        array[4] = img.ImageData[(x + 1) + (y + 1) * xsize];
        return median5(array);
    }

    if (y == ysize - 1 && x > 0 && x < xsize - 1)
    {
        // On top edge
        array[0] = img.ImageData[(x - 1) + (y)     * xsize];
        array[1] = img.ImageData[(x - 1) + (y - 1) * xsize];
        array[2] = img.ImageData[(x)     + (y - 1) * xsize];
        array[3] = img.ImageData[(x + 1) + (y)     * xsize];
        array[4] = img.ImageData[(x + 1) + (y - 1) * xsize];
        return median5(array);
    }

    if (x == 0 && y == 0)
    {
        // At lower left corner
        array[0] = img.ImageData[(x + 1) + (y)     * xsize];
        array[1] = img.ImageData[(x)     + (y + 1) * xsize];
        array[2] = img.ImageData[(x + 1) + (y + 1) * xsize];
    }
    else if (x == 0 && y == ysize - 1)
    {
        // At upper left corner
        array[0] = img.ImageData[(x + 1) + (y)     * xsize];
        array[1] = img.ImageData[(x)     + (y - 1) * xsize];
        array[2] = img.ImageData[(x + 1) + (y - 1) * xsize];
    }
    else if (x == xsize - 1 && y == ysize - 1)
    {
        // At upper right corner
        array[0] = img.ImageData[(x - 1) + (y)     * xsize];
        array[1] = img.ImageData[(x)     + (y - 1) * xsize];
        array[2] = img.ImageData[(x - 1) + (y - 1) * xsize];
    }
    else if (x == xsize - 1 && y == 0)
    {
        // At lower right corner
        array[0] = img.ImageData[(x - 1) + (y)     * xsize];
        array[1] = img.ImageData[(x)     + (y + 1) * xsize];
        array[2] = img.ImageData[(x - 1) + (y + 1) * xsize];
    }
    else
    {
        // unreachable
        return 0;
    }

    return median3(array);
}

bool SquarePixels(usImage& img, float xsize, float ysize)
{
    // Stretches one dimension to square up pixels
    if (!img.ImageData)
        return true;

    if (xsize <= ysize)
        return false;

    // Move the existing data to a temp image
    usImage tempimg;
    if (tempimg.Init(img.Size))
    {
        pFrame->Alert(_("Memory allocation error"));
        return true;
    }
    tempimg.SwapImageData(img);

    // if X > Y, when viewing stock, Y is unnaturally stretched, so stretch X to match
    double ratio = ysize / xsize;
    int newsize = ROUND((double) tempimg.Size.GetWidth() / ratio);  // make new image correct size
    img.Init(newsize,tempimg.Size.GetHeight());
    unsigned short *optr = img.ImageData;
    int linesize = tempimg.Size.GetWidth();  // size of an original line
    for (int y = 0; y < img.Size.GetHeight(); y++)
    {
        for (int x = 0; x < newsize; x++, optr++)
        {
            double oldposition = x * ratio;
            int ind1 = (unsigned int) floor(oldposition);
            int ind2 = (unsigned int) ceil(oldposition);
            if (ind2 > (tempimg.Size.GetWidth() - 1))
                ind2 = tempimg.Size.GetWidth() - 1;
            double weight = ceil(oldposition) - oldposition;
            *optr = (unsigned short) (((float) *(tempimg.ImageData + y*linesize + ind1) * weight) + ((float) *(tempimg.ImageData + y*linesize + ind1) * (1.0 - weight)));
        }
    }

    return false;
}

// Dark subtraction algorithm:
//     Pedestal = max(median(dark_frame) - median(light_frame), 0) - handles overall gain/gradient differences
//     Dark_corrected(i) = min(max(light(i) + pedestal - dark(i), 0), 65335)
bool Subtract(usImage& light, const usImage& dark)
{
    if (!light.ImageData || !dark.ImageData)
        return true;
    if (light.Size != dark.Size)
        return true;

    unsigned int left, top, width, height;
    unsigned short median_light, median_dark;
    median_light = light.MedianADU;    // median of frame or subframe

    if (!light.Subframe.IsEmpty())
    {
        left = light.Subframe.GetLeft();
        width = light.Subframe.GetWidth();
        top = light.Subframe.GetTop();
        height = light.Subframe.GetHeight();

        // compute the dark's median ADU within the subframe region
        unsigned int pixcnt = width * height;
        unsigned short *tmp = new unsigned short[pixcnt];
        const unsigned short *src = dark.ImageData + left + top * light.Size.GetWidth();
        unsigned short *dst = tmp;
        for (int y = 0; y < height; y++)
        {
            memcpy(dst, src, width * sizeof(unsigned short));
            src += light.Size.GetWidth();
            dst += width;
        }
        std::nth_element(tmp, tmp + pixcnt / 2, tmp + pixcnt);
        median_dark = tmp[pixcnt / 2];
        delete[] tmp;
    }
    else
    {
        left = top = 0;
        width = light.Size.GetWidth();
        height = light.Size.GetHeight();
        median_dark = dark.MedianADU; // use the pre-computed full frame median ADU
    }

    if (median_dark > median_light)
    {
        // dark was brighter than light
        light.Pedestal = median_dark - median_light;   // Needed for saturation detection in find-star
    }

    unsigned short *pl0 = &light.Pixel(left, top);
    const unsigned short *pd0 = &dark.Pixel(left, top);
    for (unsigned int r = 0; r < height;
         r++, pl0 += light.Size.GetWidth(), pd0 += light.Size.GetWidth())
    {
        unsigned short *const endl = pl0 + width;
        unsigned short *pl;
        const unsigned short *pd;
        for (pl = pl0, pd = pd0; pl < endl; pl++, pd++)
        {
            int newval = (int) *pl + light.Pedestal - (int) *pd;
            if (newval < 0) newval = 0; // hot pixel in dark frame isn't present in light frame
            else if (newval > 65535) newval = 65535;
            *pl = (unsigned short) newval;
        }
    }

    return false;
}

inline static unsigned short histo_median(unsigned short histo1[256], unsigned short histo2[65536], int n)
{
    n /= 2;
    unsigned int i;
    for (i = 0; i < 256; i++)
    {
        if (histo1[i] > n)
            break;
        n -= histo1[i];
    }
    for (i <<= 8; i < 65536; i++)
    {
        if (histo2[i] > n)
            break;
        n -= histo2[i];
    }
    return i;
}

static void MedianFilter(usImage& dst, const usImage& src, int halfWidth)
{
    dst.Init(src.Size);
    unsigned short *d = &dst.ImageData[0];

    int const width = src.Size.GetWidth();
    int const height = src.Size.GetHeight();

    for (int y = 0; y < height; y++)
    {
        int top = std::max(0, y - halfWidth);
        int bot = std::min(y + halfWidth, height - 1);
        int left = 0;
        int right = halfWidth;

        // TODO: we initialize the histogram at the start of each row, but we could make this faster
        // if we scan left to right, move down, scan right to left, move down so we never need to
        // reinitialize the histogram

        // initialize 2-level histogram
        unsigned short histo1[256];
        unsigned short histo2[65536];
        memset(&histo1[0], 0, sizeof(histo1));
        memset(&histo2[0], 0, sizeof(histo2));

        for (int j = top; j <= bot; j++)
        {
            const unsigned short *p = &src.Pixel(left, j);
            for (int i = left; i <= right; i++, p++)
            {
                ++histo1[*p >> 8];
                ++histo2[*p];
            }
        }
        unsigned int n = (right - left + 1) * (bot - top + 1);

        // read off first value for this row
        *d++ = histo_median(histo1, histo2, n);

        // loop across remaining columns for this row
        for (int i = 1; i < width; i++)
        {
            left = std::max(0, i - halfWidth);
            right = std::min(i + halfWidth, width - 1);

            // remove leftmost column
            if (left > 0)
            {
                const unsigned short *p = &src.Pixel(left - 1, top);
                for (int j = top; j <= bot; j++, p += width)
                {
                    --histo1[*p >> 8];
                    --histo2[*p];
                }
                n -= (bot - top + 1);
            }

            // add new column on right
            if (i + halfWidth <= width - 1)
            {
                const unsigned short *p = &src.Pixel(right, top);
                for (int j = top; j <= bot; j++, p += width)
                {
                    ++histo1[*p >> 8];
                    ++histo2[*p];
                }
                n += (bot - top + 1);
            }

            *d++ = histo_median(histo1, histo2, n);
        }
    }
}

struct ImageStatsWork
{
    ImageStats stats;
    usImage temp;
};

static void GetImageStats(ImageStatsWork& w, const usImage& img, const wxRect& win)
{
    w.temp.Init(img.Size);

    // Determine the mean and standard deviation
    double a = 0.0;
    double q = 0.0;
    double k = 1.0;
    double km1 = 0.0;

    const unsigned short *p0 = &img.Pixel(win.GetLeft(), win.GetTop());
    unsigned short *dst = &w.temp.ImageData[0];
    for (int y = 0; y < win.GetHeight(); y++)
    {
        const unsigned short *end = p0 + win.GetWidth();
        for (const unsigned short *p = p0; p < end; p++)
        {
            *dst++ = *p;
            double const x = (double) *p;
            double const a0 = a;
            a += (x - a) / k;
            q += (x - a0) * (x - a);
            km1 = k;
            k += 1.0;
        }
        p0 += img.Size.GetWidth();
    }

    w.stats.mean = a;
    w.stats.stdev = sqrt(q / km1);

    int winPixels = win.GetWidth() * win.GetHeight();
    unsigned short *tmp = &w.temp.ImageData[0];
    std::nth_element(tmp, tmp + winPixels / 2, tmp + winPixels);

    w.stats.median = tmp[winPixels / 2];

    // replace each pixel with the absolute deviation from the median
    unsigned short *p = tmp;
    for (int i = 0; i < winPixels; i++)
    {
        unsigned short ad = (unsigned short) std::abs((int) *p - (int) w.stats.median);
        *p++ = ad;
    }
    std::nth_element(tmp, tmp + winPixels / 2, tmp + winPixels);
    w.stats.mad = tmp[winPixels / 2];
}

void DefectMapDarks::BuildFilteredDark()
{
    enum { WINDOW = 15 };
    filteredDark.Init(masterDark.Size);
    MedianFilter(filteredDark, masterDark, WINDOW);
}

static wxString DefectMapMasterPath(int profileId)
{
    int inst = wxGetApp().GetInstanceNumber();
    return MyFrame::GetDarksDir() + PATHSEPSTR +
        wxString::Format("PHD2_defect_map_master%s_%d.fit", inst > 1 ? wxString::Format("_%d", inst) : "", profileId);
}
static wxString DefectMapMasterPath()
{
    return DefectMapMasterPath(pConfig->GetCurrentProfileId());
}

static wxString DefectMapFilterPath(int profileId)
{
    int inst = wxGetApp().GetInstanceNumber();
    return MyFrame::GetDarksDir() + PATHSEPSTR +
        wxString::Format("PHD2_defect_map_master_filt%s_%d.fit", inst > 1 ? wxString::Format("_%d", inst) : "", profileId);
}

static wxString DefectMapFilterPath()
{
    return DefectMapFilterPath(pConfig->GetCurrentProfileId());
}

void DefectMapDarks::SaveDarks(const wxString& notes)
{
    masterDark.Save(DefectMapMasterPath(), notes);
    filteredDark.Save(DefectMapFilterPath());
}

void DefectMapDarks::LoadDarks()
{
    masterDark.Load(DefectMapMasterPath());
    filteredDark.Load(DefectMapFilterPath());
}

struct BadPx
{
    unsigned short x;
    unsigned short y;
    int v;

    BadPx();
    BadPx(int x_, int y_, int v_) : x(x_), y(y_), v(v_) { }
    bool operator<(const BadPx& rhs) const { return v < rhs.v; }
};

typedef std::set<BadPx> BadPxSet;

struct DefectMapBuilderImpl
{
    DefectMapDarks *darks;
    ImageStatsWork w;
    wxArrayString mapInfo;
    int aggrCold;
    int aggrHot;
    BadPxSet coldPx;
    BadPxSet hotPx;
    BadPxSet::const_iterator coldPxThresh;
    BadPxSet::const_iterator hotPxThresh;
    unsigned int coldPxSelected;
    unsigned int hotPxSelected;
    bool threshValid;

    DefectMapBuilderImpl()
        :
        darks(0),
        aggrCold(100),
        aggrHot(100),
        threshValid(false)
    { }
};

DefectMapBuilder::DefectMapBuilder()
    : m_impl(new DefectMapBuilderImpl())
{
}

DefectMapBuilder::~DefectMapBuilder()
{
    delete m_impl;
}

inline static double AggrToSigma(int val)
{
    // Aggressiveness of 0 to 100 maps to signma factor from 8.0 to 0.125
    return exp2(3.0 - (6.0 / 100.0) * (double)val);
}

void DefectMapBuilder::Init(DefectMapDarks& darks)
{
    m_impl->darks = &darks;

    Debug.AddLine("DefectMapBuilder: Init");

    ::GetImageStats(m_impl->w, darks.masterDark,
        wxRect(0, 0, darks.masterDark.Size.GetWidth(), darks.masterDark.Size.GetHeight()));

    const ImageStats& stats = m_impl->w.stats;

    Debug.Write(wxString::Format("DefectMapBuilder: Dark N = %u Mean = %.f Median = %d Standard Deviation = %.f MAD=%d\n",
                                 darks.masterDark.NPixels, stats.mean, stats.median, stats.stdev, stats.mad));

    // load potential defects

    int thresh = (int)(AggrToSigma(100) * stats.stdev);

    Debug.Write(wxString::Format("DefectMapBuilder: load potential defects thresh = %d\n", thresh));

    usImage& dark = m_impl->darks->masterDark;
    usImage& medianFilt = m_impl->darks->filteredDark;

    m_impl->coldPx.clear();
    m_impl->hotPx.clear();

    for (int y = 0; y < dark.Size.GetHeight(); y++)
    {
        for (int x = 0; x < dark.Size.GetWidth(); x++)
        {
            int filt = (int) medianFilt.Pixel(x, y);
            int val = (int) dark.Pixel(x, y);
            int v = val - filt;
            if (v > thresh)
            {
                m_impl->hotPx.insert(BadPx(x, y, v));
            }
            else if (-v > thresh)
            {
                m_impl->coldPx.insert(BadPx(x, y, -v));
            }
        }
    }

    Debug.Write(wxString::Format("DefectMapBuilder: Loaded %d cold %d hot\n", m_impl->coldPx.size(), m_impl->hotPx.size()));
}

const ImageStats& DefectMapBuilder::GetImageStats() const
{
    return m_impl->w.stats;
}

void DefectMapBuilder::SetAggressiveness(int aggrCold, int aggrHot)
{
    m_impl->aggrCold = std::max(0, std::min(100, aggrCold));
    m_impl->aggrHot = std::max(0, std::min(100, aggrHot));
    m_impl->threshValid = false;
}

static void FindThresh(DefectMapBuilderImpl *impl)
{
    if (impl->threshValid)
        return;

    double multCold = AggrToSigma(impl->aggrCold);
    double multHot = AggrToSigma(impl->aggrHot);

    int coldThresh = (int) (multCold * impl->w.stats.stdev);
    int hotThresh = (int) (multHot * impl->w.stats.stdev);

    Debug.Write(wxString::Format("DefectMap: find thresholds aggr:(%d,%d) sigma:(%.1f,%.1f) px:(%+d,%+d)\n",
                                 impl->aggrCold, impl->aggrHot, multCold, multHot, -coldThresh, hotThresh));

    impl->coldPxThresh = impl->coldPx.lower_bound(BadPx(0, 0, coldThresh));
    impl->hotPxThresh = impl->hotPx.lower_bound(BadPx(0, 0, hotThresh));

    impl->coldPxSelected = std::distance(impl->coldPxThresh, impl->coldPx.end());
    impl->hotPxSelected = std::distance(impl->hotPxThresh, impl->hotPx.end());

    Debug.Write(wxString::Format("DefectMap: find thresholds found (%d,%d)\n", impl->coldPxSelected, impl->hotPxSelected));

    impl->threshValid = true;
}

int DefectMapBuilder::GetColdPixelCnt() const
{
    FindThresh(m_impl);
    return m_impl->coldPxSelected;
}

int DefectMapBuilder::GetHotPixelCnt() const
{
    FindThresh(m_impl);
    return m_impl->hotPxSelected;
}

inline static unsigned int emit_defects(DefectMap& defectMap, BadPxSet::const_iterator p0, BadPxSet::const_iterator p1, double stdev, int sign, bool verbose)
{
    unsigned int cnt = 0;
    for (BadPxSet::const_iterator it = p0; it != p1; ++it, ++cnt)
    {
        if (verbose)
        {
            int v = sign * it->v;
            Debug.Write(wxString::Format("DefectMap: defect @ (%d, %d) val = %d (%+.1f sigma)\n", it->x, it->y, v, stdev > 0.1 ? (double)v / stdev : 0.0));
        }
        defectMap.push_back(wxPoint(it->x, it->y));
    }
    return cnt;
}

void DefectMapBuilder::BuildDefectMap(DefectMap& defectMap, bool verbose) const
{
    wxArrayString& info = m_impl->mapInfo;

    double multCold = AggrToSigma(m_impl->aggrCold);
    double multHot = AggrToSigma(m_impl->aggrHot);
    const ImageStats& stats = m_impl->w.stats;

    info.Clear();
    info.push_back(wxString::Format("Generated: %s", wxDateTime::UNow().FormatISOCombined(' ')));
    info.push_back(wxString::Format("Camera: %s", pCamera->Name));
    info.push_back(wxString::Format("Dark Exposure Time: %d ms", m_impl->darks->masterDark.ImgExpDur));
    info.push_back(wxString::Format("Dark Frame Count: %d", m_impl->darks->masterDark.ImgStackCnt));
    info.push_back(wxString::Format("Aggressiveness, cold: %d", m_impl->aggrCold));
    info.push_back(wxString::Format("Aggressiveness, hot: %d", m_impl->aggrHot));
    info.push_back(wxString::Format("Sigma Thresh, cold: %.2f", multCold));
    info.push_back(wxString::Format("Sigma Thresh, hot: %.2f", multHot));
    info.push_back(wxString::Format("Mean: %.f", stats.mean));
    info.push_back(wxString::Format("Stdev: %.f", stats.stdev));
    info.push_back(wxString::Format("Median: %d", stats.median));
    info.push_back(wxString::Format("MAD: %d", stats.mad));

    int deltaCold = (int)(multCold * stats.stdev);
    int deltaHot = (int)(multHot * stats.stdev);

    info.push_back(wxString::Format("DeltaCold: %+d", -deltaCold));
    info.push_back(wxString::Format("DeltaHot: %+d", deltaHot));

    if (verbose) Debug.Write(wxString::Format("DefectMap: deltaCold = %+d deltaHot = %+d\n", -deltaCold, deltaHot));

    FindThresh(m_impl);

    defectMap.clear();
    unsigned int nr_cold = emit_defects(defectMap, m_impl->coldPxThresh, m_impl->coldPx.end(), stats.stdev, -1, verbose);
    unsigned int nr_hot = emit_defects(defectMap, m_impl->hotPxThresh, m_impl->hotPx.end(), stats.stdev, +1, verbose);

    if (verbose) Debug.Write(wxString::Format("New defect map created, count=%d (cold=%d, hot=%d)\n", defectMap.size(), nr_cold, nr_hot));
}

const wxArrayString& DefectMapBuilder::GetMapInfo() const
{
    return m_impl->mapInfo;
}

bool RemoveDefects(usImage& light, const DefectMap& defectMap)
{
    // Check to make sure the light frame is valid
    if (!light.ImageData)
        return true;

    if (!light.Subframe.IsEmpty())
    {
        // Step over each defect and replace the light value
        // with the median of the surrounding pixels
        for (DefectMap::const_iterator it = defectMap.begin(); it != defectMap.end(); ++it)
        {
            const wxPoint& pt = *it;
            // Check to see if we are within the subframe before correcting the defect
            if (light.Subframe.Contains(pt))
            {
                light.Pixel(pt.x, pt.y) = MedianBorderingPixels(light, pt.x, pt.y);
            }
        }
    }
    else
    {
        // Step over each defect and replace the light value
        // with the median of the surrounding pixels
        for (DefectMap::const_iterator it = defectMap.begin(); it != defectMap.end(); ++it)
        {
            int const x = it->x;
            int const y = it->y;

            if (x >= 0 && x < light.Size.GetWidth() && y >= 0 && y < light.Size.GetHeight())
            {
                light.Pixel(x, y) = MedianBorderingPixels(light, x, y);
            }
        }
    }

    return false;
}

wxString DefectMap::DefectMapFileName(int profileId)
{
    int inst = wxGetApp().GetInstanceNumber();
    return MyFrame::GetDarksDir() + PATHSEPSTR +
        wxString::Format("PHD2_defect_map%s_%d.txt", inst > 1 ? wxString::Format("_%d", inst) : "", profileId);
}

bool DefectMap::ImportFromProfile(int srcId, int destId)
{
    wxString sourceName;
    wxString destName;
    int rslt;

    sourceName = DefectMapFileName(srcId);
    destName = DefectMapFileName(destId);
    rslt = wxCopyFile(sourceName, destName, true);
    if (rslt != 1)
    {
        Debug.Write(wxString::Format("DefectMap::ImportFromProfile failed on defect map copy of %s to %s\n", sourceName, destName));
        return false;
    }
    sourceName = DefectMapMasterPath(srcId);
    destName = DefectMapMasterPath(destId);
    rslt = wxCopyFile(sourceName, destName, true);
    if (rslt != 1)
    {
        Debug.Write(wxString::Format("DefectMap::ImportFromProfile failed on defect map master dark copy of %s to %s\n", sourceName, destName));
        return false;
    }
    sourceName = DefectMapFilterPath(srcId);
    destName = DefectMapFilterPath(destId);
    rslt = wxCopyFile(sourceName, destName, true);
    if (rslt != 1)
    {
        Debug.Write(wxString::Format("DefectMap::ImportFromProfile failed on defect map master filtered dark copy of %s to %s\n", sourceName, destName));
        return false;
    }
    return (true);
}

bool DefectMap::DefectMapExists(int profileId, bool showAlert)
{
    bool bOk = false;

    if (wxFileExists(DefectMapFileName(profileId)))
    {
        wxString fName = DefectMapMasterPath(profileId);
        const wxSize& sensorSize = pCamera->DarkFrameSize();
        if (sensorSize == UNDEFINED_FRAME_SIZE)
        {
            bOk = true;
            Debug.AddLine("BPM check: undefined frame size for current camera");
        }
        else
        {
            fitsfile *fptr;
            int status = 0;  // CFITSIO status value MUST be initialized to zero!

            if (PHD_fits_open_diskfile(&fptr, fName, READONLY, &status) == 0)
            {
                long fsize[2];
                fits_get_img_size(fptr, 2, fsize, &status);
                if (status == 0 && fsize[0] == sensorSize.x && fsize[1] == sensorSize.y)
                    bOk = true;
                else
                {
                    Debug.AddLine(wxString::Format("BPM check: failed geometry check - fits status = %d, cam dimensions = {%d,%d}, "
                        " BPM dimensions = {%d,%d}", status, sensorSize.x, sensorSize.y, fsize[0], fsize[1]));
                    if (showAlert)
                        pFrame->Alert(_("Bad-pixel map does not match the camera in this profile - it needs to be replaced."));
                }

                PHD_fits_close_file(fptr);
            }
            else
                Debug.AddLine(wxString::Format("BPM check: fitsio error on open_diskfile = %d", status));
        }
    }

    return bOk;
}

void DefectMap::Save(const wxArrayString& info) const
{
    wxString filename = DefectMapFileName(m_profileId);
    wxFileOutputStream oStream(filename);
    wxTextOutputStream outText(oStream);

    if (oStream.GetLastError() != wxSTREAM_NO_ERROR)
    {
        Debug.AddLine(wxString::Format("Failed to save defect map to %s", filename));
        return;
    }

    outText << "# PHD2 Defect Map v1\n";

    for (wxArrayString::const_iterator it = info.begin(); it != info.end(); ++it)
    {
        outText << "# " << *it << "\n";
    }
    outText << "# Defect count: " << ((unsigned int) size()) << "\n";

    for (const_iterator it = begin(); it != end(); ++it)
    {
        outText << it->x << " " << it->y << "\n";
    }

    oStream.Close();
    Debug.AddLine(wxString::Format("Saved defect map to %s", filename));
}

DefectMap::DefectMap()
    : m_profileId(pConfig->GetCurrentProfileId())
{
}

DefectMap::DefectMap(int profileId)
    : m_profileId(profileId)
{
}

bool DefectMap::FindDefect(const wxPoint& pt) const
{
    return std::find(begin(), end(), pt) != end();
}

void DefectMap::AddDefect(const wxPoint& pt)
{
    // first add the point
    push_back(pt);

    wxString filename = DefectMapFileName(m_profileId);
    wxFile file(filename, wxFile::write_append);
    wxFileOutputStream oStream(file);
    wxTextOutputStream outText(oStream);

    if (oStream.GetLastError() != wxSTREAM_NO_ERROR)
    {
        Debug.AddLine(wxString::Format("Failed to save defect map to %s", filename));
        return;
    }

    outText << pt.x << " " << pt.y << "\n";

    oStream.Close();
    Debug.AddLine(wxString::Format("Saved defect map to %s", filename));
}

DefectMap *DefectMap::LoadDefectMap(int profileId)
{
    wxString filename = DefectMapFileName(profileId);
    Debug.AddLine(wxString::Format("Loading defect map file %s", filename));

    if (!wxFileExists(filename))
    {
        Debug.AddLine(wxString::Format("Defect map file not found: %s", filename));
        return 0;
    }

    wxFileInputStream iStream(filename);
    wxTextInputStream inText(iStream);

    // Re-initialize the defect map and parse the defect map file
    if (iStream.GetLastError() != wxSTREAM_NO_ERROR)
    {
        Debug.AddLine(wxString::Format("Unexpected eof on defect map file %s", filename));
        return 0;
    }

    DefectMap *defectMap = new DefectMap(profileId);

    int linenum = 0;
    while (!inText.GetInputStream().Eof())
    {
        wxString line = inText.ReadLine();
        ++linenum;
        line.Trim(false); // trim leading whitespace
        if (line.IsEmpty())
            continue;
        if (line.StartsWith("#"))
            continue;

        wxStringTokenizer tok(line);
        wxString s1 = tok.GetNextToken();
        wxString s2 = tok.GetNextToken();
        long x, y;
        if (s1.ToLong(&x) && s2.ToLong(&y))
        {
            defectMap->push_back(wxPoint(x, y));
        }
        else
        {
            Debug.AddLine(wxString::Format("DefectMap: ignore junk on line %d: %s", linenum, line));
        }
    }

    Debug.AddLine(wxString::Format("Loaded %d defects", defectMap->size()));
    return defectMap;
}

void DefectMap::DeleteDefectMap(int profileId)
{
    wxString filename = DefectMapFileName(profileId);
    if (wxFileExists(filename))
    {
        Debug.AddLine("Removing defect map file: " + filename);
        wxRemoveFile(filename);
    }
}
