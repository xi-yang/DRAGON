/*
 * Zebra connect library for OSPFd
 * Copyright (C) 1997, 98, 99, 2000 Kunihiro Ishiguro, Toshiaki Takada
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 */


#include <zebra.h>

#include "thread.h"
#include "command.h"
#include "network.h"
#include "prefix.h"
#include "routemap.h"
#include "table.h"
#include "stream.h"
#include "memory.h"
#include "zclient.h"
#include "filter.h"
#include "log.h"
#include "sockunion.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_te_lsa.h"
#include "ospfd/ospf_te_lsdb.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_abr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_zebra.h"
#include "ospfd/ospf_lsdb.h"
#include <sys/un.h>

#ifdef HAVE_OPAQUE_LSA

#ifdef HAVE_SNMP
#include "ospfd/ospf_snmp.h"
#endif /* HAVE_SNMP */

/* For registering threads. */
extern struct thread_master *master;

extern void delete_gri(struct ospf_interface*, u_int32_t, u_int32_t);

enum OspfRsvpMessage {
	OspfResv = 2,
	OspfPathTear = 5,
	OspfResvTear = 6,
	GetExplicitRouteByOSPF = 128,	//Get explicit route from OSPF
	FindInterfaceByData = 129,		//Find control logical interface by data plane IP / interface ID
	FindDataByInterface = 130,			//Find data plane IP / interface ID by control logical interface
	FindOutLifByOSPF = 131, 		//Find outgoing control logical interface by next hop data plane IP / interface ID
	GetVLSRRoutebyOSPF = 132,		//Get VLSR route
	GetLoopbackAddress = 133,		// Get its loopback address
	HoldVtagbyOSPF = 134,		// Hold or release a VLAN Tag
	HoldBandwidthbyOSPF = 135, 		// Hold or release a portion of bandwidth
	GetSubnetUNIDataByOSPF = 136,	// Get Subnet UNI data associated with a OSPF interface
	HoldTimeslotsbyOSPF = 137,		// Hold or release timeslots
};


static u_int32_t get_slash30_peer_address(u_int32_t addr)
{
	u_int32_t peer_addr = addr & 0xfcffffff;

	if (htonl(ntohl(peer_addr)+1) == addr)
		peer_addr = htonl(ntohl(peer_addr)+2);
	else
		peer_addr = htonl(ntohl(peer_addr)+1);

	return peer_addr;
}

#define IN_SAME_SLASH30(X, Y) (X.s_addr == Y.s_addr || X.s_addr == get_slash30_peer_address(Y.s_addr))

/*
Find control logical interface by data plane IP / interface ID 
*/
void
ospf_find_interface_by_data(struct in_addr *addr, u_int32_t if_id, int fd)
{
	struct ospf_interface *oi;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	struct stream *s;
	u_int8_t length;

	if (IS_VALID_LCL_IFID(if_id)) /* unnumbered interface */
	{
		/*if ((ntohl(OspfTeRouterAddr.value.s_addr)==addr->s_addr || ) && om->ospf)*/
		if (OspfTeRouterAddr.value.s_addr == addr->s_addr && om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_GMPLS_ENABLED(oi) &&
					ntohs(oi->te_para.link_lcrmt_id.header.type)!=0 &&
					ntohl(oi->te_para.link_lcrmt_id.link_local_id) == if_id)
				{
					length = sizeof(u_int8_t)*2 + sizeof(struct in_addr);
					s = stream_new(length);
					stream_putc(s, length);
					stream_putc(s, FindInterfaceByData);
					stream_put_ipv4(s, oi->address->u.prefix4.s_addr);
					/* Send message.  */
					write (fd, STREAM_DATA(s), length);
					goto out;
					
				}
			 }
		}
	}
	else{	/* numbered interface */
		if (om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_MPLS_ENABLED(oi) &&
					ntohs(oi->te_para.lclif_ipaddr.header.type)!=0 &&
					oi->te_para.lclif_ipaddr.value.s_addr == addr->s_addr)
				{
					length = sizeof(u_int8_t)*2 + sizeof(struct in_addr);
					s = stream_new(length);
					stream_putc(s, length);
					stream_putc(s, FindInterfaceByData);
					stream_put_ipv4(s, oi->address->u.prefix4.s_addr);
					/* Send message.  */
					write (fd, STREAM_DATA(s), length);
					goto out;
					
				}
			 }
		}
	}
	length = sizeof(u_int8_t)*2;
	s = stream_new(length);
	stream_putc(s, length);
	stream_putc(s, FindInterfaceByData);
	/* Send message.  */
	write (fd, STREAM_DATA(s), length);
	
out:
	stream_free(s);
	return;
}

