/* OSPF SPF calculation.
   Copyright (C) 1999, 2000 Kunihiro Ishiguro, Toshiaki Takada

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */



#include <zebra.h>

#include "thread.h"
#include "memory.h"
#include "hash.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "log.h"
#include "sockunion.h"          /* for inet_ntop () */

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
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_ia.h"
#include "ospfd/ospf_ase.h"
#include "ospfd/ospf_abr.h"
#include "ospfd/ospf_dump.h"

#ifdef HAVE_OPAQUE_LSA

#define DEBUG

/* My CSPF part */


/*This function is to test if the router_id exists in the list of routers*/
int
lookup_router_by_router_id (list router_list, struct in_addr router_id)
{
   listnode node;
   struct in_addr *existing_router_id;
  for (node = router_list->head; node; nextnode (node)) {
  	existing_router_id =  (struct in_addr*) (node->data);
  	if (ntohl(router_id.s_addr) == ntohl(existing_router_id->s_addr))
	      return 1;
  }
  return 0;

}

/* This function is to find all the routers in this area by looking through LSDB.
     Those routers are put into a list. */
list
ospf_build_routers_from_te_lsdb(struct ospf_te_lsdb *lsdb)
{
  struct route_node *rn;
  struct ospf_lsa *lsa;
  list router_list = NULL;
  struct in_addr *router_id;
  
  LSDB_LOOP (lsdb->db, rn, lsa)
  {
  	if (!router_list) 
  		router_list = list_new();
/*	if (!lookup_router_by_router_id(router_list, lsa->tepara_ptr->p_link_id->value)) {*/
	if (!lookup_router_by_router_id(router_list, lsa->data->adv_router)) {
		router_id=(struct in_addr *) malloc(sizeof (struct in_addr ));
		router_id->s_addr=lsa->data->adv_router.s_addr;
	  	listnode_add(router_list, router_id);
		}
  }
  return router_list;
}

/* This function is to build up the graph. Vertexes are routers and edges are TE-links */
struct in_addr *
build_CSPF_vertex(list router_list) {

  u_int8_t graphsize=router_list->count;
  struct in_addr *CSPFvertex=(struct in_addr*) malloc(graphsize*sizeof(struct in_addr));
  struct in_addr *nodedata;
  listnode node;
  u_int8_t i=0;

  for (node = router_list->head; node; nextnode (node)) {
  	nodedata=(struct in_addr*) node->data;
  	CSPFvertex[i].s_addr=nodedata->s_addr;
	i++;
  }
  if (router_list) list_free(router_list);
  return CSPFvertex;
}

/* This function is to build up the graph. Vertexes are routers and edges are TE-links */
struct in_addr *
init_CSPF_TE_Link_list(list router_list) {

  u_int8_t graphsize=router_list->count;
  u_int16_t linksize=graphsize*graphsize;
  struct in_addr *CSPFlinks=(struct in_addr*) malloc(linksize*sizeof(struct in_addr));
  return CSPFlinks;
}


/* This function is to get index of an in_addr*/
int
get_vertex_index(u_int8_t graphsize, struct in_addr *CSPFvertex, struct in_addr router_ip) {

  int i=0;
  for (i = 0; i<graphsize; i++) {
  	if (ntohl(CSPFvertex[i].s_addr) == ntohl(router_ip.s_addr))
		return i;
  }
  return -1;
}


/*This function is to test if the te-link has the required switching capability*/
int 
ospf_te_lsa_swcap_lookup (list swcap_list, u_int8_t swcap)
{
  listnode node=NULL;
  struct te_link_subtlv_link_ifswcap *nodedata;

  node=listhead(swcap_list);
  while (node) {
 	nodedata=node->data;
	if (nodedata->link_ifswcap_data.switching_cap ==swcap) return 1;
	nextnode(node);
 	}
return 0;
}

