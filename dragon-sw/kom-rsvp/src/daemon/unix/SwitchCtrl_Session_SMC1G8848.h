/****************************************************************************

SMC (vendor) 48 ports 1G 8848 (model) Control Module header file SwitchCtrl_Session_SMC1G8848.h
Created by John Qu @ 1/21/2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SWITCHCTRL_SESSION_SMC1G8848_H_
#define _SWITCHCTRL_SESSION_SMC1G8848_H_

#include "SNMP_Session.h"
#include "SwitchCtrl_Session_SMC10G8708.h"

//#ifdef SMC1G8848_SWITCH
	//#define SMC8848_DEFAULT_VLAN 4093
//#else
#define SMC8848_DEFAULT_VLAN 1
//#endif


//MAX bandwidth for SMC 8848 is 1G
#define SMC8848_MAX_BANDWIDTH 1000
//MIN bandwidth set to 10M
#define SMC8848_MIN_BANDWIDTH 10


typedef SimpleList<portRateMap> portRateMapList;

class SwitchCtrl_Session_SMC1G8848: public SNMP_Session
{
	
public:
	SwitchCtrl_Session_SMC1G8848(): SNMP_Session() { rfc2674_compatible = snmp_enabled = true;portRateMapListLoaded = false;  }
	SwitchCtrl_Session_SMC1G8848(const RSVP_String& sName, const NetAddress& swAddr): SNMP_Session(sName, swAddr) 
		{ rfc2674_compatible = snmp_enabled = true;
		  portRateMapListLoaded = false; }
	virtual ~SwitchCtrl_Session_SMC1G8848() { }

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
	//virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList);

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
	void dumpPortBits(const vlanPortMap &vpm);
	
	bool portRateMapListLoaded ;
	inline bool isPortRateMapLoaded(){ return portRateMapListLoaded;}
	inline void setPortRateMapLoaded(bool flag){ portRateMapListLoaded = flag;}



	#define SMC_VLAN_BITLEN		480  // total 60 bytes

	inline uint32 convertUnifiedPort2SMCInternal(uint32 port)
	{
		// each switch unit port bit len is 7 bytes, i.e. 7*8=56 bits
		//uint32 portNum,
		port =   (((port>>8)&0xf)*56 + (port&0xff));
		// to use 1 based cout for switch unit in local ID
		// 0/0/48 --> port 48 on switch unit 1
		// 0/1/48 --> port 48 on switch unit 1
		// 0/2/48 --> port 48 on switch unit 2.
		if(port > 56)
			port -= 56;
		return port;	
	}
	inline uint32 convertSMCInternal2UnifiedPort(uint32 port)
	{
		// to use 1 based numbering for switch unit in local ID to be consistent with CLI
		port += 56;
		return (((((port-1)/56)&0xf)<<8) | ((port-1)%56 + 1));
	}

};

#endif //ifndef _SWITCHCTRL_SESSION_SMC1G8848_H_

