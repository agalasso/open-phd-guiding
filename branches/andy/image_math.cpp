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

int dbl_sort_func (double *first, double *second) {
    if (*first < *second)
        return -1;
    else if (*first > *second)
        return 1;
    return 0;
}

int us_sort_func (const void *first, const void *second) {
    if (*(unsigned short *)first < *(unsigned short *)second)
        return -1;
    else if (*(unsigned short *)first > *(unsigned short *)second)
        return 1;
    return 0;
}

float CalcSlope(ArrayOfDbl& y) {
// Does a linear regression to calculate the slope
    int x, size;
    double s_xy, s_x, s_y, s_xx, nvalid;
    double retval;
    size = (int) y.GetCount();
    nvalid = 0.0;
    s_xy = 0.0;
    s_xx = 0.0;
    s_x = 0.0;
    s_y = 0.0;

    for (x=1; x<=size; x++) {
        if (1) {
            nvalid = nvalid + 1;
            s_xy = s_xy + (double) x * y[x-1];
            s_x = s_x + (double) x;
            s_y = s_y + y[x-1];
            s_xx = s_xx + (double) x * (double) x;
        }
    }

    retval = (nvalid * s_xy - (s_x * s_y)) / (nvalid * s_xx - (s_x * s_x));
    return (float) retval;
}

//
bool QuickLRecon(usImage& img)
{
    // Does a simple debayer of luminance data only -- sliding 2x2 window
    usImage Limg;
    int x, y;
    int xsize, ysize;
    unsigned short *ptr0, *ptr1;

    xsize = img.Size.GetWidth();
    ysize = img.Size.GetHeight();
    if (Limg.Init(xsize,ysize)) {
        pFrame->Alert(_("Memory allocation error"));
        return true;
    }
    for (y=0; y<ysize-1; y++) {
        for (x=0; x<xsize-1; x++) {
            Limg.ImageData[x+y*xsize] = (img.ImageData[x+y*xsize] + img.ImageData[x+1+y*xsize] + img.ImageData[x+(y+1)*xsize] + img.ImageData[x+1+(y+1)*xsize]) / 4;
        }
        Limg.ImageData[x+y*xsize]=Limg.ImageData[(x-1)+y*xsize];  // Last one in this row -- just duplicate
    }
    for (x=0; x<xsize; x++)
        Limg.ImageData[x+(ysize-1)*xsize]=Limg.ImageData[x+(ysize-2)*xsize];  // Last row -- just duplicate
    ptr0=img.ImageData;
    ptr1=Limg.ImageData;
    for (x=0; x<img.NPixels; x++, ptr0++, ptr1++)
        *ptr0=(*ptr1);
    //delete Limg;
    Limg.Init(0,0);

    return false;
}

bool Median3(usImage& img)
{
    return Median3(img.ImageData, img.Size.GetWidth(), img.Size.GetHeight());
}

inline static void swap(unsigned short& a, unsigned short& b)
{
    unsigned short const t = a;
    a = b;
    b = t;
}

static int partition(unsigned short *list, int left, int right, int pivotIndex)
{
    int pivotValue = list[pivotIndex];
    swap(list[pivotIndex], list[right]);  // Move pivot to end
    int storeIndex = left;
    for (int i = left; i < right; i++)
        if (list[i] < pivotValue)
        {
            if (i != storeIndex)
                swap(list[storeIndex], list[i]);
            ++storeIndex;
        }
    swap(list[right], list[storeIndex]); // Move pivot to its final place
    return storeIndex;
}

// Hoare's selection algorithm
static unsigned short select_kth(unsigned short *list, int left, int right, int k)
{
    while (true)
    {
        int pivotIndex = (left + right) / 2;  // select pivotIndex between left and right
        int pivotNewIndex = partition(list, left, right, pivotIndex);
        int pivotDist = pivotNewIndex - left + 1;
        if (pivotDist == k)
            return list[pivotNewIndex];
        else if (k < pivotDist)
            right = pivotNewIndex - 1;
        else {
            k = k - pivotDist;
            left = pivotNewIndex + 1;
        }
    }
}

