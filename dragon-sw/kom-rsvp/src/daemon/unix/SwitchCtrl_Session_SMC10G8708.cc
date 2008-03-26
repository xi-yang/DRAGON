/****************************************************************************
SMC (vendor) 8 ports 10G 8708 (model) Control Module source file SwitchCtrl_Session_SMC10G8708.cc
Created by John Qu @ 5/16/2007
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_SMC10G8708.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_SMC10G8708::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    LOG(3) (Log::MPLS, "Enter movePortToVLANAsUntagged  port, vlanID ", port,vlanID);
    //LOG(2) (Log::MPLS, "Enter movePortToVLANAsUntagged  vlanID= ", vlanID);
    uint32 mask;
    bool ret = true;
    vlanPortMap * vpmAll = NULL,*vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!
    

    // SMC switch, we can directly set port as untaggered.
    LOG(1) (Log::MPLS, "movePortToVLANAsUntagged  direct set port as untaggered");
    mask = 1<<(32-port);
       vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged){
        vpmUntagged->ports |=mask;
	    ret&=setVLANPortTag(vpmUntagged->ports, vlanID); //Set to "untagged"
    }
    
    //the untagged port is also in tagged port list, we should update the memory copy...
    // For SMC, don't need to set tagged port explicitly
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    LOG(2) (Log::MPLS, " DDD 1 tagged ports.. ", vpmAll->ports);
    if (vpmAll) {
        vpmAll->ports |=mask;
        LOG(2) (Log::MPLS, " DDD tagged ports.. ", vpmAll->ports);
        //ret&=setVLANPort(vpmAll->ports, vlanID) ;
    }
    
    LOG(2) (Log::MPLS, "movePortToVLANAsUntagged, now set this port pvid to vlan ", vlanID);
    //set pvid (native vlan) of the port to this vlan
    ret&=setVLANPVID(port, vlanID); //Set pvid
   
    LOG(2) (Log::MPLS, "Leaving movePortToVLANAsUntagged  ret= ", ret);
    return ret;
}



bool SwitchCtrl_Session_SMC10G8708::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
	LOG(3) (Log::MPLS, "Enter movePortToVLANAsTagged  port, vlanID ", port,vlanID);
    //LOG(2) (Log::MPLS, "Enter movePortToVLANAsTagged  vlanID= ", vlanID);
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
    /*Note: for SMC 8708,
     * If the port is in tagged port list of the vlan. It is not allowed to set the pvid of the
     * port to the vlan.
     * In another word, The port must be member of untagged port list of the vlan, in order to
     * set the port pvid to the vlan.
     * */ 
    // don't set pvid...
    //LOG(2) (Log::MPLS, "movePortToVLANAsTagged, now set this port pvid to vlan ", vlanID);
    //set pvid (native vlan) of the port to this vlan
    //ret&=setVLANPVID(port, vlanID); //Set pvid
	LOG(2) (Log::MPLS, "Leaving movePortToVLANAsTagged  ret= ", ret);
	return ret;
}

