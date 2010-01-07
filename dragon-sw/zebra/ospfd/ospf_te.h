/*
 * This is an implementation of draft-katz-yeung-ospf-traffic-06.txt
 * Copyright (C) 2001 KDD R&D Laboratories, Inc.
 * http://www.kddlabs.co.jp/
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

#ifndef _ZEBRA_OSPF_TE_H
#define _ZEBRA_OSPF_TE_H

/*
 * Opaque LSA's link state ID for Traffic Engineering is
 * structured as follows.
 *
 *        24       16        8        0
 * +--------+--------+--------+--------+
 * |    1   |........|........|........|
 * +--------+--------+--------+--------+
 * |<-Type->|<--      Instance     --->|
 *
 *
 * Type:      IANA has assigned '1' for Traffic Engineering.
 * Instance:  User may select an arbitrary 24-bit value.
 *
 */

#define	LEGAL_TE_INSTANCE_RANGE(i)	(0 <= (i) && (i) <= 0xffffff)
  #define LINK_MAX_PRIORITY				8

/*
 *        24       16        8        0
 * +--------+--------+--------+--------+ ---
 * |   LS age        |Options |   9/10 |  A
 * +--------+--------+--------+--------+  |
 * |    x   |      Instance            |  |  24bit instance 
 * +--------+--------+--------+--------+  |
 * |        Advertising router         |  |  Standard (Opaque) LSA header;
 * +--------+--------+--------+--------+  |  Type-9 / 10 are used.
 * |        LS sequence number         |  |
 * +--------+--------+--------+--------+  |
 * |   LS checksum   |     Length      |  V
 * +--------+--------+--------+--------+ ---
 * |      Type       |     Length      |  A
 * +--------+--------+--------+--------+  |  TLV part for TE; Values might be
 * |              Values ...           |  V  structured as a set of sub-TLVs.
 * +--------+--------+--------+--------+ ---

* Notice that GMPLS draft-ietf-ccamp-ospf-gmpls-extensions-12.txt defines a new Link Local LSA of type 9
*  , opaque type [TBD] and opaque ID (Instance) be zero. 
*/

/* The following is used to identify different types of TE-LSAs */
  #define NOT_TE_LSA 			0
  #define ROUTER_ID_TE_LSA		1
  #define LINK_TE_LSA			2
  #define LINK_LOCAL_TE_LSA		3
  
/*
 * Following section defines TLV (tag, length, value) structures,
 * used for Traffic Engineering.
 */
struct te_tlv_header
{
  u_int16_t	type;			/* TE_TLV_XXX (see below) */
  u_int16_t	length;			/* Value portion only, in octets */
};

#define TLV_HDR_SIZE \
	sizeof (struct te_tlv_header)

#define TLV_BODY_SIZE(tlvh) \
	ROUNDUP (ntohs ((tlvh)->length), sizeof (u_int32_t))

#define TLV_SIZE(tlvh) \
	(TLV_HDR_SIZE + TLV_BODY_SIZE(tlvh))

#define TLV_HDR_TOP(lsah) \
	(struct te_tlv_header *)((char *)(lsah) + OSPF_LSA_HEADER_SIZE)

#define TLV_HDR_NEXT(tlvh) \
	(struct te_tlv_header *)((char *)(tlvh) + TLV_SIZE(tlvh))

#define SUBTLV_HDR_TOP(tlvh) \
	(struct te_tlv_header *)((char *)(tlvh) + TLV_HDR_SIZE)

#define SUBTLV_HDR_NEXT(sub_tlvh) \
	(struct te_tlv_header *)((char *)(sub_tlvh) + TLV_SIZE(sub_tlvh))

/*
 * Following section defines TLV body parts.
 */
/* Router Address TLV *//* Mandatory */
#define	TE_TLV_ROUTER_ADDR		1	/* Router Address TLV uses type-10 LSA */
struct te_tlv_router_addr
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  struct in_addr	value;
};

/* Link TLV */
#define	TE_TLV_LINK			2		/* Link TLV uses type-10 LSA */
struct te_tlv_link
{
  struct te_tlv_header	header;
  /* A set of link-sub-TLVs will follow. */
};

/* Link Local Identifier TLV *//* GMPLS draft-ietf-ccamp-ospf-gmpls-extensions-12.txt */
#define	TE_TLV_LINK_LOCAL_ID		1 		/* Link local Identifier uses type-9 LSA */
struct te_tlv_link_local_id
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  u_int32_t	value;
};


