#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "libxml/relaxng.h"
#include "ast_master_ext.h"
#include "buffer.h"
#include "log.h"
#include "local_id_cfg.h"

int master_recv_alarm = 0;
struct res_defs all_res;
struct res_mods all_res_mod;
/* variables for the minions related lib functions */
int glob_minion;
char *glob_minion_ret_xml = NULL;
char *glob_minion_recv_xml = NULL;

xmlRelaxNGValidCtxtPtr incoming_xml_ctxt = NULL;

char *status_type_details[] =
  { "ast_unknown", "ast_success", "ast_failure", "ast_pending",
    "ast_ast_complete", "ast_app_complete" };

char *action_type_details[] =
  { "unknown", "setup_req", "setup_resp", "ast_complete",
    "app_complete", "release_req", "release_resp", "query_req",
    "query_resp", "update_req", "update_resp"};

struct application_cfg* master_final_parser(char*, int);

int 
init_resource()
{
  xmlDocPtr xml_doc;
  xmlRelaxNGValidCtxtPtr ctxt;
  xmlNodePtr cur, cur1;
  struct in_addr ip;
  unsigned short port, i;
  struct res_def *def;
  struct broker *broker;
  struct _xmlAttr* attr;
 
  ctxt = build_RNGValidCtxt("/usr/local/ast_file/schema/resource_def.rng"); 
  if (!ctxt) {
    zlog_err("init_resource(): ctxt NULL");
    return 1;
  }

  xml_doc = xmlParseFile("/usr/local/ast_file/res_def.xml");
  if (!xml_doc) {
    zlog_err("init_resource(): xml_doc NULL");
    return 1;
  }
  if (xmlRelaxNGValidateDoc(ctxt, xml_doc)) {
    zlog_err("init_resource(): invalid res_def file");
    return 1;
  }
 
  memset(&all_res, 0, sizeof(struct res_defs));
 
  cur = xmlDocGetRootElement(xml_doc);
  for (cur = cur->xmlChildrenNode;
	cur;
	cur = cur->next) {

    if (strcasecmp((char*)cur->name, "resource") != 0)
      continue;

    def = (struct res_def*)malloc(sizeof(struct res_def));    
    memset(def, 0, sizeof(struct res_def));

    for (attr = cur->properties;
	 attr;
	 attr=attr->next) {
      if (strcasecmp((char*)attr->name, "res_type") == 0) {
	if (strcasecmp((char*)attr->children->content, "node") == 0) 
	  def->res_type = res_node;
	else
	  def->res_type = res_link;
      } else if (strcasecmp((char*)attr->name, "subtype") == 0)
	def->name=strdup((char*)attr->children->content);
    }

    for (cur1 = cur->xmlChildrenNode;
	 cur1;
	 cur1 = cur1->next) {

      if (strcasecmp((char*)cur1->name, "broker") == 0) {
	for (attr = cur1->properties;
	     attr;
	     attr=attr->next) {
	  if (strcasecmp((char*)attr->name, "ip") == 0) 
	    ip.s_addr = inet_addr((char*)attr->children->content); 
	  else if (strcasecmp((char*)attr->name, "port") == 0) 
	    port = atoi((char*)attr->children->content);
 	}
	if (ip.s_addr != -1) {
	  broker = (struct broker*)malloc(sizeof(struct broker));
	  broker->ip = ip;
	  broker->port = port;
	  if (!def->broker_list) {
	    def->broker_list = (struct adtlist*)malloc(sizeof(struct adtlist));
	    memset(def->broker_list, 0, sizeof(struct adtlist));
	  }
	  adtlist_add(def->broker_list, broker);
	}
      } else if (strcasecmp((char*)cur1->name, "schema") == 0) {
	def->schema = strdup((char*)xmlNodeListGetString(xml_doc, cur1->xmlChildrenNode, 1));
        def->schema_ctxt = build_RelaxNGPtr(def->schema);
      } else if (strcasecmp((char*)cur1->name, "agent") == 0) {
        def->agent_ip.s_addr = -1;
	for (attr = cur1->properties;
	     attr; 
	     attr=attr->next) {
	  if (strcasecmp((char*)attr->name, "ip") == 0)
	    def->agent_ip.s_addr = inet_addr((char*)attr->children->content);
	  else if (strcasecmp((char*)attr->name, "port") == 0)
	    def->agent_port = atoi((char*)attr->children->content);
 	}
      }
    }

    for (i = 0; i < all_res_mod.total; i++) {
      if (strcasecmp(all_res_mod.res[i]->name, def->name) == 0 &&
	all_res_mod.res[i]->res_type == def->res_type) 
      def->mod = all_res_mod.res[i];
    }
  
    if (def->res_type == res_node) 
      adtlist_add(&all_res.node_list, def);
    else 
      adtlist_add(&all_res.link_list, def);
  }
  xmlFreeDoc(xml_doc);

  if (!all_res.node_list.count && !all_res.link_list.count)
    return 1;

  return 0;
}

static xmlNodePtr
cutxmlnode(xmlNodePtr cur, 
	   char* keyToFound) 
{
  xmlNodePtr prev = NULL, parent = cur;
  if (strcasecmp((char*)cur->name, "resource") != 0)
    return NULL;
 
  for (cur=cur->children; 
	cur; 
	prev = cur, cur = cur->next) {
    if (strcasecmp((char*)cur->name, keyToFound) == 0) {
      if (!prev) 
	parent->children = cur->next;
      else 
	prev->next = cur->next;
      if (cur->next)
	cur->next->prev = cur->prev;	
   
      cur->parent = NULL;
      cur->doc = NULL;
      cur->prev = NULL;
      cur->next = NULL;
      return cur;
    }
  }
 
  return NULL;
}

