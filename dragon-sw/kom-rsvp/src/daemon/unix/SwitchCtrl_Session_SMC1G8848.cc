/****************************************************************************
SMC (vendor) 48 ports 1G 8748 (model) Control Module source file SwitchCtrl_Session_SMC1G8848.cc
Created by John Qu @ 1/21/2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_SMC1G8848.h"
#include "RSVP_Log.h"


/*
 * 24/48 port 10/100/1000 Stackable Managed Switch with 2 X 10G uplinks
*/
bool SwitchCtrl_Session_SMC1G8848::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    LOG(3) (Log::MPLS, "Enter movePortToVLANAsUntagged  port, vlanID ", port,vlanID);
    
    bool ret = true;
    vlanPortMap * vpmAll = NULL,*vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!
    
    //convert port notation from localID format to port bit pattern
    LOG(2) (Log::MPLS, "before call convertUnifiedPort2SMCInternal  port =  ", port);
    port = convertUnifiedPort2SMCInternal(port);
    LOG(2) (Log::MPLS, "after call convertUnifiedPort2SMCInternal  port =  ", port);

    // SMC switch, we can directly set port as untaggered.
    LOG(1) (Log::MPLS, "movePortToVLANAsUntagged  direct set port as untaggered");
    
       vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged){      
        SetPortBit(vpmUntagged->portbits, port-1);
        dumpPortBits(*vpmUntagged);
	    ret&=setVLANPortTag(vpmUntagged->portbits, SMC_VLAN_BITLEN , vlanID); //Set to "untagged"
    }
    
    //the untagged port is also in tagged port list, we should update the memory copy...
    // For SMC, don't need to set tagged port explicitly
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    LOG(2) (Log::MPLS, " DDD 1 tagged ports..vlanID= ",vlanID );
    if (vpmAll) {       
        SetPortBit(vpmAll->portbits, port-1);
        dumpPortBits(*vpmAll);
        LOG(2) (Log::MPLS, " DDD tagged ports..vlanID= ", vlanID);
    }
    
    LOG(2) (Log::MPLS, "movePortToVLANAsUntagged, now set this port pvid to vlan ", vlanID);
    //set pvid (native vlan) of the port to this vlan
    ret&=setVLANPVID(port, vlanID); //Set pvid
   
    LOG(2) (Log::MPLS, "Leaving movePortToVLANAsUntagged  ret= ", ret);
    return ret;
}



