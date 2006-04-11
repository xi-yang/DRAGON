#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "ast_master.h"
#include "buffer.h"
#include "log.h"

#ifdef RESOURCE_BROKER
#define RESOURCE_REC	"/tmp/resource.xml"
#endif

char *entity_type_name[] =
  { "None",
    "PC",
    "correlator",
    "computation_array" };

char *link_type_name[] =
  { "None",
    "EtherPipeBasic",
    "EtherPipeUltra",
    "TDMBasic" };

char *status_type_details[] =
  { "AST_UNKNOWN", "AST_SUCCESS", "AST_FAILURE", "AST_PENDING",
    "AST_AST_COMPLETE", "AST_APP_COMPLETE" };

char *function_type_details[] =
  { "VOID", "SETUP_REQ", "SETUP_RESP", "AST_COMPLETE",
    "RELEASE_REQ", "RELEASE_RESP", "APP_COMPLETE", "QUERY_REQ",
    "QUERY_RESP", "INVALID_INCOMING_VALUE" };

struct string_syntex_check local_field =
{
	4,
	{{"lsp-id",		"LSP-ID"},
	 {"port",		"Untagged single port"},
	 {"tagged-group",       "Tagged group"},
	 {"group",		"Untagged port group"}}
};

struct string_syntex_check bandwidth_field = 
{
	22,   
 	{{"gige",	"GigE (1000.00 Mbps)"},
	 {"gige_f", 	"Fast GigE (1250.00 Mbps)"},
	 {"2gige",	"2 GigE (2000.00 Mbps)"},
	 {"3gige",      "3 GigE (3000.00 Mbps)"},
	 {"4gige", 	"4 GigE (4000.00 Mbps)"},
	 {"5gige",      "5 GigE (5000.00 Mbps)"},
	 {"6gige",      "6 GigE (6000.00 Mbps)"},
	 {"7gige",      "7 GigE (7000.00 Mbps)"},
	 {"8gige",      "8 GigE (8000.00 Mbps)"},
	 {"9gige",      "9 GigE (9000.00 Mbps)"},
	 {"eth100M",	"Ethernet (100.00 Mbps)"},
	 {"eth200M",    "Ethernet (200.00 Mbps)"},
	 {"eth300M",    "Ethernet (300.00 Mbps)"},
	 {"eth400M",    "Ethernet (400.00 Mbps)"},
	 {"eth500M",    "Ethernet (500.00 Mbps)"},
	 {"eth600M",    "Ethernet (600.00 Mbps)"},
	 {"eth700M",    "Ethernet (700.00 Mbps)"},
	 {"eth800M",    "Ethernet (800.00 Mbps)"},
	 {"eth900M",    "Ethernet (900.00 Mbps)"},
	 {"10g",	"10GigE-LAN(10000.00 Mbps)"},
	 {"hdtv", 	"HDTV/SMPTE-292 (1485.00 Mbps)"},
	 {"oc48",	"OC-48/STM-16 (2488.32 Mbps)"}}
};

struct string_syntex_check swcap_field =
{
	4,
	{{"psc1",	"Packet-Switch Capable-1"},
	 {"l2sc",	"Layer-2 Switch Capable"},
	 {"lsc",	"Lambda Switch Capable"},
	 {"tdm",	"Time Division Multiplex Capable"}}
};

struct string_syntex_check encoding_field =
{
	4,
	{{"packet",	"Packet"},
	 {"ethernet",	"Ethernet"},
	 {"lambda",	"Lambda (photonic)"},
	 {"sdh",	"SONET/SDH"}}
};

struct string_syntex_check gpid_field =
{
	3,
	{{"ethernet", 	"Ethernet"},
	 {"lambda",	"Lambda"},
	 {"sdh",	"SONET/SDH"}}
};

char* generate_ast_id(int type)
{
  static char id[100];

  memset(id, 0, 100); 

  switch (type) {
    case ID_SETUP:
      gethostname(id, 100);
      sprintf(id+strlen(id), "_%d", (int)time(NULL));
      break;
    case ID_TEMP:
      sprintf(id, "tmp");
      gethostname(id+strlen(id), 100);
      sprintf(id+strlen(id), "_%d", (int)time(NULL));
      break;
    case ID_QUERY:
      sprintf(id, "query");
      sprintf(id+strlen(id), "_%d", (int)time(NULL));
      break;
    default:
      return NULL;
  }

  return strdup(id);
}

void
free_application_cfg(struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode;
  struct node_cfg *mynode;
  struct link_cfg *mylink;
  
  if (app_cfg == NULL)
    return;

  if (app_cfg->node_list != NULL) {
    for (curnode = app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      mynode = (struct node_cfg*) curnode->data;
      if (mynode->link_list != NULL) 
	adtlist_free(mynode->link_list);
      if (mynode->agent_message != NULL)
	free(mynode->agent_message);
      if (mynode->command != NULL)
	free(mynode->command);
      free(mynode);
     curnode->data = NULL;
    }
  }
  adtlist_free(app_cfg->node_list); 
  app_cfg->node_list = NULL;

  if (app_cfg->link_list != NULL) {
    for (curnode = app_cfg->link_list->head;
	 curnode;
	 curnode = curnode->next) {
      mylink = (struct link_cfg*) curnode->data;
      if (mylink->agent_message != NULL) 
	free(mylink->agent_message);
      free(mylink);
      curnode->data = NULL;
    }
  }
  free(app_cfg->link_list);
  app_cfg->link_list = NULL;

  if (app_cfg->glob_ast_id != NULL) 
    free(app_cfg->glob_ast_id);
  if (app_cfg->ast_ip != NULL)
    free(app_cfg->ast_ip);

  memset(app_cfg, 0, sizeof(struct application_cfg));
}

