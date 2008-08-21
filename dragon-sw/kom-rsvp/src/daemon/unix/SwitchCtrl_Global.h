/****************************************************************************

Switch Control Module header file SwitchCtrl_Global.h
Created by Xi Yang @ 01/12/2006
Extended from SNMP_Global.h by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _SWITCHCTRL_GLOBAL_H_
#define _SWITCHCTRL_GLOBAL_H_

#include "RSVP_Lists.h"
#include "RSVP_TimeValue.h"
#include "RSVP_BaseTimer.h"
#include "RSVP_IntServComponents.h"
#include "RSVP_ProtocolObjects.h"
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/session_api.h>
#include "NARB_APIClient.h"

/****************************************************************************

Notes:
1. SWITCH_CTRL_PORT used in the module is defined in RSVP_Config.h. It indicates which port
    is used for control/managenment and therefore should be excluded from data plane ops.
2. If FORCE10_SOFTWARE_V6 is defined in RSVP_Config.h, we shall be compatible with 
    Force10 app. software version 6-2-1-*. In future, software version will be detected at run 
    time in addition to the switch vendor/model type defined by compilation option.
3. Vendor == Vendor + Model
4. By default SwitchCtrl_Session support all RFC2674 functions using SNMP GET.

****************************************************************************/


#define MIN_VLAN			2
#define MAX_VLAN	   		4095
#define MAX_VENDOR			20
#define MAX_VLAN_BYTES			512
#define MAX_VENDOR_NAME			128

#ifdef FORCE10_SOFTWARE_V6
    #define MAX_VLAN_PORT_BYTES 96  // FTOS-ED-6.2.1
#else
    #define MAX_VLAN_PORT_BYTES 256 // FTOS-ED-5.3.1 (Cisco Catalyst also supports 2k ports)
#endif

enum SupportedVendor{
	AutoDetect = 0,
	IntelES530 = 1, //Intel ES-530 Express Fast Ethernet Switch
	RFC2674 = 2,	// Dell 5200/5300 Series GigE Switch
	LambdaOptical = 3,
	Force10E600 = 4,
	RaptorER1010 = 5,
	LinuxSwitch = 6,
	Catalyst3750 = 7,
	Catalyst6500 = 8,
	HP5406 = 9,
	SMC10G8708 = 10, //SMC 10G 8708 switch
	CienaSubnet = 100,
	Illegal = 0xffff,
};

struct vlanPortMap{
    uint32 vid;
    union {
        uint32 ports;
        uint8 portbits[MAX_VLAN_PORT_BYTES];
    };
};
typedef SimpleList<vlanPortMap> vlanPortMapList;

struct vlanRefID{
    uint32 ref_id;
    uint32 vlan_id;
};
typedef SimpleList<vlanRefID> vlanRefIDList;

// Catalyst switches store portVlanMap info in inversed format
// vlanPortMap structure is used extract the map info and convert to portVlanMap format
struct portVlanMap{
    uint32 pid;
    union {
        uint32 vlans;
        uint8 vlanbits[MAX_VLAN_BYTES];
    };
};
typedef SimpleList<portVlanMap> portVlanMapList;

// The mapping of port id and port ref id needed for cisco catalyst switches
struct portRefID{
    uint32 ref_id;
    uint32 port_id;
};
typedef SimpleList<portRefID> portRefIDList;

typedef SimpleList<uint32> PortList;

struct LocalId {
       uint16 type;
       uint16 value;
       SimpleList<uint16>* group;
};
typedef SimpleList<LocalId> LocalIdList;

#define LOCAL_ID_TYPE_NONE (uint16)0x0
#define LOCAL_ID_TYPE_PORT (uint16)0x1
#define LOCAL_ID_TYPE_GROUP (uint16)0x2
#define LOCAL_ID_TYPE_TAGGED_GROUP (uint16)0x3
#define LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL (uint16)0x4
#define LOCAL_ID_TYPE_SUBNET_IF_ID (uint16)0x5
#define LOCAL_ID_TYPE_SUBNET_UNI_SRC (uint16)0x10 	//Source (sender)
#define LOCAL_ID_TYPE_SUBNET_UNI_DEST (uint16)0x11	//Destination (Recv)

#define SET_LOCALID_REFRESH(X) X.type |= 0x80
#define RESET_LOCALID_REFRESH(X) X.type &= (~0x80)
#define IS_LOCALID_REFRESHED(X) ((X.type & 0x80) != 0)

