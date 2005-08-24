/****************************************************************************

SNMP Management source file SNMP_Global.cc 
Created by Aihua Guo @ 04/10/2004 
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#include "SNMP_Global.h"
#include "RSVP_Log.h"

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

bool SNMP_Session::movePortToVLAN(uint32 port, uint32 vlanID)
{
	uint32 mask;
	bool ret = true;
	
	if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!
	int old_vlan = getVLANbyPort(port);
	if (old_vlan) //not in default control VLAN
	{
		mask=(~(1<<(32-port))) & 0xFFFFFFFF;	
		portList[old_vlan]&=mask;
		mask = 1<<(32-port);
		portList[vlanID]|=mask;
		if (vendor == RFC2674) 
			ret&=setVLANPortTag(portList[old_vlan], old_vlan); //Set back to "tagged"
		ret&=setVLANPort(portList[old_vlan], old_vlan); //remove untagged port out of the old VLAN
		ret = setVLANPort(portList[vlanID], vlanID); // move port to the new VLAN
		if (vendor == RFC2674){
			ret&=setVLANPortTag(portList[vlanID], vlanID); //Set to "untagged"
			ret&=setVLANPVID(port, vlanID); //Set pvid
		}
	}
	else{ //in default VLAN, so just need to set ports for vlanID
		mask = 1<<(32-port);
		portList[vlanID]|=mask;
		ret&=setVLANPort(portList[vlanID], vlanID) ;
		if (vendor == RFC2674){
			ret&=setVLANPortTag(portList[vlanID], vlanID); //Set to "untagged"
			ret&=setVLANPVID(port, vlanID); //Set pvid
		}
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
		if (vendor == RFC2674) {
			//ret&=setVLANPVID(port, 1); //Set pvid to default vlan ID
			ret&=setVLANPortTag(portList[vlanID], vlanID); //Set back to "tagged"
		}
		ret &= setVLANPort(portList[vlanID], vlanID) ;
	}
	return ret;
}

bool SNMP_Session::readVLANFromSwitch()
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	int status;
	long int ports;
	
	// vlan port list 
	const char* read_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
	char oid_str[128];

	if (vendor != RFC2674)
		return true;
	
	// Create the PDU for the data for our request. 

	uint32 vlan = MIN_VLAN;

	while (vlan <= MAX_VLAN){
		pdu = snmp_pdu_create(SNMP_MSG_GET);
		sprintf(oid_str, "%s.%d", read_oid_str, vlan);
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
			else
				ports = 0;
			portList[vlan] = ports;
		}
		if(response) snmp_free_pdu(response);
		vlan++;
	}
       return true;
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
			(*snmpIter).disconnectSwitch();
			snmpSessionList.erase(snmpIter);
		}
	}


//One unique SNMP session per switch
bool SNMP_Global::addSNMPSession(SNMP_Session& addSS)
{
	SNMPSessionList::Iterator iter = snmpSessionList.begin();
	for (; iter != snmpSessionList.end(); ++iter ) {
		if ((*iter)==addSS)
			return false;
	}

	//adding new SNMP session
	snmpSessionList.push_back(addSS);
	return  true;
}

//End of file : SNMP_Global.cc
