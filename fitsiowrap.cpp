/*
*  fitsiowrap.cpp
*  PHD Guiding
*
*  Created by Andy Galasso
*  Copyright (c) 2014 Andy Galasso
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
*    Neither the name of Craig Stark, Stark Labs,
*     Bret McKee, Dad Dog Development, Ltd, nor the names of its
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

class FitsFname
{
#ifdef __WINDOWS__
    char *m_str;
#else
    wxCharBuffer m_str;
#endif

public:
    FitsFname(const wxString& str, bool create, bool clobber);

    ~FitsFname() {
#ifdef __WINDOWS__
        delete[] m_str;
#endif
    }

    operator const char *() { return m_str; }
};

FitsFname::FitsFname(const wxString& path, bool create, bool clobber)
{
#ifdef __WINDOWS__

    if (create)
    {
        if (!clobber && wxFileExists(path))
        {
            m_str = new char[1];
            *m_str = 0;
            return;
        }

        int fd = wxOpen(path, O_BINARY | O_WRONLY | O_CREAT, wxS_DEFAULT);
        wxClose(fd);
    }

    // use the short DOS 8.3 path name to avoid problems converting UTF-16 filenames to the ANSI filenames expected by CFITTSIO

    DWORD shortlen = GetShortPathNameW(path.wc_str(), 0, 0);

    if (shortlen)
    {
        LPWSTR shortpath = new WCHAR[shortlen];
        GetShortPathNameW(path.wc_str(), shortpath, shortlen);
        int slen = WideCharToMultiByte(CP_OEMCP, WC_NO_BEST_FIT_CHARS, shortpath, shortlen, 0, 0, 0, 0);
        m_str = new char[slen + 1];
        char *str = m_str;
        if (create)
            *str++ = '!';
        WideCharToMultiByte(CP_OEMCP, WC_NO_BEST_FIT_CHARS, shortpath, shortlen, str, slen, 0, 0);
        delete[] shortpath;
    }
    else
    {
        m_str = new char[1];
        *m_str = 0;
    }

#else // __WINDOWS__

    if (clobber)
        m_str = (wxT("!") + path).fn_str();
    else
        m_str = path.fn_str();

#endif // __WINDOWS__
}

int PHD_fits_open_diskfile(fitsfile **fptr, const wxString& filename, int iomode, int *status)
{
    return fits_open_diskfile(fptr, FitsFname(filename, false, false), iomode, status);
}

int PHD_fits_create_file(fitsfile **fptr, const wxString& filename, bool clobber, int *status)
{
    return fits_create_file(fptr, FitsFname(filename, true, clobber), status);
}

void PHD_fits_close_file(fitsfile *fptr)
{
    int status = 0;
    fits_close_file(fptr, &status);
}