bool SwitchCtrl_Session_SMC1G8848::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
	LOG(3) (Log::MPLS, "Enter movePortToVLANAsTagged  port, vlanID ", port,vlanID);
    //uint32 mask;
	bool ret = true;
	vlanPortMap * vpmAll = NULL;

	if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!

	//convert port notation from localID format to port bit pattern
    LOG(2) (Log::MPLS, "before call convertUnifiedPort2SMCInternal  port =  ", port);
    port = convertUnifiedPort2SMCInternal(port);
    LOG(2) (Log::MPLS, "after call convertUnifiedPort2SMCInternal  port =  ", port);
    
    //there is no need to remove a to-be-tagged-in-new-VLAN port from old VLAN
	vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
	if (vpmAll) {
	    SetPortBit(vpmAll->portbits, port-1);
	    dumpPortBits(*vpmAll);
	    ret&=setVLANPort(vpmAll->portbits, SMC_VLAN_BITLEN, vlanID) ;
	}
   	else
        return false;
    /*Note: for SMC 8848,
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

bool SwitchCtrl_Session_SMC1G8848::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    /** this method is called by RSVP_MPLS.cc
     * Note: For SMC 8848 switch. If the vlan is the pvid (native vlan) of the port,
     * we can't remove the port from vlan ( will get error). The correct procedures (for untagged case):
     * 1) first assign a new pvid (dafault vlan as new pvid) to the port .
     * 		a) if the port is not in default vlan untagged port list, add it to the list.
     * 		b) then set the port pvid to default vlan
     * 2) then remove the port from the vlan.
     * 
     * 
     * */
    LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::removePortFromVLAN  port, vlanID ", port,vlanID);
    //LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::removePortFromVLAN  vlanID= ", vlanID);
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT)
    	return false; //don't touch the control port!

	//convert port notation from localID format to port bit pattern
    LOG(2) (Log::MPLS, "before call convertUnifiedPort2SMCInternal  port =  ", port);
    port = convertUnifiedPort2SMCInternal(port);
    LOG(2) (Log::MPLS, "after call convertUnifiedPort2SMCInternal  port =  ", port);
    
    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN) {
    	//mask to check the port for untagged vlan
    	
    	vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    	if(vpmUntagged){
    		LOG(2) (Log::MPLS, "removePortFromVLAN vpmUntagged->ports ", vpmUntagged->portbits);
    		dumpPortBits(*vpmUntagged);
    		//check if the port is in untagged port list for this vlan
    		
    		if(	HasPortBit(vpmUntagged->portbits, port-1)){
    				//set port pvid to default vlan before remove the vlan for the port
    				if(vlanID == SMC8848_DEFAULT_VLAN){
    					// should not happen. if it happen. should not allow...
    					LOG(1) (Log::MPLS, "the target vlan is default vlan. should not allowed");
    					return false;
    				}
    				else{
    					// to set the port new pvid to default vlan
    					//the port must be a member of default vlan before we can
    					// set the port pvid to default vlan.
    					vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, SMC8848_DEFAULT_VLAN);
    					if(vpmUntagged){
    						LOG(2) (Log::MPLS, "Default vlan port list. default-vlan, vpmUntagged->ports ", SMC8848_DEFAULT_VLAN);
    						dumpPortBits(*vpmUntagged);
    						if(	HasPortBit(vpmUntagged->portbits, port-1)){
    							// the port is in default vlan portlist. just need set pvid
    							LOG(1) (Log::MPLS, "Default vlan untagged port list contains this port. set pvid to default vlan  " );
    							ret&=setVLANPVID(port, SMC8848_DEFAULT_VLAN);	
    						}
    						else{
    							// not in port list, add the port to default vlan untagged port list
    							LOG(1) (Log::MPLS, "Default vlan untagged port list does not contain this port, add the port the default vlan untagged port list. then set pvid to default vlan " );
    							movePortToVLANAsUntagged(	port, SMC8848_DEFAULT_VLAN);
    							ret&=setVLANPVID(port, SMC8848_DEFAULT_VLAN);	
    						}
    					}
    					else{
    						//the portlist is empty, then should add the port the default vlan port list
    						LOG(1) (Log::MPLS, "Default vlan untagged port list is empty, add the port the untagged port list for default vlan. then set pvid to default vlan " );
    						movePortToVLANAsUntagged(port, SMC8848_DEFAULT_VLAN);
    						ret&=setVLANPVID(port, SMC8848_DEFAULT_VLAN);	
    					}
    				}
    		}
    	}
    	// For SMC 8848, removing the port from tagged port list for this vlan automatically
    	// deletes the port from the corresponding untagged port list for the vlan.   	
    	//uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;
    	vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    	LOG(1) (Log::MPLS, "retrieved tagged port list of the vlan  ");  
    	dumpPortBits(*vpmAll); 	   
        if (vpmAll)
            //vpmAll->ports &= mask;
            ResetPortBit(vpmAll->portbits, port-1);
    	if (vpmAll){
    	 	LOG(3) (Log::MPLS, "removePortFromVLAN  tagged port list (port is removed). vlan, portbits  ", vlanID, vpmAll->portbits);
    	    //ret &= setVLANPort(vpmAll->ports, vlanID); //remove   
    	    ret &= setVLANPort(vpmAll->portbits, SMC_VLAN_BITLEN , vlanID); //remove   	    
    	}
    	
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }
	LOG(2) (Log::MPLS, "leaving removePortFromVLAN  ret= ", ret);
    return ret;
}

/////////--------Vendor specific functions------///////////

bool SwitchCtrl_Session_SMC1G8848::setVLANPVID(uint32 port, uint32 vlanID)
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

bool SwitchCtrl_Session_SMC1G8848::setVLANPortTag(uint32 portListNew, uint32 vlanID)
{
    LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPortTag (untagged) portListNew, vlanID ",  portListNew, vlanID);
    //no op
    return true;
}

bool SwitchCtrl_Session_SMC1G8848::setVLANPortTag(uint8* portbits, int bitlen, uint32 vlanID)
{
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPortTag (untagged) portbits= ",  portbits );
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPortTag (untagged) bitlen= ", bitlen);
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPortTag  (untagged)vlanID= ", vlanID);
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

	  LOG(4) (Log::MPLS, "setVLANPortTag (untagged)   oid_str,type, value= ", oid_str,type, value);
	 
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
		if(response) 
			snmp_free_pdu(response);
		return false;
	  }
	  return true;
}

bool SwitchCtrl_Session_SMC1G8848::setVLANPort(uint32 portListNew, uint32 vlanID)
{
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPort (tagged) portListNew= ", portListNew);
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPort (tagged)vlanID= ", vlanID);
   
    // no op
    return true;
   
}

