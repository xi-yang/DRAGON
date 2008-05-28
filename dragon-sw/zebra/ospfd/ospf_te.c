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

#include <zebra.h>

#ifdef HAVE_OSPF_TE
#ifndef HAVE_OPAQUE_LSA
#error "Wrong configure option"
#endif /* HAVE_OPAQUE_LSA */

#include <zlib.h>

#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "memory.h"
#include "command.h"
#include "vty.h"
#include "stream.h"
#include "log.h"
#include "thread.h"
#include "hash.h"
#include "sockunion.h"		/* for inet_aton() */

#include "ospfd/ospfd.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_te_lsa.h"
#include "ospfd/ospf_te_lsdb.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_ase.h"
#include "ospfd/ospf_zebra.h"

/* Store Router-TLV in network byte order. */
struct te_tlv_router_addr OspfTeRouterAddr;
char OspfTeIfPrompt[50] = "%s(te-if)# ";

list OspfTeConfigList = NULL;
list OspfTeUniConfigList = NULL;

struct cmd_node ospf_te_link_node =
{
  OSPF_TE_IF_NODE,
  OspfTeIfPrompt,
  1
};

struct cmd_node ospf_te_uni_node =
{
  OSPF_TE_UNI_NODE,
  OspfTeIfPrompt,
  1
};

struct str_val_conv str_val_conv_protection = 
{
	6,
	{{ "extra", 	LINK_PROTYPE_SUBTLV_VALUE_EXTRA, 		2}, 	
	  { "none", 	LINK_PROTYPE_SUBTLV_VALUE_UNPRO, 		1}, 
	  { "shared", 	LINK_PROTYPE_SUBTLV_VALUE_SHARED, 		1},
	  { "1t1", 		LINK_PROTYPE_SUBTLV_VALUE_1TO1, 		2}, 
	  { "1p1", 	LINK_PROTYPE_SUBTLV_VALUE_1PLUS1, 		2}, 
	  {"en", 		LINK_PROTYPE_SUBTLV_VALUE_ENHANCED , 	2}}
};

struct str_val_conv str_val_conv_swcap = 
{
	8,
	{{ "psc1", 	LINK_IFSWCAP_SUBTLV_SWCAP_PSC1, 		4},
	{ "psc2", 		LINK_IFSWCAP_SUBTLV_SWCAP_PSC2, 		4}, 
	{ "psc3", 		LINK_IFSWCAP_SUBTLV_SWCAP_PSC3, 		4}, 
	{ "psc4", 		LINK_IFSWCAP_SUBTLV_SWCAP_PSC4, 		4},
	{ "l2sc", 		LINK_IFSWCAP_SUBTLV_SWCAP_L2SC, 		2},
	{ "tdm", 		LINK_IFSWCAP_SUBTLV_SWCAP_TDM, 		1}, 
	{ "lsc", 		LINK_IFSWCAP_SUBTLV_SWCAP_LSC, 		2}, 
	{ "fsc", 		LINK_IFSWCAP_SUBTLV_SWCAP_FSC, 		1}}
};

struct str_val_conv str_val_conv_encoding = 
{
	8,
	{{ "packet", 	LINK_IFSWCAP_SUBTLV_ENC_PKT, 			2}, 
	{ "ethernet", 	LINK_IFSWCAP_SUBTLV_ENC_ETH, 			1}, 
	{ "pdh", 		LINK_IFSWCAP_SUBTLV_ENC_PDH, 			2}, 
	{ "sdh", 		LINK_IFSWCAP_SUBTLV_ENC_SONETSDH, 		1},
	{ "dwrapper", LINK_IFSWCAP_SUBTLV_ENC_DIGIWRAP, 		1}, 
	{ "lambda", 	LINK_IFSWCAP_SUBTLV_ENC_LAMBDA, 		1}, 
	{ "fiber", 		LINK_IFSWCAP_SUBTLV_ENC_FIBER, 			2}, 
	{ "fchannel", 	LINK_IFSWCAP_SUBTLV_ENC_FIBRCHNL, 		2}}
};


struct str_val_conv str_val_conv_wavelength = 
{
	40,
	{{ "1560.61", 	192100, 			7}, 
	{ "1559.79", 	192200, 			7}, 
	{ "1558.98", 	192300, 			7}, 
	{ "1558.17", 	192400, 			7}, 
	{ "1557.36", 	192500, 			7}, 
	{ "1556.55", 	192600, 			7}, 
	{ "1555.74", 	192700, 			7}, 
	{ "1554.94", 	192800, 			7}, 
	{ "1554.13", 	192900, 			7}, 
	{ "1553.33", 	193000, 			7}, 
	{ "1552.52", 	193100, 			7}, 
	{ "1551.72", 	193200, 			7}, 
	{ "1550.92", 	193300, 			7}, 
	{ "1550.12", 	193400, 			7}, 
	{ "1549.31", 	193500, 			7}, 
	{ "1548.51", 	193600, 			7}, 
	{ "1547.71", 	193700, 			7}, 
	{ "1546.92", 	193800, 			7}, 
	{ "1546.12", 	193900, 			7}, 
	{ "1545.32", 	194000, 			7}, 
	{ "1544.52", 	194100, 			7}, 
	{ "1543.73", 	194200, 			7}, 
	{ "1542.94", 	194300, 			7}, 
	{ "1542.14", 	194400, 			7}, 
	{ "1541.35", 	194500, 			7}, 
	{ "1540.56", 	194600, 			7}, 
	{ "1539.77", 	194700, 			7}, 
	{ "1538.98", 	194800, 			7}, 
	{ "1538.19", 	194900, 			7}, 
	{ "1537.40", 	195000, 			7}, 
	{ "1536.61", 	195100, 			7}, 
	{ "1535.82", 	195200, 			7}, 
	{ "1535.04", 	195300, 			7}, 
	{ "1534.25", 	195400, 			7}, 
	{ "1533.46", 	195500, 			7}, 
	{ "1532.68", 	195600, 			7}, 
	{ "1531.90", 	195700, 			7}, 
	{ "1531.12", 	195800, 			7}, 
	{ "1530.33", 	195900, 			7}, 
	{ "1529.55", 	196000, 			7}}
};
	
/* Flag to indicate whether the parameters are changed ever since last configuration */ 
struct ospf_te_config_para te_config;

/*------------------------------------------------------------------------*/
u_char z_buffer[ZBUFSIZE+1];

/*------------------------------------------------------------------------*
 * Followings are initialize/terminate functions for OSPF-TE handling.
 *------------------------------------------------------------------------*/
static void ospf_te_register_vty (void);
static void ospf_te_config_write (struct vty *vty);
static void ospf_te_show_info (struct vty *vty, struct ospf_lsa *lsa);

#ifndef _str2val_funcs_
#define _str2val_funcs_
u_int32_t 
str2val(struct str_val_conv *db, const char *str)
{
	int i;
	
	for (i=0; i<db->number; i++)
	{
		if (strncmp(db->sv[i].string, str, db->sv[i].len)==0)
			return db->sv[i].value;
	}
	return 0;
}

const char * 
val2str(struct str_val_conv *db, u_int32_t value)
{
	int i;
	static const char* def_string = "Unknown";
	
	for (i=0; i<db->number; i++)
	{
		if (db->sv[i].value == value)
			return db->sv[i].string;
	}
	return def_string;
}
#endif

u_int32_t channel2frequency(char* channel)
{
	int chan_id;
	sscanf(channel, "%d", &chan_id);
	if (chan_id > 20 && chan_id < 61)
		return ((chan_id-20)*100+192000);
	return 0;
}

u_int32_t wavelength2frequency(char* wavelength)
{
	return str2val(&str_val_conv_wavelength, (const char*)wavelength);
}

const char* frequency2wavelength(u_int32_t frequency)
{
	return val2str(&str_val_conv_wavelength, frequency);
}

const char* logical_port_number2string(u_int32_t port_id)
{
	static char buf[10];
	int bay, shelf, slot, subslot, port;
	char shelf_alpha;

	bay = ((port_id)>>24)+1;
	shelf = (((port_id)>>16)&0xff);
	slot = (((port_id)>>12)& 0x0f)+1;
	subslot = (((port_id)>>8)&0x0f)+1;
	port = ((port_id)&0xff)+1;

	switch (shelf)
	{
	case 2:
		shelf_alpha = 'A';
		break;
	case 3:
		shelf_alpha = 'C';
		break;
	default:
		shelf_alpha = 'X';
		break;
	}
	sprintf(buf, "%d-%c-%d-%d-%d", bay, shelf_alpha, slot, subslot, port);
	return (const char*)buf;
}

u_int32_t logical_port_string2number(const char* port_str)
{
	u_int32_t bay, shelf, slot, subslot, port;
	char shelf_alpha;

	sscanf(port_str, "%d-%c-%d-%d-%d", &bay, &shelf_alpha, &slot, &subslot, &port);
	bay--; slot--; subslot--; port--;
	switch (shelf_alpha)
	{
	case 'a':
	case 'A':
		shelf = 2;
		break;
	case 'c':
	case 'C':
		shelf = 3;
		break;
	default:
		return 0;
		break;
	}
	return ((bay<<24) | (shelf <<16) | (slot << 12) | (subslot<<8) | (port&0xff) );
}

static void
ospf_te_config_para_del(struct ospf_te_config_para *oc)
{
     if (oc->te_para.link_srlg.srlg_list)
     	list_delete(oc->te_para.link_srlg.srlg_list);
     XFREE (MTYPE_OSPF_TMP, oc);
}

static int
get_switch_port_by_name(char* port_name, u_int32_t* switch_port)
{
	u_int32_t shelf, slot, port;
	if (strstr(port_name, "-"))
	{
		if (sscanf(port_name, "%d-%d-%d", &shelf, &slot, &port) == 3)
		{
			*switch_port = ((shelf&0xf) << 12) | ((slot&0xf)<<8) | (port&0xff);
			return 1;
		}
		else
			return -1;
	}
	else if (strstr(port_name, "/"))
	{
		if (sscanf(port_name, "%d/%d/%d", &shelf, &slot, &port) == 3)
		{
			*switch_port = ((shelf&0xf) << 12) | ((slot&0xf)<<8) | (port&0xff);
			return 1;
		}
		else
			return -1;
	}
	else if (sscanf(port_name, "%d", switch_port) == 1)
	{
		return 1;
	}

	return -1;
}

static char* 
get_switch_port_string(u_int32_t switch_port)
{
	static char port_string[20];
	u_int32_t shelf, slot, port;

	sprintf(port_string, "%d-%d-%d", (switch_port>>12)&0xf, (switch_port>>8)&0xf, switch_port&0xff);
	return port_string;
}

int
ospf_te_init (void)
{
  int rc;
  
/* Register functions for type-10 TE-LSA */
  rc = ospf_register_opaque_functab (
                OSPF_OPAQUE_AREA_LSA,
                OPAQUE_TYPE_TE_AREA_LSA,
		NULL, /*ospf_te_new_if,*/		/* ospf_te_new_if() is called by specific MPLS functions */
		NULL, /* ospf_te_del_if, */		/* ospf_te_del_if() is called by specific MPLS functions */
		ospf_te_ism_change,
		ospf_te_nsm_change,
		ospf_te_config_write,
		NULL, /*ospf_te_config_write_if, */
		NULL, /* ospf_te_config_write_debug */
                ospf_te_show_info,
                NULL, /*ospf_te_area_lsa_originate, */
                NULL, /* ospf_te_area_lsa_refresh, */
		NULL, /* ospf_te_new_lsa_hook */
		NULL  /* ospf_te_del_lsa_hook */);
  if (rc != 0)
    {
      zlog_warn ("ospf_te_init: Failed to register functions for OSPF-TE");
      goto out;
    }

/* Register functions for type-9 GMPLS TE-LSA */
  rc = ospf_register_opaque_functab (
                OSPF_OPAQUE_LINK_LSA,
                OPAQUE_TYPE_TE_LINKLOCAL_LSA,
		NULL, /*ospf_te_new_if,*/		/* ospf_te_new_if() is called by specific MPLS functions */
		NULL, /* ospf_te_del_if, */		/* ospf_te_del_if() is called by specific MPLS functions */
		ospf_te_ism_change,
		ospf_te_nsm_change,
		NULL, /*ospf_te_config_write_router, */
		NULL, /*ospf_te_config_write_if, */
		NULL, /* ospf_te_config_write_debug */
                ospf_te_show_info,
              NULL, /*  ospf_te_linklocal_lsa_originate, */
              NULL, /*  ospf_te_linklocal_lsa_refresh, */
		NULL, /* ospf_te_new_lsa_hook */
		NULL  /* ospf_te_del_lsa_hook */);
  if (rc != 0)
    {
      zlog_warn ("ospf_te_init: Failed to register functions for OSPF-TE");
      goto out;
    }

  ospf_te_register_vty ();

  memset(&OspfTeRouterAddr, 0, sizeof(struct te_tlv_router_addr)); 
  OspfTeConfigList = list_new();
  OspfTeConfigList->del = (void (*) (void *))ospf_te_config_para_del;
  OspfTeUniConfigList = list_new();
  OspfTeUniConfigList->del = (void (*) (void *))ospf_te_config_para_del;
  
out:
  return rc;
}

void
ospf_te_term (void)
{
	ospf_delete_opaque_functab (OSPF_OPAQUE_AREA_LSA,
			       	                      OPAQUE_TYPE_TE_AREA_LSA);
	ospf_delete_opaque_functab (OSPF_OPAQUE_LINK_LSA,
			       	                      OPAQUE_TYPE_TE_LINKLOCAL_LSA);
  memset(&OspfTeRouterAddr, 0, sizeof(struct te_tlv_router_addr));
  if (OspfTeConfigList)
  	list_delete(OspfTeConfigList);
  if (OspfTeUniConfigList)
  	list_delete(OspfTeUniConfigList);
  return;
}

/*------------------------------------------------------------------------*
 * Followings are control functions for OSPF-TE parameters management.
 *------------------------------------------------------------------------*/

struct prefix * 
get_if_ip_addr(struct interface *ifp)
{
	listnode cnode;
	struct connected *c;

	LIST_LOOP(ifp->connected, c, cnode)
	{
		if (c->address)
		{
			return (c->address);
	  	}
  	}
	return NULL;
}

void
set_ospf_te_router_addr (struct in_addr ipv4)
{
  OspfTeRouterAddr.header.type   = htons (TE_TLV_ROUTER_ADDR);
  OspfTeRouterAddr.header.length = htons (sizeof (ipv4));
  OspfTeRouterAddr.value = ipv4;
  return;
}

