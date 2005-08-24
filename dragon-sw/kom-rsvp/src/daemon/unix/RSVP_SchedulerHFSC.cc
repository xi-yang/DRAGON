/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

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
	usable bandwidth:		 100% of total bandwidth
	signalling bandwidth:  5% of total bandwidth
	reservable bandwidth: 90% of total bandwidth
	default class: 				 5% of total bandwidth
*/


#if defined(ENABLE_ALTQ)

#include "RSVP_SchedulerHFSC.h"

#include "RSVP_IntServObjects.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_ProtocolObjects.h"
#include "SystemCallCheck.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/linker.h>

static int kernelmod_fileid  = 0;
static int initCounter = 0;
int SchedulerHFSC::hfscfd = -1;

extern "C" {
static void gsc_add_sc(struct gen_sc *gsc, const struct service_curve *sc);
static void gsc_sub_sc(struct gen_sc *gsc, const struct service_curve *sc);
static int is_gsc_under_sc(struct gen_sc *gsc, struct service_curve *sc);
static void gsc_destroy(struct gen_sc *gsc);
static struct segment *gsc_getentry(struct gen_sc *gsc, double x);
static int gsc_add_seg(struct gen_sc *gsc, double x, double y, double d, double m);
static int gsc_sub_seg(struct gen_sc *gsc, double x, double y, double d, double m);
static void gsc_compress(struct gen_sc *gsc);
static double sc_x2y(struct service_curve *sc, double x);
}
                      
inline u_long SchedulerHFSC::createClass( u_long parent, uint32 m1, uint32 d, uint32 m2, int qlimit, int flags ) {
	struct hfsc_add_class new_class;
	initMemoryWithZero( &new_class, sizeof(new_class) );
	copyMemory( &new_class.iface, &hfscif, sizeof(hfscif) );

	// both, ls-sc and rt-sc, will be set the same (initially ???)
	new_class.service_curve.m1 = m1;
	new_class.service_curve.d = d;
	new_class.service_curve.m2 = ( (d || (m2 && !m1)) ? m2 : m1 ); //d=0 means a linear service curve
	new_class.qlimit = qlimit; // note that 0 defaults to 50 !!!
	new_class.flags = flags; // flags = 0 means droptail queue (other choices red, rio,...), respectively used for default class
	new_class.parent_handle = parent;
	LOG(6)( Log::HFSC, "HFSC: create class with m1 =", m1, " d =", d, " m2 = ", new_class.service_curve.m2 );
	if ( ioctl(hfscfd, HFSC_ADD_CLASS, &new_class) < 0 ) {
		return 0;
	} else {
	  return new_class.class_handle;
	 }
}

inline void SchedulerHFSC::mapFlowspec2sc( const FLOWSPEC_Object& flowspec, service_curve& s ) {
	switch( flowspec.getServiceNumber() ) {

	// we should calculate d using the error terms Csum and Dsum
	case ServiceHeader::Guaranteed:
		// m1 = R; first segment (bit/s)
		s.m1 = (u_int)(flowspec.get_R() * 8);
		// choose the angular point depending upon the relation between p and R
		if (flowspec.get_R() > flowspec.get_p()) { 
		  // d = (b - r*M/R)/(R-r) (ms)
		  s.d = (u_int)((flowspec.get_b() - flowspec.get_r() * flowspec.get_M() / flowspec.get_R()) / (flowspec.get_R() - flowspec.get_r())) * 1000;
			if ( s.d < 0 ) s.d = 0;
		} else if (flowspec.get_R() == flowspec.get_r() ) {
			s.d = 0;
		} else {
		  // d = (b*(p-r)+M(R-r))/(R*(p-r)) (ms)
		  s.d = (u_int)((flowspec.get_b() * (flowspec.get_p() - flowspec.get_r()) + flowspec.get_M() * (flowspec.get_R() - flowspec.get_r())) / (flowspec.get_R() * (flowspec.get_p() - flowspec.get_r())) * 1000);
		}
		// m2 = r; second segment (bit/s)
		s.m2 = (u_int)(flowspec.get_r() * 8);
		break;

	case ServiceHeader::ControlledLoad:
		// m1 = 0, d = 0, m2 = r (bit/s); linear service curve
		s.m1 = 0;
		s.d = 0;
		s.m2 = (u_int)(flowspec.get_r() * 8);
		break;

	default:
		assert(0); // ??? unknown service, should have never come so far
	}
}