void
print_error_response(char *path)
{
  FILE *fp;

  if (!path)
    return;
  fp = fopen(path, "w+");
  if (fp == NULL)
    return;
  
  fprintf(fp, "<topology>\n");
  fprintf(fp, "<ast_func>%s</ast_func>\n", function_type_details[glob_app_cfg.function]);
  if (glob_app_cfg.glob_ast_id != NULL) 
    fprintf(fp, "<glob_ast_id>%s</glob_ast_id>\n", glob_app_cfg.glob_ast_id);
  fprintf(fp, "<ast_status>AST_FAILURE</ast_status>\n");
  if (glob_app_cfg.details[0] != '\0')
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg.details);
  fprintf(fp, "</topology>\n");
  fflush(fp);
  fclose(fp);
}
void
print_node(char* buf, struct node_cfg* node)
{
  if (node == NULL)
    return;

  sprintf(buf, "<resource>\n");
  sprintf(buf+strlen(buf), "<name>%s</name>\n", node->name);
  sprintf(buf+strlen(buf), "<ast_status>%s</ast_status>\n", status_type_details[node->ast_status]);
  if (node->agent_message != NULL) 
    sprintf(buf+strlen(buf), "<agent_message>%s</agent_message>\n", node->agent_message);
  if (node->node_type != 0) 
    sprintf(buf+strlen(buf), "<resource_type>%s</resource_type>\n", entity_type_name[node->node_type]);
  if (node->ipadd[0] != '\0')
    sprintf(buf+strlen(buf), "<ip_addr>%s</ip_addr>\n", node->ipadd);
  if (node->te_addr[0] != '\0')
    sprintf(buf+strlen(buf), "<te_addr>%s</te_addr>\n", node->te_addr);
  if (node->command != NULL) 
    sprintf(buf+strlen(buf), "<command>%s</command>\n", node->command);
  sprintf(buf+strlen(buf), "</resource>\n");
}

void 
print_link(char* buf, struct link_cfg* link)
{
  if (link == NULL) 
    return;

  sprintf(buf, "<resource>\n");
  sprintf(buf+strlen(buf), "<name>%s</name>\n", link->name);
  sprintf(buf+strlen(buf), "<ast_status>%s</ast_status>\n", status_type_details[link->ast_status]);
  if (link->agent_message) 
    sprintf(buf+strlen(buf), "<agent_message>%s</agent_message>\n", link->agent_message);
  if (link->service_type != 0) 
    sprintf(buf+strlen(buf), "<resource_type>%s</resource_type>\n", link_type_name[link->service_type]);
  if (link->src != NULL) 
    sprintf(buf+strlen(buf), "<src>%s</src>\n", link->src->name);
  if (link->dest != NULL) 
    sprintf(buf+strlen(buf), "<dest>%s</dest>\n", link->dest->name);
  if (link->src_ip[0] != '\0')
    sprintf(buf+strlen(buf), "<src_ip>%s</src_ip>\n", link->src_ip);
  if (link->dest_ip[0] != '\0')
    sprintf(buf+strlen(buf), "<dest_ip>%s</dest_ip>\n", link->dest_ip);
  if (link->lsp_name[0] != '\0') 
    sprintf(buf+strlen(buf), "<lsp_name>%s</lsp_name>\n", link->lsp_name);
  if (link->bandwidth[0] != '\0')
    sprintf(buf+strlen(buf), "<bandwidth>%s</bandwidth>\n", link->bandwidth);
  if (link->swcap[0] != '\0') 
    sprintf(buf+strlen(buf), "<swcap>%s</swcap>\n", link->swcap); 
  if (link->encoding[0] != '\0')
    sprintf(buf+strlen(buf), "<encoding>%s</encoding>\n", link->encoding); 
  if (link->gpid[0] != '\0')
    sprintf(buf+strlen(buf), "<gpid>%s</gpid>\n", link->gpid);
  if (link->src_local_id_type[0] != '\0') {
    sprintf(buf+strlen(buf), "<src_local_id_type>%s</src_local_id_type>\n", link->src_local_id_type);
    sprintf(buf+strlen(buf), "<src_local_id>%d</src_local_id>\n", link->src_local_id);
  }
  if (link->dest_local_id_type[0] != '\0') {
    sprintf(buf+strlen(buf), "<dest_local_id_type>%s</dest_local_id_type>\n", link->dest_local_id_type);
    sprintf(buf+strlen(buf), "<dest_local_id>%d</dest_local_id>\n", link->dest_local_id);
  }
  if (link->vtag[0] != '\0') 
    sprintf(buf+strlen(buf), "<vtag>%s</vtag>\n", link->vtag);
  sprintf(buf+strlen(buf), "</resource>\n");
}

void
print_link_brief(char* buf, struct link_cfg* link)
{
  if (link == NULL) 
    return;

  sprintf(buf, "<resource>\n");
  sprintf(buf+strlen(buf), "<name>%s</name>\n", link->name);
  if (link->src != NULL) 
    sprintf(buf+strlen(buf), "<src>%s</src>\n", link->src->name);
  if (link->dest != NULL)
    sprintf(buf+strlen(buf), "<dest>%s</dest>\n", link->dest->name); 
  if (link->service_type != 0)
    sprintf(buf+strlen(buf), "<resource_type>%s</resource_type>\n", link_type_name[link->service_type]);
  sprintf(buf+strlen(buf), "<ast_status>%s</ast_status>\n", status_type_details[link->ast_status]);
  if (link->agent_message != NULL) 
    sprintf(buf+strlen(buf), "<agent_message>%s</agent_message>\n", link->agent_message);
  if (link->lsp_name[0] != '\0') 
    sprintf(buf+strlen(buf), "<lsp_name>%s</lsp_name>\n", link->lsp_name);
  sprintf(buf+strlen(buf), "</resource>\n");
}