/* Link Type Sub-TLV *//* Mandatory */
#define	TE_LINK_SUBTLV_LINK_TYPE		1
struct te_link_subtlv_link_type
{
  struct te_tlv_header	header;		/* Value length is 1 octet. */
  struct {
#define	LINK_TYPE_SUBTLV_VALUE_PTP	1
#define	LINK_TYPE_SUBTLV_VALUE_MA	2
      u_char	value;
      u_char	padding[3];
  } link_type;
};

/* Link Sub-TLV: Link ID *//* Mandatory */
#define	TE_LINK_SUBTLV_LINK_ID			2
struct te_link_subtlv_link_id
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  struct in_addr	value;		/* Same as router-lsa's link-id. */
};

/* Link Sub-TLV: Local Interface IP Address *//* Optional */
#define	TE_LINK_SUBTLV_LCLIF_IPADDR		3
struct te_link_subtlv_lclif_ipaddr
{
  struct te_tlv_header	header;		/* Value length is 4 x N octets. */
  struct in_addr	value;	/* Local IP address. Only one address is allowed, due to Zebra restrictions */
};

/* Link Sub-TLV: Remote Interface IP Address *//* Optional */
#define	TE_LINK_SUBTLV_RMTIF_IPADDR		4
struct te_link_subtlv_rmtif_ipaddr
{
  struct te_tlv_header	header;		/* Value length is 4 x N octets. */
  struct in_addr	value;	/* Neighbor's IP address(es). Only one address is allowed, due to Zebra restrictions */
};

/* Link Sub-TLV: Traffic Engineering Metric *//* Optional */
#define	TE_LINK_SUBTLV_TE_METRIC		5
struct te_link_subtlv_te_metric
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  u_int32_t		value;		/* Link metric for TE purpose. */
};

/* Link Sub-TLV: Maximum Bandwidth *//* Optional */
#define	TE_LINK_SUBTLV_MAX_BW			6
struct te_link_subtlv_max_bw
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  float			value;		/* bytes/sec */
};

/* Link Sub-TLV: Maximum Reservable Bandwidth *//* Optional */
#define	TE_LINK_SUBTLV_MAX_RSV_BW		7
struct te_link_subtlv_max_rsv_bw
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  float			value;		/* bytes/sec */
};

/* Link Sub-TLV: Unreserved Bandwidth *//* Optional */
#define	TE_LINK_SUBTLV_UNRSV_BW			8
struct te_link_subtlv_unrsv_bw
{
  struct te_tlv_header	header;		/* Value length is 32 octets. */
  float			value[8];	/* One for each priority level. */
};

/* Link Sub-TLV: Administrative Group, a.k.a. Resource Class/Color *//* Optional */
#define	TE_LINK_SUBTLV_RSC_CLSCLR		9
struct te_link_subtlv_rsc_clsclr
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  u_int32_t		value;		/* Admin. group membership. */
};

/* Link Sub-TLV: Link Local/Remote Identifiers *//* GMPLS draft-ietf-ccamp-ospf-gmpls-extensions-12.txt*/
#define	TE_LINK_SUBTLV_LINK_LCRMT_ID		11
struct te_link_subtlv_link_lcrmt_id
{
  struct te_tlv_header	header;		/* Value length is 8 octets. */
  u_int32_t		link_local_id;		/* Link Local Identifier*/
  u_int32_t		link_remote_id;		/* Link Remote Identifier*/
};

/* Link Sub-TLV: Link Protection Type *//* GMPLS draft-ietf-ccamp-ospf-gmpls-extensions-12.txt*/
#define	TE_LINK_SUBTLV_LINK_PROTYPE		14
struct te_link_subtlv_link_protype
{
  struct te_tlv_header	header;		/* Value length is 4 octets. */
  struct {
#define	LINK_PROTYPE_SUBTLV_VALUE_EXTRA		0x01
#define	LINK_PROTYPE_SUBTLV_VALUE_UNPRO		0x02
#define	LINK_PROTYPE_SUBTLV_VALUE_SHARED	0x04
#define	LINK_PROTYPE_SUBTLV_VALUE_1TO1		0x08
#define	LINK_PROTYPE_SUBTLV_VALUE_1PLUS1	0x10
#define	LINK_PROTYPE_SUBTLV_VALUE_ENHANCED	0x20
#define	LINK_PROTYPE_SUBTLV_VALUE_RESV1		0x40
#define	LINK_PROTYPE_SUBTLV_VALUE_RESV2		0x80
      u_char	value;
      u_char	padding[3];
  } value4;
};

/* Link Sub-TLV / Switching Capability-specific information: PSC-1, PSC-2, PSC-3, or PSC-4 */
struct link_ifswcap_specific_psc {
	float		 	min_lsp_bw;
	u_int16_t		mtu;
	u_char		padding[2];
};

