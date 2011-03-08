/****************************************************************************

Dell (vendor) PowerConnect 6224/6248/8024 (model) Control Module source file SwitchCtrl_Session_PowerConnect8000.cc
Created by Xi Yang 03/08/2011
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_PowerConnect6200_8000.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_PowerConnect8000::connectSwitch()
{
    bool ret = SwitchCtrl_Session::connectSwitch();
    ret = (ret && CLI_Session::engage(NULL));
    if (ret) active = true;
    return ret;
}

void SwitchCtrl_Session_PowerConnect8000::disconnectSwitch()
{
    CLI_Session::disengage("end\nend\nlogout\n");
    SwitchCtrl_Session::disconnectSwitch();
    active = false;
}

bool SwitchCtrl_Session_PowerConnect8000::preAction()
{
    if (!active || (vendor != PowerConnect8024 && vendor != PowerConnect6224 && vendor != PowerConnect6248) || !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( ">", "#", true, 1, 10)) ;
    if (n == 1)
    {
        DIE_IF_NEGATIVE(n= writeShell( "enable\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "Password: ", NULL, 0, 10)) ;
        if (strcmp(CLI_PASSWORD, "unknown") != 0)
            DIE_IF_NEGATIVE(n= writeShell( CLI_PASSWORD, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    }   
    DIE_IF_NEGATIVE(writeShell( "configure\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) 
    {
        LOG(1)(Log::Error, "Error when entering configure mode in SwitchCtrl_Session_PowerConnect8000_CLI::preAction()");
        readShell( SWITCH_PROMPT, NULL, 1, 10);
        return false;
    }
    return true;
}

bool SwitchCtrl_Session_PowerConnect8000::postAction()
{
    if (fdout < 0 || fdin < 0)
        return false;
    DIE_IF_NEGATIVE(writeShell("end\n", 5));
    int n;
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    return true;
}

bool SwitchCtrl_Session_PowerConnect8000::movePortToVLANAsTagged(uint32 portID, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL;

    if ((!active) || portID==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    bit = portToBit(portID);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
       SetPortBit(vpmAll->portbits, bit);
       ret&=addVLANPort_ShellScript(portID, vlanID, true);
    }
    
    return ret;
}


bool SwitchCtrl_Session_PowerConnect8000::movePortToVLANAsUntagged(uint32 portID, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || portID==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    int old_vlan = getVLANbyUntaggedPort(portID);
    bit = portToBit(portID);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
    if (vpmUntagged)
        ResetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
    if (vpmAll)
        ResetPortBit(vpmAll->portbits, bit);
    if (old_vlan > 1) { //Remove untagged port from old VLAN
        ret &= deleteVLANPort_ShellScript(portID, old_vlan, false);
    }

    bit = portToBit(portID);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged)
    SetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, bit);
        ret &= addVLANPort_ShellScript(portID, vlanID, false);
    }

    return ret;
}

// for trunk/tagged port only
bool SwitchCtrl_Session_PowerConnect8000::removePortFromVLAN(uint32 portID, uint32 vlanID)
{
    bool ret = false;
    uint32 bit;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || portID==SWITCH_CTRL_PORT)
    	return ret; //don't touch the control port!

    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
        bit = portToBit(portID);
        assert(bit < MAX_VLAN_PORT_BYTES*8);
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged)
            ResetPortBit(vpmUntagged->portbits, bit);
        vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
        if (vpmAll) {
            ResetPortBit(vpmAll->portbits, bit);
            bool ret1 = this->deleteVLANPort_ShellScript(portID, vlanID, false);
            bool ret2 = this->deleteVLANPort_ShellScript(portID, vlanID, true);
            ret &= (ret1 || ret2);
        }
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}


//committed_rate mbps--> kbps, burst_size kbyes-> bytes
bool SwitchCtrl_Session_PowerConnect8000::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    int n;
    char vlanNum[8], portName[8], action[64], vlanClassMap[32], vlanPolicyMap[32];
    int committed_rate_int = (int)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    sprintf(vlanNum, "%d", vlan_id);
    portToName(input_port, portName);
    sprintf(vlanClassMap, "class-map-vlan-%d", vlan_id);
    sprintf(vlanPolicyMap, "policy-map-vlan-%d", vlan_id);
    if (do_undo)
    {
        //create vlan-level class-map
        DIE_IF_NEGATIVE(n= writeShell( "end\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "show class-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, "Class Name...", true, 1, 10)) ;
        if (n == 2) // the class-map (and corresponding policy-map) have been defined
        {
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
            return postAction();
        }
        DIE_IF_NEGATIVE(n= writeShell( "configure\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "class-map match-all ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "match vlan ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanNum, 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        // configure vlan-level policy-map
        committed_rate_int *= 1000;
        if (burst_size < 32) burst_size = 32; //in Kbytes
        sprintf(action, "police-siimple %d %d conform-action transmit violate-action drop", committed_rate_int, burst_size); // no excess or peak burst size setting
        DIE_IF_NEGATIVE(n= writeShell( "policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( " in\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( "class ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // enter interface vlan configuration mode 
        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "interface ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // apply vlan-level policy map
        DIE_IF_NEGATIVE(n= writeShell( "service-policy in ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }
    else
    {
        //check if policy-map and class-map has been removed
        DIE_IF_NEGATIVE(n= writeShell( "end\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "show class-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, "Class Name...", true, 1, 10)) ;
        if (n == 1) // the class-map (and corresponding policy-map) have already been removed
        {
            return postAction();
        }
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "configure\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        // remove vlan-level policy map
        DIE_IF_NEGATIVE(n= writeShell( "no policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // remove vlan-level class-map
        DIE_IF_NEGATIVE(n= writeShell( "no class-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }

    // end
    if (!postAction())
        return false;
    return true;
}

bool SwitchCtrl_Session_PowerConnect8000::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    //NO-OP
    return true;
}


bool SwitchCtrl_Session_PowerConnect8000::addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    char portName[16], vlanNum[16];

    if (!preAction())
        return false;

    portToName(portID, portName);
    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    if (isTagged)
    {
        DIE_IF_NEGATIVE(n = writeShell( "switchport mode trunk\n", 5));
        DIE_IF_NEGATIVE(n = readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        DIE_IF_NEGATIVE(n = writeShell( "switchport trunk allowed vlan add ", 5));
        DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
        DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
        DIE_IF_NEGATIVE(n = readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }
    else
    {
        DIE_IF_NEGATIVE(n = writeShell( "switchport mode access\n", 5));
        DIE_IF_NEGATIVE(n = readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        DIE_IF_NEGATIVE(n = writeShell( "switchport access vlan ", 5));
        DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
        DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
        DIE_IF_NEGATIVE(n = readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }
    return postAction();
}


bool SwitchCtrl_Session_PowerConnect8000::deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    char portName[16], vlanNum[16];

    if (!preAction())
        return false;

    portToName(portID, portName);
    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    if (isTagged)
    {
        DIE_IF_NEGATIVE(n = writeShell( "switchport trunk allowed vlan remove ", 5));
        DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
        DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
        DIE_IF_NEGATIVE(n = readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }
    else
    {
        DIE_IF_NEGATIVE(n = writeShell( "no switchport access vlan\n", 5));
        DIE_IF_NEGATIVE(n = readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }
    return postAction();
}



bool SwitchCtrl_Session_PowerConnect8000::hook_createVLAN(const uint32 vlanID)
{
    int n;
    char vlanNum[16];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID); 
    DIE_IF_NEGATIVE(n = writeShell( "vlan database\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
    DIE_IF_NEGATIVE(n = writeShell( "vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();
}
bool SwitchCtrl_Session_PowerConnect8000::hook_removeVLAN(const uint32 vlanID)
{
    int n;
    char vlanNum[16];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    DIE_IF_NEGATIVE(n = writeShell( "vlan database\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;    
    DIE_IF_NEGATIVE(n = writeShell( "no vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();
}

bool SwitchCtrl_Session_PowerConnect8000::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    int n;
    char vlanNum[16];
    char buf[1024];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vpm.vid);
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5)); //exit configure mode
    DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;    
    DIE_IF_NEGATIVE(n = writeShell( "show vlan id ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    n= ReadShellPattern(buf, (char*)"1/xg",  (char*)"1/g", (char*)"#", (char*)DELL_ERROR_PROMPT, 5);
    if (n == 0)
    {
        postAction();
        return true;
    }
    if (n == READ_STOP) readShell( SWITCH_PROMPT, NULL, 1, 10);
    postAction();
    return false; //matching pattern 1 or 2 or there is error
}


void SwitchCtrl_Session_PowerConnect8000::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (unsigned int i = 0; i < vars->val_len && i < 100; i++) { //800 bits
           vpm.portbits[i] = vars->val.bitstring[i];
       }
    }

    vpm.vid = (uint32)vars->name[vars->name_length - 1];
}

bool SwitchCtrl_Session_PowerConnect8000::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    if (HasPortBit(vpm.portbits, portToBit(port)))
        return true;
    return false;
}

bool SwitchCtrl_Session_PowerConnect8000::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
    uint32 bit;
    uint32 port;
    vlanPortMap* vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(!vpmAll)
        return false;
    portList.clear();
    for (bit = 0; bit < sizeof(vpmAll->portbits)*8; bit++)
    {
        if (HasPortBit(vpmAll->portbits, bit))
        {
            port = bitToPort(bit);
            portList.push_back(port);
        }
    }

    if (portList.size() == 0)
        return false;
    return true;
}