void
print_xml_response(char* path, int version)
{
  struct adtlistnode *curnode;
  struct node_cfg *mynode;
  struct link_cfg *mylink;
  static char string[500];
  FILE* fp;

  if (!path)
    return;

  fp = fopen(path, "w+");
  if (!fp)
    return;

  memset(string, 0, 500);
  fprintf(fp, "<topology>\n");
  fprintf(fp, "<ast_func>%s</ast_func>\n", function_type_details[glob_app_cfg.function]);

  if (glob_app_cfg.glob_ast_id) 
    fprintf(fp, "<glob_ast_id>%s</glob_ast_id>\n", glob_app_cfg.glob_ast_id);

  if (glob_app_cfg.ast_ip) 
    fprintf(fp, "<ast_ip>%s</ast_ip>\n", glob_app_cfg.ast_ip);

  fprintf(fp, "<ast_status>%s</ast_status>\n", status_type_details[glob_app_cfg.ast_status]);

  if (glob_app_cfg.node_list) {
    for ( curnode = glob_app_cfg.node_list->head;
  	curnode;  
  	curnode = curnode->next) {
      mynode = (struct node_cfg*)(curnode->data);
      print_node(string, mynode);
      fprintf(fp, string);
    }
  }

  if (glob_app_cfg.link_list) {
    for ( curnode = glob_app_cfg.link_list->head;
  	curnode;
  	curnode = curnode->next) {
      mylink = (struct link_cfg*)(curnode->data);
  
      if (version == FULL_VERSION)
	print_link(string, mylink);
      else 
	print_link_brief(string, mylink);
      fprintf(fp, string);
    }
  }

  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);
}

void
print_final(char *path)
{
  struct adtlistnode *curnode, *curnode1;
  struct node_cfg *mynode;
  struct link_cfg *mylink;
  int i;
  char string[500];
  FILE *file;
  
  memset(string, 0, 500);
  if (!path)
    return;

  file = fopen(path, "w+");
  if (!file)
    return;

  fprintf(file, "<topology>\n");
  fprintf(file, "<ast_func>%s</ast_func>\n", function_type_details[glob_app_cfg.function]);

  if (glob_app_cfg.glob_ast_id) 
    fprintf(file, "<glob_ast_id>%s</glob_ast_id>\n", glob_app_cfg.glob_ast_id);

  fprintf(file, "<ast_status>%s</ast_status>\n", status_type_details[glob_app_cfg.ast_status]);

  if (glob_app_cfg.details[0] != '\0') 
    fprintf(file, "<details>%s</details>\n", glob_app_cfg.details);
 
  if (glob_app_cfg.ast_ip) 
    fprintf(file, "<ast_ip>%s</ast_ip>\n", glob_app_cfg.ast_ip);

  if (glob_app_cfg.node_list != NULL) { 
    for ( i = 1, curnode = glob_app_cfg.node_list->head;
	  curnode;  
	  i++, curnode = curnode->next) {
      mynode = (struct node_cfg*)(curnode->data);

      print_node(string, mynode);
      fprintf(file, string);
	
      if (glob_app_cfg.function == QUERY_RESP &&
	  mynode->link_list != NULL) {
	for (curnode1 = mynode->link_list->head;
	     curnode1;
	     curnode1 = curnode1->next) {
	  mylink = (struct link_cfg*)curnode1->data;
	  print_link(string, mylink);
	  fprintf(file, string);
	}
      }
    }
  }

  if (glob_app_cfg.link_list != NULL) {
    for (curnode = glob_app_cfg.link_list->head;
	curnode;
	curnode = curnode->next) {
      mylink = (struct link_cfg*)curnode->data;
      print_link(string, mylink); 
      fprintf(file, string);
    }
  }

  fprintf(file, "</topology>");
  fflush(file);
  fclose(file);
}

char**
parse_values(char *values, int *num)
{
  char *test, **toreturn, *word;
  int i, j, pos;
 
  test = strdup(values); 

  for ( i = 0, word = strtok(test, "|");
	word;
	i++, word = strtok(NULL, "|")) 
     ;

  if (!i)
    return NULL;

  *num = i;
  toreturn = malloc(i*sizeof(char*));

  for (pos = 0, j = 0;
       j != i;
       pos += strlen(toreturn[j])+1, j++) 
    toreturn[j] = test+pos;
  
  return toreturn;
}

char**
parse_minmax(char *minmax)
{
  char *test, **toreturn, *word;

  test = strdup(minmax);
  word = strtok(minmax, "-");
  if (!word)
    return NULL;

  toreturn = malloc(2*sizeof(char*));

  toreturn[0] = word;
  toreturn[1] = test+strlen(word)+1;

  return toreturn;
}

xmlNodePtr
findxmlnode(xmlNodePtr cur, char* keyToFound)
{
  if (strcasecmp(cur->name, keyToFound) == 0)
    return cur;

  cur = cur->xmlChildrenNode;
  while (cur) {
    if (cur->xmlChildrenNode)  {
      if (strcasecmp(cur->name, keyToFound) == 0) 
	return cur;
      return findxmlnode(cur, keyToFound);
    } 
    cur = cur->next;
  }

  return NULL;
}