static void
set_linkparams_link_type (struct ospf_interface *oi)
{
  oi->te_para.link_type.header.type   = htons (TE_LINK_SUBTLV_LINK_TYPE);
  oi->te_para.link_type.header.length = htons (sizeof (oi->te_para.link_type.link_type.value));

  switch (oi->type)
    {
    case OSPF_IFTYPE_POINTOPOINT:
      oi->te_para.link_type.link_type.value = LINK_TYPE_SUBTLV_VALUE_PTP;
      break;
    case OSPF_IFTYPE_BROADCAST:
    case OSPF_IFTYPE_NBMA:
      oi->te_para.link_type.link_type.value = LINK_TYPE_SUBTLV_VALUE_MA;
      break;
    default:
      /* Not supported yet. *//* XXX */
      oi->te_para.link_type.header.type = htons (0);
      break;
    }
  return;
}

static void
set_linkparams_link_id (struct ospf_interface *oi)
{
  struct ospf_neighbor *nbr;
  int done = 0;

  oi->te_para.link_id.header.type   = htons (TE_LINK_SUBTLV_LINK_ID);
  oi->te_para.link_id.header.length = htons (sizeof (oi->te_para.link_id.value));

  /*
   * The Link ID is identical to the contents of the Link ID field
   * in the Router LSA for these link types.
   */
  switch (oi->type)
    {
    case OSPF_IFTYPE_POINTOPOINT:
      /* Take the router ID of the neighbor. */
      if ((nbr = ospf_nbr_lookup_ptop (oi))
	  && nbr->state == NSM_Full)
        {
          oi->te_para.link_id.value = nbr->router_id;
          done = 1;
        }
      break;
    case OSPF_IFTYPE_BROADCAST:
    case OSPF_IFTYPE_NBMA:
      /* Take the interface address of the designated router. */
      if ((nbr = ospf_nbr_lookup_by_addr (oi->nbrs, &DR (oi))) == NULL)
        break;

      if (nbr->state == NSM_Full
      || (IPV4_ADDR_SAME (&oi->address->u.prefix4, &DR (oi))
      &&  ospf_nbr_count (oi, NSM_Full) > 0))
        {
          oi->te_para.link_id.value = DR (oi);
          done = 1;
        }
      break;
    default:
      /* Not supported yet. *//* XXX */
      oi->te_para.link_id.header.type = htons (0);
      break;
    }

  if (! done)
    {
      struct in_addr mask;
      masklen2ip (oi->address->prefixlen, &mask);
      oi->te_para.link_id.value.s_addr = oi->address->u.prefix4.s_addr & mask.s_addr;
     }
  return;
}

static void set_linkparams_lclif_addr(struct ospf_interface *oi)
{
   if(ospf_if_is_enable (oi)){
   	if (INTERFACE_GMPLS_ENABLED(oi))
   	{
   		if (oi->vlsr_if.data_ip.s_addr != 0)
		{
			oi->te_para.lclif_ipaddr.header.type = htons (TE_LINK_SUBTLV_LCLIF_IPADDR);
			oi->te_para.lclif_ipaddr.header.length = htons (sizeof(struct in_addr));
			oi->te_para.lclif_ipaddr.value.s_addr = oi->vlsr_if.data_ip.s_addr;
   		}
   	}
	else if (INTERFACE_MPLS_ENABLED(oi))
		oi->te_para.lclif_ipaddr.value.s_addr = oi->address->u.prefix4.s_addr;
   }
   return;
}

static void set_linkparams_rmtif_addr(struct ospf_interface *oi)
{
    struct ospf_neighbor *nbr;
    struct in_addr mask;
    struct in_addr addr;

    if(oi->type == OSPF_IFTYPE_POINTOPOINT)
    {
      /* Take the router ID of the neighbor. */
      if (((nbr = ospf_nbr_lookup_ptop (oi)) && (nbr->state == NSM_Full))
      	     && ntohs(oi->te_para.lclif_ipaddr.header.type) != 0 )
      {
	   oi->te_para.rmtif_ipaddr.header.type = htons (TE_LINK_SUBTLV_RMTIF_IPADDR);
	   oi->te_para.rmtif_ipaddr.header.length = htons (sizeof(struct in_addr));
	   masklen2ip (IPV4_ALLOWABLE_BITLEN_P2P, &mask);
	   addr.s_addr = oi->te_para.lclif_ipaddr.value.s_addr & mask.s_addr; /* Network address */
	   if (htonl(ntohl(addr.s_addr)+1) == oi->te_para.lclif_ipaddr.value.s_addr)
	   	oi->te_para.rmtif_ipaddr.value.s_addr = htonl(ntohl(addr.s_addr)+2);
	   else
	   	oi->te_para.rmtif_ipaddr.value.s_addr = htonl(ntohl(addr.s_addr)+1);
      }
    }

   return;
}

static void
set_linkparams_te_metric (struct te_link_subtlv_te_metric *para, u_int32_t te_metric)
{
  para->header.type   = htons (TE_LINK_SUBTLV_TE_METRIC);
  para->header.length = htons (sizeof(u_int32_t));
  para->value = htonl (te_metric);
  return;
}

static void
set_linkparams_max_bw (struct te_link_subtlv_max_bw *para, float *fp)
{
  para->header.type   = htons (TE_LINK_SUBTLV_MAX_BW);
  para->header.length = htons (sizeof (float));
  htonf (fp, &para->value);
  return;
}
static void
set_linkparams_max_rsv_bw (struct te_link_subtlv_max_rsv_bw *para, float *fp)
{
  para->header.type   = htons (TE_LINK_SUBTLV_MAX_RSV_BW);
  para->header.length = htons (sizeof (float));
  htonf (fp, &para->value);
  return;
}

void
set_linkparams_unrsv_bw (struct te_link_subtlv_unrsv_bw *para, int priority, float *fp)
{
  /* Note that TLV-length field is the size of array. */
  para->header.type   = htons (TE_LINK_SUBTLV_UNRSV_BW);
  para->header.length = htons (sizeof (para->value));
  htonf (fp, &para->value [priority]);
  return;
}

static void
set_linkparams_rsc_clsclr (struct te_link_subtlv_rsc_clsclr *para, u_int32_t classcolor)
{
  para->header.type   = htons (TE_LINK_SUBTLV_RSC_CLSCLR);
  para->header.length = htons (sizeof(para->value));
  para->value = htonl (classcolor);
  return;
}

static void
set_linkparams_protection_type (struct te_link_subtlv_link_protype *para, u_char ptype)
{
  para->header.type   = htons (TE_LINK_SUBTLV_LINK_PROTYPE);
  para->header.length = htons (sizeof (para->value4));
  para->value4.value = ptype;
  memset (para->value4.padding, 0, sizeof(u_char)*3);
  return;
}

void
set_linkparams_lcl_id (struct ospf_interface *oi)
{
   /* Only point-to-point interfaces are supported */
   if (oi->type != OSPF_IFTYPE_POINTOPOINT || (!ospf_if_is_enable(oi)))
   	return;
   /* We don't advertise local/remote ID for numbered interface */
   if (ntohs(oi->te_para.lclif_ipaddr.header.type) != 0 )
   	return;

   if (INTERFACE_GMPLS_ENABLED(oi) && IS_VALID_LCL_IFID(oi->vlsr_if.if_id))
   {
	oi->te_para.link_lcrmt_id.header.type = htons (TE_LINK_SUBTLV_LINK_LCRMT_ID);
	oi->te_para.link_lcrmt_id.header.length = htons (sizeof(u_int32_t)*2);
	oi->te_para.link_lcrmt_id.link_local_id = ntohl(oi->vlsr_if.if_id);
   }
  
   return;
}

void
set_linkparams_rmt_id (struct te_link_subtlv_link_lcrmt_id *para, u_int32_t value)
{
   para->header.type = htons (TE_LINK_SUBTLV_LINK_LCRMT_ID);
   para->header.length = htons (sizeof(u_int32_t)*2);
   para->link_remote_id = htonl(value);  
   
   return;
}

void 
insert_gri(list gri_list, u_int32_t ucid, u_int32_t seqnum)
{
	listnode node;
	struct dragon_gri_para* gri;
	struct timeval timer_now;
	gettimeofday (&timer_now, NULL);

	if (gri_list = NULL)
		gri_list = list_new();
	LIST_LOOP(gri_list, gri, node)
	{
		if (timer_now.tv_sec - gri->timestamp > MAX_GRI_AGE)
			list_delete_node(gri_list, node);
		if (gri->ucid == ucid && gri->seqnum == seqnum)
			return;
	}
	gri = XMALLOC(MTYPE_TMP, sizeof(struct dragon_gri_para));
	gri ->timestamp = timer_now.tv_sec;
	gri->ucid = ucid;
	gri->seqnum = seqnum;
	listnode_add(gri_list, gri);
}

static void
srlg_del(u_int32_t *value)
{
     XFREE (MTYPE_LINK_NODE, value);
}

static u_char
set_linkparams_srlg (struct te_link_subtlv_link_srlg *para, u_int32_t value, u_char action)
{
    u_int32_t *v = NULL;
    struct listnode *node;
    u_char find = 0;

   if (!para->srlg_list)
   {
   	if (action == LINK_SRLG_DEL_VALUE)
	   	return 1;
   	else{
   		para->srlg_list = list_new();
   		para->srlg_list->del = (void (*) (void *))srlg_del;
   	}
   }

   LIST_LOOP(para->srlg_list,v,node)
   {
   	if (ntohl(*v) == value) {
   		find = 1;
   		break;
   	}
   }
   if (((!find) && action == LINK_SRLG_DEL_VALUE) || 
   	  ((find) && action == LINK_SRLG_ADD_VALUE) )
   	return 0;
   if (action==LINK_SRLG_ADD_VALUE){
   	   if (ntohs(para->header.type)==0){
		para->header.type   = htons (TE_LINK_SUBTLV_LINK_SRLG);
	  	para->header.length = htons (sizeof (u_int32_t));
   	   }
   	   else
   	   	para->header.length = htons(ntohs(para->header.length)+sizeof(u_int32_t));
   	   v = XMALLOC(MTYPE_OSPF_TMP, sizeof(u_int32_t));
   	   *v = htonl(value);
   	   listnode_add(para->srlg_list, v);
   }
   else if (action==LINK_SRLG_DEL_VALUE){
   	   para->header.length = htons(ntohs(para->header.length)-sizeof(u_int32_t));
   	   listnode_delete(para->srlg_list, v);
   	   srlg_del(v);
   	   if (list_isempty(para->srlg_list)){
		para->header.type = 0;
		list_free(para->srlg_list);
   	   }
   }
   return 1;
}

static void
set_linkparams_te_lambda (struct te_link_subtlv_link_te_lambda *para, u_int32_t freq)
{
  para->header.type   = htons (TE_LINK_SUBTLV_LINK_TE_LAMBDA);
  para->header.length = htons (sizeof (para->frequency));
  para->frequency = htonl(freq);
  return;
}


/* We should read interface switching capability from the kernel*/ /*XXX*/
static void
set_linkparams_ifsw_cap1 (struct te_link_subtlv_link_ifswcap *para, u_char swcap, u_char enc)
{
  int base_length = 36;

  if (ntohs( para->header.type) == 0)
	 para->header.type   = htons (TE_LINK_SUBTLV_LINK_IFSWCAP);
  if ( swcap >= LINK_IFSWCAP_SUBTLV_SWCAP_PSC1 && 
  	 swcap <= LINK_IFSWCAP_SUBTLV_SWCAP_PSC4 )
  {
  	 para->header.length = htons(base_length + sizeof(struct link_ifswcap_specific_psc));
  }
  else if ( swcap == LINK_IFSWCAP_SUBTLV_SWCAP_TDM)
  {
  	 para->header.length = htons(base_length + sizeof(struct link_ifswcap_specific_tdm));
  }
  else if ( swcap == LINK_IFSWCAP_SUBTLV_SWCAP_L2SC)
  {
  	 para->header.length = htons(base_length); /* + sizeof(struct link_ifswcap_specific_vlan));*/
  }
  else
  	 para->header.length = htons(base_length);
   para->link_ifswcap_data.switching_cap = swcap;
   para->link_ifswcap_data.encoding = enc;
  return;
}

static void
ospf_te_set_default_link_para (struct ospf_interface *oi)
{
  /*float default_bw = (float)125000; */ /* default value : 1000Mb/s */
  
  /* Set TE instance */
  if (LEGAL_TE_INSTANCE_RANGE(oi->ifp->ifindex))
	oi->te_para.instance = oi->ifp->ifindex;

  /*
   * Try to set initial values for those that can be derived from
   * zebra-interface information (not from configuration terminal!)
   */
  set_linkparams_link_type (oi);
  set_linkparams_link_id(oi);
  set_linkparams_lclif_addr(oi);
  set_linkparams_rmtif_addr(oi);
  if ( INTERFACE_GMPLS_ENABLED(oi) && IS_VALID_LCL_IFID(oi->vlsr_if.if_id))
  	set_linkparams_lcl_id(oi);
/*  
  set_linkparams_max_bw(&oi->te_para.max_bw, &default_bw); 
  set_linkparams_max_rsv_bw(&oi->te_para.max_rsv_bw, &default_bw); 
  for (i=0; i<8; i++)
	  set_linkparams_unrsv_bw(&oi->te_para.unrsv_bw, i, &default_bw); 
  */
  return;
}


static void 
ospf_te_set_configed_link_para(struct ospf_interface *oi, struct ospf_te_config_para *conf)
{
	struct te_area_lsa_para *old_para = &oi->te_para;
	u_int32_t *v = NULL;
	struct listnode *node;
	
	if (ntohs (conf->te_para.te_metric.header.type) != 0)
		memcpy(&old_para->te_metric, &conf->te_para.te_metric, sizeof(struct te_link_subtlv_te_metric));
	if (ntohs (conf->te_para.max_bw.header.type) != 0)
		memcpy(&old_para->max_bw, &conf->te_para.max_bw, sizeof(struct te_link_subtlv_max_bw));
	if (ntohs (conf->te_para.max_rsv_bw.header.type) != 0)
		memcpy(&old_para->max_rsv_bw, &conf->te_para.max_rsv_bw, sizeof(struct te_link_subtlv_max_rsv_bw));
	if (ntohs (conf->te_para.unrsv_bw.header.type) != 0)
		memcpy(&old_para->unrsv_bw, &conf->te_para.unrsv_bw, sizeof(struct te_link_subtlv_unrsv_bw));
	if (ntohs (conf->te_para.rsc_clsclr.header.type) != 0)
		memcpy(&old_para->rsc_clsclr, &conf->te_para.rsc_clsclr, sizeof(struct te_link_subtlv_rsc_clsclr));
	if (INTERFACE_GMPLS_ENABLED(oi))
	{
		if (ntohs (conf->te_para.link_protype.header.type) != 0)
			memcpy(&old_para->link_protype, &conf->te_para.link_protype, sizeof(struct te_link_subtlv_link_protype));
		if (ntohs (conf->te_para.link_ifswcap.header.type) != 0)
			memcpy(&old_para->link_ifswcap, &conf->te_para.link_ifswcap, sizeof(struct te_link_subtlv_link_ifswcap));
		if (ntohs(conf->te_para.link_lcrmt_id.header.type) != 0 &&
		     ntohs(old_para->link_lcrmt_id.header.type) != 0)
			old_para->link_lcrmt_id.link_remote_id = conf->te_para.link_lcrmt_id.link_remote_id;
		if (ntohs (conf->te_para.link_srlg.header.type) != 0)
		{
			if (conf->te_para.link_srlg.srlg_list)
			{
				LIST_LOOP(conf->te_para.link_srlg.srlg_list, v, node)
				{
				  	set_linkparams_srlg(&old_para->link_srlg, ntohl(*v), LINK_SRLG_ADD_VALUE);
		  		}
			}
		}
		if (ntohs (conf->te_para.link_te_lambda.header.type) != 0)
			memcpy(&old_para->link_te_lambda, &conf->te_para.link_te_lambda, sizeof(struct te_link_subtlv_link_te_lambda));
	}
	
}