class SwitchCtrl_Session{
public:
	SwitchCtrl_Session():sessionName(), switchInetAddr(0), snmpSessionHandle(NULL) {
		active = false;
		rfc2674_compatible = true;
		snmp_enabled = true;
		vlanCreation_enabled = true;
		vendor = Illegal;
		setSupportedVendorOidString();
	}
	SwitchCtrl_Session(const String& sName, const NetAddress& swAddr)
		:sessionName(sName), switchInetAddr(swAddr), snmpSessionHandle(NULL) , active(false), vendor(Illegal){
		active = false;
		rfc2674_compatible = true;
		snmp_enabled = true;
		vlanCreation_enabled = true;
		vendor = Illegal;
		setSupportedVendorOidString();
	}
	virtual ~SwitchCtrl_Session() {disconnectSwitch();}
	bool operator==( const SwitchCtrl_Session& s ) const {
		return (active == s.active && sessionName == s.sessionName && switchInetAddr == s.switchInetAddr && vendor == s.vendor);
	}
	SwitchCtrl_Session& operator=(const SwitchCtrl_Session& s) { 
		sessionName = s.sessionName;
		switchInetAddr = s.switchInetAddr;
		snmpSessionHandle = s.snmpSessionHandle; 
		active = s.active;
		rfc2674_compatible = s.rfc2674_compatible;
		vendor = s.vendor;
		return *this;
	}

	bool isRFC2674Compatible() { return rfc2674_compatible; }
	void setRFC2674Compatible(bool b) { rfc2674_compatible = b; }

	bool snmpEnabled() { return snmp_enabled; }
	void enableSNMP(bool b) { snmp_enabled = b; }
	void enableVLANCreation(bool b) {vlanCreation_enabled = b;}

	uint32 getVendor() { return vendor;}
	String& getSessionName() {return sessionName;}
	NetAddress& getSwitchInetAddr() {return switchInetAddr;}
	bool isValidSession() const {return active;}
	bool hasVLSRouteConflictonSwitch(struct _vlsr_route_& vlsr);

	//VTAG mutral-exclusion feature --> Review
	//bool resetVtagBitMask(uint8* bitmask); //reset bits corresponding to existing vlans

	virtual bool connectSwitch();
	virtual void disconnectSwitch();
	virtual bool getSwitchVendorInfo();
	virtual bool refresh() { return true; }

	//////////// RFC2674 compatible functions that use SNMP GET////////////
	virtual bool isVLANEmpty(const uint32 vlanID); // RFC2674
	virtual const uint32 findEmptyVLAN(); // RFC2674
	virtual uint32 getVLANbyPort(uint32 port); // RFC2674
	virtual uint32 getVLANListbyPort(uint32 port, SimpleList<uint32> &vlan_list); // RFC2674
	virtual uint32 getVLANbyUntaggedPort(uint32 port); // RFC2674
	virtual void readVlanPortMapBranch(const char* oid_str, vlanPortMapList &vpmList); // RFC2674
	virtual bool readVLANFromSwitch(); // RFC2674
	virtual bool verifyVLAN(uint32 vlanID);// RFC2674
	virtual bool VLANHasTaggedPort(uint32 vlanID);// RFC2674
	virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID);// RFC2674

	////////// Below are vendor specific functions/////////
	virtual bool createVLAN(uint32 &vlanID);
	virtual bool removeVLAN(const uint32 vlanID);
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID) = 0;
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID) = 0;
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID) = 0;
	virtual uint32 getActiveVlanId(uint32 port) { return getVLANbyUntaggedPort(port); }
	virtual bool adjustVLANbyLocalId(uint32 vlanID, uint32 lclID, uint32 trunkPort);
	virtual bool readVlanPortMapListAllBranch(vlanPortMapList &vpmList) { return true; }

	bool movePortToDefaultVLAN(uint32 port)	{ // RFC2674
	    if ((!active) || port==SWITCH_CTRL_PORT)
	        return false;
	    uint32 vlanID = getVLANbyUntaggedPort(port);
	    return removePortFromVLAN(port, vlanID);
	}

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) { return false; }
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) { return false; }

	//Vendor/Model specific hook functions
	virtual bool hook_createVLAN(const uint32 vlanID) = 0;
	virtual bool hook_removeVLAN(const uint32 vlanID) = 0;
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm) = 0;
	virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars) = 0;
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port) = 0;
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID) = 0;
	virtual uint32 hook_convertVLANInterfaceToID(uint32 id) { return id; }
	virtual uint32 hook_convertVLANIDToInterface(uint32 id) { return id; }
	virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList) { return true; }

	//Cisco Catalyst need conversion between Port ID and port reference ID  
	virtual uint32 hook_convertPortInterfaceToID(uint32 id) { return id; }
	virtual uint32 hook_convertPortIDToInterface(uint32 id) { return id; }
	virtual bool hook_createPortToIDRefTable(portRefIDList &convList) { return true; }

	//Monitoring service specific
	virtual bool isMonSession(char* gri) { return true; } // may only return false in EoS subnet control sessions
	virtual bool getMonSwitchInfo(MON_Reply_Subobject& monReply);
	virtual bool getMonCircuitInfo(MON_Reply_Subobject& monReply) { return false; } // not to be called in Ethernet switch control session