struct res_def *
search_res_type(enum resource_type res_type, 
	        char* name)
{
  struct adtlist *res_list;
  struct adtlistnode *cur;
  struct res_def *def;

  res_list = (res_type == res_node)? &(all_res.node_list) : &(all_res.link_list);
  for (cur = res_list->head;
	cur;
	cur = cur->next) {
    def = (struct res_def*) cur->data;

    if (strcasecmp(def->name, name) == 0) 
      return def;
  }

  return NULL;
}

xmlRelaxNGPtr
build_RelaxNGPtr(char* filename)
{
  xmlDocPtr ng_doc;
  xmlRelaxNGParserCtxtPtr parserCtxt;
  xmlRelaxNGPtr schema;

  if (strncmp(filename, "http", 4) == 0) 
    parserCtxt = xmlRelaxNGNewParserCtxt(filename);
  else { 
    ng_doc = xmlParseFile(filename); 
    if (!ng_doc) { 
      zlog_err("build_RelaxNGPtr(): ng_doc NULL"); 
      return NULL; 
    } 
    parserCtxt = xmlRelaxNGNewDocParserCtxt(ng_doc); 
    xmlFreeDoc(ng_doc);
  }

  if (!parserCtxt) {
    zlog_err("build_RelaxNGPtr(): parserCtxt NULL");
    return NULL;
  }
  schema = xmlRelaxNGParse(parserCtxt);
  if (!schema) {
    xmlRelaxNGFreeParserCtxt(parserCtxt);
    zlog_err("build_RelaxNGPtr(): schema NULL");
    return NULL;
  }

  return schema;
}

xmlRelaxNGValidCtxtPtr
build_RNGValidCtxt(char* filename)
{
  xmlDocPtr ng_doc;
  xmlRelaxNGParserCtxtPtr parserCtxt;
  xmlRelaxNGPtr schema;
  xmlRelaxNGValidCtxtPtr ctxt;

  if (strncmp(filename, "http", 4) == 0) 
    parserCtxt = xmlRelaxNGNewParserCtxt(filename);
  else { 
    ng_doc = xmlParseFile(filename); 
    if (!ng_doc) { 
      zlog_err("build_RNGValidCtxt(): ng_doc NULL"); 
      return NULL; 
    } 
    parserCtxt = xmlRelaxNGNewDocParserCtxt(ng_doc); 
    xmlFreeDoc(ng_doc);
  }

  if (!parserCtxt) {
    zlog_err("build_RNGValidCtxt(): parserCtxt NULL");
    return NULL;
  }
  schema = xmlRelaxNGParse(parserCtxt);
  if (!schema) {
    xmlRelaxNGFreeParserCtxt(parserCtxt);
    zlog_err("build_RNGValidCtxt(): schema NULL");
    return NULL;
  }
  ctxt = xmlRelaxNGNewValidCtxt(schema);
  xmlRelaxNGFreeParserCtxt(parserCtxt);
  if (!ctxt) 
    xmlRelaxNGFree(schema); 

  return ctxt;
}

xmlRelaxNGValidCtxtPtr
init_schema(char *file)
{
  incoming_xml_ctxt = build_RNGValidCtxt(file);

  return incoming_xml_ctxt;
}

void 
set_allres_fail(char* error_msg)
{
  struct adtlistnode *curnode;
  struct resource *myres;

  glob_app_cfg->status = ast_failure;
  if (error_msg) {
    strcpy(glob_app_cfg->details, error_msg);
    zlog_err(error_msg);
  }
  if (glob_app_cfg->node_list) {
    for (curnode = glob_app_cfg->node_list->head;
         curnode;
         curnode = curnode->next) {
      myres = (struct resource*) curnode->data;
      myres->status = ast_failure;
    }
  }
  if (glob_app_cfg->link_list) {
    for (curnode = glob_app_cfg->link_list->head;
         curnode;
         curnode = curnode->next) {
      myres = (struct resource*) curnode->data;
      myres->status = ast_failure;
    }
  }
}

struct application_cfg*
search_cfg_in_list(char* ast_id)
{
  struct adtlistnode *curnode;
  struct application_cfg *curcfg;

  if (!ast_id)
    return NULL;

  for (curnode = app_list.head;
        curnode;
        curnode = curnode->next) {
    curcfg = (struct application_cfg*)curnode->data;

    if (strcmp(curcfg->ast_id, ast_id) == 0)
      return curcfg;
  }

  return NULL;
}

void
add_cfg_to_list()
{
  struct adtlistnode *curnode;
  struct application_cfg *curcfg;

  if (!glob_app_cfg || !glob_app_cfg->ast_id)
    return;

  curcfg = search_cfg_in_list(glob_app_cfg->ast_id);

  if (!curcfg)
    adtlist_add(&app_list, glob_app_cfg);
  else if (curcfg != glob_app_cfg) {

    zlog_warn("Finding another copy of <ast_id>:%s in app_list", glob_app_cfg->ast_id);

    for (curnode = app_list.head;
         curnode;
         curnode = curnode->next) {
      if (curcfg == curnode->data) {
        free_application_cfg(curcfg);
        curnode->data = glob_app_cfg;
      }
    }
  }
}

void
del_cfg_from_list(struct application_cfg* app_cfg)
{
  struct adtlistnode *curnode, *prevnode;
  if (!app_cfg) 
    return;

  zlog_info("Deleting %s from app_list", app_cfg->ast_id);

  for (prevnode = NULL, curnode = app_list.head;
       curnode;
       prevnode = curnode, curnode = curnode->next) {
    if (app_cfg == curnode->data) {

      if (prevnode) 
        prevnode->next = curnode->next;
      else
        app_list.head = curnode->next;

      if (app_list.tail == curnode) 
        app_list.tail = prevnode;

      app_list.count--;
      free_application_cfg(app_cfg); 
      free(curnode);

      return;
    } 
  }
}

