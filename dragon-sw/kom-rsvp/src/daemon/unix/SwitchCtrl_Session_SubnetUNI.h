/****************************************************************************

UNI Based Switch/Subnet Control Module header file SwitchCtrl_Session_SubnetUNI.h
Created by Xi Yang @ 01/9/2007
To be incorporated into KOM-RSVP-TE package

.... Implemenation in this version is for Ethernet-over-SONET (OIF UNI1.0) only  ...

****************************************************************************/

#ifndef _SwitchCtrl_Session_SubnetUNI_H_
#define _SwitchCtrl_Session_SubnetUNI_H_

#include "SwitchCtrl_Global.h"
#include "CLI_Session.h"
#include "RSVP_API.h"
#include "RSVP_API_StateBlock.h"
#include "RSVP_API_Upcall.h"
#include "RSVP_ProtocolObjects.h"

#define UNI_IPv4_SESSION_Object SESSION_Object 
#define LSP_TUNNEL_IPv4_SENDER_TEMPLATE_Object SENDER_TEMPLATE_Object
#define LSP_TUNNEL_IPv4_FILTER_SPEC_Object SENDER_TEMPLATE_Object 
#define SONET_SDH_SENDER_TSPEC_Object SENDER_TSPEC_Object
#define SONET_SDH_FLOWSPEC_Object FLOWSPEC_Object 
#define GENERALIZED_LABEL_REQUEST_Object LABEL_REQUEST_Object

#define CTRL_CHAN_NAME_LEN 12
#define NODE_NAME_LEN 16
#define MAX_TIMESLOTS_NUM 192  //STS-1 x 192 = 10 G
typedef struct SubnetUNI_Data_struct {
	uint8 subnet_id;
	uint8 first_timeslot;
	uint16 tunnel_id;
	float ethernet_bw; // in mbps
	uint32 tna_ipv4; // also used for SenderTemplate IPv4; Session Ipv4 is implied by the peer Ipv4 in the same /30
	uint32 uni_cid_ipv4; //
	uint32 uni_nid_ipv4; //
	uint32 data_if_ipv4; //Data infterface
	uint32 logical_port; //$$$$Assuming downstrem and upstream use the same port number
	uint32 egress_label;
	uint32 upstream_label;
	uint8 control_channel_name[CTRL_CHAN_NAME_LEN];
	uint8 node_name[NODE_NAME_LEN];
	uint8 timeslot_bitmask[MAX_TIMESLOTS_NUM/8]; //bitmask
} SubnetUNI_Data;

class SwitchCtrl_Session_SubnetUNI;
typedef SimpleList<SwitchCtrl_Session_SubnetUNI*> SwitchCtrl_Session_SubnetUNI_List;
class SONET_SDH_SENDER_TSPEC_Object;
class LSP_TUNNEL_IPv4_FILTER_SPEC_Object;
class SwitchCtrl_Session_SubnetUNI: public CLI_Session, public RSVP_API
{
public:
	//constructors/destuctors
	SwitchCtrl_Session_SubnetUNI(bool isSrc=true): CLI_Session(), RSVP_API(), isSource(isSrc) { internalInit(); }
	SwitchCtrl_Session_SubnetUNI(const String& sName, const NetAddress& swAddr, bool isSrc=true): 
		CLI_Session(sName, swAddr), RSVP_API(), isSource(isSrc) { internalInit(); }
	virtual ~SwitchCtrl_Session_SubnetUNI();

	bool isSourceClient() { return isSource; }
	//Backward compatibility with general SwitchCtrl_Session operations
	virtual bool connectSwitch() { return CLI_Session::engage(); }
	virtual void disconnectSwitch() { CLI_Session::disengage(); return; }
	virtual bool refresh() { return true; } //NOP
	
	//Preparing UNI parameters
	void setSubnetUniSrc(SubnetUNI_Data& data);
	void setSubnetUniDest(SubnetUNI_Data& data);
	SubnetUNI_Data* getSubnetUniSrc() { return &subnetUniSrc; }
	SubnetUNI_Data* getSubnetUniDest() { return &subnetUniDest; }

	const LogicalInterface* getControlInterface(NetAddress& gwAddress);


	void registerRsvpApiClient(); // by both source and destination
	void deregisterRsvpApiClient(); // by both source and destination

	//Performing UNI signaling
	void initUniRsvpApiSession(); // by both source and destination
	bool isSessionOwner(const Message& msg); // by both source and destination
	void receiveAndProcessMessage(const Message& msg);  // by both source and destination