bool Median3(unsigned short ImageData [], int xsize, int ysize)
{
    int x, y;
    unsigned short array[9];
    int NPixels = xsize * ysize;

    unsigned short *tmpimg = (unsigned short *) malloc(NPixels * sizeof(unsigned short));

    for (y=1; y<ysize-1; y++) {
        for (x=1; x<xsize-1; x++) {
            array[0] = ImageData[(x-1)+(y-1)*xsize];
            array[1] = ImageData[(x)+(y-1)*xsize];
            array[2] = ImageData[(x+1)+(y-1)*xsize];
            array[3] = ImageData[(x-1)+(y)*xsize];
            array[4] = ImageData[(x)+(y)*xsize];
            array[5] = ImageData[(x+1)+(y)*xsize];
            array[6] = ImageData[(x-1)+(y+1)*xsize];
            array[7] = ImageData[(x)+(y+1)*xsize];
            array[8] = ImageData[(x+1)+(y+1)*xsize];
            tmpimg[x+y*xsize] = select_kth(array, 0, 9-1, 5);
        }
        tmpimg[(xsize-1)+y*xsize] = ImageData[(xsize-1)+y*xsize];  // 1st & Last one in this row -- just grab from orig
        tmpimg[y*xsize] = ImageData[y*xsize];
    }
    for (x = 0; x < xsize; x++) {
        tmpimg[x+(ysize-1)*xsize] = ImageData[x+(ysize-1)*xsize];  // Last row -- just duplicate
        tmpimg[x] = ImageData[x];  // First row
    }

    memcpy(ImageData, tmpimg, NPixels * sizeof(unsigned short));
    free(tmpimg);

    return false;
}

unsigned short Median8(usImage& img, int x, int y) 
{
    unsigned short newValue;
    unsigned short array[8];
    int            xsize = img.Size.GetWidth();
    int            ysize = img.Size.GetHeight();

    if( (x>0) && (y>0) && (x<(xsize-1)) && (y<(ysize-1))) 
    {
        array[0] = img.ImageData[(x-1) + (y-1) * xsize];
        array[1] = img.ImageData[(x)   + (y-1) * xsize];
        array[2] = img.ImageData[(x+1) + (y-1) * xsize];
        array[3] = img.ImageData[(x-1) + (y)   * xsize];
        array[4] = img.ImageData[(x+1) + (y)   * xsize];
        array[5] = img.ImageData[(x-1) + (y+1) * xsize];
        array[6] = img.ImageData[(x)   + (y+1) * xsize];
        array[7] = img.ImageData[(x+1) + (y+1) * xsize];
        newValue = (select_kth(array, 0, 7, 4) + select_kth(array, 0, 7, 5)) / 2;
    } 
    else 
    {
        if( (x==0) && (y==0) ) 
        {
            // At lower left corner
            array[0] = img.ImageData[(x+1) + (y)   * xsize];
            array[1] = img.ImageData[(x)   + (y+1) * xsize];
            array[2] = img.ImageData[(x+1) + (y+1) * xsize];
            newValue = select_kth(array, 0, 2, 2);
        }
        if( (x==0) && (y==(ysize-1)) ) 
        {
            // At upper left corner
            array[0] = img.ImageData[(x+1) + (y)   * xsize];
            array[1] = img.ImageData[(x)   + (y-1) * xsize];
            array[2] = img.ImageData[(x+1) + (y-1) * xsize];
            newValue = select_kth(array, 0, 2, 2);
        }
        if( (x==(xsize-1)) && (y==(ysize-1)) ) 
        {
            // At upper right corner
            array[0] = img.ImageData[(x-1) + (y)   * xsize];
            array[1] = img.ImageData[(x)   + (y-1) * xsize];
            array[2] = img.ImageData[(x-1) + (y-1) * xsize];
            newValue = select_kth(array, 0, 2, 2);
        }
        if( (x==(xsize-1)) && (y==0) ) 
        {
            // At lower right corner
            array[0] = img.ImageData[(x-1) + (y)   * xsize];
            array[1] = img.ImageData[(x)   + (y+1) * xsize];
            array[2] = img.ImageData[(x-1) + (y+1) * xsize];
            newValue = select_kth(array, 0, 2, 2);
        }
        if( (x==0) && (y>0) && (y<(ysize-1)) ) 
        {
            // On left edge
            array[0] = img.ImageData[(x)   + (y-1) * xsize];
            array[1] = img.ImageData[(x)   + (y+1) * xsize];
            array[2] = img.ImageData[(x+1) + (y-1) * xsize];
            array[3] = img.ImageData[(x+1) + (y)   * xsize];
            array[4] = img.ImageData[(x+1) + (y+1) * xsize];
            newValue = select_kth(array, 0, 4, 3);
        }
        if( (x==(xsize-1)) && (y>0) && (y<(ysize-1)) ) 
        {
            // On right edge
            array[0] = img.ImageData[(x)   + (y-1) * xsize];
            array[1] = img.ImageData[(x)   + (y+1) * xsize];
            array[2] = img.ImageData[(x-1) + (y-1) * xsize];
            array[3] = img.ImageData[(x-1) + (y)   * xsize];
            array[4] = img.ImageData[(x-1) + (y+1) * xsize];
            newValue = select_kth(array, 0, 4, 3);
        }
        if( (y==0) && (x>0) && (x<(xsize-1)) ) 
        {
            // On bottom edge
            array[0] = img.ImageData[(x-1) + (y)   * xsize];
            array[1] = img.ImageData[(x-1) + (y+1) * xsize];
            array[2] = img.ImageData[(x)   + (y+1) * xsize];
            array[3] = img.ImageData[(x+1) + (y)   * xsize];
            array[4] = img.ImageData[(x+1) + (y+1) * xsize];
            newValue = select_kth(array, 0, 4, 3);
        }
        if( (y==(ysize-1)) && (x>0) && (x<(xsize-1)) ) 
        {
            // On top edge
            array[0] = img.ImageData[(x-1) + (y)   * xsize];
            array[1] = img.ImageData[(x-1) + (y-1) * xsize];
            array[2] = img.ImageData[(x)   + (y-1) * xsize];
            array[3] = img.ImageData[(x+1) + (y)   * xsize];
            array[4] = img.ImageData[(x+1) + (y-1) * xsize];
            newValue = select_kth(array, 0, 4, 3);
        }
    }

    return newValue;
}

