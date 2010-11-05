/****************************************************************************

SNMP Based Switch Control Module source file SNMP_Session.cc
Created by Xi Yang @ 01/17/2006
Extended from SNMP_Global.cc by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SNMP_Session.h"
#include "RSVP_Log.h"

bool SNMP_Session::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    //uint32 mask;
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!
    int old_vlan = getVLANbyUntaggedPort(port);
    if (old_vlan) { //Remove untagged port from old VLAN
        //mask=(~(1<<(32-port))) & 0xFFFFFFFF;
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
        if (vpmUntagged)
            //vpmUntagged->ports&=mask;
            ResetBit(vpmUntagged->portbits, port-1);
        vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
        if (vpmAll)
    	    //vpmAll->ports&=mask;
    	    ResetBit(vpmAll->portbits, port-1);
        if (vendor == RFC2674) {
            //Set this port to untagged
            if (vpmUntagged) ret&=setVLANPortTag(vpmUntagged->portbits , old_vlan); 
        }
        //remove THIS untagged port out of the old VLAN
        if (vpmUntagged) ret&=setVLANPort(vpmAll->portbits, old_vlan); 
   }

    //mask = 1<<(32-port);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged)
        //vpmUntagged->ports |=mask;
        SetBit(vpmUntagged->portbits,port-1);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        //vpmAll->ports |=mask;
        SetBit(vpmAll->portbits,port-1);
        ret&=setVLANPort(vpmAll->portbits, vlanID);
    }

    if (vendor == RFC2674) {
        if (vpmUntagged)
    	    ret&=setVLANPortTag(vpmUntagged->portbits, vlanID); //Set to "untagged"
        ret&=setVLANPVID(port, vlanID); //Set pvid
    }

    return ret;
}

bool SNMP_Session::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
	//uint32 mask;
	bool ret = true;
	vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

	if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!

    //there is no need to remove a to-be-tagged-in-new-VLAN port from old VLAN
	//mask = 1<<(32-port);
	vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
	if (vpmAll) {
	    //vpmAll->ports |=mask;
	    SetBit(vpmAll->portbits,port-1);
	    ret&=setVLANPort(vpmAll->portbits, vlanID) ;
    }
    else
        return false;
    //no need of setVLANPVID for PowerConnect 5224
	if (vendor == RFC2674 && vendorSystemDescription !="PowerConnect 5224"){
		ret&=setVLANPVID(port, vlanID); //Set pvid
	}

    if(vendorSystemDescription.leftequal("PowerConnect 6248")) {
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged)
             //bit==0 means port is tagged; also true for other switch models but may not be necessary
            ResetBit(vpmUntagged->portbits, port-1);
	    ret&=setVLANPortTag(vpmUntagged->portbits, vlanID);
    }

	return ret;
}

bool SNMP_Session::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT)
    	return false; //don't touch the control port!

    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN) {
      	 //uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;
    	 vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
        if (vpmAll)
            //vpmAll->ports &= mask;
            ResetBit(vpmAll->portbits,port-1);
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged)
            //vpmUntagged->ports &= mask;
            ResetBit(vpmUntagged->portbits,port-1);
        if (vendor == RFC2674) {
            if (vpmUntagged)
                ret&=setVLANPortTag(vpmUntagged->portbits, vlanID); //make THIS port tagged
            if (vendorSystemDescription =="PowerConnect 5224") //@@@@
                ret&=setVLANPVID(port, 1); //Set pvid to default vlan ID;
    	}
    	if (vpmAll)
    	    ret &= setVLANPort(vpmAll->portbits, vlanID); //remove
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}

/////////--------Vendor specific functions------///////////

bool SNMP_Session::setVLANPVID(uint32 port, uint32 vlanID)
{
        LOG(4)( Log::MPLS, "VLSR: SNMP: setting PVID", vlanID, "on port", port);

	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.5.1.1";

	if (!active || !rfc2674_compatible) //not initialized or session has been disconnected
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
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: Setting VLAN PVID", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

bool SNMP_Session::setVLANPortTag(uint8* portbits, uint32 vlanID)
{
    //RevertWordBytes(portListNew);
    if(vendorSystemDescription.leftequal("PowerConnect 6248"))
        return setVLANPortTag(portbits, 800, vlanID);
    return setVLANPortTag(portbits, 32, vlanID);
}

//reset port bit  for untagged-only OID
bool SNMP_Session::setVLANPortTag(uint8* portbits, int bitlen, uint32 vlanID)
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[512], oid_str[128], oct[3];
	int status, i;
	String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.4";

	if (!active || !rfc2674_compatible) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='x';

         for (i = 0, value[0] = 0; i < (bitlen+7)/8; i++) { 
            snprintf(oct, 3, "%.2x", portbits[i]);
            strcat(value,oct);
         }
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);

	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: Setting VLAN Tag of Ethernet switch", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

bool SNMP_Session::setVLANPort(uint8* portbits, uint32 vlanID)
{
    //because the portListNew was read in as a 32-bit long integer, port 1-8 goes the highest byte.
    //In other port-bitmask reading, we put port 1:8 in the lowest byte (1st byte of a uint8 string)
    //RevertWordBytes(portListNew); 

    if (vendor==IntelES530) {
        uint8 value[12];
        memset(value, 0, 12);
        memcpy(value, portbits, sizeof(uint32));
        return setVLANPort(value, 96, vlanID);
    }
    else if(vendorSystemDescription.leftequal("PowerConnect 6248"))
    	return setVLANPort(portbits, 800, vlanID);
    else 
    	return setVLANPort(portbits, 32, vlanID);
}

//set port bitmask for tagged/untagged (egress) OID
bool SNMP_Session::setVLANPort(uint8* portbits, int bitlen, uint32 vlanID)
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[512], oid_str[128], oct[3];
	int status, i;

	if (!active || !rfc2674_compatible) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", supportedVendorOidString[vendor].chars(), vlanID);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='x'; 

         for (i = 0, value[0] = 0; i < (bitlen+7)/8; i++) { 
            snprintf(oct, 3, "%.2x", portbits[i]);
            strcat(value,oct);
         }
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);

	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: Setting VLAN of Ethernet switch", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

/////////////----------///////////

bool SNMP_Session::hook_createVLAN(const uint32 vlanID)
{
    LOG(4)( Log::MPLS, "VLSR: SNMP: creating VLAN ID", vlanID, ", VLAN creation enabled=", vlanCreation_enabled);

    if (!vlanCreation_enabled)
        return false;

    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[128], oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.5";

    if (!active) //not initialized or session has been disconnected
        return false;
    // Create the PDU for the data for our request. 
    pdu = snmp_pdu_create(SNMP_MSG_SET);

    // vlan port list 
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    type='i'; 
    strcpy(value, "4");

    status = snmp_add_var(pdu, anOID, anOID_len, type, value);

    /*
     *  Dell PowerConnect 5224/5324/6024 switches require a bit more coaxing to create a new VLAN
     *  NOTE: The Dell PowerConnect 6224 does not require these extra SNMP SETs..
     */ 
    if (String("PowerConnect 5224") == vendorSystemDescription ||
	String("Ethernet Switch") == vendorSystemDescription ||
	String("Neyland 24T") == vendorSystemDescription ||
	String("Ethernet Routing Switch") == vendorSystemDescription) {
      tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.1";
      sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
      status = read_objid(oid_str, anOID, &anOID_len);
      type='s'; 
      strcpy(value, "");
      status = snmp_add_var(pdu, anOID, anOID_len, type, value);
      
      tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
      sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
      status = read_objid(oid_str, anOID, &anOID_len);
      type='x'; 
      strcpy(value, "00000000");
      status = snmp_add_var(pdu, anOID, anOID_len, type, value);
      
      tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.3";
      sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
      status = read_objid(oid_str, anOID, &anOID_len);
      type='x'; 
      strcpy(value, "00000000");
      status = snmp_add_var(pdu, anOID, anOID_len, type, value);
      
      tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.4";
      sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
      status = read_objid(oid_str, anOID, &anOID_len);
      type='x'; 
      strcpy(value, "00000000");
      status = snmp_add_var(pdu, anOID, anOID_len, type, value);
    }

    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);

    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	snmp_free_pdu(response);
    }
    else {
        if (status == STAT_SUCCESS){
	  LOG(4)( Log::MPLS, "VLSR: SNMP: Create VLAN on", switchInetAddr, "failed. Reason: ", snmp_errstring(response->errstat));
        }
        else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
        if(response) snmp_free_pdu(response);
        return false;
    }

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    memset(vpm.portbits, untaggedPortBit_reverse? 0xff : 0, MAX_VLAN_PORT_BYTES);
    vlanPortMapListUntagged.push_back(vpm);

    return true;
}

