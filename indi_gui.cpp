/*
 *    indi_gui.cpp
 *    PHD Guiding
 *
 *    Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
 *
 *    Redraw for libindi/baseclient by Patrick Chevalley
 *    Copyright (c) 2014 Patrick Chevalley
 *    All rights reserved.
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 2.1 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Contact Information: gcx@phracturedblue.com <Geoffrey Hausheer>
 *******************************************************************************/

#ifdef _WIN32
#pragma warning(disable: 4996)
#endif

#include "phd.h"
#include "indi_gui.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wx/progdlg.h>

/*
 *  Status LED
 */
class IndiStatus : public wxLed
{
    public:
        IndiStatus(wxWindow *parent, wxWindowID id, IPState state) : wxLed(parent, id)
        {
            SetState(state);
            Enable();
        }

        void SetState(int state)
        {
            static const char indi_state[4][6] =
            {
                "Idle",
                "Ok",
                "Busy",
                "Alert",
            };
            switch(state)
            {
                case IPS_IDLE:
                    SetColor("808080");
                    break;
                case IPS_OK:
                    SetColor("008000");
                    break;
                case IPS_BUSY:
                    SetColor("FFFF00");
                    break;
                case IPS_ALERT:
                    SetColor("FF0000");
                    break;
            }
            SetToolTip(wxString::FromAscii(indi_state[state]));
        }
};

/*
 *  A device page and related properties
 */
class IndiDev
{
    public:
        wxNotebook *page;
        INDI::BaseDevice dp;
        PtrHash groups;
        PtrHash properties;
};

/*
 *  Property Information
 */
class IndiProp
{
    public:
        wxString PropName;
        PtrHash ctrl;
        PtrHash entry;
        IndiStatus *state;
        wxStaticText *name;
        wxPanel *page;
        wxPanel *panel;
        wxGridBagSizer *gbs;
        INDI::Property property;
        IndiDev *idev;
};

enum
{
    SWITCH_CHECKBOX,
    SWITCH_BUTTON,
    SWITCH_COMBOBOX,
};

#define POS(r, c)        wxGBPosition(r,c)
#define SPAN(r, c)       wxGBSpan(r,c)

wxDEFINE_EVENT(INDIGUI_THREAD_NEWDEVICE_EVENT, wxThreadEvent);
wxDEFINE_EVENT(INDIGUI_THREAD_NEWPROPERTY_EVENT, wxThreadEvent);
wxDEFINE_EVENT(INDIGUI_THREAD_UPDATEPROPERTY_EVENT, wxThreadEvent);
wxDEFINE_EVENT(INDIGUI_THREAD_NEWMESSAGE_EVENT, wxThreadEvent);
wxDEFINE_EVENT(INDIGUI_THREAD_REMOVEPROPERTY_EVENT, wxThreadEvent);

wxBEGIN_EVENT_TABLE(IndiGui, wxDialog)
    EVT_CLOSE(IndiGui::OnQuit)
    EVT_THREAD(INDIGUI_THREAD_NEWDEVICE_EVENT, IndiGui::OnNewDeviceFromThread)
    EVT_THREAD(INDIGUI_THREAD_NEWPROPERTY_EVENT, IndiGui::OnNewPropertyFromThread)
    EVT_THREAD(INDIGUI_THREAD_UPDATEPROPERTY_EVENT, IndiGui::OnUpdatePropertyFromThread)
    EVT_THREAD(INDIGUI_THREAD_NEWMESSAGE_EVENT, IndiGui::OnNewMessageFromThread)
    EVT_THREAD(INDIGUI_THREAD_REMOVEPROPERTY_EVENT, IndiGui::OnRemovePropertyFromThread)
wxEND_EVENT_TABLE()

//////////////////////////////////////////////////////////////////////
// Functions running in the INDI client thread
//////////////////////////////////////////////////////////////////////