/*
Find data plane IP / interface ID by control logical interface 
*/
void
ospf_find_data_by_interface(struct in_addr *addr, int fd)
{
	struct ospf_interface *oi;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	struct stream *s;
	u_int8_t length;
	struct in_addr data_addr;
	u_int32_t data_local_id = 0;

	data_addr.s_addr = 0;
	LIST_LOOP(om->ospf, ospf, node1)
	{
		if (ospf->oiflist)
		LIST_LOOP(ospf->oiflist, oi, node2){
			if (INTERFACE_GMPLS_ENABLED(oi) &&
			    oi->address->u.prefix4.s_addr == addr->s_addr)
			{
				if (ntohs(oi->te_para.lclif_ipaddr.header.type)!=0) /* numbered interface */
					data_addr.s_addr = oi->te_para.lclif_ipaddr.value.s_addr;
				else if (ntohs(oi->te_para.link_lcrmt_id.header.type)!=0) /* un-numbered interface */
				{
					data_addr.s_addr = OspfTeRouterAddr.value.s_addr;
					data_local_id = ntohl(oi->te_para.link_lcrmt_id.link_local_id);
				}
				goto out;
			}
		 }
	}
	
out:
	if (data_addr.s_addr!=0){
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t);
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, FindDataByInterface);
		stream_put_ipv4(s, data_addr.s_addr);
		/*
		if (data_local_id == 0 && (fd >> 16) != 0)
			data_local_id = fd;
		*/
		stream_putl(s, data_local_id);
	}
	else{
		length = sizeof(u_int8_t)*2;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, FindDataByInterface);
	}
	/* Send message.  */
	write (fd, STREAM_DATA(s), length);
	stream_free(s);
	return;
}


/* Find outgoing control logical interface by next hop data plane IP / interface ID */
void
ospf_find_out_lif(struct in_addr *addr,  u_int32_t if_id, int fd)
{
	struct ospf_interface *oi;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	struct stream *s = NULL;
	u_int8_t length;
	
	if (om->ospf)
	LIST_LOOP(om->ospf, ospf, node1)
	{
		if (ospf->oiflist)
		LIST_LOOP(ospf->oiflist, oi, node2){
			if (if_id == 0 && INTERFACE_MPLS_ENABLED(oi) &&
			    ntohs(oi->te_para.lclif_ipaddr.header.type)!=0 &&
			    IN_SAME_SLASH30(oi->te_para.lclif_ipaddr.value, (*addr)))
			{
				length = sizeof(u_int8_t)*2 + sizeof(struct in_addr);
				s = stream_new(length);
				stream_putc(s, length);
				stream_putc(s, FindOutLifByOSPF);
				stream_put_ipv4(s, oi->address->u.prefix4.s_addr);
				/* Send message.  */
				write (fd, STREAM_DATA(s), length);
				goto out;
			}
			else if ( (if_id && INTERFACE_GMPLS_ENABLED(oi) &&
					ntohs(oi->te_para.link_lcrmt_id.header.type)!=0 &&
					IN_SAME_SLASH30(oi->te_para.link_id.value, (*addr)) &&
					ntohl(oi->te_para.link_lcrmt_id.link_remote_id) == if_id)
					||
					((if_id >> 16) && INTERFACE_GMPLS_ENABLED(oi) &&
					( (ntohs(oi->te_para.link_id.header.type)!=0 && oi->te_para.link_id.value.s_addr == addr->s_addr) || (ntohs(oi->te_para.lclif_ipaddr.header.type) != 0 && IN_SAME_SLASH30(oi->te_para.lclif_ipaddr.value, (*addr))))))
				{
					length = sizeof(u_int8_t)*2 + sizeof(struct in_addr);
					s = stream_new(length);
					stream_putc(s, length);
					stream_putc(s, FindOutLifByOSPF);
					stream_put_ipv4(s, oi->address->u.prefix4.s_addr);
					/* Send message.  */
					write (fd, STREAM_DATA(s), length);
					goto out;
					
				}
	 	 }
	}
	/* Default return value is itself */  /* ??? */
	length = sizeof(u_int8_t)*2;
	s = stream_new(length);
	stream_putc(s, length);
	stream_putc(s, FindOutLifByOSPF);
	/* Send message.  */
	write (fd, STREAM_DATA(s), length);

out:
	stream_free(s);
	return;
}

