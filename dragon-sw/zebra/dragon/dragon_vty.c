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
#include "log.h"
#include "linklist.h"
#include "dragon/dragond.h"
#include "buffer.h"

char lsp_prompt[100] = "%s(edit-lsp)# ";
u_int32_t narb_extra_options = 0;

struct cmd_node lsp_node =
{
  LSP_NODE,
  lsp_prompt,
  1
};

struct dragon_module dmodule[] =
{
	{MODULE_CLI, 	  {0x100007f},   2611,     "CLI"},
	{MODULE_OSPF,	  {0x100007f},   2604,     "OSPF-TE"},
	{MODULE_ZEBRA,	  {0x100007f},   2601,   	"ZEBRA"   },
	{MODULE_RSVP,	  {0x100007f},   0,  		 "RSVP-TE"},
	{MODULE_ASTDL,	  {0},   		0,   		"ASTDL"   },
	{MODULE_NARB_INTRA,   {0},   	0,  		 "NARB-Intra"	 },
	{MODULE_NARB_INTER,   {0},   	0,   		"NARB-Inter"    },
	{MODULE_PCE,	  {0},   		0,    		"PCE"      },
        {MODULE_XML,      {0x100007f},   2618,     "XML"}
};


struct string_value_conversion conv_protection = 
{
	6,
	{{ "extra", 	LINK_PROTYPE_SUBTLV_VALUE_EXTRA, 		2}, 	
	  { "none", 	LINK_PROTYPE_SUBTLV_VALUE_UNPRO, 		1}, 
	  { "shared", 	LINK_PROTYPE_SUBTLV_VALUE_SHARED, 		1},
	  { "1t1", 		LINK_PROTYPE_SUBTLV_VALUE_1TO1, 		2}, 
	  { "1p1", 	LINK_PROTYPE_SUBTLV_VALUE_1PLUS1, 		2}, 
	  {"en", 		LINK_PROTYPE_SUBTLV_VALUE_ENHANCED , 	2}}
};

struct string_value_conversion conv_swcap = 
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

struct string_value_conversion conv_encoding = 
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

struct string_value_conversion conv_gpid = 
{
	29,
	{{ "e4async", 		G_Asyn_E4, 			3}, 
	{ "ds3async", 		G_Asyn_DS3, 			4}, 
	{ "e3async", 		G_Asyn_E3, 			3}, 
	{ "e3bitsync", 		G_BitSyn_E3, 			4},
	{ "e3bytesync",   	G_ByteSyn_E3, 		4}, 
	{ "ds2async", 		G_Asyn_DS2, 			4}, 
	{ "ds2bitsync", 	G_BitSyn_DS2, 		4}, 
	{ "e1asyn", 		G_Asyn_E1, 			3},
	{ "e1bytesyn", 	G_ByteSyn_E1, 		3},
	{ "ds031bytesyn",	G_ByteSyn_31DS0, 	3},
	{ "ds1async",		G_Asyn_DS1,			4},
	{ "ds1bitsyn",		G_BitSyn_DS1,		4},
	{ "t1bytesyn",		G_ByteSyn_T1,		1},
	{ "vc11",			G_VC11,				2},
	{ "ds1sfasyn",		G_DS1SFAsyn,		4},
	{ "ds1esfasyn",	G_DS1ESFAsyn,		4},
	{ "ds3m23asyn",	G_DS3M23Asyn,		4},
	{ "ds3cbitasyn",	G_DS3CBitAsyn,		4},
	{ "vtlovc",		G_VT_LOVC,			2},
	{ "stshovc",		G_STS_HOVC,		2},
	{ "posunscr16",	G_POSUnscr16,		9},
	{ "posunscr32",	G_POSUnscr32,		9},
	{ "posscr16",		G_POSScram16,		7},
	{ "posscr32",		G_POSScram32, 		7},
	{ "atm",			G_ATM, 				1},
	{ "ethernet",		G_Eth, 				2},
	{ "sdh",			G_SONET_SDH, 		2},
	{ "wrapper",		G_DigiWrapper, 		1},
	{ "lambda",		G_Lamda, 			1}}
};

struct string_value_conversion conv_bandwidth = 
{
	42,
	{{ "ds0", 	LSP_BW_DS0, 			3}, 
	{ "ds1", 		LSP_BW_DS1, 			3}, 
	{ "e1", 		LSP_BW_E1, 				2}, 
	{ "ds2", 		LSP_BW_DS2, 			3},
	{ "e2", 		LSP_BW_E2, 				2}, 
	{ "eth10M", 	LSP_BW_Eth, 				6}, 
	{ "e3", 		LSP_BW_E3, 				2}, 
	{ "ds3",		LSP_BW_DS3, 			3},
	{ "sts1", 		LSP_BW_STS1,			1}, 
	{ "eth100M", 	LSP_BW_Fast_Eth,			7}, 
	{ "eth150M", 	LSP_BW_150m_Eth,		7}, 
	{ "eth200M", 	LSP_BW_200m_Eth,		7}, 
	{ "eth300M", 	LSP_BW_300m_Eth,		7}, 
	{ "eth400M", 	LSP_BW_400m_Eth,		7}, 
	{ "eth500M", 	LSP_BW_500m_Eth,		7}, 
	{ "eth600M", 	LSP_BW_600m_Eth,		7}, 
	{ "eth700M", 	LSP_BW_700m_Eth,		7}, 
	{ "eth800M", 	LSP_BW_800m_Eth,		7}, 
	{ "eth900M", 	LSP_BW_900m_Eth,		7}, 
	{ "e4", 		LSP_BW_E4,				2}, 
	{ "fc0133M", 	LSP_BW_FC0_133M,		5}, 
	{ "oc3", 		LSP_BW_OC3,				3}, 
	{ "fc0266M", 	LSP_BW_FC0_266M,		5}, 
	{ "fc0531M", 	LSP_BW_FC0_531M,		5}, 
	{ "oc12",		LSP_BW_OC12,			4}, 
	{ "gige",		LSP_BW_Gig_E,			5}, 
	{ "2gige",	LSP_BW_2Gig_E,			6}, 
	{ "3gige",	LSP_BW_3Gig_E,			6}, 
	{ "4gige",	LSP_BW_4Gig_E,			6}, 
	{ "5gige",	LSP_BW_5Gig_E,			6}, 
	{ "6gige",	LSP_BW_6Gig_E,			6}, 
	{ "7gige",	LSP_BW_7Gig_E,			6}, 
	{ "8gige",	LSP_BW_8Gig_E,			6}, 
	{ "9gige",	LSP_BW_9Gig_E,			6}, 
	{ "10g",		LSP_BW_10Gig_E,			3}, 
	{ "fc01062M",	LSP_BW_FC0_1062M,		6}, 
	{ "oc48",		LSP_BW_OC48,			4}, 
	{ "oc192",	LSP_BW_OC192,			4}, 
	{ "oc768",	LSP_BW_OC768,			4}, 
	{ "gige_f", 	LSP_BW_Gig_E_OverFiber,	5},
	{ "hdtv", 		LSP_BW_HDTV,			4}, 
	{ "zero", 		0,			4}} 
};

struct string_value_conversion conv_rsvp_event = 
{
	13,
	{{ "INIT_API", 		InitAPI, 			1}, 
	{ "PATH_EVENT", 		Path, 			7}, 
	{ "RESV_EVENT", 		Resv, 			7}, 
	{ "PATH_ERR", 		PathErr, 			7},
	{ "RESV_ERR", 		ResvErr, 			7}, 
	{ "PATH_TEAR", 		PathTear,			7}, 
	{ "RESV_TEAR", 		ResvTear, 		6}, 
	{ "RESV_CONF",		ResvConf, 		6},
	{ "ACK_EVENT", 		Ack,				1}, 
	{ "SREFRESH", 		Srefresh,			1}, 
	{ "LOAD_EVENT", 		Load,			1}, 
	{ "PATH_RESV", 		PathResv,		6}, 
	{ "RMV_API", 			RemoveAPI,		2}} 
};

struct string_value_conversion conv_lsp_status = 
{
	6,
	{{ "Edit", 			LSP_EDIT,		1}, 
	{ "Commit", 			LSP_COMMIT, 		1}, 
	{ "In service", 		LSP_IS, 		1}, 
	{ "Delete", 			LSP_DELETE, 		1},
	{ "Listening", 			LSP_LISTEN, 		1},
	{ "Error", 			LSP_ERROR, 		1}}
};

/* registerred local_id's */
list registered_local_ids;

char *lid_types[] = {"none id", "single port", "untagged group", "tagged group", "reserved", "subnet interface id"};

u_int32_t 
string_to_value(struct string_value_conversion *db, const char *str)
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
value_to_string(struct string_value_conversion *db, u_int32_t value)
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

const char* bandwidth_value_to_string(struct string_value_conversion *db, u_int32_t value)
{
	static char bw_str[20];
	float* bw = (float*)&value;
	const char* pstr = value_to_string (&conv_bandwidth, value);
	if (strcmp(pstr, "Unknown") != 0)
		return pstr;
	*bw = (*bw)*8/1000000;
	if (*bw > 0 && *bw <= 10000.0)
		sprintf(bw_str, "eth%.1fM", *bw);
	else
		sprintf(bw_str, "Unknown");		
	return (const char*)bw_str;
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

	sprintf(port_string, "%d/%d/%d", (switch_port>>12)&0xf, (switch_port>>8)&0xf, switch_port&0xff);
	return port_string;
}

struct local_id *
search_local_id(u_int16_t tag, u_int16_t type)
{
    struct local_id *lid = NULL;
    listnode node;
    int found = 0;

    LIST_LOOP(registered_local_ids, lid, node) {
      if (lid->type == type && lid->value == tag) {
	found = 1;
	break;
      }
    }

    if (found)
      return lid;
    else
      return NULL;
}

/* local_id_group_mapping operators */
void local_id_group_add(struct local_id *lid, u_int16_t  tag)
{
    u_int16_t * ptag = NULL;
    listnode node;
    assert(lid);
    if (lid->type != LOCAL_ID_TYPE_GROUP && lid->type != LOCAL_ID_TYPE_TAGGED_GROUP)
        return;

    if (!lid->group)
        lid->group = list_new();
    LIST_LOOP(lid->group, ptag, node)
    {
        if (*ptag == tag)
            return;
    }

    ptag = XMALLOC(MTYPE_TMP, 2);
    *ptag = tag;
    listnode_add(lid->group, ptag);
}

void local_id_group_delete(struct local_id *lid, u_int16_t  tag)
{
    u_int16_t * ptag = NULL;
    listnode node;
    assert(lid);
    if (lid->type != LOCAL_ID_TYPE_GROUP && lid->type != LOCAL_ID_TYPE_TAGGED_GROUP)
        return;

    if (!lid->group)
        return;

    LIST_LOOP(lid->group, ptag, node)
    {
        if (*ptag == tag)
        {
            listnode_delete(lid->group, ptag);
            XFREE(MTYPE_TMP, ptag);
            if (lid->group->count == 0)
            {
                list_free(lid->group);
                lid->group = NULL;
            }
            return;
        }
    }

}
 
void local_id_group_free(struct local_id *lid)
{
    u_int16_t * ptag = NULL;
    listnode node;
    assert(lid);
    if (lid->type != LOCAL_ID_TYPE_GROUP && lid->type != LOCAL_ID_TYPE_TAGGED_GROUP)
        return;

    LIST_LOOP(lid->group, ptag, node)
    {
        XFREE(MTYPE_TMP, ptag);
    }

    list_delete(lid->group);
    lid->group = NULL;
}

void local_id_group_show(struct vty *vty, struct local_id *lid)
{
    u_int16_t * ptag = NULL;
    listnode node;
    assert(lid);
    if (lid->type != LOCAL_ID_TYPE_GROUP && lid->type != LOCAL_ID_TYPE_TAGGED_GROUP)
        return;

    vty_out(vty, "%-7d [%-12s] ", lid->value, lid_types[lid->type]);

    LIST_LOOP(lid->group, ptag, node)
    {
        vty_out(vty, "  %d(%s)", *ptag, get_switch_port_string(*ptag));
    }
    vty_out(vty, "%s", VTY_NEWLINE);
}

/*Fiona's addion in support of query in XML format*/
void
process_xml_query(FILE* fp, char* src)
{
  struct listnode *node;
  struct lsp *lsp = NULL;
  char *localid[] = {"lsp-id", "port", "tagged-group", "group"};

  if (fp == NULL)
    return;
 
  LIST_LOOP(dmaster.dragon_lsp_table, lsp, node) {

    fprintf(fp, "<resource>\n");
    fprintf(fp, "<src>%s</src>\n", src);
    fprintf(fp, "<ast_status>AST_SUCCESS</ast_status>\n");
    fprintf(fp, "<lsp_name>%s</lsp_name>\n", (lsp->common.SessionAttribute_Para)->sessionName);    

    fprintf(fp, "<src_ip>%s</src_ip>\n", inet_ntoa(lsp->common.Session_Para.srcAddr));
    fprintf(fp, "<dest_ip>%s</dest_ip>\n", inet_ntoa(lsp->common.Session_Para.destAddr));
    fprintf(fp, "<src_local_id_type>%s</src_local_id_type>\n", localid[lsp->dragon.srcLocalId>>16] );
    fprintf(fp, "<src_local_id>%d</src_local_id>\n", lsp->common.Session_Para.srcPort);
    fprintf(fp, "<dest_local_id_type>%s</dest_local_id_type>\n", localid[lsp->dragon.srcLocalId>>16]);
    fprintf(fp, "<dest_local_id>%d</dest_local_id>\n", lsp->common.Session_Para.destPort);
    fprintf(fp, "<lsp_status>%s</lsp_status>\n", value_to_string(&conv_lsp_status, lsp->status));

    fprintf(fp, "<bandwidth>%s</bandwidth>\n", value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->R));
    fprintf(fp, "<swcap>%s</swcap>\n",value_to_string (&conv_swcap, lsp->common.LabelRequest_Para.data.gmpls.switchingType) );
    fprintf(fp, "<gpid>%s</gpid>\n", value_to_string (&conv_gpid, lsp->common.LabelRequest_Para.data.gmpls.gPid) );
    fprintf(fp, "<encoding>%s</encoding>\n", value_to_string (&conv_encoding, lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType) );
    if (lsp->dragon.lspVtag == 0) 
      fprintf(fp, "<vtag>any</vtag>\n");
    else 
      fprintf(fp, "<vtag>%d</vtag>\n", lsp->dragon.lspVtag);
    fprintf(fp, "</resource>\n");
  } 
}


