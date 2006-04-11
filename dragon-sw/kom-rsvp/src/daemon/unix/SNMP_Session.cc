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
    uint32 mask;
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!
    int old_vlan = getVLANbyUntaggedPort(port);
    if (old_vlan) { //Remove untagged port from old VLAN
        mask=(~(1<<(32-port))) & 0xFFFFFFFF;
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
        if (vpmUntagged)
            vpmUntagged->ports&=mask;
        vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
        if (vpmAll)
    	    vpmAll->ports&=mask;
        if (vendor == RFC2674) {
            //Set this port to untagged
            if (vpmUntagged) ret&=setVLANPortTag(vpmUntagged->ports , old_vlan); 
        }
        //remove THIS untagged port out of the old VLAN
        if (vpmUntagged) ret&=setVLANPort(vpmUntagged->ports, old_vlan); 
   }

    mask = 1<<(32-port);
       vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged)
        vpmUntagged->ports |=mask;
        vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        vpmAll->ports |=mask;
        ret&=setVLANPort(vpmAll->ports, vlanID) ;
        }

    if (vendor == RFC2674) {
        if (vpmUntagged)
    	    ret&=setVLANPortTag(vpmUntagged->ports, vlanID); //Set to "untagged"
        ret&=setVLANPVID(port, vlanID); //Set pvid
    }

    return ret;
}

bool SNMP_Session::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
	uint32 mask;
	bool ret = true;
	vlanPortMap * vpmAll = NULL;

	if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!

       //there is no need to remove a to-be-tagged-in-new-VLAN port from old VLAN
	mask = 1<<(32-port);
	vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
	if (vpmAll) {
	    vpmAll->ports |=mask;
	    ret&=setVLANPort(vpmAll->ports, vlanID) ;
       }
       else
            return false;
          //no need of setVLANPVID for PowerConnect 5224
	if (vendor == RFC2674 && venderSystemDescription !="PowerConnect 5224"){
		ret&=setVLANPVID(port, vlanID); //Set pvid
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
      	 uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;
    	 vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
        if (vpmAll)
            vpmAll->ports &= mask;
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged)
            vpmUntagged->ports &= mask;
        if (vendor == RFC2674) {
            if (vpmUntagged)
                ret&=setVLANPortTag(vpmUntagged->ports, vlanID); //make THIS port tagged
            if (venderSystemDescription =="PowerConnect 5224") //@@@@
                ret&=setVLANPVID(port, 1); //Set pvid to default vlan ID;
    	}
    	if (vpmAll)
    	    ret &= setVLANPort(vpmAll->ports, vlanID); //remove
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}

/////////--------Vendor specific functions------///////////

bool SNMP_Session::setVLANPVID(uint32 port, uint32 vlanID)
{
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

bool SNMP_Session::setVLANPortTag(uint32 portListNew, uint32 vlanID)
{
    RevertWordBytes(portListNew);

    return setVLANPortTag((uint8*)&portListNew, 32, vlanID);
}

bool SNMP_Session::setVLANPortTag(uint8* portbits, int bitlen, uint32 vlanID)
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128], oct[3];
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

bool SNMP_Session::setVLANPort(uint32 portListNew, uint32 vlanID)
{
    //because the portListNew was read in as a 32-bit long integer, port 1-8 goes the highest byte.
    //In other port-bitmask reading, we put port 1:8 in the lowest byte (1st byte of a uint8 string)
    RevertWordBytes(portListNew); 

    if (vendor==IntelES530) {
        uint8 value[12];
        memset(value, 0, 12);
        memcpy(value, &portListNew, sizeof(uint32));
    	 return setVLANPort(value, 96, vlanID);
    }
    else 
    	return setVLANPort((uint8*)&portListNew, 32, vlanID);
}

bool SNMP_Session::setVLANPort(uint8* portbits, int bitlen, uint32 vlanID)
{
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128], oct[3];
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

bool SNMP_Session::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    return (vpm.ports == 0);
}

void SNMP_Session::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    if (vars->val.integer){
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
    if ((vpm.ports)&(1<<(32-port)))
        return true;;

    return false;
}

