/****************************************************************************

CLI Based Switch Control Module header file SwitchCtrl_Session_Force10S2410.h
Created by Xi Yang @ 02/23/2009
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SwitchCtrl_Session_Force10S2410_H_
#define _SwitchCtrl_Session_Force10S2410_H_

#include "SwitchCtrl_Global.h"
#include "CLI_Session.h"
#include "SwitchCtrl_Session_Force10E600.h"

#define FORCE10_ERROR_PROMPT "% Error"

class SwitchCtrl_Session_Force10S2410: public SwitchCtrl_Session_Force10E600
{

public:
	SwitchCtrl_Session_Force10S2410(): SwitchCtrl_Session_Force10E600() { }	
	SwitchCtrl_Session_Force10S2410(const String& sName, const NetAddress& swAddr): SwitchCtrl_Session_Force10E600(sName, swAddr) { }
	virtual ~SwitchCtrl_Session_Force10S2410() { }

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) 
		{ return false; }
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0)
		{ return false; }

	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	//virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID) { return true; }

	//Vendor/Model specific hook functions
	virtual bool hook_createVLAN(const uint32 vlanID);
	//virtual bool hook_removeVLAN(const uint32 vlanID); //Call Force10E600
	//virtual bool hook_isVLANEmpty(const vlanPortMap &vpm); //Call Force10E600
       //virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars); //Call Force10E600
	//virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port); //Call Force10E600
	//virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID); //Call Force10E600
	virtual uint32 hook_convertVLANInterfaceToID(uint32 id) { return id; }
	virtual uint32 hook_convertVLANIDToInterface(uint32 id) { return id; }
	virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList) { return true; }

	/*
	bool endWithShowVLAN(char* vlanNum) {
		if (!postAction())
			return false;
		DIE_IF_NEGATIVE(writeShell("show vlan id ", 5));
		DIE_IF_NEGATIVE(writeShell(vlanNum, 5));
		DIE_IF_NEGATIVE(writeShell("\n", 5));
		readShell(SWITCH_PROMPT, NULL, 1, 10);
		return true;
	}
	*/

	 //interface to the force10_hack module 
	virtual bool deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged = false);
	virtual bool addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged = false);

	///////////------QoS Functions ------/////////
	// Not applicable?
};

/*
inline uint32 Port2BitForce10(uint32 port)
{
    return ((port>>8)&0xf)*24 + ((port)&0xff) + 1;
}

inline uint32 Bit2PortForce10(uint32 bit)
{
    return (((bit-1)/24)<<8) | (((bit-1)%24) & 0xff);
}
*/

#define FORCE10_ERROR_PROMPT2 "% Invalid"

#endif