void IndiGui::newDevice(INDI::BaseDevice dp)
{
    auto event = new wxThreadEvent(wxEVT_THREAD, INDIGUI_THREAD_NEWDEVICE_EVENT);
    event->SetPayload(dp);
    wxQueueEvent(this, event);
}

void IndiGui::newProperty(INDI::Property property)
{
    auto event = new wxThreadEvent(wxEVT_THREAD, INDIGUI_THREAD_NEWPROPERTY_EVENT);
    event->SetPayload(property);
    wxQueueEvent(this, event);
}

void IndiGui::updateProperty(INDI::Property property)
{
    auto event = new wxThreadEvent(wxEVT_THREAD, INDIGUI_THREAD_UPDATEPROPERTY_EVENT);
    event->SetPayload(property);
    wxQueueEvent(this, event);
}

void IndiGui::newMessage(INDI::BaseDevice dp, int messageID)
{
    wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, INDIGUI_THREAD_NEWMESSAGE_EVENT);
    event->SetString(dp.messageQueue(messageID));
    wxQueueEvent(this, event);
}

void IndiGui::removeProperty(INDI::Property property)
{
    if (property)
    {
        wxString devname =  wxString::FromAscii(property.getDeviceName());
        wxString groupname =  wxString::FromAscii(property.getGroupName());
        wxString propname =  wxString::FromAscii(property.getName());
        IndiDev *indiDev = (IndiDev *)devlist[devname];
        if (!indiDev) return;
        IndiProp *indiProp = (IndiProp *)indiDev->properties[propname];
        if (!indiProp) return;

        wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, INDIGUI_THREAD_REMOVEPROPERTY_EVENT);
        event->SetExtraLong((long) indiProp);
        wxQueueEvent(this, event);
    }
}

//////////////////////////////////////////////////////////////////////

void IndiGui::ConnectServer(const wxString& INDIhost, long INDIport)
{
    setServer(INDIhost.mb_str(wxConvUTF8), INDIport);
    connectServer();
}

void IndiGui::serverConnected()
{
    setBLOBMode(B_NEVER, "", nullptr);
    m_lastUpdate = wxGetUTCTimeMillis();
}

void IndiGui::serverDisconnected(int exit_code)
{
    if (m_deleted)
    {
        // nothing to do if we're getting the notification via the
        // disconnectServer() call from the destructor
        return;
    }

    if (wxThread::IsMain())
    {
        Destroy();
    }
    else
    {
        wxCloseEvent *event = new wxCloseEvent(wxEVT_CLOSE_WINDOW, GetId());
        event->SetEventObject(this);
        event->SetCanVeto(false);
        wxQueueEvent(this, event);
    }
}

void IndiGui::OnNewDeviceFromThread(wxThreadEvent& event)
{
    auto dp = event.GetPayload<INDI::BaseDevice>();
    //printf("newdevice from thread %s \n",dp->getDeviceName());
    auto name = dp.getDeviceName();
    wxString devname =  wxString::FromAscii(dp.getDeviceName());
    IndiDev *indiDev = new IndiDev();
    wxPanel *panel = new wxPanel(parent_notebook);
    indiDev->page = new wxNotebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
    wxBoxSizer *nb_sizer = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(nb_sizer);
    nb_sizer->Add(indiDev->page, 1,  wxEXPAND | wxALL);
    parent_notebook->AddPage(panel, devname);
    indiDev->dp = dp;
    devlist[devname] = indiDev;
    panel->Fit();
    sizer->Layout();
    Fit();
    m_lastUpdate = wxGetUTCTimeMillis();
}

