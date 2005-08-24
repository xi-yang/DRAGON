/****************************************************************************

  KOM RSVP Engine (release version 2.1)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version
  2 of the License, or (at your option) any later version.

  Author: Aart van Halteren (a.t.vanhalteren@kpn.com)
  Date: December 2000

  Based on the TrafficControl software written by Alexey Kuznetsov (kuznet@ms2.inr.ac.ru)

*/

#ifndef _LLInfoManager_h_
#define _LLInfoManager_h_

#if defined(Linux) && defined(ENABLE_CBQ) && defined(REAL_NETWORK)

#include "libnetlink.h"

struct CBQ_Class  {
	__u32 classid_; /* handle for cbq instance */
	__u32 parent_; /* handle for parent of cbq instance */
	struct tc_cbq_lssopt	lss_;
	struct tc_cbq_wrropt	wrr_;
	struct tc_ratespec		rate_;
	CBQ_Class() : classid_(TC_H_UNSPEC), parent_(TC_H_UNSPEC) {};
};

struct CBQ_Admission_Info
{
/* Parameters */
	int			bw_max_;
	float		epsilon_;
	float		interval_;
	float		time_const_;
	int			cl_max_;
	int			g_max_;
	int			mem_max_;

/* State */
	int			cl_num_;
	int			cl_sum_;
	float		clp2_sum_;
	int			cl_rate_;
	int			cl_mem_;
	int			cl_cur_rate_;
	int			cl_cur_mem_;

	int			g_num_;
	int			g_sum_;

	int			mem_cur_;

	time_t		last_m_;
	float		ewma_const_;
	float		mu_;
};

struct CBQ_Mgmt_Info
{
	float		C_;
	float		D_;
	float		wfactor_;
	float		lminlatency_;
	float		lmaxlatency_;
	int			laddend_;
	int			lmtu_;
	int			llhead_;
	CBQ_Admission_Info* ai_;
	CBQ_Mgmt_Info(): ai_(new CBQ_Admission_Info) {}
};

class IndexMap
{
  public:
	IndexMap* next;
	int		index;
	int		type;
	int		alen;
	unsigned	flags;
	unsigned char	addr[8];
	char		name[16];
};

class LLInfoManager
{
	private:
		static bool initialised_;
		static RTNetlink::rtnl_handle* tc_handle_;
		char namebuf_[16];
		char namecache_[16];
		int icache_;
		IndexMap* idxmap_[16]; 
		static LLInfoManager* instance_;
		static int remember_index(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
		const char* idx_n2a(int idx, char *buf);
	protected:
		//ctor, protected
		LLInfoManager():icache_(0){
			memset( namebuf_, 0, sizeof(namebuf_) );
			memset( namecache_, 0, sizeof(namecache_) );
			memset( idxmap_, 0, sizeof(idxmap_) );
		};
	
	public:
		static LLInfoManager* instance();
		static void init(RTNetlink::rtnl_handle* tc_handle);
		int name_to_index(const char *name);
		const char* index_to_name(int idx);
		int index_to_type(int idx);
		unsigned index_to_flags(int idx);

		CBQ_Mgmt_Info* mgmt_info_;
		void init_mgmt_info( CBQ_Class*, CBQ_Class*, unsigned int );
};

#endif // Linux && Real_Network

#endif // _LLInfoManager_h_