int
template_xml_parser(char* filename, struct network_link* profile)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur, service_ptr;
  char keyToFound[100];
  struct link_cfg *newlink;

  doc = xmlParseFile(filename);

  if (doc == NULL) {
    zlog_err("template_xml_parser: Document %s is not parsed successfully", filename);
    return 0;
  }

  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    zlog_err("template_xml_parser: Document %s is empty", filename);
    xmlFreeDoc(doc);
    return 0;
  }

  /* first locate "Service_Definition" 
   */
  strcpy(keyToFound, "Service_Definition");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("template_xml_parser: Document %s doesn't have <%s>", 
	      filename, keyToFound);
    return 0;
  }

  newlink = &(profile->default_cfg);

  for ( cur = cur->xmlChildrenNode; 
	cur; 
	cur = cur->next ) {
    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
    if (!key)
      continue;

    if (strcasecmp(cur->name, "Service_Name") == 0)
      profile->service_name = key;
    else if (strcasecmp(cur->name, "xmlns") == 0) 
      profile->xmlns = key;
    else if (strcasecmp(cur->name, "version") == 0) 
      profile->version = key;
    else if (strcasecmp(cur->name, "Service_Characteristics") == 0) {
      service_ptr = cur;
      service_ptr = service_ptr->xmlChildrenNode;

      while (service_ptr) { 
	key = xmlNodeListGetString(doc, service_ptr->xmlChildrenNode, 1);

	if (strcasecmp(service_ptr->name, "Bandwidth") == 0) {
	  profile->bandwidth_minmax = parse_minmax(key);
	  if (!(profile->bandwidth_minmax)) {
	    zlog_err("template_xml_parser: Bandwidth should be specified as \"<min>-<max>\", error ...");
	    xmlFreeDoc(doc);
	    return 0;
	  }
	} else if (strcasecmp(service_ptr->name, "Bandwidth_DEF") == 0) 
	  strncpy(newlink->bandwidth, key, REG_TXT_FIELD_LEN);
	else if (strcasecmp(service_ptr->name, "VLAN_Transport") == 0) {
	  profile->vlan_transport = parse_values(key, &(profile->vlan_transport_num));
	  if (!(profile->vlan_transport)) {
	    zlog_err("template_xml_parser: vlan_transport should be specified as \"<value1>|...|<valueN>, error ...");
	    xmlFreeDoc(doc);
	    return 0;
	  }
	}

	service_ptr = service_ptr->next;
      }
    } else if (strcasecmp(cur->name, "Default_Values") == 0) {
	service_ptr = cur;
	service_ptr = service_ptr->xmlChildrenNode;

	while (service_ptr) {
	  key = xmlNodeListGetString(doc, service_ptr->xmlChildrenNode, 1);
	
	  if (strcasecmp(service_ptr->name, "Bandwidth") == 0) 
	    strncpy(newlink->bandwidth, key, REG_TXT_FIELD_LEN);

	  service_ptr = service_ptr->next; 
	}
    }
  }

  xmlFreeDoc(doc);
  return 1;
}

int 
master_template_parser(int profile_id)
{
  char filename[100];

  /* if the linkprofile has already been parsed, simply return
   */
  if (linkprofile[profile_id])
    return 1;
  else
    linkprofile[profile_id] = malloc(sizeof(struct network_link));

  memset(linkprofile[profile_id], 0, sizeof(struct network_link));
 
  switch (profile_id) {
    case 1:
      strcpy(filename, XML_ETHERBASIC_FILE);
      break;
    case 2:
      strcpy(filename, XML_ETHERULTRA_FILE);
      break;
    case 3:
      strcpy(filename, XML_TDMBASIC_FILE);
      break;
    }   

  if (template_xml_parser(filename, linkprofile[profile_id]) == 0) 
    return 0;

  return 1;
}

/* parse service xml resource broker location file
 */
int
service_xml_parser(char* filename)
{
  xmlChar* key;
  xmlDocPtr doc;
  xmlNodePtr cur, node_ptr;
  char keyToFound[100];
  struct resource_agent *newagent;
  
  doc = xmlParseFile(filename);

  if (doc == NULL) {
    zlog_err("service_xml_parser: Document is not parsed successfully");
    return (0);
  }

  cur = xmlDocGetRootElement(doc);
  
  if (cur == NULL) {
    zlog_err("service_xml_parser: service_xml_parser: document is empty");
    xmlFreeDoc(doc);
    return (0);
  }

  /* first locate "brokers_definition"
   */
  strcpy(keyToFound, "brokers_definition");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("service_xml_parser: can't locate <%s> in the document", keyToFound);
    xmlFreeDoc(doc);
    return (0);
  }

  /* parse all the <resource> node
   */
  for (cur = cur->xmlChildrenNode;
       cur;
       cur = cur->next) {
    if (strcasecmp(cur->name, "broker") == 0) {
      newagent = malloc(sizeof(struct resource_agent));
      memset(newagent, 0, sizeof(struct resource_agent));

      node_ptr = cur;
      node_ptr = node_ptr->xmlChildrenNode;
      while (node_ptr) {
	key = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1);

	if (strcasecmp(node_ptr->name, "name") == 0) 
	  newagent->resource_type = key;
	else if (strcasecmp(node_ptr->name, "ipadd") == 0) 
	  newagent->add = key;
	else if (strcasecmp(node_ptr->name, "port") == 0)
	  newagent->port = atoi(key);

	node_ptr = node_ptr->next;
      }

      adtlist_add(&glob_agency_list, newagent);
    }
  }

  xmlFreeDoc(doc);
  return (1);
}

