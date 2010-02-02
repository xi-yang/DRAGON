/****************************************************************************

UNI Based Switch/Subnet Control Module header file SwitchCtrl_Session_CienaCN4200.h
Created by Xi Yang @ 01/12/2010
To be incorporated into KOM-RSVP-TE package

.... Implemenation in this version is for Ethernet-over-OTN/TDM ...

****************************************************************************/

#ifndef _SwitchCtrl_Session_CienaCN4200_H_
#define _SwitchCtrl_Session_CienaCN4200_H_

#include "SwitchCtrl_Global.h"
#include "CLI_Session.h"
#include "RSVP_ProtocolObjects.h"

//CIENA OTNX definitions
#define MAX_OTNX_CHAN_NUM 256 //64: 10G with OPVCX; 40: WDM
#define CIENA_OTNX_OTU1		1
#define CIENA_OTNX_OTU2		2
#define CIENA_OTNX_OTU3		4
#define CIENA_OTNX_OPVC		8
#define CIENA_OTNX_WDM10G		16
#define CIENA_OTNX_WDM40G		32

typedef struct Ciena_OTNX_Data_struct {
	uint32		switch_ip;
	uint16		tl1_port;
	uint8		eth_edge; // 1 = true, 0 = false 
	union {
		uint8	reserved;
		uint8	otnx_if_id;
	};
	uint32		data_ipv4;
	uint32		logical_port_number;
	uint16 	channel_type;
	uint16 	num_chans; //number of sub-wavelength channels
	uint8 wave_opvc_bitmask[MAX_OTNX_CHAN_NUM/8]; // bit =1 means available
} OTNX_Data;

class SwitchCtrl_Session_CienaCN4200: public CLI_Session
{
public:
	//constructors/destuctors
	SwitchCtrl_Session_CienaCN4200(): CLI_Session() { internalInit(); }
	SwitchCtrl_Session_CienaCN4200(const String& sName, const NetAddress& swAddr): 
		CLI_Session(sName, swAddr) { internalInit(); }
	virtual ~SwitchCtrl_Session_CienaCN4200() {}

	void setLspName(const String& name) 
	{ 
		currentLspName = name; 
		//Replace colon, semicolon and comma with dash
		currentLspName.replacechar(';', '-');	currentLspName.replacechar(':', '-');	currentLspName.replacechar(',', '-');
	}
	bool hasSourceDestPortConflict() // Source and Dest ports should not be on the same ETTP
	{ 
		return ((otnxDataSrc.logical_port_number >> 16) == (otnxDataDest.logical_port_number>>16));
	}
	//Backward compatibility with general SwitchCtrl_Session operations
	virtual bool connectSwitch() 
	{
		if (otnxDataSrc.tl1_port > 0) 
			CLI_Session::setPort((int)otnxDataSrc.tl1_port);
		return CLI_Session::engage(); 
	}
	virtual void disconnectSwitch() 
	{
		char canc_user[50];
		sprintf (canc_user, "canc-user::%s:%d;", CLI_USERNAME, getNewCtag());
		CLI_Session::disengage(canc_user);
	}
	virtual bool refresh() { return true; } //NOP
	
	//Preparing OTNX parameters
	void setOTNXDataSrc(OTNX_Data& data);
	void setOTNXDataDest(OTNX_Data& data);
	OTNX_Data* getOTNXDataSrc() { return &otnxDataSrc; }
	OTNX_Data* getOTNXDataDest() { return &otnxDataDest; }
	void setEthernetBandwidth(float bw) { ethernetBw = bw; }
	float getEthernetBandwidth(float bw) { return ethernetBw; }
	bool isIngressNode() { return (otnxDataSrc.eth_edge == 1); }
	bool isEgressNode() { return (otnxDataDest.eth_edge == 1); }
	bool isResourceHeld() { return resourceHeld; }
	void setResourceHeld(bool hold) { resourceHeld = hold; }
	uint32 getOPVCX(bool isSrc)
	{
		if (ethernetBw < 0.001)
			return 0;
		return getOPVCXByBandwidth(isSrc?otnxDataSrc:otnxDataDest, ethernetBw);
	}

	//////////////// TL1 related functions >> begin  //////////////
	uint32 getNewCtag() { ++ctagNum; return ctagNum; }
	uint32 getCurrentCtag() { return ctagNum;}

	//////// ---- To be overriden for edge control ---- ////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID)  { return false; }
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID) { return false; }
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID) { return false; }
	virtual bool hook_createVLAN(const uint32 vlanID) { return false; }
	virtual bool hook_removeVLAN(const uint32 vlanID) { return false; }
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm) { return false; }
	virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars) { return; }
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port) { return false; }
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID) { return false; }

	////// ---- For monitoring service API
	virtual bool isMonSession(char* gri) { return (currentLspName == (const char*)gri); } // may only return false in EoS subnet control sessions
	virtual bool getMonSwitchInfo(MON_Reply_Subobject& monReply) {return false;} // to be implemented later
	virtual bool getMonCircuitInfo(MON_Reply_Subobject& monReply) {return false;} // to be implemented later

	// return *continguous* opvcx range in uint32 (lower_16-bit = range_start | higher_16-bit = range_end);
	static uint32 getOPVCXByBandwidth(OTNX_Data& otnxData, float bw)
	{
		uint8 ts, ts1, ts2, i;
		uint8 num_opvcx = (uint8)ceilf(bw/49.536);

		if (num_opvcx > 16)  //requiring OTU2
		{
			for (ts = 1; ts <= 64; ts++)
			{
				if (!HAS_TIMESLOT(otnxData.wave_opvc_bitmask, ts))
					return 0;
			}
			ts1 =1; ts2 = 64;
		}
		else //requiring single OTU1
		{
			for (i = 0; i < 4; i++) // 4 OTU1s
			{
				for (ts = i*16+1, ts1 = i*16+1; ts < (i+1)*16; ts++)
				{
					if (!HAS_TIMESLOT(otnxData.wave_opvc_bitmask, ts))
					{
						ts1 = ts+1;
					}
					if (HAS_TIMESLOT(otnxData.wave_opvc_bitmask, ts))
					{
						if (ts-ts1+1 >= num_opvcx) //unsatisfying blocks
						{
							ts2 = ts;
							goto _range_ok;
						}
					}
				}
			}
			return 0;
		}

	_range_ok:
		return (ts1 | (ts2 <<16));
	}

protected:
	uint32 ctagNum;
	String currentLspName;
	float ethernetBw;
	OTNX_Data otnxDataSrc;
	OTNX_Data otnxDataDest;
	bool resourceHeld;

private:	
	void internalInit ();
	void setOTNXData(OTNX_Data& data, uint32 switch_ip, uint16 tl1_port, uint8 eth_edge, uint8 otnx_if_id, uint32 data_if, 
		uint32 logical_port, uint16 channel_type, uint16 num_chans, uint8* bitmask);

	char bufCmd[LINELEN+1];
	char strCOMPLD[20];
	char strDENY[20];
};

#endif
