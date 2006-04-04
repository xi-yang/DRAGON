/*
 * OSPFd main header.
 * Copyright (C) 1998, 99, 2000 Kunihiro Ishiguro, Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 * 
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _ZEBRA_DRAGOND_H
#define _ZEBRA_DRAGOND_H

#define DEBUG

/* DRAGON version number */
#define DRAGON_VERSION 	1
#define IS_VALID_DRAGON_VERSION(X) (X==DRAGON_VERSION)

/* VTY port number. */
#define DRAGON_VTY_PORT          2611
#define DRAGON_VTYSH_PATH        "/tmp/.dragon"
#define DRAGON_XML_PATH		 "/tmp/.dragon_xml"
/* Default configuration file name for dragon. */
#define DRAGON_DEFAULT_CONFIG   "dragon.conf"

#define DRAGON_INITIAL_SEQUENCE_NUMBER    0x80000001
#define DRAGON_MAX_SEQUENCE_NUMBER          0x8fffffff
#define IS_VALID_DRAGON_SEQNO(X)		(X >= DRAGON_INITIAL_SEQUENCE_NUMBER && X <= DRAGON_MAX_SEQUENCE_NUMBER)

#define DRAGON_MAX_PACKET_SIZE		1500
#define DRAGON_MSG_HDR_LENGTH 		8

/* Module definition for DRAGON */
#define   MODULE_CLI 		0
#define   MODULE_OSPF 		1
#define   MODULE_ZEBRA		2
#define   MODULE_RSVP  		3
#define   MODULE_ASTDL 		4
#define   MODULE_NARB_INTRA	5
#define   MODULE_NARB_INTER	6
#define   MODULE_PCE  		7
#define   MODULE_XML		8

/* The following defines DRAGON message types */
/* CLI/ASTDL to NARB message */
#define DMSG_CLI_TO_NARB_BASE			0x01	/* 0x01 -- 0x1F */
#define DMSG_CLI_TOPO_CREATE			DMSG_CLI_TO_NARB_BASE+1
#define DMSG_CLI_TOPO_CONFIRM			DMSG_CLI_TO_NARB_BASE+2
#define DMSG_CLI_TOPO_DELETE			DMSG_CLI_TO_NARB_BASE+3
#define DMSG_CLI_TOPO_ERO				DMSG_CLI_TO_NARB_BASE+4

#define DMSG_NARB_TO_CLI_BASE			0x20	/* 0x20 -- 0x3F */
#define DMSG_NARB_TOPO_RESPONSE		DMSG_NARB_TO_CLI_BASE+1
#define DMSG_NARB_TOPO_REJECT			DMSG_NARB_TO_CLI_BASE+2
#define DMSG_NARB_TOPO_DELETE_CONF	DMSG_NARB_TO_CLI_BASE+3

#define DMSG_CLI_TO_RSVP_BASE			0x40	/* 0x40 -- 0x5F */
#define DMSG_CLI_PATH_SETUP			DMSG_CLI_TO_RSVP_BASE+1

#define DMSG_RSVP_TO_CLI_BASE			0x60	/* 0x60 -- 0x7F */
#define DMSG_NARB_TO_PCE_BASE		0x80	/* 0x80 -- 0x9F */
#define DMSG_PCE_TO_NARB_BASE		0xA0	/* 0xA0 -- 0xBF */

#ifdef DEBUG
#define DRAGON_LSP_REFRESH_INTERVAL		2000
#else
#define DRAGON_LSP_REFRESH_INTERVAL		100
#endif

/* Maximum LSP name length */
#define MAX_LSP_NAME_LENGTH			64

/* OSPF protocol specific definiton, copied from ospfd/ospf_te.c */
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC1		1
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC2		2
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC3		3 
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC4		4
#define LINK_IFSWCAP_SUBTLV_SWCAP_L2SC		51
#define LINK_IFSWCAP_SUBTLV_SWCAP_TDM		100
#define LINK_IFSWCAP_SUBTLV_SWCAP_LSC		150
#define LINK_IFSWCAP_SUBTLV_SWCAP_FSC		200