struct application_cfg* 
retrieve_app_cfg(char* ast_id, 
		 int agent)
{
  char path[100];
  struct application_cfg *ret = NULL;
  struct stat fs;

  if (!ast_id)
    return NULL;

  ret = search_cfg_in_list(ast_id);
  if (ret)
    return ret;

  if (agent == MASTER) 
    sprintf(path, "%s/%s/final.xml", AST_DIR, ast_id);
  else if (agent == NODE_AGENT)
    sprintf(path, "%s/%s/final.xml", NODE_AGENT_DIR, ast_id); 
  else if (agent == LINK_AGENT) 
    sprintf(path, "%s/%s/final.xml", LINK_AGENT_DIR, ast_id);
  else 
    return NULL;

  if (stat(path, &fs) == -1) 
    return NULL;

  if (agent == MASTER) 
    ret = master_final_parser(path, MASTER);       
  else 
    ret = agent_final_parser(path);

  ret->clnt_sock = -1;
  return ret;
}

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

int
get_status_by_str(char* key)
{
  int i;

  for (i = 0; key && i <= ast_app_complete ; i++) 
    if (strcasecmp(key, status_type_details[i]) == 0) 
      return i;

  return 0;
}

int 
get_action_by_str(char* key)
{
  int i;

  for (i = 0; key && i <= update_resp; i++) 
    if (strcasecmp(key, action_type_details[i]) == 0) 
      return i;

  return 0;
}

struct resource *
search_res_by_name(struct application_cfg *app_cfg, 
		   enum resource_type res_type, 
		   char* str)
{
  struct adtlist *res_list;
  struct adtlistnode *curnode;
  struct resource *res;
  
  if (!app_cfg || !str)
    return NULL;

  res_list = (res_type == res_node )? app_cfg->node_list:app_cfg->link_list;
  if (!res_list)
    return NULL;

  for (curnode = res_list->head;
       curnode;
       curnode = curnode->next) {
    res = (struct resource*) curnode->data;

    if (strcasecmp(res->name, str) == 0)
      return res;
  }

  return NULL;
}

void
free_id_cfg_res(struct id_cfg_res* ptr)
{
  struct adtlistnode* curnode;
  struct local_id_cfg *id;

  if (!ptr)
    return;

  if (ptr->cfg_list) {
    for (curnode = ptr->cfg_list->head;
         curnode;
         curnode = curnode->next) {
      id = (struct local_id_cfg*) curnode->data;

      if (id->mems)
        free(id->mems);
      if (id->msg)
        free(id->msg);
      free (id);
      curnode->data = NULL;
    }
  }
  adtlist_free(ptr->cfg_list);

  if (ptr->msg)
    free(ptr->msg);
  free(ptr);
}

void
free_application_cfg(struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode;
  
  if (!app_cfg)
    return;

  switch (app_cfg->xml_type) {

  case TOPO_XML:

  free_res_list(app_cfg->node_list);
  free_res_list(app_cfg->link_list);

  if (app_cfg->ast_id) 
    free(app_cfg->ast_id);

  break;

  case ID_XML:
  case ID_QUERY_XML:
 
  if (app_cfg->node_list) {
    for (curnode = app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      free_id_cfg_res(curnode->data);
      curnode->data = NULL;
    }
  }
  adtlist_free(app_cfg->node_list);
  if (app_cfg->ast_id)
    free(app_cfg->ast_id);
    
  break;

  }
  free(app_cfg);
}

int
recv_file(int servSock, char* path, int buffersize, int to, int agent)
{
  FILE *ret_file = NULL;
  int bytesRcvd, ret_value = 0, total = 0;
  char buffer[buffersize];

  if (servSock == -1 || !path)
    return 1;

  total = 0;
  ret_file = fopen(path, "w");
  if (!ret_file) {
    zlog_err("recv_file(): can't open %s", path);
    return 1;
  }
  master_recv_alarm = 1;
  alarm(to);
  errno = 0;
  while ((bytesRcvd = recv(servSock, buffer, buffersize-1, 0)) >0 ) {
    if (errno == EINTR) {
      /* alarm went off
       */
      zlog_info("client: alarm went off");
      buffer[bytesRcvd]='\0';
      fprintf(ret_file, "%s", buffer);
      if (agent == ASTB)
        printf(buffer);
      break;
    }
    total+=bytesRcvd;
    buffer[bytesRcvd] = '\0';
    fprintf(ret_file, buffer);
    if (agent == ASTB)
      printf(buffer);
    alarm(to);
  }
  alarm(0);
  master_recv_alarm = 0;

  if (!ret_value) {
    fflush(ret_file); 
    fclose(ret_file);
  }

  return ret_value; 
}

