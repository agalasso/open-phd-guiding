/*
 *  cam_KWIQGuider.cpp
 *  PHD Guiding
 *
 *  Adapted by jb from cam_openssag.cpp
 *  Created by Craig Stark.
 *  Copyright (c) 2009 Craig Stark.
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

#ifdef KWIQGUIDER_CAMERA

#include "cam_KWIQGuider.h"
#include "camera.h"

#include <KWIQGuider.h>

using namespace KWIQ;

class CameraKWIQGuider : public GuideCamera
{
    KWIQ::KWIQGuider *KWIQguider;
public:
    CameraKWIQGuider();
    bool Capture(int duration, usImage& img, int options, const wxRect& subframe) override;
    bool Connect(const wxString& camId) override;
    bool Disconnect() override;
    bool ST4PulseGuideScope(int direction, int duration) override;
    bool ST4HasNonGuiMove() override { return true; }
    bool HasNonGuiCapture() override { return true; }
    wxByte BitsPerPixel() override;
    bool GetDevicePixelSize(double *devPixelSize) override;
};

CameraKWIQGuider::CameraKWIQGuider()
{
    Connected = false;
    Name = _T("KWIQGuider (KWIQGuider)");
    FullSize = wxSize(1280, 1024);  // Current size of a full frame
    m_hasGuideOutput = true;  // Do we have an ST4 port?
    HasGainControl = true;  // Can we adjust gain?

    KWIQguider = new KWIQGuider();
}

wxByte CameraKWIQGuider::BitsPerPixel()
{
    return 8;
}

bool CameraKWIQGuider::Connect(const wxString& camId)
{
    if (!KWIQguider->Connect())
        return CamConnectFailed(_("Could not connect to KWIQGuider"));

    Connected = true;  // Set global flag for being connected
    return false;
}

bool CameraKWIQGuider::Disconnect()
{
    Connected = false;
    KWIQguider->Disconnect();
    return false;
}

bool CameraKWIQGuider::ST4PulseGuideScope(int direction, int duration)
{
    switch (direction) {
        case WEST:
            KWIQguider->Guide(guide_west, duration);
            break;
        case NORTH:
            KWIQguider->Guide(guide_north, duration);
            break;
        case SOUTH:
            KWIQguider->Guide(guide_south, duration);
            break;
        case EAST:
            KWIQguider->Guide(guide_east, duration);
            break;
        default: return true; // bad direction passed in
    }

    wxMilliSleep(duration + 10);

    return false;
}

bool CameraKWIQGuider::Capture(int duration, usImage& img, int options, const wxRect& subframe)
{
    int xsize = FullSize.GetWidth();
    int ysize = FullSize.GetHeight();

    if (img.Init(xsize,ysize)) {
        DisconnectWithAlert(CAPT_FAIL_MEMORY);
        return true;
    }

    KWIQguider->SetGain((int)(GuideCameraGain / 24));
//    KWIQguider->SetGain((int)(GuideCameraGain / 7));    // Won't exceed 15, not < 1

    struct raw_image *raw = KWIQguider->Expose(duration);

    for (unsigned int i = 0; i < raw->width * raw->height; i++) {
        img.ImageData[i] = (unsigned short) raw->data[i];
    }

    KWIQguider->FreeRawImage(raw);

    if (options & CAPTURE_SUBTRACT_DARK) SubtractDark(img);

    return false;
}

bool CameraKWIQGuider::GetDevicePixelSize(double *devPixelSize)
{
    *devPixelSize = 5.2;
    return false;
}

GuideCamera *KWIQGuiderCameraFactory::MakeKWIQGuiderCamera()
{
    return new CameraKWIQGuider();
}

#endif // KWIQGUIDER_CAMERA