void IndiGui::OnNewPropertyFromThread(wxThreadEvent& event)
{
    auto property = event.GetPayload<INDI::Property>();
    //printf("newproperty from thread %s %s %s\n",property->getDeviceName(),property->getGroupName(),property->getName());
    wxString devname =  wxString::FromAscii(property.getDeviceName());
    wxString groupname =  wxString::FromAscii(property.getGroupName());
    wxString propname =  wxString::FromAscii(property.getName());

    IndiProp *indiProp = new IndiProp();
    wxPanel *page;
    wxGridBagSizer *gbs;
    int next_free_row;
    IndiDev *indiDev = (IndiDev *)devlist[devname];
    if (! indiDev) return;
    indiProp->idev = indiDev;

    page = (wxPanel *)indiDev->groups[groupname];
    if (!page)
    {
        page = new wxPanel(indiDev->page);
        indiDev->page->AddPage(page, groupname);
        page->SetSizer(new wxGridBagSizer(0, 20));
        indiDev->groups[groupname] = page;
    }

    gbs = (wxGridBagSizer *)page->GetSizer();
    gbs->Layout();
    next_free_row = gbs->GetRows();
    BuildPropWidget(property, page, indiProp);

    gbs->Add(indiProp->state, POS(next_free_row, 0), SPAN(1, 1), wxALIGN_LEFT | wxALL);
    gbs->Add(indiProp->name, POS(next_free_row, 1), SPAN(1, 1), wxALIGN_LEFT | wxALL);
    gbs->Add(indiProp->panel, POS(next_free_row, 2), SPAN(1, 1), wxALIGN_LEFT | wxEXPAND | wxALL);
    gbs->Layout();
    page->Fit();
    panel->Fit();
    indiDev->properties[propname] = indiProp;
    indiDev->page->Fit();
    indiDev->page->Layout();
    indiDev->page->Show();
    sizer->Layout();
    Fit();
    m_lastUpdate = wxGetUTCTimeMillis();
}

void IndiGui::BuildPropWidget(INDI::Property property, wxPanel *parent, IndiProp *indiProp)
{
    wxString propname =  wxString::FromAscii(property.getName());
    wxString proplbl =  wxString::FromAscii(property.getLabel());
    if (! proplbl) proplbl = propname;
    auto proptype = property.getType();

    indiProp->page = parent;
    indiProp->panel = new wxPanel(parent);
    indiProp->gbs  = new wxGridBagSizer(0, 20);
    indiProp->panel->SetSizer(indiProp->gbs);

    indiProp->state = new IndiStatus(parent, wxID_ANY, property.getState());
    indiProp->name  = new wxStaticText(parent, wxID_ANY, proplbl);
    indiProp->PropName = propname;
    indiProp->property = property;

    switch (proptype)
    {
        case INDI_TEXT:
            CreateTextWidget(property, indiProp);
            break;
        case INDI_SWITCH:
            CreateSwitchWidget(property, indiProp);
            break;
        case INDI_NUMBER:
            CreateNumberWidget(property, indiProp);
            break;
        case INDI_LIGHT:
            CreateLightWidget(property, indiProp);
            break;
        case INDI_BLOB:
            CreateBlobWidget(property, indiProp);
            break;
        case INDI_UNKNOWN:
            CreateUnknowWidget(property, indiProp);
            break;
    }
    indiProp->gbs->Layout();
}

int IndiGui::GetSwitchType(ISwitchVectorProperty *svp)
{
    int num_props = svp->nsp;

    if (svp->r == ISR_NOFMANY)
        return SWITCH_CHECKBOX;

    if (num_props <= 4)
        return SWITCH_BUTTON;

    return SWITCH_COMBOBOX;
}

void IndiGui::CreateSwitchWidget(INDI::Property property, IndiProp *indiProp)
{
    //printf("CreateSwitchWidget\n");
    int guitype = GetSwitchType(property.getSwitch());

    switch (guitype)
    {
        case SWITCH_COMBOBOX:
            CreateSwitchCombobox(property.getSwitch(), indiProp);
            break;
        case SWITCH_CHECKBOX:
            CreateSwitchCheckbox(property.getSwitch(), indiProp);
            break;
        case SWITCH_BUTTON:
            CreateSwitchButton(property.getSwitch(), indiProp);
            break;
    }
}

