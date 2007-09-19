/* Virtual terminal interface shell.
 * Copyright (C) 2000 Kunihiro Ishiguro
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

#include <sys/un.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
 #include <fcntl.h>


#include "command.h"
#include "memory.h"
#include "vector.h"
#include "vty.h"
#include "prefix.h"
#include "thread.h"
#include "stream.h"
#include "linklist.h"
#include "log.h"
#include "dragon/dragond.h"

extern u_int32_t narb_extra_options;
extern void dragon_upcall_callback(int, struct lsp*);

int 
is_mandated_params_set_for_lsp(struct lsp *lsp)
{
	if ( lsp->common.Session_Para.srcAddr.s_addr == 0 || lsp->common.Session_Para.destAddr.s_addr == 0
	    || (lsp->common.Session_Para.srcPort == 0 && lsp->dragon.srcLocalId == 0)
	    || (lsp->common.Session_Para.destPort == 0 && lsp->dragon.destLocalId == 0) )
	     return 0;
	
	return 1;
}

/*Obsolete header format*/
static struct dmsg_header *
build_dragon_msg_header (struct stream *s, u_int8_t msgtype, u_int32_t seqno) 
{
	struct dmsg_header dmsgh, *dmsgh_p;
	
	dmsgh.version = DRAGON_VERSION;
	dmsgh.msgtype = msgtype;	/* create a topology */
	dmsgh.seqno = htonl(seqno);

	stream_put (s, &dmsgh, sizeof (struct dmsg_header));

	dmsgh_p = (struct dmsg_header *) STREAM_DATA (s);

	return dmsgh_p; 
}

/*Instead we use the unified api_msg format*/
static struct api_msg_header * 
build_api_msg_header (struct stream *s, u_int16_t type, u_int16_t length , u_int32_t ucid, u_int32_t seqnum, u_int32_t opt, u_int32_t tag)
{
    struct api_msg_header ah, *ah_p;

    memset (&ah, 0, sizeof(struct api_msg_header));

    ah.type = htons(type);
    ah.length = htons (length);
    ah.seqnum = htonl (seqnum);
    ah.ucid = htonl(ucid);
    ah.options = htonl(opt);
    ah.tag = htonl(tag);
    ah.chksum = API_MSG_CHKSUM(ah);

    stream_put (s, &ah, sizeof(struct api_msg_header));

    ah_p = (struct api_msg_header *)STREAM_DATA(s);

    return ah_p;
}

static void 
build_dragon_tlv_srcdst (struct stream *s, u_int16_t t, struct lsp *lsp) 
{
	u_int16_t type = htons(t);
	u_int16_t length = htons(16);
       float bw = *(float*)&(lsp->common.GenericTSpec_Para)->R;
       bw = Bps2Mbps(bw);

	/* Put TLV header */
	stream_put (s, &type, sizeof (u_int16_t));
	stream_put (s, &length, sizeof (u_int16_t));

	/* Put TLV data */
	stream_put (s, &lsp->common.Session_Para.srcAddr.s_addr, sizeof (u_int32_t));
	stream_put (s, &lsp->common.Session_Para.destAddr.s_addr, sizeof (u_int32_t));
	stream_put (s, &lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType, sizeof (u_int8_t));
	stream_put (s, &lsp->common.LabelRequest_Para.data.gmpls.switchingType, sizeof (u_int8_t));
	stream_put (s, &lsp->common.LabelRequest_Para.data.gmpls.gPid, sizeof (u_int16_t));
	stream_put (s, &bw, sizeof (u_int32_t));

	return; 
}

static int
build_dragon_tlv_ero (char *buf, struct lsp *lsp)
{
	u_int16_t type = htons(DMSG_CLI_TOPO_ERO);
	u_int16_t length = 4;
	struct _EROAbstractNode_Para *eroNodePara;
	char *p = buf + length;
	int i;

	if (lsp->common.ERONodeNumber == 0)
		return 0;

	for (i = 0, eroNodePara = lsp->common.EROAbstractNode_Para; i < lsp->common.ERONodeNumber; i++, eroNodePara++)
	{
		if (eroNodePara->type == IPv4)
		{
			((struct AbstractNode_IPv4 *)p)->typeOrLoose = eroNodePara->type | (eroNodePara->isLoose << 7);
			((struct AbstractNode_IPv4 *)p)->length = sizeof(struct AbstractNode_IPv4 );
			memcpy(((struct AbstractNode_IPv4 *)p)->addr, &eroNodePara->data.ip4.addr, sizeof(struct in_addr));
			((struct AbstractNode_IPv4 *)p)->prefix = eroNodePara->data.ip4.prefix;
			((struct AbstractNode_IPv4 *)p)->resvd = 0;
			p+=sizeof(struct AbstractNode_IPv4);
			length += sizeof(struct AbstractNode_IPv4);
			
		}
		else if (eroNodePara->type == UNumIfID)
		{
			((struct AbstractNode_UnNumIfID *)p)->typeOrLoose = eroNodePara->type | (eroNodePara->isLoose << 7);
			((struct AbstractNode_UnNumIfID *)p)->length = sizeof(struct AbstractNode_UnNumIfID );
			memcpy(((struct AbstractNode_UnNumIfID *)p)->routerID, &eroNodePara->data.uNumIfID.routerID, sizeof(struct in_addr));
			((struct AbstractNode_UnNumIfID *)p)->interfaceID =  htonl(eroNodePara->data.uNumIfID.interfaceID);
			((struct AbstractNode_UnNumIfID *)p)->resvd = 0;
			p+=sizeof(struct AbstractNode_UnNumIfID);
			length += sizeof(struct AbstractNode_UnNumIfID);
		}
	}

	if (length <= 4)
		return 0;

	/* Put TLV header */
	*(u_int16_t*)buf = type;
	*(u_int16_t*)(buf+2) = htons(length - 4);
	
	return length;
}

