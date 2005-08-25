/****************************************************************************

SNMP Management source file SNMP_Global.cc 
Created by Aihua Guo @ 04/10/2004 
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#include "SNMP_Global.h"
#include "RSVP_Log.h"
#include "RSVP_Message.h"

LocalIdList SNMP_Global::localIdList;

bool SNMP_Session::connectSwitch()
{
	struct snmp_session session;	
	char str[128];
	char* community = "dragon";
	 // Initialize a "session" that defines who we're going to talk to   
	 snmp_sess_init( &session);			
	 // set up defaults   
	 strcpy(str, convertAddressToString(switchInetAddr).chars());
	 session.peername = str;  
	 // set the SNMP version number   
	 session.version = SNMP_VERSION_1;  
	 // set the SNMPv1 community name used for authentication   
	 session.community = (u_char*)community;  
	 session.community_len = strlen((const char*)session.community);  
	 // Open the session   
	 if (!(sessionHandle = snmp_open(&session))){
		snmp_perror("snmp_open");
		return false;  
	 }

	//Read Ethernet switch vendor info
	if (!(setSwitchVendorInfo() && readVLANFromSwitch())){
		disconnectSwitch();
		return false;
	}
	active = true;
	return true;
}

bool SNMP_Session::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
	uint32 mask;
	bool ret = true;
	
	if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!
	int old_vlan = getVLANbyUntaggedPort(port);
	if (old_vlan) //Remove untagged port from old VLAN
	{
		mask=(~(1<<(32-port))) & 0xFFFFFFFF;	
		portListUntagged[old_vlan]&=mask;
		portList[old_vlan]&=mask;
		if (vendor == RFC2674) 
			ret&=setVLANPortTag(portListUntagged[old_vlan], old_vlan); //Set back to "tagged"
		ret&=setVLANPort(portListUntagged[old_vlan], old_vlan); //remove untagged port out of the old VLAN
       }
	mask = 1<<(32-port);
	portListUntagged[vlanID]|=mask;
	portList[vlanID]|=mask;
	ret&=setVLANPort(portList[vlanID], vlanID) ;
	if (vendor == RFC2674){
		ret&=setVLANPortTag(portListUntagged[vlanID], vlanID); //Set to "untagged"
		ret&=setVLANPVID(port, vlanID); //Set pvid
	}
	return ret;
}

bool SNMP_Session::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
	uint32 mask;
	bool ret = true;
	
	if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!

       //there is no need to remove a to-be-tagged-in-new-VLAN port from old VLAN
	mask = 1<<(32-port);
	portList[vlanID]|=mask;
	ret&=setVLANPort(portList[vlanID], vlanID) ;
	if (vendor == RFC2674){
		ret&=setVLANPVID(port, vlanID); //Set pvid
	}
	return ret;
}



bool SNMP_Session::removePortFromVLAN(uint32 port, uint32 vlanID)
{
	bool ret = true;
	
	if ((!active) || port==SWITCH_CTRL_PORT)
		return false; //don't touch the control port!

	if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
		uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;	
		portList[vlanID]&=mask;
		portListUntagged[vlanID]&=mask;
		if (vendor == RFC2674) {
			ret&=setVLANPortTag(portList[vlanID], vlanID); //Set back to "tagged"
		}
		ret &= setVLANPort(portList[vlanID], vlanID) ;
	}
	return ret;
}

bool SNMP_Session::movePortToDefaultVLAN(uint32 port)
{
	bool ret = true;
	
	if ((!active) || port==SWITCH_CTRL_PORT)
		return false; //don't touch the control port!
	uint32 vlanID = getVLANbyPort(port);
	if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
		uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;	
		portList[vlanID]&=mask;
		portListUntagged[vlanID]&=mask;
		if (vendor == RFC2674) {
			//ret&=setVLANPVID(port, 1); //Set pvid to default vlan ID
			ret&=setVLANPortTag(portList[vlanID], vlanID); //Set back to "tagged"
		}
		ret &= setVLANPort(portList[vlanID], vlanID) ;
	}
	return ret;
}

void SNMP_Session::readVlanPortMapBranch(const char* oid_str, vlanPortMapList &vpmList)
{
        struct snmp_pdu *pdu;
        struct snmp_pdu *response;
        netsnmp_variable_list *vars;
        oid anOID[MAX_OID_LEN];
        oid root[MAX_OID_LEN];
        size_t anOID_len = MAX_OID_LEN;
        int status;
        long int ports;
        bool running = true;
        size_t rootlen;

        status = read_objid(oid_str, anOID, &anOID_len);
        rootlen = anOID_len;
        memcpy(root, anOID, rootlen*sizeof(oid));
        vpmList.clear();
        while (running) {
                // Create the PDU for the data for our request.
                pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
                snmp_add_null_var(pdu, anOID, anOID_len);
                // Send the Request out.
                status = snmp_synch_response(sessionHandle, pdu, &response);
                if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
                        for (vars = response->variables; vars; vars = vars->next_variable) {
                                if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                                        running = false;
                                        continue;
                                }

                                if (response->variables->val.integer){
                                        ports = ntohl(*(response->variables->val.integer));
                                        if (response->variables->val_len < 4) {
                                                uint32 mask = (uint32)0xFFFFFFFF << ((4-response->variables->val_len)*8);
                                                ports &= mask;
                                                //portList[(int)vars->name[vars->name_length - 1]] = ports;
                                                vlanPortMap portmap;
                                                portmap.pvid = (int)vars->name[vars->name_length - 1];
                                                portmap.portMask = ports;
                                                vpmList.push_back(portmap);
                                        }
                                }
                                else
                                        ports = 0;
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
                }
                if(response) snmp_free_pdu(response);
        }
}

bool SNMP_Session::readVLANFromSwitch()
{
    bool ret = true;;
    readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.2", vlanPortMapListAll);
    if (vlanPortMapListAll.size() == 0)
        ret = false;

    readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.4", vlanPortMapListUntagged);
    if (vlanPortMapListUntagged.size() == 0)
        ret = false;

    return ret;
}

bool SNMP_Session::setSwitchVendorInfo()
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
            	return false;
	  }
	  else
		  snmp_add_null_var(pdu, anOID, anOID_len);

	  // Send the Request out. 
	  status = snmp_synch_response(sessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
		char vname[128];
		strncpy(vname, (const char*)response->variables->val.string, response->variables->val_len);
		vname[response->variables->val_len] = 0;

		String buf = vname;
		snmp_free_pdu(response);
		if (String("PowerConnect 5224") == buf)
			vendor = RFC2674;
		else if (String("Intel(R) Express 530T Switch ") == buf)
			vendor = IntelES530;
		else if (String("Ethernet Switch") == buf)
			vendor = RFC2674;
		else if (buf.leftequal("Summit1i") || buf.leftequal("Summit5i")) 
			vendor = RFC2674;
		else{
			vendor = Illegal;
		 	return false;
		}
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: Reading ", switchInetAddr, " vendor info failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpget", sessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }

	  return true;
}

bool SNMP_Session::setVLANPVID(uint32 port, uint32 vlanID)
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.5.1.1";

	if (!active || vendor!=RFC2674) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='u'; 
  	  sprintf(value, "%d", vlanID);
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);

	  // Send the Request out. 
	  status = snmp_synch_response(sessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: Setting VLAN PVID", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", sessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

bool SNMP_Session::setVLANPortTag(uint32 portListNew, uint32 vlanID)
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.4";

	if (!active || vendor!=RFC2674) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='x'; 
  	  sprintf(value, "%.8lx", portListNew);
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);

	  // Send the Request out. 
	  status = snmp_synch_response(sessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: Setting VLAN Tag of Ethernet switch", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", sessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

bool SNMP_Session::verifyVLAN(uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[128], oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
    uint32 ports = 0;

    if (!active || vendor!=RFC2674) //not initialized or session has been disconnected
    	return false;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // vlan port list 
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(sessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        	snmp_free_pdu(response);
                return true;
    	}
    if(response) 
        snmp_free_pdu(response);
    return false;
}

bool SNMP_Session::setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[128], oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.4";
    uint32 ports = 0;

    if (!active || vendor!=RFC2674) //not initialized or session has been disconnected
    	return false;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // vlan port list 
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(sessionHandle, pdu, &response);
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
    sprintf(value, "%.8lx", 0); // set all ports tagged
    status = snmp_add_var(pdu, anOID, anOID_len, type, value);
    sprintf(value, "%.8lx", ports); //restore those originally untagged ports execept the 'taggedPorts'
    status = snmp_add_var(pdu, anOID, anOID_len, type, value);
    // Send the Request out. 
    status = snmp_synch_response(sessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	snmp_free_pdu(response);
    }
    else {
    	if (status == STAT_SUCCESS){
    	LOG(2)( Log::MPLS, "VLSR: SNMP: Setting VLAN Tag failed. Reason : ", snmp_errstring(response->errstat));
    	}
    	else
      		snmp_sess_perror("snmpset", sessionHandle);
    	if(response)
        	snmp_free_pdu(response);
        return false;
    }
    return true;
}


bool SNMP_Session::VLANHasTaggedPort(uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[128], oid_str[128];
    int status;
    long int ports_all = 0, ports_untagged = 0;
    String oid_str_all = ".1.3.6.1.2.1.17.7.1.4.3.1.2";    
    String oid_str_untagged = ".1.3.6.1.2.1.17.7.1.4.3.1.4";

    if (!active || vendor!=RFC2674) //not initialized or session has been disconnected
      return false;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // all vlan ports list 
    sprintf(oid_str, "%s.%d", oid_str_all.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(sessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        if (response && response->variables->val.integer)
            ports_all = ntohl(*(response->variables->val.integer));
        else
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
    status = snmp_synch_response(sessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        if (response && response->variables->val.integer)
            ports_untagged = ntohl(*(response->variables->val.integer));
        else
            return false;
    }
    else 
        return false;
    if(response) 
      snmp_free_pdu(response);

    if (ports_all == ports_untagged)
        return false;

    return true;
}

bool SNMP_Session::setVLANPort(uint32 portListNew, uint32 vlanID)
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;

	if (!active || vendor==Illegal) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", supportedVendorOidString[vendor].chars(), vlanID);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='x'; 

	  if (vendor==IntelES530)
		sprintf(value, "%.8lx0000000000000000", portListNew);
	  else if (vendor==RFC2674)
	  	sprintf(value, "%.8lx", portListNew);
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);

	  // Send the Request out. 
	  status = snmp_synch_response(sessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: Setting VLAN of Ethernet switch", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", sessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

SNMP_Global::~SNMP_Global() {
		SNMPSessionList::Iterator snmpIter = snmpSessionList.begin();
		for ( ; snmpIter != snmpSessionList.end(); ++snmpIter){
			(*snmpIter)->disconnectSwitch();
			snmpSessionList.erase(snmpIter);
		}
	}


//One unique SNMP session per switch
bool SNMP_Global::addSNMPSession(SNMP_Session* addSS)
{
	SNMPSessionList::Iterator iter = snmpSessionList.begin();
	for (; iter != snmpSessionList.end(); ++iter ) {
		if ((*(*iter))==(*addSS))
			return false;
	}

	//adding new SNMP session
	snmpSessionList.push_back(addSS);
	return  true;
}

void SNMP_Global::processLocalIdMessage(uint8 msgType, LocalId& lid)
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
    default:
        break;
    }
}
void SNMP_Global::getPortsByLocalId(SimpleList<uint32>&portList, uint32 port)
{
    portList.clear();
    uint16 type = (uint16)(port >> 16);
    uint16 value =(uint16)(port & 0xffff) ;
    if (!hasLocalId(type, value))
        return;
    if (type == LOCAL_ID_TYPE_PORT)
    {
        portList.push_back(value);
        return;
    }
    else if (type != LOCAL_ID_TYPE_GROUP && type != LOCAL_ID_TYPE_TAGGED_GROUP)
        return;
            
    LocalIdList::Iterator it;
    LocalId lid;
    for (it = localIdList.begin(); it != localIdList.end(); ++it) 
    {
        lid = *it;
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

//End of file : SNMP_Global.cc
