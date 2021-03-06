/****************************************************************************

Raptor (vendor) ER1010 (model) Control Module source file SwitchCtrl_Session_RatporER1010.cc
Created by Xi Yang @ 02/24/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_RaptorER1010.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_RaptorER1010_CLI::preAction()
{
    if (!active || vendor!=RaptorER1010 || !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( ">", "#", true, 1, 10)) ;
    if (n == 1)
    {
        DIE_IF_NEGATIVE(n= writeShell( "enable\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "Password:", NULL, 0, 10)) ;
        if (strcmp(CLI_ENABPASS, "unknown") != 0)
	{
    	    char onekey[2];
	    int len = strlen(CLI_ENABPASS);
	    for (int i = 0; i < len; i++) 
	    {
	        onekey[0] = CLI_ENABPASS[i];
	        onekey[1] = 0;
            	DIE_IF_NEGATIVE(n= writeShell( onekey, 2)) ;
		sleep(1);
            }
	}
        else if (strcmp(CLI_PASSWORD, "unknown") != 0)
	{
    	    char onekey[2];
	    int len = strlen(CLI_PASSWORD);
	    for (int i = 0; i < len; i++) 
	    {
	        onekey[0] = CLI_PASSWORD[i];
	        onekey[1] = 0;
            	DIE_IF_NEGATIVE(n= writeShell( onekey, 2)) ;
		sleep(1);
            }
	}
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    }
    DIE_IF_NEGATIVE(n= writeShell( "configure\n\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    return true;
}

bool SwitchCtrl_Session_RaptorER1010_CLI::postAction()
{
    if (fdout < 0 || fdin < 0)
        return false;
    int n;
    do {
        DIE_IF_NEGATIVE(writeShell("exit\n", 5));
        n = readShell("Invalid", SWITCH_PROMPT, true, 1, 10);
    } while (n != 1);
    return true;
}

//committed_rate in bit/second, burst_size in bytes
bool SwitchCtrl_Session_RaptorER1010_CLI::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    int n;
    char portName[50], vlanNum[10], action[100], vlanClassMap[50],  portPolicyMap[50];
    int committed_rate_int = (int)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    uint32 port,slot, shelf;
    port=(input_port)&0xff;
    slot=(input_port>>8)&0xf;
    shelf = (input_port>>12)&0xf;
    sprintf(portName, "%d/%d/%d",shelf, slot, port);
    sprintf(vlanNum, "%d", vlan_id);
    sprintf(vlanClassMap, "class-map-vlan-%d", vlan_id);
    sprintf(portPolicyMap, "policy-map-port-%d", input_port);
    if (do_undo)
    {
        // create vlan class-map for the port
        DIE_IF_NEGATIVE(n= writeShell( "class-map match-all ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "classmap)#", "This Diffserv class already exists.", true, 1, 10)) ;
        if (n == 2) 
        { // the class-map already exists
            readShell( SWITCH_PROMPT, NULL, 1, 10);
        }
        else 
        { // the class-map is created
            DIE_IF_NEGATIVE(n= writeShell( "match vlan ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( vlanNum, 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
            if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
            DIE_IF_EQUAL(n, 2);
            DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        }
        // create port-level inbound polilcy-map and add the class-map into vlan policy-map
        committed_rate_int *= 1000; // in Kbps
        if (burst_size < 32) // 1/4 of the max burst size
            burst_size = 32; // in KB
        else if (burst_size > 128) //max burst size 
            burst_size = 128;  // in KB
        sprintf(action, "police-simple %d %d conform-action transmit violate-action drop", committed_rate_int, burst_size); // no excess or peak burst size setting
        DIE_IF_NEGATIVE(n= writeShell( "policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( portPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( " in\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "policy-map)#", "A Diffserv policy with this name already exists.", true, 1, 10)) ;
        if (n == 2) 
        { // the policy-map already exists
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            DIE_IF_NEGATIVE(n= writeShell( "policy-map ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( portPolicyMap, 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            readShell( SWITCH_PROMPT, NULL, 1, 10);
        }

        //add class-map in the policy-map
        DIE_IF_NEGATIVE(n= writeShell( "class ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;

        // enter interface
        DIE_IF_NEGATIVE(n= writeShell( "interface  ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);

        // set mtu to 9216 for the interface
        DIE_IF_NEGATIVE(n= writeShell( "mtu 9216\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        
        // apply vlan-level policy map on the interface
        DIE_IF_NEGATIVE(n= writeShell( "no shutdown\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "service-policy in ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( portPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    }
    else
    {
        // parse policy-map content
        char bufOutput[LINELEN];
        SimpleList<String> policyClassMaps;
        SimpleList<int> policyCIRs;
        SimpleList<int> policyCBSs;
        SimpleList<String>::Iterator iterClassMap;
        SimpleList<int>::Iterator iterPolicyCIR;
        SimpleList<int>::Iterator iterPolicyCBS;

        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "show policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( portPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n-------------------\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShellBuffer( bufOutput, "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE (n = getPolicyClassMaps(bufOutput, policyClassMaps, policyCIRs, policyCBSs));
        DIE_IF_NEGATIVE(n= writeShell( "configure\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;

        // remove service-policy and port policy-map for the interface if this is the last vlan class-map in the policy-map
        if (policyClassMaps.size() == 1)
        {
            DIE_IF_NEGATIVE(n= writeShell( "no service-policy in ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( portPolicyMap, 5)) ; // try port as GigE interface
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10));
            DIE_IF_NEGATIVE(n= writeShell( "no policy-map ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( portPolicyMap, 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
            if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
            DIE_IF_EQUAL(n, 2);
        }
        else if (policyClassMaps.size() > 1)
        {
            SimpleList<String> policyClassMaps2;
            SimpleList<int> policyCIRs2;
            SimpleList<int> policyCBSs2;
            
            iterClassMap = policyClassMaps.begin();
            iterPolicyCIR = policyCIRs.begin();
            iterPolicyCBS = policyCBSs.begin();
            for (; iterClassMap != policyClassMaps.end(); ++iterClassMap, ++iterPolicyCIR, ++iterPolicyCBS)
            {
                if ((*iterClassMap) == vlanClassMap)
                    break;
            }
            for ( ; iterClassMap != policyClassMaps.end(); ++iterClassMap, ++iterPolicyCIR, ++iterPolicyCBS)
            {
                policyClassMaps2.push_front(*iterClassMap);
                policyCIRs2.push_front(*iterPolicyCIR);
                policyCBSs2.push_front(*iterPolicyCBS);
            }

            // remove the vlan-class-map from policy-map in reverse order until the current one
            DIE_IF_NEGATIVE(n= writeShell( "policy-map ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( portPolicyMap, 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            iterClassMap = policyClassMaps2.begin();
            iterPolicyCIR = policyCIRs2.begin();
            iterPolicyCBS = policyCBSs2.begin();
            for (; iterClassMap != policyClassMaps2.end(); ++iterClassMap, ++iterPolicyCIR, ++iterPolicyCBS)
            {
                DIE_IF_NEGATIVE(n= writeShell( "no class ", 5)) ;
                DIE_IF_NEGATIVE(n= writeShell( (*iterClassMap).chars(), 5)) ;
                DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
                DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
            }

            // add back the removed vlan-class-maps to policy-map except for the current one
            policyClassMaps2.pop_back();
            policyCIRs2.pop_back();
            policyCBSs2.pop_back();
            iterClassMap = policyClassMaps2.begin();
            iterPolicyCIR = policyCIRs2.begin();
            iterPolicyCBS = policyCBSs2.begin();
            for (; iterClassMap != policyClassMaps2.end(); ++iterClassMap, ++iterPolicyCIR, ++iterPolicyCBS)
            {
                DIE_IF_NEGATIVE(n= writeShell( "class ", 5)) ;
                DIE_IF_NEGATIVE(n= writeShell( (*iterClassMap).chars(), 5)) ;
                DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
                DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
                if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
                DIE_IF_EQUAL(n, 2);
                if ((*iterPolicyCIR) == 0 ||  (*iterPolicyCBS)== 0)
                {
                    DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
                    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
                    continue;
                }
                sprintf(action, "police-simple %d %d conform-action transmit violate-action drop", *iterPolicyCIR, *iterPolicyCBS);
                DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
                DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
                DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
                if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
                DIE_IF_EQUAL(n, 2);
                DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
                DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
            }            
            DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        }
        // remove input vlan class-map
        DIE_IF_NEGATIVE(n= writeShell( "no class-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", RAPTOR_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }

    // end
    if (!postAction())
        return false;
    return true;
}

int SwitchCtrl_Session_RaptorER1010_CLI::getPolicyClassMaps(char* buf, SimpleList<String>&classMaps, SimpleList<int>& policyCIRs, SimpleList<int>& policyCBSs)
{
    char* pStr = buf;
    char classMapCstr[32];
    int cir, cbs;

    while ((pStr = strstr(pStr, "Class Name.....................................")) != NULL)
    {
        pStr += 48;
        sscanf(pStr, "%s", classMapCstr);
        classMaps.push_back(String(classMapCstr));
        cir = cbs = 0;
        pStr = strstr(pStr, "Committed Rate.................................");
        if (!pStr)
        {
            LOG(2)( Log::MPLS, "SwitchCtrl_Session_RaptorER1010_CLI::getPolicyClassMaps - failed to parse CIR for policy-class:  ", classMapCstr);
            return -1;
        }  
        pStr += 48;
        sscanf(pStr, "%d", &cir);
        pStr = strstr(pStr, "Committed Burst Size...........................");
        if (!pStr)
        {
            LOG(2)( Log::MPLS, "SwitchCtrl_Session_RaptorER1010_CLI::getPolicyClassMaps - failed to parse  CBS for policy-class:  ", classMapCstr);
            return -1;
        }  
        pStr += 48;
        sscanf(pStr, "%d", &cbs);
        policyCIRs.push_back(cir);
        policyCBSs.push_back(cbs);
    }

    if (classMaps.size() == 0)
        return -1;
    return 0;
}
    
bool SwitchCtrl_Session_RaptorER1010_CLI::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (!postAction())
        return false;
    return true;
}


////////////////////////////////////////////////

bool SwitchCtrl_Session_RaptorER1010::connectSwitch()
{
    if (SwitchCtrl_Session::connectSwitch() == false)
        return false;

    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        cliSession.vendor = this->vendor;
        cliSession.active = true;
        LOG(2)( Log::MPLS, "VLSR: CLI connecting to RaptorER1010 Switch: ", switchInetAddr);
        return cliSession.engage("User:");
    }

    return true;
}

void SwitchCtrl_Session_RaptorER1010::disconnectSwitch()
{
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        char logout[50];
        sprintf (logout, "exit\nlogout\nn"); //logout without saving 'n' 
        LOG(2)( Log::MPLS, "VLSR: CLI disconnecting from RaptorER1010 Switch: ", switchInetAddr);
        cliSession.disengage(logout);
        cliSession.active = false;
    }
}


bool SwitchCtrl_Session_RaptorER1010::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        return cliSession.policeInputBandwidth(do_undo, input_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size); 
    }
    return false;
}

bool SwitchCtrl_Session_RaptorER1010::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        return cliSession.limitOutputBandwidth(do_undo, output_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size);
    }
    return false;
}

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
        if (vpmUntagged) ret&=setVLANPort(vpmAll->ports, old_vlan); 
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

