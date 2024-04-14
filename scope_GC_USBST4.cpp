/*
 *  GC_USBST4.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark.
 *  Copyright (c) 2009-2010 Craig Stark.
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
#ifdef GUIDE_GCUSBST4
//#include "scope_GC_USBST4.h"
#include    <sys/ioctl.h>
#include <termios.h>

#ifdef __linux__
#include <errno.h>
#endif

#define _U(String)  wxString(String, wxConvUTF8).c_str()

#ifdef __APPLE__

#include <IOKit/serial/IOSerialKeys.h>

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 120000
# define IOMainPort IOMasterPort
#endif

#define IOSSDATALAT    _IOW('T', 0, unsigned long)

static kern_return_t createSerialIterator(io_iterator_t *serialIterator)
{
    kern_return_t   kernResult;
    mach_port_t     masterPort;
    CFMutableDictionaryRef  classesToMatch;
    if ((kernResult = IOMainPort(0, &masterPort)) != KERN_SUCCESS)
    {
        printf("IOMainPort returned %d\n", kernResult);
        return kernResult;
    }
    if ((classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue)) == NULL)
    {
        printf("IOServiceMatching returned NULL\n");
        return kernResult;
    }
    // GC device is a "modem"
    CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey),
//                         CFSTR(kIOSerialBSDRS232Type));
                            CFSTR(kIOSerialBSDModemType));
    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, serialIterator);
    if (kernResult != KERN_SUCCESS)
    {
        printf("IOServiceGetMatchingServices returned %d\n", kernResult);
    }
    return kernResult;
}

static char *getRegistryString(io_object_t sObj, const char *propName)
{
    static char resultStr[256];
    //   CFTypeRef  nameCFstring;
    CFStringRef nameCFstring;
    resultStr[0] = 0;
    nameCFstring = (CFStringRef) IORegistryEntryCreateCFProperty(sObj,
                                                                 CFStringCreateWithCString(kCFAllocatorDefault, propName, kCFStringEncodingASCII),
                                                                 kCFAllocatorDefault, 0);
    if (nameCFstring) {
        CFStringGetCString(nameCFstring, resultStr, sizeof(resultStr),
                           kCFStringEncodingASCII);
        CFRelease(nameCFstring);
    }
    return resultStr;
}

#endif

Mount::MOVE_RESULT ScopeGCUSBST4::Guide(GUIDE_DIRECTION direction, int duration)
{
    char buf[16];
    switch (direction) {
        case NORTH:
            snprintf(buf, sizeof(buf), ":Mg0%4d#", duration);
            break;
        case SOUTH:
            snprintf(buf, sizeof(buf), ":Mg1%4d#", duration);
            break;
        case EAST:
            snprintf(buf, sizeof(buf), ":Mg2%4d#", duration);
            break;
        case WEST:
            snprintf(buf, sizeof(buf), ":Mg3%4d#", duration);
            break;
        case NONE:
            return MOVE_OK;
    }
    int num_bytes = write(portFID,buf,strlen(buf));
    if (num_bytes == -1) {
        pFrame->Alert(wxString::Format(_("Error writing to GC USB ST4: %s(%d)"),_U(strerror(errno)),errno));
    }
    WorkerThread::MilliSleep(duration + 50);
    return MOVE_OK;
}

bool ScopeGCUSBST4::Connect()
{
#ifdef __APPLE__

    wxArrayString DeviceNames;
    wxArrayString PortNames;
    char tempstr[256];
    io_iterator_t   theSerialIterator;
    io_object_t     theObject;

    if (createSerialIterator(&theSerialIterator) != KERN_SUCCESS) {
        wxMessageBox(_T("Error in finding serial ports"),_("Error"));
        return false;
    }
    bool found_device = false;
    while ((theObject = IOIteratorNext(theSerialIterator)) != 0)
    {
        // Find the device   should be usbmodem1*
        strcpy(tempstr, getRegistryString(theObject, kIOTTYDeviceKey));
        if (strstr(tempstr, "usbmodem"))
        {
            strcpy(tempstr, getRegistryString(theObject, kIODialinDeviceKey));
            found_device = true;
            break;
        }
    }
    IOObjectRelease(theSerialIterator); // Release the iterator.

    if (!found_device) {
        wxMessageBox("Could not find device - searched for usbmodem* to no avail...",_("Error"));
        return true;
    }

#endif   //__APPLE__

#ifdef  __linux__
       char tempstr[256] = "/dev/ttyACM0";
#endif

    portFID = open(tempstr, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (portFID == -1) { // error on opening
        wxMessageBox(wxString::Format(_T("Error opening serial port %s: %s(%d)"),
                                        _U(tempstr), _U(strerror(errno)), errno),_("Error"));
        return true;
    }
    ioctl(portFID, TIOCEXCL);
    fcntl(portFID, F_SETFL, 0);

    // Setup port
    struct termios  options;
    //options = gOriginalTTYAttrs;
    if (tcgetattr(portFID, &options) == -1) {
        wxMessageBox(_T("Error getting port options"),_("Error"));
        close(portFID);
        return true;
    }
    cfmakeraw(&options);
    options.c_cflag = CREAD | CLOCAL;
    options.c_cflag |= CS8;
    options.c_iflag |= IXON | IXOFF;
//  options.c_cc[VMIN] = 1;
 //   options.c_cc[VTIME] = 10;
    options.c_cc[VSTART]=0x11;
    options.c_cc[VSTOP]=0x13;
    cfsetspeed(&options, B9600);
    //options.c_cflag = 0x8b00;
    /*wxMessageBox(wxString::Format("SET termios: iFlag %x  oFlag %x  cFlag %x  lFlag %x  speed %d\n",
           options.c_iflag,
           options.c_oflag,
           options.c_cflag,
           options.c_lflag,
           options.c_ispeed));*/
    if (tcsetattr(portFID, TCSANOW, &options) == -1) {
        wxMessageBox(_T("Error setting port options"),_("Error"));
        close(portFID);
        return true;
    }
