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
    numGroups = 0;
    ptpCatUnit = CATUNIT_UNKNOWN;
    memset(&DTL, 0, sizeof(DTL_Subobject));
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniData(SubnetUNI_Data& data, uint8 subuni_id, uint8 first_ts,
 uint16 tunnel_id, float bw, uint32 tna_ipv4, uint32 uni_cid_ipv4, uint32 uni_nid_ipv4, uint32 data_if,
 uint32 port, uint32 egress_label, uint32 upstream_label, uint8* node_name, uint8* cc_name, uint8* bitmask)
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

    memcpy(data.node_name, node_name, NODE_NAME_LEN);
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
		data.node_name, data.control_channel_name, data.timeslot_bitmask);
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniDest(SubnetUNI_Data& data)
{
	const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByName(String((char*)data.control_channel_name));
	data.uni_cid_ipv4 = lif ? lif->getLocalAddress().rawAddress() : data.tna_ipv4; //?
	//uint32 data_if = 0;

	setSubnetUniData(subnetUniDest, data.subnet_id, data.first_timeslot, data.tunnel_id, data.ethernet_bw, data.tna_ipv4, 
		data.uni_cid_ipv4, data.uni_nid_ipv4, data.data_if_ipv4, data.logical_port, data.egress_label, data.upstream_label, 
		data.node_name, data.control_channel_name, data.timeslot_bitmask);
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
	//return ((uint32)uniData->tunnel_id);
	return ((((uint32)this)&0xff)*100 + (uint32)uniData->tunnel_id); //the session has an unique ID on this VLSR!
}

SwitchCtrl_Session_SubnetUNI::~SwitchCtrl_Session_SubnetUNI() 
{
    deregisterRsvpApiClient();
    if (uniSessionId)
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
    createSender( *uniSessionId, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, NULL, labelSet, ssAttrib, upLabel, 50); //dagonExtInfo?

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

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
    	if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
            return;
    }
    uint8 ts = pUniData->first_timeslot;
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
        return;

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    if (!sonet_tb1)
        return;

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    for (uint8 x = 0; x < ts_num && ts+x <= MAX_TIMESLOTS_NUM; x++)
    {
        timeslots.push_back(ts + x);
    }
}

//////////////////////////////////
/////// TL1 related commands  //////
/////////////////////////////////

void SwitchCtrl_Session_SubnetUNI::getCienaTimeslotsString(String& groupMemString)
{
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    uint8 ts = pUniData->first_timeslot;

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
        if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
    	{
            groupMemString = "";
            return;
    	}
    }
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
    {
        groupMemString = "";
        return;
    }


    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    if (!sonet_tb1)
    {
        groupMemString = "";
        return;
    }

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ts_num == 0 || ts+ts_num-1 > MAX_TIMESLOTS_NUM)
    {
        groupMemString = "";
        return;
    }
    
    sprintf(bufCmd, "%d&&%d", ts, ts+ts_num-1);
    groupMemString = (const char*)bufCmd;
}

void SwitchCtrl_Session_SubnetUNI::getCienaCTPGroupsInVCG(String*& ctpGroupStringArray, String& vcgName)
{
    assert(ctpGroupStringArray);
    int group;
    for (group = 0; group < 4; group++) 
        ctpGroupStringArray[group] = "";
    
    char ctp[60];
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    uint8 ts = pUniData->first_timeslot;

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
    	if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
    	{
            return;
    	}
    }
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
    {
        return;
    }

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    if (!sonet_tb1)
    {
        return;
    }

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = (ts_num+2)/3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ts_num == 0 || ts+ts_num-1 > MAX_TIMESLOTS_NUM)
    {
        return;
    }

    uint8 first_ts = ts;
    group = 0; 
    numGroups = 0;
    if (ptpCatUnit == CATUNIT_150MBPS)
    {
        sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts/3+1);
        ts_num += ts;
        ts += 3;

        for ( ; ts < ts_num; ts += 3)
        {
            if (ts - first_ts == 48)
            {
                ctpGroupStringArray[group] = (const char*)bufCmd;
                group++;
                numGroups++;
                first_ts = ts;
                sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts/3+1);
                continue;
            }
            sprintf(ctp, "&%s-CTP-%d", vcgName.chars(), ts/3+1);
            strcat(bufCmd, ctp);
        }
        ctpGroupStringArray[group] = (const char*)bufCmd;
        numGroups++;        
    }
    else // must be CATUNIT_50MBPS
    {
        assert (ptpCatUnit == CATUNIT_50MBPS);
        sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts);
        ts_num += ts;
        ts += 1;
        for ( ; ts < ts_num; ts++)
        {
            if (ts - first_ts == 48)
            {
                ctpGroupStringArray[group] = (const char*)bufCmd;
                group++;
                numGroups++;
                first_ts = ts;
                sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts);
                continue;
            }
            sprintf(ctp, "&%s-CTP-%d", vcgName.chars(), ts);
            strcat(bufCmd, ctp);
        }
        ctpGroupStringArray[group] = (const char*)bufCmd;
        numGroups++;        
    }

}