/* Calculate an explicit route to the specified destination */
void
ospf_get_explicit_route(struct stream * sin, int fd)
{
	struct ospf_interface *oi;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	struct ospf_area *area;
	struct stream *s = NULL;
	u_int8_t length;
	u_int8_t service;
	struct in_addr src, dest;
	u_int8_t switching, encoding;
	u_int16_t gpid;
	float bandwidth;
	u_int8_t sonet_signal_type;  	/* Signal type */
	u_int8_t sonet_rcc;			/* Requested Contiguous Concatenation */
	u_int16_t sonet_ncc; 			/*  Number of Contiguous Components */
	u_int16_t sonet_nvc; 			/* Number of Virtual Components */
	u_int16_t sonet_mt;			/* Multiplier */
	u_int32_t sonet_t;				/* Transparency */
	u_int32_t sonet_p;			/*  Profile */
	list explicit_path = NULL;
	listnode node;
	struct route_node *rn;
	struct ospf_lsa *lsa;
	struct in_addr area_id;
	int find;
	
	service = stream_getc(sin);
	src.s_addr = stream_get_ipv4(sin);
	dest.s_addr = stream_get_ipv4(sin);
	encoding = stream_getc(sin);
	switching = stream_getc(sin);
	gpid = stream_getw(sin);
	if (service==2) /* GMPLS Generic T-Spec */
		bandwidth = (float)stream_getl(sin);
	else /* Sonet T-Spec */
	{
		bandwidth = 1000; /* temporary hack */
		sonet_signal_type = stream_getc(sin);
		sonet_rcc = stream_getc(sin);
		sonet_ncc = stream_getw(sin);
		sonet_nvc = stream_getw(sin);
		sonet_mt = stream_getw(sin);
		sonet_t = stream_getl(sin);
		sonet_p = stream_getl(sin);
	}

	/* find area id */
	area = NULL;
	if (om->ospf){
		if (src.s_addr==0 || ntohl(src.s_addr)==INADDR_LOOPBACK)
		{
			area_id.s_addr = OSPF_AREA_BACKBONE;
			area = ospf_area_lookup_by_area_id(getdata(listhead(om->ospf)), area_id);
		}
		else{
			LIST_LOOP(om->ospf, ospf, node1)
			{
				if (ospf->oiflist)
				LIST_LOOP(ospf->oiflist, oi, node2){
					if (ntohl(oi->address->u.prefix4.s_addr) == ntohl(src.s_addr))
					{
						area = oi->area;
						break;
					}
				 }
			}
		}
	}
	if (area)
	  {
	  	/* set src to its lookback address */
		src.s_addr = OspfTeRouterAddr.value.s_addr;

		find = 0;
		/* find dest's lookback address */
		LSDB_LOOP (area->te_rtid_db->db, rn, lsa)
		{
		  /* If dest is the *router ID * of the remote node */
		  if (lsa->tepara_ptr && lsa->tepara_ptr->p_router_addr && 
			   ntohs(lsa->tepara_ptr->p_router_addr->header.type)!=0 &&
			   ntohl(lsa->tepara_ptr->p_router_addr->value.s_addr) == ntohl(dest.s_addr))
		   {
		   	   find = 1;
			   break;
		   }
		}
		if (!find)
		{
			LSDB_LOOP (area->te_lsdb->db, rn, lsa)
			{
				if (lsa->tepara_ptr && lsa->tepara_ptr->p_lclif_ipaddr && 
				     ntohs(lsa->tepara_ptr->p_lclif_ipaddr->header.type)!=0 &&
				     ntohl(lsa->tepara_ptr->p_lclif_ipaddr->value.s_addr) == ntohl(dest.s_addr))
				{
					find  = 1;
					dest.s_addr = lsa->data->adv_router.s_addr;
					break;
				}
			}
		}
		/*cspf routing calculation on demand*/
		if (find)
			explicit_path=ospf_cspf_calculate (area, src, dest, switching);
	  }
	if (explicit_path){
		listnode_delete(explicit_path, listnode_head(explicit_path)); /* we don't need  the first hop which is itself */
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr)*listcount(explicit_path);
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetExplicitRouteByOSPF);
		for (node = explicit_path->head; node; nextnode (node)) 
		   {
			stream_put_ipv4(s, *(u_int32_t*) (node->data));
			 XFREE(MTYPE_TMP, node->data);
		   }
		/* Send message.  */
		write (fd, STREAM_DATA(s), length);
		list_delete(explicit_path);
		goto out;
	}
		

	/* Default return value is itself */  /* ??? */
	length = sizeof(u_int8_t)*2;
	s = stream_new(length);
	stream_putc(s, length);
	stream_putc(s, GetExplicitRouteByOSPF);
	/* Send message.  */
	write (fd, STREAM_DATA(s), length);

out:
	if (s)
		stream_free(s);
	return;
}

void
ospf_hold_vtag(u_int32_t port, u_int32_t vtag, u_int8_t hold_flag)
{
	struct ospf_interface *oi;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	int updated = 0;
	
	if (om->ospf)
	LIST_LOOP(om->ospf, ospf, node1)
	{
		if (ospf->oiflist)
		LIST_LOOP(ospf->oiflist, oi, node2){
			if (oi && INTERFACE_MPLS_ENABLED(oi) && oi->vlsr_if.switch_port == port) {
				if (hold_flag == 1 && HAS_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, vtag))
				{
					RESET_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, vtag);
					SET_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask_alloc, vtag);
					updated = 1;
				}
				else if (hold_flag == 0 && !HAS_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, vtag))
				{
					SET_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, vtag);
					RESET_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask_alloc, vtag);
					updated = 1;
				}
				if (updated && oi->t_te_area_lsa_link_self)
				{
					OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
					OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_MIN_LS_INTERVAL);
				}
			}
		}
	}
}

static int hold_bandwidth(struct ospf_interface *oi, float bandwidth, u_int32_t ucid, u_int32_t seqnum)
{
	u_int8_t i;
	static float zero_bw = 0;
	float unrsv_bw;

	for (i=0; i<8; i++)
	{
		ntohf(&oi->te_para.unrsv_bw.value[i], &unrsv_bw);
		unrsv_bw -= bandwidth;
		if (unrsv_bw < 0)
			unrsv_bw = zero_bw;
		set_linkparams_unrsv_bw(&oi->te_para.unrsv_bw, i, &unrsv_bw);
		htonf(&unrsv_bw, &oi->te_para.link_ifswcap.link_ifswcap_data.max_lsp_bw_at_priority[i]);
		if (ucid != 0 && seqnum != 0)
			insert_gri(oi, ucid, seqnum);
	}
	return 1;
}