int 
send_file_over_sock(int sock, char* newpath)
{
  int fd, total;
#ifndef __FreeBSD__
  static struct stat file_stat;
#endif

  if (sock < 0 || !newpath)
    return 1;
  
  fd = open(newpath, O_RDONLY);

  if (fd == -1) 
    return 1;

#ifdef __FreeBSD__
  total = sendfile(fd, sock, 0, 0, NULL, NULL, 0);
  if (total < 0) {
#else
  if (fstat(fd, &file_stat) == -1) {
    zlog_err("send_file_over_sock: fstat() failed on %s", newpath);
    close(sock);
    close(fd);
    return (1);
  }
  total = sendfile(sock, fd, 0, file_stat.st_size);
  if (total < 0) {
#endif
    zlog_err("send_file_over_sock: sendfile() failed; %d(%s)", errno, strerror(errno));
    close(sock);
    close(fd);
    return (1);
  }

  close(fd);
  return 0;
}

int
send_file_to_agent(char *ipadd, int port, char *newpath)
{
  int sock;
  struct sockaddr_in servAddr;
  int fd;
#ifndef __FreeBSD__
  static struct stat file_stat;
#endif

  if (!ipadd || !newpath) 
    return -1;

  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    zlog_err("send_file_to_agent: socket() failed");
    return (-1);
  }

  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = inet_addr(ipadd);
  servAddr.sin_port = htons(port);

  if (connect(sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
    zlog_err("send_file_to_agent: connect() failed to %s port %d", ipadd, port);
    close(sock);
    return -1;
  }

  fd = open(newpath, O_RDONLY);

#ifdef __FreeBSD__
  if (sendfile(fd, sock, 0, 0, NULL, NULL, 0) < 0) {
#else
  if (fstat(fd, &file_stat) == -1) {
    zlog_err("send_file_to_agent: fstat() failed on %s", newpath);
    close(sock);
    close(fd);
    return (-1);
  }
  if (sendfile(sock, fd, 0, file_stat.st_size) < 0) {
#endif
    zlog_err("send_file_to_agent: sendfile() failed; %d(%s)", errno, strerror(errno));
    close(sock);
    close(fd);
    return (-1);
  }

  close(fd);
  return sock;
}

void
print_error_response(char *path)
{
  FILE *fp;

  if (!path)
    return;
  fp = fopen(path, "w+");
  if (!fp)
    return;

  if (!glob_app_cfg) {
    fprintf(fp, "<topology>\n");
    fprintf(fp, "<status>AST_FAILURE</status>\n");
    fprintf(fp, "<details>Incoming XML file can't be parsed successfully</details>\n");
    fprintf(fp, "</topology>\n");
    fflush(fp);
    fclose(fp);
    return;
  }

  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n",
		glob_app_cfg->ast_id,
  		action_type_details[glob_app_cfg->action]);
  fprintf(fp, "<status>AST_FAILURE</status>\n");
  if (glob_app_cfg->details[0] != '\0')
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg->details);
  fprintf(fp, "</topology>\n");
  fflush(fp);
  fclose(fp);
}

void 
print_res(FILE *fp, 
	  struct resource *res, 
	  int agent)
{
  if (!fp || !res) 
    return;

  fprintf(fp, "<resource res_type=\"%s\" subtype=\"%s\" name=\"%s\">\n",
		res->res_type == res_node ? "node":"link",
		res->subtype->name, 
		res->name);
  if (res->status)
    fprintf(fp, "\t<status>%s</status>\n", status_type_details[res->status]);
  if (res->agent_message)
    fprintf(fp, "\t<agent_message>%s</agent_message>\n", res->agent_message);
  fprintf(fp, "\t<ip>%s</ip>\n", inet_ntoa(res->ip));
  if (res->res && res->subtype->mod && res->subtype->mod->print_func) 
    res->subtype->mod->print_func(fp, res->res, agent);
  else if (res->xml_node) 
    xmlElemDump(fp, NULL, res->xml_node);
  fprintf(fp, "</resource>\n");
}

void
print_res_list(FILE *fp, 
	       struct adtlist *res_list, 
	       int agent) 
{
  struct adtlistnode *curnode;

  if (!fp || !res_list)
    return;

  for ( curnode = res_list->head;
      curnode;
      curnode = curnode->next)
    print_res(fp, (struct resource*)curnode->data, agent);
}

void
free_res(struct resource* res)
{
  if (!res)
    return;

  if (res->agent_message)
    free(res->agent_message);
  
  if (res->res && res->subtype->mod && res->subtype->mod->free_func) 
    res->subtype->mod->free_func(res->res);
  else if (res->res) 
    free(res->res);
  
  if (res->xml_node)
    xmlFreeNode(res->xml_node);
}

void 
free_res_list(struct adtlist* res_list)
{
  struct adtlistnode *curnode;

  if (!res_list)
    return;

  for (curnode = res_list->head;
       curnode;
       curnode = curnode->next) {
    free_res(curnode->data);
    curnode->data = NULL;
  }
  adtlist_free(res_list);
}

void
print_xml_response(char* path, int agent)
{
  FILE* fp;

  if (!path)
    return;

  if (agent != LINK_AGENT && agent != NODE_AGENT)
    return;

  fp = fopen(path, "w+");
  if (!fp)
    return;

  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n", glob_app_cfg->ast_id, action_type_details[glob_app_cfg->action]);

  if (glob_app_cfg->ast_ip.s_addr != -1) 
    fprintf(fp, "<ast_ip>%s</ast_ip>\n", inet_ntoa(glob_app_cfg->ast_ip));
  if (glob_app_cfg->status)
    fprintf(fp, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);
  if (glob_app_cfg->details[0] != '\0')
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg->details);

  if (agent == LINK_AGENT)
    print_res_list(fp, glob_app_cfg->link_list, agent);
  if (agent == NODE_AGENT)
    print_res_list(fp, glob_app_cfg->node_list, agent);

  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);
}

void
print_final(char *path, int agent)
{
  FILE *fp;
  
  if (!path)
    return;

  fp = fopen(path, "w+");
  if (!fp)
    return;

  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n",  
		glob_app_cfg->ast_id, action_type_details[glob_app_cfg->action]);
  if (glob_app_cfg->status)
    fprintf(fp, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);
  if (glob_app_cfg->xml_file[0] != '\0') 
    fprintf(fp, "<xml_file>%s</xml_file>\n", glob_app_cfg->xml_file);
  if (glob_app_cfg->details[0] != '\0') 
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg->details);
  if (glob_app_cfg->ast_ip.s_addr != -1) 
    fprintf(fp, "<ast_ip>%s</ast_ip>\n", inet_ntoa(glob_app_cfg->ast_ip));

  print_res_list(fp, glob_app_cfg->node_list, agent);
  print_res_list(fp, glob_app_cfg->link_list, agent);

  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);
}