void IndiGui::CreateSwitchCombobox(ISwitchVectorProperty *svp, IndiProp *indiProp)
{
    wxString *choices = new wxString[svp->nsp];
    wxPanel *p = indiProp->panel;
    wxGridBagSizer *gbs = indiProp->gbs;

    int idx = 0;
    for (int i = 0; i < svp->nsp; i++)
    {
        if(svp->sp[i].s == ISS_ON)
            idx = i;
        indiProp->ctrl[wxString::FromAscii(svp->sp[i].name)] = (void *) (intptr_t) i;
        wxString swlbl = wxString::FromAscii(svp->sp[i].label);
        if (swlbl.empty())
            swlbl = wxString::FromAscii(svp->sp[i].name);
        choices[i] = swlbl;
    }
    wxChoice *combo = new wxChoice(p, wxID_ANY, wxDefaultPosition, wxDefaultSize, svp->nsp, choices);
    combo->SetSelection(idx);
    combo->SetClientData(indiProp);
    Connect(combo->GetId(), wxEVT_COMMAND_CHOICE_SELECTED,
            wxCommandEventHandler(IndiGui::SetComboboxEvent));
    gbs->Add(combo, POS(0, 0), SPAN(1, 1), wxALIGN_LEFT | wxALL);
    indiProp->ctrl[wxString::FromAscii(svp->name)] = (void *) combo;
    delete [] choices;
}

void IndiGui::CreateSwitchCheckbox(ISwitchVectorProperty *svp, IndiProp *indiProp)
{
    wxPanel *p = indiProp->panel;
    wxGridBagSizer *gbs = indiProp->gbs;
    for (int pos = 0; pos < svp->nsp; pos++)
    {
        wxString swlbl = wxString::FromAscii(svp->sp[pos].label);
        if (swlbl.empty())
            swlbl = wxString::FromAscii(svp->sp[pos].name);
        wxCheckBox *button = new wxCheckBox(p, wxID_ANY, swlbl);
        indiProp->ctrl[wxString::FromAscii(svp->sp[pos].name)] = button;
        if (svp->sp[pos].s == ISS_ON)
            button->SetValue(true);
        button->SetClientData(indiProp);
        Connect(button->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
                wxCommandEventHandler(IndiGui::SetCheckboxEvent));
        gbs->Add(button, POS(pos / 4, pos % 4), SPAN(1, 1), wxALIGN_LEFT | wxALL);
    }
}

void IndiGui::CreateSwitchButton(ISwitchVectorProperty *svp, IndiProp *indiProp)
{
    wxPanel *p = indiProp->panel;
    wxGridBagSizer *gbs = indiProp->gbs;
    for (int pos = 0; pos < svp->nsp; pos++)
    {
        wxString swlbl = wxString::FromAscii(svp->sp[pos].label);
        if (swlbl.empty())
            swlbl = wxString::FromAscii(svp->sp[pos].name);
        wxToggleButton *button = new wxToggleButton(p, wxID_ANY, swlbl);
        indiProp->ctrl[wxString::FromAscii(svp->sp[pos].name)] = button;
        if (svp->sp[pos].s == ISS_ON)
            button->SetValue(true);
        button->SetClientData(indiProp);
        Connect(button->GetId(), wxEVT_COMMAND_TOGGLEBUTTON_CLICKED,
                wxCommandEventHandler(IndiGui::SetToggleButtonEvent));
        if (!allow_connect_disconnect && strcmp(svp->name, "CONNECTION") == 0)
        {
            button->Enable(false);
        }
        gbs->Add(button, POS(0, pos), SPAN(1, 1), wxALIGN_LEFT | wxALL);
    }
}

