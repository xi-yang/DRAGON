/****************************************************************************

Switch Control Module source file SwitchCtrl_Global.cc
Created by Xi Yang @ 01/12/2006
Extended from SNMP_Global.cc by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Global.h"
#include "RSVP_Log.h"
#include "RSVP_Message.h"
#include "SNMP_Session.h"
#include "CLI_Session.h"
#include "RSVP_RoutingService.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_PHopSB.h"
#include "RSVP_PSB.h"
#include "RSVP_Session.h"
#include "SwitchCtrl_Session_Force10E600.h"
#include "SwitchCtrl_Session_RaptorER1010.h"
#include "SwitchCtrl_Session_Catalyst3750.h"
#include "SwitchCtrl_Session_Catalyst6500.h"
#include "SwitchCtrl_Session_HP5406.h"
#include "SwitchCtrl_Session_SMC10G8708.h"



#ifdef Linux
#include "SwitchCtrl_Session_Linux.h"
#endif

#include <signal.h>

////////////////Global Definitions//////////////

LocalIdList SwitchCtrl_Global::localIdList;


/////////////////////////////////////////////////////////////
////////////////SwitchCtrl_Session Implementation////////////////
/////////////////////////////////////////////////////////////

bool SwitchCtrl_Session::connectSwitch() 
{
    if (!snmp_enabled)
        return false;

    if (!SwitchCtrl_Global::static_connectSwitch(snmpSessionHandle, switchInetAddr))
        return false;

    //Read Ethernet switch vendor info and verify against the compilation configuration
    if (!(getSwitchVendorInfo() && readVLANFromSwitch())){ 
        LOG(1)( Log::MPLS, "VLSR: Error while reading switch vendor info or VLANs, disconnecting from switch..");
    	disconnectSwitch();
    	return false;
    }

    LOG(2)( Log::MPLS, "VLSR: Successfully connected to switch (via SNMP): ", switchInetAddr);
    active = true;
    return true;
}

void SwitchCtrl_Session::disconnectSwitch() 
{ 
    if (snmp_enabled)
        SwitchCtrl_Global::static_disconnectSwitch(snmpSessionHandle); 
    //RSVP_Global::switchController->removeSession(this);
}

bool SwitchCtrl_Session::getSwitchVendorInfo()
{ 
    bool ret = false;
    if (!snmp_enabled)
        return ret;

    ret = SwitchCtrl_Global::static_getSwitchVendorInfo(snmpSessionHandle, vendor, venderSystemDescription); 
    switch (vendor) {
    case RFC2674:
    case IntelES530:
        rfc2674_compatible = snmp_enabled = true;
        break;
    case Catalyst3750:
    case Catalyst6500:
    	snmp_enabled = true;
     	rfc2674_compatible = false;
    }

    return ret;
}

void SwitchCtrl_Session::addRsvpSessionReference(Session* rsvpSession)
{
    RsvpSessionList::Iterator it;
    for (it = rsvpSessionRefList.begin(); it != rsvpSessionRefList.end(); ++it)
    {
        if ((*it) == rsvpSession)
            return;
    }
    rsvpSessionRefList.push_back(rsvpSession);
}

bool SwitchCtrl_Session::removeRsvpSessionReference(Session* rsvpSession)
{
    RsvpSessionList::Iterator it;
    for (it = rsvpSessionRefList.begin(); it != rsvpSessionRefList.end(); ++it)
    {
        if ((*it) == rsvpSession)
        {
            it = rsvpSessionRefList.erase(it);
            return true;
        }
    }
    return false;
}

//VTAG mutral-exclusion feature --> Review
/*
bool SwitchCtrl_Session::resetVtagBitMask(uint8* bitmask)
{
    bool ret = false;

    vlanPortMapList::ConstIterator iter = vlanPortMapListAll.begin();
    for (; iter != vlanPortMapListAll.end(); ++iter) {
        RESET_VLAN(bitmask, (*iter).vid);
        ret = true;
    }

    return ret;
}
*/

bool SwitchCtrl_Session::createVLAN(uint32 &vlanID)
{
    vlanPortMapList::ConstIterator iter;

    //@@@@ vlanID == 0 is supposed to create an arbitrary new VLAN and re-assign the vlanID.
    //@@@@ For now, we igore this case and only create VLAN for a specified vlanID > 0.
    if (vlanID == 0)
        return false;

    //check if the VLAN has already been existing
    for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
        if ((*iter).vid == vlanID)
        {
            LOG(3)( Log::MPLS, "Warning : VLAN ", vlanID,  " shows up  in vlanPortMapListAll but not on switch --> corrupted VLSR data?");
            return false;
        }
    }

    //otherwise, create it
    if (!hook_createVLAN(vlanID))
        return false;

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    /*
    vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    vlanPortMapListUntagged.push_back(vpm);
    */
    return true;
}

bool SwitchCtrl_Session::removeVLAN(const uint32 vlanID)
{
    vlanPortMapList::ConstIterator iter;

    if (vlanID == 0)
        return false;

    if (!hook_removeVLAN(vlanID))
        return false;

    //remove the vlan from vlanPortMapLists
    for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
        if ((*iter).vid == vlanID) {
            vlanPortMapListAll.erase(iter);
            break;
        }
    }

    for (iter = vlanPortMapListUntagged.begin(); iter != vlanPortMapListUntagged.end(); ++iter) {
        if ((*iter).vid == vlanID) {
            vlanPortMapListUntagged.erase(iter);
            break;
        }
    }        

    return true;
}

bool SwitchCtrl_Session::isVLANEmpty(const uint32 vlanID)
{
    vlanPortMapList::ConstIterator iter;
    for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
        if ((*iter).vid == vlanID && hook_isVLANEmpty(*iter))
            return true;
    }
    return false;
}

