/*
 *  comdispatch.h
 *  PHD Guiding
 *
 *  Created by Andy Galasso.
 *  Copyright (c) 2013 Andy Galasso.
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
#ifndef COMDISPATCH_INCLUDED
#define COMDISPATCH_INCLUDED

#if defined(__WINDOWS__)

class _com_error;

wxString ExcepMsg(const EXCEPINFO& excep);
wxString ExcepMsg(const wxString& prefix, const EXCEPINFO& excep);

struct Variant : public VARIANT
{
    Variant() { VariantInit(this); }
};

class ExcepInfo : public EXCEPINFO
{
    ExcepInfo& operator=(const ExcepInfo& rhs) = delete;
public:
    ExcepInfo();
    ~ExcepInfo();
    void Assign(HRESULT hr, const wxString& source);
    void Assign(const _com_error& err, const wxString& source);
};

typedef Variant VariantArg;

class DispatchClass
{
    typedef std::map<wxString, DISPID> idmap_t;
    idmap_t m_idmap;
public:
    DispatchClass() { }
    ~DispatchClass() { }
    static bool dispid(DISPID *ret, IDispatch *idisp, OLECHAR *name, ExcepInfo *excep);
    bool dispid_cached(DISPID *ret, IDispatch *idisp, OLECHAR *name, ExcepInfo *excep);
};

class DispatchObj
{
    DispatchClass *m_class;
    IDispatch *m_idisp;
    ExcepInfo m_excep;
public:
    DispatchObj();
    DispatchObj(DispatchClass *cls);
    DispatchObj(IDispatch *idisp, DispatchClass *cls);
    ~DispatchObj();
    void Attach(IDispatch *idisp, DispatchClass *cls);
    bool Create(OLECHAR *progid);
    bool GetDispatchId(DISPID *ret, OLECHAR *name);
    bool GetProp(Variant *res, DISPID dispid);
    bool GetProp(Variant *res, OLECHAR *name);
    bool GetProp(Variant *res, OLECHAR *name, int arg);
    bool PutProp(OLECHAR *name, OLECHAR *val);
    bool PutProp(DISPID dispid, bool val);
    bool PutProp(DISPID dispid, double val);
    bool PutProp(DISPID dispid, LONG val);
    bool PutProp(OLECHAR *name, bool val);
    bool PutProp(OLECHAR* name, double val);
    bool InvokeMethod(Variant *res, OLECHAR *name);
    bool InvokeMethod(Variant *res, OLECHAR *name, OLECHAR *arg);
    bool InvokeMethod(Variant *res, OLECHAR *name, double arg1, double arg2);
    bool InvokeMethod(Variant *res, DISPID dispid, double arg1, double arg2);
    bool InvokeMethod(Variant *res, DISPID dispid);
    const EXCEPINFO& Excep() const { return m_excep; }
    IDispatch *IDisp() const { return m_idisp; }
};

// IGlobalInterfaceTable wrapper
class GITEntry
{
    IGlobalInterfaceTable *m_pIGlobalInterfaceTable;
    DWORD m_dwCookie;
public:
    GITEntry();
    ~GITEntry();
    void Register(IDispatch *idisp);
    void Register(const DispatchObj& obj) { Register(obj.IDisp()); }
    void Unregister();
    bool IsRegistered() const { return m_dwCookie != 0; }
    IDispatch *Get() const
    {
        IDispatch *idisp = 0;
        if (m_dwCookie)
            m_pIGlobalInterfaceTable->GetInterfaceFromGlobal(m_dwCookie, IID_IDispatch, (LPVOID *)&idisp);
        return idisp;
    }
};

struct GITObjRef : public DispatchObj
{
    GITObjRef(const GITEntry& gitentry)
    {
        Attach(gitentry.Get(), 0);
    }
};

#endif
#endif