struct dragon_fifo_elt *
dragon_packet_new(size_t size)
{
   struct dragon_fifo_elt *new;
	
   new = XCALLOC (MTYPE_OSPF_DRAGON, sizeof (struct dragon_fifo_elt));
   new->s = stream_new (size);

   new->next = NULL;

  return new;
  
}

void
dragon_packet_free (struct dragon_fifo_elt *op)
{
  if (op->s)
    stream_free (op->s);

  XFREE (MTYPE_OSPF_DRAGON, op);

  op = NULL;
}

struct dragon_fifo *
dragon_fifo_new ()
{
  struct dragon_fifo *new;

  new = XCALLOC (MTYPE_OSPF_DRAGON, sizeof (struct dragon_fifo));
  return new;
}

/* Add new packet to fifo. */
void
dragon_fifo_push (struct dragon_fifo *fifo, struct dragon_fifo_elt *op)
{
  if (fifo->tail)
    fifo->tail->next = op;
  else
    fifo->head = op;

  fifo->tail = op;

  fifo->count++;
}

/* Delete first packet from fifo. */
struct dragon_fifo_elt *
dragon_fifo_pop (struct dragon_fifo *fifo)
{
  struct dragon_fifo_elt *op;

  op = fifo->head;

  if (op)
    {
      fifo->head = op->next;

      if (fifo->head == NULL)
	fifo->tail = NULL;

      fifo->count--;
    }

  return op;
}

/* Return first fifo entry. */
struct dragon_fifo_elt *
dragon_fifo_head (struct dragon_fifo *fifo)
{
  return fifo->head;
}

/* Flush dragon packet fifo. */
void
dragon_fifo_flush (struct dragon_fifo *fifo)
{
  struct dragon_fifo_elt *op;
  struct dragon_fifo_elt *next;

  for (op = fifo->head; op; op = next)
    {
      next = op->next;
      dragon_packet_free (op);
    }
  fifo->head = fifo->tail = NULL;
  fifo->count = 0;
}

int
dragon_fifo_count (struct dragon_fifo *fifo)
{
   return fifo->count;
}

/* Free dragon packet fifo. */
void
dragon_fifo_free (struct dragon_fifo *fifo)
{
  dragon_fifo_flush (fifo);

  XFREE (MTYPE_OSPF_DRAGON, fifo);
}

u_int32_t
dragon_assign_seqno (void)
{
  static u_int32_t seqnr = DRAGON_INITIAL_SEQUENCE_NUMBER; /* + global offset !! */
  u_int32_t tmp;

  tmp = seqnr;
  /* Increment sequence number */
  if (seqnr < DRAGON_MAX_SEQUENCE_NUMBER)
    {
      seqnr++;
    }
  else
    {
      seqnr = DRAGON_INITIAL_SEQUENCE_NUMBER;
    }
  return tmp;
}


/* Topology creation message */
struct dragon_fifo_elt *
dragon_topology_create_msg_new(struct lsp *lsp)
{
  struct stream *s;
  struct api_msg_header *amsgh;
  struct dragon_fifo_elt *packet;
  int msglen;

  /* Create a stream for topology request. */
  packet = dragon_packet_new(DRAGON_MAX_PACKET_SIZE);
  s = packet->s;
  packet->lsp = lsp;
  
  /* Build DRAGON message header */
  msglen = 20; 
  if (lsp->dragon.srcLocalId != 0 && lsp->dragon.destLocalId != 0)
      msglen += sizeof(u_int16_t)*2 + sizeof(u_int32_t)*2;
  if (lsp->dragon.lspVtag)
      amsgh = build_api_msg_header(s, NARB_MSG_LSPQ, msglen, dmaster.UCID, lsp->seqno, 
        LSP_OPT_STRICT | LSP_OPT_MRN | LSP_OPT_E2E_VTAG
        |((lsp->flag & LSP_FLAG_BIDIR) == 0 ? 0: LSP_OPT_BIDIRECTIONAL) | narb_extra_options, 
        lsp->dragon.lspVtag);
  else
      amsgh = build_api_msg_header(s, NARB_MSG_LSPQ, msglen, dmaster.UCID, lsp->seqno,
        LSP_OPT_STRICT | ((lsp->flag & LSP_FLAG_BIDIR) == 0 ? 0: LSP_OPT_BIDIRECTIONAL) | narb_extra_options, 
        0);

  /* Build mandatory /request TLVs */
  build_dragon_tlv_srcdst(s, DMSG_CLI_TOPO_CREATE, lsp);

  /* Put optional TLV data */
  /* Local ID TLV */
  if (lsp->dragon.srcLocalId != 0 && lsp->dragon.destLocalId != 0)
  {
      u_int16_t type, length;
      u_int32_t src_lclid, dest_lclid;
      type = htons(DRAGON_TLV_LCLID);
      length = htons(sizeof(u_int32_t)*2);
      src_lclid = htonl(lsp->dragon.srcLocalId);
      dest_lclid = htonl(lsp->dragon.destLocalId);
      stream_put (s, &type, sizeof(u_int16_t));
      stream_put (s, &length, sizeof(u_int16_t));
      stream_put (s, &src_lclid, sizeof(u_int32_t));
      stream_put (s, &dest_lclid, sizeof(u_int32_t));
  }
  
  return packet;
}