/* Making connection to protocol daemon. */
int
dragon_module_connect (struct dragon_module *module)
{
  int val, ret=0;
  int sock;
  struct sockaddr_in addr;
  int flags, old_flags;
  fd_set sset;
  struct timeval tv;
  socklen_t lon;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
      return -1;

  flags = old_flags = fcntl(sock, F_GETFL, 0);
#if defined(O_NONBLOCK)
    flags |= O_NONBLOCK;
#elif defined(O_NDELAY)
    flags |= O_NDELAY;
#elif defined(FNDELAY)
    flags |= FNDELAY;
#endif

  if (fcntl(sock, F_SETFL, flags) == -1) {
      return -1;
  }

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = module->ip_addr.s_addr;
  addr.sin_port = htons(module->port);

  ret = connect (sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
  if(ret < 0) {
     if (errno == EINPROGRESS) { 
         tv.tv_sec = 1;
         tv.tv_usec = 0;
         FD_ZERO(&sset);
         FD_SET(sock, &sset);
         if(select(sock+1, NULL, &sset, NULL, &tv) > 0) {
             lon = sizeof(int);
             getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&val), &lon); 
             return ((val != 0)?(-1):0);
         }
         else {
             return -1;
         }
     }
     else {
         /*!EINPROGRESS */
         return -1;
     }
 }

 if (fcntl(sock, F_SETFL, old_flags) == -1) {
     return -1;
 }
  close (sock);

  return ret;
}


/* Execute command in child process, forked from vtysh.c */
int
dragon_execute_command (char *command, int argc, char *arg1, char *arg2, int fd)
{
  int ret;
  pid_t pid;
  int status;

  /* Call fork(). */
  pid = fork ();

  if (pid < 0)
    {
      /* Failure of fork(). */
      fprintf (stderr, "Can't fork: %s\n", strerror (errno));
      return (-1);
    }
  else if (pid == 0)
    {
      /* This is child process. */

      /* First, we need to re-direct stdin, stdout, stderr to the current vty */
      dup2(fd, 0);  /* stdin */
      dup2(fd, 1); /* stdout */
      dup2(fd, 2); /* stderr */
      switch (argc)
	{
	case 0:
	  ret = execlp (command, command, NULL);
	  break;
	case 1:
	  ret = execlp (command, command, arg1, NULL);
	  break;
	case 2:
	  ret = execlp (command, command, arg1, arg2, NULL);
	  break;
	}
    }
  else
    {
      /* This is parent. */
      ret = wait4 (pid, &status, 0, NULL);
    }
  return 0;
}


DEFUN (dragon_show_version,
       dragon_show_version_cmd,
       "show version",
       "Show Version\n"
       "Revision and Last Change Date\n"
       )
{
  vty_out(vty, "%s%s", dragon_version_string(), VTY_NEWLINE);
  return CMD_SUCCESS;
}

DEFUN (dragon_set_pce_para,
       dragon_set_pce_para_cmd,
       "configure pce ip-address A.B.C.D port <0-65535>",
       "Set IP address and port of supported software modules\n"
       "PCE (default : localhost/2610)\n"
       "IP address\n"
	"IP address, where A, B, C, and D are integers 0 to 255\n"
       "port\n"
       "Port number\n"
       )
{
  struct in_addr ip;

  if (! inet_aton (argv[0], &ip))
  {
    vty_out (vty, "Please specify IP address by A.B.C.D%s", VTY_NEWLINE);
    return CMD_WARNING;
  }
  dmaster.module[MODULE_PCE].ip_addr.s_addr = ip.s_addr;

  if (argc>1)
	  dmaster.module[MODULE_PCE].port = atoi(argv[1]);
  
  return CMD_SUCCESS;
}

ALIAS (dragon_set_pce_para,
       dragon_set_pce_para_ip_cmd,
       "configure pce ip-address A.B.C.D",
       "Set IP address and port of supported software modules\n"
       "PCE (default : localhost/2610)\n"
       "IP address\n"
	"IP address, where A, B, C, and D are integers 0 to 255\n");

DEFUN (dragon_set_narb_para,
       dragon_set_narb_para_cmd,
       "configure narb (intra-domain|inter-domain) ip-address A.B.C.D port <0-65535>",
       "Set IP address and port of supported software modules\n"
       "NARB\n"
	"NARB Intra-domain (default: localhost/2609)\n"
	"NARB Inter-domain (default: localhost/0)\n"
       "IP address\n"
	"IP address, where A, B, C, and D are integers 0 to 255\n"
       "port\n"
       "Port number\n"
       )
{
  static int m;
  struct in_addr ip;

  if (strncmp (argv[0], "intr", 4) == 0)
	    m = MODULE_NARB_INTRA;
  else if (strncmp (argv[0], "inte", 4) == 0)
	    m = MODULE_NARB_INTER;

  if (! inet_aton (argv[1], &ip))
  {
    vty_out (vty, "Please specify IP address by A.B.C.D%s", VTY_NEWLINE);
    return CMD_WARNING;
  }
  /* NARB intra-domain IP address == NARB inter-domain IP address */
  dmaster.module[MODULE_NARB_INTRA].ip_addr.s_addr = ip.s_addr;
  dmaster.module[MODULE_NARB_INTER].ip_addr.s_addr = ip.s_addr;

  if (argc>2)
	  dmaster.module[m].port = atoi(argv[2]);
  
  return CMD_SUCCESS;
}

ALIAS (dragon_set_narb_para,
       dragon_set_narb_para_ip_cmd,
       "configure narb (intra-domain|inter-domain) ip-address A.B.C.D",
       "Configure NARB parameters\n"
       "NARB\n"
	"NARB Intra-domain (default: localhost/2609)\n"
	"NARB Inter-domain (default: localhost/0)\n"
       "IP address\n"
	"IP address, where A, B, C, and D are integers 0 to 255\n");

int override_narb_ero_forced = 0;

DEFUN (dragon_set_narb_extra_options,
       dragon_set_narb_extra_options_cmd,
       "set narb-extra-options (use-movaz-special|override-narb-ero|query-with-holding|query-with-confirmation|query-subnet-ero|query-subnet-dtl|query-alt-paths|exclude-layer1|exclude-layer2|exclude-tdm|exclude-layer3|none)",
       "Set NARB extra options\n"
       "NARB options\n"
       "Instructing NARB to compute a path using Movaz proprietary information\n"
       "Forcing overriding NARB ERO w/ user supplied ERO\n"
       "Query and hold the resources being queried\n"
       "Query that requires confirmation ID\n"
       "Query subnet ERO\n"
       "Query subnet DTL\n"
       "Query alternate paths (w/ DCN PCE module)\n"
       "Routing-layer exclusion\n"
       "Routing-layer exclusion\n"
       "Routing-layer exclusion\n"
       "Routing-layer exclusion\n"
       "Clear\n"
       )
{
  if (strncmp (argv[0], "use-movaz-special", 12) == 0)
	narb_extra_options |= LSP_OPT_VIA_MOVAZ;
  else if (strncmp (argv[0], "query-with-holding", 15) == 0)
	narb_extra_options |= LSP_OPT_QUERY_HOLD;
  else if (strncmp (argv[0], "query-with-confirmation", 15) == 0)
	narb_extra_options |= LSP_OPT_QUERY_CONFIRM;
  else if (strncmp (argv[0], "query-subnet-ero", 15) == 0)
	narb_extra_options |= LSP_OPT_SUBNET_ERO;
  else if (strncmp (argv[0], "query-subnet-dtl", 15) == 0)
	narb_extra_options |= LSP_OPT_SUBNET_DTL;
  else if (strncmp(argv[0], "query-alt-paths", 14) == 0)
	narb_extra_options |= LSP_OPT_ALT_PATHS;
  else if (strncmp (argv[0], "exclude-layer1", 14) == 0)
	narb_extra_options |= LSP_OPT_EXCLUD_L1;
  else if (strncmp (argv[0], "exclude-layer2", 14) == 0)
	narb_extra_options |= LSP_OPT_EXCLUD_L2;
  else if (strncmp (argv[0], "exclude-tdm", 11) == 0)
	narb_extra_options |= LSP_OPT_EXCLUD_TDM;
  else if (strncmp (argv[0], "exclude-layer3", 14) == 0)
	narb_extra_options |= LSP_OPT_EXCLUD_L3;
  else if (strncmp (argv[0], "override-narb-ero", 12) == 0)
	override_narb_ero_forced = 1;
  else if (strncmp (argv[0], "none", 4) == 0)
  {
	narb_extra_options = 0;
	override_narb_ero_forced = 0;
  }
  return CMD_SUCCESS;
}