xmlNodePtr
findxmlnode(xmlNodePtr cur, char* keyToFound)
{
  if (strcasecmp((char*)cur->name, keyToFound) == 0)
    return cur;

  cur = cur->xmlChildrenNode;
  while (cur) {
    if (cur->xmlChildrenNode)  {
      if (strcasecmp((char*)cur->name, keyToFound) == 0) 
	return cur;
      return findxmlnode(cur, keyToFound);
    } 
    cur = cur->next;
  }

  return NULL;
}

int
xml_parser(char* filename)
{
  xmlDocPtr doc;
  xmlNodePtr cur;

  doc = xmlParseFile(filename);

  if (doc == NULL) {
    zlog_err("xml_parser: Invalid XML document");
    xmlFreeDoc(doc);
    return 0;
  }

  cur = xmlDocGetRootElement(doc);
  if (!cur) {
    zlog_err("xml_parser: Invalid XML document");
    xmlFreeDoc(doc);
    return 0;
  }

  if (findxmlnode(cur, "topology")) {
    xmlFreeDoc(doc);
    return TOPO_XML;
  }

  if (findxmlnode(cur, "local_id_cfg")) {
    xmlFreeDoc(doc);
    return ID_XML;
  }

  if (findxmlnode(cur, "local_id_query")) {
    xmlFreeDoc(doc);
    return ID_QUERY_XML;
  }

  if (findxmlnode(cur, "ast_ctrl")) {
    xmlFreeDoc(doc);
    return CTRL_XML;
  }

  zlog_err("xml_parser: xml file is neither <topology> nor <local_id_cfg> type")
;
  xmlFreeDoc(doc);
  return 0;
}

struct application_cfg*
agent_final_parser(char* filename)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur;
  char keyToFound[100]; 
  struct _xmlAttr* attr;
  struct application_cfg *app_cfg;

  doc = xmlParseFile(filename);
  if (doc == NULL) {
    zlog_err("agent_final_parser: Document not parsed successfully.");
    return NULL;
  }

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    zlog_err("agent_final_parser: Empty document");
    xmlFreeDoc(doc);
    return NULL;
  }

  /* first locate "topology" keyword
   */
  strcpy(keyToFound, "topology");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("agent_final_parser: Can't locate <%s> in the document", keyToFound);
    xmlFreeDoc(doc);
    return NULL;
  }

  app_cfg = (struct application_cfg*)malloc(sizeof(struct application_cfg));
  memset(app_cfg, 0, sizeof(struct application_cfg));
  app_cfg->xml_type = TOPO_XML;

  /* parse the ast_id and action */
  for (attr = cur->properties;
	attr;
	attr = attr->next) {
    if (strcasecmp((char*)attr->name, "action") == 0) {
      app_cfg->action = get_action_by_str((char*)attr->children->content);
      if (app_cfg->action == 0) {
	xmlFreeDoc(doc);
	free(app_cfg);
	return NULL;
      }
    } else if (strcasecmp((char*)attr->name, "ast_id") == 0) 
      app_cfg->ast_id = strdup((char*)attr->children->content);
  } 
	 
  for (cur = cur->xmlChildrenNode;
       cur;
       cur=cur->next) {

    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

    if (strcasecmp((char*)cur->name, "status") == 0) 
      app_cfg->status = get_status_by_str((char*)key);
    else if (strcasecmp((char*)cur->name, "ast_ip") == 0) 
      app_cfg->ast_ip.s_addr = inet_addr((char*)key);

    if (strcasecmp((char*)cur->name, "resource") != 0) 
      continue;
  }

  xmlFreeDoc(doc);
  app_cfg->clnt_sock = -1;
  return app_cfg;
}

struct application_cfg*
master_final_parser(char* filename, int agent)
{
  struct application_cfg* ret;

  ret = topo_xml_parser(filename, agent);

  return (ret);
}

  
/* parse xml and build internal representation;
 * parser_type:
 *	FULL_VERSION		1
 *	BRIEF_VERSION		2
 */