/*  int handshake;
    if (ioctl(portFID, TIOCMGET, &handshake) == -1) {
        wxMessageBox("Error getting port handshake");
        close(portFID);
        return false;
    }
    unsigned long mics = 1UL;
    if (ioctl(portFID, IOSSDATALAT, &mics) == -1) {
        wxMessageBox("Error setting port latency");
        close(portFID);
        return false;

    }*/
//  wxMessageBox(wxString::Format("%d",(int) cfgetispeed(&options)));

    // Init / check the device
    char buf[2];
    int num_bytes;

    // Send the '#' needed to kickstart things
    buf[0]='#';
    buf[1]=0;
    num_bytes = write(portFID,buf,1);
    if (num_bytes == -1) {
        wxMessageBox(wxString::Format(_T("Error during initial kickstart: %s(%d)"),_U(strerror(errno)),errno),_("Error"));
        close(portFID);
        return true;
    }

    // Do a quick check
    buf[0]=0x6;
    buf[1]=0;
    num_bytes = write(portFID,buf,1);
    if (num_bytes == -1) {
        wxMessageBox(wxString::Format(_T("Error during test polling of device: %s(%d)"),_U(strerror(errno)),errno),_("Error"));
        close(portFID);
        return true;
    }
    num_bytes = read(portFID,buf,1);
    if (num_bytes == -1) {
        wxMessageBox(_T("Error during test read of device"));
        close(portFID);
        return true;
    }
    if (buf[0] != 'A') {
        wxMessageBox(wxString::Format(_T("Device returned %x instead of %x on test poll"),buf[0],'A'));
        close(portFID);
        return true;
    }

    Scope::Connect();

    return false;
}

bool ScopeGCUSBST4::Disconnect() {
    if (portFID > 0) {
        close(portFID);
        portFID = 0;
    }

    Scope::Disconnect();
    return false;
}

#endif  //GUIDE_GCUSBST4