	void createRsvpUniPath(); //by source
	void receiveAndProcessPath(const Message& msg); //by destination
	void createRsvpUniResv(const SONET_SDH_SENDER_TSPEC_Object& sendTSpec, const LSP_TUNNEL_IPv4_FILTER_SPEC_Object& senderTemplate); //by destination
	void receiveAndProcessResv(const Message& msg); //by source
	void releaseRsvpPath(); //PTEAR by source and RTEAR by destination
	void refreshUniRsvpSession(); //SRefresh by source

	uint8 getUniState() { return uniState; }
	uint32 getPseudoSwitchID();
	void getTimeslots(SimpleList<uint8>& timeslots);

	//////////////// TL1 related functions >> begin  //////////////
	//bool getReplyShell_TL1( uint32 ctag = 0 );

	uint32 getNewCtag() { ++ctagNum; return (getPseudoSwitchID()+ctagNum)%999999+1; }
	uint32 getCurrentCtag() { return (getPseudoSwitchID()+ctagNum)%999999+1; }
	void getCienaTimeslotsString(String& groupMemString);
	void getCienaLogicalPortString(String& OMPortString, String& ETTPString, uint32 logicalPort=0);
	void getCienaCTPGroupInVCG(String& ctpGroupString, String& vcgName);
	void getCienaDestTimeslotsString(String& destTimeslotsString);

	bool hasVCG_TL1(String& vcgName);
	bool createVCG_TL1(String& vcgName);
	bool deleteVCG_TL1(String& vcgName);
	bool hasGTP_TL1(String& gtpName);
	bool createGTP_TL1(String& gtpName, String& vcgName);
	bool deleteGTP_TL1(String& gtpName);
	bool hasSNC_TL1(String& sncName);
	bool createSNC_TL1(String& sncName, String& gtpName);
	bool deleteSNC_TL1(String& sncName);

	bool createVCG()
	{
		//if (hasVCG_TL1(currentVCG)) return true;
		return createVCG_TL1(currentVCG);
	}
	bool deleteVCG() 
	{
		return deleteVCG_TL1(currentVCG);
	}
	bool createGTP()
	{
		//if (hasGTP_TL1(currentGTP)) return true;
		return createGTP_TL1(currentGTP, currentVCG); 
	}
	bool deleteGTP()
	{
		return deleteGTP_TL1(currentGTP);
	}
	bool createSNC() 
	{
		//if (hasSNC_TL1(currentSNC)) return true;
		return createSNC_TL1(currentSNC, currentGTP); 
	}
	bool deleteSNC()
	{
		return deleteSNC_TL1(currentSNC);
	}

	bool hasVCG()
	{
		return hasVCG(currentVCG);
	}
	bool hasGTP()
	{
		return hasGTP(currentGTP);
	}
	bool hasSNC()
	{
		return hasSNC(currentSNC);
	}

	//////////////// TL1 related functions << end //////////////

	//Upcall for source/destination client
	static void uniRsvpSrcUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData); // to be called by createSession
	static void uniRsvpDestUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData); // to be called by createSession

	static SwitchCtrl_Session_SubnetUNI_List* subnetUniApiClientList ;


	static void getSessionNameString(String& ssName, uint32 uni_tna_ip, const String& mainSessionName, uint32 main_ss_ip, bool isSource = true) {
		ssName = "subnet-uni-";
		ssName += (isSource? "src" : "dest");
		char ssTail[20];
		//sprintf(ssTail, "-tna-%x-main-session-%x",  uni_tna_ip, main_ss_ip);
		sprintf(ssTail, "-tna-%x-by-",  uni_tna_ip);
		ssName += (const char*)ssTail;
		ssName += mainSessionName;
	}
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

protected:

	bool isSource; //true --> isSender == 1
	RSVP_API::SessionId* uniSessionId;

	//UNI Ctrl/TE parameters
	SubnetUNI_Data subnetUniSrc; //not needed for destination UNI client
	SubnetUNI_Data subnetUniDest; //needed for source UNI client

	//UNI signaling states (along with PATH/RESV/ERR/TEAR messages); To be handled by uniRsvpSrcUpcall/uniRsvpDestUpcall.
	uint8 uniState; //Message::Type 

	uint32 ctagNum;
	String currentVCG;
	String currentGTP;
	String currentSNC;

private:	
	void internalInit ();
	void setSubnetUniData(SubnetUNI_Data& data, uint8 id, uint8 first_ts, uint16 tunnel_id, float bw, uint32 tna, uint32 uni_c_id, 
		uint32 uni_n_id, uint32 data_if, uint32 port, uint32 egress_label, uint32 upstream_label, uint8* node_name, uint8* cc_name, uint8* bitmask);

	char bufCmd[500];
	char strCOMPLD[20];
	char strDENY[20];
};


#endif