struct application_cfg *
topo_xml_parser(char* filename, int agent)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur, topo_ptr, node_ptr, resource_ptr;
  xmlRelaxNGValidCtxtPtr ctxt;
  struct resource *myres, *myres2;
  struct adtlist *node_list, *link_list, *res_list;
  struct adtlistnode *curnode;
  struct _xmlAttr* attr;
  struct application_cfg* app_cfg;
  int i;

  node_list = NULL;
  link_list = NULL;

  doc = xmlParseFile(filename);
  if (!doc) {
    zlog_err("topo_xml_parser: Document not parsed successfully.");
    return NULL;
  }

  /* validate the doc against standard topology xml file */
  if (xmlRelaxNGValidateDoc(incoming_xml_ctxt, doc)) {
    zlog_err("topo_xml_parser: Incomding xml failed to validate against standard schema; consult schema at ...");
    return NULL;
  }
   
  cur = xmlDocGetRootElement(doc);
  topo_ptr = cur = findxmlnode(cur, "topology");
  app_cfg = (struct application_cfg*)malloc(sizeof(struct application_cfg));
  memset(app_cfg, 0, sizeof(struct application_cfg));
  app_cfg->xml_type = TOPO_XML;
  app_cfg->ast_ip.s_addr = -1;
  app_cfg->clnt_sock = -1;

  /* parse the parameter inside topology tab
   */
  for ( attr = topo_ptr->properties;
        attr;
        attr = attr->next) {
    if (strcasecmp((char*)attr->name, "action") == 0) 
      app_cfg->action = get_action_by_str((char*)attr->children->content);
    else if (strcasecmp((char*)attr->name, "ast_id") == 0)
      app_cfg->ast_id = strdup((char*)attr->children->content);
  }

  /* parse all the nodes
   * since cur is now pointing to the <topology> child xmlnode,
   * set it to point to the child level; then, go through all
   * the nodes in the child level and dump the extract nodes into
   * node_list
   */
  for (cur = cur->xmlChildrenNode;
       cur;
       cur=cur->next) {
    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

    if (strcasecmp((char*)cur->name, "status") == 0)
      app_cfg->status = get_status_by_str((char*)key);
    else if (strcasecmp((char*)cur->name, "ast_ip") == 0)
      app_cfg->ast_ip.s_addr = inet_addr((char*)key);
    else if (strcasecmp((char*)cur->name, "details") == 0)
      strncpy(app_cfg->details, (char*)key, 200-1);
    else if (strcasecmp((char*)cur->name, "xml_file") == 0)
      strncpy(app_cfg->xml_file, (char*)key, 100-1);

    if (strcasecmp((char*)cur->name, "resource") != 0)
      continue;

    /* First, we need to know what kind of resource this is:
     * link or node by looking at its <resource type> keyword
     */
    resource_ptr = cur;

    myres = (struct resource*) malloc(sizeof(struct resource));
    memset(myres, 0, sizeof(struct resource));
    myres->ip.s_addr = -1;
    myres->minion_sock = -1;

    /* parse the parameter inside resource tab
     */
    for ( attr = resource_ptr->properties;
          attr;
          attr = attr->next) {
      if (strcasecmp((char*)attr->name, "res_type") == 0) {
	if (strcasecmp((char*)attr->children->content, "node") == 0)
	  myres->res_type = res_node;
	else
	  myres->res_type = res_link;
      } else if (strcasecmp((char*)attr->name, "subtype") == 0) {
 	myres->subtype = search_res_type(myres->res_type, (char*)attr->children->content);
	if (!myres->subtype) {
	  zlog_info("%s resource subtype %s is unknown",
			(myres->res_type == res_node)? "node":"link",
			attr->children->content);
	  free(myres);
	  myres = NULL;
	  break;
 	} 
      } else if (strcasecmp((char*)attr->name, "name") == 0)
        strncpy(myres->name, (char*)attr->children->content, NODENAME_MAXLEN);
    }

    /* some sanity check for "resource" */
    if (!myres)
      continue;
    if (myres->name[0] == '\0') {
      zlog_err("RESource doesn't have a name; ignore ...");
      free(myres);
      myres = NULL;
      continue;
    }

    /* check for name */
    res_list = (myres->res_type == res_node)? node_list:link_list;
    if (res_list) {
      for (curnode = res_list->head;
           curnode;
           curnode = curnode->next) {
        myres2 = (struct resource*)curnode->data;
        if (strcasecmp(myres2->name, myres->name) == 0) {
          zlog_warn("res name (%s) has to be unique", myres->name);
          free(myres);
          myres = NULL;
          break;
        }
      }
    }
    if (!myres)
      continue;
 
    for (node_ptr = resource_ptr->xmlChildrenNode;
         node_ptr;
         node_ptr = node_ptr->next) {
      key = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1);

      if (!key)
        continue;

      if (strcasecmp((char*)node_ptr->name, "ip") == 0)
	myres->ip.s_addr = inet_addr((char*)key);
      else if (strcasecmp((char*)node_ptr->name, "status") == 0)
        myres->status = get_status_by_str((char*)key);
      else if (strcasecmp((char*)node_ptr->name, "agent_message") == 0)
        myres->agent_message = strdup((char*)key);
      else if (strcasecmp((char*)node_ptr->name, "res_details") == 0) {
	if (myres->subtype->schema_ctxt) { 
          ctxt = xmlRelaxNGNewValidCtxt(myres->subtype->schema_ctxt);
	  if (ctxt) {
	    if ((i = xmlRelaxNGValidatePushElement(ctxt, doc, node_ptr)) == 0) {
	      if ((i = xmlRelaxNGValidateFullElement(ctxt, doc, node_ptr)) != 1) {
		zlog_err("Failed to validate resource %s againt the schema of subtype %s", 
			 myres->name, myres->subtype->name); 
		free(myres); 
		myres = NULL; 
		xmlRelaxNGFreeValidCtxt(ctxt); 
		break;
	      }
	    } 
	    xmlRelaxNGFreeValidCtxt(ctxt);
	  }
	} else 
	  myres->xml_node = node_ptr;

	if (myres->subtype->mod && myres->subtype->mod->read_func) 
	  myres->res = myres->subtype->mod->read_func(app_cfg, node_ptr, agent);
      }
    }
    if (!myres)
      continue;
    if (myres->xml_node)
      myres->xml_node = cutxmlnode(resource_ptr, "res_details");
    
    if (!res_list) {
      res_list = malloc(sizeof(struct adtlist));
      memset(res_list, 0, sizeof(struct adtlist));
     
      if (myres->res_type == res_node) {
	node_list = res_list;
	app_cfg->node_list = node_list;
      } else {
	link_list = res_list;
	app_cfg->link_list = link_list;
      }
    }
    adtlist_add(res_list, myres);
  }

  xmlFreeDoc(doc);
  if (app_cfg->node_list)
    app_cfg->total += app_cfg->node_list->count;
  if (app_cfg->link_list)
    app_cfg->total += app_cfg->link_list->count;

  return app_cfg;
}

int 
validate_res_list(struct application_cfg *app_cfg, 
		  struct adtlist *res_list, 
		  int agent)
{
  struct adtlistnode *curnode;
  struct resource *res;
  int ret = 0;

  if (!app_cfg || !res_list)
    return 0;

  for ( curnode = res_list->head;
      curnode && !ret;
      curnode = curnode->next) {
    res = (struct resource*)curnode->data;
    
    if (res->subtype->mod && res->subtype->mod->validate_func)
      ret += res->subtype->mod->validate_func(app_cfg, res, agent);
  }

  return ret;
}