#define LINK_IFSWCAP_SUBTLV_ENC_PKT			1
#define LINK_IFSWCAP_SUBTLV_ENC_ETH			2
#define LINK_IFSWCAP_SUBTLV_ENC_PDH			3
#define LINK_IFSWCAP_SUBTLV_ENC_RESV1		4
#define LINK_IFSWCAP_SUBTLV_ENC_SONETSDH	5
#define LINK_IFSWCAP_SUBTLV_ENC_RESV2		6
#define LINK_IFSWCAP_SUBTLV_ENC_DIGIWRAP	7
#define LINK_IFSWCAP_SUBTLV_ENC_LAMBDA		8
#define LINK_IFSWCAP_SUBTLV_ENC_FIBER		9
#define LINK_IFSWCAP_SUBTLV_ENC_RESV3		10
#define LINK_IFSWCAP_SUBTLV_ENC_FIBRCHNL		11

#define	LINK_PROTYPE_SUBTLV_VALUE_EXTRA		0x01
#define	LINK_PROTYPE_SUBTLV_VALUE_UNPRO		0x02
#define	LINK_PROTYPE_SUBTLV_VALUE_SHARED	0x04
#define	LINK_PROTYPE_SUBTLV_VALUE_1TO1		0x08
#define	LINK_PROTYPE_SUBTLV_VALUE_1PLUS1	0x10
#define	LINK_PROTYPE_SUBTLV_VALUE_ENHANCED	0x20
#define	LINK_PROTYPE_SUBTLV_VALUE_RESV1		0x40
#define	LINK_PROTYPE_SUBTLV_VALUE_RESV2		0x80


#define DRAGON_TIMER_OFF(X)                                                     \
    do {                                                                      \
      if (X)                                                                  \
        {                                                                     \
          thread_cancel (X);                                                  \
          (X) = NULL;                                                         \
        }                                                                     \
    } while (0)

#define DRAGON_TIMER_ON(T,F,A, V) \
    do {                                                                      \
      if (!(T)) \
        (T) = thread_add_timer (master, (F), (A), (V)); 	\
    } while (0)

#define DRAGON_WRITE_ON(T, L, FD)                                                  \
      do                                                                      \
        {                                                                     \
	  if (!(T))                                           \
	    (T) =                                                    \
	      thread_add_write (master, dragon_write, (L), (FD));            \
        } while (0)

/* The following defines DRAGON message and TLVs */

/*Obsolete header format*/
struct dmsg_header {
	u_int8_t version;		/* Version */
	u_int8_t msgtype;		/* Message type */
	u_int16_t msglen;		/* Total message length, including header */
	u_int32_t seqno;		/* Sequence number */
};

#define DRAGON_TLV_BASE		0x01
/* Source, destination, bandwidth, encoding type, switching type, G-Pid */ /*Mandatory TLV */
#define	DRAGON_TLV_SRCDST	DRAGON_TLV_BASE + 1
/* ERO - a list of IP addresses */ /*Mandatory TLV */
#define	DRAGON_TLV_ERO		DRAGON_TLV_BASE + 2
/* Error code - returned by NARB */ 
#define	DRAGON_TLV_ERR		DRAGON_TLV_BASE + 3

struct dragon_tlv_header
{
  u_int16_t	type;			/* DRAGON_TLV_XXX (see above) */
  u_int16_t	length;			/* Value portion only, in octets */
};

/*App-NARB API message types*/
#define NARB_MSG_LSPQ 0x0001
#define NARB_MSG_CTRL 0x0100
#define NARB_MSG_ASTB 0x0200
#define NARB_MSG_AAA 0x0300

/*App-NARB API message header*/
struct api_msg_header
{
    u_int16_t type;
    u_int16_t length;
    u_int32_t ucid; /*  unique client id*/
    u_int32_t seqnum; /* sequence number, specified by requestor*/
    u_int32_t chksum;   /* checksum for the above three 32-b words*/
    u_int32_t options; /* routing and data format options*/
    u_int32_t tag; /*optional tag*/
};

#define API_MSG_CHKSUM(X) (((u_int32_t*)&X)[0] + ((u_int32_t*)&X)[1] + ((u_int32_t*)&X)[2])

