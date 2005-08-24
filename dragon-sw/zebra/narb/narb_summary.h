/*narb_summary.h*/

#ifndef _NARB_SUMMARY_
#define _NARB_SUMMARY_

#define MAX_ADDR_LEN 40

/* data structure storing OSPFd server information*/
struct ospfd_info 
{
  char addr[MAX_ADDR_LEN];
  int port;
  int localport;
  struct in_addr ori_if;
  struct in_addr area;
};

/* data structure of an opaque router id generated in domain summary*/
struct router_id_info
{
  struct in_addr adv_id;
  struct in_addr id;
  u_int32_t opaque_id;
  int hide;
  int type; 
};
#define RT_TYPE_DEFAULT 0x0
#define RT_TYPE_HOST 0x01
#define RT_TYPE_BORDER 0x02

/* data structure of an opaque TE-link generated in domain summary*/
struct link_info
{
  struct in_addr adv_id;
  struct in_addr id;
  u_int32_t opaque_id;
  int hide;
  struct in_addr loc_if;
  struct in_addr rem_if;
  int type;
  float max_bw;
  float max_rsv_bw;
  float unrsv_bw[8];
  u_int32_t metric;
  struct link_ifswcap_type ifswcap;
  list resvs;
  /*a 16-bit flag indicating which optinal TE parameters are included.
    Flag masks for those parameters are defined below.*/
  u_int16_t info_flag;
};

#define LINK_PARA_FLAG_MAX_BW 0x1
#define LINK_PARA_FLAG_MAX_RSV_BW 0x2
#define LINK_PARA_FLAG_UNRSV_BW 0x4
#define LINK_PARA_FLAG_IFSW_CAP 0x10
#define LINK_PARA_FLAG_METRIC 0x20
#define LINK_PARA_FLAG_LOC_IF 0x40
#define LINK_PARA_FLAG_REM_IF 0x80
/* Flag indicating whether the link is originated by automatic probing 
    This is not a TE parameter!*/
#define LINK_PARA_FLAG_AUTO_PROBED 0x1000 
/* macros to access link_info flag */
#define SET_LINK_PARA_FLAG(X, F) ((X) = (X) | (F))
#define LINK_PARA_FLAG(X, F) ((X) & (F))

/* data structure describing associating of remote if_addr of an  inter-domain 
te-link with the narb in a neighboring domain*/
struct if_narb_info
{
  char addr[MAX_ADDR_LEN]; /*narb address*/
  int port; /*narb port*/
  list if_addr_list; /*a list of interfaces associated with this narb */
};

/* Data structure to probe indicating that a CSPF virtual link exists between 
  a source node and a bunch of destination nodes. Abstract Link TE LSAs will
  be originated based on these data.*/
struct service_info
{
  int service_id;
  int sw_type;
  int enc_type;
  float max_bw;
};

struct svc_probe
{
  struct router_id_info *router;
  struct service_info *service;
};
/*Default auto-generated virtual link metrics, 
  forcing through-traffic to use border routers*/
#define HOST_BORDER_METRIC 5
#define BORDER_BORDER_METRIC 3


/* a super data structure summarzing the domain*/
struct narb_domain_info
{
  /* info of intra-domain OSPFd */
  struct ospfd_info ospfd_intra;
  /* info of intra-domain OSPFd */
  struct ospfd_info ospfd_inter;
  /* the domain identifier (other than the area id) */
  u_int32_t domain_id;
  /* a list of opaque router id's (struct router_id_info) in this domain*/
  list router_ids;
  /* a list of opaque te links (struct link_info) in this domain*/
  list te_links;
  /* a list of iner-domain te-link ids (struct in_addr)  in this domain*/
  list inter_domain_te_links;
  /* a list of (struct if_narb_info)*/
  list if_narb_table;
  /* vector storing services*/
  list services;
  /* a list of svc_probe*/
  list svc_probes;
};


/*************   Define DRAGON Speficit TLV's  *************/
#define DRAGON_TLV_TYPE_BASE 0x4000
#define TE_LINK_SUBTLV_LINK_DRAGON_RESV  DRAGON_TLV_TYPE_BASE + 1
#define TE_LINK_SUBTLV_LINK_DRAGON_IACD DRAGON_TLV_TYPE_BASE + 2

#define TE_LINK_SUBTLV_LINK_DRAGON_DOMAIN_ID DRAGON_TLV_TYPE_BASE + 0x10

#define LINK_PARA_FLAG_RESV 0x100
#define LINK_PARA_FLAG_IACD 0x200

struct reservation
{
    u_int32_t domain_id;
    u_int32_t lsp_id;
    u_int32_t uptime;
    u_int32_t duration;
    float bandwidth;
    u_int32_t status;
};

#define RESV_SIZE (sizeof(struct reservation))

struct te_link_domain_id
{
    struct te_tlv_header header;
    u_int32_t value;
};

/******************************************************/


/* references to global narb domain information data*/
extern struct narb_domain_info narb_domain_info;
extern struct ospf_apiclient *oclient_inter;
extern struct ospf_apiclient *oclient_intra;


/* declarations of narb_summary functions */
extern int 
narb_originate_router_id (struct ospf_apiclient * oc, struct router_id_info* router);

extern int 
narb_originate_te_link (struct ospf_apiclient * oc, struct link_info* link);

extern int
narb_refresher_originate_summary (struct thread *t);

extern int
narb_originate_summary (struct ospf_apiclient * oc);

extern void
narb_delete_summary (struct ospf_apiclient * oc);

extern struct if_narb_info * 
if_narb_lookup(list if_narb_table, struct in_addr if_addr);

extern struct link_info*
narb_cspf_probe(struct svc_probe *svc_probe, struct router_id_info *router);

extern void
narb_probe_virtual_links(struct narb_domain_info *p_domain_info);

extern void
narb_cleanup_probed_links(struct narb_domain_info *p_domain_info);

#endif
