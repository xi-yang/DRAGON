/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

  Note: All code to interface with Linux traffic control is based on code
  written by Aart van Halteren (a.t.vanhalteren@kpn.com)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  Contact:	Martin Karsten
		TU Darmstadt, FG KOM
		Merckstr. 25
		64283 Darmstadt
		Germany
		Martin.Karsten@KOM.tu-darmstadt.de

  Other copyrights might apply to parts of this package and are so
  noted when applicable. Please see file COPYRIGHT.other for details.

****************************************************************************/

/* IMPORTANT NOTICE:
	the configuration of link bandwidth is currently hard-coded as follows:
	usable bandwidth:			70% of total bandwidth
	signalling bandwidth:  2% of total bandwidth
	reservable bandwidth: 40% of total bandwidth
	default class: 				28% of total bandwidth
	for FreeBSD and Solaris only!!
*/


#if defined(ENABLE_CBQ)

#include "RSVP_SchedulerCBQ.h"

#include "RSVP_IntServObjects.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_ProtocolObjects.h"
#include "SystemCallCheck.h"

#include <math.h>
#include <fcntl.h>

#if defined(FreeBSD)
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/linker.h>
#endif

#if defined(Linux)
#include <linux/if.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <iosfwd>
#if defined(HAVE_SSTREAM)
#include <sstream>
#else
#include <strstream>
#define ostringstream ostrstream 
#endif

#define RSVP_DEF_FPRIO4   TC_H_MAKE((1<<16), 0)

uint32 SchedulerCBQ::t2us_ = 1;
uint32 SchedulerCBQ::us2t_ = 1;
double SchedulerCBQ::tick_in_usec_ = 1;
RTNetlink::rtnl_handle* SchedulerCBQ::tc_handle_ = NULL;

#else

#define RM_FILTER_GAIN  5
#define RM_MINQSZ 10 

int SchedulerCBQ::cbqfd = -1;

#endif

#if defined(HAVE_KLD)
static int kernelmod_fileid  = 0;
#endif

#define FLOW_COUNT_CHUNK 100
#define NS_PER_MS (1000000.0)
#define NS_PER_SEC  (NS_PER_MS*1000.0)

static int initCounter = 0;

void SchedulerCBQ::updateADSPEC() {
	if ( flowCounter >= flowAdspecCount ) {
		flowAdspecCount += FLOW_COUNT_CHUNK;
	} else if ( flowCounter < flowAdspecCount - FLOW_COUNT_CHUNK ) {
		flowAdspecCount -= FLOW_COUNT_CHUNK;
	} else {
		return;
	}
/*
	// see Notes on CBQ and GS (S.Floyd)
	// for M we take the lower bound: 2 * flowAdspecCount * MTU
	// Note that these error terms depend on the number of flows because of WRR!!
	AdSpecGSParameters localAdspecGSparameters(
		0, ((2*flowAdspecCount*MTU) / (totalBw/8)) * NS_PER_MS,
		0, ((2*flowAdspecCount*MTU) / (totalBw/8)) * NS_PER_MS
	);
	adspec->addGS( localAdspecGSparameters );
*/
}

