/****************************************************************************

Cisco (vendor) Catalyst 6500 (model) Control Module source file SwitchCtrl_Session_Catalyst6500.cc
Created by Ajay Todimala, 2007
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_Catalyst6500.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_Catalyst6500::PortTrunkingOn(uint32 port)
{

    int i;
    uint32 port_id;
    char oct[3];
    char type, value[500], oid_str[128];

    if (!active) //not initialized or session has been disconnected
        return false;

    if (!isSwitchport(port))
        SwitchPortOnOff(port, true);

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.3";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "4");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the Port encapsulation (vlanTrunkPortEncapsulationType) to dot1Q failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "5");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortDynamicState to onNoNegotiate failed.");
        return false;
    }


    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.17";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    value[0]=0;
    strcat(value,"");
    type='x'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled2k to None failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.18";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    type='x'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled3k to None failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.19";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    type='x'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled4k to None failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.4";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    type='x'; 
   
    value[0]=0;
    for (i = 0; i < 128; i++) {
	snprintf(oct, 3, "%.2x", 0);
	strcat(value,oct);
    }

    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled to None failed.");
        return false;
    }

    return true;
}


bool SwitchCtrl_Session_Catalyst6500::PortTrunkingOff(uint32 port)
{
    char type, value[128], oid_str[128];
    uint32 port_id;

    if (!active) //not initialized or session has been disconnected
        return false;

    port_id = hook_convertPortIDToInterface(port);

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "2");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortDynamicState to Off failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.3";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "5");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the Port encapsulation (vlanTrunkPortEncapsulationType) to negotiate failed.");
        return false;
    }

    // Shutdown the port
    SwitchPortOnOff(port, false);

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::PortStaticAccessOn(uint32 port)
{
    char type, value[128], oid_str[128];
    uint32 port_id;

    if (!active) //not initialized or session has been disconnected
        return false;

    if (!isSwitchport(port))
        SwitchPortOnOff(port, true);

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.3";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "5");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the Port encapsulation (vlanTrunkPortEncapsulationType) to negotiate failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "2");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortDynamicState to Off failed.");
        return false;
    }

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    /* vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    memset(vpm.portbits, 0, MAX_VLAN_PORT_BYTES);
    vlanPortMapListUntagged.push_back(vpm); */

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::PortStaticAccessOff(uint32 port)
{
   return SwitchPortOnOff(port, false);
}

bool SwitchCtrl_Session_Catalyst6500::TurnOnPort(uint32 port, bool on)
{
    char type, value[128], oid_str[128];
    if (!active)  return false; 
    uint32 port_id;
	
    String tag_oid_str = ".1.3.6.1.2.1.2.2.1.7";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    if (on) strcpy(value, "1"); 
    else strcpy(value, "2");
    type='i';
    if (!SNMPSet(oid_str, type, value)) 
    {
       LOG(3)( Log::MPLS, "VLSR: SNMP: Turning port ", port, " On/Off failed.");
       return false;
    }
   
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::SwitchPortOnOff(uint32 port, bool on)
{
    char type, value[128], oid_str[128];
    if (!active)  return false; 
    uint32 port_id;
	
    String tag_oid_str = ".1.3.6.1.4.1.9.9.151.1.1.1.1.1";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    if (on) strcpy(value, "2"); 
    else strcpy(value, "1");
    type='i';
    if (!SNMPSet(oid_str, type, value)) 
    {
       LOG(3)( Log::MPLS, "VLSR: SNMP: Turning On/Off switchport on port ", port, " failed.");
       return false;
    }
   
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::isSwitchport(uint32 port)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    uint32 port_id;

    if (!active)  
    	return false; 
	

    String tag_oid_str = ".1.3.6.1.4.1.9.9.151.1.1.1.1.1";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
        vars = response->variables;
       bool ret = ((*(vars->val.integer)) ==2);
    	snmp_free_pdu(response);
	return ret;
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(4)( Log::MPLS, "VLSR: SNMP: Reading switchport ", port, " information failed. Reason : ", snmp_errstring(response->errstat));
       }
       else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    return true;
}