void QuickSort(unsigned short *list, int left, int right) 
{
    if(left<right) 
    {
        int            l          = left+1;
        int            r          = right;
        unsigned short pivotValue = list[left];

        while(l<r) 
        {
            if(list[l]<=pivotValue)
                l++;
            else if(list[r]>=pivotValue)
                r--;
            else
                swap(list[l], list[r]);
        }
        if(list[l]<pivotValue) 
        {
            swap(list[l], list[left]);
            l--;
        } 
        else 
        {
            l--;
            swap(list[l], list[left]);
        }
        QuickSort(list, left, l);
        QuickSort(list, r, right);
    }
}

unsigned short ImageMedian(usImage& img) 
{
    int             xsize    = img.Size.GetWidth();
    int             ysize    = img.Size.GetHeight();
    int             NPixels  = xsize * ysize;
    int             mid      = NPixels / 2 ;
    unsigned short *tmpImage = (unsigned short *) malloc(NPixels * sizeof(unsigned short));
    int             x, y;
    int             median;

    // Duplicate Image data to temporary array
    for (y = 0; y<ysize; y++) 
    {
        for (x = 0; x<xsize; x++) 
        {
            tmpImage[x + y*xsize] = img.Pixel(x, y);
        }
    }

    QuickSort(tmpImage, 0, NPixels-1);

    if( (NPixels % 2)==0 ) 
    {
        median  = (tmpImage[mid-1] + tmpImage[mid]) / 2;
    } else 
    {
        median  = tmpImage[mid];
    }

    return median;
}