ADSPEC_Object* SchedulerCBQ::init( const LogicalInterface& lif, uint32 &serviceSupport ) {

	LOG(2)( Log::CBQ, "CBQ: enabling on interface", lif.getName() );

	initCounter += 1;
	if ( initCounter == 1 ) {
#if defined(Linux)
		FILE *fp = fopen("/proc/net/psched", "r");
		if (fp == NULL) return NULL;
		if (fscanf(fp, "%08x%08x", &t2us_, &us2t_) != 2) {
			fclose(fp);
			return NULL;
		}
		fclose(fp);
		tick_in_usec_ = (double)t2us_/us2t_;
		tc_handle_ = new RTNetlink::rtnl_handle;
		if (RTNetlink::rtnl_open(tc_handle_, 0) < 0) {
			LOG(1)( Log::CBQ, "cannot open rtnetlink");
			RTNetlink::rtnl_close(tc_handle_);
			delete tc_handle_; tc_handle_ = NULL;
			return NULL;
		}
		LOG(1)( Log::CBQ, "Opened RTNetlink socket");
		LOG(2)( Log::CBQ, "No. of ticks in a usec =", tick_in_usec_);
		LLInfoManager::init(tc_handle_);
#else
		cbqfd = open(CBQ_DEVICE, O_RDWR);
#if defined(HAVE_KLD)
		if ( cbqfd < 0 ) {
			struct stat sbuf;
			if ( stat( "/modules/altq_cbq.ko", &sbuf ) >= 0 ) {
				kernelmod_fileid = kldload("/modules/altq_cbq.ko");
			}
			cbqfd = open(CBQ_DEVICE, O_RDWR);
		}
#endif
		if ( cbqfd < 0 ) {
			LOG(1)( Log::CBQ, "CBQ: cannot open " CBQ_DEVICE " -> CBQ support disabled" );
	return NULL;
		}
#endif
	}

#if defined(Linux)

	ll_info_ = LLInfoManager::instance();

	// Remember the interface name where this Scheduler is instantiated
	ifname_ = lif.getName();
	
	LOG(2)( Log::CBQ, "CBQ: enabling on interface", ifname_ );
	LOG(4)( Log::CBQ, "The index of", ifname_, "is:", ll_info_->name_to_index(ifname_.chars()));

	//
	// Dump QDISC
	// 
	if (RTNetlink::rtnl_wilddump_request(tc_handle_, AF_UNSPEC, RTM_GETQDISC) < 0) {
		LOG(1)( Log::CBQ, "cannot send dump request");
		RTNetlink::rtnl_close(tc_handle_);
		delete tc_handle_; tc_handle_ = NULL;
		return NULL;
	}

	if (RTNetlink::rtnl_dump_filter(tc_handle_, &SchedulerCBQ::dump_qdiscinfo, this, NULL, NULL) < 0) {
		LOG(1)( Log::CBQ, "dump terminated\n");
		RTNetlink::rtnl_close(tc_handle_);
		delete tc_handle_; tc_handle_ = NULL;
		return NULL;
	}

	//
	// Dump classes
	// 
	LOG(1)( Log::CBQ,  "About to dump TC classes");
	struct tcmsg t;
	memset(&t, 0, sizeof(t));
	t.tcm_family = AF_UNSPEC;
	// The LLInfoManager knows which index belongs to which interface name
	t.tcm_ifindex = ll_info_->name_to_index(ifname_.chars()); 

	if (RTNetlink::rtnl_dump_request(tc_handle_, RTM_GETTCLASS, &t, sizeof(t)) < 0) {
		LOG(1)( Log::CBQ, "cannot send dump request");
		RTNetlink::rtnl_close(tc_handle_);
		delete tc_handle_; tc_handle_ = NULL;
		return NULL;
	}

	if (RTNetlink::rtnl_dump_filter(tc_handle_, &SchedulerCBQ::dump_classinfo, this, NULL, NULL) < 0) {
		LOG(1)( Log::CBQ, "dump terminated");
		RTNetlink::rtnl_close(tc_handle_);
		delete tc_handle_; tc_handle_ = NULL;
		return NULL;
	}

	if ( !root_class_ || !rsvp_class_ ) {
		FATAL(1)( Log::Fatal, "FATAL ERROR: No root or rsvp class. You have to configure TC," );
		FATAL(1)( Log::Fatal, "FATAL ERROR: before starting RSVPD. See script 'cbqinit'." );
		abortProcess();
	}

	ll_info_->init_mgmt_info( root_class_, rsvp_class_, lif.getMTU() );		

	if (root_class_ && rsvp_class_ && add_filter_proto()) {
		LOG(3)(Log::CBQ, "Enabling CBQ scheduler for Linux, bandwidth",  8*(ieee32float_p)rsvp_class_->rate_.rate, "bps" ); 
		availBw = 8*(ieee32float_p)rsvp_class_->rate_.rate;
	} else {
		LOG(1)(Log::CBQ, "DISABLING CBQ scheduler for Linux");
		return NULL;
	}
	
#else

	this->MTU = lif.getMTU();
	this->nsPerByteTotal = ((1.0 / totalBw) * NS_PER_SEC * 8);
	flowCounter = flowAdspecCount = 0;

	initMemoryWithZero( &cbqif, sizeof(cbqif) );
	strncpy(cbqif.cbq_ifacename, lif.getName().chars(), IFNAMSIZ);
#if defined(SunOS)
	cbqif.cbq_ifacelen = lif.getName().length();
#endif

#if defined(SunOS)
	if ( cbqfd < 0 || ioctl( cbqfd, CBQ_DISABLE, &cbqif ) < 0 ) {
#else
	if ( cbqfd < 0 || ioctl( cbqfd, CBQ_IF_ATTACH, &cbqif ) < 0 || ioctl( cbqfd, CBQ_DISABLE, &cbqif ) < 0 ) {
#endif
		LOG(2)( Log::CBQ, "CBQ: cannot enable", lif.getName() );
	return NULL;
	}
	CHECK( ioctl( cbqfd, CBQ_CLEAR_HIERARCHY, &cbqif ) );

	root_handle = cbq_create_class( NULL_CLASS_HANDLE, NULL_CLASS_HANDLE, 7, 0, 0, (uint32)(0.7*totalBw), 100, 2, 1, 1000, MTU );
                                    assert( root_handle != NULL_CLASS_HANDLE );
	default_handle = cbq_create_class( root_handle, root_handle, 6, 1, 0, (uint32)(0.28*totalBw), 100, 2, 1, 1000, MTU );
                                 assert( default_handle != NULL_CLASS_HANDLE );
	ctl_handle = cbq_create_class( root_handle, root_handle, 7, 0, 1, (uint32)(0.02*totalBw), 100, 2, 1, 1000, MTU );
                                     assert( ctl_handle != NULL_CLASS_HANDLE );
	resv_handle = cbq_create_class( root_handle, root_handle, 6, 0, 0, (uint32)(0.40*totalBw), 100, 2, 1, 1000, MTU );
                                    assert( resv_handle != NULL_CLASS_HANDLE );

	CHECK( ioctl(cbqfd, CBQ_ENABLE, &cbqif) );
	availBw = totalBw*0.4;
#endif

#if defined(ENABLE_ALTQ)
	int altq_fd = CHECK( open(ALTQ_DEVICE, O_RDWR) );
	struct tbrreq req; 
	copyMemory( req.ifname, &cbqif, sizeof(cbqif) );
	req.tb_prof.rate = (u_int)totalBw;
	req.tb_prof.depth = 1514*10;
	CHECK( ioctl( altq_fd, ALTQTBRSET, &req ) );
	CHECK( close( altq_fd ) );
#endif

	serviceSupport = (1 << ServiceHeader::Default)
									|(1 << ServiceHeader::ControlledLoad);

	adspec = new ADSPEC_Object( 1, availBw, latency, lif.getMTU() );
	updateADSPEC();
	LOG(2)( Log::CBQ, "SchedulerCBQ: localAdspec:", *adspec );
	return adspec;
}

SchedulerCBQ::~SchedulerCBQ() {
#if !defined(Linux)
	if ( root_handle != NULL_CLASS_HANDLE && cbqfd >= 0 ) {
#if defined(ENABLE_ALTQ)
		int altq_fd = CHECK( open(ALTQ_DEVICE, O_RDWR) );
		struct tbrreq req;
		copyMemory( req.ifname, &cbqif, sizeof(cbqif) );
		req.tb_prof.rate = 0;
		req.tb_prof.depth = 0;
		CHECK( ioctl( altq_fd, ALTQTBRSET, &req ) );
		CHECK( close( altq_fd ) );
#endif
		CHECK( ioctl( cbqfd, CBQ_DISABLE, &cbqif ) );
		CHECK( ioctl( cbqfd, CBQ_CLEAR_HIERARCHY, &cbqif ) );
	  initCounter -= 1;
	}
#else
	if ( ll_info_ ) {
		initCounter -= 1;
		delete ll_info_->mgmt_info_;
		delete ll_info_;
		ll_info_ = NULL;
	}
#endif
  if ( initCounter == 0 ) {
#if defined(Linux)
		if (tc_handle_) {
			RTNetlink::rtnl_close(tc_handle_);
			delete tc_handle_;
			tc_handle_ = NULL;
		}
#else
  	if ( cbqfd >= 0 ) {
			CHECK( close(cbqfd) );
			cbqfd = -1;
		}
#if defined(HAVE_KLD)
		if (kernelmod_fileid) kldunload(kernelmod_fileid);
#endif
#endif
	}
}

BaseScheduler::Reservation* SchedulerCBQ::internalMakeReservation( const RSB& ) {
	return new CBQ_Reservation;
}

bool SchedulerCBQ::modFlowspec( TrafficControl::RHandle* resv,
	const FLOWSPEC_Object& flowspec, uint8 pFlags,
	const FLOWSPEC_Object*& fwdFlowspec, const RSB&, TrafficControl::UpdateFlag,
	const FLOWSPEC_Object* replaceFlowspec ) {

	// add new class corresponding to parameters and possibly del old class

	ieee32float rate = flowspec.get_r() * 8;
           assert ( flowspec.getServiceNumber() != ServiceHeader::Guaranteed );

	if ( !admitSimple( *reinterpret_cast<CBQ_Reservation*>(resv), rate, (replaceFlowspec ? replaceFlowspec->getEffectiveRate() * 8 : 0) ) ) {
		return false;
	}

#if defined(Linux)

	char buf[4096];
	struct rtattr* rta = (struct rtattr*)buf;
	struct tc_cbq_police pol;
	uint32 rtab[256];
	
	LOG(2)( Log::CBQ, "CBQ: modifying Flowspec", flowspec );
	CBQ_Reservation* cbq_resv = reinterpret_cast<CBQ_Reservation*>(resv);
	CBQ_Class* cbq_class = cbq_resv->cbq_class_;

	// set parent classid to rsvp_class
	cbq_class->parent_ = rsvp_class_->classid_;

	// set all relevant parameters for class
	// TODO: choose proper values!!
	// 
	cbq_class->lss_ = rsvp_class_->lss_;
	cbq_class->wrr_ = rsvp_class_->wrr_;
	cbq_class->wrr_.weight = (uint32)rint(flowspec.getEffectiveRate()/ll_info_->mgmt_info_->wfactor_);
	//cbq_class->wrr_.weight = rsvp_class_->wrr_.weight;
	cbq_class->wrr_.priority = 1;
	cbq_class->rate_ = rsvp_class_->rate_;
	cbq_class->rate_.rate = (uint32)rint(rate);
	LOG(3)( Log::CBQ, "CBQ: setting rate to", cbq_class->rate_.rate, "bits/s");
	cbq_class->lss_.flags = 0;
	cbq_class->lss_.change = 2|4|0x10|0x20;
	cbq_class->lss_.flags |= TCF_CBQ_LSS_ISOLATED; // make sure this class cannot borrow bytes to others.
	cbq_class->lss_.change |= TCF_CBQ_LSS_FLAGS;

	cbq_class->lss_.maxidle = tc_cbq_calc_maxidle(root_class_->rate_.rate,
						    cbq_class->rate_.rate,
						    cbq_class->lss_.avpkt,
						    cbq_class->lss_.ewma_log,
						    ll_info_->mgmt_info_->ai_->cl_mem_/root_class_->lss_.avpkt);
	cbq_class->lss_.offtime = tc_cbq_calc_offtime(root_class_->rate_.rate,
						    cbq_class->rate_.rate,
						    cbq_class->lss_.avpkt,
						    cbq_class->lss_.ewma_log,
						    1);

	tc_calc_rtable(cbq_class->rate_.rate, rtab, 8, cbq_class->wrr_.allot, cbq_class->rate_.mpu);
	
	memset(&pol, 0, sizeof(pol));
	pol.police = TC_POLICE_RECLASSIFY;

	// set the RTA buffer
	rta->rta_type = TCA_OPTIONS;
	rta->rta_len  = RTA_LENGTH(0);
	RTNetlink::rta_addattr_l(rta, sizeof(buf), TCA_CBQ_LSSOPT, &cbq_class->lss_, sizeof(cbq_class->lss_));
	RTNetlink::rta_addattr_l(rta, sizeof(buf), TCA_CBQ_WRROPT, &cbq_class->wrr_, sizeof(cbq_class->wrr_));
	RTNetlink::rta_addattr_l(rta, sizeof(buf), TCA_CBQ_RATE, &cbq_class->rate_, sizeof(cbq_class->rate_));
	RTNetlink::rta_addattr_l(rta, sizeof(buf), TCA_CBQ_RTAB, &rtab, sizeof(rtab));
	RTNetlink::rta_addattr_l(rta, sizeof(buf), TCA_CBQ_POLICE, &pol, sizeof(pol));

	if (tc_add_class(cbq_class,rta)) {
		LOG(1)( Log::CBQ, " -> Unsuccesful in modifying Flowspec" );
		return false;
	}
	LOG(2)( Log::CBQ, "* Added class *  -> ", print_tc_classid(cbq_class->classid_) );

#else

	u_int bandwidth = (u_int)rate;

	// determines maxq (number of av_pkt_size packets in queue)
	u_int maxdelay = (u_int)(8000 * flowspec.get_b() / rate);

	//determines maxidle, b/m is worst case(for CL one may also take (M+m)/2)
	u_int maxburst = (u_int)(flowspec.get_b() / flowspec.get_m());
	if ( flowspec.getServiceNumber() == ServiceHeader::ControlledLoad ) {
		maxburst = (flowspec.get_M() + flowspec.get_m()) / 2;
	}

	// determines gtom; determines offtime (worst case again here)
	u_int minburst = 1;

	// av_pkt_size is used to compute packet service time
	u_int av_pkt_size = flowspec.get_m();

	// determines minidle and max_ptime
	u_int max_pkt_size = flowspec.get_M();

	u_long new_handle = cbq_create_class( resv_handle, resv_handle, 6,
		0, 0, bandwidth, maxdelay, maxburst, minburst, av_pkt_size, max_pkt_size );

                                     assert( new_handle != NULL_CLASS_HANDLE );

	if ( reinterpret_cast<CBQ_Reservation*>(resv)->rhandle != NULL_CLASS_HANDLE ) {
		// reset all filters from old class to new class;
		BaseScheduler::FList::Iterator filterIter = reinterpret_cast<CBQ_Reservation*>(resv)->filterList.begin();
		for ( ; filterIter != reinterpret_cast<CBQ_Reservation*>(resv)->filterList.end(); ++filterIter ) {
			struct cbq_delete_filter fltr_del;
			copyMemory( &fltr_del.cbq_iface, &cbqif, sizeof(cbqif) );
			fltr_del.cbq_filter_handle = reinterpret_cast<const CBQ_Filter*>(*filterIter)->fhandle;
			CHECK( ioctl(cbqfd, CBQ_DEL_FILTER, &fltr_del) );
			setFilter( resv, reinterpret_cast<CBQ_Filter*>(*filterIter)->session, reinterpret_cast<const CBQ_Filter*>(*filterIter)->sender, reinterpret_cast<CBQ_Filter&>(**filterIter) );
		}
		// delete old class
		struct cbq_delete_class cdc;
		copyMemory( &cdc.cbq_iface, &cbqif, sizeof(cbqif) );
		cdc.cbq_class_handle = reinterpret_cast<const CBQ_Reservation*>(resv)->rhandle;
		CHECK( ioctl(cbqfd, CBQ_DEL_CLASS, &cdc ) );
		return false;
	}
	reinterpret_cast<CBQ_Reservation*>(resv)->rhandle = new_handle;

#endif

	flowCounter += 1;
	updateADSPEC();
	fwdFlowspec = &flowspec;
	return true;
}

void SchedulerCBQ::delFlowspec( const TrafficControl::RHandle* resv ) {
#if defined(Linux)
	__u32 h = reinterpret_cast<const CBQ_Reservation*>(resv)->cbq_class_->classid_;
	LOG(2)( Log::CBQ, "Deleting CBQ class", print_tc_classid(h) );
	if ( tc_kill_class( h ) ) {
		ERROR(2)( Log::Error, "ERROR: couldn't delete class", print_tc_classid(h) );
	}
#else
	struct cbq_delete_class cdc;
	copyMemory( &cdc.cbq_iface, &cbqif, sizeof(cbqif) );
	cdc.cbq_class_handle = reinterpret_cast<const CBQ_Reservation*>(resv)->rhandle;
	CHECK( ioctl(cbqfd, CBQ_DEL_CLASS, &cdc ) );
#endif
	flowCounter -= 1;
	updateADSPEC();
	releaseSimple( *reinterpret_cast<const CBQ_Reservation*>(resv) );
	delete reinterpret_cast<const CBQ_Reservation*>(resv);
}

bool SchedulerCBQ::setFilter( TrafficControl::RHandle* resv,
	const SESSION_Object& session, const SENDER_Object& sender,
	SchedulerCBQ::CBQ_Filter& filter ) {

#if defined(Linux)

	RTNetlink::rtnl_dialog d;
	char buf_for_msghdr[4096];
	struct nlmsghdr *n = (struct nlmsghdr*)buf_for_msghdr;
	struct nlmsghdr *h;
	struct tcmsg* tc_reply_msg;
 	char buf_for_tcmsg[4096];	
	struct tcmsg *t = (struct tcmsg*)buf_for_tcmsg;
	int err;
	struct iovec iov[3];
	int ct = 0; // This is used in rtnl_iov_set macro.
	struct tc_rsvp_pinfo pinfo;
	uint32  dst[4];
	uint32  src[4];
	int dlen = 0;
	int slen = 0;
	char buf_for_rta[4096];
	struct rtattr* rta = (struct rtattr*)buf_for_rta;

	// set values for pinfo structure
	memset(&pinfo, 0, sizeof(pinfo));
	/* Encode destination port to DPI 
	 * Note that no support for GPI is provided. 
	 * See  tc_add_filter() in isi_rsvp/rsvpd/tc_filter.c for further details
	 */
	/* Encode destination IPv4 address/port */
	pinfo.dpi.key = htonl(session.getTunnelId());
	if (pinfo.dpi.key) pinfo.dpi.mask = htonl(0x0000FFFF);
	dst[0] = session.getDestAddress().rawAddress();
	dlen = session.getDestAddress().size();
	/* Encode source IPv4 address/port */
	pinfo.spi.key = htonl(sender.getLspId() << 16);
	if (pinfo.spi.key) pinfo.spi.mask = htonl(0xFFFF0000);
	if ( sender.getSrcAddress() ) {
		src[0] = sender.getSrcAddress().rawAddress();
		slen = sender.getSrcAddress().size();
	}
	pinfo.protocol = 17;

	// create NETLINK message
	rtnl_iov_set(n, sizeof(*n));
	memset(n, 0, sizeof(*n));
	n->nlmsg_type = RTM_NEWTFILTER;
	n->nlmsg_flags =  NLM_F_ECHO|NLM_F_CREATE|NLM_F_EXCL;
	n->nlmsg_len = NLMSG_LENGTH(sizeof(*t));
	//t = (struct tcmsg *)NLMSG_DATA(n);

	rtnl_iov_set(t, sizeof(*t));
	memset(t, 0, sizeof(*t));
	t->tcm_parent = root_qdisc_handle_; // TODO: check if this correct
	t->tcm_handle = 0;
	t->tcm_ifindex = ll_info_->name_to_index(ifname_.chars());
	t->tcm_family = AF_INET;
	t->tcm_info = TC_H_MAKE(RSVP_DEF_FPRIO4, htons(ETH_P_IP));

	RTNetlink::addattr_l(n, sizeof(buf_for_msghdr), TCA_KIND, (char*)"rsvp", strlen("rsvp")+1);

	rta->rta_type = TCA_OPTIONS;
	rta->rta_len = RTA_LENGTH(0);

	RTNetlink::rta_addattr_l(rta, sizeof(buf_for_rta), TCA_RSVP_DST, dst, dlen);
	if (slen) RTNetlink::rta_addattr_l(rta, sizeof(buf_for_rta), TCA_RSVP_SRC, src, slen);
	RTNetlink::rta_addattr_l(rta, sizeof(buf_for_rta), TCA_RSVP_PINFO, &pinfo, sizeof(pinfo));
	uint32 classid = reinterpret_cast<CBQ_Reservation*>(resv)->cbq_class_->classid_;
	RTNetlink::rta_addattr_l(rta, sizeof(buf_for_rta), TCA_RSVP_CLASSID, &(classid), 4);

	// TODO: policing is not yet supported!
	//if (f->flow->policer.action == TC_POLICE_RECLASSIFY)
	//	tca_add_policing(rta, sizeof(buf), TCA_RSVP_POLICE, f->flow->qi, &f->flow->policer);

	rtnl_iov_set(rta, rta->rta_len);

	// Talk to kernel
	err = RTNetlink::rtnl_ask(tc_handle_, iov, ct, &d, buf_for_rta, sizeof(buf_for_rta));
	if (err) {
		ERROR(4)( Log::Error, "ERROR: CBQ could not add filter for", sender, "Error no.:", err );
		return false;
	}
	
	while ((h = RTNetlink::rtnl_wait(tc_handle_, &d, &err)) != NULL)  {
		tc_reply_msg = (struct tcmsg *)NLMSG_DATA(h);
		if (h->nlmsg_type != RTM_NEWTFILTER)
	continue;
		LOG(4)( Log::CBQ, "CBQ: added filter for", sender, "Returned filter handle:", print_tc_classid(tc_reply_msg->tcm_handle) );
		filter.fhandle = tc_reply_msg->tcm_handle;
		return true;
	}
                                                                  assert(err);
	// Something must have gone wrong. Print error message and return null-filter.
	ERROR(4)( Log::Error, "ERROR: CBQ could not add filter for", sender, "Error no.:", err );
	return false;

#else

	struct cbq_add_filter fltr_add;
	initMemoryWithZero(&fltr_add, sizeof(fltr_add));
	copyMemory( &fltr_add.cbq_iface, &cbqif, sizeof(cbqif) );
	fltr_add.cbq_class_handle = reinterpret_cast<CBQ_Reservation*>(resv)->rhandle;
#if defined(SunOS)
	fltr_add.cbq_filter.daddr = session.getDestAddress().rawAddress();
	fltr_add.cbq_filter.saddr = sender.getSrcAddress().rawAddress();
	fltr_add.cbq_filter.sport = htons(sender.getLspId());
	fltr_add.cbq_filter.dport = htons(session.getTunnelId());
	fltr_add.cbq_filter.proto = 17;
#else
	fltr_add.cbq_filter.ff_flow.fi_dst.s_addr = session.getDestAddress().rawAddress();
	fltr_add.cbq_filter.ff_flow.fi_src.s_addr = sender.getSrcAddress().rawAddress();
	fltr_add.cbq_filter.ff_flow.fi_sport = htons(sender.getLspId());
	fltr_add.cbq_filter.ff_flow.fi_dport = htons(session.getTunnelId()); 
	fltr_add.cbq_filter.ff_flow.fi_proto = 17;
	fltr_add.cbq_filter.ff_flow.fi_family = AF_INET;
	fltr_add.cbq_filter.ff_flow.fi_len = sizeof(struct flowinfo_in);
	if ( fltr_add.cbq_filter.ff_flow.fi_dst.s_addr != 0 ) {
		fltr_add.cbq_filter.ff_mask.mask_dst.s_addr = 0xffffffff;
	}
	if ( fltr_add.cbq_filter.ff_flow.fi_src.s_addr != 0 ) {
		fltr_add.cbq_filter.ff_mask.mask_src.s_addr = 0xffffffff;
	}
#endif
	if ( ioctl(cbqfd, CBQ_ADD_FILTER, &fltr_add) == 0 ) {
		filter.fhandle = fltr_add.cbq_filter_handle;
		return true;
	}
	return false;

#endif

}

BaseScheduler::Filter* SchedulerCBQ::addFilter( TrafficControl::RHandle* resv,
	const SESSION_Object& session, const SENDER_Object& sender ) {

	SchedulerCBQ::CBQ_Filter* filter = new CBQ_Filter( sender, session, *reinterpret_cast<CBQ_Reservation*>(resv), 0 );
	if ( setFilter( resv, session, sender, *filter ) ) {
		LOG(2)( Log::CBQ, "CBQ: added filter for", sender );
		return filter;
	} else {
		ERROR(2)( Log::Error, "ERROR: CBQ could not add filter for", sender );
		delete filter;
		return NULL;
	}
}

void SchedulerCBQ::delFilter( const TrafficControl::FHandle* filter ) {
#if defined(Linux)
	char buf[1024];
	struct nlmsghdr *n = (struct nlmsghdr*)buf;
	struct tcmsg *t;
	memset(n, 0, sizeof(*n));
	n->nlmsg_type = RTM_DELTFILTER;
	n->nlmsg_flags = NLM_F_REQUEST;
	n->nlmsg_len = NLMSG_LENGTH(sizeof(*t));
	t = (struct tcmsg *)NLMSG_DATA(n);
	memset(t, 0, sizeof(*t));
	t->tcm_parent = root_qdisc_handle_; 
	t->tcm_handle = reinterpret_cast<const CBQ_Filter*>(filter)->fhandle;
	t->tcm_ifindex = ll_info_->name_to_index(ifname_.chars());
	t->tcm_family = AF_INET;
	t->tcm_info = TC_H_MAKE(RSVP_DEF_FPRIO4, htons(ETH_P_IP));
	RTNetlink::rtnl_tell(tc_handle_, n);
	LOG(2)( Log::CBQ, "CBQ: Deleted filter with handle:", print_tc_classid(reinterpret_cast<const CBQ_Filter*>(filter)->fhandle) );
#else
	struct cbq_delete_filter fltr_del;
	copyMemory( &fltr_del.cbq_iface, &cbqif, sizeof(cbqif) );
	fltr_del.cbq_filter_handle = reinterpret_cast<const CBQ_Filter*>(filter)->fhandle;
	CHECK( ioctl(cbqfd, CBQ_DEL_FILTER, &fltr_del) );
#endif
	delete reinterpret_cast<const CBQ_Filter*>(filter);
}

#if !defined(Linux)

/*
 * Copyright (c) Sun Microsystems, Inc. 1996-1998 All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the SMCC Technology
 *      Development Group at Sun Microsystems, Inc.
 *
 * 4. The name of the Sun Microsystems, Inc nor may not be used to endorse or
 *      promote products derived from this software without specific prior
 *      written permission.
 *
 * SUN MICROSYSTEMS DOES NOT CLAIM MERCHANTABILITY OF THIS SOFTWARE OR THE
 * SUITABILITY OF THIS SOFTWARE FOR ANY PARTICULAR PURPOSE.  The software is
 * provided "as is" without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this software.
 */

u_long SchedulerCBQ::cbq_create_class( u_long parent_class,
	u_long borrow_class, u_int pri, u_int is_default, u_int is_ctl,
	u_int bandwidth, u_int maxdelay, u_int maxburst, u_int minburst,
	u_int av_pkt_size, u_int max_pkt_sz) {

	struct cbq_add_class cl_add;

	double maxq, maxidle_s, maxidle, minidle, offtime, nsPerByte, ptime, cptime;
	double z = (double)(1 << RM_FILTER_GAIN);
	double g = (1.0 - 1.0/ z);
	double f;
	double gton = pow(g, (double)maxburst);
	double gtom = pow(g, (double)minburst);
	double maxrate;

	double max_ptime;

	/*
	 * Sanity Checks for Bandwidth, avg packet size and max packet size.
	 */
	if (bandwidth == 0)
		f = 0.0001;
	else
		f = (double) bandwidth / totalBw;

	if (av_pkt_size == 0)
		av_pkt_size = 512;
	else if ( av_pkt_size > MTU )
		av_pkt_size = MTU;

	if (max_pkt_sz == 0)
		max_pkt_sz = MTU;
	else if (max_pkt_sz > MTU)
		max_pkt_sz = MTU;

	/*
	 * Compute other class parameters
	 */
	nsPerByte = nsPerByteTotal / f;
	ptime = (double)av_pkt_size * nsPerByteTotal;
	/* JS: also compute ptime based on max_pkt_size */
	max_ptime = (double)max_pkt_sz * nsPerByteTotal;
	maxrate = f * (totalBw / 8); 
	cptime = ptime * (1.0 - f) / f;
	maxidle = ((1.0 / f - 1.0) * ((1.0 - gton) / gton));
	maxidle_s = (1.0 - g);
	/* JS: compute maxidle based on max_ptime */
	if (maxidle > maxidle_s)
		maxidle = /*ptime*/max_ptime * maxidle;
	else
		maxidle = /*ptime*/max_ptime * maxidle_s;

	if (minburst) {
		offtime = cptime * (1.0 + (1.0/(1.0 - g) *
			(1.0 - gtom) / gtom));
	} else
		offtime = cptime;

	minidle = -((double)max_pkt_sz * nsPerByte);

	maxidle = ((maxidle * 8.0) / nsPerByte);
	offtime = ((offtime * 8.0) / nsPerByte) * pow(2, RM_FILTER_GAIN);
#ifndef USE_HRTIME
	maxidle = maxidle * pow(2, RM_FILTER_GAIN);
	maxidle = maxidle / 1000.0;
	offtime = offtime / 1000.0;
	minidle = minidle / 1000.0;
#endif
	if (maxdelay == 0) maxq = RM_MINQSZ;
	else {
		maxq = ((double) maxdelay * NS_PER_MS) /
			(nsPerByte * av_pkt_size); 
		if (maxq < RM_MINQSZ) maxq = RM_MINQSZ;
	}
#if defined(FreeBSD)
	if (maxq > CBQ_MAXQSIZE) maxq = CBQ_MAXQSIZE;
#endif

	/* create class */
	initMemoryWithZero( &cl_add, sizeof(cl_add) );

	copyMemory( &cl_add.cbq_iface, &cbqif, sizeof(cbqif) );
	cl_add.cbq_class.parent_class_handle = parent_class;
	cl_add.cbq_class.borrow_class_handle = borrow_class;
	cl_add.cbq_class.priority = pri;
	cl_add.cbq_class.nano_sec_per_byte = (u_int)fabs(nsPerByte);
	cl_add.cbq_class.maxq = (u_int) fabs(maxq);  /* currently ignored in kernel module */
	cl_add.cbq_class.maxidle = (u_int) fabs(maxidle);
	cl_add.cbq_class.minidle = (int)minidle;
	cl_add.cbq_class.offtime = (u_int) fabs(offtime);
#if defined(SunOS)
	cl_add.cbq_class.defaultclass = is_default;
	cl_add.cbq_class.ctlclass = is_ctl;
	cl_add.cbq_class.red = 0;               /* only for root class */
	cl_add.cbq_class.wrr = 1;               /* only for root class */
	cl_add.cbq_class.efficient = 1;         /* only for root class */
#else
//	cl_add.cbq_class.flags = CBQCLF_WRR | CBQCLF_EFFICIENT;
	cl_add.cbq_class.flags = CBQCLF_EFFICIENT;
	if ( is_default ) cl_add.cbq_class.flags |= CBQCLF_DEFCLASS;
	if ( is_ctl ) cl_add.cbq_class.flags |= CBQCLF_CTLCLASS;
	if ( parent_class == NULL_CLASS_HANDLE ) cl_add.cbq_class.flags |= CBQCLF_ROOTCLASS;
#endif

/*
		printf("cbq_create_class.%lx:\n", cl_add.cbq_class_handle);
		printf("\tavg packet size is %d\n", av_pkt_size);
		printf("\tbandwidth is %f\n", (double)bandwidth);
		printf("\tmaxrate is %f\n", maxrate);
		printf("\tmax, min, g, gton, gtom = %d, %d, %f, %f, %f\n",
			maxburst, minburst, g, gton, gtom);
		printf("\tclassptime, ptime = %f, %f\n", cptime, ptime);
		printf("\tparent class: 0x%lx\n", parent_class);
		printf("\tborrow class: 0x%lx\n", borrow_class);
		printf("\tpriority: %d\n", pri);
		printf("\tbandwidth (in nano sec's per byte): %d\n",
			(u_int)fabs(nsPerByte));
		printf("\tmax queue size: %d\n", (u_int) fabs(maxq));
		printf("\tminidle: %d\n", (int)minidle);
		printf("\tmaxidle: %d\n", (u_int) fabs(maxidle));
		printf("\tofftime: %d\n", (u_int)fabs(offtime));
		printf("\tdefault: %d\n", is_default);
		printf("\tctl: %d\n", is_ctl);
		printf("\tred: %d\n", 0);
		printf("\twrr: %d\n", 1);
		printf("\tefficient: %d\n", 1);
*/

	if ( ioctl(cbqfd, CBQ_ADD_CLASS, &cl_add) < 0 ) {
		return NULL_CLASS_HANDLE;
	} else {
		return cl_add.cbq_class_handle;
	}
}

#else

/* 
 * Note: The code to interface with Linux traffic control 
 * was written by Aart van Halteren (a.t.vanhalteren@kpn.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 * 
 * Date: December 2000
 * 
 */

int 
SchedulerCBQ::dump_classinfo(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	// NOTE: Look at 'store_classinfo' in isi-rsvp/rsvpd/tc_cbq.c.
	//       There you'll find how to get more CBQ specific attributes
	 
	struct tcmsg *t = (struct tcmsg *)NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr* tb[TCA_MAX+1];
	struct rtattr *rta[TCA_CBQ_MAX+1];
	SchedulerCBQ* scheduler = (SchedulerCBQ*)arg;

	LOG(1)( Log::CBQ, "in dump_classinfo" );
	
	if (n->nlmsg_type != RTM_NEWTCLASS) 
	{
		LOG(1)( Log::CBQ, "Not a class!!");
		return 0;
	}

	len -= NLMSG_LENGTH(sizeof(*t));
	if (len < 0) {
		LOG(2)( Log::CBQ,  "Wrong len", len);
		return -1;
	}

	if (scheduler->ll_info_->name_to_index(scheduler->ifname_.chars()) != t->tcm_ifindex)
	{
		LOG(4)( Log::CBQ,  "Wrong interface, index", t->tcm_ifindex, "does not match", \
							scheduler->ll_info_->name_to_index(scheduler->ifname_.chars()) );
		return 0;
	}

	memset(tb, 0, sizeof(tb));
	RTNetlink::parse_rtattr(tb, TCA_MAX, TCA_RTA(t), len);
	if (tb[TCA_OPTIONS])
	{
		memset(rta, 0, sizeof(rta));
		RTNetlink::parse_rtattr(rta, TCA_CBQ_MAX, (rtattr*)RTA_DATA(tb[TCA_OPTIONS]), RTA_PAYLOAD(tb[TCA_OPTIONS]));
		if (rta[TCA_CBQ_LSSOPT] == NULL || rta[TCA_CBQ_WRROPT] == NULL || rta[TCA_CBQ_RATE] == NULL)
		{
			LOG(1)( Log::CBQ,  "Wrong CBQ options (no LSS, WRR and/or RATE");
			return 0;
		}

		// check if we have the all-link class
		if (t->tcm_handle == TC_H_MAKE(scheduler->root_qdisc_handle_, 0)) 
		{
			scheduler->root_class_ = new CBQ_Class;
			CBQ_Class* cl = scheduler->root_class_;
			cl->classid_ = t->tcm_handle;
			cl->parent_ = t->tcm_parent;
			memcpy(&cl->lss_, RTA_DATA(rta[TCA_CBQ_LSSOPT]), sizeof(cl->lss_));
			memcpy(&cl->wrr_, RTA_DATA(rta[TCA_CBQ_WRROPT]), sizeof(cl->wrr_));
			memcpy(&cl->rate_, RTA_DATA(rta[TCA_CBQ_RATE]), sizeof(cl->rate_));
			LOG(2)( Log::CBQ,  "** ROOT class **   rate =", print_rate(cl->rate_.rate) );
		}

		// check if we have the reserved class
		if (t->tcm_handle == TC_H_MAKE(scheduler->root_qdisc_handle_, 0x7FFE)) 
		{
			scheduler->rsvp_class_ = new CBQ_Class;
			CBQ_Class* cl = scheduler->rsvp_class_;
			cl->classid_ = t->tcm_handle;
			cl->parent_ = t->tcm_parent;
			memcpy(&cl->lss_, RTA_DATA(rta[TCA_CBQ_LSSOPT]), sizeof(cl->lss_));
			memcpy(&cl->wrr_, RTA_DATA(rta[TCA_CBQ_WRROPT]), sizeof(cl->wrr_));
			memcpy(&cl->rate_, RTA_DATA(rta[TCA_CBQ_RATE]), sizeof(cl->rate_));
			LOG(2)( Log::CBQ,  "** RSVP (reserved) class **   rate =", print_rate(cl->rate_.rate) );
		}
	
	}

	if (tb[TCA_KIND] == NULL) {
		LOG(1)( Log::CBQ,  "NULL kind");
		return -1;
	}

#if defined(LOG_ON)
	String logString;
	logString += "class ";
	logString += (char*)RTA_DATA(tb[TCA_KIND]);
	logString += " ";
	if ( t->tcm_handle ) logString += print_tc_classid(t->tcm_handle);
	logString += " dev ";
	logString += LLInfoManager::instance()->index_to_name(t->tcm_ifindex);

	if (t->tcm_parent == TC_H_ROOT) {
		logString += " root";
	} else  {
		logString += " parent "; 
		logString += print_tc_classid(t->tcm_parent);
	}
	if (t->tcm_info) {
		logString += " leaf";
//		logString += (void*)(t->tcm_info>>16);
	}
	LOG(1)( Log::CBQ,  logString);
#endif

	return 0;
}

int 
SchedulerCBQ::dump_qdiscinfo(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct tcmsg *t =  (struct tcmsg *)NLMSG_DATA(n);
	struct rtattr *rta[TCA_MAX+1];
	SchedulerCBQ* scheduler = (SchedulerCBQ*)arg;

	if (n->nlmsg_type != RTM_NEWQDISC)
		return 0;
	if (n->nlmsg_len < NLMSG_LENGTH(sizeof(t)))
		return -1;
	if (scheduler->ll_info_->name_to_index(scheduler->ifname_.chars()) != t->tcm_ifindex)
		return 0;
	if (RTNetlink::parse_rtattr(rta, TCA_MAX, TCA_RTA(t), TCA_PAYLOAD(n)) < 0)
		return -1;

	LOG(5)( Log::CBQ,  "qdisc:", (char*)RTA_DATA(rta[TCA_KIND]), (void*)((t->tcm_handle)>>16), \
	                   "dev", LLInfoManager::instance()->index_to_name(t->tcm_ifindex) );
	
	if (t->tcm_parent == TC_H_ROOT) {
		LOG(1)( Log::CBQ,  "root" );
	} else if (t->tcm_parent) {
		LOG(2)( Log::CBQ, "parent", print_tc_classid(t->tcm_parent) );	
	}
	
	if (t->tcm_info != 1) {
		LOG(2)( Log::CBQ, "refcnt ", t->tcm_info );
	}

	// check if we have the root CBQ
	// the handle is stored as an attribute of the scheduler
	if ( String("cbq") == String((char*)RTA_DATA(rta[TCA_KIND])) ) // && (t->tcm_parent == TC_H_ROOT)
	{
		scheduler->root_qdisc_handle_ = t->tcm_handle;
		LOG(2)( Log::CBQ, "root_qdisc_handle_ =", (void*)((scheduler->root_qdisc_handle_)>>16) );
	}

	return 0;

}

bool
SchedulerCBQ::add_filter_proto()
{
	char buf[512];
	struct nlmsghdr* n = (struct nlmsghdr*)buf;
	int err;

	memset(n, 0, sizeof(*n));
	n->nlmsg_type = RTM_NEWTFILTER;
	n->nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
	n->nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	struct tcmsg* t = (struct tcmsg*)NLMSG_DATA(n);
	memset(t, 0, sizeof(*t));
	t->tcm_family = AF_INET;
	t->tcm_parent = root_qdisc_handle_;
	t->tcm_ifindex = ll_info_->name_to_index(ifname_.chars());;
	t->tcm_info = TC_H_MAKE(RSVP_DEF_FPRIO4, htons(ETH_P_IP));
	RTNetlink::addattr_l(n, sizeof(buf), TCA_KIND, (char*)"rsvp", strlen("rsvp")+1 );

	err = RTNetlink::rtnl_tell(tc_handle_, n);
	if (err) {
		ERROR(4)( Log::Error, "ERROR: CBQ could not add proto_filter. errno =", errno, "err =", err);
		return false;
	}
	return true;
}

unsigned 
SchedulerCBQ::tc_cbq_calc_maxidle(unsigned bndw, unsigned rate, unsigned avpkt,
			     int ewma_log, unsigned maxburst)
{
	double maxidle;
	double g = 1.0 - 1.0/(1<<ewma_log);
	double xmt = (double)avpkt/bndw;

	maxidle = xmt*(1-g);
	if (bndw != rate && maxburst) {
		double vxmt = (double)avpkt/rate - xmt;
		vxmt *= (pow(g, -(double)maxburst) - 1);
		if (vxmt > maxidle)
			maxidle = vxmt;
	}
	return tc_usec2tick((long)(maxidle*(1<<ewma_log)*NS_PER_MS));
}

unsigned 
SchedulerCBQ::tc_cbq_calc_offtime(unsigned bndw, unsigned rate, unsigned avpkt,
			     int ewma_log, unsigned minburst)
{
	double g = 1.0 - 1.0/(1<<ewma_log);
	double offtime = (double)avpkt/rate - (double)avpkt/bndw;

	if (minburst == 0)
		return 0;
	if (minburst == 1)
		offtime *= pow(g, -(double)minburst) - 1;
	else
		offtime *= 1 + (pow(g, -(double)(minburst-1)) - 1)/(1-g);
	return tc_usec2tick((long)(offtime*NS_PER_MS));
}

/*
   rtab[pkt_len>>cell_log] = pkt_xmit_time
 */

int 
SchedulerCBQ::tc_calc_rtable(unsigned bps, uint32* rtab, int cell_log, unsigned mtu, unsigned mpu)
{
	int i;

	if (mtu == 0)
		mtu = 2047;

	if (cell_log < 0) {
		cell_log = 0;
		while ((mtu>>cell_log) > 255)
			cell_log++;
	}
	for (i=0; i<256; i++) {
		unsigned sz = (i<<cell_log);
		if (sz < mpu)
			sz = mpu;
		rtab[i] = tc_usec2tick((long)(NS_PER_MS*((double)sz/bps)));
	}
	return cell_log;
}

int 
SchedulerCBQ::tc_kill_class(uint32 classid)
{
	char buf[256];
	struct nlmsghdr *n = (struct nlmsghdr*)buf;
	struct tcmsg *t;

	memset(n, 0, sizeof(*n));
	n->nlmsg_type = RTM_DELTCLASS;
	n->nlmsg_flags = NLM_F_REQUEST;
	n->nlmsg_len = NLMSG_LENGTH(sizeof(*t));
	t = (struct tcmsg *)NLMSG_DATA(n);

	memset(t, 0, sizeof(*t));
	t->tcm_handle = classid;
	t->tcm_ifindex = ll_info_->name_to_index(ifname_.chars());

	return RTNetlink::rtnl_tell(tc_handle_, n);
}


int 
SchedulerCBQ::tc_add_class(CBQ_Class* cbq_class, struct rtattr *opt)
{
	int err;
	RTNetlink::rtnl_dialog d;
	char buf[4096];
	struct nlmsghdr n;
	struct nlmsghdr* h;
	struct tcmsg t;
	struct iovec iov[3];
	int ct = 0; // Note that this is used in rtnl_iov_set (Ugly!)

	rtnl_iov_set(&n, sizeof(n));
	memset(&n, 0, sizeof(n));
	n.nlmsg_type = RTM_NEWTCLASS;
	n.nlmsg_flags = NLM_F_ECHO|NLM_F_EXCL|NLM_F_CREATE;
	n.nlmsg_len = NLMSG_LENGTH(sizeof(t));

	rtnl_iov_set(&t, sizeof(t));
	memset(&t, 0, sizeof(t));
	t.tcm_parent = cbq_class->parent_;
	t.tcm_handle = cbq_class->classid_;
	t.tcm_ifindex = ll_info_->name_to_index(ifname_.chars());

	if (opt)
		rtnl_iov_set(opt, opt->rta_len);

	err = RTNetlink::rtnl_ask(tc_handle_, iov, ct, &d, buf, sizeof(buf));
	if (err)
		return err;

	cbq_class->classid_ = TC_H_UNSPEC;
	while ((h = RTNetlink::rtnl_wait(tc_handle_, &d, &err)) != NULL)
	{
		struct tcmsg *tcm = (struct tcmsg *)NLMSG_DATA(h);

		if (h->nlmsg_type != RTM_NEWTCLASS)
			continue;

		cbq_class->classid_ = tcm->tcm_handle;
	}
	if (err) {
		ERROR(2)( Log::Error, "add_class", strerror(err));
	}

	return err;
}

String SchedulerCBQ::print_rate(uint32 rate) {
	std::ostringstream os;
  double tmp = (double)rate*8;
	if (tmp >= 1024*1023 && fabs(1024*1024*rint(tmp/(1024*1024)) - tmp) < 1024)
		os << rint(tmp/(1024*1024)) << "Mbit";
	else if (tmp >= 1024-16 && fabs(1024*rint(tmp/1024) - tmp) < 16)   
		os << rint(tmp/1024) << "Kbit";
	else
		os << rate << "bps";
#if defined(HAVE_SSTREAM)
	return String(os.str().c_str());
#else
	return String(os.str());
#endif
}

String SchedulerCBQ::print_tc_classid(__u32 h) {
	std::ostringstream os;
	if (h == TC_H_ROOT) os << "root";
	else if (h == TC_H_UNSPEC) os << "none";
	else if (TC_H_MAJ(h) == 0) os << (void*)TC_H_MIN(h);
	else if (TC_H_MIN(h) == 0) os << (void*)(TC_H_MAJ(h)>>16);
	else os << (void*)(TC_H_MAJ(h)>>16) << ":" << (void*)TC_H_MIN(h);
#if defined(HAVE_SSTREAM)
	return String(os.str().c_str());
#else
	return String(os.str());
#endif
}

#endif

#endif