ADSPEC_Object*  SchedulerHFSC::init( const LogicalInterface& lif, uint32& serviceSupport ) {
	LOG(2)( Log::HFSC, "HFSC: enabling on interface", lif.getName() );

	initMemoryWithZero(&hfscif, sizeof(hfscif));
	strncpy(hfscif.hfsc_ifname, lif.getName().chars(), IFNAMSIZ);

	initCounter += 1;
	if ( initCounter == 1 ) {
		hfscfd = open(HFSC_DEVICE, O_RDWR);
#if defined(HAVE_KLD)
		if ( hfscfd < 0 ) {
			struct stat sbuf;
			if ( stat( "/modules/altq_hfsc.ko", &sbuf ) >= 0 ) {
				kernelmod_fileid = kldload("/modules/altq_hfsc.ko");
			}
			hfscfd = open(HFSC_DEVICE, O_RDWR);
		}
#endif
		if ( hfscfd < 0 ) {
			LOG(1)( Log::HFSC, "HFSC: cannot open " HFSC_DEVICE " -> HFSC support disabled" );
	return NULL;
		}
	}

	struct hfsc_attach attach;
	initMemoryWithZero( &attach, sizeof(attach) );
	copyMemory( &attach.iface, &hfscif, sizeof(hfscif) );
	attach.bandwidth = (uint32)(1.0 * totalBw);
	CHECK( ioctl(hfscfd, HFSC_IF_ATTACH, &attach) );
	CHECK( ioctl(hfscfd, HFSC_DISABLE, &hfscif) );
	CHECK( ioctl(hfscfd, HFSC_CLEAR_HIERARCHY, &hfscif) );

	// the default class shall have no real-time requirements, therefore we
	// we set its rt-sc to 0, whereas its ls-sc is set to a linear service
	// curve with a slope of 5% of the total link bandwidth
	default_handle = createClass( HFSC_ROOTCLASS_HANDLE, 0.05 * totalBw, 0, 0.05 * totalBw, 0, HFCF_DEFAULTCLASS );
                                                 assert( default_handle != 0 );
/*
	struct hfsc_modify_class mod_class;
	initMemoryWithZero( &mod_class, sizeof(mod_class) );
	copyMemory( &mod_class.iface, &hfscif, sizeof(hfscif) );	  
	mod_class.class_handle = default_handle;
	mod_class.service_curve.m1 = 0.05 * totalBw;
	mod_class.service_curve.d = 0;
	mod_class.service_curve.m2 = 0.05 * totalBw;
	mod_class.sctype = HFSC_REALTIMESC;
	CHECK( ioctl(hfscfd, HFSC_MOD_CLASS, &mod_class));
*/

	// enable HFSC on this interface
	CHECK( ioctl(hfscfd, HFSC_ENABLE, &hfscif) );

	// control class
	ctl_handle = createClass( HFSC_ROOTCLASS_HANDLE, 0.05 * totalBw );
                                                     assert( ctl_handle != 0 );
	// add filter for ctl class
	struct hfsc_add_filter af;
	initMemoryWithZero(&af, sizeof(af));	
	copyMemory( &af.iface, &hfscif, sizeof(hfscif) );
	af.class_handle = ctl_handle;
	af.filter.ff_flow.fi_proto = IPPROTO_RSVP; // RSVP protocol number
	af.filter.ff_flow.fi_family = AF_INET;
	af.filter.ff_flow.fi_len = sizeof(struct flowinfo_in);
	CHECK( ioctl(hfscfd, HFSC_ADD_FILTER, &af) );

	// reserved traffic class (interior node)
	resv_handle = createClass( HFSC_ROOTCLASS_HANDLE, 0.90 * totalBw, 0, 0, 200 );
                                                    assert( resv_handle != 0 );
	realtimeSC.m1 = u_int(0.90 * totalBw);
	realtimeSC.d = 0;
	realtimeSC.m2 = u_int(0.90 * totalBw);

	int altq_fd = CHECK( open(ALTQ_DEVICE, O_RDWR) );
	struct tbrreq req;
	copyMemory( req.ifname, &hfscif, sizeof(hfscif) );
	req.tb_prof.rate = (u_int)totalBw;
	req.tb_prof.depth = 1514*10;
	CHECK( ioctl( altq_fd, ALTQTBRSET, &req ) );
	CHECK( close( altq_fd ) );

	serviceSupport = (1 << ServiceHeader::Default)
									|(1 << ServiceHeader::Guaranteed)
									|(1 << ServiceHeader::ControlledLoad);
	availBw = totalBw*0.90;
	ADSPEC_Object* localAdspec = new ADSPEC_Object( 1, availBw, latency, lif.getMTU() );
	LOG(2)( Log::HFSC, "SchedulerHFSC: localAdspec:", *localAdspec );
	return localAdspec;
}