DEFUN (dragon_show_narb_extra_options,
       dragon_show_narb_extra_options_cmd,
       "show narb-extra-options",
       "Show NARB extra options\n"
       "NARB options\n"
       )
{
  if ((narb_extra_options & LSP_OPT_VIA_MOVAZ) != 0)
  	vty_out(vty, "    >use-movaz-special%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_QUERY_HOLD) != 0)
  	vty_out(vty, "    >query-with-holding%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_QUERY_CONFIRM) != 0)
  	vty_out(vty, "    >query-with-confirmation%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_SUBNET_ERO) != 0)
  	vty_out(vty, "    >query-subnet-ero%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_SUBNET_DTL) != 0)
  	vty_out(vty, "    >query-subnet-dtl%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_ALT_PATHS) != 0)
  	vty_out(vty, "    >query-alt-paths%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_EXCLUD_L1) != 0)
  	vty_out(vty, "    >exclude-layer1%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_EXCLUD_TDM) != 0)
  	vty_out(vty, "    >exclude-tdm%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_EXCLUD_L2) != 0)
  	vty_out(vty, "    >exclude-layer2%s", VTY_NEWLINE);
  if ((narb_extra_options & LSP_OPT_EXCLUD_L3) != 0)
  	vty_out(vty, "    >exclude-layer3%s", VTY_NEWLINE);
  if (override_narb_ero_forced == 1)
  	vty_out(vty, "    >override-narb-ero%s", VTY_NEWLINE);
  return CMD_SUCCESS;
}

DEFUN (dragon_telnet_module,
       dragon_telnet_module_cmd,
       "telnet (ospf|rsvp|astdl|pce)",
       "Telnet to software modules\n"
       "Telnet to OSPF-TE\n"
       "Telnet to RSVP\n"
       "Telnet to ASTDL\n"
       "Telnet to NARB\n"
       "Telnet to PCE\n")
{
  static int m;
  char buf[] = "telnet 255.255.255.255 65535";
  
  if (strncmp (argv[0], "o", 1) == 0)
	m = MODULE_OSPF;
  else if (strncmp (argv[0], "r", 1) == 0)
	m = MODULE_RSVP;
  else if (strncmp (argv[0], "a", 1) == 0)
	m = MODULE_ASTDL;
  else if (strncmp (argv[0], "intr", 4) == 0)
	m = MODULE_NARB_INTRA;
  else if (strncmp (argv[0], "inte", 4) == 0)
	m = MODULE_NARB_INTER;
  else if (strncmp (argv[0], "p", 1) == 0)
	m = MODULE_PCE;
  else
  {
      vty_out (vty, "Unrecognized module %s %s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
  }
  sprintf (buf, "%d", dmaster.module[m].port);
  if (dmaster.module[m].ip_addr.s_addr==0 || dmaster.module[m].port==0)
  {
	  vty_out (vty, "Module address / port not configured %s %s", argv[0], VTY_NEWLINE);
	  return CMD_WARNING;
  }
  dragon_execute_command ("telnet", 2, inet_ntoa(dmaster.module[m].ip_addr), buf, vty->fd);
  return CMD_SUCCESS;
}

ALIAS (dragon_telnet_module,
       dragon_telnet_module_narb_cmd,
       "telnet narb (intra-domain|inter-domain)",
       "Telnet to software modules\n"
       "Telnet to NARB\n"
       "Telnet to NARB intra-domain module\n"
       "Telnet to NARB inter-domain module\n");

DEFUN (dragon_show_module_status,
       dragon_show_module_status_cmd,
       "show module",
       SHOW_STR
       "Show status of software modules\n")
{
  int m;
  int connected;
  const char* RSVPD_PID_FILE = "var/run/RSVPD.pid";
  int rsvpd_pid;
  FILE *fd;
  pid_t pid;
  
  /* print header */
  vty_out (vty, "         **Module status summary** %s%s", VTY_NEWLINE, VTY_NEWLINE);
  vty_out (vty, "Module      IP               Port   Status%s", VTY_NEWLINE);
  vty_out (vty, "-------------------------------------------%s", VTY_NEWLINE);
  for (m = MODULE_OSPF; m <= MODULE_PCE; m++)
  {
	vty_out (vty, "%-12s", dmaster.module[m].name);
	if (dmaster.module[m].ip_addr.s_addr == 0 || dmaster.module[m].ip_addr.s_addr==0x100007f)
	  vty_out (vty, "localhost        ");
	else
	  vty_out (vty, "%-17s", inet_ntoa(dmaster.module[m].ip_addr));
	vty_out (vty, "%-7d", dmaster.module[m].port);
	if (m==MODULE_ASTDL || m==MODULE_PCE)
  		vty_out (vty, "n/a%s", VTY_NEWLINE);
	else if (m==MODULE_RSVP){
		if (!(fd = fopen(RSVPD_PID_FILE, "r")))
	  	    vty_out (vty, "offline%s", VTY_NEWLINE);
		else{
		    fscanf(fd, "%d", &rsvpd_pid);
		    if ((pid = getsid(rsvpd_pid))>0)
	  	        vty_out (vty, "online%s", VTY_NEWLINE);
		    else
	  	        vty_out (vty, "online%s", VTY_NEWLINE);
		    fclose(fd);
		}
	}
	else if (dmaster.module[m].ip_addr.s_addr==0 || dmaster.module[m].port==0)
	{
		vty_out (vty, "n/a%s", VTY_NEWLINE);
	}
  	else{ 
  	  connected = dragon_module_connect(&dmaster.module[m]);
	  if (connected == 0)
  	    vty_out (vty, "online%s", VTY_NEWLINE);
	  else
  	    vty_out (vty, "offline%s", VTY_NEWLINE);
	}
  }
  return CMD_SUCCESS;
}


void
lsp_del(struct lsp *lsp)
{
     if (lsp->common.SessionAttribute_Para){
     	     if (lsp->common.SessionAttribute_Para->sessionName)
		     XFREE(MTYPE_OSPF_DRAGON, lsp->common.SessionAttribute_Para->sessionName);
     	     XFREE(MTYPE_OSPF_DRAGON, lsp->common.SessionAttribute_Para);
     }
     if (lsp->common.ADSpec_Para)
     	XFREE(MTYPE_OSPF_DRAGON, lsp->common.ADSpec_Para);
     if (lsp->common.GenericTSpec_Para)
     	XFREE(MTYPE_OSPF_DRAGON, lsp->common.GenericTSpec_Para);
     if (lsp->common.SonetTSpec_Para)
     	XFREE(MTYPE_OSPF_DRAGON, lsp->common.SonetTSpec_Para);
     if (lsp->common.EROAbstractNode_Para)
     	XFREE(MTYPE_OSPF_DRAGON, lsp->common.EROAbstractNode_Para);
     if (lsp->common.labelSet)
     	XFREE(MTYPE_OSPF_DRAGON, lsp->common.labelSet);
     if (lsp->common.upstreamLabel)
     	XFREE(MTYPE_OSPF_DRAGON, lsp->common.upstreamLabel);
     if (lsp->common.Protection_Para)
     	XFREE(MTYPE_OSPF_DRAGON, lsp->common.Protection_Para);
     
     XFREE (MTYPE_OSPF_DRAGON, lsp);
}

void 
set_lsp_default_para (struct lsp *lsp)
{
	const char* ssName = "Unknown";

	lsp->common.LabelRequest_Para.labelType = LABEL_GENERALIZED;
	lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType = LINK_IFSWCAP_SUBTLV_ENC_ETH;
	lsp->common.LabelRequest_Para.data.gmpls.switchingType = LINK_IFSWCAP_SUBTLV_SWCAP_LSC;
	lsp->common.LabelRequest_Para.data.gmpls.gPid = G_Lamda;

	lsp->common.GenericTSpec_Para = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct _GenericTSpec_Para));
	(lsp->common.GenericTSpec_Para)->R = LSP_BW_Gig_E_OverFiber;
	(lsp->common.GenericTSpec_Para)->B = LSP_BW_Gig_E_OverFiber;
	(lsp->common.GenericTSpec_Para)->P = LSP_BW_Gig_E_OverFiber;
	(lsp->common.GenericTSpec_Para)->m = 100;
	(lsp->common.GenericTSpec_Para)->M = 1500;

	lsp->common.SessionAttribute_Para = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct _SessionAttribute_Para));
	(lsp->common.SessionAttribute_Para)->excludeAny = 0;
	(lsp->common.SessionAttribute_Para)->includeAny = 0;
	(lsp->common.SessionAttribute_Para)->includeAll = 0;
	(lsp->common.SessionAttribute_Para)->setupPri = 7;
	(lsp->common.SessionAttribute_Para)->holdingPri = 7;
	(lsp->common.SessionAttribute_Para)->flags = 0;
	(lsp->common.SessionAttribute_Para)->sessionName = XMALLOC(MTYPE_OSPF_DRAGON, MAX_LSP_NAME_LENGTH);
	strncpy((lsp->common.SessionAttribute_Para)->sessionName, ssName, strlen(ssName));
	(lsp->common.SessionAttribute_Para)->nameLength = strlen(ssName);
}

DEFUN (dragon_edit_lsp,
       dragon_edit_lsp_cmd,
       "edit lsp NAME",
       "Edit a GMPLS label switched path (LSP)\n"
       "Label switched path\n"
       "LSP name, maximum length is 64 characters\n"
       )
{
  struct listnode *node;
  struct lsp *lsp = NULL;
  int found;

  if (strlen(argv[0])>=MAX_LSP_NAME_LENGTH)
  {
	vty_out (vty, "The name is too long. Maximum name length is %d%s", MAX_LSP_NAME_LENGTH, VTY_NEWLINE);
	return CMD_WARNING;
  }
  found = 0;
  LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
  {
  	if (strcmp((lsp->common.SessionAttribute_Para)->sessionName, argv[0])==0)
  	{
  		if (lsp->status != LSP_EDIT)
  		{
			vty_out (vty, "The LSP %s is not in edit state. Please delete it first%s", argv[0], VTY_NEWLINE);
			return CMD_WARNING;
  		}
		else
		{
			found = 1;
			/*$$$$ clear ERO*/
			if (lsp->dragon.ero != NULL)
			{
				struct _EROAbstractNode_Para *hop;
				listnode node1;
				LIST_LOOP(lsp->dragon.ero, hop, node1)
				{
					free(hop);
				}
				list_delete_all_node(lsp->dragon.ero);
			}
			/*$$$$ clear DTL*/
			if (lsp->dragon.subnet_dtl != NULL)
			{
				struct dtl_hop *hop;
				listnode node2;
				LIST_LOOP(lsp->dragon.subnet_dtl, hop, node2)
				{
					free(hop);
				}
				list_delete_all_node(lsp->dragon.subnet_dtl);
			}
			/*$$$$ clear subnet ERO*/
			if (lsp->dragon.subnet_ero != NULL)
			{
				struct _EROAbstractNode_Para *hop;
				listnode node3;
				LIST_LOOP(lsp->dragon.subnet_ero, hop, node3)
				{
					free(hop);
				}
				list_delete_all_node(lsp->dragon.subnet_ero);
			}
			break;
		}
  	}
  }
  if (!found)
  {
  	lsp = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct lsp));
	memset(lsp, 0, sizeof(struct lsp));
	set_lsp_default_para(lsp);
	lsp->status = LSP_EDIT;
	strcpy((lsp->common.SessionAttribute_Para)->sessionName, argv[0]);
	(lsp->common.SessionAttribute_Para)->nameLength = strlen(argv[0]);
	listnode_add(dmaster.dragon_lsp_table, lsp);	
  }
  vty->node = LSP_NODE;
  strcpy(lsp_prompt,"%s(edit-lsp-");
  strcat(lsp_prompt,argv[0]);
  strcat(lsp_prompt,")# ");
  vty->index = lsp;

  return CMD_SUCCESS;
}

DEFUN (dragon_set_lsp_name,
       dragon_set_lsp_name_cmd,
       "set name NAME",
       "Set LSP parameters\n"
	"LSP name, maximum length is 64 characters\n"
       )
{
  struct lsp *lsp = (struct lsp *)(vty->index);
  u_int8_t find = 0;
  listnode node;
  struct lsp *l;

  if (strlen(argv[0])>=MAX_LSP_NAME_LENGTH)
  {
	vty_out (vty, "The name is too long. Maximum name length is %d%s", MAX_LSP_NAME_LENGTH, VTY_NEWLINE);
	return CMD_WARNING;
  }
  /* Check if there is another LSP using the same name */
  if (dmaster.dragon_lsp_table)
	  LIST_LOOP(dmaster.dragon_lsp_table, l , node)
	  {
		if (lsp != l && strcmp((l->common.SessionAttribute_Para)->sessionName, argv[0])==0)
		{
			find = 1;
		}
	  }
  if (find)
  {
  	vty_out(vty, "Another LSP with the same name already exists.\n");
  	zlog_info("Another LSP with the same name already exists.");
	return CMD_WARNING;
  }
  if (strlen(argv[0]))
  {
    strcpy((lsp->common.SessionAttribute_Para)->sessionName, argv[0]);
    (lsp->common.SessionAttribute_Para)->nameLength = strlen(argv[0]);
    strcpy(lsp_prompt,"%s(edit-lsp-");
    strcat(lsp_prompt,argv[0]);
    strcat(lsp_prompt,")# ");
  }
  return CMD_SUCCESS;
}

DEFUN (dragon_set_lsp_dir,
       dragon_set_lsp_dir_cmd,
       "set direction (bi|uni) upstream-label <1-4294967295> ",
	"Set LSP parameters\n"
       "Set LSP direction\n"
       "Bi-directional LSP\n"
       "Uni-directional LSP\n"
       "Upstream label value\n")
{
  struct lsp *lsp = (struct lsp *)(vty->index);
  int up_label;

  if (strncmp(argv[0], "u", 1)==0)
  {
	  lsp->flag &= (~LSP_FLAG_BIDIR);
	  if (lsp->common.upstreamLabel)
		 XFREE(MTYPE_OSPF_DRAGON, lsp->common.upstreamLabel);
	  lsp->common.upstreamLabel = NULL;
  }
  else if (strncmp(argv[0], "b", 1)==0)
  {
	  lsp->flag |= LSP_FLAG_BIDIR;
	  if (sscanf (argv[1], "%d", &up_label) != 1)
	  {
		  vty_out (vty, "Invalid upstream label: %s%s", strerror (errno), VTY_NEWLINE);
		  return CMD_WARNING;
	  }
	  if (lsp->common.upstreamLabel)
		 XFREE(MTYPE_OSPF_DRAGON, lsp->common.upstreamLabel);
	  lsp->common.upstreamLabel = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(u_int32_t));
	  *(lsp->common.upstreamLabel) = (u_int32_t)up_label;
  }
  return CMD_SUCCESS;
}

DEFUN (dragon_set_label_set,
       dragon_set_label_set_cmd,
       "set label-set <1-4294967295> ",
       "Set label set value\n"
       "Include the value into the label set\n")
{
  struct lsp *lsp = (struct lsp *)(vty->index);
  int label;
  
  if (sscanf (argv[0], "%d", &label) != 1)
  {
	  vty_out (vty, "Invalid label: %s%s", strerror (errno), VTY_NEWLINE);
	  return CMD_WARNING;
  }
  if (!lsp->common.labelSet)
  {
	 lsp->common.labelSet = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(u_int32_t)*MAX_LABEL_SET_SIZE);
	 lsp->common.labelSetSize = 0;
  }
  if (lsp->common.labelSetSize < MAX_LABEL_SET_SIZE)
  {
	  lsp->common.labelSet[lsp->common.labelSetSize] = (u_int32_t)label;
	  lsp->common.labelSetSize++;
  }

  return CMD_SUCCESS;
}


DEFUN (dragon_set_lsp_ip,
       dragon_set_lsp_ip_cmd,
       "set source ip-address A.B.C.D (port|group|tagged-group|subnet-interface|lsp-id) ID destination ip-address A.B.C.D  (port|group|tagged-group|subnet-interface|tunnel-id) ID",
       "Set LSP parameters\n"
       "Source and destination nodes\n"
       "Source node IP address\n"
       "IP address, where A, B, C, and D are integers 0 to 255\n"
       "port LocalID (must be registered at source)\n"
       "group LocalID (must be registered at source)\n"
       "tagged-group LocalID (must be registered at source)\n"
       "subnet-interface LocalID (must be registered at source)\n"
       "lsp-id\n"
       "LSP ID, integer between 1 and 65535, or any (for localID)\n"
       "Destination node IP address\n"
       "Destination node IP address\n"
       "IP address, where A, B, C, and D are integers 0 to 255\n"
       "port LocalID (must be registered at destination)\n"
       "group LocalID (must be registered at destination)\n"
       "tagged-group LocalID (must be registered at destination)\n"
       "subnet-interface LocalID (must be registered at destination)\n"
       "tunnel-id\n"
       "Tunnel ID, integer between 1 and 65535, or any (for localID)\n"
       )
{
    struct lsp *lsp = (struct lsp *)(vty->index);
    struct in_addr ip_src, ip_dst;
    u_int16_t type_src, type_dest;
    int port_src, port_dest;
    struct local_id * lid = NULL;
    listnode node;
    
    inet_aton(argv[0], &ip_src);
    if (strcmp(argv[1], "port") == 0)
        type_src = LOCAL_ID_TYPE_PORT;
    else if (strcmp(argv[1], "group") == 0)
        type_src = LOCAL_ID_TYPE_GROUP;
    else if (strcmp(argv[1], "tagged-group") == 0)
        type_src = LOCAL_ID_TYPE_TAGGED_GROUP;
    else if (strcmp(argv[1], "subnet-interface") == 0)
        type_src = LOCAL_ID_TYPE_SUBNET_IF_ID;
    else
        type_src = LOCAL_ID_TYPE_NONE;
  
    if (lsp->common.DragonUni_Para && type_src == LOCAL_ID_TYPE_TAGGED_GROUP && strcasecmp(argv[2], "any") == 0)
    {
        port_src = ANY_VTAG;
    }
    else if (type_src != LOCAL_ID_TYPE_NONE && strcasecmp(argv[2], "null") == 0)
    {
        type_src = LOCAL_ID_TYPE_TAGGED_GROUP;
        port_src = 0;
    }
    else if (sscanf (argv[2], "%d", &port_src) != 1)
    {
        vty_out (vty, "Invalid source port: %s%s", argv[2], VTY_NEWLINE);
        return CMD_WARNING;
    }

    /*check type_src /port_src against registered_local_ids*/
    if (!(type_src == LOCAL_ID_TYPE_TAGGED_GROUP && port_src == 0)) /*<--- exclude the special case for Tagged-Group-ANY */
    {
        LIST_LOOP(registered_local_ids, lid, node)
        {
            if (lid->type == type_src && lid->value == port_src)
                break;
        }    
        if (node == NULL && type_src != LOCAL_ID_TYPE_NONE && port_src != ANY_VTAG)
        {
            vty_out (vty, "Unregistered source local-id (type %s): %s.%s",  argv[1], argv[2], VTY_NEWLINE);
            return CMD_WARNING;
        }
    }

    inet_aton(argv[3], &ip_dst);
    if (strcmp(argv[4], "port") == 0)
        type_dest = LOCAL_ID_TYPE_PORT;
    else if (strcmp(argv[4], "group") == 0)
        type_dest = LOCAL_ID_TYPE_GROUP;
    else if (strcmp(argv[4], "tagged-group") == 0)
        type_dest = LOCAL_ID_TYPE_TAGGED_GROUP;
    else if (strcmp(argv[4], "subnet-interface") == 0)
        type_dest = LOCAL_ID_TYPE_SUBNET_IF_ID;
    else
        type_dest = LOCAL_ID_TYPE_NONE;

    /*special handling for subnet-interface at destination*/
    /*	Relaxed --> Obsolete code
    if ((type_src == LOCAL_ID_TYPE_SUBNET_IF_ID || type_dest == LOCAL_ID_TYPE_SUBNET_IF_ID) && type_src != type_dest )
    {
        vty_out(vty, "\t >>Warning>> Only one of the ingress and egress local IDs is subnet-interface!%s", VTY_NEWLINE);
    }
    */
  
    if (lsp->common.DragonUni_Para && type_dest== LOCAL_ID_TYPE_TAGGED_GROUP && strcasecmp(argv[5], "any") == 0)
    {
        port_dest = ANY_VTAG;
    }
    else if (type_dest != LOCAL_ID_TYPE_NONE && strcasecmp(argv[5], "null") == 0)
    {
        type_dest = LOCAL_ID_TYPE_TAGGED_GROUP;
        port_dest = 0;
    }
    else if (sscanf (argv[5], "%d", &port_dest) != 1)
    {
        vty_out (vty, "Invalid destination port: %s%s", argv[5], VTY_NEWLINE);
        return CMD_WARNING;
    }

    /*check type_dest /port_dest against registered_local_ids for the src-dest local-id co-located case */
    if (ip_src.s_addr == ip_dst.s_addr && ip_src.s_addr !=0 &&  !(type_dest == LOCAL_ID_TYPE_TAGGED_GROUP && port_dest == 0))
    {
        LIST_LOOP(registered_local_ids, lid, node)
        {
            if (lid->type == type_dest && lid->value == port_dest)
                break;
        }    
        if (node == NULL && type_dest != LOCAL_ID_TYPE_NONE && port_dest != ANY_VTAG)
        {
            vty_out (vty, "Unregistered destination local-id (type %s): %s.%s",  argv[3], argv[4], VTY_NEWLINE);
            return CMD_WARNING;
        }
    }
  
    /*finished validation and writes the inputs to lsp data structure*/
    lsp->common.Session_Para.srcAddr.s_addr = ip_src.s_addr;
    /* ramdomize source port (lsp-id) */
    if (type_src== LOCAL_ID_TYPE_TAGGED_GROUP && port_src == ANY_VTAG)
        lsp->common.Session_Para.srcPort = (u_int16_t)random();
    else if (type_src== LOCAL_ID_TYPE_SUBNET_IF_ID && (port_src & 0xff) == ANY_TIMESLOT)
        lsp->common.Session_Para.srcPort = (u_int16_t)((port_src & 0xff00) + random()%255);
    else
        lsp->common.Session_Para.srcPort = (u_int16_t)port_src;
    lsp->common.Session_Para.destAddr.s_addr = ip_dst.s_addr;
    /* ramdomize detination port (tunnel-id) */
    if (type_dest== LOCAL_ID_TYPE_TAGGED_GROUP && port_dest == ANY_VTAG)
        lsp->common.Session_Para.destPort = (u_int16_t)random();
    else if (type_dest== LOCAL_ID_TYPE_SUBNET_IF_ID && (port_dest & 0xff) == ANY_TIMESLOT)
        lsp->common.Session_Para.destPort = (u_int16_t)((port_dest & 0xff00) + random()%255);
    else
        lsp->common.Session_Para.destPort = (u_int16_t)port_dest;
    lsp->dragon.srcLocalId = ((u_int32_t)type_src)<<16 |port_src;
    lsp->dragon.destLocalId = ((u_int32_t)type_dest)<<16 |port_dest;

    if ( (type_src != LOCAL_ID_TYPE_TAGGED_GROUP || port_src != 0) &&
		(type_dest != LOCAL_ID_TYPE_TAGGED_GROUP || port_dest != 0) )
    {
        if (type_src  == LOCAL_ID_TYPE_TAGGED_GROUP 
            && type_dest == LOCAL_ID_TYPE_TAGGED_GROUP
            && port_src != port_dest)
        {
            vty_out(vty, "###Ingress Tag (%d) and Egress Tag (%d) do not match!%s", 
                port_src, port_dest, VTY_NEWLINE);
            return CMD_WARNING;
        }

        if (type_src == LOCAL_ID_TYPE_TAGGED_GROUP)
            lsp->dragon.lspVtag = port_src;
        else  if (type_dest  == LOCAL_ID_TYPE_TAGGED_GROUP)
            lsp->dragon.lspVtag = port_dest;
    }
    
    if (lsp->common.DragonUni_Para) {
  	lsp->common.DragonUni_Para->srcLocalId = lsp->dragon.srcLocalId;
  	lsp->common.DragonUni_Para->destLocalId = lsp->dragon.destLocalId;
       if (lsp->dragon.lspVtag != 0)
           lsp->common.DragonUni_Para->vlanTag = lsp->dragon.lspVtag;
    }

    return CMD_SUCCESS;
}

DEFUN (dragon_set_lsp_uni,
       dragon_set_lsp_uni_cmd,
       "set uni (client|network|none) ingress NAME egress NAME",
       "Set LSP Mode as originated from UNI Client\n"
       "UNI mode\n"
       "Ingress control channel\n"
       "NAME\n"
       "Egress control channel\n"
       "NAME\n"
	)
{
    struct lsp *lsp = (struct lsp *)(vty->index);
    if (strcmp(argv[0], "client") == 0)
	    lsp->uni_mode = UNI_CLIENT;
    else if (strcmp(argv[0], "network") == 0)
	    lsp->uni_mode = UNI_NETWORK;
    else 
	    lsp->uni_mode = UNI_NONE;
    lsp->common.DragonUni_Para = XMALLOC(MTYPE_TMP, sizeof(struct _Dragon_Uni_Para));
    memset(lsp->common.DragonUni_Para, 0, sizeof(struct _Dragon_Uni_Para));
    if (argc <2)
        strcpy(lsp->common.DragonUni_Para->ingressChannel, "implicit");
    else
        strncpy(lsp->common.DragonUni_Para->ingressChannel, argv[1], 12);
    if (argc <3)
        strcpy(lsp->common.DragonUni_Para->egressChannel, "implicit");
    else
        strncpy(lsp->common.DragonUni_Para->egressChannel, argv[2], 12);

    if (strncmp(lsp->common.DragonUni_Para->ingressChannel, "implicit", 8) ==0 && lsp->dragon.srcLocalId != 0)
        lsp->common.DragonUni_Para->srcLocalId = lsp->dragon.srcLocalId;
    if (strncmp(lsp->common.DragonUni_Para->egressChannel, "implicit", 8) ==0 && lsp->dragon.destLocalId != 0)
        lsp->common.DragonUni_Para->destLocalId = lsp->dragon.srcLocalId;
    if (lsp->dragon.lspVtag != 0)
        lsp->common.DragonUni_Para->vlanTag = lsp->dragon.lspVtag;
		
    return CMD_SUCCESS;
}

ALIAS (dragon_set_lsp_uni,
       dragon_set_lsp_uni_implicit_cmd,
       "set uni (client|network|none)",
       "Set LSP Mode as originated from UNI Client\n"
       "UNI mode\n"
	);

DEFUN (dragon_set_lsp_vtag,
       dragon_set_lsp_vtag_cmd,
       "set vtag <1-4095>",
       "Set LSP VLAN Tag\n"
       "VLAN Tag from end to end\n"
       "Tag value\n")
{
    struct lsp *lsp = (struct lsp *)(vty->index);
    u_int32_t vtag = atoi(argv[0]);
    
    if ((lsp->dragon.srcLocalId >> 16)  == LOCAL_ID_TYPE_TAGGED_GROUP
        && vtag != (lsp->dragon.srcLocalId & 0xffff) && (lsp->dragon.srcLocalId & 0xffff) != 0)
    {
        vty_out(vty, "###Ingress Tag (%d) do not match LSP VLAN Tag!%s", 
            (lsp->dragon.srcLocalId & 0xffff), VTY_NEWLINE);
        return CMD_WARNING;
    }
    if ((lsp->dragon.destLocalId>> 16)  == LOCAL_ID_TYPE_TAGGED_GROUP
        && vtag != (lsp->dragon.destLocalId & 0xffff) && (lsp->dragon.destLocalId & 0xffff) != 0)
    {
        vty_out(vty, "###Egress Tag (%d) do not match LSP VLAN Tag!%s", 
            (lsp->dragon.destLocalId & 0xffff), VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    lsp->dragon.lspVtag = vtag;

    /* Mandate a VLAN via DragonExtInfo::edgeVlanMapping subobject for (1) subnet-interface local-id provisioning,
        or (2) source-destination colocated local-id provisioning*/
    if ( ((lsp->dragon.srcLocalId>> 16)  == LOCAL_ID_TYPE_SUBNET_IF_ID || (lsp->dragon.destLocalId>> 16)  == LOCAL_ID_TYPE_SUBNET_IF_ID)
	|| (lsp->common.Session_Para.srcAddr.s_addr == lsp->common.Session_Para.destAddr.s_addr && lsp->common.Session_Para.srcAddr.s_addr != 0
		&& lsp->dragon.srcLocalId>>16 != LOCAL_ID_TYPE_NONE && lsp->dragon.destLocalId>>16 != LOCAL_ID_TYPE_NONE) )
    {
        if (lsp->common.DragonExtInfo_Para == NULL)
        {
            lsp->common.DragonExtInfo_Para = XMALLOC(MTYPE_TMP, sizeof(struct _Dragon_ExtInfo_Para));
            memset(lsp->common.DragonExtInfo_Para, 0, sizeof(struct _Dragon_ExtInfo_Para));
            lsp->common.DragonExtInfo_Para->ingress_vtag = lsp->common.DragonExtInfo_Para->egress_vtag = ANY_VTAG;
        }
        lsp->common.DragonExtInfo_Para->ingress_vtag = lsp->common.DragonExtInfo_Para->egress_vtag = vtag;
	 lsp->common.DragonExtInfo_Para->flags |= EXT_INFO_FLAG_SUBNET_EDGE_VLAN;
    }

    if (lsp->common.DragonUni_Para)
	lsp->common.DragonUni_Para->vlanTag = vtag;

    return CMD_SUCCESS;
}

DEFUN (dragon_set_lsp_vtag_default,
       dragon_set_lsp_vtag_default_cmd,
       "set vtag",
       "Set LSP VLAN Tag\n"
       "VLAN Tag from end to end; To be computed\n")
{
    struct lsp *lsp = (struct lsp *)(vty->index);

    if ((lsp->dragon.srcLocalId >> 16)  == LOCAL_ID_TYPE_TAGGED_GROUP && (lsp->dragon.srcLocalId & 0xffff) != 0)
        lsp->dragon.lspVtag = (lsp->dragon.srcLocalId & 0xffff);
    else if ((lsp->dragon.destLocalId >> 16)  == LOCAL_ID_TYPE_TAGGED_GROUP && (lsp->dragon.destLocalId & 0xffff) != 0)
        lsp->dragon.lspVtag = (lsp->dragon.destLocalId & 0xffff);
    else
        lsp->dragon.lspVtag = ANY_VTAG;

    /*
    if (((lsp->dragon.srcLocalId>> 16)  == LOCAL_ID_TYPE_SUBNET_IF_ID || 
        (lsp->dragon.destLocalId>> 16)  == LOCAL_ID_TYPE_SUBNET_IF_ID) && lsp->dragon.lspVtag == ANY_VTAG)
    {
        vty_out(vty, "###LocalID subnet-interface cannot be used with 'vtag any.' Must give a specific vlan in (1-4095)!%s", VTY_NEWLINE);
        lsp->dragon.lspVtag = 0;
        return CMD_WARNING;
    }
    */

    /* Mandate a VLAN via DragonExtInfo::edgeVlanMapping subobject for source-destination colocated local-id provisioning*/
    /* Moved to dragon_narb_topo_rsp_proc (in support of VTAG ANY
    if ( lsp->common.Session_Para.srcAddr.s_addr == lsp->common.Session_Para.destAddr.s_addr
		&& lsp->common.Session_Para.srcAddr.s_addr != 0 && lsp->dragon.srcLocalId>>16 != LOCAL_ID_TYPE_NONE
		&& lsp->dragon.destLocalId>>16 != LOCAL_ID_TYPE_NONE && lsp->dragon.lspVtag != 0 && lsp->dragon.lspVtag != ANY_VTAG)
    {
        if (lsp->common.DragonExtInfo_Para == NULL)
        {
            lsp->common.DragonExtInfo_Para = XMALLOC(MTYPE_TMP, sizeof(struct _Dragon_ExtInfo_Para));
            memset(lsp->common.DragonExtInfo_Para, 0, sizeof(struct _Dragon_ExtInfo_Para));
        }
        lsp->common.DragonExtInfo_Para->ingress_vtag = lsp->common.DragonExtInfo_Para->egress_vtag = lsp->dragon.lspVtag;
    }
    */
    if (lsp->common.DragonUni_Para)
	lsp->common.DragonUni_Para->vlanTag = lsp->dragon.lspVtag;

    return CMD_SUCCESS;
}

ALIAS (dragon_set_lsp_vtag_default,
       dragon_set_lsp_vtag_any_cmd,
       "set vtag any",
       "Set LSP VLAN Tag\n"
       "VLAN Tag from end to end\n"
       "Any Vtag to be computed\n");

DEFUN (dragon_set_lsp_vtag_subnet_edge,
       dragon_set_lsp_vtag_subnet_edge_cmd,
       "set vtag (subnet-ingress|subnet-egress) TAG",
       "Set LSP VLAN Tag\n"
       "VLAN Tag\n"
       "Subnet-Ingress\n"
       "Subnet-Egress\n"
       "'untagged' or vtag#\n")
{
    int is_ingress = 0;
    int edge_vtag = ANY_VTAG; /* ANY_VTAG: tunnel mode */
    struct lsp *lsp = (struct lsp *)(vty->index);

    if (strncmp(argv[0], "subnet-ingress", 8) == 0)
    {
        is_ingress = 1;
    }

    if (strncmp(argv[1], "untagged", 2) == 0)
    {
        edge_vtag = 0; /* 0: untagged*/
    }
    else 
    {
	sscanf(argv[1], "%d", &edge_vtag); /* tagged; tunnel mode if sccanf fails */
    }

    if (lsp->common.DragonExtInfo_Para == NULL)
    {
        lsp->common.DragonExtInfo_Para = XMALLOC(MTYPE_TMP, sizeof(struct _Dragon_ExtInfo_Para));
        memset(lsp->common.DragonExtInfo_Para, 0, sizeof(struct _Dragon_ExtInfo_Para));
        lsp->common.DragonExtInfo_Para->ingress_vtag = lsp->common.DragonExtInfo_Para->egress_vtag = ANY_VTAG;
    }
    if (is_ingress)
    {
        lsp->common.DragonExtInfo_Para->ingress_vtag = (u_int16_t)edge_vtag;
    }
    else
    {
        lsp->common.DragonExtInfo_Para->egress_vtag = (u_int16_t)edge_vtag;
    }

    lsp->common.DragonExtInfo_Para->flags |= EXT_INFO_FLAG_SUBNET_EDGE_VLAN;

    /* lsp->common.DragonExtInfo_Para->trunk_vtag is determined by 'set vtag TAG' command. 
        The actual tag assignment happens with NARB replies a path with a specific vtag */

    return CMD_SUCCESS;
}

/*
DEFUN (dragon_set_lsp_ext_info_edge_vtag,
       dragon_enable_lsp_ext_info_edge_vtag_cmd,
       "set edge ingress-vtag <1-4095> egress-vtag <1-4095>",
       "Enable LSP using DragonExtInfo_Para to carry edge vtag information\n")
{
    u_int32_t ingress_vtag = atoi(argv[0]);
    u_int32_t egress_vtag = ingress_vtag;
    struct lsp *lsp = (struct lsp *)(vty->index);
    if (argc >= 2)
        egress_vtag = atoi(argv[1]);

    if (lsp->common.DragonExtInfo_Para == NULL)
    {
        lsp->common.DragonExtInfo_Para = XMALLOC(MTYPE_TMP, sizeof(struct _Dragon_ExtInfo_Para));
        memset(lsp->common.DragonExtInfo_Para, 0, sizeof(struct _Dragon_ExtInfo_Para));
    }
    lsp->common.DragonExtInfo_Para->ingress_vtag = (u_int16_t)ingress_vtag;
    lsp->common.DragonExtInfo_Para->egress_vtag = (u_int16_t)egress_vtag;

    return CMD_SUCCESS;
}

ALIAS (dragon_set_lsp_ext_info_edge_vtag,
       dragon_enable_lsp_ext_info_edge_vtag_default_cmd,
       "set edge vtag <1-4095>",
       "Enable LSP using DragonExtInfo_Para to carry edge vtag information\n");
*/

DEFUN (dragon_set_lsp_sw,
       dragon_set_lsp_sw_cmd,
       "set bandwidth NAME swcap (psc1|l2sc|lsc|tdm) encoding (packet|ethernet|lambda|sdh) gpid (lambda|ethernet|sdh)",
       "Set LSP parameters\n"
       "Bandwidth\n"
       "Bandwidth name: eth100M, eth149.3M, eth2.4G, gige_f, hdtv, etc\n"
       "Switching capability\n"
       "Packet-Switch Capable-1 (PSC-1)\n"
       "Layer-2 Switch Capable (L2SC)\n"
       "Lambda-Switch Capable (LSC)\n"
       "Time-Division-Multiplex Capable (TDM)\n"
       "LSP Encoding Type\n"
       "Packet\n"
       "Ethernet\n"
       "Lambda (photonic)\n"
       "SDH ITU-T G.707 / SONET ANSI T1.105\n"
       "Generalized PID (G-PID)\n"
       "Lambda\n"
       "Ethernet\n"
       "SONET/SDH\n"
       )
{
  struct lsp *lsp = (struct lsp *)(vty->index);
  u_int32_t bandwidth;
  u_int8_t swcap, encoding;
  u_int16_t gpid;
  float float_bw;
  char char_mg;

  if (strcasecmp(argv[0], "zero") == 0)
  {
      bandwidth = 0;
  }
  else if (sscanf(argv[0], "eth%f%c", &float_bw, &char_mg) == 2 || sscanf(argv[0], "Eth%f%c", &float_bw, &char_mg) == 2 || sscanf(argv[0], "ETH%f%c", &float_bw, &char_mg) == 2)
  {
      if (float_bw == 0 || (char_mg != 'm' && char_mg != 'M' && char_mg != 'g' && char_mg != 'G'))
      {
          vty_out (vty, "###ERROR: unsupported bandwidth : %s %s", argv[0], VTY_NEWLINE);
          return CMD_WARNING;
      }
      if (char_mg == 'g' || char_mg == 'G')
      {
          float_bw *= 1000.0;
      }
      if (float_bw > 10000.0)
      {
          vty_out (vty, "###ERROR: unsupported bandwidth : %s (bandwith over eth10G or eth10000M)%s", argv[0], VTY_NEWLINE);
          return CMD_WARNING;
      }	  
      float_bw = float_bw*1000000/8;
      bandwidth = *(u_int32_t*)&float_bw;
  }
  else
  {
      bandwidth = string_to_value(&conv_bandwidth, argv[0]);
      if (bandwidth==0)
      {
          vty_out (vty, "###ERROR: unsupported bandwidth: %s %s", argv[0], VTY_NEWLINE);
          return CMD_WARNING;
      }
  }

  swcap = string_to_value(&conv_swcap, argv[1]);
  if (swcap==0)
  {
      vty_out (vty, "###ERROR: unsupported switching capability: %s %s", argv[1], VTY_NEWLINE);
      return CMD_WARNING;
  }
  /* Layer2 LSPs should always be bi-directional */
  if (swcap == LINK_IFSWCAP_SUBTLV_SWCAP_L2SC)
  {
      lsp->flag |= LSP_FLAG_BIDIR;
  }

  encoding = string_to_value(&conv_encoding, argv[2]);
  if (encoding==0)
  {
      vty_out (vty, "###ERROR: unsupported encoding type: %s %s", argv[2], VTY_NEWLINE);
      return CMD_WARNING;
  }
  gpid = string_to_value(&conv_gpid, argv[3]);
  if (gpid==0)
  {
      vty_out (vty, "###ERROR: unsupported gpid: %s %s", argv[3], VTY_NEWLINE);
      return CMD_WARNING;
  }

  (lsp->common.GenericTSpec_Para)->R = bandwidth;
  (lsp->common.GenericTSpec_Para)->B = bandwidth;
  (lsp->common.GenericTSpec_Para)->P = bandwidth;

  lsp->common.LabelRequest_Para.labelType = LABEL_GENERALIZED;
  lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType = encoding;
  lsp->common.LabelRequest_Para.data.gmpls.switchingType = swcap;
  lsp->common.LabelRequest_Para.data.gmpls.gPid = gpid;

  return CMD_SUCCESS;
}


DEFUN (dragon_set_lsp_ero_hop,
       dragon_set_lsp_ero_hop_cmd,
       "set ero-hop (strict|loose) ip-address A.B.C.D interface-id ID",
       "Add an ero-hop to the ERO list\n"
       "ERO hop\n"
       "Strict hop\n"
       "Loose hop\n"
       "IP address\n"
       "IPv4n"
       "Interface ID\n"
       "ID\n"
	)
{
  struct _EROAbstractNode_Para *hop;
  struct in_addr ip;
  u_int32_t if_id;

  struct lsp *lsp = (struct lsp *)(vty->index);
  if (lsp->dragon.subnet_ero != NULL && listcount(lsp->dragon.subnet_ero) > 0)
  {
      vty_out(vty, "###User supplied subnet-DTL cannot be used together with subnet-ERO!%s", VTY_NEWLINE);
      return CMD_WARNING;
  }
  if (lsp->dragon.ero == NULL)
  {
  	lsp->dragon.ero = list_new();
  }

  hop = XMALLOC(MTYPE_TMP, sizeof(struct _EROAbstractNode_Para));
  memset(hop, 0, sizeof(struct _EROAbstractNode_Para));
  if (strcmp(argv[0], "loose") == 0)
	hop->isLoose = 1;
  inet_aton(argv[1], &ip);
  if (argc < 3 || sscanf(argv[2], "%d", &if_id) != 1 || if_id == 0)
  {
  	hop->type = IPv4;
	hop->data.ip4.addr.s_addr = ip.s_addr;
	hop->data.ip4.prefix = 32;
  }
  else
  {
  	hop->type = UNumIfID;
	hop->data.uNumIfID.routerID.s_addr = ip.s_addr;
	hop->data.uNumIfID.interfaceID = if_id;
  }

  listnode_add(lsp->dragon.ero, hop);
  return CMD_SUCCESS;
}

ALIAS (dragon_set_lsp_ero_hop,
       dragon_set_lsp_ero_hop_ipv4_cmd,
       "set ero-hop-ipv4 (loose|strict) ip-address A.B.C.D",
       "Add an ero-hop to the ERO list\n"
       "IPv4 ERO hop\n"
       "Strict hop\n"
       "Loose hop\n"
       "IP address\n"
       "IPv4\n")

DEFUN (dragon_set_lsp_dtl_hop,
       dragon_set_lsp_dtl_hop_cmd,
       "set dtl-hop node NAME link <1-4294967295>",
       "Add an dlt-hop to the DLT list\n"
       "DTL hop\n"
       "Node name\n"
       "NAME\n"
       "Link ID number\n"
       "NUMBER\n"
	)
{
  struct lsp *lsp = (struct lsp *)(vty->index);
  struct dtl_hop *hop;
  if (lsp->dragon.subnet_dtl == NULL)
  {
  	lsp->dragon.subnet_dtl = list_new();
  }
  hop = XMALLOC(MTYPE_TMP, sizeof(struct dtl_hop));
  strncpy((char*)hop->nodename, argv[0], MAX_DTL_NODENAME_LEN);
  sscanf(argv[1], "%d", &hop->linkid);
  listnode_add(lsp->dragon.subnet_dtl, hop);
  return CMD_SUCCESS;
}

DEFUN (dragon_set_lsp_subnet_ero_hop,
       dragon_set_lsp_subnet_ero_hop_cmd,
       "set subnet-ero-hop ip-address A.B.C.D interface-id ID",
       "Add a subnet-ero-hop to the SubnetERO list\n"
       "SubnetERO hop\n"
       "IP address\n"
       "IPv4n"
       "Interface ID\n"
       "ID\n"
	)
{
  struct _EROAbstractNode_Para *hop;
  struct in_addr ip;
  u_int32_t if_id;

  struct lsp *lsp = (struct lsp *)(vty->index);
  if (lsp->dragon.subnet_dtl != NULL && listcount(lsp->dragon.subnet_dtl) > 0)
  {
      vty_out(vty, "###User supplied subnet-ERO cannot be used together with subnet-DTL!%s", VTY_NEWLINE);
      return CMD_WARNING;
  }  	
  if (lsp->dragon.subnet_ero == NULL)
  {
  	lsp->dragon.subnet_ero = list_new();
  }

  hop = XMALLOC(MTYPE_TMP, sizeof(struct _EROAbstractNode_Para));
  memset(hop, 0, sizeof(struct _EROAbstractNode_Para));
  hop->isLoose = 0; //subnet hop must be 'strict'
  inet_aton(argv[0], &ip);
  if (argc < 2 || sscanf("%d", argv[1], &if_id) != 1 || if_id == 0)
  {
  	hop->type = IPv4;
	hop->data.ip4.addr.s_addr = ip.s_addr;
	hop->data.ip4.prefix = 32;
  }
  else
  {
  	hop->type = UNumIfID;
	hop->data.uNumIfID.routerID.s_addr = ip.s_addr;
	hop->data.uNumIfID.interfaceID = if_id;
  }

  listnode_add(lsp->dragon.subnet_ero, hop);
  return CMD_SUCCESS;
}

ALIAS (dragon_set_lsp_subnet_ero_hop,
       dragon_set_lsp_subnet_ero_hop_ipv4_cmd,
       "set subnet-ero-hop-ipv4 ip-address A.B.C.D",
       "Add a subnet-ero-hop to the ERO list\n"
       "SunbetERO hop\n"
       "IP address\n"
       "IPv4\n")


DEFUN (dragon_commit_lsp_sender,
       dragon_commit_lsp_sender_cmd,
       "commit lsp NAME sender",
       "Commit a GMPLS label switched path (LSP)\n"
       "Label switched path\n"
       "LSP name\n"
       "This node is an RSVP sender\n")
{
  struct listnode *node;
  struct lsp *lsp = NULL, *lsp2 = NULL;
  struct dragon_fifo_elt *new;
  int found;
  
  found = 0;
  LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
  {
  	if (strcmp((lsp->common.SessionAttribute_Para)->sessionName, argv[0])==0)
  	{
  		if (lsp->status != LSP_EDIT)
  		{
			vty_out (vty, "The LSP %s is not in edit state. %s", argv[0], VTY_NEWLINE);
			return CMD_WARNING;
  		}
		else
		{
			found = 1;
			break;
		}
  	}
  }
  if (!found)
  {
  	vty_out (vty, "Unable to find LSP %s. %s", argv[0], VTY_NEWLINE);
	return CMD_WARNING;
  }
  if (!is_mandated_params_set_for_lsp(lsp))
  {
  	vty_out (vty, "Mandated parameter not set for lsp (address or port cannot be zero) %s. %s", argv[0], VTY_NEWLINE);
  	vty_out (vty, "LSP \"%s\" could not be committed... %s", (lsp->common.SessionAttribute_Para)->sessionName,  VTY_NEWLINE);
	return CMD_WARNING;
  }

  /* Check if there is another non-subnet-interface LSP with the same session parameter */
  if ((lsp->dragon.destLocalId >> 16) != LOCAL_ID_TYPE_SUBNET_IF_ID && dmaster.dragon_lsp_table)
      LIST_LOOP(dmaster.dragon_lsp_table, lsp2 , node)
      {
          if (lsp!=lsp2 && LSP_SAME_SESSION(lsp, lsp2))
          {
              vty_out(vty, "Cannot commit: another non-subnet-interface LSP with the same session parameters already exists!");
              zlog_info("Cannot commit: another non-subnet-interface LSP with the same session parameters already exists!");
              return CMD_WARNING;
          }
      }

  if ((lsp->dragon.srcLocalId >> 16)  == LOCAL_ID_TYPE_TAGGED_GROUP 
      && (lsp->dragon.srcLocalId & 0xffff) != lsp->dragon.lspVtag 
      && lsp->dragon.lspVtag  != ANY_VTAG)
  {
      vty_out(vty, "###Ingress port tag (%d) does not match the LSP Vtag (%d)!%s", 
          (lsp->dragon.srcLocalId & 0xffff), lsp->dragon.lspVtag, VTY_NEWLINE);
  	vty_out (vty, "LSP \"%s\" could not be committed... %s", (lsp->common.SessionAttribute_Para)->sessionName,  VTY_NEWLINE);
      return CMD_WARNING;
  }
  
  if ((lsp->dragon.destLocalId >> 16)  == LOCAL_ID_TYPE_TAGGED_GROUP 
      && (lsp->dragon.destLocalId & 0xffff) != lsp->dragon.lspVtag
      && lsp->dragon.lspVtag  != ANY_VTAG)
  {
 	vty_out(vty, "###Egress port tag (%d) does not match the LSP Vtag (%d)!%s", 
          (lsp->dragon.destLocalId & 0xffff), lsp->dragon.lspVtag, VTY_NEWLINE);
  	vty_out (vty, "LSP \"%s\" could not be committed... %s", (lsp->common.SessionAttribute_Para)->sessionName,  VTY_NEWLINE);
      return CMD_WARNING;
  }

  /*$$$$ Special handling for DCN Subnet-DTL*/
  /* Note: With the user supplied DTL in lsp->dragon.subnet_dtl, NARB will not send back a DTL TLV. */
  if (lsp->dragon.subnet_dtl != NULL && listcount(lsp->dragon.subnet_dtl) > 0)
  {
  	int i = 0;
	struct dtl_hop *hop;
	listnode node2;
	if (lsp->common.DragonExtInfo_Para == NULL)
	{
		lsp->common.DragonExtInfo_Para = XMALLOC(MTYPE_TMP, sizeof(struct _Dragon_ExtInfo_Para));
		memset(lsp->common.DragonExtInfo_Para, 0, sizeof(struct _Dragon_ExtInfo_Para));
       	lsp->common.DragonExtInfo_Para->ingress_vtag = lsp->common.DragonExtInfo_Para->egress_vtag = ANY_VTAG;
	}
	lsp->common.DragonExtInfo_Para->num_subnet_dtl_hops = listcount(lsp->dragon.subnet_dtl);
	lsp->common.DragonExtInfo_Para->subnet_dtl_hops = XMALLOC(MTYPE_TMP, sizeof(struct dtl_hop)*lsp->common.DragonExtInfo_Para->num_subnet_dtl_hops);
	LIST_LOOP(lsp->dragon.subnet_dtl, hop, node2)
	{
		memcpy(lsp->common.DragonExtInfo_Para->subnet_dtl_hops+i, hop, sizeof(struct dtl_hop));
		i++;
	}
	lsp->common.DragonExtInfo_Para->flags |= EXT_INFO_FLAG_SUBNET_DTL;
  }

  zlog_info("Committing LSP %s (sender/source node)", (lsp->common.SessionAttribute_Para)->sessionName);
  
  if (lsp->common.Session_Para.srcAddr.s_addr == lsp->common.Session_Para.destAddr.s_addr && lsp->common.Session_Para.srcAddr.s_addr != 0
  	&& !((lsp->dragon.srcLocalId >> 16) == LOCAL_ID_TYPE_SUBNET_IF_ID || (lsp->dragon.destLocalId >> 16) == LOCAL_ID_TYPE_SUBNET_IF_ID))
  {
  	  struct _EROAbstractNode_Para *srcLocalId, *destLocalId;
	  /* NARB is not required for source and destination co-located local ID provisioning, unless one of the local-ids is subnet-interface */
	  if (lsp->dragon.srcLocalId>>16 == LOCAL_ID_TYPE_NONE || lsp->dragon.destLocalId>>16 == LOCAL_ID_TYPE_NONE)
	  {
		  vty_out (vty, "### Both source and destation must use true local ID for source and destination co-located provisioning.%s", VTY_NEWLINE);
		  return CMD_WARNING;
	  }
	  if ( lsp->uni_mode == 1 )
	  {
		  vty_out (vty, "### UNI mode is not supported in source and destination co-located provisioning.%s", VTY_NEWLINE);
		  return CMD_WARNING;          
	  }
	  /* create and add localId subobjects to ERO*/
	  lsp->common.ERONodeNumber = 2;
	  lsp->common.EROAbstractNode_Para = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct _EROAbstractNode_Para)*2);
	  memset(lsp->common.EROAbstractNode_Para, 0, sizeof(struct _EROAbstractNode_Para)*2);
	  srcLocalId = lsp->common.EROAbstractNode_Para;
	  srcLocalId->type = UNumIfID;
	  srcLocalId->isLoose = 1;
	  memcpy(&srcLocalId->data.uNumIfID.routerID, &lsp->common.Session_Para.srcAddr, sizeof(struct in_addr));
	  srcLocalId->data.uNumIfID.interfaceID = lsp->dragon.srcLocalId;
	  srcLocalId->isLoose = 0;
	  destLocalId = lsp->common.EROAbstractNode_Para+1;
	  destLocalId->type = UNumIfID;
	  destLocalId->isLoose = 1;
	  memcpy(&destLocalId->data.uNumIfID.routerID, &lsp->common.Session_Para.destAddr, sizeof(struct in_addr));
	  destLocalId->data.uNumIfID.interfaceID = lsp->dragon.destLocalId;
	  destLocalId->isLoose = 0;
	  /* call RSVPD to set up the path */
	  zlog_info("Initiating RSVP path request for LSP %s (source and destination co-located provisioning mode)",
		    (lsp->common.SessionAttribute_Para)->sessionName);
	  zInitRsvpPathRequest(dmaster.api, &lsp->common, 1);
  }
  else if (dmaster.module[MODULE_NARB_INTRA].ip_addr.s_addr == 0 || dmaster.module[MODULE_NARB_INTRA].port==0)
  {
  	  /* NARB address/port is not configured, try sending PATH requests without ERO */
         if ((lsp->dragon.srcLocalId>>16 != LOCAL_ID_TYPE_NONE || lsp->dragon.destLocalId>>16 != LOCAL_ID_TYPE_NONE)
		 	&& lsp->uni_mode != 1 )
         {
          	vty_out (vty, "NARB is required to setup LSP with localId.%s", VTY_NEWLINE);
	  	vty_out (vty, "LSP \"%s\" could not be committed... %s", (lsp->common.SessionAttribute_Para)->sessionName,  VTY_NEWLINE);
        	return CMD_WARNING;
         }
	  /* call RSVPD to set up the path */
	  zlog_info("Initiating RSVP path request for LSP %s (no NARB address/port configured, sending PATH without ERO)",
		    (lsp->common.SessionAttribute_Para)->sessionName);
	  zInitRsvpPathRequest(dmaster.api, &lsp->common, 1);
  }
  else{
	  /* CLI/ASTDL --> NARB TCP socket initalization */
	  lsp->narb_fd = dragon_narb_socket_init();
	  if (lsp->narb_fd < 0) 
	  {
	  	lsp->narb_fd = 0;
          	vty_out (vty, "Failed to connect to the NARB server.%s", VTY_NEWLINE);
	  	vty_out (vty, "LSP \"%s\" could not be committed... %s", (lsp->common.SessionAttribute_Para)->sessionName,  VTY_NEWLINE);
	  	return CMD_WARNING;
	  }
	  /* Assign a unique sequence number */
	  lsp->seqno = dragon_assign_seqno();
	  
	  /* Construct topology create message */
	  new = dragon_topology_create_msg_new(lsp);

	  /* Put packet into fifo */
	  dragon_fifo_push(dmaster.dragon_packet_fifo, new);

	  /* Start LSP refresh timer */
	  DRAGON_TIMER_OFF (lsp->t_lsp_refresh);
	  DRAGON_TIMER_ON (lsp->t_lsp_refresh, dragon_lsp_refresh_timer, lsp, DRAGON_LSP_REFRESH_INTERVAL);

	  /* Write packet to socket */
          zlog_info("Sending topology create message to NARB");
	  DRAGON_WRITE_ON(dmaster.t_write, NULL, lsp->narb_fd);
  }
  /* Set commit flag */
  lsp->status = LSP_COMMIT;
  
  return CMD_SUCCESS;
}

ALIAS (dragon_commit_lsp_sender,
       dragon_commit_lsp_default_cmd,
       "commit lsp NAME",
       "Commit a GMPLS label switched path (LSP)\n"
       "Label switched path\n"
       "LSP name\n");

DEFUN (dragon_commit_lsp_receiver,
       dragon_commit_lsp_receiver_cmd,
       "commit lsp NAME receiver",
       "Commit a GMPLS label switched path (LSP)\n"
       "Label switched path\n"
       "LSP name\n"
       "This node is an RSVP receiver\n")
{
  struct listnode *node;
  struct lsp *lsp = NULL;
  int found;
  
  found = 0;
  LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
  {
  	if (strcmp((lsp->common.SessionAttribute_Para)->sessionName, argv[0])==0)
  	{
  		if (lsp->status != LSP_EDIT)
  		{
			vty_out (vty, "The LSP %s is not in edit state. %s", argv[0], VTY_NEWLINE);
			return CMD_WARNING;
  		}
		else
		{
			found = 1;
			break;
		}
  	}
  }
  if (!found)
  {
  	vty_out (vty, "Unable to find LSP %s. %s", argv[0], VTY_NEWLINE);
	return CMD_WARNING;
  }
  if (!is_mandated_params_set_for_lsp(lsp))
  {
  	vty_out (vty, "Mandated parameter not set for lsp %s. %s", argv[0], VTY_NEWLINE);
	return CMD_WARNING;
  }

  zlog_info("Committing LSP %s (receiver/destination node)", (lsp->common.SessionAttribute_Para)->sessionName);

  /* call RSVPD to set up the path */
  zInitRsvpPathRequest(dmaster.api, &lsp->common, 0);

  /* Set commit flag */
  lsp->status = LSP_LISTEN;
  lsp->flag |= LSP_FLAG_RECEIVER;
  
  return CMD_SUCCESS;
}


DEFUN (dragon_delete_lsp,
       dragon_delete_lsp_cmd,
       "delete lsp NAME",
       "Delete an existing LSP\n"
       "Label switched path\n"
       "LSP name\n")
{
  struct listnode *node;
  struct lsp *lsp = NULL;
  struct dragon_fifo_elt *new;
  int found;
  
  found = 0;
  if (dmaster.dragon_lsp_table)
  {
	  LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
  	{
  		if (strcmp((lsp->common.SessionAttribute_Para)->sessionName, argv[0])==0)
  		{
			found = 1;
			break;
	  	}
 	 }
  }
  if (!found)
  {
  	vty_out (vty, "No matching LSP named %s. %s", argv[0], VTY_NEWLINE);
	return CMD_WARNING;
  }

   /* if the LSP is in state COMMIT or IS then we need to notify the NARB 
        if the LSP is in state LISTEN then we need to unregister from RSVP */
  if ((lsp->status == LSP_COMMIT || lsp->status == LSP_IS || lsp->status == LSP_ERROR) && 
  	(!(lsp->flag & LSP_FLAG_RECEIVER)) && (!(lsp->flag & LSP_FLAG_REG_BY_RSVP)) && lsp->narb_fd>0)
  {
	/* Construct topology remove message */
	new = dragon_topology_remove_msg_new(lsp);

	/* Put packet into fifo */
	dragon_fifo_push(dmaster.dragon_packet_fifo, new);

	/* Write packet to socket */
	zlog_info("Sending topology removal message to NARB");
	DRAGON_WRITE_ON(dmaster.t_write, NULL, lsp->narb_fd);
	/* Set DELETE flag */
	lsp->status = LSP_DELETE;
  }
  else if ((lsp->status == LSP_IS || lsp->status == LSP_ERROR) && (lsp->flag & LSP_FLAG_RECEIVER))
  {
        zlog_info("Initiating RSVP path tear request for LSP %s",
		  (lsp->common.SessionAttribute_Para)->sessionName);
	zTearRsvpPathRequest(dmaster.api, &lsp->common);
	lsp->status = LSP_LISTEN;  	
  }
  else{
	DRAGON_TIMER_OFF(lsp->t_lsp_refresh);
        zlog_info("Initiating RSVP path tear request for LSP %s",
		  (lsp->common.SessionAttribute_Para)->sessionName);
	zTearRsvpPathRequest(dmaster.api, &lsp->common);
	listnode_delete(dmaster.dragon_lsp_table, lsp);
	lsp_recycle(lsp);
	//lsp_del(lsp);
  }

  return CMD_SUCCESS;
}

void 
dragon_show_lsp_detail(struct lsp *lsp, struct vty* vty)
{
	char temp1[20], temp2[20]; /* Added to avoid a C optimization (?) inet_ntoa problem */
	if (vty)
	{
		strcpy(temp1, inet_ntoa(lsp->common.Session_Para.srcAddr));
		strcpy(temp2, inet_ntoa(lsp->common.Session_Para.destAddr));
		vty_out(vty, "Src %s/%d, dest %s/%d %s", temp1, 
					lsp->common.Session_Para.srcPort,temp2, 
					lsp->common.Session_Para.destPort, VTY_NEWLINE);
		if (lsp->common.DragonExtInfo_Para && (lsp->common.DragonExtInfo_Para->flags&EXT_INFO_FLAG_CONFIRMATION_ID) != 0) {
			vty_out(vty, "GRI: %u-%u%s", 
				lsp->common.DragonExtInfo_Para->ucid, lsp->common.DragonExtInfo_Para->seqnum, VTY_NEWLINE);
		}
		if (lsp->common.GenericTSpec_Para)
		{
			vty_out(vty, "Generic TSPEC R=%s, B=%s, P=%s, m=%d, M=%d %s", 
					  bandwidth_value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->R),
					  bandwidth_value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->B),
					  bandwidth_value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->P),
					  (lsp->common.GenericTSpec_Para)->m,
					  (lsp->common.GenericTSpec_Para)->M, VTY_NEWLINE);
		}
		else if (lsp->common.SonetTSpec_Para)
		{
			vty_out(vty, "SONET TSPEC ST=%d, RCC=%d, NCC=%d, NVC=%d, MT=%d, T=%d, P=%d %s", 
					  (lsp->common.SonetTSpec_Para)->Sonet_ST,
					  (lsp->common.SonetTSpec_Para)->Sonet_RCC,
					  (lsp->common.SonetTSpec_Para)->Sonet_NCC,
					  (lsp->common.SonetTSpec_Para)->Sonet_NVC,
					  (lsp->common.SonetTSpec_Para)->Sonet_MT,
					  (lsp->common.SonetTSpec_Para)->Sonet_T,
					  (lsp->common.SonetTSpec_Para)->Sonet_P, VTY_NEWLINE);
		}
		vty_out(vty, "Encoding %s, Switching %s, G-Pid %s %s", 
				    value_to_string (&conv_encoding, lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType),
				    value_to_string (&conv_swcap, lsp->common.LabelRequest_Para.data.gmpls.switchingType),
				    value_to_string (&conv_gpid, lsp->common.LabelRequest_Para.data.gmpls.gPid), VTY_NEWLINE);

              if (lsp->dragon.srcLocalId)
        	    vty_out(vty, "Ingress Local ID Type: %s, Value: %d %s", 
			    lid_types[lsp->dragon.srcLocalId>>16], lsp->dragon.srcLocalId&0xffff, VTY_NEWLINE);
              else
        	    vty_out(vty, "No ingress Local ID configured. %s", VTY_NEWLINE);
              
              if (lsp->dragon.destLocalId)
        	    vty_out(vty, "Egress Local ID Type: %s, Value: %d. %s", 
			    lid_types[lsp->dragon.destLocalId>>16], lsp->dragon.destLocalId&0xffff, VTY_NEWLINE);
              else
        	    vty_out(vty, "No egress Local ID configured. %s", VTY_NEWLINE);

              if (lsp->dragon.lspVtag)
        	    vty_out(vty, "E2E LSP VLAN Tag: %d. %s", lsp->dragon.lspVtag, VTY_NEWLINE);
              else {
                if (lsp->flag & LSP_FLAG_RECEIVER)
                    vty_out(vty, "LSP untagged at destination. %s", VTY_NEWLINE);
                else
                    vty_out(vty, "No E2E LSP VLAN Tag configured. %s", VTY_NEWLINE);
              }

              if (lsp->common.DragonExtInfo_Para && (lsp->common.DragonExtInfo_Para->flags&EXT_INFO_FLAG_SUBNET_EDGE_VLAN) != 0)
              {
                  if (lsp->common.DragonExtInfo_Para->ingress_vtag != lsp->dragon.lspVtag && lsp->common.DragonExtInfo_Para->ingress_vtag == 0)
                      vty_out(vty, "Subnet Ingress VLAN: untagged %s", VTY_NEWLINE);
                  else if (lsp->common.DragonExtInfo_Para->ingress_vtag != lsp->dragon.lspVtag 
		      && lsp->common.DragonExtInfo_Para->ingress_vtag > 0 
		      && lsp->common.DragonExtInfo_Para->ingress_vtag < 4096)
                      vty_out(vty, "Subnet Ingress VLAN: tag-%d %s", lsp->common.DragonExtInfo_Para->ingress_vtag, VTY_NEWLINE);
                  else if (lsp->common.DragonExtInfo_Para->ingress_vtag == ANY_VTAG)
                      vty_out(vty, "Subnet Ingress VLAN: tunnel-mode %s", VTY_NEWLINE);

                  if (lsp->common.DragonExtInfo_Para->egress_vtag != lsp->dragon.lspVtag && lsp->common.DragonExtInfo_Para->egress_vtag == 0)
                      vty_out(vty, "Subnet Egress VLAN: untagged %s", VTY_NEWLINE);
                  else if (lsp->common.DragonExtInfo_Para->egress_vtag != lsp->dragon.lspVtag 
		      && lsp->common.DragonExtInfo_Para->egress_vtag > 0 
		      && lsp->common.DragonExtInfo_Para->egress_vtag < 4096)
                      vty_out(vty, "Subnet Egress VLAN: tag-%d %s", lsp->common.DragonExtInfo_Para->egress_vtag, VTY_NEWLINE);
                  else if (lsp->common.DragonExtInfo_Para->egress_vtag == ANY_VTAG)
                      vty_out(vty, "Subnet Egress VLAN: tunnel-mode %s", VTY_NEWLINE);
              }

              vty_out(vty, "Status: %s %s", value_to_string(&conv_lsp_status, lsp->status), VTY_NEWLINE);
		if (lsp->error_spec.errCode != 0)
		{
			vty_out(vty, "Error Spec: node=%s, flags=%d, code=%d, value=%d %s", inet_ntoa(lsp->error_spec.nodeAddress), lsp->error_spec.errFlags, lsp->error_spec.errCode, lsp->error_spec.errValue, VTY_NEWLINE); 
		}

	}
	else
	{
		strcpy(temp1, inet_ntoa(lsp->common.Session_Para.srcAddr));
		strcpy(temp2, inet_ntoa(lsp->common.Session_Para.destAddr));
		zlog_info("Src %s/%d, dest %s/%d", temp1, lsp->common.Session_Para.srcPort,
					temp2, lsp->common.Session_Para.destPort);
		if (lsp->common.GenericTSpec_Para)
		{
			zlog_info("Generic TSPEC R=%s, B=%s, P=%s, m=%d, M=%d", 
					  bandwidth_value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->R),
					  bandwidth_value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->B),
					  bandwidth_value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->P),
					  (lsp->common.GenericTSpec_Para)->m,
					  (lsp->common.GenericTSpec_Para)->M);
		}
		else if (lsp->common.SonetTSpec_Para)
		{
			zlog_info("SONET TSPEC ST=%d, RCC=%d, NCC=%d, NVC=%d, MT=%d, T=%d, P=%d", 
					  (lsp->common.SonetTSpec_Para)->Sonet_ST,
					  (lsp->common.SonetTSpec_Para)->Sonet_RCC,
					  (lsp->common.SonetTSpec_Para)->Sonet_NCC,
					  (lsp->common.SonetTSpec_Para)->Sonet_NVC,
					  (lsp->common.SonetTSpec_Para)->Sonet_MT,
					  (lsp->common.SonetTSpec_Para)->Sonet_T,
					  (lsp->common.SonetTSpec_Para)->Sonet_P);
		}
		zlog_info("Encoding %s, Switching %s, G-Pid %s", 
				    value_to_string (&conv_encoding, lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType),
				    value_to_string (&conv_swcap, lsp->common.LabelRequest_Para.data.gmpls.switchingType),
				    value_to_string (&conv_gpid, lsp->common.LabelRequest_Para.data.gmpls.gPid));

		zlog_info("Status: %s", value_to_string(&conv_lsp_status, lsp->status));
		if (lsp->error_spec.errCode != 0)
		{
			zlog_info("Error Spec: node=%s, flags=%d, code=%d, value=%d", inet_ntoa(lsp->error_spec.nodeAddress), lsp->error_spec.errFlags, lsp->error_spec.errCode, lsp->error_spec.errValue); 
		}
	}

}