int
agent_final_parser(char* filename)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur;
  char keyToFound[100]; 
  int i;

  doc = xmlParseFile(filename);
  if (doc == NULL) {
    zlog_err("agent_final_parser: Document not parsed successfully.");
    return 0;
  }

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    zlog_err("agent_final_parser: Empty document");
    xmlFreeDoc(doc);
    return 0;
  }

  /* first locate "topology" keyword
   */
  strcpy(keyToFound, "topology");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("agent_final_parser: Can't locate <%s> in the document", keyToFound);
    xmlFreeDoc(doc);
    return 0;
  }
  for (cur = cur->xmlChildrenNode;
       cur;
       cur=cur->next) {

    if (strcasecmp(cur->name, "ast_func") == 0) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

      if (!key) {
	zlog_err("No <ast_func> specified\n");
	xmlFreeDoc(doc);
	return 0;
      }

      for (i = 1;
	   i < INVALID_INCOMING_VALUE;
	   i++) {
	if (strcasecmp(key, function_type_details[i]) == 0) {
	  glob_app_cfg.function = i;
	  break;
	}
      }
      if (i == INVALID_INCOMING_VALUE) {
	glob_app_cfg.function = INVALID_INCOMING_VALUE;
	zlog_err("Invalid <ast_func> found: %s", key);
	xmlFreeDoc(doc);
	return (0);
      }
    }

    if (strcasecmp(cur->name, "ast_status") == 0) {
	key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

	for (i = 0; key && i <= AST_APP_COMPLETE ; i++) {
	  if (strcasecmp(key, status_type_details[i]) == 0) {
	    glob_app_cfg.ast_status = i;
	    break;
	  }
	}
    } else if (strcasecmp(cur->name, "glob_ast_id") == 0) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

      glob_app_cfg.glob_ast_id = strdup(key);
    } else if (strcasecmp(cur->name, "ast_ip") == 0) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      glob_app_cfg.ast_ip = strdup(key);
    } 

    if (strcasecmp(cur->name, "resource") != 0) 
      continue;
  }

  xmlFreeDoc(doc);
  return 1;
}

/* parse xml and build internal representation;
 * parser_type:
 *	FULL_VERSION		1
 *	BRIEF_VERSION		2
 */
