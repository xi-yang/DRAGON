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
#define MAX_VLAN	   		4095
#define MAX_VENDOR			2

struct vlanPortMap{
    uint32 vid;
    uint32 ports;
};

typedef SimpleList<vlanPortMap> vlanPortMapList;

class SNMP_Session{
	String sessionName;
	NetAddress switchInetAddr;
	//uint32 portList[MAX_VLAN+1];
	//uint32 portListUntagged[MAX_VLAN+1];
	vlanPortMapList vlanPortMapListAll;
	vlanPortMapList vlanPortMapListUntagged;
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
		//for (uint32 vlan=0; vlan<=MAX_VLAN; vlan++) 
		//	portList[vlan]=0;
		setSupportedVendorOidString();
	}
	~SNMP_Session() {disconnectSwitch();}
	bool operator==( const SNMP_Session& s ) const {
		return (active == s.active && sessionName == s.sessionName && switchInetAddr == s.switchInetAddr && vendor == s.vendor);
	}
	SNMP_Session& operator=(const SNMP_Session& s) { 
		sessionName = s.sessionName;
		switchInetAddr = s.switchInetAddr;
		//for (uint32 i = 0; i <= MAX_VLAN; i++)
		//	portList[i] = s.portList[i];
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
		vlanPortMapList::iterator iter;
		//for (uint32 vlan=MIN_VLAN; vlan<=MAX_VLAN; vlan++)
		//	if (!portList[vlan]) return vlan;
		for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); iter++)
		    if ((*iter).ports == 0)
                      return (*iter).vid;  
		return 0;
	}
	bool setVLANPVID(uint32 port, uint32 vlanID); // A hack to Dell 5324 switch
	bool setVLANPortTag(uint32 portListNew, uint32 vlanID); // A hack to Dell 5324 switch
	bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	bool removePortFromVLAN(uint32 port, uint32 vlanID);
	bool movePortToDefaultVLAN(uint32 port);
	uint32 getVLANbyUntaggedPort(uint32 port){
		//for(int i=MIN_VLAN;i<=MAX_VLAN;i++){
		//	if(portListUntagged[i]&(1<<(32-port)))
		//	      return i;
		vlanPortMapList::iterator iter;
		for (iter = vlanPortMapListUntagged.begin(); iter != vlanPortMapListAll.end(); iter++)
		    if (((*iter).ports)&(1<<(32-port)))
                      return (*iter).vid;  
		return 0;
	}
	uint32 getVLANbyPort(uint32 port){
		//for(int i=MIN_VLAN;i<=MAX_VLAN;i++){
		//	if(portList[i]&(1<<(32-port)))
		//	      return i;
		//}
		vlanPortMapList::iterator iter;
		for (iter = vlanPortMapListAll.begin(); iter != vlanPortMapListAll.end(); iter++)
		    if (((*iter).ports)&(1<<(32-port)))
                      return (*iter).vid;  
		return 0;
	}
	uint32 getVLANListbyPort(uint32 port, SimpleList<uint32> &vlan_list){
		vlan_list.clear();
		vlanPortMapList::iterator iter;
		for (iter = vlanPortMapListUntagged.begin(); iter != vlanPortMapListAll.end(); iter++)
		    if (((*iter).ports)&(1<<(32-port)))
                      return vlan_list.push_back((*iter).vid);

		//for(int i=MIN_VLAN;i<=MAX_VLAN;i++){
		//	if(portList[i]&(1<<(32-port)))
		//	      vlan_list.push_back(i);
		//}
		return vlan_list.size();
	}
	void readVlanPortMapBranch(const char* oid_str, vlanPortMapList &vpmList);
	bool readVLANFromSwitch();
	String& getSessionName() {return sessionName;}
	NetAddress& getSwitchInetAddr() {return switchInetAddr;}
	bool isValidSession() const {return active;}
        bool verifyVLAN(uint32 vlanID);
        bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID);
        bool VLANHasTaggedPort(uint32 vlanID);
};

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

typedef SimpleList<SNMP_Session*> SNMPSessionList;
class SNMP_Global{
	SNMPSessionList snmpSessionList;

public:
	SNMP_Global() { snmpSessionList.clear(); init_snmp("VLSRCtrl");} 
	~SNMP_Global();
	bool addSNMPSession(SNMP_Session* addSS);
	SNMPSessionList& getSNMPSessionList() { return snmpSessionList; }
public:
        static LocalIdList localIdList;
        static void addLocalId(uint16 type, uint16 value, uint16  tag = 0) {
        LocalIdList::Iterator it;
        LocalId lid;

        for (it = localIdList.begin(); it != localIdList.end(); ++it) {
            lid = *it;
            if (lid.type == type && lid.value == value) {
                if (type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP)  {
                    SimpleList<uint16>::Iterator it_uint16;
                    for (it_uint16 = lid.group->begin(); it_uint16 != lid.group->end(); ++it_uint16) {
                        if (*it_uint16 == tag)
                            return;
                        }
                    lid.group->push_back(tag);
                    return;
                    }
                else
                    return;
                }
            }
            lid.type = type;
            lid.value = value;
            localIdList.push_back(lid);
            localIdList.back().group = new SimpleList<uint16>;
            if ((type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP) && tag != 0)
                localIdList.back().group->push_back(tag);
            }
        static void deleteLocalId(uint16 type, uint16 value, uint16  tag = 0) {
            LocalIdList::Iterator it;
            LocalId lid;
            if (type == 0xffff && value == 0xffff) {
                    //for (it = localIdList.begin(); it != localIdList.end(); ++it)
                     //   if (lid.group)
                     //       delete lid.group;
                    localIdList.clear();
                    return;
                }
            for (it = localIdList.begin(); it != localIdList.end(); ++it) {
                lid = *it;
                if (lid.type == type && lid.value == value) {
                    if ((type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP)) {
                        if (tag == 0 && lid.group) {
                            delete lid.group;
                            localIdList.erase(it);
                            }
                        else {
                            SimpleList<uint16>::Iterator it_uint16;
                            for (it_uint16 = lid.group->begin(); it_uint16 != lid.group->end(); ++it_uint16) {
                                if (*it_uint16 == tag)
                                    lid.group->erase(it_uint16);
                                }
                            if (lid.group->size() == 0) {
                                delete lid.group;
                                localIdList.erase(it);
                                }
                            }
                        return;
                        }
                    else {
                            delete lid.group;
                            localIdList.erase(it);
                            return;
                        }
                    }
                }
            }
        static bool hasLocalId(uint16 type, uint16 value, uint16  tag = 0) {
            LocalIdList::Iterator it;
            LocalId lid;
            for (it = localIdList.begin(); it != localIdList.end(); ++it) {
                lid = *it;
                if (lid.type == type && lid.value == value) {
                    if ((type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP) && tag != 0) {
                        SimpleList<uint16>::Iterator it_uint16;
                        for (it_uint16 = lid.group->begin(); it_uint16 != lid.group->end(); ++it_uint16) {
                            if (*it_uint16 == tag)
                                return true;;
                            }
                        return false;
                        }
                    else
                        return true;
                    }
                }
                return false;
            }
        static void clearLocalIdList( ) {
            localIdList.clear();
            }
        static void processLocalIdMessage(uint8 msgType, LocalId& lid);
        static void getPortsByLocalId(SimpleList<uint32>&portList, uint32 port);

};

#endif /* _SNMP_Global_h_ */