bool SwitchCtrl_Session_Catalyst6500::isPortTurnedOn(uint32 port)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    uint32 port_id;

    if (!active)  
    	return false; 
	
    String tag_oid_str = ".1.3.6.1.2.1.2.2.1.7";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
        vars = response->variables;
    	snmp_free_pdu(response);
	if ((*(vars->val.integer)) ==1) return true;
	else if ((*(vars->val.integer)) ==1) return false;
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(4)( Log::MPLS, "VLSR: SNMP: Reading port ", port, " information failed. Reason : ", snmp_errstring(response->errstat));
       }
       else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;
    char type, value[300], oid_str[128];
    uint32 port_id;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<CATALYST6500_MIN_VLAN_ID || vlanID>CATALYST6500_MAX_VLAN_ID) 
    	return false; //don't touch the control port!

    PortStaticAccessOn(port);

    if (isPortTrunking(port))
        PortTrunkingOff(port);

    PortStaticAccessOn(port);

    port_id = hook_convertPortIDToInterface(port);
    port = convertUnifiedPort2Catalyst6500(port);

    String tag_oid_str = ".1.3.6.1.4.1.9.9.68.1.2.2.1.2";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    type='i'; 
    sprintf(value, "%d", vlanID);
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(3)( Log::MPLS, "VLSR: SNMP: Moving port ", port, "failed.");
        return false;
    }

    int old_vlan = getVLANbyUntaggedPort(port);
    if (old_vlan) { //Remove untagged port from old VLAN
        uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
        if (vpmUntagged)
            vpmUntagged->ports&=mask;
        vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
        if (vpmAll)
    	    vpmAll->ports&=mask;

        //Set original ports back to their "tagged" or "untagged" states
        if (vpmUntagged) setVlanPortMapById(vlanPortMapListUntagged, old_vlan, &vpmUntagged->portbits[0]); 

        //remove THIS untagged port out of the old VLAN
        if (vpmAll) setVlanPortMapById(vlanPortMapListAll, old_vlan, &vpmAll->portbits[0]); 
    }

    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged) { //bit==1 means port is untagged
        SetPortBit(vpmUntagged->portbits, port-1);
        setVlanPortMapById(vlanPortMapListUntagged, vlanID, &vpmUntagged->portbits[0]); 
    }
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
       SetPortBit(vpmAll->portbits, port-1);
        setVlanPortMapById(vlanPortMapListAll, vlanID, &vpmAll->portbits[0]); 
    }
    else
       return false;

    activeVlanId = vlanID; //$$
    return ret;
}

bool SwitchCtrl_Session_Catalyst6500::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[500], oid_str[128], oct[3];
    int status, i;
    portVlanMap vlanmap;
    uint32 port_id;
    String tag_oid_str[4] = { ".1.3.6.1.4.1.9.9.46.1.6.1.1.4", ".1.3.6.1.4.1.9.9.46.1.6.1.1.17", \
    				".1.3.6.1.4.1.9.9.46.1.6.1.1.18", ".1.3.6.1.4.1.9.9.46.1.6.1.1.19"};

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<CATALYST6500_MIN_VLAN_ID || vlanID>CATALYST6500_MAX_VLAN_ID) 
    	return false; //don't touch the control port!
	
    if (isSwitchport(port)) {
       if (!isPortTrunking(port)) {
          PortStaticAccessOff(port); 
          PortTrunkingOn(port);
       }
    }
    else PortTrunkingOn(port);
        
    // Get the current vlan mapping for the port
    port_id = hook_convertPortIDToInterface(port);
    port = convertUnifiedPort2Catalyst6500(port);
    sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
        vars = response->variables;
	hook_getVlanMapFromSnmpVars(vlanmap, vars);
    	snmp_free_pdu(response);
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(4)( Log::MPLS, "VLSR: SNMP: Reading Vlan map of Trunk port ", port, "failed. Reason : ", snmp_errstring(response->errstat));
       }
       else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    uint8 mask=((1<<(7-(vlanID%8)))) & 0xFF;
    vlanmap.vlanbits[vlanID/8] |= mask;

    // Set the vlan mapping for the port
    sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
    int j=((vlanID-1)/1024)*128;
    value[0] = 0;
    for (i = 0; i < 128; i++) {
        snprintf(oct, 3, "%.2x", vlanmap.vlanbits[i+j]);
	strcat(value,oct);
    }
    type='x';

    if (!SNMPSet(oid_str, type, value)) 
    {
       LOG(3)( Log::MPLS, "VLSR: SNMP: Setting Vlan map of Trunk port ", port, "failed.");
       return false;
    }
   
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetBit(vpmAll->portbits, port-1);
    } else
        return false;

    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged) {
         //bit==0 means port is untagged
        ResetBit(vpmUntagged->portbits, port-1);
    }
    else
        return false;

    activeVlanId = vlanID;
    return ret;
}