/* api_msg options*/
/* LSP computation options*/
#define LSP_OPT_STRICT  ((u_int32_t)(0x01 << 16)) /*otherwise LSP_OPT_LOOSE*/
#define LSP_OPT_PREFERRED ((u_int32_t)(0x02 << 16)) /*otherwise LSP_OPT_ONLY*/
#define LSP_OPT_MRN 0x04 << 16  /*otherwise SINGLE_REGION*/
#define LSP_OPT_BIDIRECTIONAL 0x10 << 16  /* otherwise UNIDIRECTIONAL*/
#define LSP_OPT_E2E_VTAG  ((u_int32_t)(0x20 << 16)) //otherwise Untgged VLAN for E2E Ethernet

#ifdef roundup
#  define ROUNDUP(val, gran)	roundup(val, gran)
#else /* roundup */
#  define ROUNDUP(val, gran)	(((val) - 1 | (gran) - 1) + 1)
#endif /* roundup */

#define DTLV_HDR_SIZE \
	sizeof (struct dragon_tlv_header)

#define DTLV_BODY_SIZE(tlvh) \
	ROUNDUP (ntohs ((tlvh)->length), sizeof (u_int32_t))

#define DTLV_SIZE(tlvh) \
	(sizeof(struct api_msg_header) + DTLV_BODY_SIZE(tlvh))

#define DTLV_HDR_TOP(amsgh) \
	(struct dragon_tlv_header *)((char *)(amsgh) + sizeof(struct api_msg_header))

#define DTLV_HDR_NEXT(tlvh) \
	(struct te_tlv_header *)((char *)(tlvh) + DTLV_SIZE(tlvh))


/* The following defines locally stored parameters */
struct dragon_module {
  int module;					/* Module's id */
  struct in_addr ip_addr;		/* IP address of the server that is running the module */
  u_int16_t port;				/* Port that the module is lisening on */
  const char* name;				/* Module's name */
};

#define Bps2Mbps(x) (x*8/1000000)

enum lsp_flags {
	LSP_FLAG_RRO = (1 << 0),
	LSP_FLAG_BIDIR = (1 << 1),
	LSP_FLAG_AFFINITY = (1 << 2),
	LSP_FLAG_MAXMETRIC = (1 << 3),
	LSP_FLAG_HOPLIMIT = (1 << 4),
	LSP_FLAG_LOOSEHOP = (1 << 5),
	LSP_FLAG_EXCLUDENODE = (1 << 6),
	LSP_FLAG_SCHEDULE = (1 << 7),
	LSP_FLAG_AAA = (1 << 8),
	LSP_FLAG_RECEIVER = (1 << 9),
	LSP_FLAG_REG_BY_RSVP = (1 << 10)
};