static int release_bandwidth(struct ospf_interface *oi, float bandwidth, u_int32_t ucid, u_int32_t seqnum)
{
	u_int8_t i;
	float max_rsv_bw;
	float unrsv_bw;

	ntohf(&oi->te_para.max_rsv_bw.value, &max_rsv_bw);
	for (i=0; i<8; i++)
	{
		ntohf(&oi->te_para.unrsv_bw.value[i], &unrsv_bw);
		unrsv_bw += bandwidth;
		if (unrsv_bw > max_rsv_bw)
			unrsv_bw = max_rsv_bw;
		set_linkparams_unrsv_bw(&oi->te_para.unrsv_bw, i, &unrsv_bw);
		htonf(&unrsv_bw, &oi->te_para.link_ifswcap.link_ifswcap_data.max_lsp_bw_at_priority[i]);
		if (ucid != 0 && seqnum != 0)
			delete_gri(oi, ucid, seqnum);
	}

	return 1;
}

void
ospf_hold_bandwidth(u_int32_t port, float bw, u_int8_t hold_flag, u_int32_t ucid, u_int32_t seqnum)
{
	struct ospf_interface *oi;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	int updated = 0;

	if (bw == 0)
		return;

	bw = (bw *1000000) / 8;

	if (om->ospf)
	LIST_LOOP(om->ospf, ospf, node1)
	{
		if (ospf->oiflist)
		LIST_LOOP(ospf->oiflist, oi, node2){
			if (oi && INTERFACE_MPLS_ENABLED(oi) && (oi->vlsr_if.switch_port == port
				|| ( ( (port>>16) == 0x10 || (port>>16) == 0x11 ) 
					&& ( ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.version) & IFSWCAP_SPECIFIC_SUBNET_UNI) != 0
					&& ( oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.subnet_uni_id == (u_int8_t)(port>>8) ) ) ) ) {
				if (hold_flag == 1)
				{
					updated = hold_bandwidth(oi, bw, ucid, seqnum);
				}
				else 
				{
					updated = release_bandwidth(oi, bw, ucid, seqnum);
				}
				if (updated && oi->t_te_area_lsa_link_self)
				{
					OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
					OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_MIN_LS_INTERVAL);
				}
			}
		}
	}
}

void
ospf_hold_timeslots(u_int32_t port, list ts_list, u_int8_t hold_flag)
{
	struct ospf_interface *oi;
	struct listnode *node1, *node2, *node3;
	struct ospf *ospf;
	int updated = 0;
	u_int8_t* ts;

	if (om->ospf)
	LIST_LOOP(om->ospf, ospf, node1)
	{
		if (ospf->oiflist)
		LIST_LOOP(ospf->oiflist, oi, node2){
			if ( ( (port>>16) == 0x10 || (port>>16) == 0x11 ) 
					&& ( ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.version) & IFSWCAP_SPECIFIC_SUBNET_UNI) != 0
					&& ( oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.subnet_uni_id == (u_int8_t)(port>>8) ) ) {
				if (hold_flag == 1)
				{
					LIST_LOOP(ts_list, ts, node3)
						RESET_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.timeslot_bitmask, *ts);						
					updated = 1;
				}
				else 
				{
					LIST_LOOP(ts_list, ts, node3)
						SET_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.timeslot_bitmask, *ts);						
					updated = 1;
				}
				if (updated && oi->t_te_area_lsa_link_self)
				{
					OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
					OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_MIN_LS_INTERVAL);
				}
			}
		}
	}

}