bool SwitchCtrl_Session_SMC1G8848::setVLANPort(uint8* portbits, int bitlen, uint32 vlanID)
{
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPort (tagged) portbits= ", portbits);
	LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPort (tagged) bitlen= ", bitlen);
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::setVLANPort (tagged) vlanID= ", vlanID);
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

LOG(2) (Log::MPLS, "setVLANPort vendor= ", vendor);
LOG(2) (Log::MPLS, "setVLANPort supportedVendorOidString[vendor].chars()= ", supportedVendorOidString[vendor].chars());
	
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

bool SwitchCtrl_Session_SMC1G8848::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    LOG(2) (Log::MPLS, "Enter hook_isVLANEmpty ?  ", vpm.vid);
    dumpPortBits(vpm);
    bool flag = true;
    for(int n = 0; n < MAX_VLAN_PORT_BYTES; n++){
    	if(vpm.portbits[n] & 0xff){
    		flag = false;
    		LOG(3) (Log::MPLS, "found port in vlan, vlan is not empty, n, portbit   ",n, vpm.portbits[n]);
    		break;	
    	}	
    }
    LOG(2) (Log::MPLS, "Enter hook_isVLANEmpty flag=   ", flag);
	return flag;
}

void SwitchCtrl_Session_SMC1G8848::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    LOG(2) (Log::MPLS, "in hook_getPortMapFromSnmpVars ( 60?)  MAX_VLAN_PORT_BYTES=" , MAX_VLAN_PORT_BYTES);
    
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = (uint32)vars->name[vars->name_length - 1];
    LOG(2) (Log::MPLS, "SwitchCtrl_Session_SMC1G8848::hook_getPortMapFromSnmpVars  vpm.vid= ", vpm.vid);   
    if (vars->val.bitstring ){
        for (uint32 i = 0; i < vars->val_len && i < SMC_VLAN_BITLEN/8; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
            //LOG(3) (Log::MPLS, "SwitchCtrl_Session_SMC1G8848::hook_getPortMapFromSnmpVars  n, portbit= ", i,vpm.portbits[i]  );   
       }
    }
    dumpPortBits(vpm);
    LOG(2) (Log::MPLS, "Leaving....hook_getPortMapFromSnmpVars  vars->val_len=" , vars->val_len);   
    
}

bool SwitchCtrl_Session_SMC1G8848::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    LOG(3) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::hook_hasPortinVlanPortMap  port,vid", port, vpm.vid);
    dumpPortBits(vpm);
    /* Important note: For SMC 8848, all ports must be member of one untagged vlan.
     * By default, we put all ports in default vlan.
     * Therefore, we should not check default vlan in this case.
     * 
     */
    if(vpm.vid == SMC8848_DEFAULT_VLAN){
    	LOG(1) (Log::MPLS, "SwitchCtrl_Session_SMC1G8848::hook_hasPortinVlanPortMap. this is default vlan, all. don't check default vlan. return false...");  
        return false;
    }
    bool ret = true;
    LOG(2) (Log::MPLS, "before call convertSMCInternal2UnifiedPort port=", port);
    port = convertUnifiedPort2SMCInternal(port);
    LOG(2) (Log::MPLS, "after call convertSMCInternal2UnifiedPort port=", port);
    ret &= HasPortBit(vpm.portbits, port-1);
    
    LOG(2) (Log::MPLS, "SwitchCtrl_Session_SMC1G8848::hook_hasPortinVlanPortMap, flag = ", ret);  
    return ret;
}

bool SwitchCtrl_Session_SMC1G8848::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::hook_getPortListbyVLAN  vlanID=", vlanID);
    uint32 port;
    uint32 bit;
    vlanPortMap* vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(!vpmAll)
        return false;

    portList.clear();
    for (bit = 0; bit < sizeof(vpmAll->portbits)*8; bit++)
    {
        if (HasPortBit(vpmAll->portbits, bit))
        {
            port = bit+1;
            LOG(2) (Log::MPLS, "before call convertSMCInternal2UnifiedPort port=", port);
            port = convertSMCInternal2UnifiedPort(port);
            LOG(2) (Log::MPLS, "after call convertSMCInternal2UnifiedPort port=", port);
            portList.push_back(port);
        }
    }
    if (portList.size() == 0)
        return false;
    return true;
}

