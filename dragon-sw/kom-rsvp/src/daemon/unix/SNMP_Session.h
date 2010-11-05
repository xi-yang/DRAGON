/****************************************************************************

SNMP Based Switch Control Module header file SNMP_Session.h
Created by Xi Yang @ 01/17/2006
Extended from SNMP_Global.h by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SNMP_SESSION_H_
#define _SNMP_SESSION_H_

#include "SwitchCtrl_Global.h"

class SNMP_Session: public SwitchCtrl_Session
{
	
public:
	SNMP_Session(): SwitchCtrl_Session() { untaggedPortBit_reverse = false; }
	SNMP_Session(const RSVP_String& sName, const NetAddress& swAddr): SwitchCtrl_Session(sName, swAddr) { untaggedPortBit_reverse = false;  }
	virtual ~SNMP_Session() { }

	virtual bool SNMPSet(char*, char, char*);
	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
    virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID);

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) { return false; }
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) { return false; }

	////////-----Vendor/Model specific hook functions------//////
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
       virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);

	///////------Vendor/model specific functions--------///////
	bool setVLANPortTag(uint8 * portbits, uint32 vlanID);
	bool setVLANPort(uint8 * portbits, uint32 vlanID);
	bool setVLANPortTag(uint8 * portbits, int bitlen, uint32 vlanID);	
	bool setVLANPort(uint8 * portbits, int bitlen, uint32 vlanID);	
	//  Dell 5324 switch only
	bool setVLANPVID(uint32 port, uint32 vlanID); 
	// RFC2674 (DELL and Extreme)
	bool movePortToDefaultVLAN(uint32 port); 

	// when set true, bit=0 means *set* to indicate the port is untagged
	void setUntaggedPortBitReverse(bool b) { untaggedPortBit_reverse = b; } 

protected:
	bool untaggedPortBit_reverse;
};

#endif //ifndef _SNMP_SESSION_H_