enum BwEnc{
	LSP_BW_DS0			= 0x45FA0000,
	LSP_BW_DS1			= 0x483C7A00,
	LSP_BW_E1				= 0x487A0000,
	LSP_BW_DS2			= 0x4940A080,
	LSP_BW_E2				= 0x4980E800,
	LSP_BW_Eth				= 0x49989680,
	LSP_BW_E3				= 0x4A831A80,
	LSP_BW_DS3			= 0x4AAAA780,
	LSP_BW_STS1			= 0x4AC5C100,
	LSP_BW_Fast_Eth		= 0x4B3EBC20,
	LSP_BW_200m_Eth		= 0x4BBEBC20,
	LSP_BW_300m_Eth		= 0x4C0F0D18,
	LSP_BW_400m_Eth		= 0x4C3EBC20,
	LSP_BW_500m_Eth		= 0x4C6E6B28,
	LSP_BW_600m_Eth		= 0x4C8F0D18,
	LSP_BW_700m_Eth		= 0x4CA6E49C,
	LSP_BW_800m_Eth		= 0x4CBEBC20,
	LSP_BW_900m_Eth		= 0x4CD693A4,
	LSP_BW_E4				= 0x4B84D000,
	LSP_BW_FC0_133M		= 0x4B7DAD68,
	LSP_BW_OC3			= 0x4B9450C0,
	LSP_BW_FC0_266M		= 0x4BFDAD68,
	LSP_BW_FC0_531M		= 0x4C7D3356,
	LSP_BW_OC12			= 0x4C9450C0,
	LSP_BW_Gig_E 			= 0x4CEE6B28,
	LSP_BW_2Gig_E			= 0x4D6E6B28,
	LSP_BW_3Gig_E			= 0x4DB2D05E,
	LSP_BW_4Gig_E			= 0x4DEE6B28,
	LSP_BW_5Gig_E			= 0x4E1502F9,
	LSP_BW_6Gig_E			= 0x4E32D05E,
	LSP_BW_7Gig_E			= 0x4E509DC3,
	LSP_BW_8Gig_E			= 0x4E6E6B28,
	LSP_BW_9Gig_E			= 0x4E861C46,
	LSP_BW_10Gig_E		= 0x4E9502F9,
	LSP_BW_FC0_1062M 		= 0x4CFD3356,
	LSP_BW_OC48			= 0x4D9450C0,
	LSP_BW_OC192 			= 0x4E9450C0,
	LSP_BW_OC768 			= 0x4F9450C0,
	LSP_BW_Gig_E_OverFiber = 0x4D1502F9,	/* Movaz specific value */
	LSP_BW_HDTV			= 0x4D31069A,       /* Movaz specific value */
};

enum G_PID {
	G_Illegal = 0,
	G_Reserved1 = 1,
	G_Reserved2 = 2,
	G_Reserved3 = 3,
	G_Reserved4 = 4,
	G_Asyn_E4 = 5,
	G_Asyn_DS3 = 6,
	G_Asyn_E3 = 7,
	G_BitSyn_E3 = 8,
	G_ByteSyn_E3 = 9,
	G_Asyn_DS2 = 10,
	G_BitSyn_DS2 = 11,
	G_Reserved5 = 12,
	G_Asyn_E1 = 13,
	G_ByteSyn_E1 = 14,
	G_ByteSyn_31DS0 = 15,
	G_Asyn_DS1 = 16,
	G_BitSyn_DS1 = 17,
	G_ByteSyn_T1 = 18,
	G_VC11 = 19,
	G_Reserved6 = 20,
	G_Reserved7 = 21,
	G_DS1SFAsyn = 22,
	G_DS1ESFAsyn = 23,
	G_DS3M23Asyn = 24,
	G_DS3CBitAsyn = 25,
	G_VT_LOVC = 26,
	G_STS_HOVC = 27,
	G_POSUnscr16 = 28,
	G_POSUnscr32 = 29,
	G_POSScram16 = 30,
	G_POSScram32 = 31,
	G_ATM = 32,
	G_Eth = 33,
	G_SONET_SDH = 34,
	G_Reserved = 35,
	G_DigiWrapper = 36,
	G_Lamda = 37
};

enum _RSVP_MsgType { 
	InitAPI = 0, 
	Path = 1, 
	Resv, PathErr, ResvErr, PathTear, ResvTear, 
	ResvConf, Ack = 13, Srefresh = 15, Load = 126, 
	PathResv = 127, RemoveAPI = 255 
};


struct _Session_Para {
	struct in_addr srcAddr;	
	u_int16_t srcPort;
	struct in_addr destAddr;	
	u_int16_t destPort;
};
struct _ADSpec_Para {
	u_int32_t ADSpecHopCount;
	s_int32_t ADSpecBandwidth;
	s_int32_t ADSpecMinPathLatency;
	u_int32_t ADSpecMTU;
	u_int32_t ADSpecCLHopCount;
	s_int32_t ADSpecCLMinPathLatency;
	s_int32_t ADSpecGSCtot;
	s_int32_t ADSpecGSDtot;
	s_int32_t ADSpecGSCsum;
	s_int32_t ADSpecGSDsum;
	s_int32_t ADSpecGSMinPathLatency;
};
struct _GenericTSpec_Para {
	u_int32_t R;
	u_int32_t B;
	u_int32_t P;
	s_int32_t m;
	s_int32_t M;
};
struct _SonetTSpec_Para {
	u_int8_t Sonet_ST;		/* "Sonet_" is added to avoid compilation problem in gcc 3.4.x :-( */
	u_int8_t Sonet_RCC;
	u_int16_t Sonet_NCC;
	u_int16_t Sonet_NVC;
	u_int16_t Sonet_MT;
	u_int32_t Sonet_T;
	u_int32_t Sonet_P;
};