DEFUN (dragon_show_lsp,
       dragon_show_lsp_cmd,
       "show lsp NAME",
       SHOW_STR
       "Show LSP status\n")
{
  struct listnode *node;
  struct lsp *lsp = NULL;
  int found = 0;
  
  if (argc > 0)
  {
  	found = 0;
  	LIST_LOOP(dmaster.dragon_lsp_table, lsp, node)
  	{
  		if (lsp->common.SessionAttribute_Para && 
  		    strcmp((lsp->common.SessionAttribute_Para)->sessionName, argv[0])==0)
  		{
  			found = 1;
  		   	dragon_show_lsp_detail(lsp, vty); 
  		}
  	}
  	if (!found)
  		vty_out(vty, "No matching LSP named %s %s", argv[0], VTY_NEWLINE);
  }
  else
  {
	  vty_out (vty, "                          **LSP status summary** %s%s", VTY_NEWLINE, VTY_NEWLINE);
          vty_out (vty, "Name        Status     Dir   Source (IP/LSP ID)  Destination (IP/Tunnel ID)%s", VTY_NEWLINE);
          vty_out (vty, "--------------------------------------------------------------------------%s", VTY_NEWLINE);

          LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
          {
                          if ((lsp->common.SessionAttribute_Para)->nameLength>=10){
                                        vty_out (vty, "%s%s", (lsp->common.SessionAttribute_Para)->sessionName, VTY_NEWLINE);
                                   vty_out(vty, "            %-11s", value_to_string(&conv_lsp_status, lsp->status));
                          }
                          else{
                                          vty_out (vty, "%-12s", (lsp->common.SessionAttribute_Para)->sessionName);
                                        vty_out(vty, "%-11s", value_to_string(&conv_lsp_status, lsp->status));
                          }
                          if (lsp->flag & LSP_FLAG_BIDIR)
                              vty_out(vty, "%-6s", "<=>");
                          else
                              vty_out(vty, "%-6s", " =>");
                          vty_out (vty, "%-20s", inet_ntoa(lsp->common.Session_Para.srcAddr));
                          vty_out (vty, "%s%s", inet_ntoa(lsp->common.Session_Para.destAddr), VTY_NEWLINE);
                          vty_out (vty, "                             %-20d", lsp->common.Session_Para.srcPort );
                          vty_out (vty, "%d%s", lsp->common.Session_Para.destPort, VTY_NEWLINE);
          }
 
   }
  return CMD_SUCCESS;
}