bool SquarePixels(usImage& img, float xsize, float ysize)
{
    // Stretches one dimension to square up pixels
    int x,y;
    int newsize;
    usImage tempimg;
    unsigned short *ptr;
    unsigned short *optr;
    double ratio, oldposition;

    if (!img.ImageData)
        return true;
    if (xsize == ysize) return false;  // nothing to do

    // Copy the existing data
    if (tempimg.Init(img.Size.GetWidth(), img.Size.GetHeight())) {
        pFrame->Alert(_("Memory allocation error"));
        return true;
    }
    ptr = tempimg.ImageData;
    optr = img.ImageData;
    for (x=0; x<img.NPixels; x++, ptr++, optr++) {
        *ptr=*optr;
    }

    float weight;
    int ind1, ind2, linesize;
    // if X > Y, when viewing stock, Y is unnaturally stretched, so stretch X to match
    if (xsize > ysize) {
        ratio = ysize / xsize;
        newsize = ROUND((float) tempimg.Size.GetWidth() * (1.0/ratio));  // make new image correct size
        img.Init(newsize,tempimg.Size.GetHeight());
        optr=img.ImageData;
        linesize = tempimg.Size.GetWidth();  // size of an original line
        for (y=0; y<img.Size.GetHeight(); y++) {
            for (x=0; x<newsize; x++, optr++) {
                oldposition = x * ratio;
                ind1 = (unsigned int) floor(oldposition);
                ind2 = (unsigned int) ceil(oldposition);
                if (ind2 > (tempimg.Size.GetWidth() - 1)) ind2 = tempimg.Size.GetWidth() - 1;
                weight = ceil(oldposition) - oldposition;
                *optr = (unsigned short) (((float) *(tempimg.ImageData + y*linesize + ind1) * weight) + ((float) *(tempimg.ImageData + y*linesize + ind1) * (1.0 - weight)));
            }
        }

    }
    return false;
}

bool Subtract(usImage& light, const usImage& dark)
{
    if ((!light.ImageData) || (!dark.ImageData))
        return true;
    if (light.NPixels != dark.NPixels)
        return true;

    unsigned int left, top, width, height;
    if (light.Subframe.GetWidth() > 0 && light.Subframe.GetHeight() > 0)
    {
        left = light.Subframe.GetLeft();
        width = light.Subframe.GetWidth();
        top = light.Subframe.GetTop();
        height = light.Subframe.GetHeight();
    }
    else
    {
        left = top = 0;
        width = light.Size.GetWidth();
        height = light.Size.GetHeight();
    }

    int mindiff = 65535;

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
            int diff = (int) *pl - (int) *pd;
            if (diff < mindiff)
                mindiff = diff;
        }
    }

    int offset = 0;
    if (mindiff < 0) // dark was lighter than light
        offset = -mindiff;

    pl0 = &light.Pixel(left, top);
    pd0 = &dark.Pixel(left, top);
    for (unsigned int r = 0; r < height;
         r++, pl0 += light.Size.GetWidth(), pd0 += light.Size.GetWidth())
    {
        unsigned short *const endl = pl0 + width;
        unsigned short *pl;
        const unsigned short *pd;
        for (pl = pl0, pd = pd0; pl < endl; pl++, pd++)
        {
            int newval = (int) *pl - (int) *pd + offset;
            if (newval < 0) newval = 0; // shouldn't hit this...
            else if (newval > 65535) newval = 65535;
            *pl = (unsigned short) newval;
        }
    }

    return false;
}