/* This function is to build up the graph. Vertexes are routers and edges are TE-links */
void
build_CSPF_graph(struct ospf_area *area, list router_list, struct in_addr *CSPFvertex, 
		struct in_addr *CSPFlinks, u_int32_t graph[100][100], u_int8_t SwitchingCapability) {

  list te_link_lsa_list=NULL;
  struct ospf_lsa* te_lsa=NULL;
  listnode te_lsa_node;
  u_int32_t bandwidth=1000; /*Suppose one wavelength capacity is 2.5G*/
  
  u_int8_t graphsize;
  int i, j, m;
  int linkindex;
  int linksize; /*the number of links*/
  graphsize=router_list->count;
  linksize=graphsize*graphsize;


  /*initialize the graph array */
  for (i = 0; i<graphsize; i++) {
  	 for (j = 0; j<graphsize; j++) {
	  	graph[i][j]=1000000; /*infinity*/
  	  }
  }

  /*the value in graph array is metric*/
  for (i = 0; i<graphsize; i++) {
  	te_link_lsa_list=ospf_te_lsdb_lookup_by_adv_router (area->te_lsdb, CSPFvertex[i]);
  	te_lsa_node=listhead(te_link_lsa_list);

	while (te_lsa_node) {
		te_lsa=te_lsa_node->data;

		if (!te_lsa) {
			nextnode(te_lsa_node);
			continue;
		}
		
		/* Check the switching capability of current te_link. */
		if ( te_lsa->te_lsa_type == ROUTER_ID_TE_LSA) {
			nextnode(te_lsa_node);
			continue;
		}

		if (!te_lsa->tepara_ptr) {
			nextnode(te_lsa_node);
			continue;
		} 
		if (!te_lsa->tepara_ptr->p_link_ifswcap_list) {
			nextnode(te_lsa_node);
			continue;
		}
		if (!te_lsa->tepara_ptr->p_link_ifswcap_list) {
			nextnode(te_lsa_node);
			continue;
		}
		if (!ospf_te_lsa_swcap_lookup(te_lsa->tepara_ptr->p_link_ifswcap_list, SwitchingCapability)) {
			nextnode(te_lsa_node);
			continue;
		}
		/*no enough bandwidth*/
		/*
		if (te_lsa->tepara_ptr->p_unrsv_bw->value[0]==0)  {
			nextnode(te_lsa_node);
			continue;
		}
		*/
		if (IS_LSA_MAXAGE (te_lsa)) {
			nextnode(te_lsa_node);
			continue;
		}
		/* Now we can add this link to graph*/
		m=get_vertex_index(graphsize, CSPFvertex,te_lsa->tepara_ptr->p_link_id->value);
		if (m==-1) {
			nextnode(te_lsa_node);
			continue;
		}

       if (te_lsa->tepara_ptr->p_te_metric)
      	  graph[i][m] = ntohl (te_lsa->tepara_ptr->p_te_metric->value);
       else
         graph[i][m] = 1;
		linkindex=i*graphsize+m;
		if (!te_lsa->tepara_ptr->p_lclif_ipaddr || 
				!te_lsa->tepara_ptr->p_rmtif_ipaddr){
			nextnode(te_lsa_node);
			continue;
		}
		CSPFlinks[linkindex].s_addr=te_lsa->tepara_ptr->p_lclif_ipaddr->value.s_addr;
		linkindex=m*graphsize+i;
		CSPFlinks[linkindex].s_addr=te_lsa->tepara_ptr->p_rmtif_ipaddr->value.s_addr;
		
		/*add link local id and remote id to TE-link list*/
				
		nextnode(te_lsa_node);
	 }
  }
  if (te_link_lsa_list) 
  	list_free(te_link_lsa_list);
}


void DJhop(u_int32_t node[100][100], u_int8_t node_number, int Source, 
	int path[100][100], int hop[100], int cost[100])
	{
		int node_inset[100];
			// array to record if a node has been selected by the shortest path
		int graphhop[100][100];
			// array to ignore the cost of a connection and focus on
			// if there is connection only
		
		int i,j,k,m;
		int infcost=1000000;
		int shortcost;

		for (i=0; i<node_number; i++)
			for (j=0; j<node_number; j++)
				path[i][j]=-1;
		
		path[Source][0]=Source;
		
		for (i=0; i<node_number; i++) {
			for (j=0; j<node_number; j++) {
        /*
				graphhop[i][j]=(int) infcost;
				if (node[i][j]!=infcost)
					graphhop[i][j]=1;
    	*/
    	      graphhop[i][j]=node[i][j];
			}
		}
		
		for (i=0; i<node_number; i++) {
			node_inset[i]=0;
			hop[i]=node_number+1;
			cost[i]=infcost;
			if (node[Source][i]!=infcost) {
				hop[i]=2;
				cost[i]=graphhop[Source][i];
			} 
			if (hop[i]<node_number+1) {
				path[i][0]=Source;
				path[i][1]=i;
			} else {
				path[i][0]=-1;
			}
		}
		
		node_inset[Source]=1;
		
		for (i=0; i<node_number; i++) {
			shortcost=infcost;
			k=-1;
			for (j=0; j<node_number; j++) {
				if ((!node_inset[j]) && (cost[j]<shortcost)) {
					shortcost=cost[j];
					k=j;
				}
			}
			if (k!=-1) {
				node_inset[k]=1;
				for (j=0; j<node_number; j++) {
					if ((!node_inset[j]) && (node[k][j]!=infcost)) {
						if (cost[j]>cost[k]+graphhop[k][j]) {
							cost[j]=cost[k]+graphhop[k][j];
							for (m=0; m<node_number; m++)
								path[j][m]=path[k][m];
							hop[j]=hop[k]+1;
							path[j][hop[j]-1]=j;
						}
							
					}
				}
			}
		}

	}