/* Topology Confirmation message */
struct dragon_fifo_elt *
dragon_topology_confirm_msg_new(struct lsp *lsp)
{
  struct stream *s;
  struct api_msg_header *amsgh;
  struct dragon_fifo_elt *packet;
  char ero_buf[DRAGON_MAX_PACKET_SIZE];
  int ero_len = build_dragon_tlv_ero(ero_buf, lsp);

  /* Create a stream for topology request. */
  packet = dragon_packet_new(DRAGON_MAX_PACKET_SIZE);
  s = packet->s;
  packet->lsp = lsp;
  
  amsgh = build_api_msg_header(s, NARB_MSG_LSPQ, 20 + ero_len, dmaster.UCID, lsp->seqno,
    LSP_OPT_STRICT|((lsp->flag & LSP_FLAG_BIDIR) == 0 ? 0: LSP_OPT_BIDIRECTIONAL)|narb_extra_options, lsp->dragon.lspVtag);

  /* Build TLVs */
  build_dragon_tlv_srcdst(s, DMSG_CLI_TOPO_CONFIRM, lsp);

  if (ero_len > 0)
  	stream_put(s, ero_buf, ero_len);

  return packet;
}

/* Topology Removal message */
struct dragon_fifo_elt *
dragon_topology_remove_msg_new(struct lsp *lsp)
{
  struct stream *s;
  struct api_msg_header *amsgh;
  struct dragon_fifo_elt *packet;
  char ero_buf[DRAGON_MAX_PACKET_SIZE];
  int ero_len = build_dragon_tlv_ero(ero_buf, lsp);
  
  /* Create a stream for topology request. */
  packet = dragon_packet_new(DRAGON_MAX_PACKET_SIZE);
  s = packet->s;
  packet->lsp = lsp;
  
  amsgh = build_api_msg_header(s, NARB_MSG_LSPQ, 20 + ero_len, dmaster.UCID, lsp->seqno,
      LSP_OPT_STRICT|((lsp->flag & LSP_FLAG_BIDIR) == 0 ? 0: LSP_OPT_BIDIRECTIONAL)|narb_extra_options, lsp->dragon.lspVtag);

  /* Build TLVs */
  build_dragon_tlv_srcdst(s, DMSG_CLI_TOPO_DELETE, lsp);
  if (ero_len > 0)
  	stream_put(s, ero_buf, ero_len);

  return packet;
}

int 
dragon_lsp_refresh_timer(struct thread *t)
{
   struct lsp *lsp = THREAD_ARG (t);
   struct dragon_fifo_elt *new;

   lsp->t_lsp_refresh = NULL;

   /* LSP is still in state COMMIT, indicating that 
       a response hasn't been received from NARB. 
       Have a retry here 
   */
   if (lsp->status == LSP_COMMIT)
   {
	   /* Construct topology create message */
	   new = dragon_topology_create_msg_new(lsp);
	   
	   /* Put packet into fifo */
	   dragon_fifo_push(dmaster.dragon_packet_fifo, new);
	   
	   /* Start LSP refresh timer */
	   DRAGON_TIMER_ON (lsp->t_lsp_refresh, dragon_lsp_refresh_timer, lsp, DRAGON_LSP_REFRESH_INTERVAL);
	   
	   /* Write packet to socket */
	   DRAGON_WRITE_ON(dmaster.t_write, NULL, lsp->narb_fd);
   	
   }
   return 0;
}

struct stream *
dragon_recv_packet (int fd)
{
  struct stream *ibuf;
  char buff[DRAGON_MAX_PACKET_SIZE];
  struct sockaddr_in from;
  int fromlen, len;
  struct api_msg_header amsgh;

  len = recvfrom (fd, (void *)&amsgh, sizeof (amsgh), MSG_PEEK, NULL, 0);
  if (len < 0 || len != sizeof(amsgh)) 
  {
      zlog_info ("recvfrom failed: %s", strerror (errno));
      return NULL;
  }
  memset (&from, 0, sizeof (struct sockaddr_in));
  fromlen = sizeof(amsgh) + ntohs(amsgh.length);

  len = recvfrom (fd, &buff, DRAGON_MAX_PACKET_SIZE, 0, 
		  (struct sockaddr *) &from, &fromlen);
  if (len < 0) 
    {
      zlog_info ("recvfrom failed: %s", strerror (errno));
      return NULL;
    }
  
  ibuf = stream_new (len);
  stream_put(ibuf, (void *)buff, len);
  
  return ibuf;
}

