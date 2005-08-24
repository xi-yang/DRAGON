/****************************************************************************

SNMP Management header file SNMP.h 
Created by Aihua Guo @ 04/10/2004 
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#ifndef _SNMP_Global_h_
#define _SNMP_Global_h_ 1

#include "RSVP_Lists.h"
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/session_api.h>

#define SWITCH_CTRL_PORT	1
#define MIN_VLAN			2
#define MAX_VLAN	   		9
#define MAX_VENDOR			2

class SNMP_Session{
	String sessionName;
	NetAddress switchInetAddr;
	uint32 portList[MAX_VLAN+1];
	struct snmp_session* sessionHandle;	
	bool active;
	uint32 vendor;
	String supportedVendorOidString[MAX_VENDOR+1];
	bool setVLANPort(uint32 portListNew, uint32 vlanID);
	void setSupportedVendorOidString() {
		supportedVendorOidString[IntelES530] = ".1.3.6.1.4.1.343.6.63.3.8.2.3.1.3";
		supportedVendorOidString[RFC2674] = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
	}

protected:
	enum SupportedVendor{
			Illegal = 0,
			IntelES530 = 1, //Intel ES-530 Express Fast Ethernet Switch
			RFC2674 = 2,	// Dell 5200/5300 Series GigE Switch
	};

public:
	SNMP_Session():sessionName(), switchInetAddr(0), sessionHandle(NULL), active(false), vendor(Illegal){
		setSupportedVendorOidString();
	}
	SNMP_Session(const String& sName, const NetAddress& swAddr)
		:sessionName(sName), switchInetAddr(swAddr), sessionHandle(NULL) , active(false), vendor(Illegal){
		for (uint32 vlan=0; vlan<=MAX_VLAN; vlan++) 
			portList[vlan]=0;
		setSupportedVendorOidString();
	}
	~SNMP_Session() {disconnectSwitch();}
	bool operator==( const SNMP_Session& s ) const {
		return (active == s.active && sessionName == s.sessionName && switchInetAddr == s.switchInetAddr && vendor == s.vendor);
	}
	SNMP_Session& operator=(const SNMP_Session& s) { 
		sessionName = s.sessionName;
		switchInetAddr = s.switchInetAddr;
		for (uint32 i = 0; i <= MAX_VLAN; i++)
			portList[i] = s.portList[i];
		sessionHandle = s.sessionHandle; 
		active = s.active;
		vendor = s.vendor;
		return *this;
	}
	bool connectSwitch();
	void disconnectSwitch() { 
		snmp_close(sessionHandle);
		active = false;
		vendor = Illegal;
	}
	bool setSwitchVendorInfo();
	const uint32 findEmptyVLAN() const{
		for (uint32 vlan=MIN_VLAN; vlan<=MAX_VLAN; vlan++)
			if (!portList[vlan]) return vlan;
		return 0;
	}
	bool setVLANPVID(uint32 port, uint32 vlanID); // A hack to Dell 5324 switch
	bool setVLANPortTag(uint32 portListNew, uint32 vlanID); // A hack to Dell 5324 switch
	bool movePortToVLAN(uint32 port, uint32 vlanID);
	bool movePortToDefaultVLAN(uint32 port);
	uint32 getVLANbyPort(uint32 port){
		for(int i=MIN_VLAN;i<=MAX_VLAN;i++){
			if(portList[i]&(1<<(32-port)))
			      return i;
		}
		return 0;
	}
	bool readVLANFromSwitch();
	String& getSessionName() {return sessionName;}
	NetAddress& getSwitchInetAddr() {return switchInetAddr;}
	bool isValidSession() const {return active;}
};

typedef SimpleList<SNMP_Session> SNMPSessionList;
class SNMP_Global{
	SNMPSessionList snmpSessionList;

public:
	SNMP_Global() { init_snmp("VLSRCtrl");} 
	~SNMP_Global();
	bool addSNMPSession(SNMP_Session& addSS);
	SNMPSessionList& getSNMPSessionList() { return snmpSessionList; }
};

#endif /* _SNMP_Global_h_ */