// DefectMap* CalculateDefectMap(usImage *dark, double dmSigmaFactor)
void CalculateDefectMap(usImage *dark, double dmSigmaFactor, DefectMap*& defectMap)
{
    int                   x, y, i;
    int                   pixelCnt = 0;
    double                sum = 0.0;
    int                   mean = 0;
    int                  *tmpimg = NULL;
    int                   tmpInt = 0;
    int                   median = 0;
    int                   min = 0;
    int                   max = 0;
    double                tmpFloat = 0.0;
    int                   stdev = 0;
    int                   clipLow = 0;
    int                   clipHigh = 65535;
    bool                  DMUseMedian = false;              // Vestigial - maybe use median instead of mean

    Debug.AddLine("Creating defect map...");
    // pixelCnt = dark->Size.GetHeight() * dark->Size.GetWidth();
    pixelCnt = dark->NPixels;

    if (DMUseMedian) {
        // Find the median of the image
        median = (int)ImageMedian(*dark);
        Debug.AddLine("Dark Median is = %d", median);
    }
    else {
        // Find the mean of the image
        sum = 0.0;
        for (y = 0; y<dark->Size.GetHeight(); y++) 
        {
            for (x = 0; x<dark->Size.GetWidth(); x++) 
            {
                sum += ((double)dark->Pixel(x, y));
            }
        }
        mean = (int)(sum / (double)pixelCnt);
        Debug.AddLine("Dark Mean is = %d", mean);
    }


    // Determine the standard deviation from the median or the mean, depending on user choice
    sum = 0.0;
    for (y = 0; y<dark->Size.GetHeight(); y++) 
    {
        for (x = 0; x<dark->Size.GetWidth(); x++)
        {
            if (DMUseMedian)
                tmpFloat = (double)(((int)dark->Pixel(x, y)) - median);
            else
                tmpFloat = (double)(((int)dark->Pixel(x, y)) - mean);
            sum += (tmpFloat * tmpFloat);
        }
    }
    stdev = (int)sqrt(sum / ((double)pixelCnt));
    Debug.AddLine("Dark Standard Deviation is = %d", stdev);

    // Find the clipping points beyond which the pixels will be considered defects
    int midpoint = (DMUseMedian ? median : mean);
    clipLow = midpoint - (dmSigmaFactor * stdev);
    clipHigh = midpoint + (dmSigmaFactor * stdev);
    if (clipLow<0) 
    {
        clipLow = 0;
    }
    if (clipHigh>65535) 
    {
        clipHigh = 65535;
    }

    // Make a first pass to count the number of defects
    i = 0;
    for (y = 0; y<dark->Size.GetHeight(); y++) 
    {
        for (x = 0; x<dark->Size.GetWidth(); x++) 
        {
            tmpInt = (int)(dark->Pixel(x, y));
            if ((tmpInt<clipLow) || (tmpInt>clipHigh)) 
            {
                i++;
            }
        }
    }

    // Allocate the defect map entries
    defectMap = (DefectMap *)malloc(sizeof(DefectMap));
    defectMap->numDefects = i;
    defectMap->defects = (Defect *)malloc(i * sizeof(Defect));
    Debug.AddLine("New defect map created, count=%d, sigmaX=%0.2f", i, dmSigmaFactor);

    // Assign the defect map entries
    i = 0;
    for (y = 0; y<dark->Size.GetHeight(); y++) 
    {
        for (x = 0; x<dark->Size.GetWidth(); x++) 
        {
            tmpInt = (int)(dark->Pixel(x, y));
            if ((tmpInt<clipLow) || (tmpInt>clipHigh)) 
            {
                defectMap->defects[i].x = x;
                defectMap->defects[i].y = y;
                i++;
            }
        }
    }
    Debug.AddLine("Defect map built");
}