struct _EROAbstractNode_Para *
dragon_narb_topo_rsp_proc_ero(struct dragon_tlv_header *tlvh, u_int8_t *node_number)
{
	struct _EROAbstractNode_Para *node;
	struct AbstractNode_IPv4 *node_ipv4;
	struct AbstractNode_AS *node_as;
	struct AbstractNode_UnNumIfID *node_un;
	u_int32_t read_len = 0;
	u_int8_t i = 0;
 	char *p = (char*)((char *)tlvh + DTLV_HDR_SIZE);

	node = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct _EROAbstractNode_Para)*MAX_ERO_NUMBER);
	while (read_len < DTLV_BODY_SIZE(tlvh))
	{
		switch (ABSTRACT_NODE_TYPE(*p))
		{	
			case IPv4:
				node_ipv4 = (struct AbstractNode_IPv4 *) p;
				node[i].type = IPv4;
				node[i].isLoose = ABSTRACT_NODE_LOOSE(*p)?1:0;
				memcpy(&node[i].data.ip4.addr, node_ipv4->addr, sizeof(struct in_addr));
				node[i].data.ip4.prefix = node_ipv4->prefix;
				read_len += sizeof(struct AbstractNode_IPv4);
				p+=sizeof(struct AbstractNode_IPv4);
				i++;
				break;

			case UNumIfID:
				node_un = (struct AbstractNode_UnNumIfID*) p;
				node[i].type = UNumIfID;
				node[i].isLoose = ABSTRACT_NODE_LOOSE(*p)?1:0;
				memcpy(&node[i].data.uNumIfID.routerID, node_un->routerID, sizeof(struct in_addr));
				node[i].data.uNumIfID.interfaceID = ntohl(node_un->interfaceID);
				read_len += sizeof(struct AbstractNode_UnNumIfID);
				p+=sizeof(struct AbstractNode_UnNumIfID);
				i++;
				break;

			case AS:
				node_as = (struct AbstractNode_AS*) p;
				node[i].type = AS;
				node[i].isLoose = ABSTRACT_NODE_LOOSE(*p)?1:0;
				node[i].data.asNum = ntohs(node_as->asNum);
				read_len += sizeof(struct AbstractNode_AS);
				p+=sizeof(struct AbstractNode_AS);
				i++;
				break;
				
			default:
				break;
		}
	}
	*node_number = i;
	return node;
}

struct lsp *
dragon_find_lsp_by_seqno(u_int32_t seqno)
{
	struct lsp *lsp, *find=NULL;
	listnode node;

	if (dmaster.dragon_lsp_table)
	{
		LIST_LOOP(dmaster.dragon_lsp_table,lsp,node)
		{
			 if (lsp->seqno == seqno) {
				 find = lsp;
				 break;
			 }
		}
	}
	return find;
}

struct lsp *
dragon_find_lsp_by_rsvpupcallparam(struct _rsvp_upcall_parameter *p)
{
	struct lsp *lsp, *find=NULL;
	listnode node;

	if (dmaster.dragon_lsp_table)
	{
		LIST_LOOP(dmaster.dragon_lsp_table,lsp,node)
		{
			 if (lsp->common.Session_Para.srcAddr.s_addr == p->srcAddr.s_addr &&
			     lsp->common.Session_Para.destAddr.s_addr == p->destAddr.s_addr &&
			     lsp->common.Session_Para.destPort == p->destPort ) {
				 find = lsp;
				 break;
			 }
		}
	}
	return find;
}