/*------------------------------------------------------------------------*
 * Followings are callback functions against generic Opaque-LSAs handling.
 *------------------------------------------------------------------------*/

void
ospf_te_new_if (struct ospf_interface *oi)
{
	struct ospf_area *area = oi->area;
	
  	if (INTERFACE_MPLS_ENABLED(oi) && area != NULL)
       {
          if (!area->te_area_lsa_rtid_self)
          {
		    OSPF_TIMER_OFF (area->t_te_area_lsa_rtid_self);
		    OSPF_AREA_TIMER_ON (area->t_te_area_lsa_rtid_self,
				  ospf_te_area_lsa_rtid_timer, OSPF_MIN_LS_INTERVAL);
          }

	      OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
	      OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_MIN_LS_INTERVAL);

          if (INTERFACE_GMPLS_ENABLED(oi) && 
          	ntohs(oi->te_para.lclif_ipaddr.header.type) == 0){
	      OSPF_TIMER_OFF (oi->t_te_linklocal_lsa_self);
	      OSPF_INTERFACE_TIMER_ON (oi->t_te_linklocal_lsa_self, ospf_te_linklocal_lsa_timer, OSPF_MIN_LS_INTERVAL);
          }
       }

  return;
}

void
ospf_te_del_if (struct ospf_interface *oi)
{

  if (INTERFACE_GMPLS_ENABLED(oi) && oi->te_para.link_srlg.srlg_list)
  {
	list_delete(oi->te_para.link_srlg.srlg_list);
  }
  oi->te_enabled = INTERFACE_NO_TE;
  memset(&oi->te_para, 0, sizeof(struct te_area_lsa_para));
  
  /* router ID TE LSA is not flushed because we are only disabling one TE interface, not all */
  /* flush others immediately */  
  if (oi->te_area_lsa_link_self)
  {
  	if (oi->t_te_area_lsa_link_self)
  		OSPF_TIMER_OFF(oi->t_te_area_lsa_link_self);
 	ospf_te_area_lsa_link_refresh(oi->te_area_lsa_link_self);  /* if MPLS/GMPLS is disabled, then this lsa will be flushed */
  }

  if (oi->te_linklocal_lsa_self)
  {
  	if (oi->t_te_linklocal_lsa_self)
  		OSPF_TIMER_OFF(oi->t_te_linklocal_lsa_self);
	ospf_te_linklocal_lsa_refresh(oi->te_linklocal_lsa_self); /* if GMPLS is disabled, then this lsa will be flushed */
  }
  
  return;
}

void
ospf_te_ism_change (struct ospf_interface *oi, int old_state)
{
  struct te_link_subtlv_link_type old_type;
  struct te_link_subtlv_link_id   old_id;
  struct ospf_area *area = oi->area;
  struct ospf_te_config_para *oc;
  listnode node;
  
  if (area == NULL || oi->area->ospf == NULL)
    {
      zlog_warn ("ospf_te_ism_change: Cannot refer to OSPF from OI(%s)?", IF_NAME (oi));
      goto out;
    }
  /* Make sure opaque LSA option is set ! */
  if (!CHECK_FLAG(oi->nbr_self->options, OSPF_OPTION_O))
	SET_FLAG (oi->nbr_self->options, OSPF_OPTION_O);

  /* We don't have inconsistency between ospf-te interfaces and ospf interfaces, as there were in original Zebra-TE
    * because we have combined them together by "oi->te_enabled" 
  */
  switch (oi->state)
    {
    case ISM_PointToPoint:
    case ISM_DROther:
    case ISM_Backup:
    case ISM_DR:
    	/* Load configuration from the buffer */
    	if ((!INTERFACE_MPLS_ENABLED(oi)))
    	{
		LIST_LOOP(OspfTeConfigList, oc, node)
		    if (strcmp(oc->if_name, oi->ifp->name)==0)
		    {
		    	if (ospf_te_verify_config(oi, oc)==0)
		        	ospf_te_new_if(oi);
		      if (oc->te_para.link_srlg.srlg_list)
		     	      list_delete(oc->te_para.link_srlg.srlg_list);
		    	listnode_delete(OspfTeConfigList, oc);

		    	break;
		    }
    	}
	else
	{
	      old_type = oi->te_para.link_type;
	      old_id   = oi->te_para.link_id;

	      ospf_te_set_default_link_para(oi);
	      if ((ntohs (old_type.header.type) != ntohs (oi->te_para.link_type.header.type)
	      ||   old_type.link_type.value     != oi->te_para.link_type.link_type.value)
	      ||  (ntohs (old_id.header.type)   != ntohs (oi->te_para.link_id.header.type)
	      ||   ntohl (old_id.value.s_addr)  != ntohl (oi->te_para.link_id.value.s_addr)))
	        {
	        	ospf_te_new_if(oi);
	        }
	}
      break;
    default:
    	if (INTERFACE_MPLS_ENABLED(oi))
	    	ospf_te_del_if(oi);
      break;
    }

out:
  return;
}

void
ospf_te_nsm_change (struct ospf_neighbor *nbr, int old_state)
{
  struct ospf_interface *oi;
  struct te_link_subtlv_link_type old_type;
  struct te_link_subtlv_link_id   old_id;
  struct ospf_area *area;
  struct ospf_te_config_para *oc;
  listnode node;
  
  if ((!nbr) || (!nbr->oi) || (nbr->state <= NSM_ExStart))
  	return;
  oi = nbr->oi;
  area = oi->area;
  
  if (!INTERFACE_MPLS_ENABLED(oi))
  {
		LIST_LOOP(OspfTeConfigList, oc, node)
		    if (strcmp(oc->if_name, oi->ifp->name)==0)
		    {
		    	if (ospf_te_verify_config(oi, oc)==0)
		        	ospf_te_new_if(oi);
		      if (oc->te_para.link_srlg.srlg_list)
		     	      list_delete(oc->te_para.link_srlg.srlg_list);
		    	listnode_delete(OspfTeConfigList, oc);

		    	break;
		    }
  }
  else
  {
      old_type = oi->te_para.link_type;
      old_id   = oi->te_para.link_id;
      ospf_te_set_default_link_para(oi);

      if ((ntohs (old_type.header.type) != ntohs (oi->te_para.link_type.header.type)
      ||   old_type.link_type.value     != oi->te_para.link_type.link_type.value)
      ||  (ntohs (old_id.header.type)   != ntohs (oi->te_para.link_id.header.type)
      ||   ntohl (old_id.value.s_addr)  != ntohl (oi->te_para.link_id.value.s_addr)))
        {
		ospf_te_new_if(oi);
        }
  	
  }

  return;
}


/*------------------------------------------------------------------------*
 * Followings are vty session control functions.
 *------------------------------------------------------------------------*/