SchedulerHFSC::~SchedulerHFSC() {

	gsc_destroy(&totalGenSC);
	if ( default_handle && hfscfd >= 0 ) {
		int altq_fd = CHECK( open(ALTQ_DEVICE, O_RDWR) );
		struct tbrreq req;
		copyMemory( req.ifname, &hfscif, sizeof(hfscif) );
		req.tb_prof.rate = 0;
		req.tb_prof.depth = 0;
		CHECK( ioctl( altq_fd, ALTQTBRSET, &req ) );
		CHECK( close( altq_fd ) );

		CHECK( ioctl( hfscfd, HFSC_DISABLE, &hfscif ) );
		CHECK( ioctl( hfscfd, HFSC_CLEAR_HIERARCHY, &hfscif ) );
		CHECK( ioctl( hfscfd, HFSC_IF_DETACH, &hfscif ) );
  	initCounter -= 1;
	  if ( initCounter == 0 ) {
  		if ( hfscfd >= 0 ) {
				CHECK( close(hfscfd) );
				hfscfd = -1;
			}
#if defined(HAVE_KLD)
			if (kernelmod_fileid) kldunload(kernelmod_fileid);
#endif
		}
	}
}

BaseScheduler::Reservation* SchedulerHFSC::internalMakeReservation( const RSB& ) {
	return new HFSC_Reservation;
}