bool SwitchCtrl_Session_SMC10G8708::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    /** this method is called by RSVP_MPLS.cc
     * Note: For SMC 8708 switch. If the vlan is the pvid (native vlan) of the port,
     * we can't remove the port from vlan ( will get error). The correct procedures (for untagged case):
     * 1) fisr assign a new pvid (dafault vlan as new pvid) to the port .
     * 		a) if the port is not in default vlan untagged port list, add it to the list.
     * 		b) then set the port pvid to default vlan
     * 2) then remove the port from the vlan.
     * 
     * 
     * */
    LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::removePortFromVLAN  port, vlanID ", port,vlanID);
    //LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::removePortFromVLAN  vlanID= ", vlanID);
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT)
    	return false; //don't touch the control port!

    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN) {
    	//mask to check the port for untagged vlan
    	uint32 untaggedMask = (1<<(32-port)) & 0xFFFFFFFF;
    	vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    	if(vpmUntagged){
    		LOG(3) (Log::MPLS, "removePortFromVLAN  untaggedMask, vpmUntagged->ports ", untaggedMask, vpmUntagged->ports);
    		//check if the port is in untagged port list for this vlan
    		if(	(vpmUntagged->ports)& untaggedMask){
    				//set port pvid to default vlan.
    				if(vlanID == SMC8708_DEFAULT_VLAN){
    					// should not happen. if it happen. should not allow...
    					LOG(1) (Log::MPLS, "the target vlan is default vlan. should not allowed");
    					return false;
    				}
    				else{
    					// to set the port new pvid to default vlan
    					//the port must be a member of default vlan before we can
    					// set the port pvid to default vlan.
    					vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, SMC8708_DEFAULT_VLAN);
    					if(vpmUntagged){
    						LOG(4) (Log::MPLS, "Default vlan port list. default-vlan, untaggedMask, vpmUntagged->ports ", SMC8708_DEFAULT_VLAN,untaggedMask, vpmUntagged->ports);
    						if(	(vpmUntagged->ports)& untaggedMask){
    							// the port is in default vlan portlist. just need set pvid
    							LOG(1) (Log::MPLS, "Default vlan untagged port list contains this port. set pvid to default vlan  " );
    							ret&=setVLANPVID(port, SMC8708_DEFAULT_VLAN);	
    						}
    						else{
    							// not in port list, add the port to default vlan untagged port list
    							LOG(1) (Log::MPLS, "Default vlan untagged port list does not contain this port, add the port the default vlan untagged port list. then set pvid to default vlan " );
    							movePortToVLANAsUntagged(	port, SMC8708_DEFAULT_VLAN);
    							ret&=setVLANPVID(port, SMC8708_DEFAULT_VLAN);	
    						}
    					}
    					else{
    						//the portlist is empty, then should add the port the default vlan port list
    						LOG(1) (Log::MPLS, "Default vlan untagged port list is empty, add the port the untagged port list for default vlan. then set pvid to default vlan " );
    						movePortToVLANAsUntagged(port, SMC8708_DEFAULT_VLAN);
    						ret&=setVLANPVID(port, SMC8708_DEFAULT_VLAN);	
    					}
    				}
    		}
    	}
    	// For SMC 8708, removing the port from tagged port list for this vlan automatically
    	// deletes the port from the corresponding untagged port list for the vlan.   	
    	uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;
    	vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    	LOG(2) (Log::MPLS, "retrieved tagged port list of the vlan = ", vpmAll->ports);   	   
        if (vpmAll)
            vpmAll->ports &= mask;
    	if (vpmAll){
    	 	LOG(2) (Log::MPLS, "removePortFromVLAN  tagged port list (port is removed) of the vlan = ", vpmAll->ports);
    	    ret &= setVLANPort(vpmAll->ports, vlanID); //remove   	    
    	}
    	
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }
	LOG(2) (Log::MPLS, "leaving removePortFromVLAN  ret= ", ret);
    return ret;
}

/////////--------Vendor specific functions------///////////

bool SwitchCtrl_Session_SMC10G8708::setVLANPVID(uint32 port, uint32 vlanID)
{
	LOG(3) (Log::MPLS, "Enter setVLANPVID  port, vlanID ", port, vlanID);
    //LOG(2) (Log::MPLS, "Enter setVLANPVID  vlanID= ", vlanID);
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	bool ret = true;
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
      LOG(4) (Log::MPLS, "setVLANPVID   oid_str,type, value= ", oid_str,type, value);
	
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
		ret =  false;
	  }
	  LOG(2) (Log::MPLS, "leaving setVLANPVID  ret= ", ret);
	  return ret;
}

bool SwitchCtrl_Session_SMC10G8708::setVLANPortTag(uint32 portListNew, uint32 vlanID)
{
    /* this method sets port as untaggered for the vlan...
     * 
     * */
    LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPortTag (untagged) portListNew, vlanID ", portListNew, vlanID);
    //LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPortTag (untagged) vlanID= ", vlanID);
    RevertWordBytes(portListNew);

    //return setVLANPortTag((uint8*)&portListNew, 32, vlanID);
    //SMC 8 port 10G switch support 16bit for port list len
    return setVLANPortTag((uint8*)&portListNew, 16, vlanID);
}

