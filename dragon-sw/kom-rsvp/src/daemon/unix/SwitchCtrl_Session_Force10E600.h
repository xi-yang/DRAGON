/****************************************************************************

CLI Based Switch Control Module header file SwitchCtrl_Session_Force10E600.h
Created by Xi Yang @ 01/17/2006
Extended from SNMP_Global.h by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SwitchCtrl_Session_Force10E600_H_
#define _SwitchCtrl_Session_Force10E600_H_

#include "SwitchCtrl_Global.h"
#include "CLI_Session.h"

#define FORCE10_ERROR_PROMPT "% Error"

class SwitchCtrl_Session_Force10E600: public CLI_Session
{

public:
	SwitchCtrl_Session_Force10E600(): CLI_Session() { }	
	SwitchCtrl_Session_Force10E600(const String& sName, const NetAddress& swAddr): CLI_Session(sName, swAddr) { }
	virtual ~SwitchCtrl_Session_Force10E600() { }

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) 
		{ return policeInputBandwidth_ShellScript(do_undo, input_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size) ; }
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0)
		{ return limitOutputBandwidth_ShellScript(do_undo, output_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size) ; }		

	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);

	//Vendor/Model specific hook functions
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
       virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);
	virtual uint32 hook_convertVLANInterfaceToID(uint32 id);
	virtual uint32 hook_convertVLANIDToInterface(uint32 id);

	bool endWithShowVLAN(char* vlanNum) {
		if (!postAction())
			return false;
		DIE_IF_NEGATIVE(writeShell("show vlan id ", 5));
		DIE_IF_NEGATIVE(writeShell(vlanNum, 5));
		DIE_IF_NEGATIVE(writeShell("\n", 5));
		readShell(SWITCH_PROMPT, NULL, 1, 10);
		return true;
	}

	 //interface to the force10_hack module 
	bool deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged = false);
	bool addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged = false);

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth_ShellScript(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth_ShellScript(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
};


inline uint32 Port2BitForce10(uint32 port)
{
#ifdef FORCE10_SOFTWARE_V6
    return ((port>>8)&0xf)*96 + ((port)&0xff); //FTOS-ED-6.2.1
#else
    return ((port>>8)&0xf)*24 + ((port)&0xff) + 1; //FTOS-ED-5.3.1
#endif
}

inline uint32 Bit2PortForce10(uint32 bit)
{
#ifdef FORCE10_SOFTWARE_V6
    return ((bit/96)<<8) | ((bit%96)&0xff); //FTOS-ED-6.2.1
#else
    return (((bit-1)/24)<<8) | (((bit-1)%24) & 0xff) ; //FTOS-ED-5.3.1
#endif
}

#endif //ifndef _CLI_SESSION_H_