void SwitchCtrl_Session_SubnetUNI::getCienaLogicalPortString(String& OMPortString, String& ETTPString, uint32 logicalPort)
{
    int bay, shelf, slot, subslot, port;
    char shelf_alpha;
    SubnetUNI_Data* pSubnetUni = (isSource ? &subnetUniSrc : &subnetUniDest);

    if (logicalPort == 0)
    {
        logicalPort = ntohl(pSubnetUni->logical_port);
    }

    bay = (logicalPort >> 24) + 1;
    shelf = ((logicalPort >> 16)&0xff);
    slot = ((logicalPort >> 12)&0x0f) + 1;
    subslot = ((logicalPort >> 8)&0x0f) + 1;
    port = (logicalPort&0xff) + 1;

    switch (shelf)
    {
    case 2:
        shelf_alpha = 'A';
        break;
    case 3:
        shelf_alpha = 'C';
        break;
    default:
        shelf_alpha = 'X';
        break;
    }
    sprintf(bufCmd, "%d-%c-%d-%d", bay, shelf_alpha, slot, subslot);
    OMPortString = (const char*)bufCmd;
    sprintf(bufCmd, "%d-%c-%d-%d-%d", bay, shelf_alpha, slot, subslot, port);
    ETTPString = (const char*)bufCmd;
}

void SwitchCtrl_Session_SubnetUNI::getCienaDestTimeslotsString(String*& destTimeslotsStringArray)
{
    assert(destTimeslotsStringArray);
    int group;
    for (group = 0; group < 4; group++) 
        destTimeslotsStringArray[group] = "";

    int bay, shelf, slot, subslot;
    char shelf_alpha;
    uint32 logicalPort = ntohl(subnetUniDest.logical_port);
    uint8 ts = subnetUniDest.first_timeslot;

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
    	if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
    	{
            return;
    	}
    }
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
    {
        return;
    }

    bay = (logicalPort >> 24) + 1;
    shelf = ((logicalPort >> 16)&0xff);
    slot = ((logicalPort >> 12)&0x0f) + 1;
    subslot = ((logicalPort >> 8)&0x0f) + 1;

    switch (shelf)
    {
    case 2:
        shelf_alpha = 'A';
        break;
    case 3:
        shelf_alpha = 'C';
        break;
    default:
        shelf_alpha = 'X';
        break;
    }

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(subnetUniDest.ethernet_bw);
    if (!sonet_tb1)
    {
        return;
    }

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }
    if (ts_num == 0 || ts+ts_num-1 > MAX_TIMESLOTS_NUM || ts_num/48 != numGroups - (ts_num%48 == 0 ? 0 : 1))
    {
        return;
    }

    for (group = 0; group < numGroups; group++)
    {
        sprintf(bufCmd, "%d-%c-%d-%d-%d&&%d", bay, shelf_alpha, slot, subslot, ts+group*48, (group == numGroups-1? ts+ts_num-1 : ts+group*48+47));
        destTimeslotsStringArray[group] = (const char*)bufCmd;
    }
}

void SwitchCtrl_Session_SubnetUNI::getPeerCRS_GTP(String& gtpName)
{
    gtpName = "";
    SwitchCtrl_Session_SubnetUNI* pSubnetSession;
    
    SwitchCtrlSessionList::Iterator sessionIter = RSVP_Global::switchController->getSessionList().begin();
    for ( ; sessionIter != RSVP_Global::switchController->getSessionList().end(); ++sessionIter)
    {
        if ( (*sessionIter)->getSessionName().leftequal("subnet-uni") ) {
            pSubnetSession = (SwitchCtrl_Session_SubnetUNI*)(*sessionIter);
            if (pSubnetSession != this && this->isSourceClient() != pSubnetSession->isSourceClient() 
                && pSubnetSession->getSubnetUniDest()->subnet_id == this->getSubnetUniDest()->subnet_id
                && pSubnetSession->getSubnetUniDest()->tunnel_id == this->getSubnetUniDest()->tunnel_id)

            {
                pSubnetSession->getCurrentGTP(gtpName);
                break;
            }
        }
    }
    return;
}

void SwitchCtrl_Session_SubnetUNI::getDTLString(String& dtlStr)
{
    dtlStr = "";
    if (DTL.count == 0 || DTL.count > MAX_DTL_LEN)
        return;
    bufCmd[0] = 0;
    int i = 0; char hop[40];
    for (i=0; i < DTL.count; i++)
    {
        sprintf(hop, "nodename%d=%s,osrpltpid%d=%d,", i+1, (char*)DTL.hops[i].nodename, i+1, DTL.hops[i].linkid);
        strcat(bufCmd, hop);
    }
    sprintf(hop, "termnodename=%s", (char*)subnetUniDest.node_name);
    strcat(bufCmd, hop);
    dtlStr = bufCmd;
    return;
}