//NOP!
bool SwitchCtrl_Session_Catalyst6500::setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID)
{
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[500], oid_str[128], oct[3];
    int status, i, j;
    uint32 port_id;
    portVlanMap vlanmap;
    uint8 mask;
    String tag_oid_str[4] = { ".1.3.6.1.4.1.9.9.46.1.6.1.1.4", ".1.3.6.1.4.1.9.9.46.1.6.1.1.17", \
    				".1.3.6.1.4.1.9.9.46.1.6.1.1.18", ".1.3.6.1.4.1.9.9.46.1.6.1.1.19"};

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<CATALYST6500_MIN_VLAN_ID || vlanID>CATALYST6500_MAX_VLAN_ID) 
    	return false; //don't touch the control port!
	
    // We only need the remove the port if the port is Trunkport	
    if (!isPortTrunking(port)) {
        SwitchPortOnOff(port, false); //Trun off the switch port
        //removeVLAN(vlanID);
        //return true;
        port = convertUnifiedPort2Catalyst6500(port);
        goto _update_vpm;
    }

    if (!isSwitchport(port))
        return false;

    // Get the current vlan mapping for the port
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
        vars = response->variables;
	hook_getVlanMapFromSnmpVars(vlanmap, vars);
    	snmp_free_pdu(response);
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(4)( Log::MPLS, "VLSR: SNMP: Reading Vlan map of Trunk port ", port, "failed. Reason : ", snmp_errstring(response->errstat));
       }
       else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    mask =(~(1<<(7-(vlanID%8)))) & 0xFF;
    vlanmap.vlanbits[vlanID/8] &= mask;

    // Set the vlan mapping for the port
    sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
    j=((vlanID-1)/1024)*128;
    value[0] = 0;
    for (i = 0; i < 128; i++) {
        snprintf(oct, 3, "%.2x", vlanmap.vlanbits[i+j]);
	strcat(value,oct);
    }
    type='x';

    if (!SNMPSet(oid_str, type, value)) 
    {
       LOG(3)( Log::MPLS, "VLSR: SNMP: Setting Vlan map of Trunk port ", port, "failed.");
       return false;
    }

_update_vpm:

    if (vlanID>=CATALYST6500_MIN_VLAN_ID && vlanID<=CATALYST6500_MAX_VLAN_ID) {
       vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
       if (vpmAll) {
          ResetBit(vpmAll->portbits, port-1);
          vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
          if (vpmUntagged) {
             //bit==1 means port is untagged
             SetBit(vpmUntagged->portbits, port-1);
      	  }
    	  else
            return false;
       } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
       }
    }

    return ret;
}


/////////--------Catalyst 6500 specific functions------///////////


///////////------Catalyst 6500 specific hooks -------///////////

