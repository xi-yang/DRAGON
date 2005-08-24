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

#ifndef _ZEBRA_OSPF_TE_LSDB_H
#define _ZEBRA_OSPF_TE_LSDB_H


/* TE-LSDB */
/* In this implementation, 
  * There is only one type-10 TE-area LSA for each MPLS/GMPLS enabled ospf interface 
  * There is only one type-9 TE-link-local LSA for each MPLS/GMPLS enabled ospf interface 
*/
struct ospf_te_lsdb{
	struct route_table *db; /* db->routing_node->info = ospf_te_lsa (area_lsa ONLY!!!) */
	unsigned long total;	/* Count of TE LSAs */
	/* Hooks for callback functions to catch every add/del event. */
	int (* new_lsa_hook)(struct ospf_lsa *);
	int (* del_lsa_hook)(struct ospf_lsa *);
};


/* OSPF TE-LSDB related functions. */
extern struct ospf_te_lsdb *ospf_te_lsdb_new ();
extern void ospf_te_lsdb_free (struct ospf_te_lsdb *lsdb);
extern void ospf_te_lsdb_cleanup (struct ospf_te_lsdb *lsdb);
extern void ospf_te_lsdb_add (struct ospf_te_lsdb *lsdb, struct ospf_lsa *lsa);
extern void ospf_te_lsdb_delete (struct ospf_te_lsdb *lsdb, struct ospf_lsa *lsa);
extern void ospf_te_lsdb_delete_all (struct ospf_te_lsdb *lsdb);
extern struct ospf_lsa *ospf_te_lsdb_lookup (struct ospf_te_lsdb *lsdb, struct ospf_lsa *lsa);
extern list ospf_te_lsdb_lookup_by_adv_router (struct ospf_te_lsdb *lsdb, struct in_addr adv_router);
extern unsigned long ospf_te_lsdb_count (struct ospf_te_lsdb *lsdb);
extern unsigned long ospf_te_lsdb_isempty (struct ospf_te_lsdb *lsdb);

#endif /* _ZEBRA_OSPF_TE_LSDB_H */