//ent-eflow::myeflow1:123:::ingressporttype=ettp,ingressportname=1-A-3-1-1, 
//pkttype=single_vlan_tag,outervlanidrange=1&&5,,priority=1&&8,egressporttype=vcg, 
//egressportname=vcg02,cosmapping=cos_port_default;
//
//                  $$$$ We do not consider inner VLAN tags at this point!
//
bool SwitchCtrl_Session_SubnetUNI::createEFLOWs_TL1(String& vcgName, int vlanLow, int vlanHigh)
{
    int ret = 0;
    char packetType[60];
    String suppTtp, ettpName;

    if (vlanLow == 0 && vlanHigh == 0)
        sprintf(packetType, "pkttype=all");
    else if (vlanHigh > vlanLow)
        sprintf(packetType, "pkttype=single_vlan_tag,outervlanidrange=%d&&%d", vlanLow,vlanHigh);
    else
        sprintf(packetType, "pkttype=single_vlan_tag,outervlanidrange=%d", vlanLow);

    getCienaLogicalPortString(suppTtp, ettpName);

    sprintf(bufCmd, "ent-eflow::dcs_eflow_%s_in:%d:::ingressporttype=ettp,ingressportname=%s,%s,,priority=1&&8,egressporttype=vcg,egressportname=%s,cosmapping=cos_port_default,collectpm=yes;",
        vcgName.chars(), getNewCtag(), ettpName.chars(), packetType, vcgName.chars());

    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, vcgName, " Ingress-EFLOW has been created successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        // contine to egress EFLOW creation ...
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " Ingress-EFLOW creation has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;


    sprintf(bufCmd, "ent-eflow::dcs_eflow_%s_out:%d:::ingressporttype=vcg,ingressportname=%s,%s,,priority=1&&8,egressporttype=ettp,egressportname=%s,cosmapping=cos_port_default,collectpm=yes;",
        vcgName.chars(), getNewCtag(), vcgName.chars(), packetType, ettpName.chars());

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, vcgName, " Egress-EFLOW has been created successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " Egress-EFLOW creation has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        //$$$$ Delete ingress EFLOW
        return false;
    }
    else 
        goto _out;

_out:
        LOG(3)(Log::MPLS, vcgName, " EFLOWs creation via TL1_TELNET failed...\n", bufCmd);
        return false;
}

//dlt-elow::myeflow1:myctag;
bool SwitchCtrl_Session_SubnetUNI::deleteEFLOWs_TL1(String& vcgName)
{
    int ret = 0;

    sprintf(bufCmd, "dlt-eflow::dcs_eflow_%s_in:%d;", vcgName.chars(), getNewCtag());

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, vcgName, " Ingress-EFLOW has been deleted successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        // contine to egress EFLOW creation ...
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " Ingress-EFLOW deletion has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

    sprintf(bufCmd, "dlt-eflow::dcs_eflow_%s_out:%d;", vcgName.chars(), getNewCtag());

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, vcgName, " Egress-EFLOW has been deleted successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " Egress-EFLOW deletion has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(3)(Log::MPLS, vcgName, " EFLOWs deletion via TL1_TELNET failed...\n", bufCmd);
        return false;
}