bool SwitchCtrl_Session_Catalyst6500::hook_createVLAN(const uint32 vlanID)
{
    char type, value[128], oid_str[128];

    if (!active) //not initialized or session has been disconnected
        return false;

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
    sprintf(oid_str, "%s", tag_oid_str.chars());
    strcpy(value, "2");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(3)( Log::MPLS, "VLSR: SNMP: Creating rows in the vtpVlanEditTable for the new VLAN ", vlanID, "failed.");
        return false;
    }

    // Create the VLAN using SNMP request
    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.2.1.11.1";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    strcpy(value, "4");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(3)( Log::MPLS, "VLSR: SNMP: Creating VLAN ", vlanID, "failed. ");
        return false;
    } 

    // Set the type of VLAN to 'ethernet' 
    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.2.1.3.1";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    strcpy(value, "1");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(3)( Log::MPLS, "VLSR: SNMP: Setting the type of VLAN ", vlanID, "to 'ethernet' failed. ");
        return false;
    } 

    // Apply the VLAN creation request 
    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
    sprintf(oid_str, "%s", tag_oid_str.chars());
    strcpy(value, "3");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Applying the VLAN creation request failed.");
        return false;
    }

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    memset(vpm.portbits, 0, MAX_VLAN_PORT_BYTES);
    vlanPortMapListUntagged.push_back(vpm);

    // Release the Lock of the VLAN table 
    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
    sprintf(oid_str, "%s", tag_oid_str.chars());
    strcpy(value, "4");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Releasing the Lock of the VLAN table creation failed.");
        return false;
    } 

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::hook_removeVLAN(const uint32 vlanID)
{
    char type, value[128], oid_str[128];
    if (!active) //not initialized or session has been disconnected
        return false;

    // Create a rows in the vtpVlanEditTable for the new VLAN using SNMP request
    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
    sprintf(oid_str, "%s", tag_oid_str.chars());
    strcpy(value, "2");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(3)( Log::MPLS, "VLSR: SNMP: Locking the vtpVlanEditTable for editing to remove VLAN ", vlanID , "failed. ");
        return false;
    }


    // Delete the VLAN using SNMP request
    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.2.1.11.1";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    strcpy(value, "6");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(3)( Log::MPLS, "VLSR: SNMP: Removing VLAN ", vlanID , "failed. ");
        return false;
    }

    // Apply the VLAN removal  request 
    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
    sprintf(oid_str, "%s", tag_oid_str.chars());
    strcpy(value, "3");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Applying the VLAN removal request failed.");
        return false;
    }

    // Removal of vlan info from PortMapListAll and portMapListUntagged is performed in removeVLAN (caller)

    // Release the Lock of the VLAN table 
    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
    sprintf(oid_str, "%s", tag_oid_str.chars());
    strcpy(value, "4");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Releasing the Lock of the VLAN table after VLAN removal failed.");
        return false;
    } 

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    uint8 portbits[MAX_VLAN_PORT_BYTES];
    memset(portbits, 0, MAX_VLAN_PORT_BYTES);
    return (memcmp(vpm.portbits, portbits, MAX_VLAN_PORT_BYTES) == 0);
}

void SwitchCtrl_Session_Catalyst6500::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (unsigned int i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
       }
    }
    vpm.vid = (uint32)vars->name[vars->name_length - 1];
}

bool SwitchCtrl_Session_Catalyst6500::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    port = convertUnifiedPort2Catalyst6500(port);
    return HasPortBit(vpm.portbits, port-1);
}

