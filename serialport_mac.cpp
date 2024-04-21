/*
 *  serialport_mac.cpp
 *  PHD Guiding
 *
 *  Created by Craig Stark on 4/17/13.
 *  Copyright (c) 2013 Craig Stark.
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

#ifdef __APPLE__

#include "phd.h"
#include <IOKit/serial/IOSerialKeys.h>

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 120000
# define IOMainPort IOMasterPort
#endif

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

    CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey),CFSTR(kIOSerialBSDAllTypes));
    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, serialIterator);
    if (kernResult != KERN_SUCCESS)
    {
        printf("IOServiceGetMatchingServices returned %d\n", kernResult);
    }

    return kernResult;
}

static const char *getRegistryString(io_object_t sObj, const char *propName)
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

wxArrayString SerialPortMac::GetSerialPortList(void)
{
    wxArrayString ret;

    io_iterator_t iterator;
    kern_return_t result = createSerialIterator(&iterator);
    if (result == KERN_SUCCESS){
        io_object_t port;
        while ((port = IOIteratorNext(iterator)) != 0){
            const char* name = getRegistryString(port,kIOCalloutDeviceKey);
            if (name){
                ret.Add(name);
            }
            IOObjectRelease(port);
        }
        IOObjectRelease(iterator);
    }

    return ret;
}

#endif // _APPLE_