protected:
	String sessionName;
	NetAddress switchInetAddr;
	struct snmp_session* snmpSessionHandle;  //snmp_session is defined in net-snmp package 
	bool active;	// Indicator: the session is active
	bool rfc2674_compatible;	// Flag indicating whether the VLSR/switch is SNMP MIB-QBridge compatible
	bool snmp_enabled;
	bool vlanCreation_enabled;
	uint32 vendor;	//vendor/model ID

	String venderSystemDescription;
	String supportedVendorOidString[MAX_VENDOR+1];

	vlanPortMapList vlanPortMapListAll;	// List of VLANs with a map of contained untagged and tagged ports
	vlanPortMapList vlanPortMapListUntagged; 	// List of VLANs with a map of contained untagged ports
	vlanRefIDList vlanRefIdConvList;	// Mapping table btwn vendor's private VLAN interface ID and regular VLAN ID.
	portRefIDList portRefIdConvList;		// Mapping table btwn vendor's private Port interface ID and regular Port ID.

       //Add ports in the port mask portListNew into VLAN.
       bool setVLANPort(uint32 portListNew, uint32 vlanID);

	void setSupportedVendorOidString() {
		supportedVendorOidString[IntelES530] = ".1.3.6.1.4.1.343.6.63.3.8.2.3.1.3";
		supportedVendorOidString[RFC2674] = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
		supportedVendorOidString[LambdaOptical] = "1.3.6.1.4.1.7156.1.4.1.1";
		supportedVendorOidString[RaptorER1010] = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
		supportedVendorOidString[HP5406] = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
		supportedVendorOidString[SMC10G8708] = ".1.3.6.1.2.1.17.7.1.4.3.1.2";
	}
};


typedef SimpleList<SwitchCtrl_Session*> SwitchCtrlSessionList;

/*manual configured switch slot/card types*/
#define		SLOT_TYPE_ILLEGAL 0
#define		SLOT_TYPE_GIGE 1
#define		SLOT_TYPE_TENGIGE 2
struct slot_entry {
	uint16 slot_type;
	uint16 slot_num;
};

/*manual configured exclusion of certain switching layers from routing computation based on session_name matching*/
#define		SW_EXCL_L_1 0x0010
#define		SW_EXCL_TDM 0x0020
#define		SW_EXCL_L_2 0x0040
#define		SW_EXCL_L_3 0x0080
struct sw_layer_excl_name_entry {
	uint32 sw_layer;
	char excl_name[16]; 
};

struct eos_map_entry {
	float bandwidth;
	SONET_TSpec* sonet_tspec; 
};

class sessionsRefreshTimer;
class SwitchCtrl_Global{
public:
	~SwitchCtrl_Global();

	static SwitchCtrl_Global& instance();

	SwitchCtrl_Session* createSession(NetAddress& swAddr);
	SwitchCtrl_Session* createSession(uint32 vendor_model, NetAddress& swAddr);

	void readPreservedLocalIds();
	bool addSession(SwitchCtrl_Session* addSS);
	void removeSession(SwitchCtrl_Session* addSS);
	SwitchCtrlSessionList& getSessionList() { return sessionList; }
	bool refreshSessions();
	void startRefreshTimer();

	/*called by object functions  in SwitchCtrl_Session*/
	static bool static_connectSwitch(struct snmp_session* &session, NetAddress &switchAddress);
	static void static_disconnectSwitch(struct snmp_session* &session);
	static bool static_getSwitchVendorInfo(struct snmp_session* &session, uint32 &vendor_id, String &vendorDesc);

	/*interact with dragond*/
        static LocalIdList localIdList;
        static void addLocalId(uint16 type, uint16 value, uint16  tag = ANY_VTAG);
        static void deleteLocalId(uint16 type, uint16 value, uint16  tag = ANY_VTAG);
        static void refreshLocalId(uint16 type, uint16 value, uint16  tag = ANY_VTAG);
        static bool hasLocalId(uint16 type, uint16 value, uint16  tag = ANY_VTAG);
        static void clearLocalIdList( ) { localIdList.clear(); }
        static void processLocalIdMessage(uint8 msgType, LocalId& lid);
        static void getPortsByLocalId(SimpleList<uint32>&portList, uint32 port);