ALIAS (dragon_show_lsp,
       dragon_show_lsp_all_cmd,
       "show lsp",
       SHOW_STR
       "Show LSP status\n");

DEFUN (dragon_set_ucid,
       dragon_set_ucid_cmd,
       "set ucid ID",
       "Set Universal Client ID (UCID)\n"
       "UCID\n"
       "UCID number or IP\n"
	)
{
    struct in_addr ip;
    ip.s_addr = 0;
    inet_aton(argv[0], &ip);
    if (ip.s_addr != 0)
        dmaster.UCID = htonl(ip.s_addr);
    else {
	vty_out(vty, "Wrong UCID %s: (format: number or A.B.C.D) %s", argv[0], VTY_NEWLINE);
	return CMD_WARNING;
    }
    return CMD_SUCCESS;
}

DEFUN (dragon_show_ucid,
       dragon_show_ucid_cmd,
       "show ucid",
       "Show Universal Client ID (UCID)\n"
       "UCID\n"
	)
{
    vty_out(vty, "\tUniversal Client Identifier (UCID) = 0x%x (%s)%s", dmaster.UCID, inet_ntoa(*(struct in_addr*)&dmaster.UCID), VTY_NEWLINE);
    return CMD_SUCCESS;
}

int
dragon_master_init()
{
  struct _sessionParameters default_session;

  srandom(getpid() + getppid() + time(NULL));
  dmaster.UCID = random();

  dmaster.master = thread_master_create ();
  master = dmaster.master;
  
  dmaster.dragon_lsp_table = list_new();
  dmaster.dragon_lsp_table->del = (void (*) (void *))lsp_del;

  dmaster.recycled_lsp_list = list_new();
  dmaster.recycled_lsp_list->del = (void (*) (void *))lsp_del;
  
  dmaster.dragon_packet_fifo = dragon_fifo_new();


  dmaster.module = dmodule;
  
  dmaster.t_write = NULL;

  /* Init RSVP API instance (socket connection to RSVPD) */
  zlog_info("Initializing socket connect to RSVP daemon API");
  dmaster.api = zInitRsvpApiInstance();
  dmaster.rsvp_fd = zGetApiFileDesc(dmaster.api);
  if (!dmaster.rsvp_fd)
  {
	  zlog_warn("dragon_master_init(): Cannot get file descriptor for RSVP upcall. ");
	  return (-1);
  }

  dmaster.t_rsvp_read = thread_add_read (master, dragon_rsvp_read, NULL, dmaster.rsvp_fd);

  dmaster.t_mon_accept = NULL;
  dmaster.mon_apiserver_list = NULL;

  default_session.Session_Para.destAddr.s_addr = 0x100007f; /* "127.0.0.1" */
  default_session.Session_Para.destPort = 0; 
  default_session.Session_Para.srcAddr.s_addr = 0;
  zInitRsvpPathRequest(dmaster.api, &default_session, 0);
  //clear current localId list in RSVPD
  zDeleteLocalId(dmaster.api, 0xffff, 0xffff, 0xffff);

  return 0;
}

