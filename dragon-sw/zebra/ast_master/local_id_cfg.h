
#define NUM_LOCAL_ID_TYPE	3

#define ID_DIR         "/usr/local/local_id"

#define ID_CREATE	1
#define ID_DELETE	2
#define	ID_MODIFY	3

struct id_cfg_res {
  int status;
  char *msg;

  char name[NODENAME_MAXLEN + 1];
  char ip[IP_MAXLEN+1];
  enum node_stype stype;
  struct adtlist *cfg_list;
};

struct local_id_cfg {
  int status;
  char *msg;

  int id;
  int action;
  int type;
  int num_mem;
  int *mems;
};

struct adtlist *glob_id_cfg_list;

int id_xml_parser(char*, int);