const uint32 SwitchCtrl_Session::findEmptyVLAN()
{
    vlanPortMapList::ConstIterator iter;
    uint32 ret =1;
    for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
        if (hook_isVLANEmpty(*iter) && (*iter).vid != 1)
            return (*iter).vid;
        else if (ret == (*iter).vid)
            ret = (*iter).vid+1;
    }
    return ret;
}

void SwitchCtrl_Session::readVlanPortMapBranch(const char* oid_str, vlanPortMapList &vpmList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    int status;
    vlanPortMap portmap;
    bool running = true;
    size_t rootlen;

    //Since the function takes oid_str as argument, can be used even for switches 
    // that are not RFC2674 compatible such as Cisco Catalyst
    //if (!rfc2674_compatible || !snmp_enabled)
    if (!snmp_enabled)
        return;

    status = read_objid(oid_str, anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    vpmList.clear();
    while (running) {
        // Create the PDU for the data for our request.
        pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
        snmp_add_null_var(pdu, anOID, anOID_len);
        // Send the Request out.
        status = snmp_synch_response(snmpSessionHandle, pdu, &response);
        if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
            for (vars = response->variables; vars; vars = vars->next_variable) {
                if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                    running = false;
                    continue;
                }

                hook_getPortMapFromSnmpVars(portmap, vars);
                 
                vpmList.push_back(portmap);
                if ((vars->type != SNMP_ENDOFMIBVIEW) &&
                    (vars->type != SNMP_NOSUCHOBJECT) &&
                    (vars->type != SNMP_NOSUCHINSTANCE)) {
                    memcpy((char *)anOID, (char *)vars->name, vars->name_length * sizeof(oid));
                    anOID_len = vars->name_length;
                }
                else {
                    running = 0;
                }
            }
        }
        else {
            running = false;
            LOG(1)( Log::MPLS, "VLSR: Error while reading VLAN/port mapping from Switch!");
        }
        if(response) snmp_free_pdu(response);
    }
}

bool SwitchCtrl_Session::readVLANFromSwitch()
{
    bool ret = true;

    if (!hook_createVlanInterfaceToIDRefTable(vlanRefIdConvList))
        return false;

    if (!hook_createPortToIDRefTable(portRefIdConvList))
        return false;

    // Read and record the list of ALL ports and their vlan associations
    if (rfc2674_compatible) {
    	readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.2", vlanPortMapListAll);
    }
    else if (vendor == Catalyst3750 || vendor == Catalyst6500) {
	readVlanPortMapListAllBranch(vlanPortMapListAll);
    }
    else {
    	readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.2", vlanPortMapListAll);
    }
    if (vlanPortMapListAll.size() == 0)
        ret = false;

    // Read and record the list of UNTAGGED/NON-TRUNK ports and their vlan associations
    if (rfc2674_compatible) {
    	readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.4", vlanPortMapListUntagged);
    }
    else if (vendor == Catalyst3750 || vendor == Catalyst6500) {
    	readVlanPortMapBranch(".1.3.6.1.4.1.9.9.68.1.2.1.1.3", vlanPortMapListUntagged);
    }
    else {
    	readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.4", vlanPortMapListUntagged);
    }
    if (vlanPortMapListUntagged.size() == 0)
        ret = false;

    /*
     *  Dell PowerConnect 5324/6024/6024F do not show VLAN 1 in the RFC2674 MIB,
     *  so if VLAN 1 is the only VLAN configured, *.size() == 0 above,
     *  but return true anyways to prevent the VLSR from disconnecting..
     */ 
    if (String("Ethernet Switch") == venderSystemDescription ||
        String("Neyland 24T") == venderSystemDescription ||
        String("Ethernet Routing Switch") == venderSystemDescription) {
      if (vlanPortMapListAll.size() == 0 || vlanPortMapListUntagged.size() == 0)
	ret = true;
    }

    LOG(4)( Log::MPLS, "VLSR: received VLAN/port mapping from Switch, total VLANs=", 
	    vlanPortMapListAll.size(), ", untagged VLANs=", vlanPortMapListUntagged.size());

    return ret;
}

uint32 SwitchCtrl_Session::getVLANbyPort(uint32 port){
    vlanPortMapList::Iterator iter;
    for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
        if (hook_hasPortinVlanPortMap(*iter, port))
            return (*iter).vid;
    }

    return 0;
}

uint32 SwitchCtrl_Session::getVLANListbyPort(uint32 port, SimpleList<uint32> &vlan_list){
    vlan_list.clear();
    vlanPortMapList::Iterator iter;
    for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
        if (hook_hasPortinVlanPortMap(*iter, port))
            vlan_list.push_back((*iter).vid);
    }

    return vlan_list.size();
}

uint32 SwitchCtrl_Session::getVLANbyUntaggedPort(uint32 port){
    vlanPortMapList::Iterator iter;
    for (iter = vlanPortMapListUntagged.begin(); iter != vlanPortMapListUntagged.end(); ++iter) {
        if (hook_hasPortinVlanPortMap(*iter, port))	                
            return (*iter).vid;
    }

    return 0;
}

bool SwitchCtrl_Session::verifyVLAN(uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.2";

    if (!active || !rfc2674_compatible || !snmp_enabled)
        return false;

    vlanID = hook_convertVLANIDToInterface(vlanID);

    if (vlanID == 0)
        return false;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // vlan port list 
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        	snmp_free_pdu(response);
                return true;
    	}
    if(response) 
        snmp_free_pdu(response);
    return false;
}