int
topo_xml_parser(char* filename, int parser_type)
{
  xmlChar *key, *type;
  xmlDocPtr doc;
  xmlNodePtr cur, topo_ptr, node_ptr, link_ptr, resource_ptr;
  char keyToFound[100]; 
  struct node_cfg *newnode, *curnode;
  struct link_cfg *newlink;
  struct adtlistnode *curlistnode;
  struct adtlist *node_list, *link_list;
  int i, j, found, found_type, found_src;

  node_list = NULL;
  link_list = NULL;

  doc = xmlParseFile(filename);
 
  if (doc == NULL) {
    zlog_err("topo_xml_parser: Document not parsed successfully.");
    return 0;
  }

  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    zlog_err("topo_xml_parser: Empty document");
    xmlFreeDoc(doc);
    return 0;
  }

  /* first locate "topology" keyword
   */
  strcpy(keyToFound, "topology");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("topo_xml_parser: Can't locate <%s> in the document", keyToFound);
    xmlFreeDoc(doc);
    return 0;
  }
  topo_ptr = cur;

  /* parse all the nodes 
   * since cur is now pointing to the <nodes> xmlnode, 
   * set it to point to the child level; then, go through all
   * the nodes in the child level and dump the extract nodes into 
   * node_list
   */
  for (cur = cur->xmlChildrenNode, found = 0;
       cur;
       cur=cur->next) {

    if (strcasecmp(cur->name, "ast_func") == 0) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

      if (!key) {
	zlog_err("No <ast_func> specified\n");
	xmlFreeDoc(doc);
	return 0;
      }

      for (i = 1;
	   i < INVALID_INCOMING_VALUE;
	   i++) {
	if (strcasecmp(key, function_type_details[i]) == 0) {
	  glob_app_cfg.function = i;
	  break;
	}
      }
      if (i == INVALID_INCOMING_VALUE) {
	glob_app_cfg.function = INVALID_INCOMING_VALUE;
	zlog_err("Invalid <ast_func> found: %s", key);
	xmlFreeDoc(doc);
	return (0);
      }
    }

    if (strcasecmp(cur->name, "ast_status") == 0) {
	key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

	for (i = 0; key && i <= AST_APP_COMPLETE ; i++) {
	  if (strcasecmp(key, status_type_details[i]) == 0) {
	    glob_app_cfg.ast_status = i;
	    break;
	  }
	}
    } else if (strcasecmp(cur->name, "glob_ast_id") == 0) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

      glob_app_cfg.glob_ast_id = strdup(key);
    } else if (strcasecmp(cur->name, "ast_ip") == 0) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      glob_app_cfg.ast_ip = strdup(key);
    } else if (strcasecmp(cur->name, "details") == 0) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      strncpy(glob_app_cfg.details, key, 200-1);
    }

    if (strcasecmp(cur->name, "resource") != 0) 
      continue;

    /* First, we need to know what kind of resource this is:
     * link or node by looking at its <resource_type> keyword
     */
    resource_ptr = cur;
    
    for (node_ptr = cur->xmlChildrenNode, found_type = 0, found_src = 0, type = NULL; 
	 node_ptr; 
	 node_ptr = node_ptr->next) {

      if (strcasecmp(node_ptr->name, "resource_type") == 0) {
	found_type = 1;
	type = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1);
      } else if (strcasecmp(node_ptr->name, "src") == 0) 
	found_src = 1;
    }

    if (!found_type && parser_type == FULL_VERSION && glob_app_cfg.function == SETUP_REQ) {
  
      /* without the <resource_type> tab, this resource is invalid
       * error!
       */
      zlog_warn("topo_xml_parser: Can't locate <resource_type> for the resource");
    }

    if (!found_src) {
      newnode = malloc(sizeof(struct node_cfg));
      memset(newnode, 0, sizeof(struct node_cfg));

      for (i = 0, found = 0;
	   i <= NUM_NODE_TYPE && !found && found_type; 
	   i++) {

	if (strcmp(type, entity_type_name[i]) == 0 ) {
	  found = 1;

	  newnode->node_type = i;
	}
      }

      for (node_ptr = resource_ptr->xmlChildrenNode;
	   node_ptr;
	   node_ptr = node_ptr->next) {
	key = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1); 

	if (!key)
	  continue;
	if (strcasecmp(node_ptr->name, "name") == 0) 
	  strncpy(newnode->name, key, NODENAME_MAXLEN); 
	else if (strcasecmp(node_ptr->name, "ip_addr") == 0) 
	  strncpy(newnode->ipadd, key, IP_MAXLEN); 
	else if (strcasecmp(node_ptr->name, "te_addr") == 0)
	  strncpy(newnode->te_addr, key, IP_MAXLEN);
	else if (strcasecmp(node_ptr->name, "ast_status") == 0) {
	  for (j = 0; key && j <= AST_APP_COMPLETE ; j++) {
	    if (strcasecmp(key, status_type_details[j]) == 0) {
	      newnode->ast_status = j;
	      break; 
	    } 
	  }
	} else if (strcasecmp(node_ptr->name, "agent_message") == 0)
	  newnode->agent_message = strdup(key);
	else if (strcasecmp(node_ptr->name, "command") == 0)
	  newnode->command = strdup(key);
      } 

      if (node_list == NULL) { 
	node_list = malloc(sizeof(struct adtlist)); 
	memset(node_list, 0, sizeof(struct adtlist)); 
      } 

      adtlist_add(node_list, newnode); 
    } else {

      newlink = malloc(sizeof(struct link_cfg));
      memset(newlink, 0, sizeof(struct link_cfg));

      for (i = 0;
	   i <= NUM_LINK_TYPE && found_type;
	   i++) {

	/* if this is a query request, we can simply ignore all link resource in the file
	 */
	if (strcmp(type, link_type_name[i]) == 0) {
	  found_type = 1; 

	  newlink->service_type = i;

	  /* after we have already determined what kind of service this 
	   * link provides, we need to read the template for this 
	   * particular service type if it hasn't been read yet 
	   */ 
	  if (master_template_parser(i) == 0 && parser_type == FULL_VERSION) 
	    zlog_err("topo_xml_parser: can't parse the file for \"%s\"", link_type_name[i]); 
	}
      }
 
      /* remember, cur is still pointing to the XML node
       * that has "resource" as keyword
       */
      for (link_ptr= cur->xmlChildrenNode;
	   link_ptr;
	   link_ptr = link_ptr->next) { 
	key = xmlNodeListGetString(doc, link_ptr->xmlChildrenNode, 1); 
	if (!key)
	  continue;

	if (strcasecmp(link_ptr->name, "src") == 0 ||
	    strcasecmp(link_ptr->name, "dest") == 0) { 
      
	  /* src_node may use either <name> or <add> to denote the 
	   * node 
	   */ 
	  for (curlistnode = node_list->head; 
      	   curlistnode; 
       	   curlistnode = curlistnode->next) { 

	    curnode = (struct node_cfg*)(curlistnode->data); 
	    if (strcasecmp(key, curnode->name) == 0 || 
		strcasecmp(key, curnode->ipadd) == 0) { 

	      if (strcasecmp(link_ptr->name, "src") == 0) 
		newlink->src = curnode; 
	      else 
		newlink->dest = curnode; 
	    } 
	  } 
	} else if (strcasecmp(link_ptr->name, "name") == 0) 
	  strncpy(newlink->name, key, NODENAME_MAXLEN); 
	else if (strcasecmp(link_ptr->name, "bandwidth") == 0) 
	  strncpy(newlink->bandwidth, key, REG_TXT_FIELD_LEN); 
	else if (strcasecmp(link_ptr->name, "swcap") == 0) 
	  strncpy(newlink->swcap, key, REG_TXT_FIELD_LEN); 
	else if (strcasecmp(link_ptr->name, "encoding") == 0) 
	  strncpy(newlink->encoding, key, REG_TXT_FIELD_LEN); 
	else if (strcasecmp(link_ptr->name, "gpid") == 0) 
	  strncpy(newlink->gpid, key, REG_TXT_FIELD_LEN); 
	else if (strcasecmp(link_ptr->name, "lsp_name") == 0 && 
      	     glob_app_cfg.function != SETUP_REQ ) 
	  strncpy(newlink->lsp_name, key, LSP_NAME_LEN);
	else if (strcasecmp(link_ptr->name, "src_local_id_type") == 0)
	  strncpy(newlink->src_local_id_type, key, REG_TXT_FIELD_LEN);
	else if (strcasecmp(link_ptr->name, "dest_local_id_type") == 0)
	  strncpy(newlink->dest_local_id_type, key, REG_TXT_FIELD_LEN);
	else if (strcasecmp(link_ptr->name, "src_local_id") == 0)
	  newlink->src_local_id = atoi(key);
	else if (strcasecmp(link_ptr->name, "dest_local_id") == 0)
	  newlink->dest_local_id = atoi(key);
	else if (strcasecmp(link_ptr->name, "vtag") == 0) 
	  strncpy(newlink->vtag, key, REG_TXT_FIELD_LEN);
	else if (strcasecmp(link_ptr->name, "src_ip") == 0)
	  strncpy(newlink->src_ip, key, IP_MAXLEN);
	else if (strcasecmp(link_ptr->name, "dest_ip") == 0)
	  strncpy(newlink->dest_ip, key, IP_MAXLEN);
	else if (strcasecmp(link_ptr->name, "ast_status") == 0) {
	  for (j = 0; key && j <= AST_APP_COMPLETE; j++) {
	    if (strcasecmp(key, status_type_details[j]) == 0) {
	      newlink->ast_status = j;
	      break; 
	    } 
	  }
	} else if (strcasecmp(link_ptr->name, "agent_message") == 0) 
	  newlink->agent_message = strdup(key);
      } 

      if (link_list == NULL) { 
	link_list = malloc(sizeof(struct adtlist)); 
	memset(link_list, 0, sizeof(struct adtlist)); 
      } 

      adtlist_add(link_list, newlink);    

      if (glob_app_cfg.function != QUERY_RESP) { 
	/* also add the link the to src node */ 
	if (newlink->src->link_list == NULL) { 
	  newlink->src->link_list = malloc(sizeof(struct adtlist)); 
	  memset(newlink->src->link_list, 0, sizeof(struct adtlist)); 
	} 
	adtlist_add(newlink->src->link_list, newlink); 
      }
    } 
  }

  glob_app_cfg.node_list = node_list;
  glob_app_cfg.link_list = link_list;

  xmlFreeDoc(doc);

  return 1;
}