/* Link Sub-TLV / Switching Capability-specific information: TDM */
struct link_ifswcap_specific_tdm {
	float	 		min_lsp_bw;
	u_char		indication;
	u_char		padding[3];
};

#define STD_ISCD_LENGTH 44 // without header

/* Link Sub-TLV / Switching Capability-specific information: VLAN/Ethernet*/
#define MAX_VLAN_NUM 4096 /* Maximum number of available vlan's that a port/IP is assigned to */
#define IFSWCAP_SPECIFIC_VLAN_BASIC 0x0002
#define IFSWCAP_SPECIFIC_VLAN_ALLOC 0x0004
#define IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z 0x8000
struct link_ifswcap_specific_vlan {
	u_int16_t		length;
	u_int16_t	 	version;  /*version id and options mask*/
	u_char           bitmask[MAX_VLAN_NUM/8];
	u_char           bitmask_alloc[MAX_VLAN_NUM/8];
};
#define HAS_VLAN(P, VID) ((P[(VID-1)/8] & (0x80 >> (VID-1)%8)) != 0)
#define SET_VLAN(P, VID) P[(VID-1)/8] = (P[(VID-1)/8] | (0x80 >> (VID-1)%8))
#define RESET_VLAN(P, VID) P[(VID-1)/8] = (P[(VID-1)/8] & ~(0x80 >> (VID-1)%8))

#define HAS_TIMESLOT HAS_VLAN
#define SET_TIMESLOT SET_VLAN
#define RESET_TIMESLOT RESET_VLAN

/* Link Sub-TLV / Switching Capability-specific information: VLAN/Ethernet via Subnet-UNI */
#define IFSWCAP_SPECIFIC_SUBNET_UNI 0x4000

/* The following options depends on IFSWCAP_SPECIFIC_SUBNET_UNI being set */
#define IFSWCAP_SPECIFIC_SUBNET_CIEN 0x0100
#define IFSWCAP_SPECIFIC_SUBNET_INFN 0x0200
#define IFSWCAP_SPECIFIC_SUBNET_JNPR 0x0400
#define IFSWCAP_SPECIFIC_SUBNET_CSCO 0x0800
/* When this option set, timeslots on the interface has to be allocated contiguously for each circuit. */
#define IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS 0x1000
/* Ciena  Switching Capability-specific */
#define IFSWCAP_SPECIFIC_CIENA_OPVCX 0x0010

#define MAX_TIMESLOTS_NUM 192 /* 192 STS-1 = 10Gbps */
struct link_ifswcap_specific_subnet_uni {
	u_int16_t		length;
	u_int16_t	 	version;       /*version id and options mask | IFSWCAP_SPECIFIC_SUBNET_UNI*/
	u_int8_t		subnet_uni_id;
	u_int8_t		first_timeslot;
	u_char		swcap_ext;
	u_char		encoding_ext;
	u_int32_t		tna_ipv4;
	u_int32_t		nid_ipv4;
	u_int32_t		data_ipv4;
	u_int32_t		logical_port_number;
	u_int32_t		egress_label_downstream; /*egress label on the UNI interface*/ /*to be removed*/
	u_int32_t		egress_label_upstream; /*egress label on the UNI interface for bidirectional traffic*/ /*to be removed*/
	char			control_channel[12];
	char			node_name[16]; /* In Ciena subnet, this is the node name in OSRP. */
	u_int8_t		timeslot_bitmask[MAX_TIMESLOTS_NUM/8]; /*time slots available = 1*/
};

struct wavelength_grid_label {
	u_int32_t		grid_type:3; /*Grid type: 1=ITU-T_DWDM 2=ITU-T_CWDM*/
	u_int32_t	 	spacing:3; /*Channel spacing: 1=100, 2=50*/
	u_int32_t	 	reserved:9;
	int			wavelength:17; /*Wavelength channel number*/
};

#define MAX_SUBWAVE_CHANNELS 256 /*64: 10G with OPVCX; 256: 40G OPVCX */
/*TODO: make NUM_SUBWAVE_CHANNELS a configurable constant*/
struct link_ifswcap_specific_ciena_opvcx {
	u_int16_t		length;
	u_int16_t	 	version;       /*IFSWCAP_SPECIFIC_CIENA_OPVCX*/
	u_int32_t		switch_ip;
	u_int16_t		tl1_port;
	u_int8_t		eth_edge; /* 1 = true, 0 = false */
	u_int8_t		reserved;
	u_int32_t		data_ipv4;
	u_int32_t		logical_port_number;
	u_int16_t 	num_waves; /*number of wavelengths*/
	u_int16_t 	num_chans; /*number of sub-wavelength channels = NUM_SUBWAVE_CHANNELS*/
	struct {
		struct wavelength_grid_label wave_id; /* default = all 0 for single-wave TDM (non-WDM) */
		u_int8_t opvc_bitmask[MAX_SUBWAVE_CHANNELS/8]; /* bit =1 means available */
	} wave_opvc_map[1]; /* num_waves blocks */
};