bool SwitchCtrl_Session::VLANHasTaggedPort(uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    netsnmp_variable_list *vars;
    int status;
    vlanPortMap portmap_all, portmap_untagged;
    String oid_str_all = ".1.3.6.1.2.1.17.7.1.4.3.1.2";    
    String oid_str_untagged = ".1.3.6.1.2.1.17.7.1.4.3.1.4";

    if (!active || !rfc2674_compatible || !snmp_enabled)
        return false;

    memset(&portmap_all, 0, sizeof(vlanPortMap));

    portmap_all.vid = hook_convertVLANIDToInterface(vlanID);
    portmap_untagged = portmap_all;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // all vlan ports list 
    sprintf(oid_str, "%s.%d", oid_str_all.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        vars = response->variables;

        hook_getPortMapFromSnmpVars(portmap_all, vars);
        if (portmap_all.vid == 0)
            return false;
    }
    else 
        return false;
    if(response) 
      snmp_free_pdu(response);

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // untagged vlan ports list 
    sprintf(oid_str, "%s.%d", oid_str_untagged.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {

        vars = response->variables;

        hook_getPortMapFromSnmpVars(portmap_untagged, vars);
        if (portmap_untagged.vid == 0)
            return false;
    }
    else 
        return false;
    if(response) 
      snmp_free_pdu(response);

    if (memcmp(&portmap_all, &portmap_untagged, sizeof(vlanPortMap)) == 0)
        return false;

    return true;
}

bool SwitchCtrl_Session::setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[128], oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.4";
    uint32 ports = 0;

    //not initialized or session has been disconnected; RFC2674 only!
    if (!active || !rfc2674_compatible || !snmp_enabled)
    	return false;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // vlan port list 
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	if (response->variables->val.integer){
    		ports = ntohl(*(response->variables->val.integer));
    		if (response->variables->val_len < 4){
    			uint32 mask = (uint32)0xFFFFFFFF << ((4-response->variables->val_len)*8); 
    			ports &= mask; 
    		}
    	}
    }
    if(response) 
        snmp_free_pdu(response);

    ports &= (~taggedPorts) & 0xFFFFFFFF; //take port away from those untagged
    pdu = snmp_pdu_create(SNMP_MSG_SET);
    // vlan port list 
    type='x';   
    sprintf(value, "%.8lx", (long unsigned int)0); // set all ports tagged
    status = snmp_add_var(pdu, anOID, anOID_len, type, value);
    sprintf(value, "%.8lx", (long unsigned int)ports); //restore those originally untagged ports execept the 'taggedPorts'
    status = snmp_add_var(pdu, anOID, anOID_len, type, value);
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	snmp_free_pdu(response);
    }
    else {
    	if (status == STAT_SUCCESS){
    	LOG(2)( Log::MPLS, "VLSR: SNMP: Setting VLAN Tag failed. Reason : ", snmp_errstring(response->errstat));
    	}
    	else
      		snmp_sess_perror("snmpset", snmpSessionHandle);
    	if(response)
        	snmp_free_pdu(response);
        return false;
    }
    return true;
}


