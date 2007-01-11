/****************************************************************************

UNI Based Switch/Subnet Control Module header file SwitchCtrl_Session_SubnetUNI.h
Created by Xi Yang @ 01/9/2007
To be incorporated into KOM-RSVP-TE package

.... Implemenation in this version is for Ethernet-over-SONET (OIF UNI1.0) only  ...

****************************************************************************/

#ifndef _SwitchCtrl_Session_SubnetUNI_H_
#define _SwitchCtrl_Session_SubnetUNI_H_

#include "SwitchCtrl_Global.h"
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

typedef struct IPv4TNA IPv4TNA_Subobject;
#define CTRL_CHAN_NAME_LEN 12
typedef struct SubnetUNI_Data_struct {
	uint16 subnet_id;
	uint16 tunnel_id;
	float ethernet_bw; // in mbps
	uint32 tna_ipv4; // also used for SenderTemplate IPv4; Session Ipv4 is implied by the peer Ipv4 in the same /30
	uint32 uni_cid_ipv4; //reserved ! == tna_ipv4 ...
	uint32 uni_nid_ipv4; //reserved ! (might be redundant) //might also need to configure in ospf _subnet_uni_speicific_info
	uint32 logical_port;
	uint32 egress_label;
	uint32 upstream_label;
	char control_channel_name[CTRL_CHAN_NAME_LEN]; //consistent with TNA_IPv4 //redundant@@@@
} SubnetUNI_Data;

class SwitchCtrl_Session_SubnetUNI;
typedef SimpleList<SwitchCtrl_Session_SubnetUNI*> SwitchCtrl_Session_SubnetUNI_List;
class SONET_SDH_SENDER_TSPEC_Object;
class LSP_TUNNEL_IPv4_FILTER_SPEC_Object;
class SwitchCtrl_Session_SubnetUNI: public SwitchCtrl_Session, public RSVP_API
{
public:
	//constructors/destuctors
	SwitchCtrl_Session_SubnetUNI(bool isSrc=true): SwitchCtrl_Session(), RSVP_API(), isSource(isSrc) { internalInit(); }
	SwitchCtrl_Session_SubnetUNI(const String& sName, const NetAddress& swAddr, bool isSrc=true): 
		SwitchCtrl_Session(sName, swAddr), RSVP_API(), isSource(isSrc) { internalInit(); }
	virtual ~SwitchCtrl_Session_SubnetUNI();

	//Backward compatibility with general SwitchCtrl_Session operations
	virtual bool connectSwitch() { return true; } //NOP
	virtual void disconnectSwitch() { return; } //NOP

	//Preparing UNI parameters
	void setSubnetUniSrc(uint16 id, float bw, uint32 tna, uint32 port, uint32 egress_label, uint32 upstream_label, char* cc_name)
		{ setSubnetUniData(subnetUniSrc, id, bw, tna, port, egress_label, upstream_label, cc_name); }
	void setSubnetUniDest(uint16 id, float bw, uint32 tna, uint32 port, uint32 egress_label, uint32 upstream_label, char* cc_name)
		{ setSubnetUniData(subnetUniDest, id, bw, tna, port, egress_label, upstream_label, cc_name); }

	void registerRsvpApiClient(); // by both source and destination
	void deregisterRsvpApiClient(); // by both source and destination

	//Performing UNI signaling
	void initUniRsvpApiSession(); // by both source and destination
	bool isSessionOwner(Message& msg); // by both source and destination
	void receiveAndProcessMessage(Message& msg);  // by both source and destination

	void createRsvpUniPath(); //by source
	void receiveAndProcessPath(Message& msg); //by destination
	void createRsvpUniResv(const SONET_SDH_SENDER_TSPEC_Object& sendTSpec, const LSP_TUNNEL_IPv4_FILTER_SPEC_Object& senderTemplate); //by destination
	void receiveAndProcessResv(Message& msg); //by source
	void releaseRsvpPath(); //PTEAR by source and RTEAR by destination
	void refreshUniRsvpSession(); //SRefresh by source

	//Upcall for source/destination client
	static void uniRsvpSrcUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData); // to be called by createSession
	static void uniRsvpDestUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData); // to be called by createSession

	static SwitchCtrl_Session_SubnetUNI_List* subnetUniApiClientList ;

protected:

	bool isSource; //true --> isSender == 1
	RSVP_API::SessionId* uniSessionId;

	//UNI Ctrl/TE parameters
	SubnetUNI_Data subnetUniSrc; //not needed for destination UNI client
	SubnetUNI_Data subnetUniDest; //needed for source UNI client

	//UNI signaling states (along with PATH/RESV/ERR/TEAR messages); To be handled by uniRsvpSrcUpcall/uniRsvpDestUpcall.
	
