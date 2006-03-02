/*
 * OSPF LSDB support.
 * Copyright (C) 1999, 2000 Alex Zinin, Kunihiro Ishiguro, Toshiaki Takada
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

#include "prefix.h"
#include "table.h"
#include "memory.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_te_lsdb.h"

#ifdef HAVE_OPAQUE_LSA

struct ospf_te_lsdb *
ospf_te_lsdb_new ()
{
  struct ospf_te_lsdb *new;

  new = XCALLOC (MTYPE_OSPF_TE_LSDB, sizeof (struct ospf_te_lsdb));
  new->db = route_table_init ();
  return new;
}

void
ospf_te_lsdb_free (struct ospf_te_lsdb *lsdb)
{
  ospf_te_lsdb_cleanup(lsdb); 
  XFREE (MTYPE_OSPF_TE_LSDB, lsdb);
  return;
}

void
ospf_te_lsdb_cleanup (struct ospf_te_lsdb *lsdb)
{
  assert (lsdb);

  ospf_te_lsdb_delete_all (lsdb);
  
  route_table_finish (lsdb->db);
}

/* Each te_lsdb entry is uniquely identified by adv_router,
     and each te_lsdb entry contains a list which points to a set of LSAs originated by the same "adv_router"
    */
void
ospf_te_lsdb_prefix_set (struct prefix *lp, struct ospf_lsa *lsa)
{
	if (lsa->te_lsa_type == ROUTER_ID_TE_LSA)
	{
		  memset (lp, 0, sizeof (struct prefix_ipv4));
		  lp->family = 0;
		  lp->prefixlen = 32;
		  lp->u.prefix4 = lsa->data->adv_router;
	}
	else  /* LINK_TE_LSA */
	{
		  memset (lp, 0, sizeof (struct prefix_ls));
		  lp->family = 0;
		  lp->prefixlen = 64;
		  lp->u.lp.id = lsa->data->id;
		  lp->u.lp.adv_router = lsa->data->adv_router;
	}
}

/* Add new TE-LSA to lsdb. */
void
ospf_te_lsdb_add (struct ospf_te_lsdb *lsdb, struct ospf_lsa *lsa)
{
  struct route_table *table;
  struct prefix lp;
  struct route_node *rn;
  
  table = lsdb->db;
  ospf_te_lsdb_prefix_set (&lp, lsa);
  rn = route_node_get (table, (struct prefix *)&lp);
  if (rn->info){
      if (rn->info == lsa)
	return;
      
      ospf_lsa_unlock (rn->info);
      route_unlock_node (rn);
  }
  else
  	 lsdb->total++;
  if (lsdb->new_lsa_hook != NULL)
    (* lsdb->new_lsa_hook)(lsa);

  rn->info = ospf_lsa_lock (lsa);

}

void
ospf_te_lsdb_delete (struct ospf_te_lsdb *lsdb, struct ospf_lsa *lsa)
{
  struct route_table *table;
  struct prefix_ipv4 lp;
  struct route_node *rn;

  table = lsdb->db;
  ospf_te_lsdb_prefix_set ((struct prefix *)&lp, lsa);
  rn = route_node_lookup (table, (struct prefix *) &lp);
  if (rn)
    if (rn->info == lsa)
      {
	lsdb->total--;
	rn->info = NULL;
	route_unlock_node (rn);
	route_unlock_node (rn);
       if (lsdb->del_lsa_hook != NULL)
          (* lsdb->del_lsa_hook)(lsa);
	ospf_lsa_unlock (lsa);
	return;
      }
}

void
ospf_te_lsdb_delete_all (struct ospf_te_lsdb *lsdb)
{
  struct route_table *table;
  struct route_node *rn;
  struct ospf_lsa *lsa;

	table = lsdb->db;
	for (rn = route_top (table); rn; rn = route_next (rn))
	if ((lsa = (rn->info)) != NULL)
	  {
	    rn->info = NULL;
	    route_unlock_node (rn);
           if (lsdb->del_lsa_hook != NULL)
              (* lsdb->del_lsa_hook)(lsa);
	    ospf_lsa_unlock (lsa);
	  }
}

struct ospf_lsa *
ospf_te_lsdb_lookup (struct ospf_te_lsdb *lsdb, struct ospf_lsa *lsa)
{
  struct route_table *table;
  struct prefix lp;
  struct route_node *rn;
  struct ospf_lsa *find;

  if (lsa->te_lsa_type == NOT_TE_LSA)
  	return NULL;
  
  table = lsdb->db;
  ospf_te_lsdb_prefix_set (&lp, lsa);
  rn = route_node_lookup (table, &lp);
  if (rn)
    {
      find = rn->info;
      route_unlock_node (rn);
      return find;
    }
  return NULL;
}

list
ospf_te_lsdb_lookup_by_adv_router (struct ospf_te_lsdb *lsdb, struct in_addr adv_router)
{
  struct route_node *rn;
  struct ospf_lsa *lsa;
  list lsa_list = NULL;
  
  LSDB_LOOP (lsdb->db, rn, lsa)
  {
  	if (ntohl(lsa->data->adv_router.s_addr) == ntohl(adv_router.s_addr))
  	{
  		if (!lsa_list) 
  			lsa_list = list_new();
  		listnode_add(lsa_list, lsa);
  	}
  }
  return lsa_list;

}

unsigned long
ospf_te_lsdb_count (struct ospf_te_lsdb *lsdb)
{
  return lsdb->total;
}

unsigned long
ospf_te_lsdb_isempty (struct ospf_te_lsdb *lsdb)
{
  return (lsdb->total == 0);
}

#endif
