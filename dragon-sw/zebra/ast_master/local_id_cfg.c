#include <zebra.h>
#include <sys/socket.h>
#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "ast_master.h"
#include "buffer.h"
#include "log.h"
#include "local_id_cfg.h"

extern char* node_stype_name[];
extern char* status_type_details[];
extern int send_file_to_agent(char *, int, char *);

extern xmlNodePtr findxmlnode(xmlNodePtr, char*);
extern void free_id_cfg_res(struct id_cfg_res*);
extern int get_status_by_str(char*);
 
char *local_id_name[] =
{ "none",
  "group",
  "tagged-group",
  "port" };

char *id_action_name[] =
{ "none",
  "create",
  "delete",
  "modify" };

char* 
generate_cfg_id()
{
  static char id[100];

  gethostname(id, 100);
  sprintf(id+strlen(id), "_%d", (int)time(NULL));
   
  return strdup(id);
}

static int
get_type_by_name(char* type)
{
  int i;

  for ( i = 1; i <= NUM_LOCAL_ID_TYPE; i++) 
    if (strcasecmp(type, local_id_name[i]) == 0) 
      return i;
  
  return 0;
}

static int
get_id_action_by_name(char* action)
{
  if (strcasecmp(action, "CREATE") == 0) 
    return ID_CREATE;
  else if (strcasecmp(action, "DELETE") == 0)
    return ID_DELETE;
  else if (strcasecmp(action, "MODIFY") == 0)
    return ID_MODIFY;
  
  return 0;
}

int
compose_id_req(struct application_cfg *app_cfg, char* path, struct id_cfg_res *res)
{
  struct adtlistnode *cur;
  struct local_id_cfg *id_cfg;
  FILE *send_file;
  int i;

  if (!path || strlen(path) == 0 || !res ) 
    return 0;

  send_file = fopen(path, "w+");
  if (!send_file) {
    zlog_err("compose_id_req: Can't open the file %s", path);
    return 0;
  }

  fprintf(send_file, "<local_id_cfg ast_id=\"%s\">\n", app_cfg->ast_id);
  fprintf(send_file, "<resource type=\"%s\" name=\"%s\">\n", node_stype_name[res->stype], res->name);
  fprintf(send_file, "\t<ip>%s</ip>\n", res->ip);
  
  for (cur = res->cfg_list->head;
	cur;
	cur = cur->next) {
    id_cfg = (struct local_id_cfg*) cur->data;

    fprintf(send_file,
	"\t<local_id id=\"%d\" action=\"%s\" type=\"%s\">\n", id_cfg->id, 
	id_action_name[id_cfg->action], local_id_name[id_cfg->type]);

    for (i = 0;
	 i < id_cfg->num_mem;
	 i++)
      fprintf(send_file, "\t\t<member>%d</member>\n", id_cfg->mems[i]);
    fprintf(send_file, "\t</local_id>\n");
  }

  fprintf(send_file, "</resource>\n"); 
  fprintf(send_file, "</local_id_cfg>\n");
  
  fflush(send_file);
  fclose(send_file);

  return 1;
}

