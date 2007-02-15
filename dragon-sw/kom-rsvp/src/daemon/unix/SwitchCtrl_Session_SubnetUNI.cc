/****************************************************************************

UNI Based Switch/Subnet Control Module source file SwitchCtrl_Session_SubnetUNI.cc
Created by Xi Yang @ 01/9/2007
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_SubnetUNI.h"
#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_RoutingService.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_Message.h"
#include "RSVP_Log.h"

SwitchCtrl_Session_SubnetUNI_List* SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = NULL;


void SwitchCtrl_Session_SubnetUNI::internalInit ()
{
    active = false;
    snmp_enabled = false; 
    rfc2674_compatible = false; 
    snmpSessionHandle = NULL; 
    uniSessionId = NULL; 
    uniState = 0; //Message::initApi
    ctagNum = 0;
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniData(SubnetUNI_Data& data, uint8 subuni_id, uint8 first_ts,
 uint16 tunnel_id, float bw, uint32 tna_ipv4, uint32 uni_cid_ipv4, uint32 uni_nid_ipv4, uint32 data_if,
 uint32 port, uint32 egress_label, uint32 upstream_label, uint8* cc_name, uint8* bitmask)
{
    memset(&data, 0, sizeof(SubnetUNI_Data));
    data.subnet_id = subuni_id;
    data.first_timeslot = first_ts;
    data.tunnel_id = tunnel_id; 
    data.ethernet_bw = bw; 
    data.tna_ipv4 = tna_ipv4;
    data.logical_port = port;
    data.egress_label = egress_label; 
    data.upstream_label = upstream_label;

    data.uni_cid_ipv4 = uni_cid_ipv4;
    data.uni_nid_ipv4 = uni_nid_ipv4;
    data.data_if_ipv4 = data_if;

    memcpy(data.control_channel_name, cc_name, CTRL_CHAN_NAME_LEN);
    memcpy(data.timeslot_bitmask, bitmask, MAX_TIMESLOTS_NUM/8);
}       

void SwitchCtrl_Session_SubnetUNI::setSubnetUniSrc(SubnetUNI_Data& data)
{
	const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByName(String((char*)data.control_channel_name));
	data.uni_cid_ipv4 = lif ? lif->getLocalAddress().rawAddress() : data.tna_ipv4; //?
	//uint32 data_if = 0;

	setSubnetUniData(subnetUniSrc, data.subnet_id, data.first_timeslot, data.tunnel_id, data.ethernet_bw, data.tna_ipv4, 
		data.uni_cid_ipv4, data.uni_nid_ipv4, data.data_if_ipv4, data.logical_port, data.egress_label, data.upstream_label, 
		data.control_channel_name, data.timeslot_bitmask);
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniDest(SubnetUNI_Data& data)
{
	const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByName(String((char*)data.control_channel_name));
	data.uni_cid_ipv4 = lif ? lif->getLocalAddress().rawAddress() : data.tna_ipv4; //?
	//uint32 data_if = 0;

	setSubnetUniData(subnetUniDest, data.subnet_id, data.first_timeslot, data.tunnel_id, data.ethernet_bw, data.tna_ipv4, 
		data.uni_cid_ipv4, data.uni_nid_ipv4, data.data_if_ipv4, data.logical_port, data.egress_label, data.upstream_label, 
		data.control_channel_name, data.timeslot_bitmask);
}

const LogicalInterface* SwitchCtrl_Session_SubnetUNI::getControlInterface(NetAddress& gwAddress)
{
	SubnetUNI_Data* uniData = (isSource ? &subnetUniSrc : &subnetUniDest);
	const NetAddress nidAddress(uniData->uni_nid_ipv4);
	const LogicalInterface* lif = NULL;
	if ( uniData->control_channel_name[0] == 0 || strcmp((char*)uniData->control_channel_name, "implicit") == 0 )
	{
		lif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( nidAddress, gwAddress );
		gwAddress = nidAddress;
		return lif;
	}
	else
	{
		lif = RSVP_Global::rsvp->findInterfaceByName(String((char*)uniData->control_channel_name));
		if (lif)
			RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(lif->getLocalAddress(), gwAddress);
		return lif;
	}
}

uint32 SwitchCtrl_Session_SubnetUNI::getPseudoSwitchID()
{
	SubnetUNI_Data* uniData = (isSource ? &subnetUniSrc : &subnetUniDest);
	//return (((uint32)uniData->subnet_id) << 16) | ((uint32)uniData->tunnel_id);
	return ((uint32)uniData->tunnel_id);
}

SwitchCtrl_Session_SubnetUNI::~SwitchCtrl_Session_SubnetUNI() 
{
    deregisterRsvpApiClient();
    if (!uniSessionId)
        delete uniSessionId;
}


void SwitchCtrl_Session_SubnetUNI::uniRsvpSrcUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}

void SwitchCtrl_Session_SubnetUNI::uniRsvpDestUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}


void SwitchCtrl_Session_SubnetUNI::registerRsvpApiClient()
{
    //insert the UNI ApiClient logicalLif into rsvp lifList if it has not been added.
    assert (RSVP_API::apiLif);
    assert (getFileDesc() > 0);
    const String ifName(RSVP_Global::apiUniClientName);
    if (!RSVP_Global::rsvp->findInterfaceByName(String(RSVP_Global::apiUniClientName))) {
        RSVP_Global::rsvp->addApiClientInterface(RSVP_API::apiLif);
    }
    NetworkServiceDaemon::registerApiClient_Handle(getFileDesc());
    if (!SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList)
        SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = new SwitchCtrl_Session_SubnetUNI_List;
    subnetUniApiClientList->push_front(this);
}

void SwitchCtrl_Session_SubnetUNI::deregisterRsvpApiClient()
{
    if(!SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList)
	return;
    SwitchCtrl_Session_SubnetUNI_List::Iterator it = subnetUniApiClientList->begin() ;
    for (; it != subnetUniApiClientList->end(); ++it) {
        if ((*it) == this) {
            subnetUniApiClientList->erase(it);
            break;
        }
    }
    if (subnetUniApiClientList->size() == 0) {
        NetworkServiceDaemon::deregisterApiClient_Handle(getFileDesc());
        delete SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList;
        SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = NULL;
    }
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessMessage(const Message& msg)
{
    //checking msg owner
    if (!isSessionOwner(msg))
        return;

    if (isSource)
        receiveAndProcessResv(msg);
    else
        receiveAndProcessPath(msg);
}

bool SwitchCtrl_Session_SubnetUNI::isSessionOwner(const Message& msg)
{
    const SESSION_Object* session_obj = &msg.getSESSION_Object();
    const LSP_TUNNEL_IPv4_SENDER_TEMPLATE_Object* sender_obj;
    const FlowDescriptorList* fdList;
    SubnetUNI_Data* uniData;

    if (isSource) {
        if ( !(session_obj->getDestAddress().rawAddress() == subnetUniSrc.uni_nid_ipv4 && session_obj->getTunnelId() == subnetUniSrc.tunnel_id) )
            return false;
        uniData = &subnetUniSrc;
    }
    else if ( !(session_obj->getDestAddress().rawAddress() == subnetUniDest.uni_cid_ipv4 && session_obj->getTunnelId() == subnetUniDest.tunnel_id) )
    {
        return false;
        uniData = &subnetUniDest;
    }

    if (msg.getMsgType() == Message::Path)
    {
        sender_obj = &msg.getSENDER_TEMPLATE_Object();
        if (sender_obj->getSrcAddress().rawAddress() == uniData->uni_cid_ipv4
            || sender_obj->getSrcAddress().rawAddress() == 0 || sender_obj->getSrcAddress().rawAddress() == 0x100007f)
            return true;
    }
    else if  (msg.getMsgType() == Message::Resv)
    {
        fdList = &msg.getFlowDescriptorList();
        FlowDescriptorList::ConstIterator it = fdList->begin();
        for (; it != fdList->end(); ++it)
        {
             if ((*it).filterSpecList.size()>0 && (*(*it).filterSpecList.begin()).getSrcAddress().rawAddress()  == uniData->uni_cid_ipv4)
                return true;
        }
    }

    return false;
}

void SwitchCtrl_Session_SubnetUNI::initUniRsvpApiSession()
{
    uniSessionId = new RSVP_API::SessionId();
    if (isSource)
        *uniSessionId = createSession( NetAddress(subnetUniSrc.uni_nid_ipv4), subnetUniSrc.tunnel_id,subnetUniSrc.uni_cid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpSrcUpcall);
    else
        *uniSessionId = createSession( NetAddress(subnetUniDest.uni_cid_ipv4), subnetUniDest.tunnel_id, subnetUniDest.uni_nid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpDestUpcall);
    active = true;

    uniState = Message::InitAPI;
}

static uint32 getSONETLabel(uint8 timeslot, SONET_TSpec* tspec)
{
    assert(tspec);
    switch (tspec->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        return (((uint32)(timeslot/3) << 16) | ((uint32)(timeslot%3) << 12) );
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        return ((uint32)(timeslot/3) << 16);
        break;
    }

    return 0;
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

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(subnetUniSrc.ethernet_bw);
    if (sonet_tb1)
        stb = new SENDER_TSPEC_Object(*sonet_tb1);

    // Pick the first available timeslot, convert it into SONET label and add to LABEL_SET
    uint8 ts;
    if (subnetUniSrc.first_timeslot == 0)
    {
        for (ts = 1; ts <= MAX_TIMESLOTS_NUM; ts++)
        {
            if ( HAS_TIMESLOT(subnetUniSrc.timeslot_bitmask, ts) )
            {
                subnetUniSrc.first_timeslot = ts;
                break;
            }
        }
    }
    assert (subnetUniSrc.first_timeslot != 0);
    labelSet = new LABEL_SET_Object(); //LABEL_GENERALIZED
    uint32 label = getSONETLabel(subnetUniSrc.first_timeslot, sonet_tb1); 
    labelSet->addSubChannel(htonl(label));
    subnetUniSrc.upstream_label = htonl(label);

    // Update egress_label ... (using the same upstream label) ==> always do symetric provisioning!
    if (subnetUniDest.first_timeslot == 0)
    {
        for (ts = 1; ts <= MAX_TIMESLOTS_NUM; ts++)
        {
            if ( HAS_TIMESLOT(subnetUniDest.timeslot_bitmask, ts) )
            {
                subnetUniDest.first_timeslot = ts;
                break;
            }
        }
    }
    assert (subnetUniDest.first_timeslot != 0);
    label = getSONETLabel(subnetUniDest.first_timeslot, sonet_tb1);
    subnetUniDest.egress_label = subnetUniDest.upstream_label = htonl(label);

    uni = new GENERALIZED_UNI_Object (subnetUniSrc.tna_ipv4, subnetUniDest.tna_ipv4, 
                    subnetUniDest.logical_port, subnetUniDest.egress_label,
                    subnetUniDest.logical_port, subnetUniDest.upstream_label);

    ssAttrib = new SESSION_ATTRIBUTE_Object(sessionName);

    upLabel = new UPSTREAM_LABEL_Object(subnetUniSrc.upstream_label);

    lr = new LABEL_REQUEST_Object ( LABEL_REQUEST_Object::L_ANSI_SDH, 
    							       LABEL_REQUEST_Object::S_TDM,
                                                        LABEL_REQUEST_Object::G_SONET_SDH);

    //NetAddress srcAddress(subnetUniSrc.uni_cid_ipv4);
    //createSender( *uniSessionId, srcAddress, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, labelSet, ssAttrib, upLabel, 50);
    createSender( *uniSessionId, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, labelSet, ssAttrib, upLabel, 50);

    if (uni) uni->destroy();
    if (labelSet) labelSet->destroy();
    if (lr) delete lr;
    if (stb) delete stb;
    if (ssAttrib) delete ssAttrib;
    if (upLabel) delete upLabel;

    if (uniState == Message::InitAPI)
        uniState = Message::Path;

    return;
}

void SwitchCtrl_Session_SubnetUNI::createRsvpUniResv(const SONET_SDH_SENDER_TSPEC_Object& sendTSpec, const LSP_TUNNEL_IPv4_FILTER_SPEC_Object& senderTemplate)
{
    if (!active || !uniSessionId)
        return;

    SONET_SDH_FLOWSPEC_Object* flowspec = new FLOWSPEC_Object((const SONET_TSpec&)(sendTSpec));
    FlowDescriptorList fdList;
    fdList.push_back( flowspec );
    fdList.back().filterSpecList.push_back( senderTemplate );
    //IPv4_IF_ID_RSVP_HOP_Object? ==> revise RSVP_AP::createReservation

    createReservation( *uniSessionId, false, FF, fdList, NULL);

    if (uniState == Message::InitAPI)
        uniState = Message::Resv;

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessPath(const Message & msg)
{
    if (!active)
        return;

    //change session states ...
    //notify main session indirectly
    uniState = msg.getMsgType();

    switch (msg.getMsgType())
    {
    case Message::Path:
    case Message::PathResv:

        createRsvpUniResv(msg.getSENDER_TSPEC_Object(), msg.getSENDER_TEMPLATE_Object());

        break;
    case Message::InitAPI:

        assert( *(*uniSessionId) );
        //refreshSession( **(*uniSessionId), RSVP_Global::defaultApiRefresh );

        break;
    default:
        //unprocessed RSVP messages
        break;
    }

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessResv(const Message & msg)
{
    if (!active)
        return;

    //change session states ...
    //notify main session indirectly
    uniState = msg.getMsgType();
    
    switch (msg.getMsgType())
    {
    case Message::Resv:
    case Message::ResvConf:

        break;
    case Message::PathErr:

        break;
    case Message::ResvTear:

        break;
    case Message::InitAPI:

        assert( *(*uniSessionId) );
        //refreshSession( **(*uniSessionId), RSVP_Global::defaultApiRefresh );

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

    if (isSource)
        uniState = Message::PathTear;
    else
        uniState = Message::ResvTear; 

    return;
}

void SwitchCtrl_Session_SubnetUNI::refreshUniRsvpSession()
{
    if (!active)
        return;
    
    //do nothing?
}

void SwitchCtrl_Session_SubnetUNI::getTimeslots(SimpleList<uint8>& timeslots)
{
    timeslots.clear();
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;

    uint8 ts = pUniData->first_timeslot;
    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);

    uint8 ts_num = 0;
    assert(sonet_tb1);
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    default:
        ts_num = 0;
    }

    for (uint8 x = 0; x < ts_num; x++)
    {
        timeslots.push_back(ts + x);
    }
}

//////////////////////////////////
/////// TL1 related commands  //////
/////////////////////////////////

void SwitchCtrl_Session_SubnetUNI::getCienaTimeslotsString(String& groupMemString)
{
    SimpleList<uint8> timeslots;
    getTimeslots(timeslots);
    char buf[100], ts[5];

    assert(timeslots.size() > 0);
    SimpleList<uint8>::Iterator iter = timeslots.begin();
    for (; iter != timeslots.end(); ++iter)
    {
        if (iter == timeslots.begin())
            sprintf(ts, "%d", *iter);
        else
            sprintf(ts, "&%d", *iter);
        strcat(buf, ts);
    }
    groupMemString = (const char*)buf;
}


void SwitchCtrl_Session_SubnetUNI::getCienaLogicalPortString(String& OMPortString, String& ETTPString, uint32 logicalPort)
{
    int chasis, shelf, slot, subslot, port;
    char buf[20];
    SubnetUNI_Data* pSubnetUni = (isSource ? &subnetUniSrc : &subnetUniDest);

    if (logicalPort == 0)
    {
        logicalPort = pSubnetUni->logical_port;
    }

    chasis = (logicalPort >> 24) + 1;
    shelf = ((logicalPort >> 16)&0xff) + 1;
    slot = ((logicalPort >> 12)&0xf) + 1;
    subslot = ((logicalPort >> 8)&0xf) + 1;
    port = (logicalPort&0xff) + 1;

    sprintf(buf, "%d-%c-%d-%d", chasis, 'A'+shelf, slot, subslot);
    OMPortString = (const char*)buf;

    sprintf(buf, "%d-%c-%d-%d-%d", chasis, 'A'+shelf, slot, subslot, port);
    ETTPString = (const char*)buf;
}

//ENT-VCG::NAME=vcg01:456::,PST=is,SUPPTTP=1-A-3-1,CRCTYPE=CRC_32,,,FRAMINGMODE=GFP,
//TUNNELPEERTYPE=ETTP,TUNNELPEERNAME=1-A-3-1-1,,GFPFCSENABLED=yes,,,GROUPMEM=1&&3,,;
bool SwitchCtrl_Session_SubnetUNI::createVCG_TL1(String& vcgName)
{
    int ret = 0;
    char ctag[10], strCOMPLD[20], strDENY[20];
    String suppTtp, tunnelPeerName, groupMem;

    sprintf(ctag, "%d", getNewCtag());

    vcgName = "vcg_";
    vcgName += (const char*)ctag;

    getCienaLogicalPortString(suppTtp, tunnelPeerName);
    getCienaTimeslotsString(groupMem);

    String cmdString = "ent-vcg::name=";
    cmdString += vcgName;
    cmdString += ":";
    cmdString += (const char*)ctag;
    cmdString += "::,pst=IS,suppttp=";
    cmdString += suppTtp;
    cmdString += ",crctype=CRC_32,,,framingmode=GFP,tunnelpeertype=ETTP,tunnelpeername=";
    cmdString += tunnelPeerName;
    cmdString += ",,gfpfcsenabled=YES,,,groupmem=";
    cmdString += groupMem;
    cmdString += ";";

    if ( (ret = writeShell(cmdString.chars(), 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(2)(Log::MPLS, vcgName, " has been created successfully.\n");
        readShell(";", NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(2)(Log::MPLS, vcgName, " creation has been denied.\n");
        readShell(";", NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(2)(Log::MPLS, vcgName, " creation via TL1_TELNET failed...\n");
        return false;
}

//ED-VCG::NAME=vcg01:123::,PST=OOS;
//DLT-VCG::NAME=vcg01:123;
bool SwitchCtrl_Session_SubnetUNI::deleteVCG_TL1(String& vcgName)
{
    int ret = 0;
    char bufCmd[100], strCOMPLD[20], strDENY[20];
    uint32 ctag = getNewCtag();

    sprintf(bufCmd, "ed-vcg::name=%s:%d::,pst=OOS;", vcgName.chars(), ctag);
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(2)(Log::MPLS, vcgName, " status has been set to OOS.\n");
        readShell(";", NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(2)(Log::MPLS, vcgName, " status change (to OOS) has been denied.\n");
        readShell(";", NULL, 1, 5);
        return false;
    }
    else 
        goto _out;
    
    ctag = getNewCtag();
    sprintf(bufCmd, "dlt-vcg::name=%s:%d;", vcgName.chars(), ctag);
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(2)(Log::MPLS, vcgName, " has been deleted successfully.\n");
        readShell(";", NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(2)(Log::MPLS, vcgName, " deletion has been denied.\n");
        readShell(";", NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(2)(Log::MPLS, vcgName, " change/deletion via TL1_TELNET failed...\n");
        return false;
}