/* Topology response from NARB, should contain an ERO */
void 
dragon_narb_topo_rsp_proc(struct api_msg_header *amsgh)
{
	int i, num_newnodes;
	struct dragon_tlv_header *tlvh;
	u_int32_t read_len;
	struct lsp *lsp = NULL;
       struct _EROAbstractNode_Para *srcLocalId=NULL, *destLocalId=NULL;
       struct _EROAbstractNode_Para *temp_ero = NULL;
       char buf[sizeof(struct _EROAbstractNode_Para)*2];

	/* Look for the corresponding LSP request according to the sequence number */
	if (!(lsp = dragon_find_lsp_by_seqno(ntohl(amsgh->seqnum))))
		return;
	
	/* Parse message */
	read_len = 0;
	tlvh = DTLV_HDR_TOP(amsgh);

	while (read_len < ntohs(amsgh->length))
	{
		switch (ntohs(tlvh->type))
		{
			case DRAGON_TLV_ERO:
				if (lsp->common.EROAbstractNode_Para)
					XFREE(MTYPE_OSPF_DRAGON, lsp->common.EROAbstractNode_Para);
				lsp->common.EROAbstractNode_Para = dragon_narb_topo_rsp_proc_ero(tlvh, &lsp->common.ERONodeNumber);

				if (lsp->dragon.lspVtag == ANY_VTAG)
				{
					for ( i = 0; i < lsp->common.ERONodeNumber; i++)
					{
						if (lsp->common.EROAbstractNode_Para[i].type == UNumIfID
							&& (lsp->common.EROAbstractNode_Para[i].data.uNumIfID.interfaceID >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
						{
							lsp->dragon.lspVtag = (lsp->common.EROAbstractNode_Para[i].data.uNumIfID.interfaceID & 0xffff);
						}
					}
				}

				// NARB returned ERO with confirmation ID, which indicates a different inter-domain routing/signaling mode
				if (ntohl(amsgh->options) & LSP_OPT_QUERY_CONFIRM)
				{
					if (lsp->common.DragonExtInfo_Para == NULL)
					{
						lsp->common.DragonExtInfo_Para = XMALLOC(MTYPE_TMP, sizeof(struct _Dragon_ExtInfo_Para));
						memset(lsp->common.DragonExtInfo_Para, 0, sizeof(struct _Dragon_ExtInfo_Para));
					}
					lsp->common.DragonExtInfo_Para->ucid = ntohl(amsgh->ucid);
					lsp->common.DragonExtInfo_Para->seqnum = ntohl(amsgh->seqnum);
				}

				
				if (lsp->dragon.srcLocalId != 0 && lsp->dragon.destLocalId != 0
					&& lsp->common.EROAbstractNode_Para[0].data.uNumIfID.interfaceID >> 16 == LOCAL_ID_TYPE_SUBNET_UNI_SRC
					&& lsp->common.EROAbstractNode_Para[lsp->common.ERONodeNumber-1].data.uNumIfID.interfaceID >> 16 == LOCAL_ID_TYPE_SUBNET_UNI_DEST)
				{
					/*subnet-interface local-ids have been passed to NARB for edge constraints.	The returned ERO contains 
					  edge port control information equivalent to local-ids. Just convert them into the equivalent local-ids*/
					lsp->common.EROAbstractNode_Para[0].data.uNumIfID.routerID.s_addr = lsp->common.Session_Para.srcAddr.s_addr;
					lsp->common.EROAbstractNode_Para[0].data.uNumIfID.interfaceID = ((LOCAL_ID_TYPE_SUBNET_IF_ID << 16) 
						| (lsp->common.EROAbstractNode_Para[0].data.uNumIfID.interfaceID & 0xffff));
					lsp->common.EROAbstractNode_Para[lsp->common.ERONodeNumber-1].data.uNumIfID.routerID.s_addr = lsp->common.Session_Para.destAddr.s_addr;
					lsp->common.EROAbstractNode_Para[lsp->common.ERONodeNumber-1].data.uNumIfID.interfaceID = ((LOCAL_ID_TYPE_SUBNET_IF_ID << 16)
						| (lsp->common.EROAbstractNode_Para[lsp->common.ERONodeNumber-1].data.uNumIfID.interfaceID & 0xffff));					
					break;
				}

				/*Adding Local-ID ERO subobject(s)*/ 
				/*Create source localID subobj */
				if(lsp->dragon.srcLocalId >> 16 != LOCAL_ID_TYPE_NONE)
				{
				    srcLocalId = XMALLOC(MTYPE_TMP, sizeof(struct _EROAbstractNode_Para));
				    memset(srcLocalId, 0, sizeof(struct _EROAbstractNode_Para));
				    srcLocalId->type = UNumIfID;
				    srcLocalId->isLoose = 1;
				    memcpy(&srcLocalId->data.uNumIfID.routerID, &lsp->common.Session_Para.srcAddr, sizeof(struct in_addr));
				    srcLocalId->data.uNumIfID.interfaceID = lsp->dragon.srcLocalId;
				    srcLocalId->isLoose = 0;
				}
				/*Create destination localID subobj */
				if(lsp->dragon.destLocalId >> 16 != LOCAL_ID_TYPE_NONE)
				{
				    destLocalId = XMALLOC(MTYPE_TMP, sizeof(struct _EROAbstractNode_Para));
				    memset(destLocalId, 0, sizeof(struct _EROAbstractNode_Para));
				    destLocalId->type = UNumIfID;
				    destLocalId->isLoose = 1;
				    memcpy(&destLocalId->data.uNumIfID.routerID, &lsp->common.Session_Para.destAddr, sizeof(struct in_addr));
				    destLocalId->data.uNumIfID.interfaceID = lsp->dragon.destLocalId;
				    destLocalId->isLoose = 0;
				}
				num_newnodes = 0;
				if (srcLocalId) 
				    num_newnodes++;
				if (destLocalId)
				    num_newnodes++;                            
				if (srcLocalId)
				{
				    if (memmove(lsp->common.EROAbstractNode_Para + 1, lsp->common.EROAbstractNode_Para, sizeof(struct _EROAbstractNode_Para)*(lsp->common.ERONodeNumber)) == NULL)
				    {
				        zlog_warn("Error: dragon_narb_topo_rsp_proc: memory memmove failed");
				        return;
				    }
				    memcpy(lsp->common.EROAbstractNode_Para, srcLocalId, sizeof(struct _EROAbstractNode_Para));
				    lsp->common.ERONodeNumber++;
				    XFREE(MTYPE_TMP, srcLocalId);
				}
				if (destLocalId)
				{
				    memcpy(lsp->common.EROAbstractNode_Para + lsp->common.ERONodeNumber, destLocalId, sizeof(struct _EROAbstractNode_Para));
				    lsp->common.ERONodeNumber++;
				    XFREE(MTYPE_TMP, destLocalId);
				}
				break;

			default: /* Unrecognized tlv from NARB, just ignore it */
				break;
		}
		read_len += DTLV_SIZE(tlvh);
		tlvh = DTLV_HDR_NEXT(tlvh);
	}
	
	/* call RSVPD to set up the path */
	zInitRsvpPathRequest(dmaster.api, &lsp->common, 1);

	return;
}



/* Initialize a TCP socket for CLI/ASTDL --> NARB */
int 
dragon_narb_socket_init()
{
	struct sockaddr_in addr;
	int fd;
	int ret;
	static u_int16_t NARB_TCP_PORT = 2609;
	
	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
	  zlog_warn("dragon_narb_socket_init: connect(): %s", strerror (errno));
	  return (-1);
	}

	/* Prepare address structure for connect */
	  addr.sin_addr.s_addr = dmaster.module[MODULE_NARB_INTRA].ip_addr.s_addr;
	  addr.sin_family = AF_INET;
	  if (	dmaster.module[MODULE_NARB_INTRA].port == 0)
		addr.sin_port = htons(NARB_TCP_PORT);
	  else
	  	addr.sin_port = htons(dmaster.module[MODULE_NARB_INTRA].port);
#ifdef HAVE_SIN_LEN
	  addr.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
	
	  /* Now establish synchronous channel with NARB daemon */
	  ret = connect (fd, (struct sockaddr *) &addr,
			 sizeof (struct sockaddr_in));
	  if (ret < 0)
	  {
		  zlog_warn("dragon_narb_socket_init: connect(): %s", strerror (errno));
		  close (fd);
		  return (-1);
	  }

	  return fd;
}

int
dragon_read (struct thread *thread)
{
  struct stream *ibuf;
  struct api_msg_header *amsgh;
  struct lsp *lsp = THREAD_ARG(thread);
  struct lsp *l;
  u_int8_t find = 0;
  listnode node;
  int fd = THREAD_FD(thread);
  int lsp_deleted = 0;
 
  /* read packet. */
  ibuf = dragon_recv_packet (fd);
  if (ibuf == NULL)
    return -1;
    
  /* make sure that this lsp is still in the global LSP table */
  if (dmaster.dragon_lsp_table)
	  LIST_LOOP(dmaster.dragon_lsp_table,l,node)
	  {
		   if (lsp == l && lsp->narb_fd == fd) {
			   find = 1;
			   break;
		   }
	  }

  if (lsp->status == LSP_RECYCLE)
  {
	  zlog_warn("dragon_read: lsp has been deleted (recycled).");
	  return -1;  
  }
  if (!find)
  {
	  zlog_warn("dragon_read: cannot find lsp");
	  goto out;
  }
  	
  /* first of all get interface pointer. */
  lsp->t_narb_read = NULL;

  /* Process packet */
  amsgh = (struct api_msg_header *)STREAM_DATA(ibuf);
  if (ntohs(amsgh->length) != stream_get_endp(ibuf)-sizeof(struct api_msg_header))
  {
  	zlog_warn("dragon_read: incorrect dragon msglen %d, actual %d", ntohs(amsgh->length), stream_get_endp(ibuf)-sizeof(struct api_msg_header));
	goto out;
  }
  if (API_MSG_CHKSUM(*amsgh) != amsgh->chksum)
  {
	  zlog_warn("dragon_read: corrupt header in topology response message");
	  goto out;
  }
  if (!IS_VALID_DRAGON_SEQNO(ntohl(amsgh->seqnum)))
  {
	  zlog_warn("dragon_read: incorrect sequence number (%d)", ntohl(amsgh->seqnum) );
	  goto out;
  }
  if (ntohl(amsgh->seqnum) != lsp->seqno)
  {
	  zlog_warn("dragon_read: this packet (seqno=%d) is not for this LSP (seqno=%d)", ntohl(amsgh->seqnum), lsp->seqno);
	  goto out;
  }

  switch (ntohs(amsgh->type))
  {
  	case DMSG_NARB_TOPO_RESPONSE: /* contain ERO */
  		dragon_narb_topo_rsp_proc(amsgh);
		break;

	case DMSG_NARB_TOPO_REJECT: /* Request is rejected by the NARB */
		/*process_narb_topo_rej();*/
		break;

	case DMSG_NARB_TOPO_DELETE_CONF: /* Topology removal request is being confirmed by the NARB */
		if (lsp->status==LSP_DELETE && dmaster.api)
			zTearRsvpPathRequest(dmaster.api, &lsp->common);
		if (lsp->narb_fd > 0){
			close(lsp->narb_fd);
			lsp->narb_fd = 0;
		}
		lsp->status = LSP_EDIT;
		lsp_deleted = 1;
		break;

	default:
		break;
  }

out:
  /* prepare for next packet. */
  if (lsp->narb_fd)
	lsp->t_narb_read = thread_add_read (master, dragon_read, lsp, lsp->narb_fd);

  if (lsp_deleted)
  {
       DRAGON_TIMER_OFF (lsp->t_lsp_refresh);
	listnode_delete(dmaster.dragon_lsp_table, lsp);
	lsp_recycle(lsp); /*Keep lsp in recycle list for messages that were orginted from this LSP*/
	//lsp_del(lsp);
  }

  stream_free (ibuf); /* ??? */
  return 0;
}

int
dragon_write (struct thread *t)
{
  struct dragon_fifo_elt *packet = NULL;
  struct lsp *lsp;
  int ret;
  
  /* first of all get interface pointer. */
  dmaster.t_write = NULL;

  /* fetch from fifo and write packet. */
  while (dragon_fifo_count(dmaster.dragon_packet_fifo))
  {
  	packet = dragon_fifo_pop(dmaster.dragon_packet_fifo);
	lsp = packet->lsp;
	ret = write(lsp->narb_fd, (void *)(STREAM_DATA(packet->s)), stream_get_endp(packet->s));
	if (ret != stream_get_endp(packet->s))
	  zlog_warn ("packet was sent incompletely: total %d, actual %d", stream_get_endp(packet->s), ret);

	/* release packet */
	dragon_packet_free(packet);

	/* Prepare dragon_read thread */
	lsp->t_narb_read = thread_add_read (master, dragon_read, lsp, lsp->narb_fd);
  }

  return 0;
}

void dragon_fifo_lsp_cleanup (struct lsp* lsp)
{
	struct dragon_fifo_elt* packet, *packet_next;
	packet = dmaster.dragon_packet_fifo->head;
	if (!packet) return;

	packet = dmaster.dragon_packet_fifo->head;
	while (packet)
	{
		packet_next = packet->next;
		if (packet->lsp == lsp && packet == dmaster.dragon_packet_fifo->head)
		{
			dmaster.dragon_packet_fifo->head = packet->next;
			if (dmaster.dragon_packet_fifo->head== NULL)
				dmaster.dragon_packet_fifo->tail = NULL;
			dragon_packet_free(packet);
			dmaster.dragon_packet_fifo->count--;
			packet = packet_next;
			continue;
		}
		if (packet_next && packet_next->lsp == lsp)
		{
			packet->next = packet_next->next;
			if (dmaster.dragon_packet_fifo->tail == packet_next)
				dmaster.dragon_packet_fifo->tail = packet;
			dragon_packet_free(packet_next);
			dmaster.dragon_packet_fifo->count--;			
		}
		packet = packet->next;
	}
}

struct lsp* lsp_recycle(struct lsp* lsp)
{
	lsp->status = LSP_RECYCLE;
	listnode_add(dmaster.recycled_lsp_list, lsp);
}

struct lsp* lsp_new()
{
	struct lsp *l;
	listnode node;

	if (dmaster.recycled_lsp_list)
	LIST_LOOP(dmaster.recycled_lsp_list,l,node)
	{
		if (l->status == LSP_RECYCLE)
		{
			dragon_fifo_lsp_cleanup(l);
			memset(l, 0, sizeof(struct lsp));
			listnode_delete(dmaster.recycled_lsp_list, l);
			return l;
		}
	}
  	l = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct lsp));
	memset(l, 0, sizeof(struct lsp));
	return l;
}

