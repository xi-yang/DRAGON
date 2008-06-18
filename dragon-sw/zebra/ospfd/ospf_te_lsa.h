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

#ifndef _ZEBRA_OSPF_TE_LSA_H
#define _ZEBRA_OSPF_TE_LSA_H

#ifdef HAVE_OPAQUE_LSA
/* A group of pointers to different TE-LSA tlvs and sub-tlvs */
struct te_lsa_para_ptr
{
  struct te_tlv_router_addr  *p_router_addr;
  struct te_tlv_link *p_link;
  struct te_tlv_link_local_id *p_link_local_id;

  struct te_link_subtlv_link_type    *p_link_type;
  struct te_link_subtlv_link_id      *p_link_id;
  struct te_link_subtlv_lclif_ipaddr *p_lclif_ipaddr;   
  struct te_link_subtlv_rmtif_ipaddr *p_rmtif_ipaddr;  
  struct te_link_subtlv_te_metric *p_te_metric;
  struct te_link_subtlv_max_bw *p_max_bw;
  struct te_link_subtlv_max_rsv_bw *p_max_rsv_bw;
  struct te_link_subtlv_unrsv_bw *p_unrsv_bw;
  struct te_link_subtlv_rsc_clsclr *p_rsc_clsclr;
  struct te_link_subtlv_link_lcrmt_id *p_link_lcrmt_id;
  struct te_link_subtlv_link_protype  *p_link_protype;
  list   p_link_ifswcap_list;
  struct te_tlv_header	*p_link_srlg;   
  struct te_link_subtlv_link_te_lambda  *p_link_te_lambda;
};


/*Type-9 and type-10 TE-LSA */
/* ospf_te_lsa is the same as ospf_lsa */


/* OSPF TE-LSA related functions */
extern int ospf_te_area_lsa_rtid_timer(struct thread *t);
extern int ospf_te_area_lsa_link_timer(struct thread *t);
extern int ospf_te_linklocal_lsa_timer(struct thread *t);
extern int ospf_te_area_lsa_link_originate (struct ospf_interface *oi);
extern int ospf_te_area_lsa_rtid_originate (struct ospf_area *area);
extern int ospf_te_area_lsa_link_refresh (struct ospf_lsa *lsa);
extern int ospf_te_area_lsa_rtid_refresh (struct ospf_lsa *lsa);
extern int ospf_te_linklocal_lsa_refresh (struct ospf_lsa *lsa);
extern int ospf_te_linklocal_lsa_originate (struct ospf_interface *oi);
extern struct ospf_lsa *ospf_te_lsa_install (struct ospf_lsa *new, struct ospf_interface *oi);
extern struct ospf_lsa *ospf_te_lsa_parse (struct ospf_lsa *new);
extern list ospf_cspf_calculate (struct ospf_area *area, struct in_addr source_ip, struct in_addr dest_ip, 
                    u_int8_t SwitchingCapability);
extern void ospf_te_cspf_calculate_schedule (struct ospf_area *area);

#endif
#endif /* _ZEBRA_OSPF_TE_LSA_H */