static u_int16_t
show_vty_router_addr (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_tlv_router_addr *top = (struct te_tlv_router_addr *) tlvh;

  if (vty != NULL)
    vty_out (vty, "  Router-Address: %s%s", inet_ntoa (top->value), VTY_NEWLINE);
  else
    zlog_info ("    Router-Address: %s", inet_ntoa (top->value));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_local_id (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_tlv_link_local_id *top = (struct te_tlv_link_local_id*) tlvh;

  if (vty != NULL)
    vty_out (vty, "  Link local identifier: 0x%x%s", ntohl (top->value), VTY_NEWLINE);
  else
    zlog_info ("    Link local identifier: 0x%x", ntohl (top->value));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_header (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_tlv_link *top = (struct te_tlv_link *) tlvh;

  if (vty != NULL)
    vty_out (vty, "  Link: %u octets of data%s", ntohs (top->header.length), VTY_NEWLINE);
  else
    zlog_info ("    Link: %u octets of data", ntohs (top->header.length));

  return TLV_HDR_SIZE;	/* Here is special, not "TLV_SIZE". */
}

static u_int16_t
show_vty_link_subtlv_link_type (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_link_type *top;
  const char *cp = "Unknown";

  top = (struct te_link_subtlv_link_type *) tlvh;
  switch (top->link_type.value)
    {
    case LINK_TYPE_SUBTLV_VALUE_PTP:
      cp = "Point-to-point";
      break;
    case LINK_TYPE_SUBTLV_VALUE_MA:
      cp = "Multiaccess";
      break;
    default:
      break;
    }

  if (vty != NULL)
    vty_out (vty, "  Link-Type: %s (%u)%s", cp, top->link_type.value, VTY_NEWLINE);
  else
    zlog_info ("    Link-Type: %s (%u)", cp, top->link_type.value);

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_link_id (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_link_id *top;

  top = (struct te_link_subtlv_link_id *) tlvh;
  if (vty != NULL)
    vty_out (vty, "  Link-ID: %s%s", inet_ntoa (top->value), VTY_NEWLINE);
  else
    zlog_info ("    Link-ID: %s", inet_ntoa (top->value));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_lclif_ipaddr (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_lclif_ipaddr *top;

  top = (struct te_link_subtlv_lclif_ipaddr *) tlvh;

  if (vty != NULL)
    vty_out (vty, "  Local Interface IP Address(es): %s%s", inet_ntoa (top->value), VTY_NEWLINE);
  else
    zlog_info ("    Local Interface IP Address(es): %s", inet_ntoa (top->value));


  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_rmtif_ipaddr (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_rmtif_ipaddr *top;

  top = (struct te_link_subtlv_rmtif_ipaddr *) tlvh;
  if (vty != NULL)
    vty_out (vty, "  Remote Interface IP Address(es): %s%s", inet_ntoa (top->value), VTY_NEWLINE);
  else
    zlog_info ("    Remote Interface IP Address(es): %s", inet_ntoa (top->value));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_te_metric (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_te_metric *top;

  top = (struct te_link_subtlv_te_metric *) tlvh;
  if (vty != NULL)
    vty_out (vty, "  Traffic Engineering Metric: %u%s", (u_int32_t) ntohl (top->value), VTY_NEWLINE);
  else
    zlog_info ("    Traffic Engineering Metric: %u", (u_int32_t) ntohl (top->value));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_max_bw (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_max_bw *top;
  float fval;

  top = (struct te_link_subtlv_max_bw *) tlvh;
  ntohf (&top->value, &fval);

  if (vty != NULL)
    vty_out (vty, "  Maximum Bandwidth: %.0lf (Bytes/sec)%s", fval, VTY_NEWLINE);
  else
    zlog_info ("    Maximum Bandwidth: %.0lf (Bytes/sec)", fval);

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_max_rsv_bw (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_max_rsv_bw *top;
  float fval;

  top = (struct te_link_subtlv_max_rsv_bw *) tlvh;
  ntohf (&top->value, &fval);

  if (vty != NULL)
    vty_out (vty, "  Maximum Reservable Bandwidth: %.0lf (Bytes/sec)%s", fval, VTY_NEWLINE);
  else
    zlog_info ("    Maximum Reservable Bandwidth: %.0lf (Bytes/sec)", fval);

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_unrsv_bw (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_unrsv_bw *top;
  float fval;
  int i;

  top = (struct te_link_subtlv_unrsv_bw *) tlvh;
  for (i = 0; i < 8; i++)
    {
      ntohf (&top->value[i], &fval);
      if (vty != NULL)
        vty_out (vty, "  Unreserved Bandwidth (pri %d): %.0lf (Bytes/sec)%s", i, fval, VTY_NEWLINE);
      else
        zlog_info ("    Unreserved Bandwidth (pri %d): %.0lf (Bytes/sec)", i, fval);
    }

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_rsc_clsclr (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_rsc_clsclr *top;

  top = (struct te_link_subtlv_rsc_clsclr *) tlvh;
  if (vty != NULL)
    vty_out (vty, "  Resource class/color: 0x%x%s", (u_int32_t) ntohl (top->value), VTY_NEWLINE);
  else
    zlog_info ("    Resource Class/Color: 0x%x", (u_int32_t) ntohl (top->value));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_lcrmt_id (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_link_lcrmt_id *top;

  top = (struct te_link_subtlv_link_lcrmt_id *) tlvh;
  if (vty != NULL){
    vty_out (vty, "  Link local / remote id : 0x%x(%u) / 0x%x(%u) %s", ntohl (top->link_local_id),ntohl (top->link_local_id),ntohl (top->link_remote_id),  ntohl (top->link_remote_id), VTY_NEWLINE);
    }   
  else
    zlog_info ("  Link local / remote id : 0x%x / 0x%x", ntohl (top->link_local_id), ntohl (top->link_remote_id));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_protection_type (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_link_protype *top;
  const char *p;
  
  top = (struct te_link_subtlv_link_protype *) tlvh;
  p = val2str(&str_val_conv_protection, top->value4.value);
 
  if (vty != NULL)
    vty_out (vty, "  Link Protection Type: %s (%u)  %s", p, (u_int32_t)ntohs (top->value4.value), VTY_NEWLINE);
  else
    zlog_info ("    Link Protection Type: %s (%u)",p, (u_int32_t)ntohs(top->value4.value));

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_srlg (struct vty *vty, struct te_tlv_header *tlvh, int local)
{
  struct te_link_subtlv_link_srlg *top = NULL;
  u_int32_t *v = NULL;
  struct listnode *node;
  int i, n;

  n = ntohs (tlvh->length) / sizeof (u_int32_t);
  if (local)
  {
	  top = (struct te_link_subtlv_link_srlg *) tlvh;
			  assert(n == listcount(top->srlg_list));
  }
		  
  if (vty != NULL)
    vty_out (vty, "  Shared Risk Link Group(s): %d%s", n, VTY_NEWLINE);
  else
    zlog_info ("    Shared Risk Link Group(s): %d", n);

  if (local)
  {
	  i = 0;
	  LIST_LOOP(top->srlg_list, v, node)
	    {
	      if (vty != NULL)
	        vty_out (vty, "    #%d: %d%s", ++i, ntohl (*v), VTY_NEWLINE);
	      else
	        zlog_info ("      #%d: %d", ++i, ntohl (*v));
	    }
  }
  else
  {
  	v = (u_int32_t*)((char*)tlvh+TLV_HDR_SIZE);
	for (i=0; i<n; i++)
	{
	      if (vty != NULL)
	        vty_out (vty, "    #%d: %d%s", i+1, ntohl (*v), VTY_NEWLINE);
	      else
	        zlog_info ("      #%d: %d", i, ntohl (*v));
		v++;
	}
  }
  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_te_lambda (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_link_te_lambda *top;
  const char *p;
  
  top = (struct te_link_subtlv_link_protype *) tlvh;
  p = frequency2wavelength(ntohl(top->frequency));
 
  if (vty != NULL)
    vty_out (vty, "  DRAGON TE Lambda: %u (%s nm)%s", ntohl(top->frequency), p, VTY_NEWLINE);
  else
    zlog_info ("   DRAGON TE Lambda: %u (%s nm)", ntohl(top->frequency), p);

  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_domain_id (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_link_domain_id *top = (struct te_link_subtlv_link_domain_id *) tlvh;
 
  if (vty != NULL)
    vty_out (vty, "  DRAGON TE Domain ID: 0x%x%s", ntohl(top->domain_id),  VTY_NEWLINE);
  else
    zlog_info ("   DRAGON TE Domain ID: 0x%x.", ntohl(top->domain_id));

  return TLV_SIZE (tlvh);
}


static u_int16_t
show_vty_link_subtlv_ifsw_cap_local (struct vty *vty, struct te_tlv_header *tlvh)
{
  struct te_link_subtlv_link_ifswcap *top;
  const char* swcap = "Unknown";
  const char* enc  = "Unknown";
  int i;
  float fval;
  uLongf z_len;
  char ipv4[20], ipv4_a[20], ipv4_b[20];

  top = (struct te_link_subtlv_link_ifswcap *) tlvh;
  swcap = val2str(&str_val_conv_swcap, top->link_ifswcap_data.switching_cap);
  enc = val2str(&str_val_conv_encoding, top->link_ifswcap_data.encoding);

  if (vty != NULL){
    vty_out (vty, "  Interface Switching Capability Descriptor: %s %s %s", swcap, enc, VTY_NEWLINE);
    for (i = 0; i < LINK_MAX_PRIORITY; i++)
    {
           ntohf (&top->link_ifswcap_data.max_lsp_bw_at_priority[i], &fval);
	    vty_out (vty, "  Max LSP Bandwidth %d: %.0lf (Bytes/sec) %s", i , fval, VTY_NEWLINE);
    }
  }
  else{
    zlog_info ("  Interface Switching Capability Descriptor: %s %s", swcap, enc);
    for (i = 0; i < LINK_MAX_PRIORITY; i++)
    {
           ntohf (&top->link_ifswcap_data.max_lsp_bw_at_priority[i], &fval);
	   zlog_info ("  Max LSP Bandwidth %d: %.0lf (Bytes/sec)", i , fval);
    }
  }


  /* Show Switching Capability specific information */
  if (top->link_ifswcap_data.switching_cap >= LINK_IFSWCAP_SUBTLV_SWCAP_PSC1 &&
  	top->link_ifswcap_data.switching_cap <= LINK_IFSWCAP_SUBTLV_SWCAP_PSC4)
  {
         ntohf (&top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.min_lsp_bw, &fval);
	  if (vty != NULL){
	    vty_out (vty, "  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec) %s", 
	    				fval, VTY_NEWLINE);
	    vty_out (vty, "                                      : Interface MTU: %d %s", 
	    				ntohs(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.mtu), VTY_NEWLINE);
	  }
	  else{
	    zlog_info ("  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec)",  fval);
	    zlog_info ("                                      : Interface MTU: %d", 
	    				ntohs(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.mtu));
	  }
  }
  else if (top->link_ifswcap_data.switching_cap == LINK_IFSWCAP_SUBTLV_SWCAP_TDM)
  {
         ntohf (&top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.min_lsp_bw, &fval);
	  if (vty != NULL){
	    vty_out (vty, "  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec) %s", 
	    				fval, VTY_NEWLINE);
	    vty_out (vty, "                                      : Indication: %d %s", 
	    				top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.indication, VTY_NEWLINE);
	  }
	  else{
	    zlog_info ("  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec)", 
	    				fval);
	    zlog_info ("                                      : Indication: %d", 
	    				top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.indication);
	  }
  }
  else if (top->link_ifswcap_data.switching_cap == LINK_IFSWCAP_SUBTLV_SWCAP_L2SC)
  {
	if (vty != NULL && (ntohs(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_BASIC)
		&& (ntohs(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.length) == sizeof(struct link_ifswcap_specific_vlan))) 
	{
	    u_char* v = top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask;
	    if (ntohs(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version)  & IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z) {
		  z_len = ZBUFSIZE;
		  uncompress(z_buffer, z_len, top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, ntohs(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.length) - 4);
		  v = z_buffer;
    	    }

	    vty_out (vty, "  -- L2SC specific information--%s    --> Available VLAN tag set:", VTY_NEWLINE);
	    for (i = 1; i <= MAX_VLAN_NUM; i++)
		if (HAS_VLAN(v, i)) vty_out (vty, " %d", i);
	    vty_out (vty, "%s", VTY_NEWLINE);

	    v += MAX_VLAN_NUM/8;
	    vty_out (vty, "    --> Allocated VLAN tag set:");
	    for (i = 1; i <= MAX_VLAN_NUM; i++)
		if (HAS_VLAN(v, i)) vty_out (vty, " %d", i);
	    vty_out (vty, "%s", VTY_NEWLINE);
	}
	else if (vty != NULL && (ntohs(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_SUBNET_UNI))
	{
	    vty_out (vty, "  -- L2SC Subnet-UNI specific information--%s", VTY_NEWLINE);
	    strcpy (ipv4, inet_ntoa (*(struct in_addr*)&top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.tna_ipv4));
	    strcpy (ipv4_a, inet_ntoa (*(struct in_addr*)&top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.nid_ipv4));
	    strcpy (ipv4_b, inet_ntoa (*(struct in_addr*)&top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.data_ipv4));

	    vty_out (vty, "      -> Subnet-UNI ID: %d TNA-PIv4 %s via ControlChannel %s%s", 
			top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.subnet_uni_id, ipv4,
			top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.control_channel, VTY_NEWLINE);
	    vty_out (vty, "      -> UNI-N NodeName %s NodeID: %s, DataInterface IP: %s%s", 
			top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.node_name, ipv4_a, ipv4_b, VTY_NEWLINE);
	    vty_out (vty, "      -> LogicalPort: %s, EgressLabel: %d, UpstreamLabel: %d%s", 
			logical_port_number2string(ntohl(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.logical_port_number)),
			ntohl(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.egress_label_downstream), 
			ntohl(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.egress_label_upstream), VTY_NEWLINE);

            swcap = val2str(&str_val_conv_swcap, top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.swcap_ext);
            enc = val2str(&str_val_conv_encoding, top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.encoding_ext);
            vty_out (vty, "      -> Extended SwitchingType: %s, EncodingType: %s%s", swcap, enc, VTY_NEWLINE);
            vty_out (vty, "      -> Available TimeSlots:");
            for (i = 1; i <= MAX_TIMESLOTS_NUM; i++)
                if (HAS_TIMESLOT(top->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.timeslot_bitmask, i)) vty_out (vty, " %d", i);
            vty_out (vty, "%s", VTY_NEWLINE);
	}
  }
  return TLV_SIZE (tlvh);
}

static u_int16_t
show_vty_link_subtlv_ifsw_cap_network (struct vty *vty, struct te_tlv_header *tlvh)
{
  const char* swcap = "Unknown";
  const char* enc  = "Unknown";
  int i, n;
  float fval, *f;
  u_char *v;
  u_int16_t *dc;
  struct link_ifswcap_specific_subnet_uni *subnet_uni;
  char ipv4[20], ipv4_a[20], ipv4_b[20];

  v = (u_char *)(tlvh+1);
  swcap = val2str(&str_val_conv_swcap, *v);
  
  v++;
  enc = val2str(&str_val_conv_encoding, *v);
  
    v+=3;
    f = (float *)v;
  if (vty != NULL){
    vty_out (vty, "  Interface Switching Capability Descriptor: %s %s %s", swcap, enc, VTY_NEWLINE);

    for (i = 0; i < LINK_MAX_PRIORITY; i++)
    {
           ntohf (f, &fval);
	    vty_out (vty, "  Max LSP Bandwidth %d: %.0lf (Bytes/sec) %s", i , fval, VTY_NEWLINE);
	    f++;
    }
  }
  else{
    zlog_info ("  Interface Switching Capability Descriptor: %s %s", swcap, enc);
    for (i = 0; i < LINK_MAX_PRIORITY; i++)
    {
           ntohf (f, &fval);
	   zlog_info ("  Max LSP Bandwidth %d: %.0lf (Bytes/sec)", i , fval);
	   f++;
    }
  }
  v = (u_char *)f;

  /* Show Switching Capability specific information */
  if (strncmp(swcap, "psc", 3) == 0)
  {
  	  f = (float *)v;
         ntohf (f, &fval);
	  if (vty != NULL){
	    vty_out (vty, "  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec) %s", 
	    				fval, VTY_NEWLINE);
	    f++;
	    dc = (u_int16_t *)f;
	    vty_out (vty, "                                      : Interface MTU: %d %s", 
	    				ntohs(*dc), VTY_NEWLINE);
	  }
	  else{
	    zlog_info ("  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec)",  fval);
	    f++;
	    dc = (u_int16_t *)f;
	    zlog_info ("                                      : Interface MTU: %d", 
	    				ntohs(*dc));
	  }
  }
  else if (strncmp(swcap, "tdm", 3) == 0)
  {
  	  f = (float *)v;
         ntohf (f, &fval);
	  if (vty != NULL){
	    vty_out (vty, "  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec) %s", 
	    				fval, VTY_NEWLINE);
	    f++;
	    v = (u_char *)f;
	    vty_out (vty, "                                      : Indication: %d %s", 
	    				*v, VTY_NEWLINE);
	  }
	  else{
	    zlog_info ("  --specific information-- : Minimum LSP Bandwidth: %.0lf (Bytes/sec)", 
	    				fval);
	    f++;
	    v = (u_char *)f;
	    zlog_info ("                                      : Indication: %d", 
	    				*v);
	  }
  }
  else if (strncmp(swcap, "l2sc", 4) == 0)
  {
	  if (vty != NULL && (ntohs(*(u_int16_t*)(v+2)) & IFSWCAP_SPECIFIC_VLAN_BASIC)) {
	    if (ntohs(*(u_int16_t*)(v+2)) & IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z) {
		n = ZBUFSIZE;
		uncompress(z_buffer, (uLongf*)&n, v+4, ntohs(*(u_int16_t*)(v)));
		v = z_buffer;
    	    }
	    else 
		v += 4;

	    vty_out (vty, "  -- L2SC specific information--%s    --> Available VLAN tag set:", VTY_NEWLINE);
	    for (i = 1; i <= MAX_VLAN_NUM; i++)
		if (HAS_VLAN(v, i)) vty_out (vty, " %d", i);
	    vty_out (vty, "%s", VTY_NEWLINE);

	    v += MAX_VLAN_NUM/8;
	    vty_out (vty, "    --> Allocated VLAN tag set:");
	    for (i = 1; i <= MAX_VLAN_NUM; i++)
		if (HAS_VLAN(v, i)) vty_out (vty, " %d", i);
	    vty_out (vty, "%s", VTY_NEWLINE);
	}
	else if (vty != NULL && (ntohs(*(u_int16_t*)(v+2)) & IFSWCAP_SPECIFIC_SUBNET_UNI)) {
	    subnet_uni = (struct link_ifswcap_specific_subnet_uni *)v;
            strcpy (ipv4, inet_ntoa (*(struct in_addr*)&subnet_uni->tna_ipv4));
            strcpy (ipv4_a, inet_ntoa (*(struct in_addr*)&subnet_uni->nid_ipv4));
            strcpy (ipv4_b, inet_ntoa (*(struct in_addr*)&subnet_uni->data_ipv4));
            vty_out (vty, "  -- L2SC Subnet-UNI specific information--%s", VTY_NEWLINE);
            vty_out (vty, "      -> Subnet-UNI ID: %d TNA-IPv4 %s via ControlChannel %s%s", 
				subnet_uni->subnet_uni_id, ipv4, subnet_uni->control_channel, VTY_NEWLINE);
            vty_out (vty, "      -> UNI-N NodeName %s NodeID: %s, Data IP: %s%s", subnet_uni->node_name, ipv4_a, ipv4_b, VTY_NEWLINE);
            vty_out (vty, "      -> LogicalPort: %s, EgressLabel: %d, UpstreamLabel: %d%s",
				logical_port_number2string(ntohl(subnet_uni->logical_port_number)),
				ntohl(subnet_uni->egress_label_downstream), ntohl(subnet_uni->egress_label_upstream), VTY_NEWLINE);

            swcap = val2str(&str_val_conv_swcap, subnet_uni->swcap_ext);
            enc = val2str(&str_val_conv_encoding, subnet_uni->encoding_ext);
            vty_out (vty, "      -> Extended SwitchingType: %s, EncodingType: %s%s", swcap, enc, VTY_NEWLINE);
            vty_out (vty, "      -> Available TimeSlots:");
            for (i = 1; i <= MAX_TIMESLOTS_NUM; i++)
                if (HAS_TIMESLOT(subnet_uni->timeslot_bitmask, i)) vty_out (vty, " %d", i);
            vty_out (vty, "%s", VTY_NEWLINE);
	}
  }

  return TLV_SIZE (tlvh);
}


static u_int16_t
show_vty_unknown_tlv (struct vty *vty, struct te_tlv_header *tlvh)
{
  if (vty != NULL)
    vty_out (vty, "  Unknown TLV: [type(0x%x), length(0x%x)]%s", ntohs (tlvh->type), ntohs (tlvh->length), VTY_NEWLINE);
  else
    zlog_info ("    Unknown TLV: [type(0x%x), length(0x%x)]", ntohs (tlvh->type), ntohs (tlvh->length));

  return TLV_SIZE (tlvh);
}

static u_int16_t
ospf_te_show_link_subtlv (struct vty *vty, struct te_tlv_header *tlvh0,
                               u_int16_t subtotal, u_int16_t total)
{
  struct te_tlv_header *tlvh, *next;
  u_int16_t sum = subtotal;

  for (tlvh = tlvh0; sum < total; tlvh = (next ? next : TLV_HDR_NEXT (tlvh)))
    {
      next = NULL;
      switch (ntohs (tlvh->type))
        {
        case TE_LINK_SUBTLV_LINK_TYPE:
          sum += show_vty_link_subtlv_link_type (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_LINK_ID:
          sum += show_vty_link_subtlv_link_id (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_LCLIF_IPADDR:
          sum += show_vty_link_subtlv_lclif_ipaddr (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_RMTIF_IPADDR:
          sum += show_vty_link_subtlv_rmtif_ipaddr (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_TE_METRIC:
          sum += show_vty_link_subtlv_te_metric (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_MAX_BW:
          sum += show_vty_link_subtlv_max_bw (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_MAX_RSV_BW:
          sum += show_vty_link_subtlv_max_rsv_bw (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_UNRSV_BW:
          sum += show_vty_link_subtlv_unrsv_bw (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_RSC_CLSCLR:
          sum += show_vty_link_subtlv_rsc_clsclr (vty, tlvh);
          break;
        case TE_LINK_SUBTLV_LINK_PROTYPE:
          sum += show_vty_link_subtlv_protection_type(vty,  tlvh);
          break;
        case TE_LINK_SUBTLV_LINK_SRLG:
          sum += show_vty_link_subtlv_srlg(vty,  tlvh, 0);
          break;
        case TE_LINK_SUBTLV_LINK_LCRMT_ID:
          sum += show_vty_link_subtlv_lcrmt_id(vty,  tlvh);
          break;
        case TE_LINK_SUBTLV_LINK_IFSWCAP:
          sum += show_vty_link_subtlv_ifsw_cap_network(vty,  tlvh);
          break;
        case TE_LINK_SUBTLV_LINK_TE_LAMBDA:
          sum += show_vty_link_subtlv_te_lambda(vty,  tlvh);
          break;
        case TE_LINK_SUBTLV_LINK_DOMAIN_ID:
          sum += show_vty_link_subtlv_domain_id(vty,  tlvh);
          break;
        default:
          sum += show_vty_unknown_tlv (vty, tlvh);
          break;
        }
    }
  return sum;
}

static void
ospf_te_show_info (struct vty *vty, struct ospf_lsa *lsa)
{
  struct lsa_header *lsah = (struct lsa_header *) lsa->data;
  struct te_tlv_header *tlvh, *next;
  u_int16_t sum, total;
  u_int16_t (* subfunc)(struct vty *vty, struct te_tlv_header *tlvh,
                        u_int16_t subtotal, u_int16_t total) = NULL;

  sum = 0;
  total = ntohs (lsah->length) - OSPF_LSA_HEADER_SIZE;

  for (tlvh = TLV_HDR_TOP (lsah); sum < total;
			tlvh = (next ? next : TLV_HDR_NEXT (tlvh)))
    {
      if (subfunc != NULL)
        {
          sum = (* subfunc)(vty, tlvh, sum, total);
	  next = (struct te_tlv_header *)((char *) tlvh + sum);
          subfunc = NULL;
          continue;
        }

      next = NULL;
      if (lsah->type == OSPF_OPAQUE_LINK_LSA)
      	      sum+= show_vty_link_local_id(vty, tlvh);
      else{
	      switch (ntohs (tlvh->type))
	        {
	        case TE_TLV_ROUTER_ADDR:
	          sum += show_vty_router_addr (vty, tlvh);
	          break;
	        case TE_TLV_LINK:
	          sum += show_vty_link_header (vty, tlvh);
		  subfunc = ospf_te_show_link_subtlv;
		  next = tlvh + 1;
	          break;
	        default:
	          sum += show_vty_unknown_tlv (vty, tlvh);
	          break;
	        }
      }
    }
  return;
}

int
ospf_te_verify_config(struct ospf_interface *oi, struct ospf_te_config_para *oc)
{
	int ret = 0;
	if ((!oi) || (!oc) || (strcmp(oi->ifp->name, oc->if_name)!=0))
		ret = -1;
	else if (!(INTERFACE_MPLS_ENABLED(oi) || oc->level >= INTERFACE_TE_MPLS))
		/* Level must be at least MPLS */
		ret = -1;
	else if (oc->vlsr_if.protocol != VLSR_PROTO_NONE)
	{
		if (!(INTERFACE_GMPLS_ENABLED(oi) || (oc->level == INTERFACE_TE_GMPLS)))
		  /* Level must be set to GMPLS in order to support out-of-band signaling */
			ret = -1;
		if (oc->vlsr_if.switch_ip.s_addr == oi->address->u.prefix4.s_addr)
	        /* Switch IP cannot be the same as control IP */
			ret = -1;
  	}
	if (ret==0)
  	{
  		if (!INTERFACE_MPLS_ENABLED(oi))
  			oi->te_enabled = oc->level;
		if (INTERFACE_GMPLS_ENABLED(oi) && 
			(oc->vlsr_if.protocol != VLSR_PROTO_NONE || oc->vlsr_if.data_ip.s_addr!=0 || oc->vlsr_if.if_id!=0))
			memcpy(&oi->vlsr_if, &oc->vlsr_if, sizeof(struct vlsr_if));
	      ospf_te_set_default_link_para(oi);
	      ospf_te_set_configed_link_para(oi, oc);
  	}
	return ret;
}

/* This function is called when the configuration of a TE-Link is completed */
void 
ospf_te_interface_config_update(struct vty* vty)
{
	struct ospf *ospf = (struct ospf *)(vty->index);
	struct ospf_interface *oi = NULL;
	listnode node;
	struct ospf_te_config_para *oc;
	
	if (listcount(ospf->oiflist)) /* ospfd already fetched IP data from zebra */
	{
		LIST_LOOP(ospf->oiflist, oi, node)
		    if (oi->ifp && strcmp(oi->ifp->name, te_config.if_name)==0)
		    {
		    	if (te_config.configed == 1 && ospf_te_verify_config(oi, &te_config) == 0)
		    		ospf_te_new_if(oi);
		       /* Don't forget to release occupied memory ! */
		       if (te_config.te_para.link_srlg.srlg_list)
			       list_delete(te_config.te_para.link_srlg.srlg_list);
			memset(&te_config, 0, sizeof(struct ospf_te_config_para));
		    }
	}
	else/* in the middle of reading ospfd.conf */
	{
		if (te_config.configed == 1)
		{
			oc = XMALLOC(MTYPE_OSPF_TMP, sizeof(struct ospf_te_config_para));
			memcpy(oc, &te_config, sizeof(struct ospf_te_config_para));
			listnode_add(OspfTeConfigList, oc);
		}
		memset(&te_config, 0, sizeof(struct ospf_te_config_para));
	}
}

static void ospf_te_config_write (struct vty *vty)
{
  struct ospf *ospf;
  struct ospf_interface *oi;
  struct listnode *node, *node1, *node2;
  u_int32_t *v = NULL;
  const char *p = NULL;
  float bw;
  int i;
  char temp1[20], temp2[20];  /* Added to avoid an C optimization /inet_ntoa (?) problem */

  if (ntohl(OspfTeRouterAddr.header.type)!=0)
  	vty_out(vty, "  ospf-te router-address %s%s", inet_ntoa (OspfTeRouterAddr.value), VTY_NEWLINE);

  LIST_LOOP (om->ospf, ospf, node1){		/* for each ospf instance */
  	LIST_LOOP(ospf->oiflist, oi, node2){
	    if (INTERFACE_MPLS_ENABLED(oi) && oi->address){
              vty_out(vty, "  ospf-te interface %s%s",oi->ifp->name,VTY_NEWLINE);
              if (INTERFACE_GMPLS_ENABLED(oi))
			vty_out(vty, "       level gmpls %s", VTY_NEWLINE);             
              else
              	vty_out(vty, "       level mpls %s", VTY_NEWLINE);             
              if (oi->vlsr_if.data_ip.s_addr != 0)
              {
              	if (oi->vlsr_if.protocol!=0 && oi->vlsr_if.switch_ip.s_addr!=0)
              	{
              		strcpy(temp1, inet_ntoa(oi->vlsr_if.data_ip));
				strcpy(temp2, inet_ntoa(oi->vlsr_if.switch_ip));
					if (oi->vlsr_if.protocol == VLSR_PROTO_SNMP)
			              	vty_out(vty, "       data-interface ip %s protocol snmp switch-ip %s switch-port %d %s", 
			              					temp1, temp2, oi->vlsr_if.switch_port, VTY_NEWLINE);
					else if(oi->vlsr_if.protocol == VLSR_PROTO_TL1)
						vty_out(vty, "		 data-interface ip %s protocol tl1 switch-ip %s switch-port %d %s", 
										temp1, temp2, oi->vlsr_if.switch_port, VTY_NEWLINE);
              	}
			else
				vty_out(vty, "	data-interface ip %s %s", 
								inet_ntoa (oi->vlsr_if.data_ip), VTY_NEWLINE);
              }
		else {
			if (oi->vlsr_if.protocol!=0 && oi->vlsr_if.switch_ip.s_addr!=0)
			{
				strcpy(temp2, inet_ntoa(oi->vlsr_if.switch_ip));
				if (oi->vlsr_if.protocol == VLSR_PROTO_SNMP)
					vty_out(vty, "		 data-interface unnumbered protocol snmp switch-ip %s switch-port %d %s", 
									temp2, oi->vlsr_if.switch_port, VTY_NEWLINE);
				else if (oi->vlsr_if.protocol == VLSR_PROTO_TL1)
					vty_out(vty, "		 data-interface unnumbered protocol tl1 switch-ip %s switch-port %d %s", 
									temp2, oi->vlsr_if.switch_port, VTY_NEWLINE);
			}
			else
				vty_out(vty, "	data-interface unnumbered %s",  VTY_NEWLINE);
		}	
              
	    	if (ntohs(oi->te_para.te_metric.header.type) != 0)
		    	vty_out(vty, "       metric %d %s", ntohl(oi->te_para.te_metric.value), VTY_NEWLINE);
	    	if (ntohs(oi->te_para.max_bw.header.type) != 0)
	    	{
	    		ntohf(&oi->te_para.max_bw.value, &bw);
		    	vty_out(vty, "       max-bw %.0lf %s", bw, VTY_NEWLINE);
	    	}
	    	if (ntohs(oi->te_para.max_rsv_bw.header.type) != 0)
	    	{
	    		ntohf(&oi->te_para.max_rsv_bw.value, &bw);
		    	vty_out(vty, "       max-rsv-bw %.0lf %s", bw, VTY_NEWLINE);
	    	}
	    	if (ntohs(oi->te_para.rsc_clsclr.header.type) != 0)
		    	vty_out(vty, "       color 0x%x %s", ntohl(oi->te_para.rsc_clsclr.value), VTY_NEWLINE);

	    	if (ntohs(oi->te_para.link_protype.header.type) != 0)
	    	{
	    		p = val2str(&str_val_conv_protection, oi->te_para.link_protype.value4.value);
		    	vty_out(vty, "       protection %s %s", p, VTY_NEWLINE);
	    	}

	    	if (ntohs(oi->te_para.link_srlg.header.type) != 0)
	    	{
			    LIST_LOOP(oi->te_para.link_srlg.srlg_list,v,node)
		    		vty_out(vty, "       srlg add %d %s", ntohl(*v), VTY_NEWLINE);
	    	}

	    	if (ntohs(oi->te_para.link_ifswcap.header.type) != 0)
	    	{
	    		p = val2str(&str_val_conv_swcap, oi->te_para.link_ifswcap.link_ifswcap_data.switching_cap);
		    	vty_out(vty, "       swcap %s", p);
	    		p = val2str(&str_val_conv_encoding, oi->te_para.link_ifswcap.link_ifswcap_data.encoding);
		    	vty_out(vty, "  encoding %s %s", p, VTY_NEWLINE);
	  		for (i=0;i<8;i++){  		
		    		ntohf(&oi->te_para.link_ifswcap.link_ifswcap_data.max_lsp_bw_at_priority[i], &bw);
		    		if (bw > 0)
				    	vty_out(vty, "       max-lsp-bw %d %.0lf %s", i, bw, VTY_NEWLINE);
	  		}
	  		if (oi->te_para.link_ifswcap.link_ifswcap_data.switching_cap>=LINK_IFSWCAP_SUBTLV_SWCAP_PSC1 &&
	  		     oi->te_para.link_ifswcap.link_ifswcap_data.switching_cap>=LINK_IFSWCAP_SUBTLV_SWCAP_PSC4)
	  		{
	  			if (ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.mtu) > 0)
	  			{
		  		       ntohf(&oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.min_lsp_bw, &bw);
		  			vty_out(vty, "       min-lsp-bw %.0lf mtu %d %s", 
		  				              bw,
		  				              ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.mtu), 
		  				              VTY_NEWLINE);
	  			}
	  		}
	  		else if (oi->te_para.link_ifswcap.link_ifswcap_data.switching_cap==LINK_IFSWCAP_SUBTLV_SWCAP_TDM)
	  		{
	  			if (oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.indication > 0)
	  			{
		  		       ntohf(&oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.min_lsp_bw, &bw);
		  			vty_out(vty, "       min-lsp-bw %.0lf indication %d %s", 
		  				              bw,
		  				              oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.indication, 
		  				              VTY_NEWLINE);
	  			}
	  		}
	    	}

		if (ntohs(oi->te_para.link_lcrmt_id.header.type) != 0)
		{
			if (ntohl(oi->te_para.link_lcrmt_id.link_remote_id)!=0)
			    vty_out(vty, "		 remote-interface-id 0x%x(%u) %s", ntohl(oi->te_para.link_lcrmt_id.link_remote_id), ntohl(oi->te_para.link_lcrmt_id.link_remote_id), VTY_NEWLINE);
		}

		if (ntohs(oi->te_para.link_te_lambda.header.type) != 0)
		{
			if (ntohl(oi->te_para.link_te_lambda.frequency)!=0)
			    vty_out(vty, "		 DRAGON TE Lambda: frequency %d (%s nm)%s", ntohl(oi->te_para.link_te_lambda.frequency), frequency2wavelength(ntohl(oi->te_para.link_te_lambda.frequency)), VTY_NEWLINE);
		}
			
	    	vty_out(vty, "     exit %s", VTY_NEWLINE);
	    }
  	} /* End of LIST_LOOP(ospf->oiflist, oi, node2) */
  } /*End of LIST_LOOP (om->ospf, ospf, node1) */
  return;
}

u_int32_t
ospf_te_assign_lcl_ifid (void)
{
  static u_int32_t lcl_ifid = VLSR_INITIAL_LCL_IFID;
  u_int32_t tmp;

  tmp = lcl_ifid;
  /* Increment sequence number */
  if (lcl_ifid < VLSR_MAX_LCL_IFID)
    {
      lcl_ifid++;
    }
  else
    {
      lcl_ifid = VLSR_INITIAL_LCL_IFID;
    }
  return tmp;
}


/*------------------------------------------------------------------------*
 * Followings are vty command functions.
 *------------------------------------------------------------------------*/
DEFUN (ospf_te_interface_ifname,
       ospf_te_interface_ifname_cmd,
       "ospf-te interface INTERFACE",
       "Configure OSPF-TE parameters\n"
       "Configure TE parameters for the interface\n"
       "Interface name\n")
{
  struct ospf_interface *oi;
  struct ospf_te_config_para *oc;
  struct interface *ifp;
  struct listnode *node;
  struct ospf *ospf = (struct ospf *)vty->index;
  u_int8_t find;
  
  if ((ifp = if_lookup_by_name (argv[0])) == NULL){
        vty_out (vty, "No such interface name %s %s", argv[0], VTY_NEWLINE);
        return CMD_WARNING;
  }
  
  if (listcount(ospf->oiflist)) /* ospfd already fetched IP data from zebra */
  {
  	find = 0;
	LIST_LOOP(ospf->oiflist, oi, node)
	    if (oi->ifp && strcmp(oi->ifp->name, argv[0])==0)
	    {
	    	  find = 1;
	    	  break;
	    }
	if (!find)
	{
        vty_out (vty, "Could not find ospf interface %s %s", argv[0], VTY_NEWLINE);
        return CMD_WARNING;
	}
  }
  else /* in the middle of reading ospfd.conf */
  {
  	if (OspfTeConfigList)
		LIST_LOOP(OspfTeConfigList, oc, node)
		    if (strcmp(oc->if_name, argv[0])==0)
		    {
		        vty_out (vty, "Duplicate configuration for interface %s %s", argv[0], VTY_NEWLINE);
		        return CMD_WARNING;
		    }
  }
  	
 vty->node = OSPF_TE_IF_NODE;
 strcpy(OspfTeIfPrompt,"%s(config-te-if-");
 strcat(OspfTeIfPrompt,argv[0]);
 strcat(OspfTeIfPrompt,")# ");
 memset(&te_config, 0, sizeof(struct ospf_te_config_para));
 strcpy(te_config.if_name, argv[0]);
 te_config.configed = 1;
 return CMD_SUCCESS;
}

DEFUN (no_ospf_te_interface_ifname,
       no_ospf_te_interface_ifname_cmd,
       "no ospf-te interface INTERFACE",
       NO_STR
       "Configure OSPF-TE parameters\n"
       "Disable OSPF-TE functionality for an interface\n"
       "Interface name")
{
  struct ospf_interface *oi;
  struct listnode *node;
  struct interface *ifp;
  struct ospf *ospf = (struct ospf *)vty->index;
  
  if ((ifp = if_lookup_by_name (argv[0])) == NULL){
        vty_out (vty, "No such interface name %s %s", argv[0], VTY_NEWLINE);
        return CMD_WARNING;
  }
  if (ospf->oiflist)
  LIST_LOOP(ospf->oiflist, oi, node){
    if (oi->ifp == ifp){
	 ospf_te_del_if(oi);
	 return CMD_SUCCESS;
    }
  }
  vty_out (vty, "no_ospf_te_interface: cannot find ospf interface: %s %s", argv[0], VTY_NEWLINE);
  return CMD_WARNING;

}

DEFUN (ospf_te_router_addr,
       ospf_te_router_addr_cmd,
       "ospf-te router-address A.B.C.D",
       "OSPF-TE specific commands\n"
       "Stable IP address of the advertising router\n"
       "OSPF-TE router address in IPv4 address format\n")
{
  struct te_tlv_router_addr *ra = &OspfTeRouterAddr;
  struct in_addr value;
  struct ospf *ospf = (struct ospf *)vty->index;
  
  if (! inet_aton (argv[0], &value))
    {
      vty_out (vty, "Please specify Router-Addr by A.B.C.D%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (ntohs (ra->header.type) == 0
  ||  ntohl (ra->value.s_addr) != ntohl (value.s_addr))
    {
      struct listnode *area_node, *oi_node;
      struct ospf_area *area;
      struct ospf_interface *oi;

      set_ospf_te_router_addr (value);

   	LIST_LOOP(ospf->areas, area, area_node){	/* for each ospf area */
		OSPF_TIMER_OFF (area->t_te_area_lsa_rtid_self);
		OSPF_AREA_TIMER_ON (area->t_te_area_lsa_rtid_self,
			ospf_te_area_lsa_rtid_timer, OSPF_MIN_LS_INTERVAL);

		if (area->oiflist)
		LIST_LOOP(area->oiflist, oi, oi_node){	/* for each ospf interface */
			if (!INTERFACE_MPLS_ENABLED(oi))
				continue;
			
		      OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
		      OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_MIN_LS_INTERVAL);

	          if (INTERFACE_GMPLS_ENABLED(oi) &&
	          	ntohs(oi->te_para.lclif_ipaddr.header.type) == 0){
		      OSPF_TIMER_OFF (oi->t_te_linklocal_lsa_self);
		      OSPF_INTERFACE_TIMER_ON (oi->t_te_linklocal_lsa_self, ospf_te_linklocal_lsa_timer, OSPF_MIN_LS_INTERVAL);
	          }
		}
   	}
  }
  return CMD_SUCCESS;
}


/* <0-4294967296>*/
DEFUN (ospf_te_data_interface,
       ospf_te_data_interface_cmd,
       "data-interface ip A.B.C.D protocol (snmp|tl1) switch-ip A.B.C.D switch-port PORTNAME",
       "Associate an out-of-band data interface to this interface\n"
       "The IP address of the data interface\n"
       "IPv4 address\n"
       "Protocol type\n"
	"SNMP\n"
	"TL1\n"
	"The switch's control-plane IP address\n"
	"IPv4 address\n"
	"The switch's port\n"
	"32-bit integer\n")
{
  
  if (! inet_aton (argv[0], &te_config.vlsr_if.data_ip))
  {
      vty_out (vty, "Please specify the address by A.B.C.D%s", VTY_NEWLINE);
      return CMD_WARNING;
  }
  if (argc == 1)
  {
  	te_config.vlsr_if.protocol = VLSR_PROTO_NONE;
	te_config.vlsr_if.switch_ip.s_addr = 0;
	te_config.vlsr_if.switch_port = 0;
	return CMD_SUCCESS;
  }

  if (strncmp (argv[1], "s", 1) == 0)
	   te_config.vlsr_if.protocol = VLSR_PROTO_SNMP;
  else if (strncmp (argv[1], "t", 1) == 0)
	  te_config.vlsr_if.protocol = VLSR_PROTO_TL1;
  else
  {
      vty_out (vty, "Supported protocol type is [snmp|tl1]%s", VTY_NEWLINE);
      return CMD_WARNING;
  }
  if (! inet_aton (argv[2], &te_config.vlsr_if.switch_ip))
  {
      vty_out (vty, "Please specify the address by A.B.C.D%s", VTY_NEWLINE);
      return CMD_WARNING;
  }
  if (get_switch_port_by_name(argv[3], &te_config.vlsr_if.switch_port) != 1)
  {
      vty_out (vty, "get_switch_port_by_name: %s%s",argv[3], VTY_NEWLINE);
      return CMD_WARNING;
  }

  return CMD_SUCCESS;
}

ALIAS (ospf_te_data_interface,
       ospf_te_data_interface_noproto_cmd,
       "data-interface ip A.B.C.D",
       "Associate an out-of-band data interface to this interface\n"
       "The IP address of the data interface\n"
	"IPv4 address\n");

DEFUN (ospf_te_data_interface_unnum,
       ospf_te_data_interface_unnum_cmd,
       "data-interface unnumbered protocol (snmp|tl1) switch-ip A.B.C.D switch-port <0-4294967296>",
       "Associate an out-of-band data interface to this interface\n"
       "unnumbered interface\n"
       "Protocol type\n"
	"SNMP\n"
	"TL1\n"
	"The switch's control-plane IP address\n"
	"IPv4 address\n"
	"The switch's port\n"
	"32-bit integer\n")
{
  if (argc == 0)
  {
  	te_config.vlsr_if.protocol = VLSR_PROTO_NONE;
	te_config.vlsr_if.switch_ip.s_addr = 0;
	te_config.vlsr_if.switch_port = 0;
      te_config.vlsr_if.if_id = ospf_te_assign_lcl_ifid();
	return CMD_SUCCESS;
  }
  
  if (strncmp (argv[0], "s", 1) == 0)
	   te_config.vlsr_if.protocol = VLSR_PROTO_SNMP;
  else if (strncmp (argv[0], "t", 1) == 0)
	  te_config.vlsr_if.protocol = VLSR_PROTO_TL1;
  else
  {
      vty_out (vty, "Supported protocol type is [snmp|tl1]%s", VTY_NEWLINE);
      return CMD_WARNING;
  }
  if (! inet_aton (argv[1], &te_config.vlsr_if.switch_ip))
  {
      vty_out (vty, "Please specify the address by A.B.C.D%s", VTY_NEWLINE);
      return CMD_WARNING;
  }
  if ( get_switch_port_by_name(argv[2], &te_config.vlsr_if.switch_port) != 1)
  {
      vty_out (vty, "get_switch_port_by_name: %s%s",argv[3], VTY_NEWLINE);
      return CMD_WARNING;
  }

    te_config.vlsr_if.if_id = ospf_te_assign_lcl_ifid();
   return CMD_SUCCESS;
}

ALIAS (ospf_te_data_interface_unnum,
       ospf_te_data_interface_unnum_noproto_cmd,
       "data-interface unnumbered",
       "Associate an out-of-band data interface to this interface\n"
       "unnumbered interface\n"
       );



DEFUN (ospf_te_interface_level,
       ospf_te_interface_level_cmd,
       "level (mpls|gmpls)",
       "Set OSPF-TE level for an interface\n"
       "Enable MPLS functionality only\n"
       "Enable both MPLS and GMPLS functionality\n"
       "Enable VLSR functionality\n")
{
  u_char level;
  
  if (strncmp (argv[0], "m", 1) == 0)
	    level = INTERFACE_TE_MPLS;
  else if (strncmp (argv[0], "g", 1) == 0)
	    level = INTERFACE_TE_GMPLS;
  else
  {
      vty_out (vty, "level must be [mpls|gmpls]%s", VTY_NEWLINE);
      return CMD_WARNING;
  }

  te_config.level = level;
  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_metric,
       ospf_te_interface_metric_cmd,
       "metric <0-4294967295>",
       "Link metric for OSPF-TE purpose\n"
       "Metric\n")
{
  u_int32_t value;

  value = strtoul (argv[0], NULL, 10);

  set_linkparams_te_metric (&te_config.te_para.te_metric, value);
  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_maxbw,
       ospf_te_interface_maxbw_cmd,
       "max-bw BANDWIDTH",
       "Maximum bandwidth that can be used\n"
       "Bytes/second (IEEE floating point format)\n")
{
  float bw;

  if (sscanf (argv[0], "%g", &bw) != 1)
    {
      vty_out (vty, "ospf_te_link_maxbw: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }

  set_linkparams_max_bw (&te_config.te_para.max_bw, &bw);
  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_max_rsv_bw,
       ospf_te_interface_max_rsv_bw_cmd,
       "max-rsv-bw BANDWIDTH",
       "Maximum bandwidth that can be reserved\n"
       "Bytes/second (IEEE floating point format)\n")
{
  float bw;
  int i;
  if (sscanf (argv[0], "%g", &bw) != 1)
    {
      vty_out (vty, "ospf_te_link_maxbw: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }

  set_linkparams_max_rsv_bw (&te_config.te_para.max_rsv_bw, &bw);
  for (i=0; i< 8; i++)
	set_linkparams_unrsv_bw (&te_config.te_para.unrsv_bw, i, &bw);
  
  return CMD_SUCCESS;
}


DEFUN (ospf_te_interface_rsc_clsclr,
       ospf_te_interface_rsc_clsclr_cmd, 
       "color BITPATTERN",
       "Administrative group membership\n"
       "32-bit Hexadecimal value (ex. 0xa1)\n")
{
  unsigned long value;

  if (sscanf (argv[0], "0x%lx", &value) != 1)
    {
      vty_out (vty, "ospf_te_link_rsc_clsclr: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }

  set_linkparams_rsc_clsclr (&te_config.te_para.rsc_clsclr, value);
  return CMD_SUCCESS;
}

DEFUN (ospf_te_remote_ifid,
       ospf_te_remote_ifid_cmd,
       "remote-interface-id <0-4294967295>",
       "Remote interface ID for OSPF-TE purpose\n"
       "Metric\n")
{
  u_int32_t value;

  value = strtoul (argv[0], NULL, 10);

  set_linkparams_rmt_id(&te_config.te_para.link_lcrmt_id, value);
 return CMD_SUCCESS;
}


DEFUN (ospf_te_interface_protection_type,
       ospf_te_interface_protection_type_cmd,
       "protection (extra|none|shared|1t1|1p1|en)",
       "Link protection type for OSPF-TE purpose\n"
       "Extra traffic\n"
       "Unprotected\n"
       "Shared\n"
       "Dedicated 1:1\n"
       "Dedicated 1+1\n"
       "Enhanced (Ring, etc.)\n")
{
  u_char ptype;

  ptype = str2val(&str_val_conv_protection, argv[0]);
  if (ptype==0)
    {
      vty_out (vty, "unrecognized protection type: %s %s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }

  set_linkparams_protection_type (&te_config.te_para.link_protype, ptype);
 return CMD_SUCCESS;
}


DEFUN (ospf_te_interface_te_lambda,
       ospf_te_interface_te_lambda_cmd,
       "lambda (frequency|wavelength|channel) NUMBER",
       "Link TE lambda (DRAGON specific)\n"
       "In frequency format e.g. 192300 (ghz)\n"
       "In wavelength format e.g. 1539.77 (nm)\n"
       "Channel number e.g. 33\n"
       "Frequency/wavelength/channel number\n")
{
  u_int32_t freq;

  if (strncmp(argv[0], "f", 1) == 0)
  	sscanf(argv[1], "%u", &freq);
  else if (strncmp(argv[0], "c", 1) == 0)
  	freq = channel2frequency(argv[1]);
  else if (strncmp(argv[0], "w", 1) == 0)
  	freq = wavelength2frequency(argv[1]);

  if (freq < 192100 || freq > 196000 || freq%100 != 0)
    {
      vty_out (vty, "invalid lambda frequency: %u %s", freq, VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  set_linkparams_te_lambda(&te_config.te_para.link_te_lambda, freq);
  return CMD_SUCCESS;
}


DEFUN (ospf_te_interface_srlg,
       ospf_te_interface_srlg_cmd,
       "srlg (add|delete) <0-4294967295>",
       "Link protection type for OSPF-TE purpose\n"
       "Add an SRLG value\n"
       "Delete an SRLG value\n")
{
  u_char action;
  u_char value;
  u_int32_t *v = NULL;
  struct listnode *node;
  struct ospf_interface *oi = NULL;
  struct ospf *ospf = (struct ospf*)(vty->index);
  
  if (strncmp (argv[0], "a", 1) == 0)
    action = LINK_SRLG_ADD_VALUE;
  else if (strncmp (argv[0], "d", 1) == 0)
    action = LINK_SRLG_DEL_VALUE;
  else
    {
      vty_out (vty, "srlg action must be <add|delete> %s %s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }
  value = strtoul (argv[1], NULL, 10);

  if (ospf->oiflist)
	LIST_LOOP(ospf->oiflist, oi, node)
	    if (oi->ifp && strcmp(oi->ifp->name, te_config.if_name)==0)
	    {
		   if (oi->te_para.link_srlg.srlg_list)
		   {
			  LIST_LOOP(oi->te_para.link_srlg.srlg_list, v, node)
			  {
			  	set_linkparams_srlg(&te_config.te_para.link_srlg, ntohl(*v), LINK_SRLG_ADD_VALUE);
			  }
		   }
	    	  break;
	    }
  set_linkparams_srlg (&te_config.te_para.link_srlg, value, action);	
  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_ifsw_cap1,
       ospf_te_interface_ifsw_cap1_cmd,
       "swcap (psc1|psc2|psc3|psc4|l2sc|tdm|lsc|fsc) encoding (packet|ethernet|pdh|sdh|dwrapper|lambda|fiber|fchannel)",
       "Link switching capability and encoding type for OSPF-TE purpose\n"
       "Packet-Switch Capable-1\n"
       "Packet-Switch Capable-2\n"
       "Packet-Switch Capable-3\n"
       "Packet-Switch Capable-4\n"
       "Layer-2 Switch Capable\n"
       "Time-Division-Multiplex Capable\n"
       "Lambda-Switch Capable\n"
       "Fiber-Switch Capable\n"
       "Encoding type\n"
       "Packet\n"
       "Ethernet\n"
       "ANSI/ETSI PDH\n"
       "SDH ITU-T G.707 / SONET ANSI T1.105\n"
       "Digital Wrapper\n"
       "Lambda (photonic)\n"
       "Fiber\n"
       "FiberChannel\n" )
{
  u_char swcap = 0, encoding = 0;

  swcap = str2val(&str_val_conv_swcap, argv[0]);
  if (swcap == 0)
  {
      vty_out (vty, "Invalid switching capability %s %s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
   }
  encoding = str2val(&str_val_conv_encoding, argv[1]);
  if (encoding==0)
 {
      vty_out (vty, "Invalid encoding type %s %s", argv[1], VTY_NEWLINE);
      return CMD_WARNING;
   }

  set_linkparams_ifsw_cap1(&te_config.te_para.link_ifswcap, swcap, encoding);
  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_ifsw_cap2,
       ospf_te_interface_ifsw_cap2_cmd,
       "max-lsp-bw <0-7> BANDWIDTH",
       "Max LSP bandwidth (Only applied to GMPLS-OSPF)\n"
       "Priority\n"
       "Bytes/second (IEEE floating point format)\n")
{
  int priority;
  float bw;

  if (sscanf (argv[0], "%d", &priority) != 1)
    {
      vty_out (vty, "ospf_te_link_ifsw_cap2: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (sscanf (argv[1], "%g", &bw) != 1)
    {
      vty_out (vty, "ospf_te_link_ifsw_cap2: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }

  htonf (&bw, &te_config.te_para.link_ifswcap.link_ifswcap_data.max_lsp_bw_at_priority[priority]);
  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_ifsw_cap3a,
       ospf_te_interface_ifsw_cap3a_cmd,
       "min-lsp-bw BANDWIDTH mtu <1-65535>",
       "Interface switching specific info (Only applied to GMPLS-OSPF)\n"
       "Minimum LSP bandwidth\n"
       "Bytes/second (IEEE floating point format)\n"
       "MTU\n")
{
  u_int32_t mtu;
  float bw;

  if (sscanf (argv[0], "%g", &bw) != 1)
    {
      vty_out (vty, "ospf_te_link_ifsw_cap3a: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  if (sscanf (argv[1], "%d", &mtu) != 1)
    {
      vty_out (vty, "ospf_te_link_ifsw_cap3a: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  htonf (&bw, &te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.min_lsp_bw);
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc.mtu = htons(mtu);

  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_ifsw_cap3b,
       ospf_te_interface_ifsw_cap3b_cmd,
       "min-lsp-bw BANDWIDTH indication <0-1>",
       "Interface switching specific info (Only applied to GMPLS-OSPF)\n"
       "Minimum LSP bandwidth\n"
       "Bytes/second (IEEE floating point format)\n"
       "Indication\n")
{
  int indication;
  float bw;

  if (sscanf (argv[0], "%g", &bw) != 1)
    {
      vty_out (vty, "ospf_te_link_ifsw_cap3b: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  if (sscanf (argv[1], "%d", &indication) != 1)
    {
      vty_out (vty, "ospf_te_link_ifsw_cap3b: fscanf: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  htonf (&bw, &te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.min_lsp_bw);
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm.indication = indication;
  return CMD_SUCCESS;
}


DEFUN (ospf_te_interface_ifsw_cap4,
       ospf_te_interface_ifsw_cap4_cmd,
       "vlan <1-4094>",
       "Assign this port/IP to a tagged VLAN\n"
       "Tagged VLAN ID in the range [1, 4095]\n")
{
  u_int32_t vlan, vlan1, vlan2;

  if  (te_config.te_para.link_ifswcap.link_ifswcap_data.switching_cap != LINK_IFSWCAP_SUBTLV_SWCAP_L2SC)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap4: no 'vlan' command on non-L2SC link.%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  	
  if (sscanf (argv[0], "%d", &vlan1) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap4: fscanf vlan1: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }

  te_config.te_para.link_ifswcap.header.length = htons(36 + sizeof(struct link_ifswcap_specific_vlan));

  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.length = htons(sizeof(struct link_ifswcap_specific_vlan));
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version = htons(IFSWCAP_SPECIFIC_VLAN_BASIC | IFSWCAP_SPECIFIC_VLAN_ALLOC);

  if (argc == 1) 
    {
	SET_VLAN(te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, vlan1);
    }
  else if (argc == 2 ) 
    {
	  if (sscanf (argv[1], "%d", &vlan2) != 1)
	    {
	      vty_out (vty, "ospf_te_interface_ifsw_cap4: fscanf vlan2: %s%s", strerror (errno), VTY_NEWLINE);
	      return CMD_WARNING;
	    }
	  else if (vlan2 < vlan1)
	    {
	      vty_out (vty, "ospf_te_interface_ifsw_cap4: VLAN ID2 < ID1%s", VTY_NEWLINE);
	      return CMD_WARNING;
	    }
	  for(vlan = vlan1; vlan <= vlan2; vlan++)
	      SET_VLAN(te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, vlan);
    }
  else
    {
        vty_out (vty, "ospf_te_interface_ifsw_cap4: invalid command%s", VTY_NEWLINE);
	 return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

ALIAS (ospf_te_interface_ifsw_cap4,
       ospf_te_interface_ifsw_cap4a_cmd,
       "vlan <1-4094> to <2-4095>",
       "Assign this port/IP to a tagged VLAN\n"
       "Tagged VLAN ID1 in the range [1, 4094]\n"
       "Tagged VLAN ID2 in the range [2, 4095]\n");


DEFUN (ospf_te_interface_ifsw_cap5,
       ospf_te_interface_ifsw_cap5_cmd,
       "subnet-uni <1-65535> node-name NAME tna-ipv4 A.B.C.D uni-n-ipv4  A.B.C.D control-channel NAME data-interface  A.B.C.D port ID egress-label <0-4294967295> upstream-label <0-4294967295>",
       "Assign Subnet UNI parameters\n"
       "Subnet UNI ID (16 bits)\n"
       "Node name\n"
       "Name string\n"
       "UNI TNA IP\n"
       "IPv4 address\n"
       "UNI-Nework node ID\n"
       "IPv4 address\n"
       "Control channel for the UNI\n"
       "Control channel name string\n"
       "Logical data port\n"
       "Port number in bay-shelf-slot-subslot-port format\n"
       "Egress label, downstream\n"
       "Egress label (32 bits)\n"
       "Egress label, upstream\n"
       "Egress label (32 bits)\n")
{
  u_int32_t uni_id, tna_ip, uni_n_ip, data_if, data_port, egress_label, egress_label_upstream;

  if  (te_config.te_para.link_ifswcap.link_ifswcap_data.switching_cap != LINK_IFSWCAP_SUBTLV_SWCAP_L2SC)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: no 'subnet-uni' command on non-L2SC link.%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  	
  if (argc != 9) 
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: only %d parameters present%s (needing eight)", argc, VTY_NEWLINE);
      return CMD_WARNING;
    }

  te_config.te_para.link_ifswcap.header.length = htons(36 + sizeof(struct link_ifswcap_specific_subnet_uni)); /*reassign length...*/

  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.length = htons(sizeof(struct link_ifswcap_specific_subnet_uni));
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.version = htons(IFSWCAP_SPECIFIC_SUBNET_UNI);

  if (sscanf (argv[0], "%d", &uni_id) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: fscanf uni_id: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.subnet_uni_id = (u_int8_t)uni_id;

  strncpy(te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.node_name, argv[1], 14);

  if (inet_aton (argv[2], (struct in_addr*)&tna_ip) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: inet_aton tna_ip: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.tna_ipv4 = tna_ip;

  if (inet_aton (argv[3], (struct in_addr*)&uni_n_ip) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: inet_aton uni_n_ip: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.nid_ipv4 = uni_n_ip;

  strncpy(te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.control_channel, argv[4], 11);

  if (inet_aton (argv[5], (struct in_addr*)&data_if) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: inet_aton data_if: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.data_ipv4 = data_if;

  data_port = logical_port_string2number((const char*)argv[6]);
  if (data_port == 0)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: unacceptable data_port: %s%s", argv[6], VTY_NEWLINE);
      return CMD_WARNING;
    }
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.logical_port_number = htonl(data_port);
  
  if (sscanf (argv[7], "%d", &egress_label) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: fscanf egress_label: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.egress_label_downstream = htonl(egress_label);
  
  if (sscanf (argv[8], "%d", &egress_label_upstream) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap5: fscanf egress_label_upstream: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.egress_label_upstream = htonl(egress_label_upstream);
  
  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_ifsw_cap6,
       ospf_te_interface_ifsw_cap6_cmd,
       "subnet-uni swcap-ext (l2sc|tdm|lsc|fsc) encoding-ext (packet|ethernet|pdh|sdh|dwrapper|lambda|fiber|fchannel)",
       "Subnet-UNI\n"
       "switching type\n"
       "Layer-2 Switch Capable\n"
       "Time-Division-Multiplex Capable\n"
       "Lambda-Switch Capable\n"
       "Fiber-Switch Capable\n"
       "Encoding type\n"
       "Packet\n"
       "Ethernet\n"
       "ANSI/ETSI PDH\n"
       "SDH ITU-T G.707 / SONET ANSI T1.105\n"
       "Digital Wrapper\n"
       "Lambda (photonic)\n"
       "Fiber\n")
{
  u_char swcap = 0, encoding = 0;

  if ( te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.version != htons(IFSWCAP_SPECIFIC_SUBNET_UNI) )
  {
      vty_out (vty, "subnet-uni <1-65535> node-name NAME tna-ipv4 ... must be defined first ... %s", VTY_NEWLINE);
      return CMD_ERR_INCOMPLETE;
  }
  
  swcap = str2val(&str_val_conv_swcap, argv[0]);
  if (swcap == 0)
  {
      vty_out (vty, "Invalid extended switching capability %s %s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
  }
  encoding = str2val(&str_val_conv_encoding, argv[1]);
  if (encoding==0)
  {
      vty_out (vty, "Invalid extended encoding type %s %s", argv[1], VTY_NEWLINE);
      return CMD_WARNING;
  }

  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.swcap_ext = swcap;
  te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.encoding_ext = encoding;

  return CMD_SUCCESS;
}

DEFUN (ospf_te_interface_ifsw_cap7,
       ospf_te_interface_ifsw_cap7_cmd,
       "timeslot <1-192>",
       "Assign timeslots\n"
       "TimeSlot ID in the range [1, 192]\n")
{
  u_int32_t ts, ts1, ts2;

  if ( te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.version != htons(IFSWCAP_SPECIFIC_SUBNET_UNI) )
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap7: 'timeslot' command must follow 'subnet-uni' command.%s", VTY_NEWLINE);
      return CMD_ERR_INCOMPLETE;
    }
  	
  if (sscanf (argv[0], "%d", &ts1) != 1)
    {
      vty_out (vty, "ospf_te_interface_ifsw_cap7: fscanf ts1: %s%s", strerror (errno), VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (argc == 1) 
    {
	SET_TIMESLOT(te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.timeslot_bitmask, ts1);
    }
  else if (argc == 2 ) 
    {
	  if (sscanf (argv[1], "%d", &ts2) != 1)
	    {
	      vty_out (vty, "ospf_te_interface_ifsw_cap7: fscanf ts2: %s%s", strerror (errno), VTY_NEWLINE);
	      return CMD_WARNING;
	    }
	  else if (ts2 < ts1)
	    {
	      vty_out (vty, "ospf_te_interface_ifsw_cap7: TimeSlot ID2 < ID1%s", VTY_NEWLINE);
	      return CMD_WARNING;
	    }
	  for(ts = ts1; ts <= ts2; ts++)
	      SET_TIMESLOT(te_config.te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.timeslot_bitmask, ts);
    }
  else
    {
        vty_out (vty, "ospf_te_interface_ifsw_cap7: invalid command%s", VTY_NEWLINE);
	 return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

ALIAS (ospf_te_interface_ifsw_cap7,
       ospf_te_interface_ifsw_cap7a_cmd,
       "timeslot <1-192> to <2-192>",
       "Assign timeslots\n"
       "TimeSlot ID1 in the range [1, 192]\n"
       "TimeSlot ID2 in the range [2, 192]\n");


DEFUN (show_ospf_te_router,
       show_ospf_te_router_cmd,
       "show ip ospf-te router",
       SHOW_STR
       IP_STR
       "OSPF-TE information\n"
       "TE router address\n")
{
  if (ntohs(OspfTeRouterAddr.header.type) == TE_TLV_ROUTER_ADDR)
  {
      vty_out (vty, "--- OSPF-TE router parameters ---%s", VTY_NEWLINE);
      show_vty_router_addr (vty, &OspfTeRouterAddr.header);
  }
  else if (vty != NULL)
        vty_out (vty, "  N/A%s", VTY_NEWLINE);
  return CMD_SUCCESS;
}

/*@@@@ UNI hacks ==> Obsolete*/
/*
DEFUN (ospf_te_uni_loopback,
       ospf_te_uni_loopback_cmd,
       "loopback A.B.C.D",
       "Assign UNI client loopback address\n"
       "IP address A.B.C.D\n")
{
  if (argc != 1 || ! inet_aton(argv[0], &te_config.uni_loopback)) {
      vty_out (vty, "Please specify the UNI loopback address by A.B.C.D%s", VTY_NEWLINE);
      return CMD_WARNING;
  }
  return CMD_SUCCESS;
}
*/

static void
show_ospf_te_link_sub_detail (struct vty *vty, struct ospf_interface *oi)
{
  int i;
  char data_ip_address[20];
  char switch_ip_address[20];

  if (INTERFACE_MPLS_ENABLED(oi)
  &&  (! if_is_loopback (oi->ifp) && if_is_up (oi->ifp) && ospf_oi_count (oi->ifp) > 0))
    {
      vty_out (vty, "-- OSPF-TE link parameters for %s --%s",
               inet_ntoa(oi->address->u.prefix4), VTY_NEWLINE);
	if (oi->vlsr_if.data_ip.s_addr != 0)
	{
		if (oi->vlsr_if.protocol == VLSR_PROTO_SNMP)
			vty_out(vty, "Protocol type is SNMP, ");
		else if (oi->vlsr_if.protocol == VLSR_PROTO_TL1)
			vty_out(vty, "Protocol type is TL1, ");
		else
			vty_out(vty, "Protocol type is UNKNOWN, ");
		  if (oi->vlsr_if.switch_ip.s_addr!=0)
                  {
                          strcpy(data_ip_address, inet_ntoa(oi->vlsr_if.data_ip));
                          strcpy(switch_ip_address, inet_ntoa(oi->vlsr_if.switch_ip));
			  vty_out(vty, "Data interface is numbered , IP = %s , Switch IP = %s, Switch port = %d (%s),  ", 
							  data_ip_address, switch_ip_address, oi->vlsr_if.switch_port, get_switch_port_string(oi->vlsr_if.switch_port));
                  }
		  else
			vty_out(vty, "Data interface is numbered, IP = %s,  ",  inet_ntoa (oi->vlsr_if.data_ip));
			if (oi->te_para.link_ifswcap.link_ifswcap_data.switching_cap == LINK_IFSWCAP_SUBTLV_SWCAP_L2SC) {
			  	if (ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_BASIC) {
				  vty_out(vty, "Assigned VLAN tags:");
				  for (i = 1; i <= MAX_VLAN_NUM; i++)
				  	if (HAS_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, i))
					    vty_out(vty, " %d", i);
				  vty_out(vty, "%s", VTY_NEWLINE);
			  	}
				else
				  vty_out(vty, "No VLAN tag assigned %s", VTY_NEWLINE);

			  	if (ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_ALLOC) {
				  vty_out(vty, "VLAN tags in use (allocated):");
				  for (i = 1; i <= MAX_VLAN_NUM; i++)
				  	if (HAS_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask_alloc, i))
					    vty_out(vty, " %d", i);
				  vty_out(vty, "%s", VTY_NEWLINE);
			  	}
				else
				  vty_out(vty, "No VLAN tag in use (allocated) %s", VTY_NEWLINE);
			}
	}
	  else
	  {
		  if (oi->vlsr_if.protocol == VLSR_PROTO_SNMP)
			  vty_out(vty, "Protocol type is SNMP, ");
		  else if (oi->vlsr_if.protocol == VLSR_PROTO_TL1)
			  vty_out(vty, "Protocol type is TL1, ");
		  else
			  vty_out(vty, "Protocol type is UNKNOWN, ");
		  if (oi->vlsr_if.switch_ip.s_addr!=0)
			  vty_out(vty, "Data interface is unnumbered, Switch IP %s, Switch port %d , Interface ID = 0x%x %s", 
							  inet_ntoa(oi->vlsr_if.switch_ip), oi->vlsr_if.switch_port, get_switch_port_string(oi->vlsr_if.switch_port), oi->vlsr_if.if_id, VTY_NEWLINE);
		  else
			  vty_out(vty, "Data interface is unnumbered, Interface ID = 0x%x%s", oi->vlsr_if.if_id, VTY_NEWLINE);
		  	
	  }

       if (oi->te_para.instance)
       {
	      vty_out (vty, "TE-link LSA instance is %d --%s",
	               oi->te_para.instance, VTY_NEWLINE);
       }
	if (ntohs(oi->te_para.link_type.header.type)!=0)
	       show_vty_link_subtlv_link_type (vty, &oi->te_para.link_type.header);
	if (ntohs(oi->te_para.link_id.header.type)!=0)
      	       show_vty_link_subtlv_link_id (vty, &oi->te_para.link_id.header);
	if (ntohs(oi->te_para.lclif_ipaddr.header.type)!=0)
      		show_vty_link_subtlv_lclif_ipaddr (vty, &oi->te_para.lclif_ipaddr.header);
	if (ntohs(oi->te_para.rmtif_ipaddr.header.type)!=0)
	       show_vty_link_subtlv_rmtif_ipaddr (vty, &oi->te_para.rmtif_ipaddr.header);
	if (ntohs(oi->te_para.te_metric.header.type)!=0)
	      show_vty_link_subtlv_te_metric (vty, &oi->te_para.te_metric.header);
	if (ntohs(oi->te_para.max_bw.header.type)!=0)
	      show_vty_link_subtlv_max_bw (vty, &oi->te_para.max_bw.header);
	if (ntohs(oi->te_para.max_rsv_bw.header.type)!=0)
	      show_vty_link_subtlv_max_rsv_bw (vty, &oi->te_para.max_rsv_bw.header);
	if (ntohs(oi->te_para.unrsv_bw.header.type)!=0)
	      show_vty_link_subtlv_unrsv_bw (vty, &oi->te_para.unrsv_bw.header);
	if (ntohs(oi->te_para.rsc_clsclr.header.type)!=0)
	      show_vty_link_subtlv_rsc_clsclr (vty, &oi->te_para.rsc_clsclr.header);
	
      if (INTERFACE_GMPLS_ENABLED(oi)){
      	   if (ntohs(oi->te_para.link_lcrmt_id.header.type)!=0)
	          show_vty_link_subtlv_lcrmt_id(vty, &oi->te_para.link_lcrmt_id.header);
      	   if (ntohs(oi->te_para.link_protype.header.type)!=0)
	          show_vty_link_subtlv_protection_type(vty, &oi->te_para.link_protype.header);
      	   if (ntohs(oi->te_para.link_ifswcap.header.type)!=0)
	          show_vty_link_subtlv_ifsw_cap_local(vty, &oi->te_para.link_ifswcap.header);
      	   if (ntohs(oi->te_para.link_srlg.header.type)!=0)
	          show_vty_link_subtlv_srlg(vty, &oi->te_para.link_srlg.header, 1);
      	   if (ntohs(oi->te_para.link_te_lambda.header.type)!=0)
	          show_vty_link_subtlv_te_lambda(vty, &oi->te_para.link_te_lambda.header);
      }
  }
  else
    {
      vty_out (vty, "  %s: OSPF-TE is disabled on this interface%s",
               IF_NAME(oi), VTY_NEWLINE);
    }

  return;
}

static void
show_ospf_te_link_sub_brief (struct vty *vty, struct ospf_interface *oi)
{

  if (INTERFACE_MPLS_ENABLED(oi)
  &&  (! if_is_loopback (oi->ifp) && if_is_up (oi->ifp) && ospf_oi_count (oi->ifp) > 0))
    {
      vty_out (vty, "  %s: OSPF-TE is enabled on this interface%s",
               IF_NAME(oi), VTY_NEWLINE);
  }
  else
    {
      vty_out (vty, "  %s: OSPF-TE is disabled on this interface%s",
               IF_NAME(oi), VTY_NEWLINE);
    }

  return;
}

/* Be aware that if alias IP address is enabled then this command may return improper result *XXX* */
DEFUN (show_ospf_te_interface_ifname,
       show_ospf_te_interface_ifname_cmd,
       "show ip ospf-te interface INTERFACE",
       SHOW_STR
       IP_STR
       "OSPF-TE information\n"
       "TE interface information\n"
       "Interface name\n")
{
  struct interface *ifp;
  listnode node1, node2, node3;
  struct ospf *ospf;
  struct ospf_interface *oi;
  
  /* Show All Interfaces. */
  if (argc == 0){
	LIST_LOOP (om->ospf, ospf, node1){		/* for each ospf instance */
	  	LIST_LOOP(ospf->oiflist, oi, node2){
		         show_ospf_te_link_sub_brief (vty, oi);
	  	}
	}
  }
  else{ /* Interface name is specified. */
      if ((ifp = if_lookup_by_name (argv[0])) == NULL)
        vty_out (vty, "No such interface name%s", VTY_NEWLINE);
      else{
		LIST_LOOP (om->ospf, ospf, node2){		/* for each ospf instance */
		  	LIST_LOOP(ospf->oiflist, oi, node3){
			    if (oi->ifp == ifp){
			         show_ospf_te_link_sub_detail (vty, oi);
				  return CMD_SUCCESS;
			    }
		  	}
		}
      } /* End of else */
  } /* End of else */

  return CMD_SUCCESS;
}

ALIAS (show_ospf_te_interface_ifname,
       show_ospf_te_interface_all_cmd,
       "show ip ospf-te interface",
       SHOW_STR
       IP_STR
       "OSPF-TE information\n"
       "TE interface information\n");


DEFUN (show_ospf_te_db,
       show_ospf_te_db_cmd,
       "show ip ospf-te database (brief|detail)",
       SHOW_STR
       IP_STR
       "OSPF-TE information\n"
       "TE database summary\n")
{
  listnode node1, node2;
  struct ospf *ospf;
  struct ospf_area *area;
  u_char detail;
  struct ospf_te_lsdb *db;
  struct ospf_lsa *lsa;
  struct route_node *rn;

  /* Show All TE links. */
  if ( argc > 0 && strncmp (argv[0], "d", 1) == 0)
  	detail = 1;
  else 
  	detail = 0;

  LIST_LOOP (om->ospf, ospf, node1)
  {		/* for each ospf instance */
	LIST_LOOP(ospf->areas, area, node2)
	{
		vty_out (vty, "OSPF-TE link state database, area %s %s", inet_ntoa(area->area_id), VTY_NEWLINE);
		vty_out(vty, "Type\tID\tAdv Rtr\tSeq\tAge\tCksum\tLen\t %s", VTY_NEWLINE);
		db = area->te_rtid_db;
		LSDB_LOOP (db->db, rn, lsa)
		{
			vty_out (vty, "Area-Router ID TLV\t");
			vty_out (vty, "%s\t", inet_ntoa(lsa->data->id));
			vty_out (vty, "%s\t", inet_ntoa(lsa->data->adv_router));
			vty_out (vty, "0x%x\t", ntohs(lsa->data->ls_seqnum));
			vty_out (vty, "%d\t", ntohs(lsa->data->ls_age));
			vty_out (vty, "0x%x\t", ntohs(lsa->data->checksum));
			vty_out (vty, "%d\t", ntohs(lsa->data->length));
			vty_out (vty, "%s", VTY_NEWLINE);
			if (detail) /* show LSDB in detail */
				ospf_te_show_info (vty, lsa);
			vty_out (vty, "%s", VTY_NEWLINE);
		}
		db = area->te_lsdb;
		LSDB_LOOP (db->db, rn, lsa)
		{
			vty_out (vty, "Area-Link TLV\t");
			vty_out (vty, "%s\t", inet_ntoa(lsa->data->id));
			vty_out (vty, "%s\t", inet_ntoa(lsa->data->adv_router));
			vty_out (vty, "0x%x\t", ntohs(lsa->data->ls_seqnum));
			vty_out (vty, "%d\t", ntohs(lsa->data->ls_age));
			vty_out (vty, "0x%x\t", ntohs(lsa->data->checksum));
			vty_out (vty, "%d\t", ntohs(lsa->data->length));
			vty_out (vty, "%s", VTY_NEWLINE);
			if (detail) /* show LSDB in detail */
				ospf_te_show_info (vty, lsa);
			vty_out (vty, "%s", VTY_NEWLINE);
		}
	  }
   }

  return CMD_SUCCESS;
}

ALIAS (show_ospf_te_db,
       show_ospf_te_db_brief_cmd,
       "show ip ospf-te database",
       SHOW_STR
       IP_STR
       "OSPF-TE information\n"
       "TE database summary\n");

static void
ospf_te_register_vty (void)
{
  install_node (&ospf_te_link_node, NULL);
  
  install_element (VIEW_NODE, &show_ospf_te_router_cmd);
  install_element (VIEW_NODE, &show_ospf_te_interface_ifname_cmd);
  install_element (VIEW_NODE, &show_ospf_te_interface_all_cmd);
  install_element (VIEW_NODE, &show_ospf_te_db_brief_cmd);
  install_element (VIEW_NODE, &show_ospf_te_db_cmd);
  install_element (ENABLE_NODE, &show_ospf_te_router_cmd);
  install_element (ENABLE_NODE, &show_ospf_te_interface_ifname_cmd);
  install_element (ENABLE_NODE, &show_ospf_te_interface_all_cmd);
  install_element (ENABLE_NODE, &show_ospf_te_db_cmd);
  install_element (ENABLE_NODE, &show_ospf_te_db_brief_cmd);

  install_element (OSPF_NODE, &ospf_te_router_addr_cmd);
  install_element (OSPF_NODE, &ospf_te_interface_ifname_cmd);
  /*@@@@ UNI hacks ==> Obsolete*/
  /*
  install_element (OSPF_NODE, &ospf_te_uni_ifname_cmd);
  install_element (OSPF_NODE, &no_ospf_te_uni_ifname_cmd);
  */

  install_default(OSPF_TE_IF_NODE);
  install_element (OSPF_TE_IF_NODE, &ospf_te_data_interface_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_data_interface_noproto_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_data_interface_unnum_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_data_interface_unnum_noproto_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_level_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_metric_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_maxbw_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_max_rsv_bw_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_rsc_clsclr_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_remote_ifid_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_protection_type_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_srlg_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap1_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap2_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap3a_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap3b_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap4_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap4a_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap5_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap6_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap7_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_ifsw_cap7a_cmd);
  install_element (OSPF_TE_IF_NODE, &ospf_te_interface_te_lambda_cmd);

  set_config_end_call_back_func(ospf_te_interface_config_update);
  
  return;
}

#endif /* HAVE_OSPF_TE */