#ifdef RESOURCE_BROKER
int 
master_locate_resource()
{
  int i, found;
  struct adtlistnode *curnode, *curnode1;
  struct node_cfg *mynode, *newnode;
  struct resource_agent *myagency;
  int sock;
  struct sockaddr_in echoServAddr;
  char buffer[501];
  int bytesRcvd;
  FILE *fp;
  struct application_cfg working_app_cfg; 
  
  for ( i = 1, curnode = glob_app_cfg.node_list->head;
	curnode;
	i++, curnode = curnode->next) {
    mynode = (struct node_cfg*)(curnode->data);
    if (mynode->ipadd[0] != '\0') 
      continue;

    if (mynode->node_type == 0) {
      zlog_err("Unknown resource type; don't know how to locate node %s", mynode->name);
      return 0;
    }

    zlog_info("Trying to locate node resource %s: %s", mynode->name, entity_type_name[mynode->node_type]);

    for ( found = 0, curnode1 = glob_agency_list.head;
	  curnode1 && !found;
	  curnode1 = curnode1->next ) {
      myagency = (struct resource_agent*)(curnode1->data);
 
      if (strcasecmp(entity_type_name[mynode->node_type], myagency->resource_type) == 0)
	found = 1;
    }
  
    if (!found) { 
      zlog_err("master_locate_resource: undefined resource type \"%s\"", 
	     entity_type_name[mynode->node_type]); 
      return 0;
    }

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
      zlog_err("master_locate_resource: socket() failed");
      return 0;
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(myagency->add);
    echoServAddr.sin_port = htons(myagency->port);

    if (connect(sock, (struct sockaddr*)&echoServAddr, sizeof(echoServAddr)) < 0) {
      zlog_err("master_locate_resource: connect() failed to %s port %d", myagency->add, myagency->port);
      return 0;
    }

    if (send(sock, "testing", 7, 0) != 7) {
      zlog_err("master_locate_resource: sendto() failed to %s port %d", myagency-> add, myagency->port);
      return 0;
    }

    if ((bytesRcvd = recv(sock, buffer, 500, 0))  > 0) {
      buffer[bytesRcvd] = '\0';
      unlink(RESOURCE_REC);
      fp = fopen(RESOURCE_REC, "w+");
      fprintf(fp, buffer);
      fflush(fp);
      fclose(fp);

      memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
      memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
      if (topo_xml_parser(RESOURCE_REC, BRIEF_VERSION) == 0 ||
	  glob_app_cfg.node_list == NULL) {
	zlog_err("Invalid response from resource broker");
        close(sock);
        free_application_cfg(&glob_app_cfg);
	memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
        return 0;
      }
     
      newnode = (struct node_cfg*) glob_app_cfg.node_list->head->data; 
      if (newnode->ipadd[0] == '\0' ||
	  newnode->te_addr[0] == '\0') {
	zlog_err("Invalid response from resource broker");
	close(sock);
	free_application_cfg(&glob_app_cfg);
	memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
	return 0;
      }

      strncpy(mynode->ipadd, newnode->ipadd, IP_MAXLEN);
      strncpy(mynode->te_addr, newnode->te_addr, IP_MAXLEN);
      zlog_info("location for node_type %s: %s",
	     entity_type_name[mynode->node_type], mynode->name);
      zlog_info("\tip addr: %s", mynode->ipadd);
      zlog_info("\tte addr: %s", mynode->te_addr);

      free_application_cfg(&glob_app_cfg);
      memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    } else {
      zlog_err("No response from resource broker");
      close(sock);
      return 0;
    }

    close(sock);
  }

  return 1;
}
#endif

/* validate the graph
 */
