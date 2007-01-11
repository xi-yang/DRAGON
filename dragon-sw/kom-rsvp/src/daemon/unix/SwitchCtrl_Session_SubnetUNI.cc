/****************************************************************************

UNI Based Switch/Subnet Control Module source file SwitchCtrl_Session_SubnetUNI.cc
Created by Xi Yang @ 01/9/2007
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_SubnetUNI.h"
#include "RSVP.h"
#include "RSVP_Log.h"

int SwitchCtrl_Session_SubnetUNI::clientCount = 0;
SwitchCtrl_Session_SubnetUNI_List* SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = NULL;


void SwitchCtrl_Session_SubnetUNI::internalInit ()
{
    active = false;
    snmp_enabled = false; 
    rfc2674_compatible = false; 
    snmpSessionHandle = NULL; 
    uniSessionId = NULL; 
    SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = new SwitchCtrl_Session_SubnetUNI_List;
}

virtual SwitchCtrl_Session_SubnetUNI::~SwitchCtrl_Session_SubnetUNI() 
{
    deregisterRsvpApiClient();
}


UpcallProcedure uniRsvpSrcUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}

UpcallProcedure uniRsvpDestUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}


void SwitchCtrl_Session_SubnetUNI::registerRsvpApiClient()
{
    //insert the UNI ApiClient logicalLif into rsvp lifList if it has not been added.
    assert (apiLif);
    assert (getFileDesc() > 0);
    if (!RSVP::findInterfaceByName(RSVP_Global::apiUniClientName) {
        RSVP::addApiClientInterface(apiLif);
    }
    NetworkServiceDaemon::registerApiClient_Handle(getFileDesc());
    subnetUniApiClientList.push_front(this);
}

void SwitchCtrl_Session_SubnetUNI::deregisterRsvpApiClient()
{
    SwitchCtrl_Session_SubnetUNI_List::Iterator it = subnetUniApiClientList.begin() ;
    for (; it != subnetUniApiClientList.end(); ++it) {
        if ((*it) == this) {
            subnetUniApiClientList.erase(it);
            break;
        }
    }
    if (subnetUniApiClientList.size() == 0) {
        NetworkServiceDaemon::deregisterApiClient_Handle(getFileDesc());
    }
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessMessage(Message& msg)
{
    //checking msg owner
    if (!isSessionOwner(msg))
        return;

    if (isSource)
        receiveAndProcessResv(msg);
    else
        receiveAndProcessPath(msg);
}

bool SwitchCtrl_Session_SubnetUNI::isSessionOwner(Message& msg)
{
    SESSION_Object& session_obj = msg.getSESSION_Object();
    
    if (isSource && session_obj.getDestAddress().rawAddress() == subnetUniSrc.uni_nid_ipv4 && session_obj.getTunnelID() == subnetUniSrc.tunnel_id)
    {
        LSP_TUNNEL_IPv4_SENDER_TEMPLATE_Object& sender_obj = msg.getSENDER_TEMPLATE_Object();
        if (sender_obj.getSrcAddress() == subnetUniSrc.uni_cid_ipv4)
            return true;
    }
    else if (session_obj.getDestAddress().rawAddress() == subnetUniDest.uni_cid_ipv4 && session_obj.getTunnelID() == subnetUniDest.tunnel_id)
    {
        FlowDescriptorList& fdList = msg.getFlowDescriptorList();
        FlowDescriptorList::Iterator it = fdList.begin();
        for (; it != fdList.end(); ++it)
        {
             if ((*it).filterSpecList.size()>0 && (*(*it).filterSpecList.begin()).getSrcAddress() == subnetUniDest.uni_nid_ipv4)
                return true;
        }
    }

    return false;
}

void SwitchCtrl_Session_SubnetUNI::initUniRsvpApiSession()
{
    uniSessionId = new RSVP_API::SessionId();
    if (isSource)
        *uniSessionId = createSession( NetAddress(subnetUniSrc.uni_nid_ipv4), subnetUniSrc.tunnel_id, (UpcallProcedure)subnetUniSrc.uni_nid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpSrcUpcall);
    else
        *uniSessionId = createSession( NetAddress(subnetUniDest.uni_cid_ipv4), subnetUniDest.tunnel_id, (UpcallProcedure)subnetUniDest.uni_cid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpDestUpcall);
    active = true;
}

void SwitchCtrl_Session_SubnetUNI::createRsvpUniPath()
{
    if (!active || !uniSessionId)
        initUniRsvpApiSession();
    if (!isSource)
    	return;

    SONET_SDH_SENDER_TSPEC_Object *stb = NULL;
    GENERALIZED_UNI_Object *uni = NULL;
    LABEL_SET_Object* labelSet = NULL;
    SESSION_ATTRIBUTE_Object* ssAttrib = NULL;
    UPSTREAM_LABEL_Object* upLabel = NULL;
    GENERALIZED_LABEL_REQUEST_Object *lr = NULL;
    //IPv4_IF_ID_RSVP_HOP_Object  $$$$ based on control_channel_name ???
    //==> revise RSVP_AP::createSender

    SONET_TSpec* sonet_tb1 = SwitchCtrl_Global::getEosMapEntry(subnetUniSrc.ethernet_bw);
    if (sonet_tb1)
        stb = new SENDER_TSPEC_Object(sonet_tb1);

    uni = new GENERALIZED_UNI_Object (subnetUniSrc.uni_cid_ipv4, subnetUniSrc.uni_nid_ipv4, 
                    subnetUniSrc.logical_port, subnetUniSrc.egress_label, 
                    subnetUniSrc.logical_port, subnetUniSrc.upstream_label);

    /* $$$$ we may need this later on ...
    labelSet = new LABEL_SET_Object();
    for (int i=0;i<para->labelSetSize;i++)
     	labelSet->addSubChannel(para->labelSet[i]);
    */

    ssAttrib = new SESSION_ATTRIBUTE_Object(sessionName); //$$$$

    upLabel = new UPSTREAM_LABEL_Object(subnetUniSrc.upstream_label);

    lr = new LABEL_REQUEST_Object ( LABEL_REQUEST_Object::L_ANSI_SDH, 
    							       LABEL_REQUEST_Object::S_TDM,
                                                        LABEL_REQUEST_Object::G_SONET_SDH);

    //SESSION is implied by uniSessionId and SENDER_TEMPLATE by (addr=0, port=subnetUniSrc.tunnel_id)
    createSender( *uniSessionId, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, labelSet, ssAttrib, upLabel, 50);

    if (uni) uni->destroy();
    if (labelSet) labelSet->destroy();
    if (lr) delete lr;
    if (stb) delete stb;
    if (ssAttrib) delete ssAttrib;
    if (upLabel) delete upLabel;

    return;
}