bool SwitchCtrl_Session_SMC10G8708::setVLANPortTag(uint8* portbits, int bitlen, uint32 vlanID)
{
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPortTag (untagged) portbits= ",  portbits );
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPortTag (untagged) bitlen= ", bitlen);
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPortTag  (untagged)vlanID= ", vlanID);
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

	LOG(4) (Log::MPLS, "setVLANPort (untagged)   oid_str,type, value= ", oid_str,type, value);
	 
	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP(untagged): (STAT_SUCCESS)Setting VLAN Tag of Ethernet switch", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

bool SwitchCtrl_Session_SMC10G8708::setVLANPort(uint32 portListNew, uint32 vlanID)
{
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPort (tagged) portListNew= ", portListNew);
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPort (tagged)vlanID= ", vlanID);
    //because the portListNew was read in as a 32-bit long integer, port 1-8 goes the highest byte.
    //In other port-bitmask reading, we put port 1:8 in the lowest byte (1st byte of a uint8 string)
    RevertWordBytes(portListNew); 
      
    // SM 8 port 10G switch, supports 2 byts. so the bitlen is 16
    //return setVLANPort((uint8*)&portListNew, 32, vlanID);
    return setVLANPort((uint8*)&portListNew, 16, vlanID);
   
}

bool SwitchCtrl_Session_SMC10G8708::setVLANPort(uint8* portbits, int bitlen, uint32 vlanID)
{
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPort (tagged) portbits= ", portbits);
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPort (tagged) bitlen= ", bitlen);
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::setVLANPort (tagged) vlanID= ", vlanID);
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
	  // Note: this is tagged port list....
	  sprintf(oid_str, "%s.%d", supportedVendorOidString[vendor].chars(), vlanID);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='x'; 

         for (i = 0, value[0] = 0; i < (bitlen+7)/8; i++) { 
            snprintf(oct, 3, "%.2x", portbits[i]);
            strcat(value,oct);
         }
	  LOG(4) (Log::MPLS, "setVLANPort (tagged)   oid_str,type, value= ", oid_str,type, value);
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);

	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP(tagged): (STAT_SUCCESS)Setting VLAN of Ethernet switch", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

/////////////----------///////////

bool SwitchCtrl_Session_SMC10G8708::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    LOG(3) (Log::MPLS, "Enter hook_isVLANEmpty ?  ", vpm.vid, vpm.ports);

    return (vpm.ports == 0);
}

void SwitchCtrl_Session_SMC10G8708::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    //LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::hook_getPortMapFromSnmpVars vlan ", (uint32)vars->name[vars->name_length - 1]);
    
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
     LOG(3) (Log::MPLS, "SwitchCtrl_Session_SMC10G8708::hook_getPortMapFromSnmpVars  vpm.vid, ports -> ", vpm.vid, vpm.ports);
    
}

bool SwitchCtrl_Session_SMC10G8708::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    LOG(4) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::hook_hasPortinVlanPortMap  port,vid, ports ", port, vpm.vid, vpm.ports);
    
    /* Important note: For SMC 8708, all ports must be member of one untagged vlan.
     * By default, we put all ports in default vlan.
     * Therefore, we should not check default vlan in this case.
     * 
     */
    if(vpm.vid == SMC8708_DEFAULT_VLAN){
    	LOG(1) (Log::MPLS, "SwitchCtrl_Session_SMC10G8708::hook_hasPortinVlanPortMap. this is default vlan, all. don't check default vlan. return false...");  
        return false;
    }
    
    if ((vpm.ports)&(1<<(32-port))){
     	LOG(1) (Log::MPLS, "SwitchCtrl_Session_SMC10G8708::hook_hasPortinVlanPortMap...  find port , true");  
        return true;;

    }
    LOG(1) (Log::MPLS, "SwitchCtrl_Session_SMC10G8708::hook_hasPortinVlanPortMap  No port , false");  
    return false;
}

bool SwitchCtrl_Session_SMC10G8708::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::hook_getPortListbyVLAN  vlanID=", vlanID);
    uint32 port;
    vlanPortMap* vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(!vpmAll)
        return false;

    portList.clear();
    for (port = 1; port <= 32; port++)
    {
        if ((vpmAll->ports)&(1<<(32-port)) != 0)
            portList.push_back(port);
    }

    if (portList.size() == 0)
        return false;
    return true;
}