/* Calculating the shortest-path tree for an area. */
list
ospf_cspf_calculate (struct ospf_area *area, struct in_addr source_ip, struct in_addr dest_ip, 
                    u_int8_t SwitchingCapability)
{
  struct vertex *v;
  list router_list=NULL;
  list explicit_path=NULL;
  struct in_addr *CSPFvertex;
  struct in_addr *CSPFlinks;
  u_int32_t graph[100][100];
  int path[100][100]; /*store the path*/
  int hop[100]; /*store the number of hops to each destination*/
  int cost[100]; /*store the cost to each destination*/
  int source, dest;
  int i;
  struct in_addr* router_ip;
  struct in_addr* local_if_ip;
  struct in_addr* remote_if_ip;
  u_int8_t graphsize;
  int j;
  int k;
  int linkindex;
  
  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_info ("ospf_cspf_calculate: Start");
    }

 
  /* Initialize the shortest-path tree to only the root (which is the
     router doing the calculation). */
  ospf_spf_init (area);
  v = area->spf;

 /*build router list from te_lsdb*/
  router_list=ospf_build_routers_from_te_lsdb(area->te_lsdb);
  if (!router_list) return explicit_path;
  graphsize=router_list->count;
  
  /* build router array from router list*/
  CSPFvertex=build_CSPF_vertex(router_list);
  /* build link list*/
  CSPFlinks=init_CSPF_TE_Link_list(router_list);

 /*Due to the limitation of array, we cannot process SPF with more than 100 routers*/
  if (router_list->count>100) return explicit_path;
 /* Only one vertex. Not necessary to build graph */ 
  if (router_list->count<=1) return explicit_path;
 /* Build the graph with all the links without required swcap pruned.*/ 
 build_CSPF_graph(area, router_list, CSPFvertex, CSPFlinks, graph, SwitchingCapability);
 /*Find the shortest path*/
 /*source=get_vertex_index(graphsize, CSPFvertex, v->id);*/
 source=get_vertex_index(graphsize, CSPFvertex, source_ip);
 if (source==-1) return explicit_path;
 dest=get_vertex_index(graphsize, CSPFvertex, dest_ip);
 if (dest==-1) return explicit_path;  
 DJhop(graph, graphsize, source, path, hop, cost);

 if (path[dest][0]==-1) return explicit_path; /*Path not found*/
 
 /*path found. Add path to explicit path list*/
 /*
 for (i=0; i<hop[dest]; i++)  {
 	j=path[dest][i];
 	router_ip=( struct in_addr*) malloc(sizeof(struct in_addr));
 	router_ip->s_addr=CSPFvertex[j].s_addr;
	listnode_add(explicit_path, router_ip);
 } 
 

 router_ip=( struct in_addr*) malloc(sizeof(struct in_addr));
 j=path[dest][0];
 router_ip->s_addr=CSPFvertex[j].s_addr;
 listnode_add(explicit_path, router_ip);
*/

/* explicit path list is to store the E-LSP to dest_ip */
explicit_path=list_new();
 
for (i=0; i<hop[dest]-1; i++)  {
 	j=path[dest][i];
	k=path[dest][i+1];
	linkindex=j*graphsize+k;
 	local_if_ip=( struct in_addr*) malloc(sizeof(struct in_addr));
 	local_if_ip->s_addr=CSPFlinks[linkindex].s_addr;
	listnode_add(explicit_path, local_if_ip);
	linkindex=k*graphsize+j;
	remote_if_ip=( struct in_addr*) malloc(sizeof(struct in_addr));
 	remote_if_ip->s_addr=CSPFlinks[linkindex].s_addr;
	listnode_add(explicit_path, remote_if_ip);
 }
/*
 router_ip=( struct in_addr*) malloc(sizeof(struct in_addr));
 j=path[dest][i+1];
 router_ip->s_addr=CSPFvertex[j].s_addr;
 listnode_add(explicit_path, router_ip);
 */
 if (CSPFvertex) 
 	free(CSPFvertex);
 if (CSPFlinks) 
 	free(CSPFlinks);
 
  /* Increment SPF Calculation Counter. */
  area->spf_calculation++;

  area->ospf->ts_spf = time (NULL);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_spf_calculate: Stop");

  return explicit_path;
}
 
#endif
