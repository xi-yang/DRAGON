/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  Contact:	Martin Karsten
		TU Darmstadt, FG KOM
		Merckstr. 25
		64283 Darmstadt
		Germany
		Martin.Karsten@KOM.tu-darmstadt.de

  Other copyrights might apply to parts of this package and are so
  noted when applicable. Please see file COPYRIGHT.other for details.

****************************************************************************/
#ifndef _RSVP_ConfigFileReader_h_
#define _RSVP_ConfigFileReader_h_ 1

#include <iostream>
#include <fstream>
#include "RSVP_Lists.h"
#include "RSVP_String.h"
#include "RSVP_TimeValue.h"
#include "SwitchCtrl_Global.h"
#include "NARB_APIClient.h"

class RSVP;
class TrafficControl;
class LogicalInterface;

class ConfigFileReader {
	RSVP& rsvp;
	LogicalInterfaceList& tmpLifList;
public:
	struct Hop {
		NetAddress iface;
		NetAddress addr;
		uint32 hopCount;
	};
	SimpleList<Hop> hopList;
	bool mpls_default;
	String interfaceName;
	String localId;
	NetAddress localAddress, remoteAddress, virtAddress;
	uint16 virtMTU, localPort;
	ieee32float bandwidth, lossProb;
	uint32 refreshRate, latency, rapidRefreshRate;
	bool disable, encap, virt, refresh, mpls;
	NetAddress dest, mask, gateway;
	PortList remotePortList;
	SimpleList<NetAddress> explicitRouteHops;
	TrafficControl* tc;
public:
	ConfigFileReader( RSVP& rsvp, LogicalInterfaceList& tmpLifList )
		: rsvp(rsvp), tmpLifList(tmpLifList), mpls_default(true) { cleanup(); }
	bool parseConfigFile( const String& );
	void setSessionHash( uint32 );
	void setApiHash( uint32 );
	void setIdHashSend( uint32 );
	void setIdHashRecv( uint32 );
	void setListAlloc( uint32 );
	void setSB_Alloc( uint32 );
	void setTimerTotal( uint32 );
	void setTimerSlots( uint32 );
	void setGlobalMPLS( bool );
	void setLabelHash( uint32 );
	void createInterface();
	void createTC_NONE();
	void createTC_CBQ();
	void createTC_HFSC();
	void createTC_Rate();
	void createRoute();
	void addExplicitRouteHop( const NetAddress& );
	void setExplicitRoute();
	void addHop( uint32 = 0 );
	void setApiPort( uint16 );
	void cleanup();
	void warn( const String& );
	const SimpleList<Hop>& getHopList() { return hopList; }
	void setNarbApiClient(String host, int port);
	void addSlot(String slot_type, uint16 slot_num);
};

#endif /* _RSVP_ConfigFileReader_h_ */