void SwitchCtrl_Session_SubnetUNI::createRsvpUniResv(SONET_SDH_SENDER_TSPEC_Object& sendTSpec, LSP_TUNNEL_IPv4_FILTER_SPEC_Object& senderTemplate)
{
    if (!active || !uniSessionId)
        return;

    SONET_SDH_FLOWSPEC_Object* flowspec = new FLOWSPEC_Object((const SONET_TSpec&)(sendTSpec));
    FlowDescriptorList fdList;
    fdList.push_back( flowspec );
    fdList.back().filterSpecList.push_back( senderTemplate.borrow() );
    //IPv4_IF_ID_RSVP_HOP_Object? ==> revise RSVP_AP::createReservation

    createReservation( *uniSessionId, false, FF, fdList, NULL);

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessPath(Message & msg)
{
    if (!active)
        return;

    switch (msg.getMsgType())
    {
    case Message::Path:
    case Message::PathResv:

        //update local uniSessionId based on the incoming PATH msg?

        createRsvpUniResv(msg.getSENDER_TSPEC_Object(), msg.getSENDER_TEMPLATE_Object());
        break;
    case Message::InitAPI:
        assert( *(*uniSessionId) );
        refreshSession( **(*uniSessionId) );
        break;
    default:
        //unprocessed RSVP messages
        break;
    }

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessResv(Message & msg)
{
    if (!active)
        return;
    
    switch (msg.getMsgType())
    {
    case Message::Resv:
    case Message::ResvConf:

        //change session states ...
        //notify main session ...

        break;
    case Message::InitAPI:
        assert( *(*uniSessionId) );
        refreshSession( **(*uniSessionId) );
        break;
    default:
        break;
    }

    return;
}

void SwitchCtrl_Session_SubnetUNI::releaseRsvpPath()
{
    if (!active || !uniSessionId)
        return;

    assert( *(*uniSessionId) );
    releaseSession(**(*uniSessionId));

    return;
}

void SwitchCtrl_Session_SubnetUNI::refreshUniRsvpSession()
{
    if (!active)
        return;
    
    //do nothing?
}


//////////////////////////////////////////////////////////////////////////
/////                             UNI Object implementation                                              ////
/////////////////////////////////////////////////////////////////////////

void GENERALIZED_UNI_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len)
{
	buffer >>srcTNA.length >> srcTNA.type >> srcTNA.sub_type >> srcTNA.addr.s_addr;
	buffer >>destTNA.length >> destTNA.type >> destTNA.sub_type >> destTNA.addr.s_addr;
	buffer >>egressLabel.length >> egressLabel.type >> egressLabel.sub_type >> egressLabel.logical_port >> egressLabel.label;
	buffer >>egressLabelUp.length >> egressLabelUp.type >> egressLabelUp.sub_type >> egressLabelUp.logical_port >> egressLabelUp.label;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const GENERALIZED_UNI_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::GENERALIZED_UNI, 1);
	buffer <<o.srcTNA.length << o.srcTNA.type << o.srcTNA.sub_type << o.srcTNA.addr.s_addr;
	buffer << o.destTNA.length << o.destTNA.type << o.destTNA.sub_type << o.destTNA.addr.s_addr;
	buffer << o.egressLabel.length << o.egressLabel.type << o.egressLabel.sub_type << o.egressLabel.logical_port<< o.egressLabel.label;
	buffer << o.egressLabelUp.length << o.egressLabelUp.type << o.egressLabelUp.sub_type << o.egressLabelUp.logical_port << o.egressLabelUp.label;
	return buffer;
}

ostream& operator<< ( ostream& os, const GENERALIZED_UNI_Object& o ) {
	char addr_str[20];
	os <<"[G_UNI Source: " << String( inet_ntoa(o.srcTNA.addr));
	os <<" <=> Sestination: " << String( inet_ntoa(o.destTNA.addr));
	os << " (Downstream port <<" o.egressLabel.logical_port << " /Label: " << o.egressLabel.label;
	os << " (Upstream port <<" o.egressLabelUp.logical_port << " /Label: " << o.egressLabelUp.label;
	os << ") ]";
	return os;
}