void IndiGui::CreateTextWidget(INDI::Property property, IndiProp *indiProp)
{
    ITextVectorProperty *tvp = property.getText();
    wxPanel *p = indiProp->panel;
    wxGridBagSizer *gbs = indiProp->gbs;

    int pos;
    for (pos = 0; pos < tvp->ntp; pos++)
    {
        gbs->Add(new wxStaticText(p, wxID_ANY, wxString::FromAscii(tvp->tp[pos].label)),
                 POS(pos, 0), SPAN(1, 1), wxALIGN_LEFT | wxALL);

        wxStaticText *value = new wxStaticText(p, wxID_ANY, wxString::FromAscii(tvp->tp[pos].text));
        indiProp->ctrl[wxString::FromAscii(tvp->tp[pos].name)] = value;
        gbs->Add(value, POS(pos, 1), SPAN(1, 1), wxALIGN_LEFT | wxALL);
        if (tvp->p != IP_RO)
        {
            wxTextCtrl *entry = new wxTextCtrl(p, wxID_ANY);
            indiProp->entry[wxString::FromAscii(tvp->tp[pos].name)] = entry;
            gbs->Add(entry, POS(pos, 2), SPAN(1, 1), wxALIGN_LEFT | wxEXPAND | wxALL);
        }
    }
    if (tvp->p != IP_RO)
    {
        wxButton *button = new wxButton(p, wxID_ANY, _("Set"));
        button->SetClientData(indiProp);
        Connect(button->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(IndiGui::SetButtonEvent));
        gbs->Add(button, POS(0, 3), SPAN(pos, 1), wxALIGN_LEFT | wxALL);
    }
}

void IndiGui::CreateNumberWidget(INDI::Property property, IndiProp *indiProp)
{
    INumberVectorProperty *nvp = property.getNumber();
    wxPanel *p = indiProp->panel;
    wxGridBagSizer *gbs = indiProp->gbs;

    int pos;
    for (pos = 0; pos < nvp->nnp; pos++)
    {
        gbs->Add(new wxStaticText(p, wxID_ANY, wxString::FromAscii(nvp->np[pos].label)),
                 POS(pos, 0), SPAN(1, 1), wxALIGN_LEFT | wxALL);

        wxStaticText *value = new wxStaticText(p, wxID_ANY, wxString::Format(_T("%f"), nvp->np[pos].value));
        indiProp->ctrl[wxString::FromAscii(nvp->np[pos].name)] = value;
        gbs->Add(value, POS(pos, 1), SPAN(1, 1), wxALIGN_LEFT | wxALL);
        if (nvp->p != IP_RO)
        {
            wxTextCtrl *entry = new wxTextCtrl(p, wxID_ANY);
            indiProp->entry[wxString::FromAscii(nvp->np[pos].name)] = entry;
            gbs->Add(entry, POS(pos, 2), SPAN(1, 1), wxALIGN_LEFT | wxEXPAND | wxALL);
        }
    }
    if (nvp->p != IP_RO)
    {
        wxButton *button = new wxButton(p, wxID_ANY, _("Set"));
        button->SetClientData(indiProp);
        Connect(button->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(IndiGui::SetButtonEvent));
        gbs->Add(button, POS(0, 3), SPAN(pos, 1), wxALIGN_LEFT | wxALL);
    }
}

void IndiGui::CreateLightWidget(INDI::Property property, IndiProp *indiProp)
{
    //printf("IndiGui: Unimplemented CreateLightWidget\n");
}

void IndiGui::CreateBlobWidget(INDI::Property property, IndiProp *indiProp)
{
    //printf("IndiGui: Unimplemented CreateBlobWidget\n");
}

void IndiGui::CreateUnknowWidget(INDI::Property property, IndiProp *indiProp)
{
    //printf("IndiGui: Unimplemented CreateUnknowWidget\n");
}