bool SwitchCtrl_Session_SMC10G8708::hook_createVLAN(const uint32 vlanID)
{
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::hook_createVLAN  vlanID=", vlanID);
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
	LOG(4) (Log::MPLS, "hook_createVLAN   oid_str,type, value= ", oid_str,type, value);
	  
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);

    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	snmp_free_pdu(response);
    }
    else {
        if (status == STAT_SUCCESS){
        LOG(4)( Log::MPLS, "VLSR: SNMP: Create Vlan ", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
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
    vpm.ports = 0;
    vlanPortMapListAll.push_back(vpm);
    vlanPortMapListUntagged.push_back(vpm);

    return true;
}

bool SwitchCtrl_Session_SMC10G8708::hook_removeVLAN(const uint32 vlanID)
{
   LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::hook_removeVLAN... will not remove  vlanID=", vlanID);
    
    /* Keep empty vlan on switch for Now.
     * 
     * */
    /*
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
	LOG(4) (Log::MPLS, "hook_createVLAN   oid_str,type, value= ", oid_str,type, value);
	
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);

    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
    	snmp_free_pdu(response);
    }
    else {
        if (status == STAT_SUCCESS){
        LOG(4)( Log::MPLS, "VLSR: SNMP: Delete Vlan ", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
        }
        else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
        if(response) snmp_free_pdu(response);
        return false;
    }
    */
    return true;
}

bool SwitchCtrl_Session_SMC10G8708::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size){
	
	LOG(3) (Log::MPLS, "Enter policeInputBandwidth  port, rate-limit ", input_port,committed_rate);
	bool ret = true;
	portRateMap * prm;
	// get the portRateMap
	if(hasPortRateMap(input_port)){
		LOG(1) (Log::MPLS, "find portRateMap-->");
		prm = getPortRateMap(input_port);
		dumpPRM(*prm);
		// get port ingress rate from sw
		uint32 InRate = getPortIngressRateFromSwitch(prm->port);	
		// make sure consistent with value from sw
		if( prm->swIngressRate < InRate)
			prm->swIngressRate = InRate;		
			
		// increase bandwidth for the port 
		if(	do_undo){ 
			LOG(1) (Log::MPLS, "Allocated (Increase) more bandwidth -->");
			prm->dragonIngressRate += (uint32)committed_rate;
			prm->swIngressRate += (uint32)committed_rate;
			// ensure max bandwidth is not over allowed max value
			if(prm->swIngressRate > SMC8708_MAX_BANDWIDTH)
				prm->swIngressRate = SMC8708_MAX_BANDWIDTH;
				
		}
		else{
			// decrease bandwidth from the port
			LOG(1) (Log::MPLS, "De-allocated (decrease) bandwidth -->");
			if( prm->dragonIngressRate > (uint32)committed_rate)
				prm->dragonIngressRate -= (uint32)committed_rate;
			else 	
				prm->dragonIngressRate = 0;
			// if dragon allocated bandwidth still over max bandwidth, don't reduce bw. 
			// this should not happen. make sure it is handled properly.
			if(	prm->dragonIngressRate < SMC8708_MAX_BANDWIDTH){
				if(	prm->swIngressRate > ((uint32)committed_rate + SMC8708_MIN_BANDWIDTH))
					prm->swIngressRate -= (uint32)committed_rate;
				else
					prm->swIngressRate=SMC8708_MIN_BANDWIDTH; // keep minimum bandwidth
					
				//swIngressRate should be euqal to sum of dragonIngressRate and SMC8708_MIN_BANDWIDTH
				if(prm->swIngressRate < (prm->dragonIngressRate + SMC8708_MIN_BANDWIDTH)){
					prm->swIngressRate = prm->dragonIngressRate + SMC8708_MIN_BANDWIDTH;
					//check not exceeding max vlaue
					if(prm->swIngressRate > SMC8708_MAX_BANDWIDTH)
						prm->swIngressRate = SMC8708_MAX_BANDWIDTH;
				}					
			}				
		}
		LOG(1) (Log::MPLS, "updated portRateMap-->");
		dumpPRM(*prm);
		
		
		//set port ingress rate_limit.
		ret = setPortIngressBandwidth(prm->port,vlan_id,prm->swIngressRate);
		//enable port rate_limit policy. 1:enable rate_limit policing. 2:disable
		ret = setPortIngressRateLimitFlag(prm->port,1);		
	}	
	LOG(2) (Log::MPLS, "leaving policeInputBandwidth  ret= ", ret);
	return ret;	
}

bool SwitchCtrl_Session_SMC10G8708::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size){
	LOG(3) (Log::MPLS, "Enter limitOutputBandwidth  port, rate-limit ", output_port,committed_rate);
	bool ret = true;
	portRateMap * prm;
	// get the portRateMap
	if(hasPortRateMap(output_port)){
		LOG(1) (Log::MPLS, "find portRateMap-->");
		prm = getPortRateMap(output_port);
		dumpPRM(*prm);
		// get port egress rate from sw
		uint32 outRate = getPortEgressRateFromSwitch(prm->port);	
		// make sure consistent with value from sw
		if( prm->swEgressRate < outRate)
			prm->swEgressRate = outRate;		
			
		// increase bandwidth for the port 
		if(	do_undo){ 
			prm->dragonEgressRate += (uint32)committed_rate;
			prm->swEgressRate += (uint32)committed_rate;
			// ensure max bandwidth is not over allowed max value
			if(prm->swEgressRate > SMC8708_MAX_BANDWIDTH)
				prm->swEgressRate = SMC8708_MAX_BANDWIDTH;
				
		}
		else{
			// decrease bandwidth from the port
			
			if( prm->dragonEgressRate > (uint32)committed_rate)
				prm->dragonEgressRate -= (uint32)committed_rate;
			else 	
				prm->dragonEgressRate = 0;
			// if dragon allocated bandwidth still over max bandwidth, don't reduce bw. 
			// this should not happen. make sure it is handled properly.
			if(	prm->dragonEgressRate < SMC8708_MAX_BANDWIDTH){
				if(	prm->swEgressRate > ((uint32)committed_rate + SMC8708_MIN_BANDWIDTH))
					prm->swEgressRate -= (uint32)committed_rate;
				else
					prm->swEgressRate=SMC8708_MIN_BANDWIDTH; // keep minimum bandwidth
					
				//swEgressRate should be euqal to sum of dragonEgressRate and SMC8708_MIN_BANDWIDTH
				if(prm->swEgressRate < (prm->dragonEgressRate + SMC8708_MIN_BANDWIDTH)){
					prm->swEgressRate = prm->dragonEgressRate + SMC8708_MIN_BANDWIDTH;
					//check not exceeding max vlaue
					if(prm->swEgressRate > SMC8708_MAX_BANDWIDTH)
						prm->swEgressRate = SMC8708_MAX_BANDWIDTH;
				}		
			}				
		}
		LOG(1) (Log::MPLS, "updated portRateMap-->");
		dumpPRM(*prm);
				
		//set port egress rate_limit.
		ret = setPortEgressBandwidth(prm->port,vlan_id,prm->swEgressRate);
		//enable port rate_limit policy. 1:enable rate_limit policing. 2:disable
		ret = setPortEgressRateLimitFlag(prm->port,1);		
	}	
	
	LOG(2) (Log::MPLS, "leaving limitOutputBandwidth  ret= ", ret);
	return ret;	
}

 bool SwitchCtrl_Session_SMC10G8708::setPortIngressBandwidth(uint32 input_port, uint32 vlan_id, uint32 committed_rate){
	
	LOG(3) (Log::MPLS, "Enter setPortIngressBandwidth  port, rate-limit ", input_port,committed_rate);
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	bool ret = true;
	//oid for port ingress rate_limit
	String tag_oid_str = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.2";

	if (!active || !rfc2674_compatible) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", tag_oid_str.chars(), input_port);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='i'; 
  	  sprintf(value, "%d", committed_rate);
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);
      LOG(4) (Log::MPLS, "setPortIngressBandwidth   oid_str,type, value= ", oid_str,type, value);
	
	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: (STAT_SUCCESS)setPortIngressBandwidth ", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		ret =  false;
	  }
	  LOG(2) (Log::MPLS, "leaving setPortIngressBandwidth  ret= ", ret);
	  return ret;
	
}

