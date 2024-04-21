/*
 *  logger.cpp
 *  PHD Guiding
 *
 *  Created by Bruce Waddington
 *  Copyright (c) 2013 Bruce Waddington
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
#include "logger.h"
#include "wx/dir.h"
#include "wx/filefn.h"

Logger::Logger()
{
    m_Initialized = false;
}

Logger::~Logger()
{
}

// Default, safety-net implementation behind derived logger classes
bool Logger::ChangeDirLog(const wxString& newdir)
{
    return false;
}

// Return a valid default directory location for log files.  On
// Windows, this will normally be "My Documents\PHD2"
static wxString DefaultDir()
{
    wxStandardPathsBase& stdpath = wxStandardPaths::Get();
    wxString rslt = stdpath.GetDocumentsDir() + PATHSEPSTR + "PHD2";

    if (!wxDirExists(rslt))
        if (!wxFileName::Mkdir(rslt, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
            rslt = stdpath.GetDocumentsDir();             // should never happen

    return rslt;
}

// Return the current logging directory.  Design invaraint: returned
// string must always be a valid directory
const wxString& Logger::GetLogDir()
{
    if (!m_Initialized)
    {
        // One-time initialization at start-up
        wxString rslt;

        if (pConfig)
        {
            rslt = pConfig->Global.GetString("/frame/LogDir", wxEmptyString);
            if (rslt.empty())
                rslt = DefaultDir();                // user has never even looked at it
            else
                if (!wxDirExists(rslt))        // user might have deleted our old directories
                {
                    // will build entire hierarchy if needed
                    if (!wxFileName::Mkdir(rslt, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
                        rslt = DefaultDir();
                }
        }
        else
            rslt = DefaultDir();                    // shouldn't ever happen

        m_CurrentDir = rslt;
        m_Initialized = true;
    }

    return m_CurrentDir;
}

// Change the current logging directory, creating a new directory if
// needed. File system errors will result in a 'false' return and the
// current directory will be left unchanged.
bool Logger::SetLogDir(const wxString& dir)
{
    wxString newdir(dir);
    bool bOk = true;

    if (newdir.EndsWith(PATHSEPSTR))        // Need a standard form - no trailing separators
    {
        wxString stemp = PATHSEPSTR;
        newdir = newdir.substr(0, newdir.length() - stemp.length());
    }

    if (newdir.empty())                // Empty-string shorthand for "default location"
    {
        newdir = DefaultDir();
    }
    else
    {
        if (!wxDirExists(newdir))
        {
            // will build entire hierarchy; client handles errors
            bOk = wxFileName::Mkdir(newdir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        }
    }

    if (bOk)
    {
        m_CurrentDir = newdir;
        pConfig->Global.SetString("/frame/logdir", newdir);
    }

    return bOk;
}

// Clean up old log files in the directory.  Caller gives us the file
// glob - like PHD2_DebugLog*.txt - and the retention period.  Files
// older than that are removed.
void Logger::RemoveMatchingFiles(const wxString& filePattern, int DaysOld)
{
    int hitCount = 0;
    wxDateTime oldestDate = wxDateTime::UNow() + wxDateSpan::Days(-DaysOld);
    wxString lastFile = "<None>";

    try
    {
        wxArrayString results;
        int numFiles = wxDir::GetAllFiles(GetLogDir(), &results, filePattern, wxDIR_FILES);      // No sub-directories, just files

        for (int inx = 0; inx < numFiles; inx++)
        {
            wxDateTime stamp = wxFileModificationTime(results[inx]);
            if (stamp < oldestDate)
            {
                ++hitCount;
                lastFile = results[inx];            // For error logging
                wxRemoveFile(lastFile);
            }
        }
    }
    catch (const wxString& Msg)            // Eat the errors and press ahead, no place for UI here
    {
        Debug.Write(wxString::Format("Error cleaning up old log file %s: %s\n", lastFile, Msg));
    }

    if (hitCount > 0)
        Debug.Write(wxString::Format("Removed %d files of pattern: %s\n", hitCount, filePattern));
}

// Same as RemoveMatchingFiles but this applies to subdirectories in
// the logging directory.  Implemented to clean up the
// "CameraFrames..." diagnostic directories for image logging
void Logger::RemoveOldDirectories(const wxString& filePattern, int DaysOld)
{
    wxString dirRoot = GetLogDir();
    wxArrayString dirTargets;
    int hitCount = 0;
    wxDateTime oldestDate = wxDateTime::UNow() + wxDateSpan::Days(-DaysOld);
    wxString oldestDateStr = oldestDate.Format(_T("%Y-%m-%d_%H%M%S"));
    wxDir dir;
    wxString subdir;

    try
    {
        if (wxDirExists(dirRoot))
        {
            if (dir.Open(dirRoot))
            {
                bool more = dir.GetFirst(&subdir, filePattern, wxDIR_DIRS);
                while (more)
                {
                    wxString rslt = subdir.AfterFirst('_');
                    if (rslt < oldestDateStr)
                        dirTargets.Add(subdir);
                    more = dir.GetNext(&subdir);
                }
                dir.Close();
                for (unsigned int i = 0; i < dirTargets.GetCount(); i++)
                {
                    ++hitCount;
                    subdir = dirRoot + PATHSEPSTR + dirTargets[i];
                    bool didit = wxDir::Remove(subdir, wxPATH_RMDIR_RECURSIVE);
                    if (!didit)
                        Debug.Write(wxString::Format("Error removing old debug log directory: %s\n", subdir));
                }
            }
        }
    }
    catch (const wxString& Msg)            // Eat the errors and press ahead, no place for UI here
    {
        Debug.Write(wxString::Format("Error removing old debug log directory %s: %s\n", subdir, Msg));
    }

    if (hitCount > 0)
        Debug.Write(wxString::Format("Removed %d directories of pattern: %s\n", hitCount, filePattern));
}