bool SchedulerHFSC::modFlowspec( TrafficControl::RHandle* resv,
	const FLOWSPEC_Object& flowspec, uint8 pFlags,
	const FLOWSPEC_Object*& fwdFlowspec, const RSB&, TrafficControl::UpdateFlag,
	const FLOWSPEC_Object* replaceFlowspec ) {

	u_long& hfscHandle = reinterpret_cast<HFSC_Reservation*>(resv)->rhandle;

//	if ( !admitSimple( *reinterpret_cast<HFSC_Reservation*>(resv), flowspec.getEffectiveRate() * 8, (replaceFlowspec ? replaceFlowspec->getEffectiveRate() * 8 : 0) ) ) {
//		return false;
//	}

	service_curve new_sc, old_sc = reinterpret_cast<HFSC_Reservation*>(resv)->sc;
	mapFlowspec2sc(flowspec,new_sc);
	gsc_sub_sc( &totalGenSC, &old_sc );
	gsc_add_sc( &totalGenSC, &new_sc );
	if ( !is_gsc_under_sc( &totalGenSC, &realtimeSC ) ) {
		gsc_sub_sc( &totalGenSC, &new_sc );
		gsc_add_sc( &totalGenSC, &old_sc );
		LOG(3)( Log::HFSC, "SchedulerHFSC: rejecting", flowspec.getEffectiveRate()*8, "bit/s" );
		return false;
	}
	LOG(3)( Log::HFSC, "SchedulerHFSC: admitting", flowspec.getEffectiveRate()*8, "bit/s" );
	reinterpret_cast<HFSC_Reservation*>(resv)->sc = new_sc;

	// add new class corresponding to parameters and possibly del old class
	if ( hfscHandle == HFSC_NULLCLASS_HANDLE ) {
		// create class with allocation depending on service class and parameters
		hfscHandle = createClass(resv_handle, new_sc.m1, new_sc.d, new_sc.m2);
                                                            assert(hfscHandle);
	} else {
		// modify class
		struct hfsc_modify_class mod_class;
		initMemoryWithZero( &mod_class, sizeof(mod_class) );
		copyMemory( &mod_class.iface, &hfscif, sizeof(hfscif) );	  
		mod_class.class_handle = hfscHandle;
		// set service curve depending on service class and parameters
		reinterpret_cast<ServiceCurve&>(mod_class.service_curve) = new_sc;
		mod_class.sctype = HFSC_DEFAULTSC; // rsc and fsc are the same (initially ?)
		CHECK( ioctl(hfscfd, HFSC_MOD_CLASS, &mod_class) );
	}
	fwdFlowspec = &flowspec;
	return true;
}

void SchedulerHFSC::delFlowspec( const TrafficControl::RHandle* resv ) {
	LOG(1)( Log::HFSC, "HFSC: deleting reservation" );
	struct hfsc_delete_class dc;
	copyMemory( &dc.iface, &hfscif, sizeof(hfscif) );
	dc.class_handle = reinterpret_cast<const HFSC_Reservation*>(resv)->rhandle;
	CHECK( ioctl(hfscfd, HFSC_DEL_CLASS, &dc ) );
	gsc_sub_sc( &totalGenSC, &reinterpret_cast<const HFSC_Reservation*>(resv)->sc );
	delete reinterpret_cast<const HFSC_Reservation*>(resv);
}

BaseScheduler::Filter* SchedulerHFSC::addFilter( TrafficControl::RHandle* resv,
	const SESSION_Object& session, const SENDER_Object& sender ) {

	struct hfsc_add_filter fltr_add;
	initMemoryWithZero(&fltr_add, sizeof(fltr_add));
	copyMemory( &fltr_add.iface, &hfscif, sizeof(hfscif) );
	fltr_add.class_handle = reinterpret_cast<HFSC_Reservation*>(resv)->rhandle;
	fltr_add.filter.ff_flow.fi_dst.s_addr = session.getDestAddress().rawAddress();
	fltr_add.filter.ff_flow.fi_src.s_addr = sender.getSrcAddress().rawAddress();
	fltr_add.filter.ff_flow.fi_sport = htons(sender.getLspId());
	fltr_add.filter.ff_flow.fi_dport = htons(session.getTunnelId());
	fltr_add.filter.ff_flow.fi_proto = 17;
	fltr_add.filter.ff_flow.fi_family = AF_INET;
	fltr_add.filter.ff_flow.fi_len = sizeof(struct flowinfo_in);
	if ( fltr_add.filter.ff_flow.fi_dst.s_addr != 0 ) {
		fltr_add.filter.ff_mask.mask_dst.s_addr = 0xffffffff;
	}
	if ( fltr_add.filter.ff_flow.fi_src.s_addr != 0 ) {
		fltr_add.filter.ff_mask.mask_src.s_addr = 0xffffffff;
	}
	if ( ioctl(hfscfd, HFSC_ADD_FILTER, &fltr_add) == 0 ) {
		return new HFSC_Filter( sender, session, *reinterpret_cast<HFSC_Reservation*>(resv), fltr_add.filter_handle );
	} else {
		ERROR(2)( Log::Error, "ERROR: HFSC could not add filter for", sender );
		return NULL;
	}
}