bool SwitchCtrl_Session::adjustVLANbyLocalId(uint32 vlanID, uint32 lclID, uint32 trunkPort)
{
    // !!##!! Note that we assume that this VLAN does not contain any other pors not serving for this LSP at the edge.
    PortList portList;
    uint32 port;
    if (!hook_getPortListbyVLAN(portList, vlanID))
    {
        LOG(3)( Log::MPLS, "VLSR: adjustVLANbyLocalId: hook_getPortListbyVLAN(", vlanID, ") failed...");
        return false;
    }
    //remove all ports in the current portList but the trunkport
    PortList::Iterator iter = portList.begin();
    for (; iter != portList.end(); ++iter)
    {
        port = *iter;
        if (port != trunkPort)
        {
            LOG(4)( Log::MPLS, "VLSR: adjustVLANbyLocalId: Removing port ", port, " from VLAN ", vlanID);
            removePortFromVLAN(port, vlanID);
        }
    }

    // A better scheme is to compare the two separte list and remove and add the only necessary ports 
    // (no remove and move on the same port).
    SwitchCtrl_Global::getPortsByLocalId(portList, lclID);
    //move the adjusted ports (as represented by lclID) into the VLAN
    for (iter = portList.begin(); iter != portList.end(); ++iter)
    {
        port = *iter;
        if ((lclID >> 16) == LOCAL_ID_TYPE_GROUP)
        {
            LOG(4)( Log::MPLS, "VLSR: adjustVLANbyLocalId: Moving untagged port ", port, " into VLAN ", vlanID);
            movePortToVLANAsUntagged(port, vlanID);
        }
        else if ((lclID >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP)
        {
            LOG(4)( Log::MPLS, "VLSR: adjustVLANbyLocalId: Moving tagged port ", port, " into VLAN ", vlanID);
            movePortToVLANAsTagged(port, vlanID);
        }
	else
       {
            LOG(2)( Log::MPLS, "VLSR: adjustVLANbyLocalId: Invalid localID", lclID);
            return false;
       }
    }

    return true;
}


bool SwitchCtrl_Session::hasVLSRouteConflictonSwitch(VLSR_Route& vlsr)
{
    PortList portList;
    PortList::Iterator itPort;
    vlanPortMapList::Iterator iter = vlanPortMapListAll.begin();
    for (; iter != vlanPortMapListAll.end(); ++iter) {
        if ((*iter).vid == vlsr.vlanTag && !hook_isVLANEmpty(*iter))
            return true;
    }

    if (vlsr.vlanTag == 0 || vlsr.vlanTag > MAX_VLAN)
        return false;

    uint32 vlan;
    if (vlsr.inPort >> 16 != LOCAL_ID_TYPE_TAGGED_GROUP && vlsr.inPort >> 16 != LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) {
        SwitchCtrl_Global::getPortsByLocalId(portList, vlsr.inPort);
        for (itPort = portList.begin(); itPort != portList.end(); ++itPort) {
            vlan = getVLANbyUntaggedPort(*itPort);
            if (vlan > 1 && vlan <= MAX_VLAN && vlan != vlsr.vlanTag)
                return true;
        }
    }
    
    if (vlsr.outPort >> 16 != LOCAL_ID_TYPE_TAGGED_GROUP && vlsr.outPort >> 16 != LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) {
        SwitchCtrl_Global::getPortsByLocalId(portList, vlsr.outPort);
        for (itPort = portList.begin(); itPort != portList.end(); ++itPort) {
            vlan = getVLANbyUntaggedPort(*itPort);
            if (vlan > 1 && vlan <= MAX_VLAN && vlan != vlsr.vlanTag)
                return true;
        }
    }
	
    return false;
}

bool SwitchCtrl_Session::getMonSwitchInfo(MON_Reply_Subobject& monReply)
{
    if (switchInetAddr.rawAddress() == 0)
        goto _error;
    monReply.switch_info.switch_ip[0].s_addr = switchInetAddr.rawAddress();
    if (vendor == Illegal)
    {
        goto _error;
    }
    else {
        monReply.switch_info.switch_type = (uint16)vendor;
        switch (vendor)
        {
           case AutoDetect: //case should not be present
           case IntelES530:
           case RFC2674:
           case LambdaOptical:
           case RaptorER1010:
           case Catalyst3750:
           case Catalyst6500:
           case HP5406:
           case SMC10G8708:
               monReply.switch_info.access_type = SNMP_ONLY;
               monReply.switch_info.switch_port = 161;
               break;
           case Force10E600:
               monReply.switch_info.access_type = CLI_TELNET; //ssh?
               sscanf(TELNET_PORT, "%d", &monReply.switch_info.switch_port);
               break;
           case LinuxSwitch:
               monReply.switch_info.access_type = CLI_SHELL;
               monReply.switch_info.switch_port = 0;
               break;
           default:
                goto _error;
        }
    }
    return true;

_error:
    monReply.switch_options |= MON_SWITCH_OPTION_ERROR;
    return false;
}

/////////////////////////////////////////////////////////////
//////////////////SwitchCtrl_Global Implementation///////////////
/////////////////////////////////////////////////////////////

SwitchCtrl_Global&	SwitchCtrl_Global::instance() {
	static SwitchCtrl_Global controller;
	return controller;
}

SwitchCtrl_Global::SwitchCtrl_Global() 
{
	static bool first = true;
	sessionList.clear(); 
	init_snmp("VLSRCtrl");
	if (first)
	{
		//read preserved local ids in case  dragon CLI update localids while RSVPD restarts 
		readPreservedLocalIds();
		first = false;
	}

	sessionsRefresher = NULL;
}

SwitchCtrl_Global::~SwitchCtrl_Global() {
	if (sessionsRefresher)
		delete sessionsRefresher;
	SwitchCtrlSessionList::Iterator sessionIter = sessionList.begin();
	for ( ; sessionIter != sessionList.end(); ++sessionIter){
		(*sessionIter)->disconnectSwitch();
		sessionList.erase(sessionIter);
	}
}

void SwitchCtrl_Global::startRefreshTimer() {
	sessionsRefresher = new sessionsRefreshTimer(this, TimeValue(300));
}

bool SwitchCtrl_Global::refreshSessions()
{
	bool ret = true;
	SwitchCtrlSessionList::Iterator sessionIter = sessionList.begin();
	for ( ; sessionIter != sessionList.end(); ++sessionIter){
		ret &= (*sessionIter)->refresh();
	}
	return ret;
}
bool SwitchCtrl_Global::static_connectSwitch(struct snmp_session* &sessionHandle, NetAddress& switchAddr)
{
    LOG(2)( Log::MPLS, "VLSR: establishing SNMP session with switch", switchAddr);
    char str[128];
    char* community = SWITCH_SNMP_COMMUNITY;
    snmp_session session;
    // Initialize a "session" that defines who we're going to talk to   
    snmp_sess_init(&session);

    // set up defaults   
    strcpy(str, convertAddressToString(switchAddr).chars());
    //@@@@special patch for non-standard SNMP port (161 by default)
    //strcat(str, ":3161");
    //session.remote_port = 3161;  
    session.peername = str;  
    // set the SNMP version number   
    session.version = SNMP_VERSION_1;  
    // set the SNMPv1 community name used for authentication   
    session.community = (u_char*)community;  
    session.community_len = strlen((const char*)session.community);  

    // Open the session   
    if (!(sessionHandle = snmp_open(&session))){
        snmp_perror("snmp_open");
        LOG(1)( Log::MPLS, "VLSR: snmp_open failed");
        return false; 
    }
    return true;
}

void SwitchCtrl_Global::static_disconnectSwitch(struct snmp_session* &sessionHandle) 
{
    snmp_close(sessionHandle);
}

bool SwitchCtrl_Global::static_getSwitchVendorInfo(struct snmp_session* &sessionHandle, uint32 &vendor, String &venderSystemDescription) 
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    int status;

    // Create the PDU for the data for our request. 
    pdu = snmp_pdu_create(SNMP_MSG_GET);

    // vlan port list 
    char* oid_str = "system.sysDescr.0";
    if (!snmp_parse_oid(oid_str, anOID, &anOID_len)) {
        	snmp_perror(oid_str);
		LOG(1)( Log::MPLS, "VLSR: snmp_get system.sysDescr.0 failed");
        	return false;
    }
    else
      snmp_add_null_var(pdu, anOID, anOID_len);

    // Send the Request out. 
    status = snmp_synch_response(sessionHandle, pdu, &response);

    // Cisco Catalyst switches output very long strings for switchVendorInfo
    // Modified the function to only consider a maximum length of MAX_VENDOR_NAME
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        char vname[MAX_VENDOR_NAME];

	int res_str_len = response->variables->val_len;

	if (res_str_len > MAX_VENDOR_NAME) res_str_len = MAX_VENDOR_NAME-1;

        strncpy(vname, (const char*)response->variables->val.string, res_str_len);
        vname[res_str_len] = 0;

        venderSystemDescription = vname;
        snmp_free_pdu(response);
        if (String("PowerConnect 5224") == venderSystemDescription) {
        	vendor = RFC2674;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Dell PowerConnect 5224");
        } else if (String("Intel(R) Express 530T Switch ") == venderSystemDescription) {
        	vendor = IntelES530;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Intel Express 530T");
        } else if (String("Ethernet Switch") == venderSystemDescription) {  // Dell PowerConnect 5324 running 1.0.0.xx SW
        	vendor = RFC2674;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Dell PowerConnect 5324 (1.0.x.x firmware)");
        } else if (String("Neyland 24T") == venderSystemDescription) {  // Dell PowerConnect 5324 running 2.0.0.xx SW
        	vendor = RFC2674;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Dell PowerConnect 5324 (2.0.x.x firmware)");
        } else if (String("Ethernet Routing Switch") == venderSystemDescription) {  // Dell PowerConnect 6024/6024F
        	vendor = RFC2674;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Dell PowerConnect 6024/6024F");
        } else if (venderSystemDescription.leftequal("Summit1") || venderSystemDescription.leftequal("Summit5")) {
        	vendor = RFC2674;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Extreme Summit");
        } else if (venderSystemDescription.leftequal("Spectra")) {
        	vendor = LambdaOptical;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor is Lambda Optical Systems");
        } else if (venderSystemDescription.leftequal("Force10 Networks Real Time Operating System Software")) {
        	vendor = Force10E600;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor is Force10");
        } else if (venderSystemDescription.leftequal("Ether-Raptor")) {
        	vendor = RaptorER1010;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor is Raptor");
	} else if (venderSystemDescription.leftequal("Cisco IOS Software, C3750 Software")) {
        	vendor = Catalyst3750;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Cisco 3750");
	} else if (venderSystemDescription.leftequal("Cisco Internetwork Operating System Software") || 
		   venderSystemDescription.leftequal("Cisco IOS Software,")) {
	        // Originally, the Catalyst 65xx switches used the same code as Catalyst 3750 
		// Now the 65xx use different module than 3750
        	vendor = Catalyst6500;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is Cisco 65xx");
        } else if (venderSystemDescription.leftequal("ProCurve J8697A Switch 5406zl")) {
        	vendor = HP5406;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is HP ProCurve 5406");
	} else if (String("8*10GE L2 Switch") == venderSystemDescription) {  // SMC 8 ports 10G Ethernet switch
        	vendor = SMC10G8708;
		LOG(1)( Log::MPLS, "VLSR: SNMP: switch vendor/model is SMC 8708");
        } else {
        	vendor = Illegal;
		LOG(2)( Log::MPLS, "VLSR: SNMP: Unrecognized switch vendor/model description: ", venderSystemDescription);
         	return false;
        }
    } else {
        if (status == STAT_SUCCESS){
            LOG(2)( Log::MPLS, "VLSR: SNMP: Reading vendor info failed. Reason : ", snmp_errstring(response->errstat));
        }
        else
            snmp_sess_perror("snmpget", sessionHandle);
        if(response) snmp_free_pdu(response);
	 return false;
    }

    return true;
}