bool SwitchCtrl_Session_SMC10G8708::setPortEgressBandwidth(uint32 input_port, uint32 vlan_id, uint32 committed_rate){
	
	LOG(3) (Log::MPLS, "Enter setPortEgressBandwidth  port, rate-limit ", input_port,committed_rate);
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	bool ret = true;
	//oid for port egress rate_limit
	String tag_oid_str = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.3";

	if (!active || !rfc2674_compatible) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", tag_oid_str.chars(), input_port);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='i'; 
  	  sprintf(value, "%d", committed_rate);
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);
      LOG(4) (Log::MPLS, "setPortEgressBandwidth   oid_str,type, value= ", oid_str,type, value);
	
	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: (STAT_SUCCESS)setPortEgressBandwidth ", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		ret =  false;
	  }
	  LOG(2) (Log::MPLS, "leaving setPortEgressBandwidth  ret= ", ret);
	  return ret;
	
}

bool SwitchCtrl_Session_SMC10G8708::setPortIngressRateLimitFlag(uint32 input_port, uint32 flag){
	
	LOG(3) (Log::MPLS, "Enter setPortIngressRateLimitFlag  port, flag (1:enable 2:disable)  ", input_port,flag);
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	bool ret = true;
	//oid for port ingress rate_limit flag field
	String tag_oid_str = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.6";

	if (!active || !rfc2674_compatible) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", tag_oid_str.chars(), input_port);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='i'; 
  	  sprintf(value, "%d", flag);
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);
      LOG(4) (Log::MPLS, "setPortIngressRateLimitFlag   oid_str,type, value= ", oid_str,type, value);
	
	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: (STAT_SUCCESS)setPortIngressRateLimitFlag ", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		ret =  false;
	  }
	  LOG(2) (Log::MPLS, "leaving setPortIngressRateLimitFlag  ret= ", ret);
	  return ret;
	
}