void SchedulerHFSC::delFilter( const TrafficControl::FHandle* filter ) {
	LOG(2)( Log::HFSC, "HFSC: delete filter for", reinterpret_cast<const Filter*>(filter)->sender );
	struct hfsc_delete_filter fltr_del;
	initMemoryWithZero( &fltr_del, sizeof(fltr_del) );
	copyMemory( &fltr_del.iface, &hfscif, sizeof(hfscif) );
	fltr_del.filter_handle = reinterpret_cast<const HFSC_Filter*>(filter)->fhandle;
	CHECK( ioctl(hfscfd, HFSC_DEL_FILTER, &fltr_del) );
	delete reinterpret_cast<const HFSC_Filter*>(filter);
}

/*	$KAME: qop_hfsc.c,v 1.4 2000/10/18 09:15:19 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern "C" {

/*
 * admission control using generalized service curve
 */
#define	INFINITY	1e500		/* IEEE: positive infinity */
#define is_sc_null(sc)  (((sc) == NULL) || ((sc)->m1 == 0 && (sc)->m2 == 0))

/* add a new service curve to a generilized service curve */
static void
gsc_add_sc(struct gen_sc *gsc, const struct service_curve *sc)
{
	if (is_sc_null(sc))
		return;
	if (sc->d != 0)
		gsc_add_seg(gsc, 0, 0, (double)sc->d, (double)sc->m1);
	gsc_add_seg(gsc, (double)sc->d, 0, INFINITY, (double)sc->m2);
}

/* subtract a service curve from a generilized service curve */
static void
gsc_sub_sc(struct gen_sc *gsc, const struct service_curve *sc)
{
	if (is_sc_null(sc))
		return;
	if (sc->d != 0)
		gsc_sub_seg(gsc, 0, 0, (double)sc->d, (double)sc->m1);
	gsc_sub_seg(gsc, (double)sc->d, 0, INFINITY, (double)sc->m2);
}

/*
 * check whether all points of a generalized service curve have
 * their y-coordinates no larger than a given two-piece linear
 * service curve.
 */
static int
is_gsc_under_sc(struct gen_sc *gsc, struct service_curve *sc)
{
	struct segment *s, *last, *end;
	double y;

	if (is_sc_null(sc)) {
		if (LIST_EMPTY(gsc))
			return (1);
		LIST_FOREACH(s, gsc, _next) {
			if (s->m != 0)
				return (0);
		}
		return (1);
	}
	/*
	 * gsc has a dummy entry at the end with x = INFINITY.
	 * loop through up to this dummy entry.
	 */
	end = gsc_getentry(gsc, INFINITY);
	if (end == NULL)
		return (1);
	last = NULL;
	for (s = LIST_FIRST(gsc); s != end; s = LIST_NEXT(s, _next)) {
		if (s->y > sc_x2y(sc, s->x))
			return (0);
		last = s;
	}
	/* last now holds the real last segment */
	if (last == NULL)
		return (1);
	if (last->m > sc->m2)
		return (0);
	if (last->x < sc->d && last->m > sc->m1) {
		y = last->y + (sc->d - last->x) * last->m;
		if (y > sc_x2y(sc, sc->d))
			return (0);
	}
	return (1);
}

static void
gsc_destroy(struct gen_sc *gsc)
{
	struct segment *s;

	while ((s = LIST_FIRST(gsc)) != NULL) {
		LIST_REMOVE(s, _next);
		free(s);
	}
}

/*
 * return a segment entry starting at x.
 * if gsc has no entry starting at x, a new entry is created at x.
 */
