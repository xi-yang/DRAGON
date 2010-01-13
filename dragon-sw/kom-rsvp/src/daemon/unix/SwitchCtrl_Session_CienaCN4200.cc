/****************************************************************************

UNI Based Switch/Subnet Control Module source file SwitchCtrl_Session_CienaCN4200.cc
Created by Xi Yang @ 01/12/2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_CienaCN4200.h"
#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_RoutingService.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_SignalHandling.h"
#include "RSVP_PSB.h"
#include "RSVP_Log.h"

void SwitchCtrl_Session_CienaCN4200::internalInit ()
{
	active = false;
	snmp_enabled = false; 
	rfc2674_compatible = false; 
	snmpSessionHandle = NULL; 
	ctagNum = 4200;
	ethernetBw = 0;
}

void SwitchCtrl_Session_CienaCN4200::setOTNXData(OTNX_Data& data, uint32 switch_ip, uint16 tl1_port, uint8 eth_edge, uint8 otnx_if_id, uint32 data_if, 
		uint32 logical_port, uint16 num_chans, uint32 wave_id, uint8* bitmask)
{
	memset(&data, 0, sizeof(data));
	data.switch_ip = switch_ip;
	data.tl1_port = tl1_port;
	data.eth_edge = eth_edge;
	data.otnx_if_id = otnx_if_id;
	data.data_ipv4 = data_if;
	data.logical_port_number = logical_port;
	data.num_waves = 1;
	data.num_chans = num_chans;
	data.wave_opvc_map[0].wave_id = wave_id;
	memcpy(data.wave_opvc_map[0].opvc_bitmask, bitmask, num_chans/8);
}

void SwitchCtrl_Session_CienaCN4200::setOTNXDataSrc(OTNX_Data& data)
{
	setOTNXData(otnxDataSrc, data.switch_ip, data.tl1_port, data.eth_edge, data.otnx_if_id, data.data_ipv4, data.logical_port_number, 
		data.num_chans, data.wave_opvc_map[0].wave_id, data.wave_opvc_map[0].opvc_bitmask);
}

void SwitchCtrl_Session_CienaCN4200::setOTNXDataDest(OTNX_Data& data)
{
	setOTNXData(otnxDataDest, data.switch_ip, data.tl1_port, data.eth_edge, data.otnx_if_id, data.data_ipv4, data.logical_port_number, 
		data.num_chans, data.wave_opvc_map[0].wave_id, data.wave_opvc_map[0].opvc_bitmask);
}


