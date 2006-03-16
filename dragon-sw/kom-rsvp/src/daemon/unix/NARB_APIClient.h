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


class EXPLICIT_ROUTE_Object;
class NARB_APIClient{
public:
	static NARB_APIClient& instance();
	void setHostPort(const char *host, int port);
	int doConnect(char *host, int port);
	int doConnect();
	void disconnect();
	bool operational();
	EXPLICIT_ROUTE_Object* getExplicitRoute(uint32 src, uint32 dest, uint8 swtype, uint8 encoding, float bandwidth, uint32 vtag);

protected:
	NARB_APIClient();
	~NARB_APIClient();
	NARB_APIClient(const NARB_APIClient& obj);
	static NARB_APIClient apiclient;

private:
	String _host;
	int _port;
	int fd;
};

#endif