int
master_validate_graph(int is_astb)
{
  struct adtlistnode *curnode, *curnode1;
  struct node_cfg *mynode, *mynode1;
  struct link_cfg *mylink, *mylink1;
  int i;

  if (is_astb && (glob_app_cfg.function == SETUP_RESP ||
			 glob_app_cfg.function == RELEASE_RESP ||
			 glob_app_cfg.function == AST_COMPLETE ||
			 glob_app_cfg.function == QUERY_RESP)) {
    zlog_err("Invalid ast_func defined for this topology");
    printf("Valide Values are:\n");
    printf("SETUP_REQ, RELEASE_REQ, APP_COMPLETE, QUERY_REQ\n");
    return 0;
  }

  if (is_astb && glob_app_cfg.function == SETUP_REQ &&
	glob_app_cfg.link_list == NULL) {
    printf("For SETUP_RESP, there should be at least 1 link in topology file\n");
    return 0;
  }

  if (glob_app_cfg.link_list != NULL) {
    /* all link should have both src and dest
     */
    for (curnode = glob_app_cfg.link_list->head;
	 curnode;
	 curnode = curnode->next) {
      mylink = (struct link_cfg*) curnode->data;
      if ( glob_app_cfg.function == SETUP_REQ && (!mylink->src || !mylink->dest) ) {
	zlog_err("link's src or dest is not set");
	return 0;
      } 
      if (mylink->name[0] == '\0' && glob_app_cfg.function != QUERY_RESP) {
	zlog_err("link's name is not set");
	return 0;
      }
      if (mylink->src_local_id_type[0] != '\0') {
	for (i = 0; i < local_field.number; i++) {
	  if (strcasecmp(mylink->src_local_id_type, local_field.ss[i].abbre) == 0)
	    break;
	}
	if (i == local_field.number) {
	  zlog_err("Invalid value for local: %s", mylink->src_local_id_type);
	  if (is_astb) {
	    printf("Valid values for local:\n");
	    for (i = 0; i < local_field.number; i++)
	      printf("%s\t:%s\n", local_field.ss[i].abbre, local_field.ss[i].details);
	  }
	  return 0;
	}
	if (mylink->src_local_id == -1) {
	   zlog_err("link's src_local_id is not set"); 
	   return 0;
	}
      } 
      if (mylink->dest_local_id_type[0] != '\0') {
	for (i = 0; i < local_field.number; i++) {
	  if (strcasecmp(mylink->dest_local_id_type, local_field.ss[i].abbre) == 0) 
	    break;
	}   
	if (i == local_field.number) {
	  zlog_err("Invalid value for local: %s", mylink->dest_local_id_type);
	  if (is_astb) { 
	    printf("Valid values for local:\n");
	    for (i = 0; i < local_field.number; i++)
	      printf("%s\t:%s\n", local_field.ss[i].abbre, local_field.ss[i].details);
	  }
	  return 0;
	}
	if (mylink->dest_local_id == -1) {
	  zlog_err("link's dest_local_id is not set");
	  return 0;
	}
      } 
      if (mylink->bandwidth[0] != '\0') {
	for (i = 0; i < bandwidth_field.number; i++) {
	  if (strcasecmp(mylink->bandwidth, bandwidth_field.ss[i].abbre) == 0)
	    break;
	}
	if (i == bandwidth_field.number) {
	  zlog_err("Invalid value for bandwidth: %s", mylink->bandwidth);
	  if (is_astb) {
	    printf("Valid values for bandwidth:\n"); 
	    for (i = 0; i < bandwidth_field.number; i++) 
	      printf("%s\t:%s\n", bandwidth_field.ss[i].abbre, bandwidth_field.ss[i].details);
	  }
	  return 0;
	}
      }
      if (mylink->swcap[0] != '\0') {
	for (i = 0; i < swcap_field.number; i++) {
	  if (strcasecmp(mylink->swcap, swcap_field.ss[i].abbre) == 0)
	    break;
	}
	if (i == swcap_field.number) {
	  zlog_err("Invalid value for swcap: %s", mylink->swcap);
	  if (is_astb) {
	    printf("Valid values for swcap:\n");
	    for (i = 0; i < swcap_field.number; i++)
	      printf("%s\t:%s\n", swcap_field.ss[i].abbre, swcap_field.ss[i].details);
	  }
	  return 0;
	}
      } 
      if (mylink->gpid[0] != '\0') {
	for (i = 0; i < gpid_field.number; i++) {
	  if (strcasecmp(mylink->gpid, gpid_field.ss[i].abbre) == 0)
	    break;
	}
	if (i == gpid_field.number) {
	  zlog_err("Invalid value for gpid: %s", mylink->gpid);
	  if (is_astb) {
	    printf("Valid values for gpid:\n");
	    for (i = 0; i < gpid_field.number; i++)
	      printf("%s\t:%s\n", gpid_field.ss[i].abbre, gpid_field.ss[i].details);
	  }
	  return 0;
	}
      } 
      if (mylink->encoding[0]  != '\0') {
	for (i = 0; i < encoding_field.number; i++) {
	  if (strcasecmp(mylink->encoding, encoding_field.ss[i].abbre) == 0)
	    break;
	}
	if (i == encoding_field.number) {
	  zlog_err("Invalid value for encoding: %s", mylink->encoding);
	  if (is_astb) {
	    printf("Valid values for encoding:\n");
	    for (i = 0; i < encoding_field.number; i++)
	      printf("%s\t:%s\n", encoding_field.ss[i].abbre, encoding_field.ss[i].details);
	  }
	  return 0;
	}
      }
     
      for (curnode1 = curnode->next;
	   curnode1;
	   curnode1 = curnode1->next) {
	mylink1 = (struct link_cfg*) curnode1->data;
	if (strcmp(mylink->name, mylink1->name) == 0) {
	  zlog_err("Link's name should be unique among all links");
	  return 0;
	}
      }
    }
  }
  
  if (glob_app_cfg.node_list != NULL && glob_app_cfg.function != QUERY_RESP) {
    for (curnode = glob_app_cfg.node_list->head;
	 curnode;
	 curnode = curnode->next) {
      mynode = (struct node_cfg*) curnode->data;

      if (mynode->name[0] == '\0') {
	zlog_err("node's name is not set");
	return 0;
      } else {
	for (curnode1 = curnode->next;
	     curnode1;
	     curnode1 = curnode1->next) {
	  mynode1 = (struct node_cfg*) curnode1->data;
	  if (strcmp(mynode->name, mynode1->name) == 0) {
	    zlog_err("Node's name should be unique among all nodes");
	    return 0;
	  }
	}
      }
    }
  }

  if (glob_app_cfg.function != SETUP_REQ &&
      glob_app_cfg.function != QUERY_REQ) {
    if (glob_app_cfg.glob_ast_id == NULL) {
      zlog_err("glob_ast_id is not set");
      return 0;
    }
  }

  return 1;
}