bool SwitchCtrl_Session_SMC10G8708::setPortEgressRateLimitFlag(uint32 input_port, uint32 flag){
	
	LOG(3) (Log::MPLS, "Enter setPortEgressRateLimitFlag  port, flag (1:enable 2:disable)  ", input_port,flag);
	struct snmp_pdu *pdu;
	struct snmp_pdu *response;
	oid anOID[MAX_OID_LEN];
	size_t anOID_len = MAX_OID_LEN;
	char type, value[128], oid_str[128];
	int status;
	bool ret = true;
	//oid for port egress rate_limit flag field
	String tag_oid_str = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.7";

	if (!active || !rfc2674_compatible) //not initialized or session has been disconnected
		return false;
	// Create the PDU for the data for our request. 
	  pdu = snmp_pdu_create(SNMP_MSG_SET);

	  // vlan port list 
	  sprintf(oid_str, "%s.%d", tag_oid_str.chars(), input_port);
	  status = read_objid(oid_str, anOID, &anOID_len);
	  type='i'; 
  	  sprintf(value, "%d", flag);
	  
	  status = snmp_add_var(pdu, anOID, anOID_len, type, value);
      LOG(4) (Log::MPLS, "setPortEgressRateLimitFlag   oid_str,type, value= ", oid_str,type, value);
	
	  // Send the Request out. 
	  status = snmp_synch_response(snmpSessionHandle, pdu, &response);
	    
	  if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
	  	snmp_free_pdu(response);
	  }
	  else {
	  	if (status == STAT_SUCCESS){
			LOG(4)( Log::MPLS, "VLSR: SNMP: (STAT_SUCCESS)setPortEgressRateLimitFlag ", switchInetAddr, "failed. Reason : ", snmp_errstring(response->errstat));
	    	}
	    	else
	      		snmp_sess_perror("snmpset", snmpSessionHandle);
		if(response) snmp_free_pdu(response);
		ret =  false;
	  }
	  LOG(2) (Log::MPLS, "leaving setPortEgressRateLimitFlag  ret= ", ret);
	  return ret;
	
}

//retrieve port ingress rate-limit information from switch
void SwitchCtrl_Session_SMC10G8708::readPortIngressRateListFromSwitch(){
	LOG(1) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::readPortIngressRateListFromSwitch");	
	
	struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;
    // the rate-limit information is saved in SMC private MIB.
    // oid .1.3.6.1.4.1.202.20.47.1.16.1.2.1.2 to get all ports ingress rate-limit. 
    // In returned oid: the last number is port number, integer value is rate-limit
    String oid_str = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.2";
    

    if (!rfc2674_compatible || !snmp_enabled)
        return;

    status = read_objid(oid_str.chars(), anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));

    while (running) {
        // Create the PDU for the data for our request.
        pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
        snmp_add_null_var(pdu, anOID, anOID_len);
        LOG(2) (Log::MPLS, "readPortIngressRateListFromSwitch   oid_str = ", oid_str);
        LOG(2) (Log::MPLS, "readPortEgressRateListFromSwitch   oid_str = ", (char *)anOID);
        // Send the Request out.
        status = snmp_synch_response(snmpSessionHandle, pdu, &response);
        if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
            for (vars = response->variables; vars; vars = vars->next_variable) {
                if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                    running = false;
                    continue;
                }

                //hook_getPortMapFromSnmpVars(portmap, vars);                
                //vpmList.push_back(portmap);
                LOG(1) (Log::MPLS, "looping in readPortIngressRateListFromSwitch...");	
                getPortIngressRateFromSnmpVars(vars);
                
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
	
	
	LOG(1) (Log::MPLS, "leaving SwitchCtrl_Session_SMC10G8708::readPortIngressRateListFromSwitch");	
		
}