void IndiGui::OnUpdatePropertyFromThread(wxThreadEvent& event)
{
    auto property = event.GetPayload<INDI::Property>();

    switch (property.getType())
    {
        case INDI_NUMBER:
        {
            auto nvp = property.getNumber();
            wxString devname = wxString::FromAscii(nvp->device);
            wxString propname = wxString::FromAscii(nvp->name);
            IndiDev *indiDev = (IndiDev *) devlist[devname];
            IndiProp *indiProp = (IndiProp *) indiDev->properties[propname];
            for (int i = 0; i < nvp->nnp; i++)
            {
                void *st = indiProp->ctrl[wxString::FromAscii(nvp->np[i].name)];
                wxStaticText *ctrl = (wxStaticText *)st;
                ctrl->SetLabel(wxString::Format(wxT("%f"), nvp->np[i].value));
            }
            indiProp->state->SetState(nvp->s);
        }
        break;

        case INDI_TEXT:
        {
            auto tvp = property.getText();
            wxString devname =  wxString::FromAscii(tvp->device);
            wxString propname =  wxString::FromAscii(tvp->name);
            IndiDev *indiDev = (IndiDev *) devlist[devname];
            IndiProp *indiProp = (IndiProp *) indiDev->properties[propname];
            for (int i = 0; i < tvp->ntp; i++)
            {
                void *st = indiProp->ctrl[wxString::FromAscii(tvp->tp[i].name)];
                wxStaticText *ctrl = (wxStaticText *)st;
                ctrl->SetLabel(wxString::Format(wxT("%s"), tvp->tp[i].text));
            }
            indiProp->state->SetState(tvp->s);
        }
        break;

        case INDI_SWITCH:
        {
            auto svp = property.getSwitch();
            wxString devname = wxString::FromAscii(svp->device);
            wxString propname = wxString::FromAscii(svp->name);
            int swtype = GetSwitchType(svp);
            IndiDev *indiDev = (IndiDev *) devlist[devname];
            IndiProp *indiProp = (IndiProp *) indiDev->properties[propname];
            switch (swtype)
            {
                case SWITCH_COMBOBOX:
                {
                    int idx = 0;
                    for (int i = 0; i < svp->nsp; i++)
                    {
                        if (svp->sp[i].s == ISS_ON)
                            idx = i;
                    }
                    void *st = indiProp->ctrl[wxString::FromAscii(svp->name)];
                    wxChoice *combo = (wxChoice *)st;
                    combo->SetSelection(idx);
                    break;
                }
                case SWITCH_CHECKBOX:
                {
                    for (int i = 0; i < svp->nsp; i++)
                    {
                        void *st = indiProp->ctrl[wxString::FromAscii(svp->sp[i].name)];
                        wxCheckBox *button = (wxCheckBox *) st;
                        button->SetValue(svp->sp[i].s ? true : false);
                    }
                    break;
                }
                case SWITCH_BUTTON:
                {
                    for (int i = 0; i < svp->nsp; i++)
                    {
                        void *st = indiProp->ctrl[wxString::FromAscii(svp->sp[i].name)];
                        wxToggleButton *button = (wxToggleButton *) st;
                        button->SetValue(svp->sp[i].s ? true : false);
                    }
                    break;
                }
            }
        }
        break;
        default:
            break;
    }
}

void IndiGui::OnNewMessageFromThread(wxThreadEvent& event)
{
    textbuffer->SetInsertionPoint(0);
    textbuffer->WriteText(event.GetString());
    textbuffer->WriteText(_T("\n"));
}

void IndiGui::SetButtonEvent(wxCommandEvent& event)
{
    wxButton *button = (wxButton *)event.GetEventObject();
    if (!button) return;
    IndiProp *indiProp = (IndiProp *) button->GetClientData();
    if (!indiProp) return;

    switch (indiProp->property.getType())
    {
        case INDI_TEXT:
        {
            ITextVectorProperty *tvp = indiProp->property.getText();
            for (int i = 0; i < tvp->ntp; i++)
            {
                if (tvp->p != IP_RO)
                {
                    wxTextCtrl *entry = (wxTextCtrl *)(indiProp->entry[wxString::FromAscii(tvp->tp[i].name)]);
                    size_t buf_size = strlen(tvp->tp[i].text) + 1;
                    snprintf(tvp->tp[i].text, buf_size, "%s", entry->GetLineText(0).mb_str().data());
                }
            }
            sendNewText(tvp);
            break;
        }
        case INDI_NUMBER:
        {
            INumberVectorProperty *nvp = indiProp->property.getNumber();
            for (int i = 0; i < nvp->nnp; i++)
            {
                if (nvp->p != IP_RO)
                {
                    wxTextCtrl *entry = (wxTextCtrl *)(indiProp->entry[wxString::FromAscii(nvp->np[i].name)]);
                    entry->GetLineText(0).ToDouble(&nvp->np[i].value);
                }
            }
            sendNewNumber(nvp);
            break;
        }
        default:
            break;
    }
}

