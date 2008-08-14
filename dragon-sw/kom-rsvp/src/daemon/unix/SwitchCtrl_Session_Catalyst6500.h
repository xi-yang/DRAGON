/****************************************************************************

Cisco (vendor) Catalyst 6500 (model) Control Module header file SwitchCtrl_Session_Catalyst6500.h
Created by Ajay Todimala, 2007
Modified by Xi Yang, 2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef SWITCHCTRL_SESSION_CATALYST6500_H_
#define SWITCHCTRL_SESSION_CATALYST6500_H_

#include "SNMP_Session.h"

#define CATALYST6500_MIN_VLAN_ID 1
#define CATALYST6500_MAX_VLAN_ID 4094
#define CATALYST6500_MIN_PORT_ID 0
#define CATALYST6500_MAX_PORT_ID 2048
#define CATALYST_VLAN_BITLEN		4096

class SwitchCtrl_Session_Catalyst6500: public SNMP_Session
{
	
public:
	SwitchCtrl_Session_Catalyst6500(): SNMP_Session() { rfc2674_compatible = false; snmp_enabled = true; activeVlanId = 0; }
	SwitchCtrl_Session_Catalyst6500(const RSVP_String& sName, const NetAddress& swAddr): SNMP_Session(sName, swAddr) 
		{ rfc2674_compatible = false; snmp_enabled = true; activeVlanId = 0; }
	virtual ~SwitchCtrl_Session_Catalyst6500() { }


	///////////------QoS Functions ------/////////
	//virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	//virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

	////////-----Vendor/Model specific hook functions------//////
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
        virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual void hook_getVlanMapFromSnmpVars(portVlanMap &pvm, netsnmp_variable_list *vars);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);

	virtual uint32 hook_convertPortInterfaceToID(uint32 id);
	virtual uint32 hook_convertPortIDToInterface(uint32 id);
	virtual bool hook_createPortToIDRefTable(portRefIDList &convList);
	virtual uint32 hook_convertVLANInterfaceToID(uint32 id) { return id; }
	virtual uint32 hook_convertVLANIDToInterface(uint32 id) { return id; }
	virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList);

	//////////// Functions that need implementation since Catalyst 6500 is Does not support RFC2674 ////////////
	virtual bool verifyVLAN(uint32 vlanID); 
	virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID);


	///////------Vendor/model specific functions--------///////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	virtual uint32 getActiveVlanId(uint32 port) { if (activeVlanId > 0) return activeVlanId; return getVLANbyUntaggedPort(port); }
	virtual bool readVlanPortMapListAllBranch(vlanPortMapList &vpmList);

	///////------Vendor/model specific functions for controlling port --------///////
	virtual bool isPortTurnedOn(uint32 port);
	virtual bool TurnOnPort(uint32 port, bool on);
	virtual bool isPortTrunking(uint32 port);
	virtual bool PortTrunkingOn(uint32 port);
	virtual bool PortTrunkingOff(uint32 port);
	virtual bool PortStaticAccessOn(uint32 port);
	virtual bool PortStaticAccessOff(uint32 port);
	virtual bool SwitchPortOnOff(uint32 port, bool on);
	virtual bool isSwitchport(uint32 port);

private:
	uint16 activeVlanId; 

	bool readTrunkPortVlanMap(portVlanMapList &);
	bool readVlanPortMapListALLFromPortVlanMapList(vlanPortMapList &, portVlanMapList &);

};


inline uint32 convertUnifiedPort2Catalyst6500(uint32 port)
{
	return (((port>>8)&0xf)*128 + (port&0xff));
	return port; // support up to 16 slots
}
inline uint32 convertCatalyst65002UnifiedPort(uint32 port)
{
	return (((((port-1)/128)&0xf)<<8) | ((port-1)%128 + 1));
	return port; // support up to 16 slots
}

#endif /*SWITCHCTRL_SESSION_CATALYST6500_H_*/