//retrieve port egress rate-limit information from switch
void SwitchCtrl_Session_SMC10G8708::readPortEgressRateListFromSwitch(){
	LOG(1) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::readPortEgressRateListFromSwitch");	
	
	struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;
    // the rate-limit information is saved in SMC private MIB.
    // oid .1.3.6.1.4.1.202.20.47.1.16.1.2.1.3 to get all ports egress rate-limit. 
    // In returned oid: the last number is port number, integer value is rate-limit
    String oid_str = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.3";
    

    if (!rfc2674_compatible || !snmp_enabled)
        return;

    status = read_objid(oid_str.chars(), anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));

    while (running) {
        // Create the PDU for the data for our request.
        pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
        snmp_add_null_var(pdu, anOID, anOID_len);
        LOG(2) (Log::MPLS, "readPortEgressRateListFromSwitch   oid_str = ", oid_str);
        LOG(2) (Log::MPLS, "readPortEgressRateListFromSwitch   oid_str = ", (char *)anOID);
        // Send the Request out.
        status = snmp_synch_response(snmpSessionHandle, pdu, &response);
        if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
            for (vars = response->variables; vars; vars = vars->next_variable) {
                if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                    running = false;
                    continue;
                }

                //hook_getPortMapFromSnmpVars(portmap, vars);                
                //vpmList.push_back(portmap);
                LOG(1) (Log::MPLS, "looping in readPortEgressRateListFromSwitch...");	
                getPortEgressRateFromSnmpVars(vars);
                
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
	
	
	LOG(1) (Log::MPLS, "leaving SwitchCtrl_Session_SMC10G8708::readPortEgressRateListFromSwitch");	
		
}



void SwitchCtrl_Session_SMC10G8708::getPortIngressRateFromSnmpVars(netsnmp_variable_list *vars)
{
    //LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::getPortIngressRateFromSnmpVars port, rate-limit(mb) ", (uint32)vars->name[vars->name_length - 1], ntohl(*(vars->val.integer)));
    LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::getPortIngressRateFromSnmpVars port, rate-limit(mb) ", (uint32)vars->name[vars->name_length - 1], *(vars->val.integer));
    
    //returned value is port ingress rate-limit
    uint32 swIngressRate = *(vars->val.integer);
    //the last element is port number
    uint32 port = (uint32)vars->name[vars->name_length - 1];
    if(hasPortRateMap(port)){
    	LOG(1) (Log::MPLS, "find existing portRateMap object and update data...");	   	
    	portRateMap* prm ;
    	prm = getPortRateMap(port);
    	prm->swIngressRate = swIngressRate;
    }
    else
    {
    	//first time read data from switch.
    	LOG(1) (Log::MPLS, "Create new portRateMap object and add to list...");	
    	portRateMap prm;
    	memset(&prm, 0, sizeof(portRateMap));
    	prm.port = port;
    	prm.swIngressRate = swIngressRate;
    	addPortRateMapToList(prm);   	
    }
}

void SwitchCtrl_Session_SMC10G8708::getPortEgressRateFromSnmpVars(netsnmp_variable_list *vars){
	LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::getPortEgressRateFromSnmpVars port, rate-limit(mb) ", (uint32)vars->name[vars->name_length - 1], *(vars->val.integer));
    
    //returned value is port Egress rate-limit
    uint32 swEgressRate = *(vars->val.integer);
    //the last element is port number
    uint32 port = (uint32)vars->name[vars->name_length - 1];
    if(hasPortRateMap(port)){
    	LOG(1) (Log::MPLS, "find existing portRateMap object and update data...");	
    	portRateMap* prm;
    	prm = getPortRateMap(port);
    	prm->swEgressRate = swEgressRate;
    }
    else
    {
    	//first time read data from switch.
    	LOG(1) (Log::MPLS, "Create new portRateMap object and add to list...");	
    	portRateMap prm;
    	memset(&prm, 0, sizeof(portRateMap));
    	prm.port = port;
    	prm.swEgressRate = swEgressRate;
    	addPortRateMapToList(prm);   	
    }
}
	
	
bool SwitchCtrl_Session_SMC10G8708::hasPortRateMap(uint32 targetPort){
	bool flag = false;
	LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::hasPortRateMap port, list size ", targetPort, portRateList.size());
    portRateMapList::ConstIterator iter;
    if(portRateList.empty()){
    	LOG(1) (Log::MPLS, "SwitchCtrl_Session_SMC10G8708::hasPortRateMap portRateList is empty");
    	return false;   	
    }
    for (iter = portRateList.begin(); iter != portRateList.end(); ++iter) {
        if ((*iter).port == targetPort){
            flag = true;
         	break;   
        }
    }
	LOG(2) (Log::MPLS, "leavning SwitchCtrl_Session_SMC10G8708::hasPortRateMap return flag ", flag);
	return flag;
}
	// add portRateMap to portRateList list
void SwitchCtrl_Session_SMC10G8708::addPortRateMapToList(portRateMap& prm){
	LOG(1) (Log::MPLS, "enter SwitchCtrl_Session_SMC10G8708::addPortRateMapToList");
	dumpPRM(prm);
	portRateList.push_back(prm);
}
	