void IndiGui::SetToggleButtonEvent(wxCommandEvent& event)
{
    wxToggleButton *button = (wxToggleButton *)event.GetEventObject();
    if (!button) return;
    IndiProp *indiProp = (IndiProp *) button->GetClientData();
    if (!indiProp) return;
    ISwitchVectorProperty *svp = indiProp->property.getSwitch();

    if (!allow_connect_disconnect && strcmp(svp->name, "CONNECTION") == 0 )
    {
        // Prevent device disconnection from this window.
        // Use Gear manager instead.
        return;
    }

    wxString b_name;
    for (auto it = indiProp->ctrl.begin(); it != indiProp->ctrl.end(); ++it)
    {
        wxString key = it->first;
        wxToggleButton *value = (wxToggleButton *) it->second;
        if (value == button)
        {
            b_name = key;
            break;
        }
    }
    if (svp->r == ISR_1OFMANY)
    {
        for (int i = 0; i < svp->nsp; i++)
        {
            if (svp->sp[i].name == b_name)
                svp->sp[i].s = ISS_ON;
            else
                svp->sp[i].s = ISS_OFF;
        }
    }
    else
    {
        for (int i = 0; i < svp->nsp; i++)
        {
            if (svp->sp[i].name == b_name)
            {
                svp->sp[i].s = button->GetValue() ? ISS_ON : ISS_OFF;
                break;
            }
        }
    }
    sendNewSwitch(svp);
}

void IndiGui::SetComboboxEvent(wxCommandEvent& event)
{
    wxChoice *combo = (wxChoice *)event.GetEventObject();
    if (!combo) return;
    IndiProp *indiProp = (IndiProp *) combo->GetClientData();
    if (!indiProp) return;
    ISwitchVectorProperty *svp = indiProp->property.getSwitch();
    int choice = combo->GetSelection();
    for (int i = 0; i < svp->nsp; i++)
    {
        if (i == choice)
            svp->sp[i].s = ISS_ON;
        else
            svp->sp[i].s = ISS_OFF;
    }
    sendNewSwitch(svp);
}

void IndiGui::SetCheckboxEvent(wxCommandEvent& event)
{
    wxCheckBox *button = (wxCheckBox *)event.GetEventObject();
    if (!button) return;
    IndiProp *indiProp = (IndiProp *) button->GetClientData();
    if (!indiProp) return;
    ISwitchVectorProperty *svp = indiProp->property.getSwitch();

    wxString b_name;
    for (auto it = indiProp->ctrl.begin(); it != indiProp->ctrl.end(); ++it)
    {
        wxString key = it->first;
        wxCheckBox *value = (wxCheckBox *) it->second;
        if (value == button)
        {
            b_name = key;
            break;
        }
    }
    for (int i = 0; i < svp->nsp; i++)
    {
        if (svp->sp[i].name == b_name)
        {
            svp->sp[i].s = button->GetValue() ? ISS_ON : ISS_OFF;
            break;
        }
    }
    sendNewSwitch(svp);
}

