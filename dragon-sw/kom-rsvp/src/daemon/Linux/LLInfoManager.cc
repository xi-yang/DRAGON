/****************************************************************************

  KOM RSVP Engine (release version 2.1)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version
  2 of the License, or (at your option) any later version.
 
  Author: Aart van Halteren (a.t.vanhalteren@kpn.com)
  Date: December 2000


*/

#if defined(Linux) && defined(ENABLE_CBQ) && defined(REAL_NETWORK)

#include "LLInfoManager.h"
#include <math.h>

int 
LLInfoManager::remember_index(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	int h;
	struct ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(n);
	IndexMap* im;
	IndexMap** imp;
	LLInfoManager* info_mgr = (LLInfoManager*)arg;
	struct rtattr *tb[IFLA_MAX+1];

	if (n->nlmsg_type != RTM_NEWLINK)
		return 0;

	if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifi)))
		return -1;

	memset(tb, 0, sizeof(tb));
	RTNetlink::parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), IFLA_PAYLOAD(n));
	if (tb[IFLA_IFNAME] == NULL)
		return 0;

	h = ifi->ifi_index&0xF;

	// find entry in indexmap
	for (imp=&(info_mgr->idxmap_)[h]; (im=*imp)!=NULL; imp = &im->next)
	{
		if (im->index == ifi->ifi_index)
			break;
	}

	// create new indexmap entry
	if (im == NULL) 
	{
		//LOG(1)( Log::CBQ,  "Creating new IndexMap" );
		im = new IndexMap;
		if (im == NULL)
			return 0;
		im->next = *imp;
		im->index = ifi->ifi_index;
		*imp = im;
	}

	// set attributes of indexmap entry
	im->type = ifi->ifi_type;
	im->flags = ifi->ifi_flags;
	if (tb[IFLA_ADDRESS]) {
		int alen = RTA_PAYLOAD(tb[IFLA_ADDRESS]);
		im->alen = alen; 
		if (alen > (int)sizeof(im->addr))
			alen = sizeof(im->addr);
		memcpy(im->addr, RTA_DATA(tb[IFLA_ADDRESS]), alen);
	} else {
		im->alen = 0;
		memset(im->addr, 0, sizeof(im->addr));
	}
	strcpy(im->name, (char*)RTA_DATA(tb[IFLA_IFNAME]));
	return 0;
}

const char* 
LLInfoManager::idx_n2a(int idx, char *buf)
{
  return NULL;
}

//
// Static attributes
// 
LLInfoManager* LLInfoManager::instance_ = NULL;
RTNetlink::rtnl_handle* LLInfoManager::tc_handle_ = NULL;
bool LLInfoManager::initialised_ = false;

//
// init
// 
void
LLInfoManager::init(RTNetlink::rtnl_handle* tc_handle)
{
 if (!initialised_)
 {
 	tc_handle_ = tc_handle;
 	initialised_ = true;
 }
}


LLInfoManager*
LLInfoManager::instance()
{
	if (instance_ == NULL)
	{
		instance_ = new LLInfoManager;
		
		if (RTNetlink::rtnl_wilddump_request(tc_handle_, AF_UNSPEC, RTM_GETLINK) < 0) {
			return NULL;
		}

		if (RTNetlink::rtnl_dump_filter(tc_handle_, &LLInfoManager::remember_index, instance_, NULL, NULL) < 0) {
			return NULL;
		}
		
	}
	return instance_;
}
		
int 
LLInfoManager::name_to_index(const char *name)
{
	IndexMap *im;
	int i;

	if (name == NULL)
		return 0;
	if (icache_ && strcmp(name, namecache_) == 0)
		return icache_;
	for (i=0; i<16; i++) {
		for (im = idxmap_[i]; im; im = im->next) {
			if (strcmp(im->name, name) == 0) {
				icache_ = im->index;
				strcpy(namecache_, name);
				return im->index;
			}
		}
	}
	return 0;
}

const char* 
LLInfoManager::index_to_name(int idx)
{
	IndexMap* im;
	
	if (idx == 0)
		return "*";
	for (im = idxmap_[idx&0xF]; im; im = im->next)
	{
		if (im->index == idx)
			return im->name;
	}
	snprintf(namebuf_, 16, "if%d", idx);
	return namebuf_;
}

int 
LLInfoManager::index_to_type(int idx)
{
	IndexMap* im;

	if (idx == 0)
		return -1;
	for (im = idxmap_[idx&0xF]; im; im = im->next)
		if (im->index == idx)
			return im->type;
	return -1;
}

unsigned 
LLInfoManager::index_to_flags(int idx)
{
	IndexMap* im;

	if (idx == 0)
		return 0;

	for (im = idxmap_[idx&0xF]; im; im = im->next)
		if (im->index == idx)
			return im->flags;
	return 0;
}

void 
LLInfoManager::init_mgmt_info( CBQ_Class* root_class_, CBQ_Class* rsvp_class_, unsigned int mtu )
{
	mgmt_info_ = new CBQ_Mgmt_Info;

	/* These parameters must be settable via config file! */
	mgmt_info_->ai_->epsilon_ = 1e-6;
	mgmt_info_->ai_->g_max_ = 15;
	mgmt_info_->ai_->cl_max_ = 256;
	mgmt_info_->ai_->mem_max_ = 1024*1024;
	mgmt_info_->ai_->interval_ = 1;
	mgmt_info_->ai_->time_const_ = 8;
	mgmt_info_->laddend_ = 0;
	mgmt_info_->lmtu_ = root_class_->wrr_.allot;
	mgmt_info_->wfactor_ = (float)rsvp_class_->rate_.rate/rsvp_class_->wrr_.weight;
	// TODO: get MTU from somewhere else
	mgmt_info_->llhead_ = mgmt_info_->lmtu_ - mtu;

	mgmt_info_->lmaxlatency_ = (double)mgmt_info_->lmtu_/root_class_->rate_.rate;
	//if (qi->link->ifl_min_latency == 0)
	//	qi->link->ifl_min_latency = 1000000*(double)cbq->lll.rate.mpu/cbq->lll.rate.rate;

	/* Calculate some values */
	mgmt_info_->ai_->ewma_const_ = exp(-(float)mgmt_info_->ai_->interval_/mgmt_info_->ai_->time_const_);
	mgmt_info_->ai_->bw_max_ = rsvp_class_->rate_.rate;

	mgmt_info_->D_ = mgmt_info_->lmaxlatency_ + 2*(double)mgmt_info_->lmtu_*mgmt_info_->ai_->g_max_/root_class_->rate_.rate;
	mgmt_info_->C_ = (double)mgmt_info_->lmtu_*rsvp_class_->rate_.rate/root_class_->rate_.rate;

	/* Not sure what the following does
	 * 
	for (cl = cbq->lll.next; cl; cl = cl->next) {
		qi->ai.g_max--;
		qi->ai.bw_max -= cl->rate.rate;
		log(LOG_ERR, 0, "CBQ@%s: stale class %08x eated %dbps\n",
		    qi->link->ifl_name, cl->classid, cl->rate.rate);
	}

	if (qi->ai.g_max <= 0 || qi->ai.bw_max <= 0) {
		log(LOG_ERR, 0, "CBQ@%s: no capacities for reservations.\n",
		    qi->link->ifl_name);
		return -1;
	}
	*/
}

#endif // Linux && Real_Network