void
ospf_get_vlsr_route(struct in_addr * inRtId, struct in_addr * outRtId, u_int32_t inPort, u_int32_t outPort, int fd)
{
	struct ospf_interface *oi, *in_oi = NULL, *out_oi = NULL;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	struct stream *s;
	u_int8_t length = 0;
	u_int32_t vlan = 0;

	if (ntohl(inRtId->s_addr)==ntohl(outRtId->s_addr) && inPort!=outPort){
		/* un-numbered interface */
		if (OspfTeRouterAddr.value.s_addr == inRtId->s_addr && om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_GMPLS_ENABLED(oi) &&
				    ntohs(oi->te_para.link_lcrmt_id.header.type)!=0 &&
				    ntohl(oi->te_para.link_lcrmt_id.link_local_id) == inPort)
					in_oi = oi;
				if (INTERFACE_GMPLS_ENABLED(oi) &&
				    ntohs(oi->te_para.link_lcrmt_id.header.type)!=0 &&
				    ntohl(oi->te_para.link_lcrmt_id.link_local_id) == outPort)
					out_oi = oi;
		 	 }
		}
	}
        else if (inRtId->s_addr != outRtId->s_addr && inRtId->s_addr && outRtId->s_addr != 0 
          && inPort == outPort && (inPort >> 16) == 0x4) /* 0x4: LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL*/ 
        {
		vlan = inPort & 0x0000ffff;
		if ( om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_GMPLS_ENABLED(oi) && ntohl(oi->te_para.lclif_ipaddr.value.s_addr) == ntohl(inRtId->s_addr)
                                && (ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_BASIC) 
        			    && HAS_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, (u_int16_t)vlan)) {
        			    	/*TODO @@@@ if !HAS_VLAN --> should return empty vlsr_route */
				        inPort &= 0xffff0000;
                                    inPort |= oi->vlsr_if.switch_port;
					 in_oi = oi;
                               }
				if (INTERFACE_GMPLS_ENABLED(oi) && ntohl(oi->te_para.lclif_ipaddr.value.s_addr) == ntohl(outRtId->s_addr)
                                && (ntohs(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_BASIC) 
      			           && HAS_VLAN(oi->te_para.link_ifswcap.link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, (u_int16_t)vlan)) {
        			    	/*TODO @@@@ if !HAS_VLAN --> should return empty vlsr_route */
				        outPort &= 0xffff0000;
                                    outPort |= oi->vlsr_if.switch_port;
					 out_oi = oi;
                               }
		 	 }
		}
        }
       else if (inRtId->s_addr==0 && inPort != 0 && outRtId->s_addr != 0 ){
              /* local-id configured at ingress */
		if ( om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_GMPLS_ENABLED(oi) &&
					ntohs(oi->te_para.lclif_ipaddr.header.type)!=0 &&
					ntohl(oi->te_para.lclif_ipaddr.value.s_addr) == ntohl(outRtId->s_addr))
					out_oi = oi;
		 	 }
		}       
       }
       else if (inRtId->s_addr!=0 && outRtId->s_addr == 0 && outPort != 0){
              /* local-id configured at egress */
		if ( om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_GMPLS_ENABLED(oi) &&
					ntohs(oi->te_para.lclif_ipaddr.header.type)!=0 &&
					ntohl(oi->te_para.lclif_ipaddr.value.s_addr) == ntohl(inRtId->s_addr))
					in_oi = oi;
		 	 }
		}              
       }
	else if (ntohl(inRtId->s_addr)!=ntohl(outRtId->s_addr) && inPort==0 && outPort==0){
		/* numbered interface */
		if (om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_GMPLS_ENABLED(oi) &&
					ntohs(oi->te_para.lclif_ipaddr.header.type)!=0 &&
					ntohl(oi->te_para.lclif_ipaddr.value.s_addr) == ntohl(inRtId->s_addr))
					in_oi = oi;
				if (INTERFACE_GMPLS_ENABLED(oi) &&
					ntohs(oi->te_para.lclif_ipaddr.header.type)!=0 &&
					ntohl(oi->te_para.lclif_ipaddr.value.s_addr) == ntohl(outRtId->s_addr))
					out_oi = oi;
			 }
		}
	}

       if ( (inPort >> 16) == 0x4 && (outPort >> 16) == 0x4
          //&& ((inPort & 0xffff) != 0) && ((outPort & 0xffff) != 0) 
          && (inPort != outPort ) && in_oi && out_oi && in_oi->vlsr_if.switch_ip.s_addr!=0)
        {
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t)*3;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetVLSRRoutebyOSPF);
		stream_put_ipv4(s, in_oi->vlsr_if.switch_ip.s_addr);
		stream_putl(s, inPort);
		stream_putl(s, outPort);
		stream_putl(s, vlan);
		/* Send message.  */
		write (fd, STREAM_DATA(s), length);
        }
	else if (in_oi && out_oi && in_oi->vlsr_if.switch_ip.s_addr!=0 && out_oi->vlsr_if.switch_ip.s_addr!=0 &&
		in_oi->vlsr_if.switch_ip.s_addr == out_oi->vlsr_if.switch_ip.s_addr &&
		//in_oi->vlsr_if.switch_port != 0 && out_oi->vlsr_if.switch_port != 0 &&
		in_oi->vlsr_if.switch_port != out_oi->vlsr_if.switch_port)
	{
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t)*3;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetVLSRRoutebyOSPF);
		stream_put_ipv4(s, in_oi->vlsr_if.switch_ip.s_addr);
		stream_putl(s, in_oi->vlsr_if.switch_port);
		stream_putl(s, out_oi->vlsr_if.switch_port);
		stream_putl(s, 0);
		/* Send message.  */
		write (fd, STREAM_DATA(s), length);
	}
	else if (outPort != 0 && in_oi && in_oi->vlsr_if.switch_ip.s_addr!=0 
		//&& in_oi->vlsr_if.switch_port != 0
		)
       {
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t)*3;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetVLSRRoutebyOSPF);
		stream_put_ipv4(s, in_oi->vlsr_if.switch_ip.s_addr);
		if ((inPort >> 16) == 0x4)
                {
			if (vlan == 0) vlan = inPort&0xffff;
                	inPort &= 0xffff0000;
                	inPort |= in_oi->vlsr_if.switch_port;
		}
        	else
            		inPort = in_oi->vlsr_if.switch_port;       
		stream_putl(s, inPort);
		stream_putl(s, outPort);
		stream_putl(s, vlan);
		/* Send message.  */
		write (fd, STREAM_DATA(s), length);             
       }
	else if (inPort != 0 && out_oi && out_oi->vlsr_if.switch_ip.s_addr!=0 
		//&& out_oi->vlsr_if.switch_port != 0
		)
       {
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t)*3;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetVLSRRoutebyOSPF);
		stream_put_ipv4(s, out_oi->vlsr_if.switch_ip.s_addr);
		stream_putl(s, inPort);
		if ((outPort >> 16) == 0x4)
                {
                	if (vlan == 0)	vlan = outPort&0xffff;
                	outPort &= 0xffff0000;
                	outPort |= out_oi->vlsr_if.switch_port;
		}
        	else
            		outPort = out_oi->vlsr_if.switch_port;       
		stream_putl(s, outPort);
		stream_putl(s, vlan);

		/* Send message.  */
		write (fd, STREAM_DATA(s), length);             
       }
       else if ( (inPort >> 16) == 0x10 || (outPort >> 16) == 0x11)
        {
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t)*3;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetVLSRRoutebyOSPF);
		stream_put_ipv4(s, 0);
		stream_putl(s, inPort);
		stream_putl(s, outPort);
		stream_putl(s, 0);
		/* Send message.  */
		write (fd, STREAM_DATA(s), length);
        }
	else if (inRtId->s_addr == outRtId->s_addr && OspfTeRouterAddr.value.s_addr == inRtId->s_addr
		&& (inPort >> 16) != 0x0 && (inPort>>16) != 0x4 && (outPort >> 16) != 0x0 && (outPort>>16) != 0x4) {
		/* srouce-destination colocated local-id provisioning */
		u_int32_t switch_ip = 0;
		/* looking for any switch_address availalble as long as this is not for subnet-interface provsioning */
		if ((inPort >> 16) != 0x10 && (outPort >> 16) != 0x11 && om->ospf)
		LIST_LOOP(om->ospf, ospf, node1)
		{
			if (ospf->oiflist)
			LIST_LOOP(ospf->oiflist, oi, node2){
				if (INTERFACE_GMPLS_ENABLED(oi) &&	ntohs(oi->vlsr_if.switch_port)!=0)
				{
					switch_ip = oi->vlsr_if.switch_ip.s_addr;
					break;
				}
		 	 }
		}
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t)*3;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetVLSRRoutebyOSPF);
		stream_put_ipv4(s, switch_ip);
		stream_putl(s, inPort);
		stream_putl(s, outPort);
		stream_putl(s, 0);
		/* Send message.  */
		write (fd, STREAM_DATA(s), length);		
	}
       else
	{
		length = sizeof(u_int8_t)*2 + sizeof(struct in_addr) + sizeof(u_int32_t)*3;
		s = stream_new(length);
		stream_putc(s, length);
		stream_putc(s, GetVLSRRoutebyOSPF);
		stream_put_ipv4(s, 0);
		stream_putl(s, 0);
		stream_putl(s, 0);
		stream_putl(s, 0);
		/* Send message.  */
		write (fd, STREAM_DATA(s), length);
	}
	stream_free(s);
	return;
}

