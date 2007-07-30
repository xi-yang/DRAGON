/****************************************************************************

NARB Client Interface Module header file
Created by Xi Yang @ 03/15/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _NARB_APICLIENT_H_
#define _NARB_APICLIENT_H_

//App-NARB API message types
#define NARB_MSG_LSPQ 0x0001
#define DMSG_CLI_TO_NARB_BASE			0x01	/* 0x01 -- 0x1F */
#define DMSG_CLI_TOPO_CREATE			DMSG_CLI_TO_NARB_BASE+1
#define DMSG_CLI_TOPO_CONFIRM			DMSG_CLI_TO_NARB_BASE+2
#define DMSG_CLI_TOPO_DELETE			DMSG_CLI_TO_NARB_BASE+3
#define DMSG_CLI_TOPO_ERO				DMSG_CLI_TO_NARB_BASE+4


struct te_tlv_header
{
  u_int16_t	type;	// TE_TLV Type
  u_int16_t	length;
};

// data structure of an IPv4 prefix type ERO sub-object
struct ipv4_prefix_subobj
{
    u_char l_and_type;
    u_char length;
    u_char addr[4];
    u_char prefix_len;
    u_char resvd;
};

// data structure of an IPv4 prefix type ERO sub-object
struct unum_if_subobj
{
    u_char l_and_type;
    u_char length;
    u_char resvd[2];
    in_addr addr;
    u_int32_t ifid;
};

struct narb_api_msg_header
{
    u_int16_t type;
    u_int16_t length;
    u_int32_t ucid; //  unique client id
    u_int32_t seqnum; // sequence number, specified by requestor
    u_int32_t chksum;   // checksum for the above three 32-b words
    u_int32_t options;
    u_int32_t tag;
};

struct narb_api_msg
{
    struct narb_api_msg_header header;
    char * body;
};

// data structure of APP->NARB request message
struct msg_app2narb_request
{
    u_int16_t type;
    u_int16_t length;
    struct in_addr src;
    struct in_addr dest;
    u_int8_t  encoding_type;
    u_int8_t  switching_type;
    u_int16_t gpid;
    float bandwidth;
};

#define MAX_VLAN_NUM 4096
// APP->NARB optional constraint structs
struct msg_app2narb_vtag_mask
{
    u_int16_t type;
    u_int16_t length;
    u_char bitmask[MAX_VLAN_NUM/8];
};


struct narb_hop_back_tlv {
    uint16 type;
    uint16 length;
    uint32 ipv4;
};

// each NARB<->APP contains a TLV in its message body
enum  narb_tlv_type
{
    TLV_TYPE_NARB_REQUEST = 0x02,
    TLV_TYPE_NARB_ERO = 0x03,
    TLV_TYPE_NARB_ERROR_CODE = 0x04,
    TLV_TYPE_NARB_VTAG_MASK = 0x05,
    TLV_TYPE_NARB_HOP_BACK = 0x06,
};

#define NARB_MSG_CHKSUM(X) (((u_int32_t*)&X)[0] + ((u_int32_t*)&X)[1] + ((u_int32_t*)&X)[2])
#define ANY_VTAG 0xffff
#define ANY_TIMESLOT 0xff

class EXPLICIT_ROUTE_Object;

struct ero_search_entry
{
	struct {
		uint32 dest_addr;
		uint32 tunnel_id;
		uint32 ext_tunnel_id;		
		uint32 src_addr;
		uint32 lsp_id;
		float bw;
	} index;
	struct {
		uint32 ucid;
		uint32 seqnum;
	} qconf_id;
	void * session_ptr;
	EXPLICIT_ROUTE_Object *ero;
};
extern inline bool operator== (struct ero_search_entry& a, struct ero_search_entry& b)
{
	if (a.session_ptr == NULL || b.session_ptr == NULL)
		return (memcmp(&a.index, &b.index, 12) == 0);
	else
		return (memcmp(&a.index, &b.index, 12) == 0 && a.session_ptr == b.session_ptr);
}
extern inline bool operator!= (struct ero_search_entry& a, struct ero_search_entry& b)
{
	if (a.session_ptr == NULL || b.session_ptr == NULL)
		return (memcmp(&a.index, &b.index, 12) != 0);
	else 
		return (memcmp(&a.index, &b.index, 12) != 0 || a.session_ptr == b.session_ptr);
}

typedef SimpleList<struct ero_search_entry*> EroSearchList;
typedef SimpleList<uint32> UsedVtagList;

class Message;
class NARB_APIClient{
public:
	NARB_APIClient() { Init(); }
	NARB_APIClient(const char *host, int port) { _host = host; _port = port; Init(); }
	~NARB_APIClient();
	void Init() {
		fd = -1; lastState = 0;
	}
	int doConnect(char *host, int port);
	int doConnect();
	void disconnect();
	bool active();
	EXPLICIT_ROUTE_Object* getExplicitRoute(uint32 src, uint32 dest, uint8 swtype, uint8 encoding, float bandwidth, uint32& vtag, uint32& srcLclId, uint32& destLclId, uint32 hopBackAddr, uint32 excl_options, uint32 ucid, uint32 seqnum);
	EXPLICIT_ROUTE_Object* getExplicitRoute(const Message& msg, bool hasReceivedEro, void* ss_ptr = NULL);
	//EXPLICIT_ROUTE_Object* lookupExplicitRoute(uint32 src_addr, uint32 dest_addr, uint32 lsp_id, uint32 tunnel_id, uint32 ext_tunnel_id);
	EXPLICIT_ROUTE_Object* lookupExplicitRoute(uint32 dest_addr, uint32 tunnel_id, uint32 ext_tunnel_id, void* session_ptr = NULL);
	struct ero_search_entry* lookupEntry(EXPLICIT_ROUTE_Object* ero);
	uint32 getVtagFromERO(EXPLICIT_ROUTE_Object* ero);

	void removeExplicitRoute(uint32 dest_addr, uint32 tunnel_id, uint32 ext_tunnel_id);
	void removeExplicitRoute( EXPLICIT_ROUTE_Object* ero );
	void confirmReservation(const Message& msg);
	void releaseReservation(const Message& msg);
	bool handleRsvpMessage(const Message& msg);

	static uint32 extra_options;
	static void setExtraOption(String opt_str);

       //VTAG mutral-exclusion feature --> Review
	static UsedVtagList* vtagsAllowedforUse;
	static void addVtagInUse(int vtag);
	static void removeVtagInUse(int vtag);
	static void setAllowedVtags(uint8* bitmask);

	static void setHostPort(const char *host, int port);
	static bool operational();
	static String _host;
	static int _port;


private:
	int fd;
	uint32 lastState; // last state == last processed message type ...
	EroSearchList eroSearchList; 
};

#endif