int 
process_id_result(struct application_cfg *working_app_cfg, struct id_cfg_res* res)
{
  struct adtlistnode *cur, *cur1;
  struct id_cfg_res *newres;
  struct local_id_cfg *old_cfg, *new_cfg;
  int found, ret_val = 1;

  if (!working_app_cfg->node_list) {
    zlog_err("process_id_result: no resource in the file");
    res->status = AST_FAILURE;
    return 0;
  }

  if (adtlist_getcount(working_app_cfg->node_list) != 1) 
    zlog_warn("process_id_result: there should be only 1 resource in the return file");

  /* have to found the the resource that matches "res" 
   * from the working_app_cfg.node_list 
   */
  for (cur = working_app_cfg->node_list->head, found = 0;
	cur && !found;
	cur = cur->next) {
    newres = (struct id_cfg_res*) cur->data;
    if (strcmp(newres->name, res->name) == 0) 
      found = 1;
  } 

  if (!found) {
    zlog_err("process_id_result: can't find the resource from the resturn file");
    res->status = AST_UNKNOWN;
    return 0;
  } else if (!newres->cfg_list) {
    zlog_err("process_id_result: resource found with no local_id in it");
    res->status = AST_UNKNOWN;
    return 0;
  }

  res->status = newres->status;
  if (newres->msg) {
    res->msg = newres->msg;
    newres->msg = NULL;
  }

  for (cur = newres->cfg_list->head, cur1 = res->cfg_list->head;
	cur && cur1;
	cur = cur->next, cur1 = cur1->next) {
    new_cfg = (struct local_id_cfg*) cur->data;
    old_cfg = (struct local_id_cfg*) cur1->data;

    if (new_cfg->id == old_cfg->id &&
	new_cfg->type == old_cfg->type) {
      old_cfg->status = new_cfg->status;
      if (old_cfg->status != AST_SUCCESS)
	ret_val = 0;
      if (new_cfg->msg) {
	old_cfg->msg = new_cfg->msg;
	new_cfg->msg = NULL;
      }
    } else {
      zlog_err("process_id_result: The order of id and type should be the same in both master and returned result");
      ret_val = 0;
    }
  }
 
  return ret_val; 
}
 
void
print_id_response(char * path, int agent)
{
  struct adtlistnode *cur, *cur1;
  struct id_cfg_res* myres;
  struct local_id_cfg* myid;
  FILE *fp;
  int i;

  if (!path)
    return;

  fp = fopen(path, "w+");
  if (!fp)
    return;

  if (!glob_app_cfg) {
    fprintf(fp, "<local_id_cfg>\n");
    fprintf(fp, "<status>AST_FAILURE</status>\n");
    fprintf(fp, "</local_id_cfg>\n");
    fflush(fp);
    fclose(fp);
    return;
  }

  fprintf(fp, "<local_id_cfg ast_id=\"%s\">\n", glob_app_cfg->ast_id);
  fprintf(fp, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);

  if (glob_app_cfg->node_list) {
    for (cur = glob_app_cfg->node_list->head;
	 cur;
	 cur = cur->next) {
      myres = (struct id_cfg_res*) cur->data;

      fprintf(fp, "<resource type=\"%s\" name=\"%s\">\n", node_stype_name[myres->stype], myres->name);
      fprintf(fp, "\t<ip>%s</ip>\n", myres->ip);
      fprintf(fp, "\t<status>%s</status>\n", status_type_details[myres->status]);
      if (myres->msg)
	fprintf(fp, "\t<agent_message>%s</agent_message>\n", myres->msg);

      if (myres->cfg_list) {
	for (cur1 = myres->cfg_list->head;
	     cur1;
	     cur1 = cur1->next) {
	  myid = (struct local_id_cfg*) cur1->data;

	  fprintf(fp, "\t<local_id id=\"%d\" action=\"%s\" type=\"%s\">\n",
		myid->id, id_action_name[myid->action], local_id_name[myid->type]);
	  fprintf(fp, "\t\t<status>%s</status>\n", status_type_details[myid->status]);
	  if (myid->msg) 
	    fprintf(fp, "\t\t<agent_message>%s</agent_message>\n", myid->msg);

	  for (i = 0; i < myid->num_mem; i++) 
	    fprintf(fp, "\t\t<member>%d</member>\n", myid->mems[i]);
	  fprintf(fp, "\t</local_id>\n");
	}
      }
      fprintf(fp, "</resource>\n");
    }
  }
  fprintf(fp, "</local_id_cfg>\n");
  fflush(fp);
  fclose(fp);
}

