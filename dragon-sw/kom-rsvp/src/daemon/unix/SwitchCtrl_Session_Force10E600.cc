/****************************************************************************

CLI Based Switch Control Module source file SwitchCtrl_Session_Force10E600.cc
Created by Xi Yang @ 01/17/2006
Extended from SNMP_Global.cc by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_Force10E600.h"
#include "RSVP.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_Force10E600::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    int old_vlan = getVLANbyUntaggedPort(port);
    if (old_vlan) { //Remove untagged port from old VLAN
        bit = Port2BitForce10(port);
        assert(bit < MAX_VLAN_PORT_BYTES*8);
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
        if (vpmUntagged)
            ResetPortBit(vpmUntagged->portbits, bit);
        vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
        if (vpmAll)
            ResetPortBit(vpmAll->portbits, bit);
        ret &= deleteVLANPort_ShellScript(port, old_vlan, false);
    }

    bit = Port2BitForce10(port);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged)
    SetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, bit);
        ret &= addVLANPort_ShellScript(port, vlanID, false);
    }

    return ret;
}

bool SwitchCtrl_Session_Force10E600::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    bit = Port2BitForce10(port);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
       SetPortBit(vpmAll->portbits, bit);
       ret&=addVLANPort_ShellScript(port, vlanID, true);
    }
    
    return ret;
}

bool SwitchCtrl_Session_Force10E600::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    bool ret = false;
    uint32 bit;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT)
    	return ret; //don't touch the control port!

    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
        bit = Port2BitForce10(port);
        assert(bit < MAX_VLAN_PORT_BYTES*8);
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged)
            ResetPortBit(vpmUntagged->portbits, bit);
        vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
        if (vpmAll) {
            ResetPortBit(vpmAll->portbits, bit);
            bool ret1 = this->deleteVLANPort_ShellScript(port, vlanID, false);
            bool ret2 = this->deleteVLANPort_ShellScript(port, vlanID, true);
            ret &= (ret1 || ret2);
        }
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}


bool SwitchCtrl_Session_Force10E600::addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100];
    // extern int optind;

    if (!preAction())
        return false;

    pid = -1;

    // add port to VLAN
    port_part=(portID)&0xf;     
    slot_part=(portID>>4)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    sprintf(vlanNum, "%d", vlanID);

    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

#ifdef ENABLE_SWITCH_PORT_SHUTDOWN
    DIE_IF_NEGATIVE(n = writeShell( "no shutdown\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);
#endif

    // switchport command sets an interface to layer-2 mode 
    DIE_IF_NEGATIVE(n = writeShell( "switchport\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    // exit interface configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    // enter vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "interface vlan ", 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    // add specified port as an untagged/tagged member of the specified VLAN 
    DIE_IF_NEGATIVE(n = writeShell(isTagged?(char *)"tagged ":(char *)"untagged ", 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( portName, 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    //DIE_IF_EQUAL(n, 2);

    // exit vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    if (!endWithShowVLAN(vlanNum))
        return false;
    return true;
}

bool SwitchCtrl_Session_Force10E600::deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100];
    //  extern int optind;

    if (!preAction())
        return false;

    // remove in port from VLAN
    port_part=(portID)&0xf;
    slot_part=(portID>>4)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    sprintf(vlanNum, "%d", vlanID);

#ifdef ENABLE_SWITCH_PORT_SHUTDOWN
    if (getVLANbyPort(portID) == 0) {
        DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
        DIE_IF_NEGATIVE(n = writeShell( portName, 5));
        DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
        DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));
        DIE_IF_EQUAL(n, 2);

        // shutdown a port before it is removed and put back into VLAN 1
        DIE_IF_NEGATIVE(n = writeShell( "shutdown\n", 5));
        DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));
        DIE_IF_EQUAL(n, 2);

        // exit interface port configuration mode 
        DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5));
        DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));
        DIE_IF_EQUAL(n, 2);
    }
#endif

    // enter vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "interface vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));
    DIE_IF_EQUAL(n, 2);

    // remove specified port as an untagged member/untagged of the specified VLAN 
    DIE_IF_NEGATIVE(n = writeShell( isTagged?(char *)"no tagged ":(char *)"no untagged ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, NULL, 1, 10));
    //DIE_IF_EQUAL(n, 2);

    // exit vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));
    DIE_IF_EQUAL(n, 2);

    if (!endWithShowVLAN(vlanNum))
        return false;
    return true;
}

/////--------QoS Functions------/////

//committed_rate and peak_rate are measued in Mbps; burst_size and peak_burst_size are in KB
bool SwitchCtrl_Session_Force10E600::policeInputBandwidth_ShellScript(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100], action[100];
    char append[20];
    int committed_rate_int = (int)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    port_part=(input_port)&0xf;
    slot_part=(input_port>>4)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    sprintf(vlanNum, "%d", vlan_id);
    sprintf(action, "%srate police %d", do_undo? "": "no ", committed_rate_int);
    if (burst_size > 0) {
        sprintf(append, " %d", burst_size);
        strcat(action, append);
    }
    if (peak_rate > 0.0001) {
        sprintf(append, " peak %d", (int)peak_rate);
        strcat(action, append);
        if (peak_burst_size > 0) {
            sprintf(append, " %d", peak_burst_size);
            strcat(action, append);
        }
    }

    // enter interface/port configuration mode 
    DIE_IF_NEGATIVE(n= writeShell( "interface ", 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    strcat(action, " vlan ");
    strcat(action, vlanNum);
    DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;      
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    // exit interface configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));
    DIE_IF_EQUAL(n, 2);

    if (!postAction())
        return false;
    return true;
}

bool SwitchCtrl_Session_Force10E600::limitOutputBandwidth_ShellScript(bool do_undo, uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size,  float peak_rate, int peak_burst_size)
{
    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100], action[100];
    char append[20];
    int committed_rate_int = (int)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    // add port to VLAN
    port_part=(output_port)&0xf;     
    slot_part=(output_port>>4)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    sprintf(vlanNum, "%d", vlan_id);
    sprintf(action, "%srate limit %d", do_undo? "": "no ", committed_rate_int);
    if (burst_size > 0) {
        sprintf(append, " %d", burst_size);
        strcat(action, append);
    }
    if (peak_rate > 0.0001) {
        sprintf(append, " peak %d", (int)peak_rate);
        strcat(action, append);
        if (peak_burst_size > 0) {
            sprintf(append, " %d", peak_burst_size);
            strcat(action, append);
        }
    }

    // enter interface/port configuration mode 
    DIE_IF_NEGATIVE(n= writeShell( "interface ", 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    strcat(action, " vlan ");
    strcat(action, vlanNum);
    DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10)) ;      
    DIE_IF_EQUAL(n, 2);

    // exit interface configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));
    DIE_IF_EQUAL(n, 2);


    if (!postAction())
        return false;
    return true;
}


////////-------vendor specific hook procedures------////////////

bool SwitchCtrl_Session_Force10E600::hook_createVLAN(const uint32 vlanID)
{
    int n;
    char createVlan[20];

    DIE_IF_EQUAL(vlanID, 0);
    DIE_IF_EQUAL(preAction(), false);

    sprintf(createVlan, "interface vlan %d\n", vlanID);
    DIE_IF_NEGATIVE(n= writeShell(createVlan, 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    vlanPortMapListUntagged.push_back(vpm);

    return postAction();
}

bool SwitchCtrl_Session_Force10E600::hook_removeVLAN(const uint32 vlanID)
{
    int n;
    char createVlan[20];

    DIE_IF_EQUAL(vlanID, 0);
    DIE_IF_EQUAL(preAction(), false);

    sprintf(createVlan, "no interface vlan %d\n", vlanID);
    DIE_IF_NEGATIVE(n= writeShell(createVlan, 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT, 1, 10));

    return postAction();  
}

bool SwitchCtrl_Session_Force10E600::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    vlanPortMap vpm_empty;
    memset(&vpm_empty, 0, sizeof(vlanPortMap));

    return (memcmp(&vpm.portbits, &vpm_empty.portbits, MAX_VLAN_PORT_BYTES) == 0);
}

void SwitchCtrl_Session_Force10E600::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (int i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
       }
    }
    vpm.vid = hook_convertVLANInterfaceToID((uint32)vars->name[vars->name_length - 1]);
}

bool SwitchCtrl_Session_Force10E600::hook_createVlanInterfaceToIDRefTable(vlanRefIDList &vlanRefIdConvList)
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
                                if (sscanf(ref_str, "Vlan %d", &ref_id.vlan_id) == 1)
                                    vlanRefIdConvList.push_back(ref_id);
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

bool SwitchCtrl_Session_Force10E600::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
        if (HasPortBit(vpm.portbits, Port2BitForce10(port)))
            return true;
        return false;
}

uint32 SwitchCtrl_Session_Force10E600::hook_convertVLANInterfaceToID(uint32 id)
{
    vlanRefIDList::Iterator it;
    for (it = vlanRefIdConvList.begin(); it != vlanRefIdConvList.end(); ++it)
    {
        if ((*it).ref_id == id)
            return (*it).vlan_id;
    }
 
    return 0;
}

uint32 SwitchCtrl_Session_Force10E600::hook_convertVLANIDToInterface(uint32 id)
{
    vlanRefIDList::Iterator it;
    for (it = vlanRefIdConvList.begin(); it != vlanRefIdConvList.end(); ++it)
    {
        if ((*it).vlan_id == id)
            return (*it).ref_id;
    }
 
    return 0;
}