bool SwitchCtrl_Session_SMC1G8848::hook_createVLAN(const uint32 vlanID)
{
    LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::hook_createVLAN  vlanID=", vlanID);
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
    memset(vpm.portbits, 0x00, MAX_VLAN_PORT_BYTES);
    vlanPortMapListAll.push_back(vpm);
    vlanPortMapListUntagged.push_back(vpm);


    return true;
}

bool SwitchCtrl_Session_SMC1G8848::hook_removeVLAN(const uint32 vlanID)
{
   LOG(2) (Log::MPLS, "Enter SwitchCtrl_Session_SMC1G8848::hook_removeVLAN... will not remove  vlanID=", vlanID);
    
    /* Keep empty vlan on switch for Now.
     * 
     * */
    
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
    
    return true;
}

bool SwitchCtrl_Session_SMC1G8848::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size){
	
	//LOG(3) (Log::MPLS, "Enter policeInputBandwidth  port, rate-limit ", input_port,committed_rate);
	bool ret = true;
	
	//have no information on using snmp to config bandwith
	/*
	
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
			if(prm->swIngressRate > SMC8848_MAX_BANDWIDTH)
				prm->swIngressRate = SMC8848_MAX_BANDWIDTH;
				
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
			if(	prm->dragonIngressRate < SMC8848_MAX_BANDWIDTH){
				if(	prm->swIngressRate > ((uint32)committed_rate + SMC8848_MIN_BANDWIDTH))
					prm->swIngressRate -= (uint32)committed_rate;
				else
					prm->swIngressRate=SMC8848_MIN_BANDWIDTH; // keep minimum bandwidth
					
				//swIngressRate should be euqal to sum of dragonIngressRate and SMC8848_MIN_BANDWIDTH
				if(prm->swIngressRate < (prm->dragonIngressRate + SMC8848_MIN_BANDWIDTH)){
					prm->swIngressRate = prm->dragonIngressRate + SMC8848_MIN_BANDWIDTH;
					//check not exceeding max vlaue
					if(prm->swIngressRate > SMC8848_MAX_BANDWIDTH)
						prm->swIngressRate = SMC8848_MAX_BANDWIDTH;
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
	*/
	LOG(2) (Log::MPLS, "leaving policeInputBandwidth  ret= ", ret);
	return ret;	
}

bool SwitchCtrl_Session_SMC1G8848::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size){
	//LOG(3) (Log::MPLS, "Enter limitOutputBandwidth  port, rate-limit ", output_port,committed_rate);
	bool ret = true;
	
	//have no information on using snmp to config bandwith
	
	/*
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
			if(prm->swEgressRate > SMC8848_MAX_BANDWIDTH)
				prm->swEgressRate = SMC8848_MAX_BANDWIDTH;
				
		}
		else{
			// decrease bandwidth from the port
			
			if( prm->dragonEgressRate > (uint32)committed_rate)
				prm->dragonEgressRate -= (uint32)committed_rate;
			else 	
				prm->dragonEgressRate = 0;
			// if dragon allocated bandwidth still over max bandwidth, don't reduce bw. 
			// this should not happen. make sure it is handled properly.
			if(	prm->dragonEgressRate < SMC8848_MAX_BANDWIDTH){
				if(	prm->swEgressRate > ((uint32)committed_rate + SMC8848_MIN_BANDWIDTH))
					prm->swEgressRate -= (uint32)committed_rate;
				else
					prm->swEgressRate=SMC8848_MIN_BANDWIDTH; // keep minimum bandwidth
					
				//swEgressRate should be euqal to sum of dragonEgressRate and SMC8848_MIN_BANDWIDTH
				if(prm->swEgressRate < (prm->dragonEgressRate + SMC8848_MIN_BANDWIDTH)){
					prm->swEgressRate = prm->dragonEgressRate + SMC8848_MIN_BANDWIDTH;
					//check not exceeding max vlaue
					if(prm->swEgressRate > SMC8848_MAX_BANDWIDTH)
						prm->swEgressRate = SMC8848_MAX_BANDWIDTH;
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
	*/
	LOG(2) (Log::MPLS, "leaving limitOutputBandwidth  ret= ", ret);
	return ret;	
}

void SwitchCtrl_Session_SMC1G8848::dumpPortBits(const vlanPortMap &vpm){
	char  value[128],oct[3];
	uint8 i;
	for (i = 0, value[0] = 0; i < (SMC_VLAN_BITLEN)/8; i++) { 
	        snprintf(oct, 3, "%.2x", vpm.portbits[i]);
	    strcat(value,oct);
	}
	LOG(3) (Log::MPLS, "SMC1G8848::dumpPortBits vlanid, portbits ",vpm.vid,value);
} 
