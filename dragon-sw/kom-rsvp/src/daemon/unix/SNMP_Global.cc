/****************************************************************************

SNMP Management source file SNMP_Global.cc 
Created by Aihua Guo @ 04/10/2004 
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#include "SNMP_Global.h"
#include "RSVP_Log.h"
#include "RSVP_Message.h"
#include "force10_hack.h"
#include <signal.h>


LocalIdList SNMP_Global::localIdList;

static vlanPortMap *getVlanPortMapById(vlanPortMapList &vpmList, uint32 vid)
{
    vlanPortMapList::Iterator iter;
    for (iter = vpmList.begin(); iter != vpmList.end(); ++iter)
        if (((*iter).vid) == vid)
            return &(*iter);
    return NULL;
}

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

void SNMP_Session::disconnectSwitch() 
{
	snmp_close(sessionHandle);
	force10_hack(NULL, NULL, "disengage");
	active = false;
	vendor = Illegal;
}

const uint32 SNMP_Session::findEmptyVLAN() const{
	vlanPortMapList::ConstIterator iter;
        vlanPortMap vpm;
	memset(&vpm, 0, sizeof(vlanPortMap));
	for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter)
	    if (vendor == Force10E600) {
	        if (memcmp(&(*iter).portbits, &vpm.portbits, MAX_VLAN_PORT_BYTES) == 0)
	            return (*iter).vid;
	    } else {
	        if ((*iter).ports == 0)
                     return (*iter).vid;  
            }
	return 0;
}

//@@@@ Force10 hack inside
bool SNMP_Session::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
	uint32 mask;
       uint32 bit;
	bool ret = true;
	vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

	if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!
	int old_vlan = getVLANbyUntaggedPort(port);
	if (old_vlan) //Remove untagged port from old VLAN
	{
		if (getVendor() == SNMP_Session::Force10E600) { //force10_hack
			bit = Port2BitForce10(port);
                     assert(bit < MAX_VLAN_PORT_BYTES*8);
	              vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
	              if (vpmUntagged)
	    		    ResetPortBitForce10(vpmUntagged->portbits, bit);
	              vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
	              if (vpmAll)
	    		    ResetPortBitForce10(vpmAll->portbits, bit);
                     ret &= deleteVLANPortForce10(port, vlanID, false);
		}
		else {
			mask=(~(1<<(32-port))) & 0xFFFFFFFF;
	              vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
	              if (vpmUntagged)
	    		    vpmUntagged->ports&=mask;
	              vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
	              if (vpmAll)
	    		    vpmAll->ports&=mask;
			if (vendor == RFC2674) 
	                    if (vpmUntagged) ret&=setVLANPortTag(vpmUntagged->ports , old_vlan); //Set back to "tagged"
			if (vpmUntagged) ret&=setVLANPort(vpmUntagged->ports, old_vlan); //remove untagged port out of the old VLAN
		}
       }
	if (getVendor() == SNMP_Session::Force10E600) { //force10_hack
		bit = Port2BitForce10(port);
              assert(bit < MAX_VLAN_PORT_BYTES*8);
              vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
              if (vpmUntagged)
    		    SetPortBitForce10(vpmUntagged->portbits, bit);
              vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
              if (vpmAll) {
    		    SetPortBitForce10(vpmAll->portbits, bit);
        	    ret &= addVLANPortForce10(port, vlanID, false);
              }
	}
	else {
		mask = 1<<(32-port);
	       vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
		if (vpmUntagged)
		    vpmUntagged->ports |=mask;
	        vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
		if (vpmAll) {
		    vpmAll->ports |=mask;
		    ret&=setVLANPort(vpmAll->ports, vlanID) ;
	        }
		if (vendor == RFC2674){
	       	if (vpmUntagged)
			    ret&=setVLANPortTag(vpmUntagged->ports, vlanID); //Set to "untagged"
			ret&=setVLANPVID(port, vlanID); //Set pvid
		}
	}

	return ret;
}

//@@@@ Force10 hack inside
bool SNMP_Session::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
	uint32 mask;
       uint32 bit;
	bool ret = true;
	vlanPortMap * vpmAll = NULL;

	if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
		return false; //don't touch the control port!

	if (getVendor() == SNMP_Session::Force10E600) { //force10_hack
		bit = Port2BitForce10(port);
              assert(bit < MAX_VLAN_PORT_BYTES*8);
              vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
              if (vpmAll) {
    		    SetPortBitForce10(vpmAll->portbits, bit);
        	    ret&=addVLANPortForce10(port, vlanID, true);
              }
	}
	else {
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
	}
	return ret;
}

//@@@@ Force10 hack inside
bool SNMP_Session::removePortFromVLAN(uint32 port, uint32 vlanID)
{
	bool ret = true;
	uint32 bit;
	vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

	if ((!active) || port==SWITCH_CTRL_PORT)
		return false; //don't touch the control port!

	if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
		if (getVendor() == SNMP_Session::Force10E600) { //force10_hack
			bit = Port2BitForce10(port);
	              assert(bit < MAX_VLAN_PORT_BYTES*8);
	              vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
	              if (vpmUntagged)
	    		    ResetPortBitForce10(vpmUntagged->portbits, bit);
	              vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
	              if (vpmAll) {
	    		    ResetPortBitForce10(vpmAll->portbits, bit);
			    bool ret1 = this->deleteVLANPortForce10(port, vlanID, false);
			    bool ret2 = this->deleteVLANPortForce10(port, vlanID, true);
                         ret &= (ret1 || ret2);
	              }
		}
		else {
			uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;
			vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
	              if (vpmAll)
	                vpmAll->ports &= mask;
	              vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
	              if (vpmUntagged)
	                vpmUntagged->ports &= mask;
			if (vendor == RFC2674) {
                         if (venderSystemDescription =="PowerConnect 5224")
				ret&=setVLANPVID(port, 1); //Set pvid to default vlan ID;
		           if (vpmAll)
	                      ret&=setVLANPortTag(vpmAll->ports, vlanID); //Set back to "tagged"
			}
			if (vpmAll)
			    ret &= setVLANPort(vpmAll->ports, vlanID);
		}
	}
	return ret;
}

//@@@@ Force10 hack inside
bool SNMP_Session::movePortToDefaultVLAN(uint32 port)
{
	bool ret = true;
	uint32 bit;
	vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

	if ((!active) || port==SWITCH_CTRL_PORT)
		return false; //don't touch the control port!
	uint32 vlanID = getVLANbyUntaggedPort(port);
	if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
		if (getVendor() == SNMP_Session::Force10E600) { //force10_hack
			bit = Port2BitForce10(port);
	              assert(bit < MAX_VLAN_PORT_BYTES*8);
	              vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
	              if (vpmUntagged)
	    		    ResetPortBitForce10(vpmUntagged->portbits, bit);
	              vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
	              if (vpmAll) {
	    		    ResetPortBitForce10(vpmAll->portbits, bit);
			    bool ret1 = this->deleteVLANPortForce10(port, vlanID, false);
			    bool ret2 = this->deleteVLANPortForce10(port, vlanID, true);
                         ret &= (ret1 || ret2);
	              }
		}
		else {
			uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;	
			vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
	              if (vpmAll)
	                vpmAll->ports &= mask;
	              vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
	              if (vpmUntagged)
	                vpmUntagged->ports &= mask;
			if (vendor == RFC2674) {
                            if (venderSystemDescription =="PowerConnect 5224")
				    ret&=setVLANPVID(port, 1); //Set pvid to default vlan ID;
        		       if (vpmAll)
        	                    ret&=setVLANPortTag(vpmAll->ports, vlanID); //Set back to "tagged"
			}
			if (vpmAll)
			    ret &= setVLANPort(vpmAll->ports, vlanID);
		}
	}
	return ret;
}

//@@@@ Force10 hack inside
void SNMP_Session::readVlanPortMapBranch(const char* oid_str, vlanPortMapList &vpmList)
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

                                if (vars->val.bitstring && getVendor() == SNMP_Session::Force10E600){ //force10_hack @@@@
                                        memset(&portmap.portbits, 0, MAX_VLAN_PORT_BYTES);
                                        for (int i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
                                                portmap.portbits[i] = vars->val.bitstring[i];
                                       }
                                } else if (vars->val.integer){
                                        portmap.ports = ntohl(*(vars->val.integer));
                                        if (vars->val_len < 4) {
                                                uint32 mask = (uint32)0xFFFFFFFF << ((4-response->variables->val_len)*8);
                                                portmap.ports &= mask;
                                       }
                                }
                                else
                                        portmap.ports = 0;

                                if (getVendor() == SNMP_Session::Force10E600)
                                    portmap.vid = VlanRefToIDForce10((uint32)vars->name[vars->name_length - 1]); //Force10 Translate ...
                                else
                                    portmap.vid = (uint32)vars->name[vars->name_length - 1];
                                    
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
                }
                if(response) snmp_free_pdu(response);
        }
}

//@@@@ Force10 hack inside
bool SNMP_Session::readVLANFromSwitch()
{
    bool ret = true;;

    if (getVendor() == SNMP_Session::Force10E600)
    {
        CreateVlanRefToIDTableForce10(".1.3.6.1.2.1.2.2.1.2", vlanRefIdConvList);
        if (vlanRefIdConvList.size() == 0)
            return false;
    }

    readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.2", vlanPortMapListAll);
    if (vlanPortMapListAll.size() == 0)
        ret = false;

    readVlanPortMapBranch(".1.3.6.1.2.1.17.7.1.4.3.1.4", vlanPortMapListUntagged);
    if (vlanPortMapListUntagged.size() == 0)
        ret = false;

    return ret;
}

//@@@@ Force10 hack inside
uint32 SNMP_Session::getVLANbyPort(uint32 port){
	vlanPortMapList::Iterator iter;
	for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
	    if (vendor == Force10E600) {
	        if (HasPortBitForce10((*iter).portbits, Port2BitForce10(port)))
	            return (*iter).vid;
	    } else {
      	        if (((*iter).ports)&(1<<(32-port)))
                  return (*iter).vid;
           }
	}
	return 0;
}

//@@@@ Force10 hack inside
uint32 SNMP_Session::getVLANListbyPort(uint32 port, SimpleList<uint32> &vlan_list){
	vlan_list.clear();
	vlanPortMapList::Iterator iter;
	for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); ++iter) {
	    if (vendor == Force10E600) {
	        if (HasPortBitForce10((*iter).portbits, Port2BitForce10(port)))
	            vlan_list.push_back((*iter).vid);
	    } else {
               if (((*iter).ports)&(1<<(32-port)))
                   vlan_list.push_back((*iter).vid);
    	    }
	}
	return vlan_list.size();
}

//@@@@ Force10 hack inside
uint32 SNMP_Session::getVLANbyUntaggedPort(uint32 port){
	vlanPortMapList::Iterator iter;
	for (iter = vlanPortMapListUntagged.begin(); iter != vlanPortMapListUntagged.end(); ++iter) {
	       if (vendor == Force10E600) {
	        if (HasPortBitForce10((*iter).portbits, Port2BitForce10(port)))
	                return (*iter).vid;
	       } else {
	            if (((*iter).ports)&(1<<(32-port)))
	                 return (*iter).vid;  
	       }
	}
	return 0;
}

//@@@@ Force10 hack inside
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

    if (getVendor() == SNMP_Session::Force10E600) { //force10_hack
        vlanID = VlanIDToRefForce10(vlanID);
        if (vlanID == 0)
            return false;
    }

    //not initialized or session has been disconnected; No IntelES530 at this point.
    if (!active || (vendor!=RFC2674 && vendor!=Force10E600))
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


//@@@@ Force10 hack inside
bool SNMP_Session::VLANHasTaggedPort(uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[128], oid_str[128];
    netsnmp_variable_list *vars;
    int status;
    vlanPortMap portmap_all, portmap_untagged;
    String oid_str_all = ".1.3.6.1.2.1.17.7.1.4.3.1.2";    
    String oid_str_untagged = ".1.3.6.1.2.1.17.7.1.4.3.1.4";

    memset(&portmap_all, 0, sizeof(vlanPortMap));
    if (vendor == Force10E600)
        portmap_all.vid = VlanIDToRefForce10(vlanID);
    else
        portmap_all.vid = vlanID;
    portmap_untagged = portmap_all;

    //not initialized or session has been disconnected; No IntelES530 at this point
    if (!active || (vendor!=RFC2674 && vendor!=Force10E600))
      return false;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // all vlan ports list 
    sprintf(oid_str, "%s.%d", oid_str_all.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(sessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        vars = response->variables;
        if (vars->val.bitstring && getVendor() == SNMP_Session::Force10E600){ //force10_hack @@@@
                for (int i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
                        portmap_all.portbits[i] = vars->val.bitstring[i];
               }
        } else if (vars->val.integer){
                portmap_all.ports = ntohl(*(vars->val.integer));
                if (vars->val_len < 4) {
                        uint32 mask = (uint32)0xFFFFFFFF << ((4-response->variables->val_len)*8);
                        portmap_all.ports &= mask;
               }
        }
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

        vars = response->variables;
        if (vars->val.bitstring && getVendor() == SNMP_Session::Force10E600){ //force10_hack @@@@
                for (int i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
                        portmap_untagged.portbits[i] = vars->val.bitstring[i];
               }
        }
        else if (vars->val.integer){
                portmap_untagged.ports = ntohl(*(vars->val.integer));
                if (vars->val_len < 4) {
                        uint32 mask = (uint32)0xFFFFFFFF << ((4-response->variables->val_len)*8);
                        portmap_untagged.ports &= mask;
               }
        }
        else 
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

		venderSystemDescription = vname;
		snmp_free_pdu(response);
		if (String("PowerConnect 5224") == venderSystemDescription)
			vendor = RFC2674;
		else if (String("Intel(R) Express 530T Switch ") == venderSystemDescription)
			vendor = IntelES530;
		else if (String("Ethernet Switch") == venderSystemDescription)
			vendor = RFC2674;
		else if (String("Ethernet Routing Switch") == venderSystemDescription) // Dell PowerConnect 6024/6024F
			vendor = RFC2674;
		else if (venderSystemDescription.leftequal("Summit1i") || venderSystemDescription.leftequal("Summit5i")) 
			vendor = RFC2674;
		else if (venderSystemDescription.leftequal("Spectra")) 
			vendor = LambdaOptical;
		else if (venderSystemDescription.leftequal("Force10 Networks Real Time Operating System Software")) 
			vendor = Force10E600;
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

    //not initialized or session has been disconnected; RFC2674 only!
    if (!active || vendor!=RFC2674) 
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


////////////////////////////////////////////////////
//     Definition for the interface to force10_hack module      //
////////////////////////////////////////////////////

//@@@@ Force10 hack
bool SNMP_Session::addVLANPortForce10(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    uint32 port_part,slot_part;
    char port[100], vlan[100], action[100];
    // extern int optind;
    if (!active || vendor==Illegal) //not initialized or session has been disconnected
    return false;

    pid = -1;
    verbose = 0;
    // setup signals properly
    for(n = 1; n < NSIG; n++)
       if (n != 11)
           signal(n, sigfunct);
    signal(SIGCHLD, SIG_IGN);
#ifdef SIGPIPE
    signal(SIGPIPE, sigpipe);
#endif

    strcpy(progname, "ftos_telnet_hack"); 
    strcpy(hostname, convertAddressToString(switchInetAddr).chars());

    if (isTagged)
        strcpy(action, "add tagged");
    else
        strcpy(action, "add untagged");
    // add port to VLAN
    port_part=(portID)&0xf;     
    slot_part=(portID>>4)&0xf;
    if (slot_part < 2)
        sprintf(port, "te%d/%d",slot_part,port_part);
    else
        sprintf(port, "gi%d/%d",slot_part,port_part);
    sprintf(vlan, "%d", vlanID);
    if (force10_hack(port, vlan, action) == -1)
        return false;

    return true;
}

//@@@@ Force10 hack
bool SNMP_Session::deleteVLANPortForce10(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    uint32 port_part,slot_part;
    char port[100], vlan[100], action[100];
    //  extern int optind;
    verbose = 0;
    if (!active || vendor==Illegal) //not initialized or session has been disconnected
    return false;

    // setup signals properly
    for(n = 1; n < NSIG; n++)
        if (n != 11)
            signal(n, sigfunct);
    signal(SIGCHLD, SIG_IGN);
#ifdef SIGPIPE
    signal(SIGPIPE, sigpipe);
#endif

    strcpy(progname, "ftos_telnet_hack"); 
    strcpy(hostname, convertAddressToString(switchInetAddr).chars());

    if (isTagged)
        strcpy(action, "remove tagged");
    else
        strcpy(action, "remove untagged");

    // remove in port from VLAN
    port_part=(portID)&0xf;
    slot_part=(portID>>4)&0xf;
    if (slot_part < 2)
        sprintf(port, "te%d/%d",slot_part,port_part);
    else
        sprintf(port, "gi%d/%d",slot_part,port_part);
    sprintf(vlan, "%d", vlanID);
    if (force10_hack(port, vlan, action) == -1)
        return false;

    return true;
}

//@@@@ Force10 hack
void SNMP_Session::CreateVlanRefToIDTableForce10 (const char* oid_str, vlanRefIDList& vlanRefList)
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

        status = read_objid(oid_str, anOID, &anOID_len);
        rootlen = anOID_len;
        memcpy(root, anOID, rootlen*sizeof(oid));
        vlanRefList.clear();
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

                                if (vars->val.string){
                                        strncpy(ref_str, (char*)vars->val.string, vars->val_len);
                                        ref_str[vars->val_len] = 0;
                                }
                                else
                                        ref_str[0] = 0;
                                if (ref_str[0] != 0) {
                                    ref_id.ref_id = (int)vars->name[vars->name_length - 1];
                                    if (sscanf(ref_str, "Vlan %d", &ref_id.vlan_id) == 1)
                                        vlanRefList.push_back(ref_id);
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
}

//@@@@ Force10 hack
uint32 SNMP_Session::VlanIDToRefForce10 (uint32 vlan_id)
{
    vlanRefIDList::Iterator it;
    for (it = vlanRefIdConvList.begin(); it != vlanRefIdConvList.end(); ++it)
    {
        if ((*it).vlan_id == vlan_id)
            return (*it).ref_id;
    }
 
    return 0;
}

//@@@@ Force10 hack
uint32 SNMP_Session::VlanRefToIDForce10 (uint32 ref_id)
{
    vlanRefIDList::Iterator it;
    for (it = vlanRefIdConvList.begin(); it != vlanRefIdConvList.end(); ++it)
    {
        if ((*it).ref_id == ref_id)
            return (*it).vlan_id;
    }
 
    return 0;
}

//////////////////////////////////
     
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