	/*RSVPD.conf*/
	void addSlotEntry(slot_entry &se) { slotList.push_back(se); }
	uint16 getSlotType(uint16 slot_num);
	void addExclEntry(sw_layer_excl_name_entry &ee) { exclList.push_back(ee); }
	uint32 getExclEntry(String session_name);
	SONET_TSpec* addEosMapEntry(float bandwidth, String& spe, int ncc);
	SONET_TSpec* getEosMapEntry(float bandwidth);

	/*monitoring*/
	void getMonitoringInfo(MON_Query_Subobject& monQuery, MON_Reply_Subobject& monReplym, uint32 destIp = 0);

protected:
	SwitchCtrl_Global();
	SwitchCtrl_Global(const SwitchCtrl_Global& obj);

private:
	sessionsRefreshTimer *sessionsRefresher;
	SwitchCtrlSessionList sessionList;
	SimpleList<slot_entry> slotList;
	SimpleList<sw_layer_excl_name_entry> exclList;
	SimpleList<eos_map_entry> eosMapList;
};

class sessionsRefreshTimer: public BaseTimer {
public:
	sessionsRefreshTimer(SwitchCtrl_Global* sc, const TimeValue& refreshTime): BaseTimer(refreshTime), switchController(sc), period(refreshTime) {}
	virtual void internalFire() {
		cancel();
		alarmTime += period;
		start();
		switchController->refreshSessions();
	}
	void Start() { start(); }
private:
	SwitchCtrl_Global *switchController;
	TimeValue period;
};

inline vlanPortMap *getVlanPortMapById(vlanPortMapList &vpmList, uint32 vid)
{
    vlanPortMapList::Iterator iter;
    for (iter = vpmList.begin(); iter != vpmList.end(); ++iter)
        if (((*iter).vid) == vid)
            return &(*iter);
    return NULL;
}

inline bool setVlanPortMapById(vlanPortMapList &vpmList, uint32 vid, uint8* vpmPortbits)
{
    vlanPortMapList::Iterator iter;
    for (iter = vpmList.begin(); iter != vpmList.end(); ++iter)
        if (((*iter).vid) == vid) {
	    for (int i=0; i<MAX_VLAN_PORT_BYTES; i++)
		(*iter).portbits[i] = vpmPortbits[i];
	    return true;
	}
    return false;
}

inline void SetPortBit(uint8* bitstring, uint32 bit)
{
    uint8 mask = (1 << (7 - bit%8))&0xff;
    bitstring[bit/8] |= mask;
}

inline void SetBit(uint8* bitstring, uint32 bit)
{
    uint8 mask = (1 << (7 - bit%8))&0xff;
    bitstring[bit/8] |= mask;
}

inline void ResetPortBit(uint8* bitstring, uint32 bit)
{
    uint8 mask = (~(1 << (7 - bit%8)))&0xff;
    bitstring[bit/8] &= mask;
}

inline void ResetBit(uint8* bitstring, uint32 bit)
{
    uint8 mask = (~(1 << (7 - bit%8)))&0xff;
    bitstring[bit/8] &= mask;
}

inline bool HasPortBit(uint8* bitstring, uint32 bit)
{
    uint8 mask = (1 << (7 - bit%8))&0xff;    
    return (bitstring[bit/8] & mask) == 0 ? false : true;
}

inline void RevertWordBytes(uint32& x)
{
	x = (x<<24) | (x>>24) | ((x&0x00ff0000)>>8) | ((x&0x0000ff00)<<8);
}

//macros for seeting VLAN bitmask (one based)
#define HAS_VLAN(P, VID) ((P[(VID-1)/8] & (0x80 >> (VID-1)%8)) != 0)
#define SET_VLAN(P, VID) P[(VID-1)/8] = (P[(VID-1)/8] | (0x80 >> (VID-1)%8))
#define RESET_VLAN(P, VID) P[(VID-1)/8] = (P[(VID-1)/8] & ~(0x80 >> (VID-1)%8))

//macros for seeting TDM TIMESLOT bitmask (one based)
#define HAS_TIMESLOT HAS_VLAN
#define SET_TIMESLOT SET_VLAN
#define RESET_TIMESLOT RESET_VLAN

inline char* GetSwitchPortString(u_int32_t switch_port)
{
	static char port_string[20];

	sprintf(port_string, "%d/%d/%d", (switch_port>>12)&0xf, (switch_port>>8)&0xf, switch_port&0xff);
	return port_string;
}

// Function to compute exponent of 2.
// We implement it bcoz 'exp2' is not supported on all versions of FreeBSD.
// Used in support of Cisco Catslyst switch support modules.
inline double exponentOf2(double x)
{
   if (x<0) return 0;   
   double ret = 1.0;
   for (int i=0; i<x; i++) ret *= 2.0;
   return ret;
}

#endif //#ifndef _SWITCHCTRL_GLOBAL_H_