bool DefectRemoval(usImage& light, DefectMap& defectMap) 
{
    int                   i, x, y;
    unsigned short       *pl = NULL;

    // Check to make sure the light frame is valid
    if (!light.ImageData)
        return true;

    if (light.Subframe.GetWidth() > 0 && light.Subframe.GetHeight() > 0)
    {
        // Determine the extents of the sub frame
        unsigned int llx, lly, urx, ury;
        llx = light.Subframe.GetLeft();
        lly = light.Subframe.GetTop();
        urx = llx + (light.Subframe.GetWidth() - 1);
        ury = lly + (light.Subframe.GetHeight() - 1);

        // Step over each defect and replace the light value
        // with the median of the surrounding pixels
        for (i = 0; i<defectMap.numDefects; i++)
        {
            x = defectMap.defects[i].x;
            y = defectMap.defects[i].y;
            // Check to see if we are within the subframe before correcting the defect
            if ((x >= llx) && (y >= lly) && (x <= urx) && (y <= ury)) {
                pl = &light.Pixel(x, y);
                *pl = Median8(light, x, y);
            }
        }
    }
    else
    {
        // Step over each defect and replace the light value
        // with the median of the surrounding pixels
        for (i = 0; i < defectMap.numDefects; i++)
        {
            x = defectMap.defects[i].x;
            y = defectMap.defects[i].y;
            pl = &light.Pixel(x, y);
            *pl = Median8(light, x, y);
        }
    }

    return false;
}
void AutoFindStar(usImage& img, int& xpos, int& ypos) {
    // returns x and y of best star or 0 in each if nothing good found
    float A, B1, B2, C1, C2, C3, D1, D2, D3;
//  int score, *scores;
    int x, y, i, linesize;
    unsigned short *uptr;

//  scores = new int[img.NPixels];
    linesize = img.Size.GetWidth();
    //  double PSF[6] = { 0.69, 0.37, 0.15, -0.1, -0.17, -0.26 };
    // A, B1, B2, C1, C2, C3, D1, D2, D3
    double PSF[14] = { 0.906, 0.584, 0.365, .117, .049, -0.05, -.064, -.074, -.094 };
    double mean;
    double PSF_fit;
    double BestPSF_fit = 0.0;
//  for (x=0; x<img.NPixels; x++)
//      scores[x] = 0;

    // OK, do seem to need to run 3x3 median first
    Median3(img);

    /* PSF Grid is:
        D3 D3 D3 D3 D3 D3 D3 D3 D3
        D3 D3 D3 D2 D1 D2 D3 D3 D3
        D3 D3 C3 C2 C1 C2 C3 D3 D3
        D3 D2 C2 B2 B1 B2 C2 D3 D3
        D3 D1 C1 B1 A  B1 C1 D1 D3
        D3 D2 C2 B2 B1 B2 C2 D3 D3
        D3 D3 C3 C2 C1 C2 C3 D3 D3
        D3 D3 D3 D2 D1 D2 D3 D3 D3
        D3 D3 D3 D3 D3 D3 D3 D3 D3

        1@A
        4@B1, B2, C1, and C3
        8@C2, D2
        48 * D3
        */
    for (y=40; y<(img.Size.GetHeight()-40); y++) {
        for (x=40; x<(linesize-40); x++) {
//          score = 0;
            A =  (float) *(img.ImageData + linesize * y + x);
            B1 = (float) *(img.ImageData + linesize * (y-1) + x) + (float) *(img.ImageData + linesize * (y+1) + x) + (float) *(img.ImageData + linesize * y + (x + 1)) + (float) *(img.ImageData + linesize * y + (x-1));
            B2 = (float) *(img.ImageData + linesize * (y-1) + (x-1)) + (float) *(img.ImageData + linesize * (y-1) + (x+1)) + (float) *(img.ImageData + linesize * (y+1) + (x + 1)) + (float) *(img.ImageData + linesize * (y+1) + (x-1));
            C1 = (float) *(img.ImageData + linesize * (y-2) + x) + (float) *(img.ImageData + linesize * (y+2) + x) + (float) *(img.ImageData + linesize * y + (x + 2)) + (float) *(img.ImageData + linesize * y + (x-2));
            C2 = (float) *(img.ImageData + linesize * (y-2) + (x-1)) + (float) *(img.ImageData + linesize * (y-2) + (x+1)) + (float) *(img.ImageData + linesize * (y+2) + (x + 1)) + (float) *(img.ImageData + linesize * (y+2) + (x-1)) +
                (float) *(img.ImageData + linesize * (y-1) + (x-2)) + (float) *(img.ImageData + linesize * (y-1) + (x+2)) + (float) *(img.ImageData + linesize * (y+1) + (x + 2)) + (float) *(img.ImageData + linesize * (y+1) + (x-2));
            C3 = (float) *(img.ImageData + linesize * (y-2) + (x-2)) + (float) *(img.ImageData + linesize * (y-2) + (x+2)) + (float) *(img.ImageData + linesize * (y+2) + (x + 2)) + (float) *(img.ImageData + linesize * (y+2) + (x-2));
            D1 = (float) *(img.ImageData + linesize * (y-3) + x) + (float) *(img.ImageData + linesize * (y+3) + x) + (float) *(img.ImageData + linesize * y + (x + 3)) + (float) *(img.ImageData + linesize * y + (x-3));
            D2 = (float) *(img.ImageData + linesize * (y-3) + (x-1)) + (float) *(img.ImageData + linesize * (y-3) + (x+1)) + (float) *(img.ImageData + linesize * (y+3) + (x + 1)) + (float) *(img.ImageData + linesize * (y+3) + (x-1)) +
                (float) *(img.ImageData + linesize * (y-1) + (x-3)) + (float) *(img.ImageData + linesize * (y-1) + (x+3)) + (float) *(img.ImageData + linesize * (y+1) + (x + 3)) + (float) *(img.ImageData + linesize * (y+1) + (x-3));
            D3 = 0.0;
            uptr = img.ImageData + linesize * (y-4) + (x-4);
            for (i=0; i<9; i++, uptr++)
                D3 = D3 + *uptr;
            uptr = img.ImageData + linesize * (y-3) + (x-4);
            for (i=0; i<3; i++, uptr++)
                D3 = D3 + *uptr;
            uptr = uptr + 2;
            for (i=0; i<3; i++, uptr++)
                D3 = D3 + *uptr;
            D3 = D3 + (float) *(img.ImageData + linesize * (y-2) + (x-4)) + (float) *(img.ImageData + linesize * (y-2) + (x+4)) + (float) *(img.ImageData + linesize * (y-2) + (x-3)) + (float) *(img.ImageData + linesize * (y-2) + (x-3)) +
                (float) *(img.ImageData + linesize * (y+2) + (x-4)) + (float) *(img.ImageData + linesize * (y+2) + (x+4)) + (float) *(img.ImageData + linesize * (y+2) + (x - 3)) + (float) *(img.ImageData + linesize * (y+2) + (x-3)) +
                (float) *(img.ImageData + linesize * y + (x + 4)) + (float) *(img.ImageData + linesize * y + (x-4));

            uptr = img.ImageData + linesize * (y+4) + (x-4);
            for (i=0; i<9; i++, uptr++)
                D3 = D3 + *uptr;
            uptr = img.ImageData + linesize * (y+3) + (x-4);
            for (i=0; i<3; i++, uptr++)
                D3 = D3 + *uptr;
            uptr = uptr + 2;
            for (i=0; i<3; i++, uptr++)
                D3 = D3 + *uptr;

            mean = (A+B1+B2+C1+C2+C3+D1+D2+D3)/85.0;
            PSF_fit = PSF[0] * (A-mean) + PSF[1] * (B1 - 4.0*mean) + PSF[2] * (B2 - 4.0 * mean) +
                PSF[3] * (C1 - 4.0*mean) + PSF[4] * (C2 - 8.0*mean) + PSF[5] * (C3 - 4.0 * mean) +
                PSF[6] * (D1 - 4.0*mean) + PSF[7] * (D2 - 8.0*mean) + PSF[8] * (D3 - 48.0 * mean);


            if (PSF_fit > BestPSF_fit) {
                BestPSF_fit = PSF_fit;
                xpos = x;
                ypos = y;
            }

            /*          mean = (A + B1 + B2 + C1 + C2 + C3) / 25.0;
            PSF_fit = PSF[0] * (A-mean) + PSF[1] * (B1 - 4.0*mean) + PSF[2] * (B2 - 4.0 * mean) +
                PSF[3] * (C1 - 4.0*mean) + PSF[4] * (C2 - 8.0*mean) + PSF[5] * (C3 - 4.0 * mean);*/

    /*      score = (int) (100.0 * PSF_fit);

            if (PSF_fit > 0.0)
                scores[x+y*linesize] = (int) PSF_fit;
            else
                scores[x+y*linesize] = 0;
    */
            //          if ( ((B1 + B2) / 8.0) < 0.3 * A) // Filter hot pixels
            //              scores[x+y*linesize] = -1.0;



        }
    }
/*  score = 0;
    for (x=0; x<img.NPixels; x++) {
//      img.ImageData[x] = (unsigned short) scores[x];
        if (scores[x] > score) {
            score = scores[x];
            ypos = x / linesize;
            xpos = x - (ypos * linesize);
        }
    }
*/


}
