/****************************************************************************

Dell (vendor) PowerConnect 6024/6224/6248 (model) Control Module header file SwitchCtrl_Session_PowerConnect6000.h
Created by  Xi Yang, 2009
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef SWITCHCTRL_SESSION_POWERCONNECT6000_H_
#define SWITCHCTRL_SESSION_POWERCONNECT6000_H_

#include "SNMP_Session.h"
#include "CLI_Session.h"

#ifndef DELL_ERROR_PROMPT 
#define DELL_ERROR_PROMPT "% "
#endif

class SwitchCtrl_Session_PowerConnect6000_CLI: public CLI_Session
{
public:
	SwitchCtrl_Session_PowerConnect6000_CLI(): CLI_Session() { }	
	SwitchCtrl_Session_PowerConnect6000_CLI(const String& sName, const NetAddress& swAddr): CLI_Session(sName, swAddr) { }
	virtual ~SwitchCtrl_Session_PowerConnect6000_CLI() { }

	virtual bool preAction();
	virtual bool postAction();

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

	// Dell PowerConnect specific
	bool postConnectSwitch();

	// Vendor/Model specific hook functions --> not used (for compile only)
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID)  { return false;}
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID)  { return false;}
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID)  { return false;}
	virtual bool hook_createVLAN(const uint32 vlanID) { return false;}
	virtual bool hook_removeVLAN(const uint32 vlanID) { return false;}
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm) { return false;}
       virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars) { }
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port) { return false;}
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID) { return false;}
	friend class SwitchCtrl_Session_PowerConnect6000;
};

class SwitchCtrl_Session_PowerConnect6000: public SNMP_Session
{
public:
	SwitchCtrl_Session_PowerConnect6000(): SNMP_Session() { rfc2674_compatible = true; snmp_enabled = true;}
	SwitchCtrl_Session_PowerConnect6000(const RSVP_String& sName, const NetAddress& swAddr): SNMP_Session(sName, swAddr), cliSession(sName, swAddr) 
		{ rfc2674_compatible = true; snmp_enabled = true; }
	virtual ~SwitchCtrl_Session_PowerConnect6000() { this->disconnectSwitch(); }

	virtual bool refresh() { return cliSession.refresh(); }
	virtual bool connectSwitch();
	virtual void disconnectSwitch();

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

private:

	SwitchCtrl_Session_PowerConnect6000_CLI cliSession;
};


#endif /*SWITCHCTRL_SESSION_POWERCONNECT6000_H_*/