void
ospf_rsvp_notify(u_int8_t msgtype, struct in_addr * ctrlIP, float *bandwidth, int fd)
{
	struct ospf_interface *oi = NULL, *in_oi;
	struct listnode *node1, *node2;
	struct ospf *ospf;
	u_int8_t i;
	static float zero_bw = 0;
	float max_rsv_bw;
	float unrsv_bw;
	
	if (om->ospf)
	LIST_LOOP(om->ospf, ospf, node1)
	{
		if (ospf->oiflist)
		LIST_LOOP(ospf->oiflist, in_oi, node2){
			if (INTERFACE_MPLS_ENABLED(in_oi) &&
			    ntohl(in_oi->address->u.prefix4.s_addr) == ntohl(ctrlIP->s_addr))
				oi = in_oi;
	 	 }
	}
	if (oi)
	{
		switch (msgtype)
		{
			case OspfResv:
				for (i=0; i<8; i++)
				{
					ntohf(&oi->te_para.unrsv_bw.value[i], &unrsv_bw);
					unrsv_bw -= ((*bandwidth) * 1000000 / 8);
					if (unrsv_bw < 0)
						unrsv_bw = zero_bw;
					set_linkparams_unrsv_bw(&oi->te_para.unrsv_bw, i, &unrsv_bw);
					htonf(&unrsv_bw, &oi->te_para.link_ifswcap.link_ifswcap_data.max_lsp_bw_at_priority[i]);
				}
				if (oi->t_te_area_lsa_link_self)
				{
					OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
					OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_MIN_LS_INTERVAL);
				}
				break;
			case OspfPathTear:
			case OspfResvTear:
				ntohf(&oi->te_para.max_rsv_bw.value, &max_rsv_bw);
				for (i=0; i<8; i++)
				{
					ntohf(&oi->te_para.unrsv_bw.value[i], &unrsv_bw);
					unrsv_bw += ((*bandwidth) * 1000000 / 8);
					if (unrsv_bw > max_rsv_bw)
						unrsv_bw = max_rsv_bw;
					set_linkparams_unrsv_bw(&oi->te_para.unrsv_bw, i, &unrsv_bw);
					htonf(&unrsv_bw, &oi->te_para.link_ifswcap.link_ifswcap_data.max_lsp_bw_at_priority[i]);
				}
				if (oi->t_te_area_lsa_link_self)
				{
					OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
					OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_MIN_LS_INTERVAL);
				}
				break;
			default:
				break;
		}
	}
	return;
}