bool SNMP_Session::hook_removeVLAN(const uint32 vlanID)
{
    LOG(4)( Log::MPLS, "VLSR: SNMP: removing VLAN ID", vlanID, ", VLAN deletion enabled=", vlanCreation_enabled);

    if (!vlanCreation_enabled)
        return false;

    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[128], oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.2.1.17.7.1.4.3.1.5";

    if (!active) //not initialized or session has been disconnected
        return false;
    // Create the PDU for the data for our request. 
    pdu = snmp_pdu_create(SNMP_MSG_SET);

    // vlan port list 
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    type='i'; 
    strcpy(value, "6");

    status = snmp_add_var(pdu, anOID, anOID_len, type, value);

    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);

    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	snmp_free_pdu(response);
    }
    else {
        if (status == STAT_SUCCESS){
	  LOG(4)( Log::MPLS, "VLSR: SNMP: Remove VLAN on", switchInetAddr, "failed. Reason: ", snmp_errstring(response->errstat));
        }
        else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
        if(response) snmp_free_pdu(response);
        return false;
    }
    return true;
}

bool SNMP_Session::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    return (vpm.ports == 0);
}

void SNMP_Session::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if(vendorSystemDescription.leftequal("PowerConnect 6248")) {
        if (vars->val.bitstring ){
            for (unsigned int i = 0; i < vars->val_len && i < 100; i++) { //800 bits
                vpm.portbits[i] = vars->val.bitstring[i];
           }
        }
    }
    else if (vars->val.integer){ //other cases
        vpm.ports = ntohl(*(vars->val.integer));
        if (vars->val_len < 4) {
             uint32 mask = (uint32)0xFFFFFFFF << ((4-vars->val_len)*8);
             vpm.ports &= mask;
        }
    }
    else
        vpm.ports = 0;

    vpm.vid = (uint32)vars->name[vars->name_length - 1];
}