enum _AbstractNodeType { Illegal = 0, IPv4 = 1, IPv6 = 2, AS = 32 , UNumIfID = 4, VLSR = 8};
struct _EROAbstractNode_Para {
	u_int8_t type; /* Abstract node type */
	u_int8_t isLoose;
	union {
		struct {
			struct in_addr addr;
			u_int8_t prefix;
		} ip4;
		struct {
			u_int16_t reserved;
			struct in_addr routerID;
			u_int32_t interfaceID;
		}uNumIfID;
		u_int16_t asNum;
	} data;
};

#define ABSTRACT_NODE_TYPE(X)      (X & 0x7F)
#define ABSTRACT_NODE_LOOSE(X)      (X & 0x80)
struct AbstractNode_IPv4 {
	u_int8_t typeOrLoose;
	u_int8_t length;
	u_int8_t addr[4];
	u_int8_t prefix;
	u_int8_t resvd;
};

struct AbstractNode_AS {
	u_int8_t typeOrLoose;
	u_int8_t length;
	u_int16_t asNum;
};

struct AbstractNode_UnNumIfID {
	u_int8_t typeOrLoose;
	u_int8_t length;
	u_int16_t resvd;
	u_int8_t routerID[4];
	u_int32_t interfaceID;
};

struct _SessionAttribute_Para {
	u_int32_t excludeAny;
	u_int32_t includeAny;
	u_int32_t includeAll;
	u_int8_t setupPri;
	u_int8_t holdingPri;
	u_int8_t flags;
	u_int8_t nameLength;
	char* sessionName;
};

enum LabelCType{LABEL_MPLS = 1,
				     LABEL_GENERALIZED = 2,
				     LABEL_WAVEBAND = 3
};

struct _LabelRequest_Para {
	union {
		struct {
			u_int8_t lspEncodingType;
			u_int8_t switchingType;
			u_int16_t gPid;
		} gmpls;
		u_int32_t mpls_l3pid; //For MPLS
	}data;
	u_int8_t labelType;
};

struct _Protection_Para {
	u_int8_t secondary;
	u_int8_t pro_type;
};


struct _Dragon_Uni_Para {
	u_int32_t srcLocalId;
	u_int32_t destLocalId;
	u_int32_t vlanTag;
	char ingressChannel[12];
	char egressChannel[12];
};

struct _sessionParameters {
	//Mandatory parameters
	struct _LabelRequest_Para LabelRequest_Para;
	struct _Session_Para Session_Para;
	//Optional parameters
	struct _ADSpec_Para* ADSpec_Para;
	struct _GenericTSpec_Para* GenericTSpec_Para;
	struct _SonetTSpec_Para* SonetTSpec_Para;
#define MAX_ERO_NUMBER	32
	u_int8_t ERONodeNumber;	// 32 in Maximum
	struct _EROAbstractNode_Para* EROAbstractNode_Para;
	struct _Dragon_Uni_Para* DragonUni_Para;
	u_int8_t labelSetSize;	// 8 in maximum
#define MAX_LABEL_SET_SIZE	8
	u_int32_t* labelSet;
	struct _SessionAttribute_Para* SessionAttribute_Para;
	u_int32_t* upstreamLabel;
	struct _Protection_Para* Protection_Para;
};

struct in_addr srcAddr; 
u_int16_t srcPort;
struct in_addr destAddr;	
u_int16_t destPort;

#define LSP_SAME_SESSION(X, Y) \
	(X->common.Session_Para.srcAddr.s_addr == Y->common.Session_Para.srcAddr.s_addr || \
	 X->common.Session_Para.destAddr.s_addr == Y->common.Session_Para.destAddr.s_addr || \
	 X->common.Session_Para.destPort == Y->common.Session_Para.destPort)

