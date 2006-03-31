/****************************************************************************

NARB Client Interface Module header file
Created by Xi Yang @ 03/15/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _NARB_APICLIENT_H_
#define _NARB_APICLIENT_H_


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

#define NARB_MSG_CHKSUM(X) (((u_int32_t*)&X)[0] + ((u_int32_t*)&X)[1] + ((u_int32_t*)&X)[2])
#define ANY_VTAG 0xffff

class EXPLICIT_ROUTE_Object;

struct ero_search_entry
{
	struct {
		uint32 src_addr;
		uint32 dest_addr;
		uint32 lsp_id;
		uint32 tunnel_id;
		uint32 ext_tunnel_id;		
	} index;
	EXPLICIT_ROUTE_Object *ero;
};
extern inline bool operator== (struct ero_search_entry& a, struct ero_search_entry& b)
{
	return (memcmp(&a.index, &b.index, 20) == 0);
}
extern inline bool operator!= (struct ero_search_entry& a, struct ero_search_entry& b)
{
	return (memcmp(&a.index, &b.index, 20) != 0);
}

typedef SimpleList<struct ero_search_entry*> EroSearchList;

class Message;
class NARB_APIClient{
public:
	NARB_APIClient(): fd(-1), lastMessage(0) {}
	NARB_APIClient(const char *host, int port): fd(-1), lastMessage(0) { _host = host; _port = port;}
	~NARB_APIClient();
	int doConnect(char *host, int port);
	int doConnect();
	void disconnect();
	bool active();
	EXPLICIT_ROUTE_Object* getExplicitRoute(uint32 src, uint32 dest, uint8 swtype, uint8 encoding, float bandwidth, uint32 vtag, uint32 srcLclId, uint32 destLclId);
	EXPLICIT_ROUTE_Object* getExplicitRoute(const Message& msg);
	EXPLICIT_ROUTE_Object* lookupExplicitRoute(uint32 src_addr, uint32 dest_addr, uint32 lsp_id, uint32 tunnel_id, uint32 ext_tunnel_id);

	void handleRsvpMessage(const Message& msg);	//$$$$ //RESV CONFIRM	//RESV RELEASE ...

	static void setHostPort(const char *host, int port);
	static bool operational();
	static String _host;
	static int _port;

private:
	int fd;
	uint32 lastMessage; //last message type ...
	EroSearchList eroSearchList; 
};

#endif

