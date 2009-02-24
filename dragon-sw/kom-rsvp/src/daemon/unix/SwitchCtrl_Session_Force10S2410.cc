/****************************************************************************

CLI Based Switch Control Module source file SwitchCtrl_Session_Force10S2410.cc
Created by Xi Yang @ 02/23/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_Force10S2410.h"
#include "RSVP.h"
#include "RSVP_Log.h"



bool SwitchCtrl_Session_Force10S2410::preAction()
{
    if (!active || vendor!=Force10S2410|| !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( ">", "#", true, 1, 10)) ;
    if (n == 1)
    {
        DIE_IF_NEGATIVE(n= writeShell( "enable\n", 5)) ;
        //DIE_IF_NEGATIVE(n= readShell( "Password: ", NULL, 0, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( CLI_PASSWORD, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    }
    DIE_IF_NEGATIVE(n= writeShell( "configure\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    return true;
}


bool SwitchCtrl_Session_Force10S2410::postAction()
{
    if (fdout < 0 || fdin < 0)
        return false;
    int n;
    do {
        DIE_IF_NEGATIVE(writeShell("exit\n", 5));
        n = readShell(FORCE10_ERROR_PROMPT2, SWITCH_PROMPT, 1, 10);
    } while (n != 1);
    return true;
}

void SwitchCtrl_Session_Force10S2410::disconnectSwitch()
{
    char logout[50];
    sprintf (logout, "exit\nlogout\ny");
    CLI_Session::disengage(logout);
}


bool SwitchCtrl_Session_Force10S2410::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    int old_vlan = getVLANbyUntaggedPort(port);
    bit = Port2BitForce10(port);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
    if (vpmUntagged)
        ResetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
    if (vpmAll)
        ResetPortBit(vpmAll->portbits, bit);
    if (old_vlan > 1) { //Remove untagged port from old VLAN
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

bool SwitchCtrl_Session_Force10S2410::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
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

bool SwitchCtrl_Session_Force10S2410::removePortFromVLAN(uint32 port, uint32 vlanID)
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


////////-------vendor specific hook procedures------////////////

bool SwitchCtrl_Session_Force10S2410::hook_createVLAN(const uint32 vlanID)
{
    int n;
    char createVlan[20];

    DIE_IF_EQUAL(vlanID, 0);
    DIE_IF_EQUAL(preAction(), false);

    sprintf(createVlan, "interface vlan %d\n", vlanID);
    DIE_IF_NEGATIVE(n= writeShell(createVlan, 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));
    /* @@@@
    DIE_IF_NEGATIVE(n= writeShell("no shutdown\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));

    DIE_IF_NEGATIVE(n= writeShell("mtu 9200\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));
    */

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    vlanPortMapListUntagged.push_back(vpm);

    return postAction();
}

////////-------Force10 S2410 specific CLI procedures------////////////

bool SwitchCtrl_Session_Force10S2410::addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100];
    // extern int optind;

    if (!preAction())
        return false;

    pid = -1;

    // add port to VLAN
    port_part=(portID)&0xff;     
    slot_part=(portID>>8)&0xf;
    sprintf(portName, "%d/%d",slot_part,port_part);
    sprintf(vlanNum, "%d", vlanID);

    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

#ifdef ENABLE_SWITCH_PORT_SHUTDOWN
    DIE_IF_NEGATIVE(n = writeShell( "no shutdown\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);
#endif

    // switchport command sets an interface to layer-2 mode 
    /* @@@@
    DIE_IF_NEGATIVE(n = writeShell( "switchport\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);
    */
    // exit interface configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    // enter vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "interface vlan ", 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    // add specified port as an untagged/tagged member of the specified VLAN 
    DIE_IF_NEGATIVE(n = writeShell(isTagged?(char *)"tagged ":(char *)"untagged ", 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( portName, 5)) ;
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    //DIE_IF_EQUAL(n, 2);

    // exit vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5)) ;
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10)) ;
    DIE_IF_EQUAL(n, 2);

    if (!endWithShowVLAN(vlanNum))
        return false;
    return true;
}

bool SwitchCtrl_Session_Force10S2410::deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100];
    //  extern int optind;

    if (!preAction())
        return false;

    // remove in port from VLAN
    port_part=(portID)&0xff;
    slot_part=(portID>>8)&0xf;
    sprintf(portName, "%d/%d",slot_part,port_part);
    sprintf(vlanNum, "%d", vlanID);

#ifdef ENABLE_SWITCH_PORT_SHUTDOWN
    if (getVLANbyPort(portID, false) == 0) { // countVlan1 = false
        DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
        DIE_IF_NEGATIVE(n = writeShell( portName, 5));
        DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
        DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));
        DIE_IF_EQUAL(n, 2);

        // shutdown a port before it is removed and put back into VLAN 1
        DIE_IF_NEGATIVE(n = writeShell( "shutdown\n", 5));
        DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));
        DIE_IF_EQUAL(n, 2);

        // exit interface port configuration mode 
        DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5));
        DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));
        DIE_IF_EQUAL(n, 2);
    }
#endif

    // enter vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "interface vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));
    DIE_IF_EQUAL(n, 2);

    // remove specified port as an untagged member/untagged of the specified VLAN 
    DIE_IF_NEGATIVE(n = writeShell( isTagged?(char *)"no tagged ":(char *)"no untagged ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, NULL, 1, 10));
    //DIE_IF_EQUAL(n, 2);

    // exit vlan configuration mode 
    DIE_IF_NEGATIVE(n = writeShell( "exit\n", 5));
    DIE_IF_NEGATIVE(n = readShell( SWITCH_PROMPT, FORCE10_ERROR_PROMPT2, 1, 10));
    DIE_IF_EQUAL(n, 2);

    if (!endWithShowVLAN(vlanNum))
        return false;
    return true;
}