/*#define NUM_ONTCG_GROUPS 12*/

/* Link Sub-TLV: Interface Switching Capability Descriptor */
/* GMPLS draft-ietf-ccamp-ospf-gmpls-extensions-12.txt*/
#define	TE_LINK_SUBTLV_LINK_IFSWCAP		15
struct te_link_subtlv_link_ifswcap
{
  struct te_tlv_header	header;		/* Value length is variable length octets. */
  struct {
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC1		1
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC2		2
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC3		3 
#define LINK_IFSWCAP_SUBTLV_SWCAP_PSC4		4
#define LINK_IFSWCAP_SUBTLV_SWCAP_L2SC		51
#define LINK_IFSWCAP_SUBTLV_SWCAP_TDM		100
#define LINK_IFSWCAP_SUBTLV_SWCAP_LSC		150
#define LINK_IFSWCAP_SUBTLV_SWCAP_FSC		200
       u_char	switching_cap;

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
#define LINK_IFSWCAP_SUBTLV_ENC_G709ODUK	12
#define LINK_IFSWCAP_SUBTLV_ENC_G709OCH		13

	u_char	encoding;

       u_char	reserved[2];
	float max_lsp_bw_at_priority[LINK_MAX_PRIORITY];
       union {
		struct link_ifswcap_specific_psc  ifswcap_specific_psc;
		struct link_ifswcap_specific_tdm ifswcap_specific_tdm; 
       	struct link_ifswcap_specific_vlan ifswcap_specific_vlan; 
       	struct link_ifswcap_specific_subnet_uni ifswcap_specific_subnet_uni;
		struct link_ifswcap_specific_ciena_opvcx ifswcap_specific_ciena_opvcx;
       } ifswcap_specific_info;
  } link_ifswcap_data;
  /* More Switching Capability-specific information, defined below */
};

/* Link Sub-TLV: Shared Risk Link Group *//* GMPLS draft-ietf-ccamp-ospf-gmpls-extensions-12.txt*/
#define	TE_LINK_SUBTLV_LINK_SRLG			16
struct te_link_subtlv_link_srlg
{
  struct te_tlv_header	header;		/* Value length is variable-length octets. */
#define LINK_SRLG_ADD_VALUE		1
#define LINK_SRLG_DEL_VALUE		0
  list		srlg_list;	/* SRLG value, each of which is 4-octet */
};

/* DRAGON specific TLVs*/

#define	TE_LINK_SUBTLV_LINK_DOMAIN_ID	34832 /* or 0x8810 = DRAGO_BASE+0x0010*/
#define	TE_LINK_SUBTLV_LINK_TE_LAMBDA	35073 /* or 0x8901 = DRAGO_BASE+0x0101*/
#define	TE_LINK_SUBTLV_LINK_DRAGON_GRI	35074 /* or 0x8902 = DRAGO_BASE+0x0102*/

struct te_link_subtlv_link_te_lambda
{
  struct te_tlv_header	header;		/* Value length is variable-length octets. */
  u_int32_t	frequency;			/*Lambda in frequency format*/
};

struct te_link_subtlv_link_domain_id
{
  struct te_tlv_header	header;
  u_int32_t	domain_id;			/*unsigned long*/
};

enum sched_opcode {
  REORIGINATE_THIS_LSA, REFRESH_THIS_LSA, FLUSH_THIS_LSA
};

/* Structure for string<-->value conversion */
struct str_val_conv {
	u_char 	   number;
	struct {
		const char *string;
		u_int32_t	   value;
		u_char 	   len;
	} sv[255];
};

extern struct str_val_conv str_val_conv_protection;
extern struct str_val_conv str_val_conv_swcap;
extern struct str_val_conv str_val_conv_encoding;

extern u_int32_t channel2frequency(char* channel);
extern u_int32_t wavelength2frequency(char* wavelength);
extern const char* frequency2wavelength(u_int32_t frequency);

extern const char* logical_port_number2string(u_int32_t port_id);
extern u_int32_t logical_port_string2number(const char* port_str);

