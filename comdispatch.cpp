/*
 *  comdispatch.cpp
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

#include "phd.h"

// windows only ASCOM helper code
#if defined(__WINDOWS__)

#include "comdispatch.h"
#include <comdef.h>

wxString ExcepMsg(const EXCEPINFO& excep)
{
    if (excep.bstrSource || excep.bstrDescription)
        return wxString::Format("(%s) %s", excep.bstrSource, excep.bstrDescription);
    else
        return _("A COM Error occurred. There may be more info in the Debug Log.");
}

wxString ExcepMsg(const wxString& prefix, const EXCEPINFO& excep)
{
    return prefix + ":\n" + ExcepMsg(excep);
}

ExcepInfo::ExcepInfo()
{
    memset(this, 0, sizeof(*this));
}

inline static void FreeExcep(EXCEPINFO& ex)
{
    if (ex.bstrSource)
        SysFreeString(ex.bstrSource);
    if (ex.bstrDescription)
        SysFreeString(ex.bstrDescription);
    if (ex.bstrHelpFile)
        SysFreeString(ex.bstrHelpFile);
}

void ExcepInfo::Assign(const _com_error& err, const wxString& source)
{
    FreeExcep(*this);

    wCode = 0;
    wReserved = 0;
    bstrSource = SysAllocString(source.wc_str());
    bstrDescription = SysAllocString(err.ErrorMessage());
    bstrHelpFile = nullptr;
    dwHelpContext = 0;
    pvReserved = nullptr;
    pfnDeferredFillIn = nullptr;
    scode = err.Error();
}

void ExcepInfo::Assign(HRESULT hr, const wxString& source)
{
    Assign(_com_error(hr), source);
}

ExcepInfo::~ExcepInfo()
{
    FreeExcep(*this);
}

bool DispatchClass::dispid(DISPID *ret, IDispatch *idisp, OLECHAR *wname, ExcepInfo *excep)
{
    HRESULT hr = idisp->GetIDsOfNames(IID_NULL, &wname, 1, LOCALE_USER_DEFAULT, ret);
    if (FAILED(hr))
    {
        _com_error err(hr);
        Debug.AddLine(wxString::Format("dispid(%s): [%x] %s", wname, hr, err.ErrorMessage()));
        excep->Assign(err, wxString::Format(_("Driver error preparing to call %s"), wname));
    }
    return SUCCEEDED(hr);
}

bool DispatchClass::dispid_cached(DISPID *ret, IDispatch *idisp, OLECHAR *wname, ExcepInfo *excep)
{
    wxString name(wname);

    idmap_t::const_iterator it = m_idmap.find(name);
    if (it != m_idmap.end())
    {
        *ret = it->second;
        return true;
    }

    if (!dispid(ret, idisp, wname, excep))
        return false;

    m_idmap[name] = *ret;
    return true;
}

DispatchObj::DispatchObj()
    : m_class(0),
      m_idisp(0)
{
}

DispatchObj::DispatchObj(DispatchClass *cls)
    : m_class(cls),
      m_idisp(0)
{
}

DispatchObj::DispatchObj(IDispatch *idisp, DispatchClass *cls)
    : m_class(cls),
      m_idisp(idisp)
{
    if (m_idisp)
        m_idisp->AddRef();
}

DispatchObj::~DispatchObj()
{
    if (m_idisp)
        m_idisp->Release();
}

void DispatchObj::Attach(IDispatch *idisp, DispatchClass *cls)
{
    m_class = cls;
    if (m_idisp)
        m_idisp->Release();
    m_idisp = idisp;
}

bool DispatchObj::Create(OLECHAR *progid)
{
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(progid, &clsid)))
        return false;
    IDispatch *idisp;
    HRESULT hr;
    if (FAILED(hr = CoCreateInstance(clsid, NULL, CLSCTX_SERVER, IID_IDispatch, (LPVOID *)&idisp)))
    {
        Debug.AddLine(wxString::Format("CoCreateInstance: [%x] %s", hr, _com_error(hr).ErrorMessage()));
        return false;
    }
    m_idisp = idisp;
    return true;
}

bool DispatchObj::GetDispatchId(DISPID *ret, OLECHAR *name)
{
    if (m_class)
        return m_class->dispid_cached(ret, m_idisp, name, &m_excep);
    else
        return DispatchClass::dispid(ret, m_idisp, name, &m_excep);
}

bool DispatchObj::GetProp(Variant *res, DISPID dispid)
{
    DISPPARAMS dispParms;
    dispParms.cArgs = 0;
    dispParms.rgvarg = NULL;
    dispParms.cNamedArgs = 0;
    dispParms.rgdispidNamedArgs = NULL;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dispParms, res, &m_excep, NULL);
    if (FAILED(hr))
        Debug.AddLine(wxString::Format("invoke: [%x] %s", hr, _com_error(hr).ErrorMessage()));
    return SUCCEEDED(hr);
}

bool DispatchObj::GetProp(Variant *res, OLECHAR *name)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;

    return GetProp(res, dispid);
}

bool DispatchObj::GetProp(Variant *res, OLECHAR *name, int arg)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;

    VARIANTARG rgvarg[1];
    rgvarg[0].vt = VT_INT;
    rgvarg[0].intVal = arg;
    DISPPARAMS dispParms;
    dispParms.cArgs = 1;
    dispParms.rgvarg = rgvarg;
    dispParms.cNamedArgs = 0;
    dispParms.rgdispidNamedArgs = NULL;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dispParms, res, &m_excep, NULL);

    if (FAILED(hr))
        Debug.AddLine(wxString::Format("getprop: [%x] %s", hr, _com_error(hr).ErrorMessage()));

    return SUCCEEDED(hr);
}

bool DispatchObj::PutProp(OLECHAR *name, OLECHAR *val)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;

    BSTR bs = SysAllocString(val);
    VARIANTARG rgvarg[1];
    rgvarg[0].vt = VT_BSTR;
    rgvarg[0].bstrVal = bs;
    DISPID dispidNamed = DISPID_PROPERTYPUT;
    DISPPARAMS dispParms;
    dispParms.cArgs = 1;
    dispParms.rgvarg = rgvarg;
    dispParms.cNamedArgs = 1;
    dispParms.rgdispidNamedArgs = &dispidNamed;
    Variant res;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dispParms, &res, &m_excep, NULL);
    SysFreeString(bs);

    if (FAILED(hr))
        Debug.AddLine(wxString::Format("putprop: [%x] %s", hr, _com_error(hr).ErrorMessage()));

    return SUCCEEDED(hr);
}

bool DispatchObj::PutProp(DISPID dispid, bool val)
{
    VARIANTARG rgvarg[1];
    rgvarg[0].vt = VT_BOOL;
    rgvarg[0].boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;
    DISPID dispidNamed = DISPID_PROPERTYPUT;
    DISPPARAMS dispParms;
    dispParms.cArgs = 1;
    dispParms.rgvarg = rgvarg;
    dispParms.cNamedArgs = 1;
    dispParms.rgdispidNamedArgs = &dispidNamed;
    Variant res;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dispParms, &res, &m_excep, NULL);
    if (FAILED(hr))
        Debug.AddLine(wxString::Format("putprop: [%x] %s", hr, _com_error(hr).ErrorMessage()));
    return SUCCEEDED(hr);
}

bool DispatchObj::PutProp(DISPID dispid, double val)
{
    VARIANTARG rgvarg[1];
    rgvarg[0].vt = VT_R8;
    rgvarg[0].dblVal = val;
    DISPID dispidNamed = DISPID_PROPERTYPUT;
    DISPPARAMS dispParms;
    dispParms.cArgs = 1;
    dispParms.rgvarg = rgvarg;
    dispParms.cNamedArgs = 1;
    dispParms.rgdispidNamedArgs = &dispidNamed;
    Variant res;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dispParms, &res, &m_excep, NULL);
    if (FAILED(hr))
        Debug.AddLine(wxString::Format("putprop: [%x] %s", hr, _com_error(hr).ErrorMessage()));
    return SUCCEEDED(hr);
}

bool DispatchObj::PutProp(DISPID dispid, LONG val)
{
    VARIANTARG rgvarg[1];
    rgvarg[0].vt = VT_I4;
    rgvarg[0].lVal = val;
    DISPID dispidNamed = DISPID_PROPERTYPUT;
    DISPPARAMS dispParms;
    dispParms.cArgs = 1;
    dispParms.rgvarg = rgvarg;
    dispParms.cNamedArgs = 1;
    dispParms.rgdispidNamedArgs = &dispidNamed;
    Variant res;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dispParms, &res, &m_excep, NULL);
    if (FAILED(hr))
        Debug.AddLine(wxString::Format("putprop: [%x] %s", hr, _com_error(hr).ErrorMessage()));
    return SUCCEEDED(hr);
}

bool DispatchObj::PutProp(OLECHAR *name, bool val)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;
    return PutProp(dispid, val);
}

bool DispatchObj::PutProp(OLECHAR* name, double val)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;
    return PutProp(dispid, val);
}

bool DispatchObj::InvokeMethod(Variant *res, OLECHAR *name, OLECHAR *arg)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;

    BSTR bs = SysAllocString(arg);
    VARIANTARG rgvarg[1];
    rgvarg[0].vt = VT_BSTR;
    rgvarg[0].bstrVal = bs;
    DISPPARAMS dispParms;
    dispParms.cArgs = 1;
    dispParms.rgvarg = rgvarg;
    dispParms.cNamedArgs = 0;
    dispParms.rgdispidNamedArgs = NULL;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dispParms, res, &m_excep, NULL);
    SysFreeString(bs);
    if (FAILED(hr))
        Debug.AddLine(wxString::Format("invoke(%s): [%x] %s", name, hr, _com_error(hr).ErrorMessage()));
    return SUCCEEDED(hr);
}

bool DispatchObj::InvokeMethod(Variant *res, DISPID dispid, double arg1, double arg2)
{
    VARIANTARG rgvarg[2];
    rgvarg[0].vt = VT_R8;
    rgvarg[0].dblVal = arg2;
    rgvarg[1].vt = VT_R8;
    rgvarg[1].dblVal = arg1;
    DISPPARAMS dispParms;
    dispParms.cArgs = 2;
    dispParms.rgvarg = rgvarg;
    dispParms.cNamedArgs = 0;
    dispParms.rgdispidNamedArgs = NULL;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dispParms, res, &m_excep, NULL);
    if (FAILED(hr))
        Debug.AddLine(wxString::Format("invoke: [%x] %s", hr, _com_error(hr).ErrorMessage()));
    return SUCCEEDED(hr);
}

bool DispatchObj::InvokeMethod(Variant *res, OLECHAR *name, double arg1, double arg2)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;
    return InvokeMethod(res, dispid, arg1, arg2);
}

bool DispatchObj::InvokeMethod(Variant *res, DISPID dispid)
{
    DISPPARAMS dispParms;
    dispParms.cArgs = 0;
    dispParms.rgvarg = NULL;
    dispParms.cNamedArgs = 0;
    dispParms.rgdispidNamedArgs = NULL;
    HRESULT hr = m_idisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dispParms, res, &m_excep, NULL);
    if (FAILED(hr))
        Debug.AddLine(wxString::Format("invoke: [%x] %s", hr, _com_error(hr).ErrorMessage()));
    return SUCCEEDED(hr);
}

bool DispatchObj::InvokeMethod(Variant *res, OLECHAR *name)
{
    DISPID dispid;
    if (!GetDispatchId(&dispid, name))
        return false;
    return InvokeMethod(res, dispid);
}

GITEntry::GITEntry()
    : m_pIGlobalInterfaceTable(0), m_dwCookie(0)
{
}

GITEntry::~GITEntry()
{
    Unregister();
}

void GITEntry::Register(IDispatch *idisp)
{
    if (!m_pIGlobalInterfaceTable)
    {
        // first find the global table
        HRESULT hr;
        if (FAILED(hr = ::CoCreateInstance(CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, IID_IGlobalInterfaceTable,
            (void **)&m_pIGlobalInterfaceTable)))
        {
            Debug.AddLine(wxString::Format("create global interface table: [%x] %s", hr, _com_error(hr).ErrorMessage()));
            throw ERROR_INFO("Cannot CoCreateInstance of Global Interface Table");
        }
    }

    // add the Interface to the global table. Any errors past this point need to remove the interface from the global table.
    HRESULT hr;
    if (FAILED(hr = m_pIGlobalInterfaceTable->RegisterInterfaceInGlobal(idisp, IID_IDispatch, &m_dwCookie)))
    {
        Debug.AddLine(wxString::Format("register in global interface table: [%x] %s", hr, _com_error(hr).ErrorMessage()));
        throw ERROR_INFO("Cannot register object in Global Interface Table");
    }
}

void GITEntry::Unregister()
{
    if (m_pIGlobalInterfaceTable)
    {
        if (m_dwCookie)
        {
            m_pIGlobalInterfaceTable->RevokeInterfaceFromGlobal(m_dwCookie);
            m_dwCookie = 0;
        }
        m_pIGlobalInterfaceTable->Release();
        m_pIGlobalInterfaceTable = 0;
    }
}

#endif // __WINDOWS__
