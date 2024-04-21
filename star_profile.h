/*
 *  star_profile.h
 *  PHD Guiding
 *
 *  Created by Sylvain Girard
 *  Copyright (c) 2013 Sylvain Girard
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
 *    Neither the name of Bret McKee, Dad Dog Development Ltd, nor the names of its
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
 *
 */

#ifndef STAR_PROFILE_H
#define STAR_PROFILE_H

class ProfileWindow : public wxWindow
{
public:
    ProfileWindow(wxWindow *parent);
    ~ProfileWindow(void);
    void UpdateData(const usImage *pImg, float xpos, float ypos);
    void OnPaint(wxPaintEvent& evt);
    void SetState(bool is_active);
    void OnLClick(wxMouseEvent& evt);
    void OnMouseWheel(wxMouseEvent& evt);

private:
    int mode; // 0= 2D profile of mid-row, 1=2D of avg_row, 2=2D of avg_col
    bool rawMode;
    int imageLeftMargin;
    int imageBottom;
    bool visible;

    // Rectangle where HFD label is drawn
    bool m_inFocusingMode;
    int m_labelX, m_labelY;
    int m_labelWidth, m_labelHeight;

    unsigned short *data;
    int horiz_profile[21], vert_profile[21], midrow_profile[21];
    DECLARE_EVENT_TABLE()
};

#endif