#define INTERFACE_MPLS_ENABLED(X) \
	X->te_enabled >= INTERFACE_TE_MPLS

#define INTERFACE_GMPLS_ENABLED(X) \
	X->te_enabled == INTERFACE_TE_GMPLS

#define VLSR_INITIAL_LCL_IFID	0x80000001
#define VLSR_MAX_LCL_IFID		0x8FFFFFFF
#define IS_VALID_LCL_IFID(X)	 \
	(X >= VLSR_INITIAL_LCL_IFID && X <= VLSR_MAX_LCL_IFID)


#define ZBUFSIZE 1024
extern u_char z_buffer[ZBUFSIZE+1];

/* te_area_lsa_para is present for each ospf interface 
  * te_area_lsa_para is present for each type-10 ospf_te_lsa
*/
struct te_area_lsa_para
{
  u_int32_t		instance;		/* Instance (last 24bits of the instance field) = zebra interface index */
  struct te_tlv_link link;
  struct te_link_subtlv_link_type    link_type;
  struct te_link_subtlv_link_id      link_id;
  struct te_link_subtlv_lclif_ipaddr lclif_ipaddr;   /* ONLY one IP address is allowed in this implementation, due to the restriction of Zebra */
  struct te_link_subtlv_rmtif_ipaddr rmtif_ipaddr;  /* ONLY one IP address is allowed in this implementation, due to the restriction of Zebra */
  struct te_link_subtlv_te_metric te_metric;
  struct te_link_subtlv_max_bw max_bw;
  struct te_link_subtlv_max_rsv_bw max_rsv_bw;
  struct te_link_subtlv_unrsv_bw unrsv_bw;
  struct te_link_subtlv_rsc_clsclr rsc_clsclr;
  struct te_link_subtlv_link_lcrmt_id link_lcrmt_id;
  struct te_link_subtlv_link_protype  link_protype;
  list   link_ifswcap_list;  /*$$$$ Support multiple ISCDs in one LSA*/ 
  /* struct te_link_subtlv_link_ifswcap link_ifswcap; */
  struct te_link_subtlv_link_srlg	link_srlg;   /* length is variable */
  struct te_link_subtlv_link_te_lambda	 link_te_lambda;  
};

/* te_link_local_lsa_para is present for each type-9 ospf_te_lsa */
struct te_link_local_lsa_para
{
	/* Instace field must be zero */
	struct te_tlv_link_local_id link_local_id;
};

#define VLSR_PROTO_NONE		0
#define VLSR_PROTO_SNMP		1
#define VLSR_PROTO_TL1			2
struct vlsr_if
{
	u_int8_t protocol;				/* Protocol type */
	u_int32_t if_id;				/* The unnumbered interface ID assigned by OSPF-TE */
	struct in_addr data_ip;		/* The data IP representing the TE link */
	struct in_addr switch_ip;		/* The switch's control IP address */
	u_int32_t switch_port;			/* The switch's port */
	u_char vtag_bitmask[MAX_VLAN_NUM/8]; /*Each bit per VLAN ID */
};

/* A group of parameters for OSPF-TE VTY config */
struct ospf_te_config_para
{
	u_int8_t configed; /*1: TE link 2: UNI */
	char if_name[20]; /* interface name */
	struct vlsr_if vlsr_if;
	u_char level;  /* MPLS or GMPLS */
	struct te_area_lsa_para te_para;
};

/* DRAGON GRI struct */
struct dragon_gri_para
{
	u_int32_t timestamp;
	u_int32_t ucid;
	u_int32_t seqnum;
};
#define MAX_GRI_AGE 180
extern void insert_gri(struct ospf_interface* oi, u_int32_t ucid, u_int32_t seqnum);

/* Prototypes. */
extern struct te_tlv_router_addr OspfTeRouterAddr;
extern int ospf_te_init (void);
extern void ospf_te_term (void);
extern void ospf_te_new_if (struct ospf_interface *oi);
extern void ospf_te_del_if (struct ospf_interface *oi);
extern int ospf_te_verify_config(struct ospf_interface *oi, struct ospf_te_config_para *oc);
extern void ospf_te_ism_change (struct ospf_interface *oi, int old_status);
extern void ospf_te_nsm_change (struct ospf_neighbor *nbr, int old_status);
extern void set_ospf_te_router_addr (struct in_addr ipv4);
extern struct prefix * get_if_ip_addr(struct interface *ifp);
extern void ospf_rsvp_init ();
extern void set_linkparams_unrsv_bw (struct te_link_subtlv_unrsv_bw *para, int priority, float *fp);

#endif /* _ZEBRA_OSPF_TE_H */