private:	
	inline void internalInit ();
	inline void setSubnetUniData(SubnetUNI_Data& data, uint16 id, float bw, uint32 tna, uint32 port, 
		uint32 egress_label, uint32 upstream_label, char* cc_name=NULL);
};


//////////////////////////
//UNI RSVP Protocol Objects//
//////////////////////////

typedef struct  {
	uint16 length;
	uint8 type;
	uint8 sub_type;
	uint8 u_b0;
	uint8 reserved[3];
	uint32 logical_port;
	uint32 label;
} EgressLabel_Subobject;

class GENERALIZED_UNI_Object: public RefObject<GENERALIZED_UNI_Object> {
	IPv4TNA_Subobject srcTNA;
	IPv4TNA_Subobject destTNA;
	EgressLabel_Subobject egressLabel;
	EgressLabel_Subobject egressLabelUp;

	friend ostream& operator<< ( ostream&, const GENERALIZED_UNI_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const GENERALIZED_UNI_Object& );
	uint16 size() const{ return (sizeof(IPv4TNA_Subobject)*2 + sizeof(EgressLabel_Subobject)); }

	REF_OBJECT_METHODS(GENERALIZED_UNI_Object)

public:
	GENERALIZED_UNI_Object() {
		srcTNA.length = sizeof(IPv4TNA_Subobject);
		srcTNA.type = UNI_SUBOBJ_SRCTNA;
		srcTNA.sub_type = UNI_TNA_SUBTYPE_IPV4;
		srcTNA.addr.s_addr = 0;

		destTNA = srcTNA; 
		destTNA.type = UNI_SUBOBJ_DESTTNA;

		memset(&egressLabel, 0, sizeof(EgressLabel_Subobject));
		egressLabel.length = sizeof(EgressLabel_Subobject);
		egressLabel.type = UNI_SUBOBJ_EGRESSLABEL;
		egressLabel.sub_type = 1;
		egressLabel.u_b0 = 0;

		egressLabelUp = egressLabel;		
		egressLabelUp.u_b0 = 1; //0x80 ?
	}
	GENERALIZED_UNI_Object( uint32 src_addr, uint32 dest_addr, uint32 egress_port, uint32 egress_label, uint32 egress_port_up, uint32 egress_label_up) {
		srcTNA.length = sizeof(IPv4TNA_Subobject);
		srcTNA.type = UNI_SUBOBJ_SRCTNA;
		srcTNA.sub_type = UNI_TNA_SUBTYPE_IPV4;
		srcTNA.addr.s_addr = src_addr;

		destTNA = srcTNA; 
		destTNA.type = UNI_SUBOBJ_DESTTNA;
		destTNA.addr.s_addr = dest_addr;

		memset(&egressLabel, 0, sizeof(EgressLabel_Subobject));
		egressLabel.length = sizeof(EgressLabel_Subobject);
		egressLabel.type = UNI_SUBOBJ_EGRESSLABEL;
		egressLabel.sub_type = 1;
		egressLabel.u_b0 = 0;
		egressLabel.logical_port = egress_port;
		egressLabel.label = egress_label;
			
		egressLabelUp = egressLabel;		
		egressLabelUp.u_b0 = 1; //0x80 ?		
		egressLabel.logical_port= egress_port_up;
		egressLabel.label = egress_label_up;
	}

	GENERALIZED_UNI_Object(INetworkBuffer& buffer, uint16 len) {
		readFromBuffer(buffer, len );
	}
	void readFromBuffer( INetworkBuffer& buffer, uint16 len);
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	IPv4TNA_Subobject& getSrcTNA() { return srcTNA; }
	IPv4TNA_Subobject& getDestTNA(){ return destTNA; }
	EgressLabel_Subobject& getEgressLabel(){ return egressLabel; }
	EgressLabel_Subobject& getEgressLabelUpstream(){ return egressLabelUp; }
	bool operator==(const GENERALIZED_UNI_Object& s){
		return (memcmp(&srcTNA, &s.srcTNA, sizeof(IPv4TNA_Subobject)) == 0 
			&& memcmp(&destTNA, &s.destTNA, sizeof(IPv4TNA_Subobject)) == 0
			&& memcmp(&egressLabel, &s.egressLabel, sizeof(EgressLabel_Subobject)) == 0
			&& memcmp(&egressLabelUp, &s.egressLabelUp, sizeof(EgressLabel_Subobject)) == 0);
	}
};
extern inline GENERALIZED_UNI_Object::~GENERALIZED_UNI_Object() { }


#endif