SwitchCtrl_Session* SwitchCtrl_Global::createSession(NetAddress& switchAddr)
{
    return createSession(SWITCH_VENDOR_MODEL, switchAddr);
}

SwitchCtrl_Session* SwitchCtrl_Global::createSession(uint32 vendor_model, NetAddress& switchAddr)
{
    String vendor_desc;
    SwitchCtrl_Session* ssNew = NULL;

    if (vendor_model == AutoDetect) {
        snmp_session *snmp_handle;
        if (!SwitchCtrl_Global::static_connectSwitch(snmp_handle, switchAddr))
            return NULL;
        if (!SwitchCtrl_Global::static_getSwitchVendorInfo(snmp_handle, vendor_model, vendor_desc))
            return NULL;
        SwitchCtrl_Global::static_disconnectSwitch(snmp_handle);
    }

    switch(vendor_model) {
        case Force10E600:
            ssNew = new SwitchCtrl_Session_Force10E600("VLSR-Force10", switchAddr);
            break;                                        
        case RaptorER1010:
            ssNew = new SwitchCtrl_Session_RaptorER1010("VLSR-Raptor", switchAddr);
            break;
       case Catalyst3750:
	    ssNew = new SwitchCtrl_Session_Catalyst3750("VLSR-Catalyst3750", switchAddr);
	    break;
	case Catalyst6500:
	    ssNew = new SwitchCtrl_Session_Catalyst6500("VLSR-Catalyst6500", switchAddr);
	    break;
        case HP5406:
            ssNew = new SwitchCtrl_Session_HP5406("VLSR-HP5406", switchAddr);
            break;
        case SMC10G8708:
            ssNew = new SwitchCtrl_Session_SMC10G8708("VLSR-SMC-10G8708", switchAddr);
            break;
#ifdef Linux
        case LinuxSwitch:
            ssNew = new SwitchCtrl_Session_Linux("VLSR-Linux", switchAddr);
            break;
#endif
        case Illegal:
            return NULL;
        default:
            ssNew = new SNMP_Session("VLSR-SNMP", switchAddr);
            /* SNMP_Session::hook_createVLAN updated for Dell switches to support dynamic VLAN creation */
            if (vendor_desc.leftequal("Intel(R) Express 530T Switch "))
                ssNew->enableVLANCreation(false);
            break;
    }

    return ssNew;
}
void SwitchCtrl_Global::addLocalId(uint16 type, uint16 value, uint16  tag) 
{
	LocalIdList::Iterator it;

	for (it = localIdList.begin(); it != localIdList.end(); ++it) {
        	LocalId& lid = *it;
        	if (lid.type == type && lid.value == value) {
        	    if ((type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP) && tag != ANY_VTAG)  {
        	        SimpleList<uint16>::Iterator it_uint16;
        	        for (it_uint16 = lid.group->begin(); it_uint16 != lid.group->end(); ++it_uint16) {
        	            if (*it_uint16 == tag)
        	                return;
        	            }
        	        lid.group->push_back(tag);
        	        return;
        	        }
        	    else
        	        return;
        	    }
	}

	LocalId newlid;
	newlid.type = type;
	newlid.value = value;
	localIdList.push_back(newlid);
	localIdList.back().group = new SimpleList<uint16>;
	if ((type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP) && tag != ANY_VTAG)
	    localIdList.back().group->push_back(tag);
}

