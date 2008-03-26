/****************************************************************************

SMC (vendor) 8 ports 10G 8708 (model) Control Module header file SwitchCtrl_Session_SMC10G8708.h
Created by John Qu @ 5/16/2007
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SWITCHCTRL_SESSION_SMC10G8708_H_
#define _SWITCHCTRL_SESSION_SMC10G8708_H_

#include "SNMP_Session.h"

#define SMC8708_DEFAULT_VLAN 2

//MAX bandwidth for SMC 8708 is 10000 bps
#define SMC8708_MAX_BANDWIDTH 10000
//MIN bandwidth set to 10M
#define SMC8708_MIN_BANDWIDTH 10

struct portRateMap{
    uint32 port;
    uint32 swIngressRate ; //default to 10M
    uint32 swEgressRate ; //default to 10M
    uint32 dragonIngressRate ;
    uint32 dragonEgressRate;
};
typedef SimpleList<portRateMap> portRateMapList;

class SwitchCtrl_Session_SMC10G8708: public SNMP_Session
{
	
public:
	SwitchCtrl_Session_SMC10G8708(): SNMP_Session() { rfc2674_compatible = snmp_enabled = true;portRateMapListLoaded = false;  }
	SwitchCtrl_Session_SMC10G8708(const RSVP_String& sName, const NetAddress& swAddr): SNMP_Session(sName, swAddr) 
		{ rfc2674_compatible = snmp_enabled = true;
		  portRateMapListLoaded = false; }
	virtual ~SwitchCtrl_Session_SMC10G8708() { }

	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	
	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

	////////-----Vendor/Model specific hook functions------//////
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID) ;
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
       virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);
	virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList);

	///////------Vendor/model specific functions--------///////
///////------Vendor/model specific functions--------///////
	bool setVLANPortTag(uint32 portListNew, uint32 vlanID);
	bool setVLANPort(uint32 portListNew, uint32 vlanID);
	bool setVLANPortTag(uint8 * portbits, int bitlen, uint32 vlanID);	
	bool setVLANPort(uint8 * portbits, int bitlen, uint32 vlanID);	
	//  Dell 5324 switch ,,, SMC ...
	bool setVLANPVID(uint32 port, uint32 vlanID); 
	// RFC2674 (DELL and Extreme), SMC...
	bool movePortToDefaultVLAN(uint32 port); 
	
protected:
	//
	// ---------------- SMC 8708 port rate_limit configuration ----------//
	//  set SMC 8708 port ingress rate_limit
	bool setPortIngressBandwidth(uint32 input_port, uint32 vlan_id, uint32 committed_rate);
	// enable ( 1) or disable (2) on port ingress rate_limit
	bool setPortIngressRateLimitFlag(uint32 input_port, uint32 flag);
	// set SMC 8708 port egress rate_limit
	bool setPortEgressBandwidth(uint32 input_port, uint32 vlan_id, uint32 committed_rate);
	// enable ( 1) or disable (2) on port egress rate_limit
	bool setPortEgressRateLimitFlag(uint32 input_port, uint32 flag);
	
	//retrieve port rate information from switch
	void readPortIngressRateListFromSwitch();
	void readPortEgressRateListFromSwitch();
	
	//port rate list for bandwidth management
	portRateMapList portRateList;
	
	//--------------------------help methods -----------------------------//
	// check if  portRateList contains portRateMap for a port.
	bool hasPortRateMap(uint32 port);
	// add portRateMap to portRateList list
	void addPortRateMapToList(portRateMap& prm);
	portRateMap* getPortRateMap(uint32 port);
	
private:
	
	void getPortIngressRateFromSnmpVars(netsnmp_variable_list *vars);
	void getPortEgressRateFromSnmpVars(netsnmp_variable_list *vars);
	
	uint32 getPortIngressRateFromSwitch(uint32 port);
	uint32 getPortEgressRateFromSwitch(uint32 port);
	
	//debug 
	void dumpPortRateList();
	void dumpPRM(portRateMap& prm);
	
	bool portRateMapListLoaded ;
	inline bool isPortRateMapLoaded(){ return portRateMapListLoaded;}
	inline void setPortRateMapLoaded(bool flag){ portRateMapListLoaded = flag;}
};



#endif //ifndef _SWITCHCTRL_SESSION_SMC10G8708_H_