/* return the loopback address*/
void
ospf_rsvp_get_loopback_addr(int fd)
{
	struct stream *s = NULL;
	u_int8_t length;
	
	length = sizeof(u_int8_t)*2 + sizeof(struct in_addr);
	s = stream_new(length);
	stream_putc(s, length);
	stream_putc(s, GetLoopbackAddress);
	stream_put_ipv4(s, OspfTeRouterAddr.value.s_addr);
	/* Send message.  */
	write (fd, STREAM_DATA(s), length);

	stream_free(s);
	return;
}


void
ospf_rsvp_get_subnet_uni_data(struct in_addr* data_if, u_int8_t uni_id, int fd)
{
	struct ospf_area *area;
	listnode node;
	struct route_node *rn;
	struct ospf_lsa *lsa;
	struct in_addr area_id;
	struct te_link_subtlv_link_ifswcap* ifswcap;
	struct link_ifswcap_specific_subnet_uni* uni_data = NULL;
	struct stream *s = NULL;
	u_int8_t length;
	int i;
		
	area = NULL;
	if (om->ospf){
		area_id.s_addr = OSPF_AREA_BACKBONE;
		area = ospf_area_lookup_by_area_id(getdata(listhead(om->ospf)), area_id);
	}
	if (area)
	  {
		LSDB_LOOP (area->te_lsdb->db, rn, lsa)
		{ //matching the data_if with either a te link local if addr or a link's originating end's loopback
		  if (lsa->tepara_ptr && lsa->tepara_ptr->p_lclif_ipaddr && 
		  	(lsa->tepara_ptr->p_lclif_ipaddr->value.s_addr == data_if->s_addr ||lsa->data->adv_router.s_addr == data_if->s_addr))
		   {
		   	LIST_LOOP(lsa->tepara_ptr->p_link_ifswcap_list, ifswcap, node)
	   		{
	   			if (ifswcap && ifswcap->link_ifswcap_data.switching_cap == LINK_IFSWCAP_SUBTLV_SWCAP_L2SC
				    && ntohs(ifswcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.version) == IFSWCAP_SPECIFIC_SUBNET_UNI 
				    && ifswcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni.subnet_uni_id == uni_id)
   				{
					uni_data = &ifswcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni;
					break;
   				}
			
	   		}
			if (uni_data != NULL)
				break;
		   }
		}
	}

	length = sizeof(u_int8_t)*2 + (uni_data == NULL ? 0 : sizeof(u_int32_t)*6+12+16+MAX_TIMESLOTS_NUM/8);
	s = stream_new(length);
	stream_putc(s, length);
	stream_putc(s, GetSubnetUNIDataByOSPF);
	if (uni_data)
	{
		stream_putl(s, uni_data->tna_ipv4);
		stream_putl(s, uni_data->nid_ipv4);
		stream_putl(s, uni_data->data_ipv4);
		stream_putl(s, uni_data->logical_port_number);
		stream_putl(s, uni_data->egress_label_downstream); /*to be removed*/
		stream_putl(s, uni_data->egress_label_upstream); /*to be removed*/
		for (i = 0; i < 12; i++)
			stream_putc(s, uni_data->control_channel[i]);
		for (i = 0; i < 16; i++)
			stream_putc(s, uni_data->node_name[i]);
		for (i = 0; i < MAX_TIMESLOTS_NUM/8; i++)
			stream_putc(s, uni_data->timeslot_bitmask[i]);
	}
	write (fd, STREAM_DATA(s), length);
	stream_free(s);
	return;
}