int dragon_config_write(struct vty *vty)
{
  listnode node;
  struct lsp *lsp = NULL;
  int l;
  char temp1[20], temp2[20];  /* Added to avoid an C optimization /inet_ntoa (?) problem */

  if (host.name)
    vty_out (vty, "hostname %s%s", host.name, VTY_NEWLINE);
  if (host.password)
	vty_out (vty, "password %s%s%s", host.password, VTY_NEWLINE, VTY_NEWLINE);

  vty_out(vty, "%s! configure DRAGON modules' IP addresses / ports%s", VTY_NEWLINE, VTY_NEWLINE);
  if (dmaster.module[MODULE_NARB_INTRA].ip_addr.s_addr!=0 &&
  	dmaster.module[MODULE_NARB_INTRA].ip_addr.s_addr!=0x100007f)
  {
	  vty_out (vty, "configure narb intra-domain ip-address %s port %d %s", 
	  	                  inet_ntoa(dmaster.module[MODULE_NARB_INTRA].ip_addr), 
	  	                  dmaster.module[MODULE_NARB_INTRA].port,
	  	                  VTY_NEWLINE);
  }
  if (dmaster.module[MODULE_NARB_INTER].ip_addr.s_addr!=0 &&
  	dmaster.module[MODULE_NARB_INTER].ip_addr.s_addr!=0x100007f)
  {
	  vty_out (vty, "configure narb inter-domain ip-address %s port %d %s", 
	  	                  inet_ntoa(dmaster.module[MODULE_NARB_INTER].ip_addr), 
	  	                  dmaster.module[MODULE_NARB_INTER].port,
	  	                  VTY_NEWLINE);
  }
  if (dmaster.module[MODULE_PCE].ip_addr.s_addr!=0 &&
  	dmaster.module[MODULE_PCE].ip_addr.s_addr!=0x100007f)
  {
	  vty_out (vty, "configure pce ip-address %s port %d %s", 
	  	                  inet_ntoa(dmaster.module[MODULE_PCE].ip_addr), 
	  	                  dmaster.module[MODULE_PCE].port,
	  	                  VTY_NEWLINE);
  }
  
  vty_out(vty, "%s! GMPLS label-switched paths%s", VTY_NEWLINE, VTY_NEWLINE);
  LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
  {
	vty_out (vty, "edit lsp %s%s", (lsp->common.SessionAttribute_Para)->sessionName, VTY_NEWLINE);
	strcpy(temp1, inet_ntoa(lsp->common.Session_Para.srcAddr));
	strcpy(temp2, inet_ntoa(lsp->common.Session_Para.destAddr));
       vty_out (vty, "  set source ip-address %s destination ip-address %s tunnel-id %d lsp-id %d%s", 
	   	                  temp1, 
	   	                  temp2, 
	   	                  (int)(lsp->common.Session_Para.destPort),
				    (int)(lsp->common.Session_Para.srcPort), VTY_NEWLINE);
       vty_out (vty, "  set bandwidth %s swcap %s encoding %s gpid %s%s", 
	   			     bandwidth_value_to_string (&conv_bandwidth, (lsp->common.GenericTSpec_Para)->R),
		   		     value_to_string (&conv_swcap, lsp->common.LabelRequest_Para.data.gmpls.switchingType),
				     value_to_string (&conv_encoding, lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType),
				     value_to_string (&conv_gpid, lsp->common.LabelRequest_Para.data.gmpls.gPid), 
				     VTY_NEWLINE);
	if (lsp->flag & LSP_FLAG_BIDIR)
	{
	    vty_out (vty, "  set direction bi upstream-label ");
	    if (lsp->common.upstreamLabel)
	      vty_out (vty, " %d%s", *(lsp->common.upstreamLabel), VTY_NEWLINE);
            else
	      vty_out (vty, " <unassigned-label>%s", VTY_NEWLINE);
	}
	else
	    vty_out (vty, "  set direction uni upstream-label 1%s", VTY_NEWLINE);

       if (lsp->common.labelSet && lsp->common.labelSetSize)
       {
       	for (l=0;l<lsp->common.labelSetSize;l++)
			vty_out(vty, "  set label-set %d%s", lsp->common.labelSet[l], VTY_NEWLINE);
       }
	vty_out (vty, "exit%s",  VTY_NEWLINE);
  }
  return 0;
}