void rsvpupcall_register_lsp(struct _rsvp_upcall_parameter* p)
{
	struct lsp *lsp;
	
  	lsp = lsp_new();
	set_lsp_default_para(lsp);
	lsp->status = LSP_LISTEN;
	 lsp->flag |= LSP_FLAG_REG_BY_RSVP;
	strcpy((lsp->common.SessionAttribute_Para)->sessionName, p->name);
	(lsp->common.SessionAttribute_Para)->nameLength = strlen(p->name);
	lsp->common.Session_Para.destAddr.s_addr = p->destAddr.s_addr;
	lsp->common.Session_Para.destPort = p->destPort;
	lsp->common.Session_Para.srcAddr = p->srcAddr;
	lsp->common.Session_Para.srcPort = p->srcPort;
	if (p->upstreamLabel){
		lsp->common.upstreamLabel = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(u_int32_t));
		*(lsp->common.upstreamLabel) = p->upstreamLabel;
		lsp->flag |= LSP_FLAG_BIDIR;
	}
	lsp->common.GenericTSpec_Para->R = 
	lsp->common.GenericTSpec_Para->B = 
	lsp->common.GenericTSpec_Para->P = p->bandwidth;
	lsp->common.LabelRequest_Para.labelType = LABEL_GENERALIZED;
	lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType = p->lspEncodingType;
	lsp->common.LabelRequest_Para.data.gmpls.switchingType = p->switchingType;
	lsp->common.LabelRequest_Para.data.gmpls.gPid = p->gPid;
	listnode_add(dmaster.dragon_lsp_table, lsp);	
	zInitRsvpPathRequest(dmaster.api, &lsp->common, 0); /* register this api in RSVPD */
	if (p->code == Path){
		zInitRsvpResvRequest(dmaster.api, p);
		lsp->status = LSP_IS;
	}
        /* update LSP based on dragonUNI*/
	if ( p->dragonUniPara) {
		lsp->common.DragonUni_Para = p->dragonUniPara;
		lsp->dragon.lspVtag = lsp->common.DragonUni_Para->vlanTag;
		//if ((lsp->common.DragonUni_Para->srcLocalId & 0xffff) == ANY_VTAG)
		//{
		//	lsp->common.DragonUni_Para->srcLocalId &= 0xffff0000; 
		//	lsp->common.DragonUni_Para->srcLocalId |= lsp->dragon.lspVtag ;
		//}
		//if ((lsp->common.DragonUni_Para->destLocalId & 0xffff) == ANY_VTAG)
		//{
		//	lsp->common.DragonUni_Para->destLocalId &= 0xffff0000; 
		//	lsp->common.DragonUni_Para->destLocalId |= lsp->dragon.lspVtag ;
		//}
		lsp->dragon.srcLocalId = lsp->common.DragonUni_Para->srcLocalId;
		lsp->dragon.destLocalId = lsp->common.DragonUni_Para->destLocalId;
	}

	if (p->vlanTag != 0)
		lsp->dragon.lspVtag = p->vlanTag;
}

