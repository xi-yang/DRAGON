/****************************************************************************

CLI Based Switch Control Module header file SwitchCtrl_Session_BrocadeNetIron.h
Created by Xi Yang @ 02/27/2012
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SwitchCtrl_Session_BrocadeNetIron_H_
#define _SwitchCtrl_Session_BrocadeNetIron_H_

#include "SwitchCtrl_Global.h"
#include "CLI_Session.h"

#define BROCADE_ERROR_PROMPT "Invalid"

class SwitchCtrl_Session_BrocadeNetIron: public CLI_Session
{

public:
	SwitchCtrl_Session_BrocadeNetIron(): CLI_Session() { }	
	SwitchCtrl_Session_BrocadeNetIron(const String& sName, const NetAddress& swAddr): CLI_Session(sName, swAddr) { }
	virtual ~SwitchCtrl_Session_BrocadeNetIron() { }

	virtual bool connectSwitch();
	virtual void disconnectSwitch();
	virtual bool pipeAlive();
	virtual bool preAction();
	virtual bool postAction();

	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID) { return true; }
	
	uint32 Port2Bit(uint32 port)
	{
		return (port&0x00ff)+(((port>>8)&0x000f)-1)*48-1;  //zero based bits: 80 00; one based ports: 1/1, 2/1
	}
	
	uint32 Bit2Port(uint32 bit)
	{
		return (((bit/48+1)&0x000f)<<8)|(((bit+1)%48)&0xff);
	}

	///vendor specific shell scripts
	bool addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool tagged);
	bool deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool tagged);

	//Vendor/Model specific hook functions
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
    virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,	int peak_burst_size=0);

 	bool hasVlanRateLimit(uint32 vlan, bool is_input);

};



#endif //ifndef _SwitchCtrl_Session_BrocadeNetIron_H_