struct application_cfg* 
id_xml_parser(char* filename, int agent)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur, cur1, cur2, resource_ptr;
  int i, err;
  struct _xmlAttr* attr;
  struct id_cfg_res *myres;
  struct local_id_cfg* myIDcfg;
  struct application_cfg* app_cfg;

  doc = xmlParseFile(filename);

  if (!doc) {
    zlog_err("id_xml_parser: document not parsed successfully");
    return NULL;
  }

  cur = xmlDocGetRootElement(doc);
 
  if (cur == NULL) {
    zlog_err("id_xml_parser: Empty document");
    xmlFreeDoc(doc);
    return NULL;
  }

  /* first locate the <local_id_cfg> keyword
   */
  cur = findxmlnode(cur, "local_id_cfg");
  if (!cur) {
    zlog_err("id_xml_parser: Can't locate <local_id_cfg>");
    xmlFreeDoc(doc);
    return NULL;
  }

  app_cfg = (struct application_cfg*) malloc(sizeof(struct application_cfg));
  memset(app_cfg, 0, sizeof(struct application_cfg));
  app_cfg->xml_type = ID_XML;

  for (attr = cur->properties;
	attr;
	attr = attr->next) {
    if (strcasecmp(attr->name, "ast_id") == 0) {
      app_cfg->ast_id = strdup(attr->children->content);
      break;
    }
  }

  for (cur = cur->xmlChildrenNode;
       cur;
       cur = cur->next) {
    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

    if (strcasecmp(cur->name, "resource") != 0) 
      continue;

    resource_ptr = cur;

    myres = (struct id_cfg_res*)malloc(sizeof(struct id_cfg_res));
    memset(myres, 0, sizeof(struct id_cfg_res));
   
    for (attr = resource_ptr->properties;
	 attr;
	 attr = attr->next) {
      if (strcasecmp(attr->name, "type") == 0) {
	for (i = 1;
	     i <= NUM_NODE_STYPE;
	     i++) {
	  if (strcasecmp(attr->children->content, node_stype_name[i]) == 0) {
	    myres->stype = i;
	    break;
	  }
	} 
      } else if (strcasecmp(attr->name, "name") == 0)
	strncpy(myres->name, attr->children->content, NODENAME_MAXLEN);
    }

    if (myres->stype == 0 || myres->name[0] == '\0') {
      zlog_err("id_xml_parser: resource doesn't have valid stype nor a name");
      free_id_cfg_res(myres);
      continue;
    }

    for (cur1 = resource_ptr->xmlChildrenNode;
	 cur1;
	 cur1 = cur1->next) {
      key = xmlNodeListGetString(doc, cur1->xmlChildrenNode, 1);

      if (strcasecmp(cur1->name, "ip") == 0) 
	strncpy(myres->ip, key, IP_MAXLEN);
      else if (strcasecmp(cur1->name, "status") == 0) 
	myres->status = get_status_by_str(key);
      else if (strcasecmp(cur1->name, "agent_message") == 0) 
	myres->msg = strdup(key);
      else if (strcasecmp(cur1->name, "local_id") == 0) {
	myIDcfg = (struct local_id_cfg*) malloc (sizeof(struct local_id_cfg));
	memset(myIDcfg, 0, sizeof(struct local_id_cfg));
	myIDcfg->id = -1;

	for (attr = cur1->properties, err = 0; 
	     attr && !err;
	     attr = attr->next) {
	  if (strcasecmp(attr->name, "id") == 0) {
	    myIDcfg->id = atoi(attr->children->content);
	    if (myIDcfg->id < 0 || myIDcfg->id > 65535) {
	      zlog_err("id_xml_parser: local_id (%d) ignored: not within range <0-65535>", myIDcfg->id);
	      err = 1;
	    }
	  } else if (strcasecmp(attr->name, "action") == 0) {
	    myIDcfg->action = get_id_action_by_name(attr->children->content);
	    if (!myIDcfg->action) {
	      zlog_err("id_xml_parser: <local_id action> is invalid: %s", attr->children->content);
	      err = 1;
	    } 
	  } else if (strcasecmp(attr->name, "type") == 0) {
	    myIDcfg->type = get_type_by_name(attr->children->content);
	    if (!myIDcfg->type) {
	      zlog_err("id_xml_parser: <local_id type> is invalid: %s", attr->children->content);
	      err = 1;
	    }
	  }
	}

        if (!myIDcfg->action) {
	  zlog_err("id_xml_parser: No action defined for a <local_id>; ignored ...");
	  err = 1;
        } else if (!myIDcfg->type) {
	  zlog_err("id_xml_parser: No type defined for a <local_id>; ignored ...");
	  err = 1;
 	} else if (myIDcfg->id == -1) {
	  zlog_err("id_xml_parser: No id defined for a <local_id>; ignored ...");
	  err = 1;
	}
	if (err) {
	  free(myIDcfg);
	  continue;
	}

	for (cur2 = cur1->xmlChildrenNode;
	     cur2;
	     cur2 = cur2->next) { 
	  key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);

	  if (!key) 
	    continue;
	
	  if (strcasecmp(cur2->name, "member") == 0)
	    myIDcfg->num_mem++;
	}

	/* do some error checking here */

	switch (myIDcfg->action) {

	  case ID_CREATE:
	  case ID_MODIFY:
	   
	  
	    if (myIDcfg->action == ID_MODIFY && myIDcfg->type == 3) { 
	      zlog_err("id_xml_parser: For type port, it can't be modified"); 
	      err = 1; 
	      break; 
	    } 

	    if (myIDcfg->type == 3 && myIDcfg->num_mem != 0) { 
	      zlog_warn("id_xml_parser: For type port, there doesn't need to have any member");
	      myIDcfg->num_mem = 0; 
	    } else if ((myIDcfg->type == 1 || myIDcfg->type == 2) && 
			myIDcfg->num_mem == 0) { 
	      zlog_err("id_xml_parser: For type group or tagged-group, there should be at least one member defined"); 
	      err = 1; 
	    } 
	    break;

	  case ID_DELETE:
	   if (myIDcfg->num_mem) {
	     zlog_warn("id_xml_parser: For delete, all <member> defined will be ignored as only <id> is required");
	     myIDcfg->num_mem = 0;
	   }
	   break;
        }

	if (err) {
	  free(myIDcfg);
	  continue;
	}

	if (myIDcfg->num_mem) 
	  myIDcfg->mems = (int*) malloc(sizeof(int)*myIDcfg->num_mem);

	/* loop again */
	for (cur2 = cur1->xmlChildrenNode, i=0;
	     cur2;
	     cur2 = cur2->next) {
	  key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
	    
	  if (!key)
	    continue;

	  if (strcasecmp(cur2->name, "member") == 0 && myIDcfg->num_mem) {
  
	    if (i==myIDcfg->num_mem) {
	      zlog_err("id_xml_parser: the num_mem calculated before is wrong");
	      break;
	    }
	    myIDcfg->mems[i] = atoi(key);
	    i++;
	  } else if (strcasecmp(cur2->name, "status") == 0)
	    myIDcfg->status = get_status_by_str(key);
	  else if (strcasecmp(cur2->name, "agent_message") == 0)
	    myIDcfg->msg = strdup(key);
	}

	if (!myres->cfg_list) {
	  myres->cfg_list = (struct adtlist*)malloc(sizeof(struct adtlist));
	  memset(myres->cfg_list, 0, sizeof(struct adtlist));
	}
	adtlist_add(myres->cfg_list, myIDcfg);
      }
    }

    /* ok, after parsing all attributes in this res. 
     * if there is no id cfg needed to be done, free this res
     */
    if (!myres->cfg_list) {
      free_id_cfg_res(myres);
    } else {
      if (!app_cfg->node_list) {
	app_cfg->node_list = (struct adtlist*) malloc (sizeof(struct adtlist));
	memset(app_cfg->node_list, 0, sizeof(struct adtlist));
      }
      adtlist_add(app_cfg->node_list, myres);
    }
  }

  return app_cfg;
}