void SwitchCtrl_Global::deleteLocalId(uint16 type, uint16 value, uint16  tag) 
{
	LocalIdList::Iterator it;
	if (type == 0xffff && value == 0xffff) {
	        //for (it = localIdList.begin(); it != localIdList.end(); ++it)
	         //   if (lid.group)
	         //       delete lid.group;
	        localIdList.clear();
	        return;
	    }
	for (it = localIdList.begin(); it != localIdList.end(); ++it) {
	    LocalId &lid = *it;
	    if (lid.type == type && lid.value == value) {
	        if ((type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP)) {
	            if (tag == ANY_VTAG && lid.group) {
	                delete lid.group;
	                it = localIdList.erase(it);
	                }
	            else {
	                SimpleList<uint16>::Iterator it_uint16;
	                for (it_uint16 = lid.group->begin(); it_uint16 != lid.group->end(); ++it_uint16) {
	                    if (*it_uint16 == tag) {
	                        lid.group->erase(it_uint16);
	                        break;
	                        }
	                   }
	                if (lid.group->size() == 0) {
	                    delete lid.group;
	                    it = localIdList.erase(it);
	                    }
	                }
	            return;
	            }
	        else {
	                delete lid.group;
	                it = localIdList.erase(it);
	                return;
	            }
	        }
	    }
}

void SwitchCtrl_Global::refreshLocalId(uint16 type, uint16 value, uint16 tag) 
{
	LocalIdList::Iterator it;
        if ((type != LOCAL_ID_TYPE_GROUP && type != LOCAL_ID_TYPE_TAGGED_GROUP)) {
	        return;
	    }
	for (it = localIdList.begin(); it != localIdList.end(); ++it) {
           LocalId &lid = *it;
	    if (lid.type == type && lid.value == value) {
               SET_LOCALID_REFRESH(lid);
               return;
	        }
	    }
}

void SwitchCtrl_Global::readPreservedLocalIds() 
{
	ifstream inFile;
	char line[100], *str;
	u_int32_t type, value, tag;

	inFile.open ("/var/preserve/dragon.localids", ifstream::in);
       if (!inFile  || inFile.bad()) 
       {
		LOG(1)(Log::Error, "Failed to open the /var/preserve/dragon.localids...");
		return;
       }
	while (inFile >> line)
	{
		str = strtok(line, " ");
		if(!str) break;
		sscanf(str, "%d:%d", &type, &value);
		if (type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP)
		{
			while ((str = strtok(NULL, " ")))
			{
				if (str) sscanf(str, "%d", &tag);
				else break;
				addLocalId(type, value, tag);
			}
		}
		else
		{
			addLocalId(type, value);
		}
			
	}
}
    
//One unique session per switch
bool SwitchCtrl_Global::addSession(SwitchCtrl_Session* addSS)
{
	SwitchCtrlSessionList::Iterator iter = sessionList.begin();
	for (; iter != sessionList.end(); ++iter ) {
		if ((*(*iter))==(*addSS))
			return false;
	}
	//adding new session
	sessionList.push_front(addSS);
	return  true;
}

void SwitchCtrl_Global::removeSession(SwitchCtrl_Session* addSS)
{
	SwitchCtrlSessionList::Iterator iter = sessionList.begin();
	for (; iter != sessionList.end(); ++iter ) {
		if ((*(*iter))==(*addSS)) {
			sessionList.erase(iter);
			return;
		}
	}
}

void SwitchCtrl_Global::removeRsvpSessionReference(Session* session)
{
	SwitchCtrlSessionList::Iterator iter = sessionList.begin();
	for (; iter != sessionList.end(); ++iter ) {
		(*iter)->removeRsvpSessionReference(session);
		if ((*iter)->isRsvpSessionRefListEmpty()) {
			delete (*iter);
			removeSession((*iter));
			return;
		}
	}
}
	
void SwitchCtrl_Global::processLocalIdMessage(uint8 msgType, LocalId& lid)
{
    switch(msgType)
    {
    case Message::AddLocalId:
        if (lid.group->size() > 0)
            while (lid.group->size()) {
                addLocalId(lid.type, lid.value, lid.group->front());
                lid.group->pop_front();
            }
        else
            addLocalId(lid.type, lid.value);
        break;
    case Message::DeleteLocalId:
        if (lid.group->size() > 0)
            while (lid.group->size()) {
                deleteLocalId(lid.type, lid.value, lid.group->front());
                lid.group->pop_front();
            }
        else
            deleteLocalId(lid.type, lid.value);
        break;
    case Message::RefreshLocalId:
        refreshLocalId(lid.type, lid.value);
        break;
    default:
        break;
    }
}

void SwitchCtrl_Global::getPortsByLocalId(SimpleList<uint32>&portList, uint32 port)
{
    portList.clear();
    uint16 type = (uint16)(port >> 16);
    uint16 value =(uint16)(port & 0xffff) ;
    if (!hasLocalId(type, value))
        return;
    if (type == LOCAL_ID_TYPE_PORT || type == LOCAL_ID_TYPE_NONE)
    {
        portList.push_back(value);
        return;
    }
    else if (type != LOCAL_ID_TYPE_GROUP && type != LOCAL_ID_TYPE_TAGGED_GROUP)
        return;
            
    LocalIdList::Iterator it;
    for (it = localIdList.begin(); it != localIdList.end(); ++it) 
    {
        LocalId &lid = *it;
        if (lid.type == type && lid.value == value) 
        {
            if (!lid.group || lid.group->size() == 0)
                return;
            SimpleList<uint16>::Iterator it_uint16;
            for (it_uint16 = lid.group->begin(); it_uint16 != lid.group->end(); ++it_uint16) 
                   portList.push_back(*it_uint16); //add ports
             return;
        }
    }
}