struct _rsvp_upcall_parameter {
	struct in_addr destAddr;	//tunnelAddress
	u_int16_t destPort;		//tunnelID
	struct in_addr srcAddr;	//extendedTunnelID
	u_int16_t srcPort;	//lsp-id
	const char* name;		//Name of the LSP
	u_int32_t upstreamLabel;		//!=0 if bi-dir
	u_int32_t bandwidth;	//bandwidth
	u_int8_t lspEncodingType; //LSP encoding type
	u_int8_t switchingType; // LSP switching type
	u_int16_t gPid;		//G-Pid
	void* sendTSpec;  //Sender TSpec
	void* adSpec;
	void* session;	//RSVP_API::SessionId
	void* senderTemplate;
	u_int8_t code;			//error/success code
};
typedef void (*zUpcall)(void* para);

/* Optional constraints */
struct lsp_optional_constraints {
    u_int32_t max_metric;		/* LSP total metric must be equal or less than this value */
    u_int8_t hop_limit;			/* LSP hop limit */
    list loose_hop_route;		/* Loose hop route that the LSP must traverse */
    list excluded_node;		/* Exclude all nodes in this list */
};

/* NSF DRAGON specific LSP parameters */
struct lsp_dragon_para {
    time_t lsp_start_time;		/* Scheduled establishment time of the LSP */
    time_t lsp_duration;		/* Duration of the LSP */
    u_int32_t AAA;				/* A(uthentication), A(uthorization), and A(ccounting) */
    u_int32_t srcLocalId;         /* Source is (port|tagged-group|tag-group) */
    u_int32_t destLocalId;       /* Destination is (port|untagged-group|tagged-group) */
    u_int32_t lspVtag;       /* LSP E2E VLAN Tag */
};

#define ANY_VTAG 0xffff  /*Indicating that LSP uses any available E2E VL
AN Tag*/

/* Structure of the LSP */
struct lsp {
	u_int8_t status;	/* LSP status */
#define	LSP_EDIT		0		/* This LSP is being edited */
#define	LSP_COMMIT 	1		/* This LSP is being setup */	
#define	LSP_IS 			2		/* This LSP is in service */
#define	LSP_DELETE		3		/* This LSP is in deletion */
#define	LSP_LISTEN 		4		/* This LSP is a receiver  */	
       u_int8_t uni_mode;  /* UNI mode */
#define	UNI_NONE		0		
#define	UNI_CLIENT 		1		
#define	UNI_NETWORK	2		
	u_int32_t flag;	/* LSP parameter flags */
	struct _sessionParameters common;
	struct lsp_optional_constraints constraints;
	struct lsp_dragon_para dragon;
	struct thread *t_lsp_refresh;    /* LSP refresh thread */

	int narb_fd; /* File descriptor (socket) to-from NARB */
	struct thread *t_narb_read; /* LSP packet read thread (for NARB) */
	u_int32_t seqno;  /* Unique sequence number for this LSP request */
};

/* DRAGON fifo element structure. */
struct dragon_fifo_elt {

  struct dragon_fifo_elt *next;

  /* Pointer to data stream. */
  struct stream *s;

  /* The LSP that this fifo element is related */
  struct lsp *lsp;  

};

/* DRAGON packet fifo. */
struct dragon_fifo
{
  unsigned long count;

  struct dragon_fifo_elt *head;
  struct dragon_fifo_elt *tail;
};

/* DRAGON master */
struct dragon_master {

	/* A list of current LSPs */
	list dragon_lsp_table;

	/* Packet fifo */
	struct dragon_fifo *dragon_packet_fifo;

	/* Master of threads. */
	struct thread_master *master;

	/* A list of supported DRAGON modules */
	struct dragon_module *module;

	/* LSP packet write thread */
	struct thread *t_write; 

	int rsvp_fd;	/* File descriptor (socket) to-from RSVP */
	struct thread *t_rsvp_read; /* Global LSP packet read thread (for RSVP) */

	/* Global RSVP API handle */
	void *api; 

};