bool SwitchCtrl_Session_Catalyst6500::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
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
            port = convertCatalyst65002UnifiedPort(port);
            portList.push_back(port);
        }
    }

    if (portList.size() == 0)
        return false;
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::verifyVLAN(uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.3.1.1.2.1";

    if (!active || !snmp_enabled)
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



bool SwitchCtrl_Session_Catalyst6500::hook_createPortToIDRefTable(portRefIDList &portRefIdConvList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    char ref_str[100];
    portRefID ref_id;
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;
    uint32 tmp_shelf_id = 0;
    uint32 tmp_slot_id = 0; 
    uint32 tmp_port_id = 100000; 

    if (!snmpEnabled())
        return false;

    status = read_objid(".1.3.6.1.2.1.2.2.1.2", anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    portRefIdConvList.clear();
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

                            if (vars->val.string){
                                    strncpy(ref_str, (char*)vars->val.string, vars->val_len);
                                    ref_str[vars->val_len] = 0;
                            }
                            else
                                    ref_str[0] = 0;
                            if (ref_str[0] != 0) {
                                ref_id.ref_id = (int)vars->name[vars->name_length - 1];
                                tmp_port_id = 100000; 
                                tmp_slot_id = 0; 
                                if (sscanf(ref_str, "GigabitEthernet%d/0/%d", &tmp_slot_id, &tmp_port_id) == 2) {
                                  if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
    				    	ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
                                        	portRefIdConvList.push_back(ref_id);
        				    } else {
            				LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                                else if (sscanf(ref_str, "GigabitEthernet%d/%d", &tmp_slot_id, &tmp_port_id) == 2) {
    				      if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
                                        ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
                                        portRefIdConvList.push_back(ref_id);
        				    } else {
            				    LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                                else if (sscanf(ref_str, "TenGigabitEthernet%d/0/%d", &tmp_slot_id, &tmp_port_id) == 2) {
    				      if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
    				    	ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
                                        	portRefIdConvList.push_back(ref_id);
        				    } else {
            				    LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                                else if (sscanf(ref_str, "TenGigabitEthernet%d/%d", &tmp_slot_id, &tmp_port_id) == 2) {
    				      if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
                                        ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
                                        portRefIdConvList.push_back(ref_id);
        				    } else {
            				    LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                            }
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


    if (portRefIdConvList.size() == 0)
        return false;
    return true;
}

uint32 SwitchCtrl_Session_Catalyst6500::hook_convertPortInterfaceToID(uint32 id)
{
    portRefIDList::Iterator it;
    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
    {
        if ((*it).ref_id == id)
            return (*it).port_id;
    }
 
    return 0;
}

uint32 SwitchCtrl_Session_Catalyst6500::hook_convertPortIDToInterface(uint32 id)
{
    portRefIDList::Iterator it;
    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
    {
        if ((*it).port_id == id)
            return (*it).ref_id;
    }
 
    return 0;
}

// Need to change this function to use a different OID
bool SwitchCtrl_Session_Catalyst6500::hook_createVlanInterfaceToIDRefTable(vlanRefIDList &vlanRefIdConvList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    char ref_str[100];
    vlanRefID ref_id;
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;
    uint32 tmp_vlan_id = 100000; 

    if (!snmpEnabled())
        return false;

    status = read_objid(".1.3.6.1.2.1.2.2.1.2", anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    vlanRefIdConvList.clear();
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

                            if (vars->val.string){
                                    strncpy(ref_str, (char*)vars->val.string, vars->val_len);
                                    ref_str[vars->val_len] = 0;
                            }
                            else
                                    ref_str[0] = 0;
                            if (ref_str[0] != 0) {
                                ref_id.ref_id = (int)vars->name[vars->name_length - 1];
				tmp_vlan_id = 100000; 
                                if (sscanf(ref_str, "Vlan%d", &tmp_vlan_id) == 1) {
				    if (tmp_vlan_id>=CATALYST6500_MIN_VLAN_ID && tmp_vlan_id<= CATALYST6500_MAX_VLAN_ID) {
				    	ref_id.vlan_id = tmp_vlan_id;
                                    	vlanRefIdConvList.push_back(ref_id);
    				    } else {
        				LOG(2) (Log::Error, "Illegal VLAN ID ", tmp_vlan_id);
    				    }
				}
                            }
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


    if (vlanRefIdConvList.size() == 0)
        return false;
    return true;
}


bool SwitchCtrl_Session_Catalyst6500::readVlanPortMapListAllBranch(vlanPortMapList &vpmList) 
{
    portVlanMapList trunkPortVlanMapList;

    if (!snmp_enabled)
        return false;

    vpmList.clear();
    trunkPortVlanMapList.clear();

    readVlanPortMapBranch(".1.3.6.1.4.1.9.9.68.1.2.1.1.3", vpmList);
    readTrunkPortVlanMap(trunkPortVlanMapList);

    readVlanPortMapListALLFromPortVlanMapList(vpmList, trunkPortVlanMapList);
    return true;
}


bool SwitchCtrl_Session_Catalyst6500::readTrunkPortVlanMap(portVlanMapList &trunkPortVlanMapList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    int status;
    portVlanMap vlanmap;
    bool running = true;
    size_t rootlen;

    status = read_objid(".1.3.6.1.4.1.9.9.46.1.6.1.1.4", anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    while (running) {
        // Create the PDU for the data for our request.
        pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
        snmp_add_null_var(pdu, anOID, anOID_len);
        // Send the Request out.
        status = snmp_synch_response(snmpSessionHandle, pdu, &response);
        if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
	{
            for (vars = response->variables; vars; vars = vars->next_variable) 
	    {
                if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                    running = false;
                    continue;
                }

	 	int tmp_port_id = vars->name_loc[vars->name_length-1];	
		if (isPortTrunking(hook_convertPortInterfaceToID(tmp_port_id))) {
			hook_getVlanMapFromSnmpVars(vlanmap, vars);
               	 	trunkPortVlanMapList.push_back(vlanmap);
		}

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
    return true;
}


void SwitchCtrl_Session_Catalyst6500::hook_getVlanMapFromSnmpVars(portVlanMap &pvm, netsnmp_variable_list *vars)
{
    memset(&pvm, 0, sizeof(portVlanMap));
    if (vars->val.bitstring ){
        for (unsigned int i = 0; i < vars->val_len && i < CATALYST_VLAN_BITLEN/8; i++) {
            pvm.vlanbits[i] = vars->val.bitstring[i];
       }
    }
     pvm.pid = (uint32)vars->name[vars->name_length - 1];
}


bool SwitchCtrl_Session_Catalyst6500::readVlanPortMapListALLFromPortVlanMapList(vlanPortMapList &vpmList, portVlanMapList &pvmList)
{
    uint32 portId, vlanId;
    int byteIndex, bitIndex;
    uint8 vlanbyte;

    // Iterate the List of Ports in the portVlanMapList
    portVlanMapList::Iterator pvmListIter = pvmList.begin();
    while (pvmListIter != pvmList.end()) 
    {
	portId = (*pvmListIter).pid;
	uint32 port = hook_convertPortInterfaceToID(portId);
	for (byteIndex=0; byteIndex<128; byteIndex++) {
	    vlanbyte = (uint8) (*pvmListIter).vlanbits[byteIndex]; 
	    bitIndex = 0;
	    while (vlanbyte) {
	   	if (vlanbyte >=128) { 
			vlanId = (byteIndex*8) + bitIndex;

			// Set the port 'port' for Vlan with 'vlanId' in the vlanPortMapList 'vpmList'
			vlanPortMapList::Iterator vpmListIter = vpmList.begin();
			while (vpmListIter != vpmList.end())
			{
			    if ((*vpmListIter).vid == vlanId)
			    (*vpmListIter).portbits[port/8] = (*vpmListIter).portbits[port/8] + (uint8) exponentOf2((double)(port%8)-1);
			    ++vpmListIter;
			}
		}
		vlanbyte <<= 1;
		bitIndex++;
	    }
	}
	++pvmListIter;
    }

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::isPortTrunking(uint32 port)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    uint32 port_id;

    if (!active)  return false; 
    if (!isSwitchport(port)) return false;

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
       vars = response->variables;
       bool ret = ((*(vars->val.integer)) ==4);
    	snmp_free_pdu(response);
	return ret;
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(6)( Log::MPLS, "VLSR: SNMP: Reading switchport ", port, " information at OID ",  oid_str, " failed. Reason : ", snmp_errstring(response->errstat));
       }
       else
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    return false;
}



