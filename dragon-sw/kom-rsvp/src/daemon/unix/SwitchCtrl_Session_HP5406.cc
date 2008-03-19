/****************************************************************************

HP (vendor) 5406 (model) Control Module source file SwitchCtrl_Session_HP5406.cc
Created by Xi Yang @ 03/18/2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_HP5406.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_HP5406::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!

    int old_vlan = getVLANbyUntaggedPort(port);  

    //Changing port from untagged into tagged in old VLAN
    if (old_vlan) {
        //uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF; // supporting up to 32 ports for now...
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
        if (vpmUntagged) {
            ResetPortBit(vpmUntagged->portbits, port-1);
            //Set port "tagged" state in the orignal VLAN
            ret&=setVLANPortTag(vpmUntagged->ports , old_vlan); 
        }
   }

    //moving port into new VLAN as tagged
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, port-1);
        ret&=setVLANPort(vpmAll->ports, vlanID);
    }

    //changing port from tagged into untagged in new VLAN
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged) {
        SetPortBit(vpmUntagged->portbits, port-1); //Setting bit=1: untagged
        ret&=setVLANPortTag(vpmAll->ports, vlanID);
    }

    //remove tagged port out of the old VLAN
    if (old_vlan) {
        vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
        if (vpmAll) {
            ResetPortBit(vpmAll->portbits, port-1);
            ret&=setVLANPort(vpmAll->ports, old_vlan);
        }
    }

    //ret&=setVLANPVID(port, vlanID); // pvid set automatically by system

    activeVlanId = vlanID; //$$
    return ret;
}

bool SwitchCtrl_Session_HP5406::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!

    //moving port into new VLAN as tagged
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, port-1);
        ret&=setVLANPort(vpmAll->ports, vlanID);
    }

    activeVlanId = vlanID; //$$
    return ret;
}

//NOP!
bool SwitchCtrl_Session_HP5406::setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID)
{
    return true;
}

bool SwitchCtrl_Session_HP5406::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT)
    	return false; //don't touch the control port!

    if (vlanID < MIN_VLAN && vlanID > MAX_VLAN) {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
        return false;
    }

    uint32 vlanHasUntaggedPort = getVLANbyUntaggedPort(port);

    //handling removing an untagged port from this VLAN
    if (vlanID == vlanHasUntaggedPort) {
        //changing port from untagged into tagged
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged) {
            ResetPortBit(vpmUntagged->portbits, port-1);;
            //Set port "tagged" state
            ret&=setVLANPortTag(vpmUntagged->ports , vlanID); 
        }

        //adding port as tagged in VLAN 1
        vpmAll = getVlanPortMapById(vlanPortMapListAll, 1);
        if (vpmAll) {
            SetPortBit(vpmAll->portbits, port-1);
            ret&=setVLANPort(vpmAll->ports, 1);
        }

        //changing port into untagged in VLAN 1
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, 1);
        if (vpmUntagged) {
            SetPortBit(vpmUntagged->portbits, port-1);
            ret&=setVLANPortTag(vpmAll->ports, 1);
        }
    }

    //finishing removing the port from this VLAN, tagged or untagged
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        ResetPortBit(vpmAll->portbits, port-1);
        ret&=setVLANPort(vpmAll->ports, vlanID);
    }

    return ret;
}


/////////--------HP 5406 specific functions------///////////


///////////------HP 5406 specific hooks -------///////////
// Although we are only handling up to 32 ports and these functions are no different than those in SNMP_Session,
// we keep them here for possible upgrade in future ...

bool SwitchCtrl_Session_HP5406::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    //$$$$ handling up to 32 ports for now
    return (memcmp(vpm.portbits, "\0\0\0\0", 4) == 0);
}

void SwitchCtrl_Session_HP5406::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (int i = 0; i < vars->val_len && i < HP5406_VLAN_BITLEN/8; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
       }
    }
     vpm.vid = (uint32)vars->name[vars->name_length - 1];
}

bool SwitchCtrl_Session_HP5406::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    return HasPortBit(vpm.portbits, port-1);
}

bool SwitchCtrl_Session_HP5406::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
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
            portList.push_back(port);
        }
    }

    if (portList.size() == 0)
        return false;
    return true;
}