/* Structure for localID */
struct local_id {
        u_int16_t type;
        u_int16_t value;
        list group;
};
#define LOCAL_ID_TYPE_NONE (u_int16_t)0x0
#define LOCAL_ID_TYPE_PORT (u_int16_t)0x1
#define LOCAL_ID_TYPE_GROUP (u_int16_t)0x2
#define LOCAL_ID_TYPE_TAGGED_GROUP (u_int16_t)0x3
#define LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL (u_int16_t)0x4

/* local_id_group_mapping operators */
void local_id_group_add(struct local_id *lid, u_int16_t  tag);
void local_id_group_delete(struct local_id *lid, u_int16_t  tag);
void local_id_group_free(struct local_id *lid);
void  local_id_group_show(struct vty *vty, struct local_id *lid);

/* Structure for string<-->value conversion */
struct string_value_conversion {
	u_char 	   number;
	struct {
		const char *string;
		u_int32_t	   value;
		u_char 	   len;
	} sv[255];
};

extern u_int32_t string_to_value(struct string_value_conversion *db, const char *str);
extern const char *value_to_string(struct string_value_conversion *db, u_int32_t value);

extern struct dragon_master dmaster;
extern struct thread_master *master; /* master == dmaster.master */
extern struct string_value_conversion conv_protection;
extern struct string_value_conversion conv_swcap;
extern struct string_value_conversion conv_encoding;
extern struct string_value_conversion conv_gpid;
extern struct string_value_conversion conv_bandwidth;
extern struct string_value_conversion conv_rsvp_event;
extern struct string_value_conversion conv_lsp_status;
extern struct host host;

extern int dragon_master_init();
extern void dragon_supp_vty_init ();
extern int is_mandated_params_set_for_lsp(struct lsp *lsp);
extern struct dragon_fifo_elt * dragon_topology_create_msg_new(struct lsp *lsp);
extern struct dragon_fifo_elt * dragon_topology_confirm_msg_new(struct lsp *lsp);
extern struct dragon_fifo_elt * dragon_topology_remove_msg_new(struct lsp *lsp);
extern struct dragon_fifo *dragon_fifo_new ();
extern void dragon_fifo_push (struct dragon_fifo *fifo, struct dragon_fifo_elt *op);
extern struct dragon_fifo_elt * dragon_fifo_pop (struct dragon_fifo *fifo);
extern struct dragon_fifo_elt * dragon_fifo_head (struct dragon_fifo *fifo);
extern void dragon_fifo_flush (struct dragon_fifo *fifo);
extern int  dragon_fifo_count (struct dragon_fifo *fifo);
extern void dragon_fifo_free (struct dragon_fifo *fifo);
extern int dragon_lsp_refresh_timer(struct thread *t);
extern int dragon_read (struct thread *thread);
extern int dragon_write (struct thread *thread);
extern u_int32_t dragon_assign_seqno (void);
extern int dragon_narb_socket_init(void);
extern int dragon_rsvp_read(struct thread *thread);
extern void dragon_show_lsp_detail(struct lsp *lsp, struct vty* vty);
extern void lsp_del(struct lsp *lsp);
extern int dragon_config_write(struct vty *vty);
extern void set_lsp_default_para (struct lsp *lsp);

/* Fiona: from xml */
extern int xml_serv_sock (const char*, unsigned short, char *);

/* The following functions are in libRSVP */
extern void* zInitRsvpApiInstance();
extern void zInitRsvpPathRequest(void* api, struct _sessionParameters* para, u_int8_t isSender);
extern void zTearRsvpPathRequest(void *api, struct _sessionParameters* para);
extern void zTearRsvpResvRequest(void* api, struct _sessionParameters* para);
extern int zGetApiFileDesc(void *api);
extern void zApiReceiveAndProcess(void *api, zUpcall upcall);
extern void zInitRsvpResvRequest(void* api, struct _rsvp_upcall_parameter* upcallPara);
extern void zAddLocalId(void* api, u_int16_t type, u_int16_t value, u_int16_t tag);
extern void zDeleteLocalId(void* api, u_int16_t type, u_int16_t value, u_int16_t tag);

#endif /* _ZEBRA_DRAGOND_H */