static void preserve_local_ids()
{
	struct local_id * lid = NULL;
	u_int16_t * ptag = NULL;
	listnode node, node2;
	FILE *fp;

	fp = fopen("/var/preserve/dragon.localids", "w");

	if (fp != NULL) 
	{
		LIST_LOOP(registered_local_ids, lid, node)
		{
			fprintf(fp, "%d:%d",  lid->type, lid->value);
			if (lid->type == LOCAL_ID_TYPE_GROUP || lid->type == LOCAL_ID_TYPE_TAGGED_GROUP)
			{
				LIST_LOOP(lid->group, ptag, node2)
				{
					fprintf(fp, " %d", *ptag);
				}
			}
			fprintf(fp, "\n");
		}
		fclose (fp);
	}
}

DEFUN (dragon_set_local_id,
       dragon_set_local_id_cmd,
       "set local-id port PORT",
       SET_STR
       "A local ingress/egress port identifier\n"
       "Pick a LocalId type\n"
       "Port number in the range <0-65535> or in the format shelf/slot/port\n")
{
    u_int16_t type = LOCAL_ID_TYPE_PORT;
    u_int32_t tag;
    struct local_id * lid = NULL;
    listnode node;

    if (get_switch_port_by_name(argv[0], &tag) == -1)
    {
            vty_out (vty, "Wrong localID format: %s%s", argv[0], VTY_NEWLINE);
            return CMD_WARNING;
    
    }
    LIST_LOOP(registered_local_ids, lid, node)
    {
        if (lid->type == type && lid->value == (u_int16_t)tag)
        {
            vty_out (vty, "localID %s (port) has already existed... %s", argv[0], VTY_NEWLINE);
            return CMD_WARNING;
        }
    }

    lid = XMALLOC(MTYPE_TMP, sizeof(struct local_id));
    memset(lid, 0, sizeof(struct local_id));
    lid->type = type;
    lid->value = tag;
    listnode_add(registered_local_ids, lid);
    zAddLocalId(dmaster.api, type, tag, 0xffff);
    preserve_local_ids();
    return CMD_SUCCESS;
}

DEFUN (dragon_set_local_id_group,
       dragon_set_local_id_group_cmd,
       "set local-id (group|tagged-group) <0-65535> (add|delete) PORT",
       SET_STR
       "A local ingress/egress port identifier\n"
       "Is the localId associated with a group of untagged or tagged ports?\n"
       "Group number in the range <0-65535>\n"
       "Add or Delete a port from the tagged/untagged group?\n"
       "Port number in the range <0-65535> or in shelf-slot-port format\n" )
{
    u_int16_t type = strcmp(argv[0], "group") == 0 ? LOCAL_ID_TYPE_GROUP: LOCAL_ID_TYPE_TAGGED_GROUP;
    u_int16_t  tag = atoi(argv[1]), *iter_tag;
    u_int32_t sub_tag;
    struct local_id * lid = NULL;
    listnode node, node_inner;

    if (get_switch_port_by_name(argv[3], &sub_tag) == -1)
    {
            vty_out (vty, "Wrong localID format: %s%s", argv[3], VTY_NEWLINE);
            return CMD_WARNING;
    }

    LIST_LOOP(registered_local_ids, lid, node)
    {
        if (lid->type == type && lid->value == tag)
        {
            if (strcmp(argv[2], "add") == 0)
            {
                LIST_LOOP(lid->group, iter_tag, node_inner)
                {
                    if (*iter_tag == (u_int16_t)sub_tag)
                    {
                        vty_out (vty, "localID %d (group) has already had a member %d ... %s", tag, sub_tag, VTY_NEWLINE);
                        return CMD_WARNING;
                    }
                }
                local_id_group_add(lid, sub_tag);
                zAddLocalId(dmaster.api, type, tag, sub_tag);
            }
            else
            {
                LIST_LOOP(lid->group, iter_tag, node_inner)
                {
                    if (*iter_tag == sub_tag)
                        break;
                }
                if (node_inner == NULL)
                {
                    vty_out (vty, "localID %d (group) does not have the member %d ... %s", tag, sub_tag, VTY_NEWLINE);
                    return CMD_WARNING;
                }
                local_id_group_delete(lid, sub_tag);
                if (!lid->group)
                {
                    listnode_delete(registered_local_ids, lid);
                    XFREE(MTYPE_TMP, lid);
                }
                zDeleteLocalId(dmaster.api, type, tag, sub_tag);
            }
            preserve_local_ids();
            return CMD_SUCCESS;
        }
    }

    if (strcmp(argv[2], "delete") == 0)
    {
        vty_out (vty, "localID %d (group) does not exist... %s", tag, VTY_NEWLINE);
        return CMD_WARNING;
    }

    lid = XMALLOC(MTYPE_TMP, sizeof(struct local_id));
    memset(lid, 0, sizeof(struct local_id));
    lid->type = type;
    lid->value = tag;
    local_id_group_add(lid, sub_tag);
    listnode_add(registered_local_ids, lid);
    zAddLocalId(dmaster.api, type, tag, sub_tag);
    preserve_local_ids();
    return CMD_SUCCESS;
}