#ifdef FIONA
int 
pre_process_res_list(struct application_cfg *app_cfg, struct adtlist *res_list, int agent)
{
  struct adtlistnode *curnode;
  struct resource *res;
  int ret = 0;

  if (!app_cfg || !res_list)
    return 0;

  for ( curnode = res_list->head;
      curnode && !ret;
      curnode = curnode->next) {
    res = (struct resource*)curnode->data;
    if (res->res && 
	res->subtype->mod && res->subtype->mod->pre_process_func) 
      ret += res->subtype->mod->pre_process_func(app_cfg, res->res, agent);
  }

  return ret;
}
#endif
/* validate the graph
 */
int
topo_validate_graph(int agent, struct application_cfg *app_cfg)
{
  int ret = 0;

  /* first check the validity of funciton and ast_id
   */
  switch (agent) {

    case NODE_AGENT:
    case LINK_AGENT:
      if (app_cfg->action == setup_resp ||
          app_cfg->action == app_complete ||
          app_cfg->action == release_resp ||
          app_cfg->action == query_resp) {
        zlog_err("Invalid action defined for this topology");
        return (0);
      }
      if (app_cfg->ast_id == NULL) {
        zlog_err("ast_id should be set for this topology");
        return (0);
      }
      break;

    case MASTER:
      if (app_cfg->action == ast_complete) {
        zlog_err("Invalid action defined for this topology");
        return (0);
      }
      if ((app_cfg->action != setup_req &&
           app_cfg->action != query_req) &&
           app_cfg->ast_id == NULL) {
        zlog_err("ast_id should be set for this topology");
        return (0);
      }
      break;

    case ASTB:
      if (app_cfg->action == ast_complete ||
          app_cfg->action == query_resp) {

        zlog_err("Invalid action defined for this topology");
        printf("Valide Values are:\n");
        printf("SETUP_REQ, RELEASE_REQ, APP_COMPLETE, QUERY_REQ\n");
        return 0;
      }
      break;
  }

  ret += validate_res_list(app_cfg, app_cfg->node_list, agent);
  if (!ret) 
    ret += validate_res_list(app_cfg, app_cfg->link_list, agent);
/*
  if (!ret)
    ret += pre_process_res_list(app_cfg, app_cfg->node_list, agent);
  if (!ret)
    ret += pre_process_res_list(app_cfg, app_cfg->link_list, agent);
*/
  return ret;
}

void
app_cfg_pre_req()
{
  struct adtlistnode *curnode;
  struct resource *res_cfg;

  if (!glob_app_cfg)
    return;

  memset(glob_app_cfg->details, 0, 200);
  glob_app_cfg->status = ast_success;
  
  if (glob_app_cfg->node_list) {
    for (curnode = glob_app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      res_cfg = (struct resource*) curnode->data;

      res_cfg->status = status_unknown;
      if (res_cfg->agent_message) {
	free(res_cfg->agent_message);
	res_cfg->agent_message = NULL;
      }
    }
  }
 
  if (glob_app_cfg->link_list) {
    for (curnode = glob_app_cfg->link_list->head;
	 curnode;
	 curnode = curnode->next) {
      res_cfg = (struct resource*) curnode->data;

      res_cfg->status = status_unknown;
      if (res_cfg->agent_message) {
	free(res_cfg->agent_message);
	res_cfg->agent_message = NULL;
      }
    }
  }
}

/***********  FOR MINIONS ***********/
int 
minion_process_res_list(enum action_type action, struct adtlist *res_list)
{
  struct adtlistnode *curnode;
  struct resource *res;
  int ret_value = 0;

  if (!res_list)
    return 0;

  for (curnode = res_list->head;
        curnode;
        curnode = curnode->next) {
    res = (struct resource*) curnode->data;

    if (res->agent_message) 
      free(res->agent_message);
    res->agent_message = NULL;
    res->status = ast_success;

    if (res->subtype->mod && res->subtype->mod->minion_proc_func) {
      if (res->subtype->mod->minion_proc_func(action, res)) {
	res->status = ast_failure;
	ret_value++;
      }
    } 
  }

  return ret_value;
}

int
minion_process_setup_req()
{
  static char path[300];
  static char directory[300];
  
  glob_app_cfg->action = setup_resp;
  zlog_info("Processing %s, setup_req", glob_app_cfg->ast_id);

  /* pre-processing */
  if (glob_minion == LINK_AGENT)
    strcpy(directory, LINK_AGENT_DIR);
  else 
    strcpy(directory, NODE_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    print_xml_response(glob_minion_ret_xml, glob_minion);
    return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
  if (mkdir(directory, 0755) == -1) {
    if (errno == EEXIST) {
      zlog_info("<ast_id> %s exists already", glob_app_cfg->ast_id);
    } else {
      zlog_err("Can't create the directory: %s; error = %d(%s)",
                directory, errno, strerror(errno));
      return 0;
    }
  }
  sprintf(path, "%s/setup_original.xml", directory);
  if (rename(glob_minion_recv_xml, path) == -1)
    zlog_err("can't rename the incoming setup_req file");

  minion_process_res_list(setup_req, (glob_minion == NODE_AGENT)?glob_app_cfg->node_list:glob_app_cfg->link_list);
  sprintf(path, "%s/setup_response.xml", directory);

  print_xml_response(path, glob_minion);
  symlink(path, glob_minion_ret_xml);
  
  sprintf(path, "%s/final.xml", directory);
  print_final(path, MASTER);
 
  return (glob_app_cfg->status == ast_failure);
}

int 
minion_process_release_req()
{
  static char path[300];
  static char directory[300];
  struct application_cfg *working_app_cfg;

  glob_app_cfg->action = release_resp;
  zlog_info("Processing %s, release_req", glob_app_cfg->ast_id);

  working_app_cfg = glob_app_cfg;
  glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, glob_minion);

  if (!glob_app_cfg) {
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "Can't locate the ast_id related info");
    print_xml_response(glob_minion_ret_xml, glob_minion);
    glob_app_cfg->status = ast_failure;
    return 0;
  }

  if (glob_app_cfg->action == release_resp ||
        glob_app_cfg->action == release_req) {
    free_application_cfg(glob_app_cfg);
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "ast_id has received RELEASE_REQ already");
    print_xml_response(glob_minion_ret_xml, glob_minion);
    glob_app_cfg->status = ast_failure;
    return 0;
  }

  if (glob_app_cfg->ast_ip.s_addr != working_app_cfg->ast_ip.s_addr)
    zlog_warn("NEW ast_ip: %s in this release_req",
                inet_ntoa(working_app_cfg->ast_ip));

  if (glob_minion == LINK_AGENT)
    sprintf(directory, "%s/%s", LINK_AGENT_DIR, glob_app_cfg->ast_id);
  else 
    sprintf(directory, "%s/%s", NODE_AGENT_DIR, glob_app_cfg->ast_id);

  sprintf(path, "%s/setup_response.xml", directory);
  free_application_cfg(glob_app_cfg);
  if ((glob_app_cfg = topo_xml_parser(path, glob_minion)) == NULL) {
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "didn't parse the file for ast_id successfully");
    print_xml_response(glob_minion_ret_xml, glob_minion);
    glob_app_cfg->status = ast_failure;
    return 0;
  }

  minion_process_res_list(release_req, (glob_minion == NODE_AGENT)?glob_app_cfg->node_list:glob_app_cfg->link_list);
  glob_app_cfg->ast_ip = working_app_cfg->ast_ip;
  working_app_cfg->ast_ip.s_addr = -1;
  free_application_cfg(working_app_cfg);
  working_app_cfg = NULL;
  glob_app_cfg->action = release_resp;

  sprintf(path, "%s/release_response.xml", directory);

  if (rename(glob_minion_recv_xml, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
           glob_minion_recv_xml, path, errno, strerror(errno));

  sprintf(path, "%s/release_response.xml", directory);
  print_xml_response(path, glob_minion);
  symlink(path, glob_minion_ret_xml);

  sprintf(path, "%s/final.xml", directory);
  print_final(path, MASTER);

  return 1;
}

