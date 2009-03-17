/****************************************************************************

CLI Based Switch Control Module header file SwitchCtrl_Session_JuniperEX3200.h
Created by Xi Yang on 03/12/2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SwitchCtrl_Session_JuniperEX3200_H_
#define _SwitchCtrl_Session_JuniperEX3200_H_

#include "SwitchCtrl_Global.h"
#include "CLI_Session.h"


class SwitchCtrl_Session_JuniperEX3200: public CLI_Session
{

public:
	SwitchCtrl_Session_JuniperEX3200(): CLI_Session() { }	
	SwitchCtrl_Session_JuniperEX3200(const String& sName, const NetAddress& swAddr): CLI_Session(sName, swAddr) { }
	virtual ~SwitchCtrl_Session_JuniperEX3200() { }

	virtual bool connectSwitch();
	virtual void disconnectSwitch();

	virtual bool preAction();
	virtual bool postAction();

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) 
		{ return policeInputBandwidth_JUNOScript(do_undo, input_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size) ; }
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) { return true; }

	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID) { return true; }

	//Vendor/Model specific hook functions
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
       virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual bool hook_createPortToIDRefTable(portRefIDList &portRefIdConvList);
	virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);
	virtual uint32 hook_convertPortInterfaceToID(uint32 id);
	virtual uint32 hook_convertPortIDToInterface(uint32 id);
	virtual uint32 hook_convertVLANInterfaceToID(uint32 id);
	virtual uint32 hook_convertVLANIDToInterface(uint32 id);

	 //interface to the force10_hack module 
	virtual bool deleteVLANPort_JUNOScript(uint32 portID, uint32 vlanID, bool isTagged = false);
	virtual bool addVLANPort_JUNOScript(uint32 portID, uint32 vlanID, bool isTagged = false);

	///////////------QoS Functions ------/////////
	bool policeInputBandwidth_JUNOScript(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	//bool limitOutputBandwidth_JUNOScript(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

protected:
	char bufScript[LINELEN*3+1];

	uint32 convertUnifiedPort2JuniperEXBit(uint32 port)
	{
	    portRefIDList::Iterator it;
	    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
	    {
	        if ((*it).port_id == port)
	            return (*it).port_bit;
	    }
	    return 0;
	}

	uint32 convertJuniperEXBit2UnifiedPort(uint32 bit)
	{
	    portRefIDList::Iterator it;
	    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
	    {
	        if ((*it).port_bit == bit)
	            return (*it).port_id;
	    }
	    return 0;
	}
};

#endif //ifndef _SwitchCtrl_Session_JuniperEX3200_H_