DEFUN (dragon_set_local_id_subnet_if,
       dragon_set_local_id_subnet_if_cmd,
       "set local-id subnet-interface ID",
       SET_STR
       "A local ingress/egress subnet interface identifier\n"
       "Subnet interface ID in subet_id/first_timeslot format\n"
       "ID number in the range <0-255>, see subnet-uni-id in ospfd.conf\n")
{
    u_int16_t type = LOCAL_ID_TYPE_SUBNET_IF_ID;
    u_int32_t tag = 0, subnet_if_id = 0, first_timeslot = ANY_TIMESLOT;
    struct local_id * lid = NULL;
    listnode node;

    if (strstr(argv[0], "-") != NULL)
        sscanf(argv[0], "%d-%d", &subnet_if_id, &first_timeslot);
    else if (strstr(argv[0], "/") != NULL)
        sscanf(argv[0], "%d/%d", &subnet_if_id, &first_timeslot);
    else
        sscanf(argv[0], "%d", &subnet_if_id);

    if (subnet_if_id == 0 || subnet_if_id >= 255)
    {
        vty_out (vty, "###Wrong  subnet-interface localID %s, format should be ID, ID-TS or ID/TS (0 < ID, TS <255)...%s", argv[0], VTY_NEWLINE);
        return CMD_WARNING;
    }
    tag = (((subnet_if_id&0xff) << 8) | (first_timeslot&0xff));

    LIST_LOOP(registered_local_ids, lid, node)
    {
        if (lid->type == type && lid->value == (u_int16_t)tag)
        {
            vty_out (vty, "###The localID %s (subnet-interface) has already existed... %s", argv[0], VTY_NEWLINE);
            return CMD_WARNING;
        }
    }

    lid = XMALLOC(MTYPE_TMP, sizeof(struct local_id));
    memset(lid, 0, sizeof(struct local_id));
    lid->type = type;
    lid->value = tag;
    listnode_add(registered_local_ids, lid);
    zAddLocalId(dmaster.api, type, tag, 0xffff);
    preserve_local_ids();
    return CMD_SUCCESS;
}


DEFUN (dragon_delete_local_id,
       dragon_delete_local_id_cmd,
       "delete local-id (port|group|tagged-group|subnet-interface) NAME",
       SET_STR
       "A local ingress/egress port identifier\n"
       "Pick a LocalId type\n"
       "Port number in the range <0-65535>\n" )
{
    u_int16_t type;
    u_int32_t  tag = 0;
    struct local_id * lid = NULL;
    listnode node;

    if (strcmp(argv[0], "port") == 0)
    {
        type = LOCAL_ID_TYPE_PORT;
        if (get_switch_port_by_name(argv[1], &tag) == -1)
        {
            vty_out (vty, "Wrong port format: %s%s", argv[1], VTY_NEWLINE);
            return CMD_WARNING;        
        }
    }
    else if (strcmp(argv[0], "group") == 0)
    {
        type = LOCAL_ID_TYPE_GROUP;
        sscanf(argv[1], "%d", &tag);
    }
    else if (strcmp(argv[0], "tagged-group") == 0)
    {
        type = LOCAL_ID_TYPE_TAGGED_GROUP;
        sscanf(argv[1], "%d", &tag);
    }
    else if (strcmp(argv[0], "subnet-interface") == 0)
    {
        type = LOCAL_ID_TYPE_SUBNET_IF_ID;
        sscanf(argv[1], "%d", &tag);
    }
    else
    {
        vty_out (vty, "Unknown local id type: %s%s", argv[1], VTY_NEWLINE);
        return CMD_WARNING;
    }

    LIST_LOOP(registered_local_ids, lid, node)
    {
        if (lid->type == type && lid->value == (u_int16_t)tag)
        {
            if (type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP)
                local_id_group_free(lid);
            listnode_delete(registered_local_ids, lid);
            XFREE(MTYPE_TMP, lid);
            zDeleteLocalId(dmaster.api, type, tag, 0xffff);
            preserve_local_ids();
            return CMD_SUCCESS;
        }
    }

    vty_out (vty, "localID %s of type '%s' does not exist... %s", argv[1], argv[0], VTY_NEWLINE);
    return CMD_WARNING;
}
    
DEFUN (dragon_delete_local_id_all,
       dragon_delete_local_id_all_cmd,
       "delete local-id all",
       SET_STR
       "Local ingress/egress port identifier\n"
       "All LocalId(s)\n")
{
    u_int16_t type;
    struct local_id * lid = NULL;
    listnode node;

    LIST_LOOP(registered_local_ids, lid, node)
    {
        if (type == LOCAL_ID_TYPE_GROUP || type == LOCAL_ID_TYPE_TAGGED_GROUP)
            local_id_group_free(lid);
        XFREE(MTYPE_TMP, lid);
    }
    list_delete_all_node(registered_local_ids);
    zDeleteLocalId(dmaster.api, 0xffff, 0xffff, 0xffff);
    preserve_local_ids();
    return CMD_SUCCESS;
}

ALIAS (dragon_delete_local_id_all,
       dragon_clear_local_id_cmd,
       "clear local-id",
       "Clear all localId(s)\n"
       "Local ingress/egress port identifier\n");

DEFUN (dragon_show_local_id,
       dragon_show_local_id_cmd,
       "show local-id",
       SHOW_STR
       "A local ingress/egress port identifier\n")
{
    struct local_id * lid = NULL;
    listnode node;

    if ( registered_local_ids->count == 0)
    {
        vty_out(vty, "No localID configured ...%s", VTY_NEWLINE);
        return CMD_WARNING;
    }

    vty_out(vty, "%sDisplaying %d registered localID(s)%s", VTY_NEWLINE, registered_local_ids->count, VTY_NEWLINE);
    vty_out(vty, " LocalID  Type         (Tags/Ports in Group)%s", VTY_NEWLINE);
    LIST_LOOP(registered_local_ids, lid, node)
    {
         if (lid->type == LOCAL_ID_TYPE_PORT)
	     vty_out(vty, "%-4d(%s) [%-12s]    ", lid->value, get_switch_port_string(lid->value), lid_types[lid->type]);
         else if (lid->type == LOCAL_ID_TYPE_SUBNET_IF_ID)
	     vty_out(vty, "%-4d(%d/%d) [%-12s]    %s", lid->value, (lid->value >> 8), (lid->value & 0xff),  lid_types[lid->type]
	        , (lid->value & 0xff) == 255 ? ": 255 means ANY timeslots" : "" );

         if (lid->type == LOCAL_ID_TYPE_GROUP || lid->type == LOCAL_ID_TYPE_TAGGED_GROUP)
            local_id_group_show(vty, lid);
         else
            vty_out(vty, "%s", VTY_NEWLINE);
    }
    return CMD_SUCCESS;
}

DEFUN (dragon_set_local_id_group_refresh,
       dragon_set_local_id_group_refresh_cmd,
       "set local-id (group|tagged-group) <0-65535> refresh",
       SET_STR
       "A local ingress/egress port identifier\n"
       "Is the localId associated with a group of untagged or tagged ports?\n"
       "Port number in the range <0-65535>\n"
       "Refresh a port from the tagged/untagged group?\n" )
{
    u_int16_t type = strcmp(argv[0], "group") == 0 ? LOCAL_ID_TYPE_GROUP: LOCAL_ID_TYPE_TAGGED_GROUP;
    u_int16_t  value = atoi(argv[1]);
    struct local_id * lid = NULL;
    listnode node;

    LIST_LOOP(registered_local_ids, lid, node)
    {
        if (type == lid->type && value == lid->value) {
            zRefreshLocalId(dmaster.api, type, value, 0);
            return CMD_SUCCESS;
        }
    }

    vty_out(vty, "Local ID type (%d) value (%d) cannot be refreshed --> not registered. %s", type, value, VTY_NEWLINE);
    return CMD_WARNING;
}

DEFUN (dragon_set_mon_apiserver,
       dragon_set_mon_apiserver_cmd,
       "set apiserver (on|off)",
       SET_STR
       "DRAGON service API\n"
       "On/enabled\n"
       "Off/disabled\n")
{
  int ret;
  if (strcmp(argv[0], "on") == 0)
    {
      if (dmaster.t_mon_accept)
        {
          vty_out(vty, "dragon_apiserver has already been turned on ...%s", VTY_NEWLINE);
          return CMD_SUCCESS;
        }
      ret = mon_apiserver_init();
      if (ret < 0)
        {
          vty_out(vty, "dragon_apiserver_init failed, returning code: %d %s", ret, VTY_NEWLINE);
          return CMD_WARNING;
        }
    }
  else
    {
      if (!dmaster.t_mon_accept)
        {
          vty_out(vty, "dragon_apiserver_init has already been turned off ...%s", VTY_NEWLINE);
          return CMD_SUCCESS;
        }
      mon_apiserver_term();
    }

  return CMD_SUCCESS;
}

DEFUN (dragon_show_mon_apiserver,
       dragon_show_mon_apiserver_cmd,
       "show apiserver",
       SHOW_STR
       "DRAGON service API status\n")
{
  char onoff[4];
  if (dmaster.t_mon_accept)
      strcpy(onoff, "on");
  else
      strcpy(onoff, "off");
  vty_out(vty, "\t> DRAGON API server is turned %s.%s", onoff, VTY_NEWLINE);

  return CMD_SUCCESS;
}

void
dragon_supp_vty_init ()
{
  install_node(&lsp_node, NULL);
  
  install_element(VIEW_NODE, &dragon_telnet_module_cmd);
  install_element(VIEW_NODE, &dragon_telnet_module_narb_cmd);
  install_element(VIEW_NODE, &dragon_set_pce_para_cmd);
  install_element(VIEW_NODE, &dragon_set_pce_para_ip_cmd);
  install_element(VIEW_NODE, &dragon_set_narb_para_cmd);
  install_element(VIEW_NODE, &dragon_set_narb_para_ip_cmd);
  install_element(VIEW_NODE, &dragon_show_module_status_cmd);
  install_element(VIEW_NODE, &dragon_edit_lsp_cmd);
  install_element(VIEW_NODE, &dragon_show_version_cmd);
  install_element(VIEW_NODE, &dragon_show_lsp_cmd);
  install_element(VIEW_NODE, &dragon_show_lsp_all_cmd);
  install_element(VIEW_NODE, &dragon_commit_lsp_sender_cmd);
  install_element(VIEW_NODE, &dragon_commit_lsp_receiver_cmd);
  install_element(VIEW_NODE, &dragon_commit_lsp_default_cmd);
  install_element(VIEW_NODE, &dragon_delete_lsp_cmd);
  install_element(VIEW_NODE, &dragon_show_narb_extra_options_cmd);
  install_element(VIEW_NODE, &dragon_show_mon_apiserver_cmd);

  registered_local_ids = list_new();
  install_element(VIEW_NODE, &dragon_show_local_id_cmd);
  install_element(CONFIG_NODE, &dragon_show_local_id_cmd);
  install_element(VIEW_NODE, &dragon_set_local_id_cmd);
  install_element(CONFIG_NODE, &dragon_set_local_id_cmd);
  install_element(VIEW_NODE, &dragon_set_local_id_group_cmd);
  install_element(CONFIG_NODE, &dragon_set_local_id_group_cmd);
  install_element(VIEW_NODE, &dragon_set_local_id_subnet_if_cmd);
  install_element(CONFIG_NODE, &dragon_set_local_id_subnet_if_cmd);
  install_element(VIEW_NODE, &dragon_delete_local_id_cmd);
  install_element(CONFIG_NODE, &dragon_delete_local_id_cmd);
  install_element(VIEW_NODE, &dragon_delete_local_id_all_cmd);
  install_element(CONFIG_NODE, &dragon_delete_local_id_all_cmd);
  install_element(VIEW_NODE, &dragon_clear_local_id_cmd);
  install_element(CONFIG_NODE, &dragon_clear_local_id_cmd);
  install_element(VIEW_NODE, &dragon_set_local_id_group_refresh_cmd);
  install_element(CONFIG_NODE, &dragon_set_local_id_group_refresh_cmd);
  install_element(VIEW_NODE, &dragon_set_ucid_cmd);
  install_element(CONFIG_NODE, &dragon_set_ucid_cmd);
  install_element(VIEW_NODE, &dragon_show_ucid_cmd);
  install_element(CONFIG_NODE, &dragon_show_ucid_cmd);
  install_element(VIEW_NODE, &dragon_set_narb_extra_options_cmd);
  install_element(CONFIG_NODE, &dragon_set_narb_extra_options_cmd);
  install_element(VIEW_NODE, &dragon_set_mon_apiserver_cmd);
  install_element(CONFIG_NODE, &dragon_set_mon_apiserver_cmd);
  
  install_element(CONFIG_NODE, &dragon_set_pce_para_cmd);
  install_element(CONFIG_NODE, &dragon_set_pce_para_ip_cmd);
  install_element(CONFIG_NODE, &dragon_set_narb_para_cmd);
  install_element(CONFIG_NODE, &dragon_set_narb_para_ip_cmd);
  install_element(CONFIG_NODE, &dragon_show_module_status_cmd);
  install_element(CONFIG_NODE, &dragon_edit_lsp_cmd);
  install_element(CONFIG_NODE, &dragon_commit_lsp_sender_cmd);
  install_element(CONFIG_NODE, &dragon_commit_lsp_receiver_cmd);
  install_element(CONFIG_NODE, &dragon_commit_lsp_default_cmd);
  
  install_default(LSP_NODE);
  install_element(LSP_NODE, &dragon_set_lsp_name_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_ip_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_sw_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_dir_cmd);
  install_element(LSP_NODE, &dragon_set_label_set_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_uni_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_uni_implicit_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_vtag_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_vtag_default_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_vtag_any_cmd);  
  install_element(LSP_NODE, &dragon_set_lsp_vtag_subnet_edge_cmd);  
  //install_element(LSP_NODE, &dragon_enable_lsp_ext_info_edge_vtag_cmd);
  //install_element(LSP_NODE, &dragon_enable_lsp_ext_info_edge_vtag_default_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_dtl_hop_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_ero_hop_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_ero_hop_ipv4_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_subnet_ero_hop_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_subnet_ero_hop_ipv4_cmd);
}