static struct segment *
gsc_getentry(struct gen_sc *gsc, double x)
{
	struct segment *new_seg, *prev, *s;

	prev = NULL;
	LIST_FOREACH(s, gsc, _next) {
		if (s->x == x)
			return (s);	/* matching entry found */
		else if (s->x < x)
			prev = s;
		else
			break;
	}

	/* we have to create a new entry */
	if ((new_seg = (struct segment*)calloc(1, sizeof(struct segment))) == NULL)
		return (NULL);

	new_seg->x = x;
	if (x == INFINITY || s == NULL)
		new_seg->d = 0;
	else if (s->x == INFINITY)
		new_seg->d = INFINITY;
	else
		new_seg->d = s->x - x;
	if (prev == NULL) {
		/* insert the new entry at the head of the list */
		new_seg->y = 0;
		new_seg->m = 0;
		LIST_INSERT_HEAD(gsc, new_seg, _next);
	} else {
		/*
		 * the start point intersects with the segment pointed by
		 * prev.  divide prev into 2 segments
		 */
		if (x == INFINITY) {
			prev->d = INFINITY;
			if (prev->m == 0)
				new_seg->y = prev->y;
			else
				new_seg->y = INFINITY;
		} else {
			prev->d = x - prev->x;
			new_seg->y = prev->d * prev->m + prev->y;
		}
		new_seg->m = prev->m;
		LIST_INSERT_AFTER(prev, new_seg, _next);
	}
	return (new_seg);
}

/* add a segment to a generalized service curve */
static int
gsc_add_seg(struct gen_sc *gsc, double x, double y, double d, double m)
{
	struct segment *start, *end, *s;
	double x2;

	if (d == INFINITY)
		x2 = INFINITY;
	else
		x2 = x + d;
	start = gsc_getentry(gsc, x);
	end   = gsc_getentry(gsc, x2);
	if (start == NULL || end == NULL)
		return (-1);

	for (s = start; s != end; s = LIST_NEXT(s, _next)) {
		s->m += m;
		s->y += y + (s->x - x) * m;
	}

	end = gsc_getentry(gsc, INFINITY);
	for (; s != end; s = LIST_NEXT(s, _next)) {
		s->y += m * d;
	}

	return (0);
}

/* subtract a segment from a generalized service curve */
static int
gsc_sub_seg(struct gen_sc *gsc, double x, double y, double d, double m)
{
	if (gsc_add_seg(gsc, x, y, d, -m) < 0)
		return (-1);
	gsc_compress(gsc);
	return (0);
}

/*
 * collapse adjacent segments with the same slope
 */
static void
gsc_compress(struct gen_sc *gsc)
{
	struct segment *s, *next;

 again:
	LIST_FOREACH(s, gsc, _next) {

		if ((next = LIST_NEXT(s, _next)) == NULL) {
			if (LIST_FIRST(gsc) == s && s->m == 0) {
				/*
				 * if this is the only entry and its
				 * slope is 0, it's a remaining dummy
				 * entry. we can discard it.
				 */
				LIST_REMOVE(s, _next);
				free(s);
			}
			break;
		}

		if (s->x == next->x) {
			/* discard this entry */
			LIST_REMOVE(s, _next);
			free(s);
			goto again;
		} else if (s->m == next->m) {
			/* join the two entries */
			if (s->d != INFINITY && next->d != INFINITY)
				s->d += next->d;
			LIST_REMOVE(next, _next);
			free(next);
			goto again;
		}
	}
}

/* get y-projection of a service curve */
static double
sc_x2y(struct service_curve *sc, double x)
{
	double y;

	if (x <= (double)sc->d)
		/* y belongs to the 1st segment */
		y = x * (double)sc->m1;
	else
		/* y belongs to the 2nd segment */
		y = (double)sc->d * (double)sc->m1
			+ (x - (double)sc->d) * (double)sc->m2;
	return (y);
}

} // "C"

#endif /* ENABLE_ALTQ */