int
master_process_id(char* filename)
{
  struct adtlistnode *cur;
  struct id_cfg_res* myres;
  int sock;
  static char buffer[RCVBUFSIZE];
  static char ret_buf[SENDBUFSIZE];
  int bytesRcvd, total, ret_value = 1;
  FILE* ret_file = NULL;
  char directory[80];
  char newpath[105];
  struct application_cfg *working_app_cfg;

  if ((glob_app_cfg = id_xml_parser(filename, MASTER)) == NULL) {
    print_id_response(AST_XML_RESULT, MASTER);
    return 0;
  }

  if (!glob_app_cfg->node_list) {
    glob_app_cfg->status = AST_SUCCESS;
    print_id_response(AST_XML_RESULT, MASTER);
    return 1;
  }

  glob_app_cfg->ast_id = generate_cfg_id();
  zlog_info("Processing glob_ast_id: %s <local_id_cfg> file", glob_app_cfg->ast_id);
  strcpy(directory, ID_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("master_process_id: Can't create directory %s", directory);
    return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
  if (mkdir(directory, 0755) == -1) {
    if (errno == EEXIST) {
      if (remove(directory) == -1) {
	zlog_err("master_process_id: Can't remove the directory: %s", directory);
	return 0;
      }
    } else {
      zlog_err("master_process_id: Can't create the directory: %s; error = %d(%s)",
			directory, errno, strerror(errno));
      return 0;
    }
  }

  sprintf(newpath, "%s/orig.xml", directory);
  if (rename(filename, newpath) == -1)
    zlog_err("master_process_id: Can't rename %s to %s; errno = %d(%s)",
		filename, newpath, errno, strerror(errno));

  memset(buffer, 0, RCVBUFSIZE);

  /* now, send the task list to all related node */
  for (cur = glob_app_cfg->node_list->head;
       cur;
       cur = cur->next) {
    myres = (struct id_cfg_res*) cur->data;

    sprintf(newpath, "%s/%s.xml", directory, myres->name);
    
    /* compose the sending file */
    compose_id_req(glob_app_cfg, newpath, myres);

    /* call send_file_to_agent */
    zlog_info("master_process_id: sending request to %s (%s:%d)", 
	      myres->name, myres->ip, DRAGON_XML_PORT);
    sock = send_file_to_agent(myres->ip, DRAGON_XML_PORT, newpath);

    if (sock == -1) {
      myres->status = AST_FAILURE;
      myres->msg = strdup("Failed to connect to link_agent");
      ret_value = 0;
      close(sock);
      continue; 
    }

    /* waiting the result to come back */
    total = 0;
    memset(ret_buf, 0, SENDBUFSIZE);
    while ((bytesRcvd = recv(sock, buffer, RCVBUFSIZE-1, 0))  > 0) {
      
      if (!total) {
	zlog_info("master_process_id: Received confirmation from %s", myres->name);
	sprintf(newpath, "%s/resp_%s.xml", directory, myres->name);
	ret_file = fopen(newpath, "w");
	if (!ret_file) {
	  zlog_err("master_process_id: can't open %s; error = %d(%s)",
		      newpath, errno, strerror(errno));
	  ret_value = 0;
	  continue;
	}
      }
      total+=bytesRcvd;
      buffer[bytesRcvd] = '\0';
      sprintf(ret_buf+strlen(ret_buf), buffer);
      fprintf(ret_file, buffer);
    }

    if (total == 0) {
      zlog_err("master_process_id: No confirmation from %s", myres->name);
      myres->status = AST_UNKNOWN;
      ret_value = 0;
    } else {

      fflush(ret_file);
      fclose(ret_file);

      if ((working_app_cfg = id_xml_parser(newpath, MASTER)) == NULL) {
	zlog_err("master_process_id: returned file (%s) is not parsed correctly", newpath);
	myres->status = AST_UNKNOWN;
	myres->msg = strdup("The response file is not parsed correctly");
	ret_value = 0;
      } else 
	if (process_id_result(working_app_cfg, myres) != 1)
	  ret_value = 0;
	free_application_cfg(working_app_cfg);
    }
    close(sock);
  }

  /* integrate the result and put the result into AST_XML_RESULT */
  if (ret_value)
    glob_app_cfg->status = AST_SUCCESS;
  else
    glob_app_cfg->status = AST_FAILURE;

  sprintf(newpath, "%s/final.xml", directory);
  symlink(newpath, AST_XML_RESULT);
  print_id_response(newpath, MASTER);

  return ret_value;
}