bool SwitchCtrl_Global::hasLocalId(uint16 type, uint16 value, uint16  tag)
{
    LocalIdList::Iterator it;
    for (it = localIdList.begin(); it != localIdList.end(); ++it) {
        LocalId& lid = *it;
        if (lid.type == type && lid.value == value) {
            if ((type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP) && tag != ANY_VTAG) {
                SimpleList<uint16>::Iterator it_uint16;
                for (it_uint16 = lid.group->begin(); it_uint16 != lid.group->end(); ++it_uint16) {
                    if (*it_uint16 == tag)
                        return true;;
                    }
                return false;
                }
            else
                return true;
            }
        }
        return false;
}

uint16 SwitchCtrl_Global::getSlotType(uint16 slot_num)
{
    SimpleList<slot_entry>::Iterator it = slotList.begin();
    for (; it != slotList.end(); ++it) {
        if ((*it).slot_num == slot_num)
            return (*it).slot_type;
    }

    //returning default types
    if (slot_num < 2)
        return SLOT_TYPE_TENGIGE;
    else
        return SLOT_TYPE_GIGE;

    //return SLOT_TYPE_ILLEGAL;
}

uint32 SwitchCtrl_Global::getExclEntry(String session_name)
{
    uint32 excl_options = 0;	
    SimpleList<sw_layer_excl_name_entry>::Iterator it = exclList.begin();
    for (; it != exclList.end(); ++it) {
        if (strstr(session_name.chars(), (*it).excl_name) != NULL) {
            excl_options |= (*it).sw_layer;
        }
    }
    return excl_options;
}

static uint8 spe_string2int(String& spe)
{
    if (spe == "sts-1")
        return 5;
    else if (spe == "sts-3c")
        return 6;

    // No other signals are supported at this monment!
    return 0;
}


SONET_TSpec* SwitchCtrl_Global::addEosMapEntry(float bandwidth, String& spe, int ncc)
{
    uint8 spe_int = spe_string2int(spe);
    if (spe_int == 0)
        return NULL;

    SimpleList<eos_map_entry>::Iterator it = eosMapList.begin();
    for (; it != eosMapList.end(); ++it) {
        if ((*it).bandwidth == bandwidth) {
            return (*it).sonet_tspec;
        }
        if ((*it).bandwidth > bandwidth)
            break;
    }

    SONET_TSpec* stp = new SONET_TSpec(spe_int, 1, (uint8)ncc, 0, 1, 0, 0);
    eos_map_entry eos_map;
    eos_map.bandwidth = bandwidth;
    eos_map.sonet_tspec = stp;
    if (it == eosMapList.end()) {
        eosMapList.push_back(eos_map);
    }
    else if (it == eosMapList.begin()) {
        eosMapList.push_front(eos_map);
    }
    else {
        eosMapList.insert(it, eos_map);
    }
    return stp;
}

SONET_TSpec* SwitchCtrl_Global::getEosMapEntry(float bandwidth)
{
    SimpleList<eos_map_entry>::Iterator it = eosMapList.begin();
    for (; it != eosMapList.end(); ++it) {
        if ((*it).bandwidth == bandwidth) {
            return (*it).sonet_tspec;
        }
    }

    //add additional mapping entries.
    String sts1("sts-1");
    return addEosMapEntry(bandwidth, sts1, (int)ceilf(bandwidth/49.536));
}