int
minion_process_ast_complete()
{
  static char path[300], directory[300];
  struct application_cfg *working_app_cfg;

  glob_app_cfg->action = app_complete;
  zlog_info("Processing %s, ast_complete", glob_app_cfg->ast_id);

  working_app_cfg = glob_app_cfg;
  glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, glob_minion);
  if (!glob_app_cfg) {
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "Can't locate the ast_id file successfully");
    glob_app_cfg->status = ast_failure;
    print_xml_response(glob_minion_ret_xml, glob_minion);
    return 0;
  }

  if (glob_app_cfg->action == release_resp) {
    free_application_cfg(glob_app_cfg);
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "ast_id has received RELEASE_REQ already");
    glob_app_cfg->status = ast_failure;
    print_xml_response(glob_minion_ret_xml, glob_minion);
    return 0;
  }

  if (glob_app_cfg->ast_ip.s_addr != working_app_cfg->ast_ip.s_addr)
    zlog_warn("NEW ast_ip: %s", inet_ntoa(working_app_cfg->ast_ip));

  if (glob_minion == LINK_AGENT)
    sprintf(directory, "%s/%s", LINK_AGENT_DIR, glob_app_cfg->ast_id);
  else
    sprintf(directory, "%s/%s", NODE_AGENT_DIR, glob_app_cfg->ast_id);

  sprintf(path, "%s/ast_complete.xml", directory);
  rename(glob_minion_recv_xml, path);

  sprintf(path, "%s/setup_response.xml", directory);
  free_application_cfg(glob_app_cfg);
  if ((glob_app_cfg = topo_xml_parser(path, glob_minion)) == NULL) {
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "didn't parse the ast_id file successfully");    glob_app_cfg->status = ast_failure;
    print_xml_response(glob_minion_ret_xml, glob_minion);
    return 0;
  }

  minion_process_res_list(ast_complete, (glob_minion == NODE_AGENT)?glob_app_cfg->node_list:glob_app_cfg->link_list);
 
  glob_app_cfg->ast_ip = working_app_cfg->ast_ip;
  working_app_cfg->ast_ip.s_addr = -1;
  glob_app_cfg->action = working_app_cfg->action;
  free_application_cfg(working_app_cfg);
  working_app_cfg = NULL;

  glob_app_cfg->status = ast_app_complete;

  sprintf(path, "%s/final.xml", directory);
  print_final(path, MASTER);
  symlink(path, glob_minion_ret_xml);

  return 1;
} 

int
minion_process_xml(struct in_addr clntAddr)
{
  if ((glob_app_cfg = topo_xml_parser(glob_minion_recv_xml, glob_minion)) == NULL) {
    zlog_err("minion_process_xml: received xml file with error");
    return 1;
  }

  glob_app_cfg->ast_ip = clntAddr;

  if (glob_app_cfg->action != setup_req &&
      glob_app_cfg->action != release_req &&
      glob_app_cfg->action != query_req &&
      glob_app_cfg->action != ast_complete) {
    zlog_err("minion_process_xml: invalid <action> in xml file");
    sprintf(glob_app_cfg->details, "invalid action in xml file");
    return 1;
  }

  if (glob_app_cfg->action != setup_req &&
        glob_app_cfg->ast_id == NULL) {
    set_allres_fail("ast_id should be set in non-setup request case");
    return 1;
  }

  glob_app_cfg->ast_ip = clntAddr;
  if (topo_validate_graph(glob_minion, glob_app_cfg)) {
    zlog_err("minion_process_xml: topo_validate_graph() failed");
    return 1;
  }

  if (glob_app_cfg->action == setup_req) {
    glob_app_cfg->status = ast_success;
    minion_process_setup_req();
  } else if ( glob_app_cfg->action == ast_complete) 
    minion_process_ast_complete();
  else if (glob_app_cfg->action == release_req) 
    minion_process_release_req();

  return 0;
} 