void IndiGui::OnRemovePropertyFromThread(wxThreadEvent& event)
{
    IndiProp *indiProp = (IndiProp *)event.GetExtraLong();
    if (!indiProp) return;
    IndiDev *indiDev = (IndiDev *) indiProp->idev;
    if (!indiDev) return;
    wxString propname = indiProp->PropName;

    for (int y = 0; y < indiProp->gbs->GetRows(); y++)
    {
        for (int x = 0; x < indiProp->gbs->GetCols(); x++)
        {
            wxGBSizerItem *item = indiProp->gbs->FindItemAtPosition(POS(y, x));
            if (item)
            {
                indiProp->gbs->Remove(item->GetId());
                item->GetWindow()->Destroy();
            }
        }
    }
    indiProp->gbs->Layout();
    if (indiProp->name)
        indiProp->name->Destroy();
    if (indiProp->state)
        indiProp->state->Destroy();
    if (indiProp->panel)
        indiProp->panel->Destroy();
    if (indiProp->page->GetChildren().GetCount() == 0)
    {
        for (unsigned int i = 0; i < indiDev->page->GetPageCount(); i++)
        {
            if (indiProp->page == indiDev->page->GetPage(i))
            {
                indiDev->groups.erase(indiDev->page->GetPageText(i));
                indiDev->page->DeletePage(i);
                break;
            }
        }
    }
    delete indiProp;
    indiDev->properties.erase(propname);
    indiDev->page->Layout();
    indiDev->page->Fit();
    sizer->Layout();
    Fit();
    m_lastUpdate = wxGetUTCTimeMillis();
}

void IndiGui::ShowIndiGui(IndiGui **ret, const wxString& host, long port, bool allow_connect_disconnect_, bool modal)
{
    IndiGui *gui = new IndiGui();
    gui->allow_connect_disconnect = allow_connect_disconnect_;
    gui->ConnectServer(host, port);

    {
        wxProgressDialog dlg(_("INDI"), _("Loading INDI properties..."), 0, nullptr, wxPD_APP_MODAL | wxPD_CAN_ABORT);

        enum { IDLE_TIME_MS = 500 };

        unsigned int i = 0;
        while (wxGetUTCTimeMillis().GetValue() - gui->m_lastUpdate.GetValue() < IDLE_TIME_MS)
        {
            wxSafeYield();
            wxMilliSleep(10);
            if (dlg.WasCancelled())
            {
                gui->Destroy();
                *ret = nullptr;
                return;
            }
            if (++i % 10 == 0)
                dlg.Pulse();
        }
    }

    gui->m_holder = ret;
    *ret = gui;

    if (modal)
        gui->ShowModal();
    else
        gui->Show();
}

IndiGui::IndiGui()
    :
    wxDialog(wxGetApp().GetTopWindow(), wxID_ANY,
             _("INDI Options"),
             wxDefaultPosition, wxSize(640, 400), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    m_deleted(false),
    m_holder(nullptr)
{
    panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_DOUBLE | wxTAB_TRAVERSAL);
    sizer = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(sizer);
    parent_notebook = new wxNotebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
    sizer->Add(parent_notebook, 0, wxEXPAND | wxALL);
    textbuffer = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    sizer->Add(textbuffer, 1, wxFIXED_MINSIZE | wxEXPAND | wxALL);
}

void IndiGui::OnQuit(wxCloseEvent& WXUNUSED(event))
{
    if (isServerConnected())
        Show(false);
    else
        Destroy();
}

void IndiGui::DestroyIndiGui(IndiGui **holder)
{
    IndiGui *gui = *holder;

    // disconnect gui from owner
    assert(holder == gui->m_holder);
    *gui->m_holder = nullptr;
    gui->m_holder = nullptr;

    gui->Destroy();
}

IndiGui::~IndiGui()
{
    // prevent recursive destruction when DisconnectIndiServer() calls
    // serverDisconnected() which calls Destroy()
    m_deleted = true;

    if (m_holder)
        *m_holder = nullptr;

    disconnectServer();

    for (auto itdev = devlist.begin(); itdev != devlist.end(); ++itdev)
    {
        IndiDev *indiDev = (IndiDev *) itdev->second;
        if (indiDev)
        {
            for (auto itprop = indiDev->properties.begin(); itprop != indiDev->properties.end(); ++itprop)
            {
                IndiProp *indiProp = (IndiProp *) itprop->second;
                indiProp->ctrl.clear();
                indiProp->entry.clear();
                delete indiProp;
            }
            indiDev->properties.clear();
            indiDev->groups.clear();
            delete indiDev;
        }
    }
}