portRateMap* SwitchCtrl_Session_SMC10G8708::getPortRateMap(uint32 port){
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::getPortRateMap port ", port);
    portRateMapList::Iterator iter;
    portRateMap* prm;
    
    for (iter = portRateList.begin(); iter != portRateList.end(); ++iter) {
        if ((*iter).port == port){
            prm = &(*iter);
            LOG(1) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::getPortRateMap find prm");
         	break;   
        }
    }
    return prm;
}
	
	
void SwitchCtrl_Session_SMC10G8708::dumpPortRateList(){
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::dumpPortRateList...",portRateList.size());
	portRateMapList::Iterator iter;
	for (iter = portRateList.begin(); iter != portRateList.end(); ++iter) {     
            dumpPRM(*iter);    
    }
}

void SwitchCtrl_Session_SMC10G8708::dumpPRM(portRateMap& prm){
	//LOG(1) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::dumpPRM");
    LOG(6) (Log::MPLS, "dumpPRM portRateMap.. port, IngressRate, EgRate, DragonInRate,DragonEgRate ", prm.port,prm.swIngressRate,prm.swEgressRate,prm.dragonIngressRate,prm.dragonEgressRate);     	
}

bool SwitchCtrl_Session_SMC10G8708::hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList){
	/**
	 * We take advantage of this method to read switch port ingress/egress rate-limit information
	 * this method is called from SwitchCtl_Global.cc readVLANFromSwitch();
	 * 
	 */
	LOG(1) (Log::MPLS, "Enter SwitchCtrl_Session_SMC10G8708::hook_createVlanInterfaceToIDRefTable -- read port bandwidth data from sw");
	if(!isPortRateMapLoaded()){
		LOG(1) (Log::MPLS, " HHHHHHH First time to load PortRateMaplist**************** ");
		readPortIngressRateListFromSwitch();
		readPortEgressRateListFromSwitch();	
		setPortRateMapLoaded(true);	
	}
	dumpPortRateList();
	LOG(1) (Log::MPLS, "Leaving SwitchCtrl_Session_SMC10G8708::hook_createVlanInterfaceToIDRefTable true...");
	return true;	
}

uint32 SwitchCtrl_Session_SMC10G8708::getPortIngressRateFromSwitch(uint32 port){
	uint32 portInRate = 0;
	 LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session::getPortIngressRateFromSwitch  ", port);
    
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    netsnmp_variable_list *vars;
    int status;
    // port ingress rate-limit MIB  field
    String oid_str_portrate = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.2";    
    
    if (!active || !rfc2674_compatible || !snmp_enabled)
        return false;
        
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // oid for port ingress rate 
    sprintf(oid_str, "%s.%d", oid_str_portrate.chars(), port);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    LOG(2) (Log::MPLS, "getPortIngressRateFromSwitch   oid_str =  ", oid_str);
	
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        vars = response->variables;
		//retrieve port ingress bandwidth
       portInRate = *(vars->val.integer);
       LOG(2) (Log::MPLS, "getPortIngressRateFromSwitch   port Ingress bandwidth =  ", portInRate);	
    }
    if(response) 
      snmp_free_pdu(response);

    LOG(2) (Log::MPLS, "leaving getPortIngressRateFromSwitch   portInRate =  ", portInRate);	
	return 	portInRate;
}

uint32 SwitchCtrl_Session_SMC10G8708::getPortEgressRateFromSwitch(uint32 port){
	uint32 portInRate = 0;
	 LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session::getPortEgressRateFromSwitch  ", port);
    
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    netsnmp_variable_list *vars;
    int status;
    // port egress rate-limit MIB  field
    String oid_str_portrate = ".1.3.6.1.4.1.202.20.47.1.16.1.2.1.3";    
    
    if (!active || !rfc2674_compatible || !snmp_enabled)
        return false;
        
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // oid for port ingress rate 
    sprintf(oid_str, "%s.%d", oid_str_portrate.chars(), port);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    LOG(2) (Log::MPLS, "getPortEgressRateFromSwitch   oid_str =  ", oid_str);
	
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        vars = response->variables;
		//retrieve port egress bandwidth
       portInRate = *(vars->val.integer);
       LOG(2) (Log::MPLS, "getPortEgressRateFromSwitch   port Egress bandwidth =  ", portInRate);	
    }
    if(response) 
      snmp_free_pdu(response);

    LOG(2) (Log::MPLS, "leaving getPortEgressRateFromSwitch   portInRate =  ", portInRate);	
	return 	portInRate;
}