void  rsvpUpcall(void* para)
{
	struct _rsvp_upcall_parameter* p = (struct _rsvp_upcall_parameter*)para;
	struct dragon_fifo_elt *new;
	struct lsp *lsp = NULL;
	int lsp_deleted = 0;

	lsp = dragon_find_lsp_by_rsvpupcallparam(p);
	if (!lsp)
	{
		zlog_warn("Unable to find LSP for this RSVP upcall.");
		if (p->code==Path)
			rsvpupcall_register_lsp(p);
		return;
	}
	if (lsp->common.SessionAttribute_Para)
		zlog_info("***** UPCALL: %s for LSP %s (seqno=%x)*****", 
				  value_to_string (&conv_rsvp_event, (u_int32_t)p->code),
				  (lsp->common.SessionAttribute_Para)->sessionName, lsp->seqno);
	else
		zlog_info("***** UPCALL: %s for LSP (seqno=%x)*****", 
				  value_to_string (&conv_rsvp_event, (u_int32_t)p->code),
				  lsp->seqno);

	dragon_show_lsp_detail(lsp, NULL);
	
	switch( p->code) {
		case Path:
			if (lsp->status == LSP_LISTEN && (lsp->flag & LSP_FLAG_RECEIVER) || 
				lsp->status == LSP_COMMIT && !(lsp->flag & LSP_FLAG_RECEIVER)  /* src-dest colocated w/ local-id */
				&& lsp->common.Session_Para.srcAddr.s_addr == lsp->common.Session_Para.destAddr.s_addr)
			{
				/* send RESV message to RSVPD to set up the path */
				zInitRsvpResvRequest(dmaster.api, para);
				lsp->status = LSP_IS;
				/* update LSP baed on DRAGON UNI Object */
				if (p->dragonUniPara) {
					lsp->common.DragonUni_Para = p->dragonUniPara;
					lsp->dragon.lspVtag = lsp->common.DragonUni_Para->vlanTag;
					lsp->dragon.srcLocalId = lsp->common.DragonUni_Para->srcLocalId;
					lsp->dragon.destLocalId = lsp->common.DragonUni_Para->destLocalId;
				}
				if (p->vlanTag != 0)
					lsp->dragon.lspVtag = p->vlanTag;
			}

			break;
			
		case Resv:
		case ResvConf:
			/* Update the status of the LSP */
			lsp->status = LSP_IS;
                     /* update LSP baed on DRAGON UNI Object */
			if (p->dragonUniPara) {
				lsp->common.DragonUni_Para = p->dragonUniPara;
				lsp->dragon.lspVtag = lsp->common.DragonUni_Para->vlanTag;
				lsp->dragon.srcLocalId = lsp->common.DragonUni_Para->srcLocalId;
				lsp->dragon.destLocalId = lsp->common.DragonUni_Para->destLocalId;
                     }

			if (lsp->narb_fd && (!(lsp->flag & LSP_FLAG_RECEIVER)))
			{
				/* Now we shall notify NARB these two events */
				new = dragon_topology_confirm_msg_new(lsp);
				/* Put packet into fifo */
				dragon_fifo_push(dmaster.dragon_packet_fifo, new);

				/* Write packet to socket */
				DRAGON_WRITE_ON(dmaster.t_write, NULL, lsp->narb_fd);
			}
			break;

		case PathTear:
			if (lsp->flag & LSP_FLAG_RECEIVER)
				lsp->status = LSP_LISTEN;
			else{
				if (lsp->flag & LSP_FLAG_REG_BY_RSVP)
					zTearRsvpPathRequest(dmaster.api, &lsp->common); /* Remove API entry off the list in RSVP */
				lsp_deleted = 1;
			}
			break;
			
		case ResvTear:
			if (!(lsp->flag & LSP_FLAG_RECEIVER)){
				if (lsp->narb_fd)
				{
					/* Now we shall notify NARB these two events */
					new = dragon_topology_remove_msg_new(lsp);
					/* Put packet into fifo */
					dragon_fifo_push(dmaster.dragon_packet_fifo, new);

					/* Write packet to socket */
					DRAGON_WRITE_ON(dmaster.t_write, NULL, lsp->narb_fd);
				}
				lsp->status = LSP_EDIT; 
				lsp_deleted = 1;
			}
			zTearRsvpPathRequest(dmaster.api, &lsp->common); /* Remove API entry off the list in RSVP */
			break;
			
		case PathErr:
		case ResvErr:
			break;
		default:
			break;
	}
	
	dragon_upcall_callback(p->code, lsp);
 
	if (lsp_deleted) {
	       DRAGON_TIMER_OFF (lsp->t_lsp_refresh);
	  	//dragon_fifo_lsp_cleanup(lsp); /**/
		listnode_delete(dmaster.dragon_lsp_table, lsp);
		lsp_recycle(lsp);
		//lsp_del(lsp);
	}

}

int
dragon_rsvp_read (struct thread *thread)
{
  dmaster.t_rsvp_read = NULL;

  /* prepare for next rsvp upcall. */
  dmaster.t_rsvp_read = thread_add_read (master, dragon_rsvp_read, NULL, dmaster.rsvp_fd);

  /* find the corresponding LSP */
  
  zApiReceiveAndProcess(dmaster.api, rsvpUpcall);

  return 0;
}

