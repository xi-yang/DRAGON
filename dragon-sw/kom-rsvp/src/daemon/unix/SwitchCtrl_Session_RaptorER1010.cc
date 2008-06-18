/****************************************************************************

Raptor (vendor) ER1010 (model) Control Module source file SwitchCtrl_Session_RatporER1010.cc
Created by Xi Yang @ 02/24/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_RaptorER1010.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_RaptorER1010::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!

    port = convertUnifiedPort2RaptorInternal(port);

    //@@@@ Raptor allows a port to be tagged in two VLANs
    //@@@@ Do we still need to do the following removal?
    //@@@@ Code haven't been changed after copied from SNMP_Session.cc
    /*
    int old_vlan = getVLANbyUntaggedPort(port);
    if (old_vlan) { //Remove untagged port from old VLAN
        mask=(~(1<<(32-port))) & 0xFFFFFFFF;
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
        if (vpmUntagged)
            vpmUntagged->ports&=mask;
        vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
        if (vpmAll)
    	    vpmAll->ports&=mask;
        //Set original ports back to their "tagged" or "untagged" states
        if (vpmUntagged) ret&=setVLANPortTag(vpmUntagged->ports , old_vlan); 
        //remove THIS untagged port out of the old VLAN
        if (vpmUntagged) ret&=setVLANPort(vpmUntagged->ports, old_vlan); 
   }
    */

    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged) //bit==1 means port is untagged
        SetPortBit(vpmUntagged->portbits, port-1);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, port-1);
        //By default Raptor move port into VLAN as untagged
        ret&=setVLANPort(vpmAll->portbits, RAPTOR_VLAN_BITLEN, vlanID) ;
    }
    else
        return false;
    //@@@@ Does Raptor require set PVID to the VLAN#?
    ret&=setVLANPVID(port, vlanID); //Set pvid

    activeVlanId = vlanID; //$$
    return ret;
}

bool SwitchCtrl_Session_RaptorER1010::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
    	return false; //don't touch the control port!

    port = convertUnifiedPort2RaptorInternal(port);

    //there is no need to remove a to-be-tagged-in-new-VLAN port from old VLAN
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, port-1);
        ret&=setVLANPort(vpmAll->portbits, RAPTOR_VLAN_BITLEN, vlanID) ;
    } else
        return false;

    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged) {
         //bit==0 means port is untagged
        ResetPortBit(vpmUntagged->portbits, port-1);
        ret&=setVLANPortTag(vpmUntagged->portbits, RAPTOR_VLAN_BITLEN, vlanID);
    }
    else
        return false;

    //@@@@ Does Raptor require set PVID to the VLAN#?
    ret&=setVLANPVID(port, vlanID); //Set pvid

    activeVlanId = vlanID; //$$
    return ret;
}

//NOP!
bool SwitchCtrl_Session_RaptorER1010::setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID)
{
    return true;
}

bool SwitchCtrl_Session_RaptorER1010::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    port = convertUnifiedPort2RaptorInternal(port);

    if ((!active) || !rfc2674_compatible || port==SWITCH_CTRL_PORT)
    	return false; //don't touch the control port!

    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN) {
    	 vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
        if (vpmAll) {
            ResetPortBit(vpmAll->portbits, port-1);
            ret &= setVLANPort(vpmAll->portbits, RAPTOR_VLAN_BITLEN, vlanID);
            vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
            if (vpmUntagged) {
                //After a port is moved out of a VLAN make it untagged in this VLAN.
                SetPortBit(vpmUntagged->portbits, port-1);
                ret&=setVLANPortTag(vpmUntagged->portbits, RAPTOR_VLAN_BITLEN, vlanID);
                //@@@@    ?+?   Set pvid to default vlan ID;
                ret&=setVLANPVID(port, 1);
            }
        }
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}


/////////--------Raptor ER1010 specific functions------///////////


///////////------Raptor ER1010 specific hooks -------///////////

bool SwitchCtrl_Session_RaptorER1010::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    return (memcmp(vpm.portbits, "\0\0\0\0\0\0", 6) == 0);
}

void SwitchCtrl_Session_RaptorER1010::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (uint32 i = 0; i < vars->val_len && i < RAPTOR_VLAN_BITLEN/8; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
       }
    }
     vpm.vid = (uint32)vars->name[vars->name_length - 1];
}

bool SwitchCtrl_Session_RaptorER1010::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    port = convertUnifiedPort2RaptorInternal(port);
    return HasPortBit(vpm.portbits, port-1);
}

bool SwitchCtrl_Session_RaptorER1010::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
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
            port = convertRaptorInternal2UnifiedPort(port);
            portList.push_back(port);
        }
    }

    if (portList.size() == 0)
        return false;
    return true;
}