void SwitchCtrl_Global::getMonitoringInfo(MON_Query_Subobject& monQuery, MON_Reply_Subobject& monReply, uint32 destIp)
{
    uint16 errCode = 0;
    SwitchCtrlSessionList::Iterator it;
    bool foundSession = false;
    PSB* psb = NULL;

    monReply.switch_options = 0;
    monReply.length = MON_REPLY_BASE_SIZE;
    if (strcmp(monQuery.gri, "none") == 0) {
        if (this->sessionList.size() == 0) {
             errCode = 1; //no switch control session
             goto _error;
        }
        if (!sessionList.front()->getMonSwitchInfo(monReply)) {
                  errCode = 3; //failed to retrieve switch info
                  goto _error;
        }				
        return;
    }

    psb = RSVP_Global::rsvp->getPSBbyLSPName((const char*)monQuery.gri, destIp);
    if (psb == NULL) {
        errCode = 2; //no rsvp session
        goto _error;
    }
	
    for (it = sessionList.begin(); it != sessionList.end(); ++it) {
        if ((*it)->isMonSession(monQuery.gri)) {
            if( (*it)->getMonSwitchInfo(monReply)) {
                if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET) == 0) {
                    //ethernet switch vlsr --> get vlsr route PSB
                    if (psb != NULL) {
                        VLSRRoute& vlsrtList = psb->getVLSR_Route();
                        if (vlsrtList.size() != 1) {
                            errCode = 5; // incorrect vlsr route information
                            goto _error;
                        }
                        monReply.sub_type = MON_REPLY_SUBTYPE_ETHERNET; //Ethernet
                        monReply.length = MON_REPLY_BASE_SIZE + sizeof(struct _Ethernet_Circuit_Info);
                        //$$$ get VLAN and ports from
                        VLSR_Route& vlsrt = vlsrtList.front();
                        SimpleList<uint32> portList;
                        SimpleList<uint32>::Iterator portIter;
                        int i;
                        switch (vlsrt.inPort >> 16) {
                            case LOCAL_ID_TYPE_PORT:
                                monReply.circuit_info.vlan_info.vlan_ingress = 0;
                                monReply.circuit_info.vlan_info.num_ports_ingress = 1;
                                monReply.circuit_info.vlan_info.ports_ingress[0] = (vlsrt.inPort & 0xffff);
                                break;
                            case LOCAL_ID_TYPE_GROUP:
                                monReply.circuit_info.vlan_info.vlan_ingress = 0;
                                getPortsByLocalId(portList, vlsrt.inPort);
                                monReply.circuit_info.vlan_info.num_ports_ingress = portList.size();
                                for (portIter = portList.begin(), i = 0; portIter != portList.end(); ++portIter, ++i)
                                    monReply.circuit_info.vlan_info.ports_ingress[i] = ((*portIter) & 0xffff);
                                break;
                            case LOCAL_ID_TYPE_TAGGED_GROUP:
                                monReply.circuit_info.vlan_info.vlan_ingress = (vlsrt.inPort & 0xffff);
                                getPortsByLocalId(portList, vlsrt.inPort);
                                monReply.circuit_info.vlan_info.num_ports_ingress = portList.size();
                                for (portIter = portList.begin(), i = 0; portIter != portList.end(); ++portIter, ++i)
                                    monReply.circuit_info.vlan_info.ports_ingress[i] = ((*portIter) & 0xffff);
                                break;
                            case LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL:
                                monReply.circuit_info.vlan_info.vlan_ingress = vlsrt.vlanTag;
                                monReply.circuit_info.vlan_info.num_ports_ingress = 1;
                                monReply.circuit_info.vlan_info.ports_ingress[0] = (vlsrt.inPort & 0xffff);
                                break;
                        }
                        switch (vlsrt.outPort >> 16) {
                            case LOCAL_ID_TYPE_PORT:
                                monReply.circuit_info.vlan_info.vlan_egress = 0;
                                monReply.circuit_info.vlan_info.num_ports_egress = 1;
                                monReply.circuit_info.vlan_info.ports_egress[0] = (vlsrt.outPort & 0xffff);
                                break;
                            case LOCAL_ID_TYPE_GROUP:
                                monReply.circuit_info.vlan_info.vlan_egress = 0;
                                getPortsByLocalId(portList, vlsrt.outPort);
                                monReply.circuit_info.vlan_info.num_ports_egress = portList.size();
                                for (portIter = portList.begin(), i = 0; portIter != portList.end(); ++portIter, ++i)
                                    monReply.circuit_info.vlan_info.ports_egress[i] = ((*portIter) & 0xffff);
                                break;
                            case LOCAL_ID_TYPE_TAGGED_GROUP:
                                monReply.circuit_info.vlan_info.vlan_egress = (vlsrt.outPort & 0xffff);
                                getPortsByLocalId(portList, vlsrt.outPort);
                                monReply.circuit_info.vlan_info.num_ports_egress = portList.size();
                                for (portIter = portList.begin(), i = 0; portIter != portList.end(); ++portIter, ++i)
                                    monReply.circuit_info.vlan_info.ports_egress[i] = ((*portIter) & 0xffff);
                                break;
                            case LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL:
                                monReply.circuit_info.vlan_info.vlan_egress = vlsrt.vlanTag;
                                monReply.circuit_info.vlan_info.num_ports_egress = 1;
                                monReply.circuit_info.vlan_info.ports_egress[0] = (vlsrt.outPort & 0xffff);
                                break;
                        }
                    }
                } else if (!(*it)->getMonCircuitInfo(monReply)) { //edge-control subnet vlsr
                    errCode = 4; //failed to retrieve circuit info (subnetSwitchCtrlSession)
                    goto _error;
                 }
            } else {
                errCode = 3; //failed to retrieve switch info
                goto _error;
            }
            foundSession = true;
        }        
    }
    if (!foundSession) {
        //first judging whether this is a Subnet transit vlsr
        VLSRRoute& vlsrtList = psb->getVLSR_Route();
        if (vlsrtList.size() == 0 && SwitchCtrl_Session_SubnetUNI::IsSubnetTransitERO(psb->getEXPLICIT_ROUTE_Object())) {
             monReply.switch_options |= (MON_SWITCH_OPTION_SUBNET|MON_SWITCH_OPTION_SUBNET_TRANSIT);
             monReply.sub_type = MON_REPLY_SUBTYPE_SUBNET_TRANSIT;
             monReply.length = MON_REPLY_BASE_SIZE;
        }
        else {// otherwise error!
            errCode = 1; //no switch control session matching the GRI
            goto _error;
        }
    }

    //flags whether the VLSR is source or/and destination node.
    if (psb->getSession().getDestAddress() == Session::ospfRouterID)
         monReply.switch_options |= MON_SWITCH_OPTION_CIRCUIT_SRC;
    if (psb->getSrcAddress() == Session::ospfRouterID)
         monReply.switch_options |= MON_SWITCH_OPTION_CIRCUIT_DEST;
	
    if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET)) {
        if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET_SRC) && (monReply.switch_options & MON_SWITCH_OPTION_SUBNET_DEST))
            monReply.sub_type = MON_REPLY_SUBTYPE_SUBNET_SRCDEST;
        else if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET_SRC))
            monReply.sub_type = MON_REPLY_SUBTYPE_SUBNET_SRC;
        else if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET_DEST))
            monReply.sub_type = MON_REPLY_SUBTYPE_SUBNET_DEST;
    }

    return; // normal return

  _error:
    monReply.sub_type = MON_REPLY_SUBTYPE_ERROR;
    monReply.length = MON_REPLY_BASE_SIZE;
    monReply.switch_options = (MON_SWITCH_OPTION_ERROR|errCode);
    return;
}

//End of file : SwitchCtrl_Global.cc