/* Handler of RSVP request. */
int
ospf_rsvp_read (struct thread *thread)
{
  int sock;
  int nbyte;
  u_short length;
  u_char command;
  struct stream *s;
  struct in_addr addr, addr1;
  u_int32_t if_id;
  u_int32_t vlsr_in_if_id, vlsr_out_if_id;
  u_int32_t port, bw_uint32, ucid, seqnum;
  u_int32_t vtag;
  u_int8_t hold_flag;
  u_int8_t uni_id;
  u_int8_t* ts;
  list ts_list;
  int i;
  float bandwidth, tmpbw;
  
  /* Get thread data.  Reset reading thread because I'm running. */
  sock = THREAD_FD (thread);

  s = stream_new (ZEBRA_MAX_PACKET_SIZ);
  
  /* Read length and command. */
  nbyte = stream_read (s, sock, 2);
  if (nbyte <= 0) 
  {
	zlog_info ("ospf-rsvp connection closed socket [%d]", sock);
	stream_free(s);
      return -1;
  }
  length = stream_getc (s);
  command = stream_getc (s);

 length -= 2;

  /* Read rest of data. */
  if (length)
    {
      nbyte = stream_read (s, sock, length);
      if (nbyte <= 0) 
	{
	  zlog_info ("connection closed [%d] when reading rsvp data", sock);
	  stream_free(s);
	  return -1;
	}
    }

  switch (command) 
    {
    case FindInterfaceByData:
	addr.s_addr = stream_get_ipv4(s);
	if_id = ntohl(stream_getl(s));
	ospf_find_interface_by_data(&addr, if_id, sock);
      break;

   case FindDataByInterface:
	addr.s_addr = stream_get_ipv4(s);
	ospf_find_data_by_interface(&addr, sock);
     break;
		
    case FindOutLifByOSPF:
	addr.s_addr = stream_get_ipv4(s);
	if_id = ntohl(stream_getl(s));
	ospf_find_out_lif(&addr, if_id, sock);
      break;

    case GetExplicitRouteByOSPF:
	ospf_get_explicit_route(s, sock);
     break;
		
    case GetVLSRRoutebyOSPF:
	addr.s_addr = stream_get_ipv4(s);
	addr1.s_addr = stream_get_ipv4(s);
	vlsr_in_if_id = stream_getl(s);
	vlsr_out_if_id = stream_getl(s);
	ospf_get_vlsr_route(&addr, &addr1, vlsr_in_if_id, vlsr_out_if_id, sock);
     break;

    case HoldBandwidthbyOSPF:
	port = stream_getl(s);	
	bw_uint32 = stream_getl(s);	
	hold_flag = stream_getc(s);
	ucid = stream_getl(s);
	seqnum = stream_getl(s);
	ospf_hold_bandwidth(port, *(float*)&bw_uint32, hold_flag, ucid, seqnum);
     break;

    case HoldVtagbyOSPF:
	port = stream_getl(s);	
	vtag = stream_getl(s);	
	hold_flag = stream_getc(s);
	ospf_hold_vtag(port, vtag, hold_flag);
     break;

    case HoldTimeslotsbyOSPF:
	port = stream_getl(s);	
	hold_flag = stream_getc(s);
	length -= 5;
	assert (length > 0);
	ts_list = list_new();
	for (i = 0; i < length; i++)
	{
		ts = (u_int8_t*)malloc(1);
		*ts = stream_getc(s);
		listnode_add(ts_list, ts);
	}
	ospf_hold_timeslots(port, ts_list, hold_flag);
	list_delete(ts_list);
     break;

    case OspfPathTear:
    case OspfResv:
    case OspfResvTear:
	addr.s_addr = stream_get_ipv4(s);
	stream_get (&tmpbw, s, sizeof(float));
	ntohf (&tmpbw, &bandwidth);
	ospf_rsvp_notify(command, &addr, &bandwidth, sock);
	break;

   case GetLoopbackAddress:
   	ospf_rsvp_get_loopback_addr(sock);
	break;

   case GetSubnetUNIDataByOSPF:
	addr.s_addr = stream_get_ipv4(s);
	uni_id = stream_getc (s);
	ospf_rsvp_get_subnet_uni_data(&addr, uni_id, sock);
	break;

    default:
      zlog_info ("Zebra received unknown command %d", command);
      write (sock, STREAM_DATA(s), length);
      break;
    }

  stream_free (s);
  
  /* Create new zebra client. */
  thread_add_read (master, ospf_rsvp_read, NULL, sock);

  return 0;
}


int
ospf_rsvp_accept (struct thread *thread)
{
  int val;
  int accept_sock;
  int client_sock;
  struct sockaddr_in client;
  socklen_t len;

  accept_sock = THREAD_FD (thread);

  len = sizeof (struct sockaddr_in);
  client_sock = accept (accept_sock, (struct sockaddr *) &client, &len);

  if (client_sock < 0)
    {
      zlog_warn ("Can't accept rsvp socket: %s", strerror (errno));
      return -1;
    }

  /* Make client socket non-blocking.  */

  val = fcntl (client_sock, F_GETFL, 0);
  fcntl (client_sock, F_SETFL, (val | O_NONBLOCK));

  /* Create new zebra client. */
  thread_add_read (master, ospf_rsvp_read, NULL, client_sock);

  /* Register myself */
  thread_add_read (master, ospf_rsvp_accept, NULL, accept_sock);

  return 0;
}

extern int OSPF_API_SYNC_PORT;

void
ospf_rsvp_init ()
{
  int ret;
  int accept_sock;
  struct sockaddr_in addr;
  static u_int16_t ospf_rsvp_port = 2613;
  if (OSPF_API_SYNC_PORT == 2607)
      ospf_rsvp_port = 2713;

  accept_sock = socket (AF_INET, SOCK_STREAM, 0);

  if (accept_sock < 0) 
    {
      zlog_warn ("ospf_rsvp_init(): can't bind to socket: %s", strerror (errno));
      return;
    }

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (ospf_rsvp_port);
#ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  sockopt_reuseaddr (accept_sock);
  sockopt_reuseport (accept_sock);

  ret  = bind (accept_sock, (struct sockaddr *)&addr, 
	       sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      zlog_warn ("ospf_rsvp_init(): can't bind to socket: %s", strerror (errno));
      close (accept_sock);      /* Avoid sd leak. */
      return;
    }

  ret = listen (accept_sock, 1);
  if (ret < 0)
    {
      zlog_warn ("ospf_rsvp_init(): can't listen to socket: %s", strerror (errno));
      close (accept_sock);	/* Avoid sd leak. */
      return;
    }

  thread_add_read (master, ospf_rsvp_accept, NULL, accept_sock);

}

#endif