//rtrv-eflow::myeflow1:myctag;
bool SwitchCtrl_Session_SubnetUNI::hasEFLOW_TL1(String& vcgName, bool ingress)
{
    int ret = 0;

    sprintf(bufCmd, "rtrv-eflow::dcs_eflow_%s_%s:%d;", vcgName.chars(), ingress? "in":"out", getNewCtag());

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(4)(Log::MPLS, vcgName, (ingress? "_in":"_out"), " EFLOW does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(4)(Log::MPLS, vcgName, (ingress? "_in":"_out"), " EFLOW does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(4)(Log::MPLS, vcgName, (ingress? "_in":"_out"), " EFLOW existence checking via TL1_TELNET failed...\n", bufCmd);
        return false;    
}

//ENT-VCG::NAME=vcg01:456::,PST=is,SUPPTTP=1-A-3-1,CRCTYPE=CRC_32,,,FRAMINGMODE=GFP,
//TUNNELPEERTYPE=ETTP,TUNNELPEERNAME=1-A-3-1-1,,GFPFCSENABLED=yes,,,GROUPMEM=1&&3,,;
bool SwitchCtrl_Session_SubnetUNI::createVCG_TL1(String& vcgName, bool tunnelMode)
{
    int ret = 0;
    char ctag[10];
    String suppTtp, tunnelPeerName, groupMem;

    sprintf(ctag, "%d", getNewCtag());

    vcgName = "dcs_vcg_";
    vcgName += (const char*)ctag;

    getCienaLogicalPortString(suppTtp, tunnelPeerName);
    getCienaTimeslotsString(groupMem);
    if ((groupMem).empty())
    {
        LOG(1)(Log::MPLS, "getCienaTimeslotsString failed to find available time slots...");
        vcgName = "";
        return false;
    }

    String cmdString = "ent-vcg::name=";
    cmdString += vcgName;
    cmdString += ":";
    cmdString += (const char*)ctag;
    cmdString += "::alias=";
    cmdString += lspName;
    cmdString += ",pst=IS,suppttp=";
    cmdString += suppTtp;
    if (tunnelMode)
    {
        cmdString += ",crctype=CRC_32,,,framingmode=GFP,tunnelpeertype=ETTP,tunnelpeername=";
        cmdString += tunnelPeerName;
    }
    else
    {
        cmdString += ",crctype=CRC_32,,,framingmode=GFP,tunnelpeertype=NONE,";
    }
    cmdString += ",,gfpfcsenabled=YES,,,groupmem=";
    cmdString += groupMem;
    cmdString += ",,;";

    if ( (ret = writeShell((char*)cmdString.chars(), 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, vcgName, " has been created successfully.\n", cmdString);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " creation has been denied.\n", cmdString);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        vcgName = "";
        return false;
    }
    else 
        goto _out;

_out:
        LOG(3)(Log::MPLS, vcgName, " creation via TL1_TELNET failed...\n", cmdString);
        vcgName = "";
        return false;
}

//ED-VCG::NAME=vcg01:123::,PST=OOS;
//DLT-VCG::NAME=vcg01:123;
bool SwitchCtrl_Session_SubnetUNI::deleteVCG_TL1(String& vcgName)
{
    int ret = 0;
    uint32 ctag = getNewCtag();

    sprintf(bufCmd, "ed-vcg::name=%s:%d::,pst=OOS;", vcgName.chars(), ctag);
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, vcgName, " status has been set to OOS.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
	//continue to next command ...
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " status change (to OOS) has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
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
        LOG(3)(Log::MPLS, vcgName, " has been deleted successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " deletion has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(3)(Log::MPLS, vcgName, " change/deletion via TL1_TELNET failed...\n", bufCmd);
        return false;
}

bool SwitchCtrl_Session_SubnetUNI::hasVCG_TL1(String& vcgName)
{
    int ret = 0;

    sprintf( bufCmd, "rtrv-vcg::%s:%d;\r", vcgName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, vcgName, " VCG does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, "TRUNCATED\"", true, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, vcgName, " VCG does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(3)(Log::MPLS, vcgName, " VCG existence checking via TL1_TELNET failed...\n", bufCmd);
        return false;    
}

//;ENT-GTP::gtp1:123::lbl=label,,ctp=vcg01-CTP-1&vcg01-CTP-2&vcg01-CTP-3&vcg01-CTP-4;
bool SwitchCtrl_Session_SubnetUNI::createGTP_TL1(String& gtpName, String& vcgName)
{
    int ret = 0;
    char ctag[10];
    sprintf(ctag, "%d", getNewCtag());
    gtpName = "dcs_gtp_";
    gtpName += ctag;

    String ctpGroupStringArray[4];
    String* pString = ctpGroupStringArray;
    getCienaCTPGroupsInVCG(pString, vcgName);
    if (ctpGroupStringArray[0].empty() || numGroups == 0)
    {
        LOG(1)(Log::MPLS, "getCienaCTPGroupsInVCG returned empty strings");
        gtpName = "";
        return false;
    }

    int group;
    for (group = 0; group < numGroups; group++)
    {
        assert(!ctpGroupStringArray[group].empty());

        sprintf( bufCmd, "ent-gtp::%s-%d:%s::lbl=gtp-%s,,ctp=%s;", gtpName.chars(), group+1, ctag, vcgName.chars(), ctpGroupStringArray[group].chars() );

        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, gtpName, "-", group+1, " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, gtpName, "-", group+1, " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            gtpName = "";
            return false;
        }
        else 
            goto _out;
    }

    return true;
    
_out:
    LOG(5)(Log::MPLS, gtpName, "-", group+1, " creation via TL1_TELNET failed...\n", bufCmd);
    gtpName = "";
    return false;    
}

//;DLT-GTP::gtp1:123;
bool SwitchCtrl_Session_SubnetUNI::deleteGTP_TL1(String& gtpName)
{
    int ret = 0;
    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "dlt-gtp::%s-%d:%d;", gtpName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, gtpName, "-", group+1, " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, gtpName, "-", group+1, " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

    return true;

_out:
    LOG(5)(Log::MPLS, gtpName, "-", group+1, " deletion via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

bool SwitchCtrl_Session_SubnetUNI::hasGTP_TL1(String& gtpName)
{
    int ret = 0;

    //only checking the first group if more than one.
    sprintf( bufCmd, "rtrv-gtp::%s-1:%d;", gtpName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, gtpName, " GTP does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, gtpName, " GTP does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
    LOG(3)(Log::MPLS, gtpName, " GTP existence checking via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

//;ent-snc-stspc:SEAT:gtp_x,1-a-5-1-1&&21:myctag::name=sncname,type=dynamic,rmnode=GRNOC,lep=gtp_nametype,conndir=bi_direction,prtt=aps_vlsr_unprotected,pst=is;
bool SwitchCtrl_Session_SubnetUNI::createSNC_TL1(String& sncName, String& gtpName)
{
    int ret = 0;
    char ctag[10];

    sprintf(ctag, "%d", getNewCtag());
    sncName = "dcs_snc_";
    sncName += ctag;

    assert(numGroups > 0);
    // get destination time slots!
    String destTimeslotsStringArray[4];
    String* pString = destTimeslotsStringArray;
    getCienaDestTimeslotsString(pString);
    if (destTimeslotsStringArray[0].empty())
    {
        LOG(1)(Log::MPLS, "getCienaDestTimeslotsString returned empty strings.");
        sncName = "";
        return false;
    }

    //creatign DTL and DTL-SET
    String dtlString;
    if (DTL.count > 0)
    {
        getDTLString(dtlString);
        if (dtlString.empty())
        {
            LOG(1)(Log::MPLS, "getDTLString returned empty strings.");
            sncName = "";
            return false;
        }

        //ent-dtl::dtl1:123::NODENAME1=SEAT,OSRPLTPID1=1,TERMNODENAME=GRNOC;
        //DTL named 'sncname-dtl'
        sprintf( bufCmd, "ent-dtl::%s-dtl:%d::%s;", sncName.chars(), getNewCtag(), dtlString.chars());
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl", " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(4)(Log::MPLS, sncName, "-dtl", " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            sncName = "";
            return false;
        }
        else 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl", " creation via TL1_TELNET failed...\n", bufCmd);
            return false;
        }

        //ent-dtl-set::dtlset1:123::WRKNM=dtl1,;
        //DTL-SET named 'sncname-dtl_set'
        sprintf( bufCmd, "ent-dtl-set::%s-dtl_set:%d::wrknm=%s,;", sncName.chars(), getNewCtag(), sncName.chars());
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl_set", " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(4)(Log::MPLS, sncName, "-dtl_set", " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            sncName = "";
            return false;
        }
        else 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl_set", " deletion via TL1_TELNET failed...\n", bufCmd);
            return false;
        }
    }

    int group;
    for (group = 0; group < numGroups; group++)
    {
        char dtl_cstr[40];
        dtl_cstr[0] = 0;
        if (!dtlString.empty())
        {
            sprintf(dtl_cstr, "dtlsn=%s-dtl_set, dtlexcl=yes,", sncName.chars());
        }
        sprintf( bufCmd, "ent-snc-stspc:%s:%s-%d,%s:%s::name=%s-%d,type=dynamic,rmnode=%s,lep=gtp_nametype,alias=%s,%sconndir=bi_direction,meshrst=no,prtt=aps_vlsr_unprotected,pst=is;",
            (const char*)subnetUniSrc.node_name, gtpName.chars(), group+1, destTimeslotsStringArray[group].chars(), ctag, sncName.chars(), group+1, 
                (const char*)subnetUniDest.node_name, lspName.chars(), dtl_cstr);

        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %s COMPLD", ctag);
        sprintf(strDENY, "M  %s DENY", ctag);
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, sncName, "-", group+1, " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, sncName, "-", group+1, " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            sncName = "";
            return false;
        }
        else 
            goto _out;
    }

    return true;

_out:
    LOG(5)(Log::MPLS, sncName, "-", group+1, " creation via TL1_TELNET failed...\n", bufCmd);
    sncName = "";
    return false;    
}

//ED-SNC-STSPC::snc001:123::,PST=OOS;
//dlt-snc-stspc::snc_2:myctag;
bool SwitchCtrl_Session_SubnetUNI::deleteSNC_TL1(String& sncName)
{
    int ret = 0;
    String dtlString;

    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "ed-snc-stspc::%s-%d:%d::,pst=oos;", sncName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, sncName, "-", group+1, " state has been changed into OOS.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            //continue to next command ...
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, sncName, "-", group+1, " state change to OOS has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;

        sprintf( bufCmd, "dlt-snc-stspc::%s-%d:%d;", sncName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, sncName, "-", group+1, " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, sncName, "-", group+1, " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

    getDTLString(dtlString);
    if (!dtlString.empty())
    {
        //dlt-dtl-set::dtlset2:123;
        sprintf( bufCmd, "dlt-dtl-set::%s-dtl_set:%d;", sncName.chars(), getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl_set", " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(4)(Log::MPLS, sncName, "-dtl_set", " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl_set", " deletion via TL1_TELNET failed...\n", bufCmd);
            return false;
        }
        
        //dlt-dtl::dtl1:123;
        sprintf( bufCmd, "dlt-dtl::%s-dtl:%d;", sncName.chars(), getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl", " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(4)(Log::MPLS, sncName, "-dtl", " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
        {
            LOG(4)(Log::MPLS, sncName, "-dtl", " deletion via TL1_TELNET failed...\n", bufCmd);
            return false;
        }
    }
	
    return true;

_out:
    LOG(5)(Log::MPLS, sncName, "-", group+1, " change/deletion via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

bool SwitchCtrl_Session_SubnetUNI::hasSNC_TL1(String& sncName)
{
    int ret = 0;

    //only checking the first SNC if more than one SNCs are created for the LSP.
    sprintf( bufCmd, "rtrv-snc-stspc::%s-1:%d;", sncName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, sncName, " SNC does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, sncName, " SNC does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
    LOG(3)(Log::MPLS, sncName, " SNC existence checking via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

//;ent-crs-stspc::fromendpoint=gtp01,toendpoint=gtp02:myctag::name=crs01,fromtype=gtp,totype=gtp,;
bool SwitchCtrl_Session_SubnetUNI::createCRS_TL1(String& crsName, String& gtpName)
{
    int ret = 0;
    char ctag[10];

    sprintf(ctag, "%d", getNewCtag());
    crsName = "dcs_crs_";
    crsName += ctag;

    // get destination time slots!
    String destGtpName;
    getPeerCRS_GTP(destGtpName);
    if (destGtpName.empty())
    {
        LOG(1)(Log::MPLS, "createCRS_TL1:getPeerCRS_GTP returned empty string.");
        crsName = "";
        return false;
    }

    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "ent-crs-stspc::fromendpoint=%s-%d,toendpoint=%s-%d:%s::name=%s-%d,fromtype=gtp,totype=gtp, alias=%s;",
            gtpName.chars(), group+1, destGtpName.chars(), group+1, ctag, crsName.chars(), group+1, lspName.chars());

        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, crsName, "-", group+1, " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, crsName, "-", group+1, " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            crsName = "";
            return false;
        }
        else 
            goto _out;
    }

    return true;

_out:
    LOG(5)(Log::MPLS, crsName, "-", group+1, " creation via TL1_TELNET failed...\n", bufCmd);
    crsName = "";
    return false;
}

bool SwitchCtrl_Session_SubnetUNI::deleteCRS_TL1(String& crsName)
{
    int ret = 0;

    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "ed-crs-stspc::name=%s-%d:%d::,pst=oos;", crsName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, crsName, "-", group+1, " state has been changed into OOS.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            //continue to next command ...
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, crsName, "-", group+1, " state change to OOS has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;

        sprintf( bufCmd, "dlt-crs-stspc::name=%s-%d:%d;", crsName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(5)(Log::MPLS, crsName, "-", group+1, " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(5)(Log::MPLS, crsName, "-", group+1, " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

    return true;

_out:
    LOG(5)(Log::MPLS, crsName, "-", group+1, " change/deletion via TL1_TELNET failed...\n", bufCmd);
    return false;
}

//rtrv-crs-stspc::name=crsName:123;
bool SwitchCtrl_Session_SubnetUNI::hasCRS_TL1(String& crsName)
{
    int ret = 0;

    //only checking the first CRS if more than one is created for the LSP.
    sprintf( bufCmd, "rtrv-crs-stspc::name=%s-1:%d;", crsName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, crsName, " XConn does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(3)(Log::MPLS, crsName, " XConn does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(3)(Log::MPLS, crsName, " Xconn existence checking via TL1_TELNET failed...\n", bufCmd);
        return false;    
}


//rtrv-ocn::1-A-3-1:mytag;
SONET_CATUNIT SwitchCtrl_Session_SubnetUNI::getConcatenationUnit_TL1(uint32 logicalPort)
{
    int ret = 0;
    SONET_CATUNIT funcRet = CATUNIT_UNKNOWN;
    String OMPortString, ETTPString;

    getCienaLogicalPortString(OMPortString, ETTPString, logicalPort);

    sprintf(bufCmd, "rtrv-ocn::%s:%d;", OMPortString.chars(), getCurrentCtag());
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, OMPortString, " concatentation type has been found.\n", bufCmd);
        ret = ReadShellPattern(bufCmd, "Virtual 50MBPS", "Virtual 150MBPS", "OSPFCOST", NULL, 5);
        if (ret == 1)
            funcRet = CATUNIT_50MBPS;
        else if (ret == 2)
            funcRet = CATUNIT_150MBPS;

        readShell(SWITCH_PROMPT, NULL, 1, 5);
    }
    else if (ret == 2) 
    {
        LOG(3)(Log::MPLS, OMPortString, " concatentation type checking has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return funcRet;
    }
    else
        goto _out;

_out:
    if (CATUNIT_UNKNOWN == funcRet)
    {
        LOG(3)(Log::MPLS, OMPortString, " concatentation type checking via TL1_TELNET failed...\n", bufCmd);
    }
    return funcRet;
}

// Get timeslots map based on the allocated OCN timeslots on the card that has the logicalPort
// OCN timeslots are allocated when a cross connect is in place
bool SwitchCtrl_Session_SubnetUNI::syncTimeslotsMapOCN_TL1(uint8 *ts_bitmask, uint32 logicalPort)
{
    int ret = 0;
    String OMPortString, ETTPString;
    char* pstr;
    int ts;

    assert(ts_bitmask);
    //@@@@ Should not set all bits. Otherwise, the timeslot configuration in ospfd.conf will be overriden.
    //memset(ts_bitmask, 0xff, MAX_TIMESLOTS_NUM/8);

    getCienaLogicalPortString(OMPortString, ETTPString, logicalPort);

    sprintf(bufCmd, "rtrv-ocn::%s:%d;", OMPortString.chars(), getCurrentCtag());
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(3)(Log::MPLS, OMPortString, " syncTimeslotsMapOCN_TL1 method has retrieved timeslots suceessfully.\n", bufCmd);
        ret = ReadShellPattern(bufCmd, NULL, NULL, ",NOACT", NULL, 5);
        if (ret == 0) {
            bufCmd[strlen(bufCmd) - 6] = 0;
            pstr = strstr(bufCmd, "TIMESLOTMAP=");
            if (!pstr)
                goto _out;
            pstr = strtok(pstr+12, " &");
            while (pstr)
            {
                if (sscanf(pstr, "%d", &ts) == 1)
                    RESET_TIMESLOT(ts_bitmask, ts);
                pstr = strtok(NULL, " &");
            }
        }
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2) 
    {
        LOG(3)(Log::MPLS, OMPortString, " syncTimeslotsMapOCN_TL1 retrieving timeslots has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else
        goto _out;

_out:
    LOG(3)(Log::MPLS, OMPortString, " syncTimeslotsMapOCN_TL1 method via TL1_TELNET failed...\n", bufCmd);
    return false;
}

// Get timeslots map based on existing VCGs on the card that has the logicalPort.
// This is more acturate timeslots map to avoid conflicting timeslots allocation to simultaneous VCGs.
bool SwitchCtrl_Session_SubnetUNI::syncTimeslotsMapVCG_TL1(uint8 *ts_bitmask, uint32 logicalPort)
{
    int ret = 0;
    String OMPortString, ETTPString;
    char* pstr;
    int ts, ts1, ts2;

    assert(ts_bitmask);
    //@@@@ Should not set all bits. Otherwise, the timeslot configuration in ospfd.conf will be overriden.
    //memset(ts_bitmask, 0xff, MAX_TIMESLOTS_NUM/8);

    getCienaLogicalPortString(OMPortString, ETTPString, logicalPort);

    sprintf(bufCmd, "rtrv-vcg::all:%d;\r", getCurrentCtag());
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        ret = readShell("   /* Empty", "   \"", 1, 5);
        if (ret == 1)
        {
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return true;
        }
        else if (ret == 2)
        {
            while ((ret = ReadShellPattern(bufCmd, (char*)OMPortString.chars(), "GROUPMEM=", "VCGFAILUREBASESEV=",  ";", 5)) != READ_STOP)
            { // if (ret == 3), we have reach the end, i.e., ";"...
                if (ret == 1)
                {
                    pstr = strstr(bufCmd, "GROUPMEM=");
                    if (!pstr)
                        goto _out;
                    ret = sscanf(pstr+9, "%d&&%d", &ts1, &ts2);
                    if (ret == 1)
                        ts2 = ts1;
                    else if (ret <= 0)
                        goto _out;
                    for (ts = ts1; ts <= ts2; ts++)
                    {
                        RESET_TIMESLOT(ts_bitmask, ts);
                    }
                }
                else if (ret == 2)
                    continue; // not one of the VCGs we are looing for
                else
                    goto _out; // wrong
            }
            LOG(3)(Log::MPLS, OMPortString, " syncTimeslotsMapVCG_TL1 method has retrieved timeslots suceessfully.\n", bufCmd);
            return true;    
        }
        else
            goto _out;
    }
    else if (ret == 2) 
    {
        LOG(3)(Log::MPLS, OMPortString, " syncTimeslotsMapVCG_TL1 retrieving timeslots has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else
        goto _out;

_out:
    LOG(3)(Log::MPLS, OMPortString, " syncTimeslotsMapVCG_TL1 method via TL1_TELNET failed...\n", bufCmd);
    return false;
}

bool SwitchCtrl_Session_SubnetUNI::syncTimeslotsMap() 
{
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    bool ret = syncTimeslotsMapVCG_TL1(pUniData->timeslot_bitmask);

    //TODO: make this an inline function...
    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    assert (sonet_tb1);
    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ret)
    {
        uint8 ts, ts_count;
        bool ts_ok = false;
        for (ts = 1; ts <= MAX_TIMESLOTS_NUM; ts++)
        {
            if (HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
            {
                ts_count = 1; ts++;
                for ( ; HAS_TIMESLOT(pUniData->timeslot_bitmask, ts) && ts <= MAX_TIMESLOTS_NUM; ts++)
                    ts_count++;
                if (ts_count >= ts_num)
                {
                    pUniData->first_timeslot = ts-ts_count;
                    ts_ok = true;
                    break;
                }
            }
        }
        if (!ts_ok)
        {
            LOG(1)(Log::MPLS, "Warning (syncTimeslotsMap): insufficient number of contigious time slots for this request.\n");
        } 

    }
    return ret;
}

bool SwitchCtrl_Session_SubnetUNI::verifyTimeslotsMap() 
{
    uint8 timeslots[MAX_TIMESLOTS_NUM/8]; //changing nothing in the actual UNIdata
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    memcpy(timeslots, pUniData->timeslot_bitmask, MAX_TIMESLOTS_NUM/8);
    bool ret = syncTimeslotsMapVCG_TL1(timeslots);

    //TODO: make this an inline function...
    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    assert (sonet_tb1);
    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ret)
    {
        uint8 ts, ts_count = 0;
        bool ts_ok = false;
        for (ts = pUniData->first_timeslot; ts <= MAX_TIMESLOTS_NUM && HAS_TIMESLOT(timeslots, ts); ts++)
        {
            ts_count++;
            if (ts_count >=  ts_num)
            {
                ts_ok = true;
                break;
            }
        }
        if (!ts_ok)
        {
            LOG(1)(Log::MPLS, "Warning (verifyTimeslotsMap): the range of contigious timeslots suggested by signaling may overlap with existing VCGs.\n");
        } 
    }
    return ret;
}

bool SwitchCtrl_Session_SubnetUNI::hasSystemSNCHolindgCurrentVCG_TL1(bool& noError)
{
    int ret = 0;
    noError = true;
    SubnetUNI_Data *pUniData = &subnetUniDest;
    if (pUniData->first_timeslot == 0 || pUniData->first_timeslot > MAX_TIMESLOTS_NUM || pUniData->logical_port == 0)
    {
        LOG(1)(Log::MPLS, "invalid subnetUniDest information.\n");
        return false;
    }

    String OMPortString, ETTPString;
    char* pstr;
    int ts1;
    char fromEndPointPattern2[20];
    getCienaLogicalPortString(OMPortString, ETTPString, ntohl(pUniData->logical_port));
    sprintf(fromEndPointPattern2, "_%s_S", OMPortString.chars());

    sprintf(bufCmd, "rtrv-snc-stspc::all:%d;\r", getCurrentCtag());
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        ret = readShell("   /* Empty", "   \"", 1, 5);
        if (ret == 1)
        {
            LOG(2)(Log::MPLS, " hasSystemSNCHolindgCurrentVCG_TL1 method found no SNC holding the current VCG.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else if (ret == 2)
        {
            while ((ret = ReadShellPattern(bufCmd, "FROMENDPOINT=dcs_gtp_", fromEndPointPattern2, "MAXADMINWEIGHT=", ";", 5)) != READ_STOP)
            { // if (ret == 3), we have reach the end, i.e., ";"...
                if (ret == 0) // this is an irrelevant SNC 
                    continue;
                if (ret == 1) // this is an SNC originating at source point...
                    continue;
                else if (ret == 2)
                {
                    pstr = strstr(bufCmd, fromEndPointPattern2);
                    ret = sscanf(pstr+10, "%d", &ts1);
                    if (ret != 1)
                        goto _out;

                    //TODO: make this an inline function...
                    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
                    assert (sonet_tb1);
                    uint8 ts_num = 0;
                    switch (sonet_tb1->getSignalType())
                    {
                    case SONET_TSpec::S_STS1SPE_VC3:
                    case SONET_TSpec::S_STS1_STM0:
                        ts_num = sonet_tb1->getNCC();
                        if (ptpCatUnit == CATUNIT_150MBPS)
                            ts_num = ((ts_num+2)/3)*3;
                        break;

                    case SONET_TSpec::S_STS3CSPE_VC4:
                    case SONET_TSpec::S_STS3_STM1:
                        ts_num = sonet_tb1->getNCC() * 3;
                        break;
                    }
                    //conditions for VCG-owned SNC detection
                    if ((ts1+1 - pUniData->first_timeslot) %48 == 0 &&  ts1+1 - pUniData->first_timeslot < ts_num)
                    {
                        LOG(2)(Log::MPLS, " hasSystemSNCHolindgCurrentVCG_TL1 method detected an SNC holding the current VCG.\n", bufCmd);
                        ret = readShell(SWITCH_PROMPT, NULL, 1, 5);
                        return true;                        
                    }
                }
                else
                    goto _out; // wrong
            }
            LOG(2)(Log::MPLS, " hasSystemSNCHolindgCurrentVCG_TL1 method found no SNC holding the current VCG.\n", bufCmd);
            return false;    
        }
        else
            goto _out;
    }
    else if (ret == 2) 
    {
        LOG(2)(Log::MPLS, " hasSystemSNCHolindgCurrentVCG_TL1 retrieving SNC-STSPC list was denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else
        goto _out;

_out:
    noError = false;
    LOG(3)(Log::MPLS, OMPortString, " hasSystemSNCHolindgCurrentVCG_TL1 method via TL1_TELNET failed...\n", bufCmd);
    return false;
}

bool SwitchCtrl_Session_SubnetUNI::waitUntilSystemSNCDisapear()
{
    bool noError = true;
    int counter = 5;
    do {
        if (!noError)
            return false;
        LOG(1)(Log::MPLS, " Child-Process::waitUntilSystemSNCDisapear ... 3x5 seconds left.\n");
        sleep(3); //sleeping one second and try again
        counter--;
        if (counter == 0) //timeout!
            return false;
    } while (hasSystemSNCHolindgCurrentVCG_TL1(noError));
    return true;
}

