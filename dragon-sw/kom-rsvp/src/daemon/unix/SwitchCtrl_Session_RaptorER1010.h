/****************************************************************************

Raptor (vendor) ER1010 (model) Control Module header file SwitchCtrl_Session_RatporER1010.h
Created by Xi Yang @ 02/24/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SWITCHCTRL_SESSION_RAPTORER1010_H_
#define _SWITCHCTRL_SESSION_RAPTORER1010_H_

#include "SNMP_Session.h"

class SwitchCtrl_Session_RaptorER1010: public SNMP_Session
{
	
public:
	SwitchCtrl_Session_RaptorER1010(): SNMP_Session() { rfc2674_compatible = snmp_enabled = true; activeVlanId = 0;}
	SwitchCtrl_Session_RaptorER1010(const RSVP_String& sName, const NetAddress& swAddr): SNMP_Session(sName, swAddr) 
		{ rfc2674_compatible = snmp_enabled = true; activeVlanId = 0; }
	virtual ~SwitchCtrl_Session_RaptorER1010() { }

	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID);

	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	virtual uint32 getActiveVlanId(uint32 port) { if (activeVlanId > 0) return activeVlanId; return getVLANbyUntaggedPort(port); }

	///////////------QoS Functions ------/////////
	//virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	//virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

	////////-----Vendor/Model specific hook functions------//////
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
       virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);

	///////------Vendor/model specific functions--------///////

private:
	uint16 activeVlanId; 
};

#define RAPTOR_VLAN_BITLEN		48

inline uint32 convertUnifiedPort2RaptorInternal(uint32 port)
{
	return (((port>>8)&0xf)*12 + (port&0xff));
}
inline uint32 convertRaptorInternal2UnifiedPort(uint32 port)
{
	return (((((port-1)/12)&0xf)<<8) | ((port-1)%12 + 1));
}

#endif //ifndef _SwitchCtrl_Session_RaptorER1010_H_