bool SNMP_Session::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    if(vendorSystemDescription.leftequal("PowerConnect 6248")) {
        if (HasPortBit(vpm.portbits, port-1))
            return true;
    }
    else if ((vpm.ports)&(1<<(32-port)))
        return true;;

    return false;
}

bool SNMP_Session::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
    uint32 port, portNum;
    vlanPortMap* vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(!vpmAll)
        return false;

    portList.clear();
    if(vendorSystemDescription.leftequal("PowerConnect 6248"))
        portNum = 52;
    else
        portNum = 32;
        
    for (port = 1; port <= portNum; port++)
    {
        if (HasPortBit(vpmAll->portbits, port-1))
            portList.push_back(port);
    }

    if (portList.size() == 0)
        return false;
    return true;
}

bool SNMP_Session::SNMPSet(char* oid_str, char type, char* value)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    int status;

    if (!active) //not initialized or session has been disconnected
        return false;

    // Create the PDU for the data for our SNMP request. 
    pdu = snmp_pdu_create(SNMP_MSG_SET);

    if (!(status = read_objid(oid_str, anOID, &anOID_len))) return false;
    if ((status = snmp_add_var(pdu, anOID, anOID_len, type, value))!=0) return false;

    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);

    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	snmp_free_pdu(response);
	return true;
    }
    else {
        if (status == STAT_SUCCESS){
           LOG(4)( Log::MPLS, "VLSR: SNMP: Setting SNMP at OID", oid_str, "failed. Reason : ", snmp_errstring(response->errstat));
        }
        else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
        if(response) snmp_free_pdu(response);
        return false;
    }

    return true;
}

