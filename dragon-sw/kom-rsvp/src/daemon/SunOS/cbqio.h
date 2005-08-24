/*
 * Copyright (c) Sun Microsystems, Inc. 1998 All rights reserved.
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

#ifndef _NETINET_CBQIO_H
#define	_NETINET_CBQIO_H
/*
#pragma ident "@(#)cbqio.h  1.7     98/09/30 SMI"
*/

#include <sys/ioctl.h>
#include <sys/ioccom.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IFNAMSIZ
#define	IFNAMSIZ	16
#endif

#define	MAXQ		64	/* Max allowed queue length */

/*
 * Define filter structure
 */

#define	FILT_SIZE	48

typedef struct _filt cbq_filt_t;

struct _filt {
	u_int		daddr;	/* Destination Address. */
	u_int		saddr;	/* Source Address */
	u_short		dport;	/* Destination Port. */
	u_short		sport;	/* Source port. */
	u_char		proto;	/* IP protocol Number. */
};

/*
 * Define a well known class handles
 */
#define	NULL_CLASS_HANDLE	0xffffffff
#define	ROOT_CLASS_HANDLE	0xfffffffe
#define	DEFAULT_CLASS_HANDLE	0xfffffffd
#define	CTL_CLASS_HANDLE	0xfffffffc

/*
 * Define structures associated with IOCTLS for cbq.
 */

/*
 * Define the CBQ interface structure.  This must be included in all
 * IOCTL's such that the CBQ driver may find the appropriate CBQ module
 * associated with the network interface to be affected.
 */
typedef struct cbq_interface {
	char	cbq_ifacename[IFNAMSIZ];
	u_int	cbq_ifacelen;
} cbq_iface_t;

/*
 * Define IOCTLs for CBQ.
 */
#define	CBQ_ENABLE		_IOW('Q', 1, struct cbq_interface)
#define	CBQ_DISABLE		_IOW('Q', 2, struct cbq_interface)

#define	CBQ_ADD_FILTER		_IOWR('Q', 3, struct cbq_add_filter)

struct cbq_add_filter {
	cbq_iface_t	cbq_iface;
	u_long		cbq_class_handle;
	cbq_filt_t	cbq_filter;

	u_long		cbq_filter_handle;
};

#define	CBQ_DEL_FILTER		_IOW('Q', 4, struct cbq_delete_filter)

struct cbq_delete_filter {
	cbq_iface_t	cbq_iface;
	u_long		cbq_filter_handle;
};

#define	CBQ_ADD_CLASS		_IOWR('Q', 5, struct cbq_add_class)

typedef struct cbq_class_spec {
	u_int		priority;
	u_int		nano_sec_per_byte;
	u_int		maxq;
	u_int		maxidle;
	int		minidle;
	u_int		offtime;
	u_long		parent_class_handle;
	u_long		borrow_class_handle;

	u_int		defaultclass;
	u_int		ctlclass;
	u_int		red;
	u_int		wrr;
	int		efficient;
} cbq_class_spec_t;

struct cbq_add_class {
	cbq_iface_t		cbq_iface;

	cbq_class_spec_t	cbq_class;
	u_long			cbq_class_handle;
};

#define	CBQ_DEL_CLASS		_IOW('Q', 6, struct cbq_delete_class)

struct cbq_delete_class {
	cbq_iface_t	cbq_iface;
	u_long		cbq_class_handle;
};

#define	CBQ_CLEAR_HIERARCHY	_IOW('Q', 7, struct cbq_interface)
#define	CBQ_ENABLE_STATS	_IOW('Q', 8, struct cbq_interface)
#define	CBQ_DISABLE_STATS	_IOW('Q', 9, struct cbq_interface)

#ifndef _CLASS_STATS
#define	_CLASS_STATS

typedef struct _class_stats_ {
	u_int		handle;
	u_int		depth;

	int		npackets;	/* packets sent in this class */
	int		nbytes;		/* bytes sent in this class */
	int		over;		/* # times went over limit */
	int		borrows;	/* # times tried to borrow */
	int		drops;		/* # times dropped packets */
	int		overactions;	/* # times invoked overlimit action */
	int		delays;		/* # times invoked delay actions */
	int		emptys;		/* # times empty queue in sched */
} class_stats_t;
#endif

struct cbq_getstats {
	cbq_iface_t	iface;
	int		nclasses;
	class_stats_t	*stats;
};

#define	CBQ_GETSTATS		_IOW('Q', 10, struct cbq_getstats)

#ifdef CBQPERF
struct cbq_perf {
	u_int		class_id;
	hrtime_t	schedtime;	/* In ns. */
	hrtime_t	departtime;	/* In ns. */
};

struct cbq_getperf {
	cbq_iface_t	iface;
	int		id;
	struct cbq_perf	perf;
};

struct cbq_getnperf {
	cbq_iface_t	iface;
	int		nrecords;
};

#define CBQ_GETNPERF		_IOW('Q', 11, struct cbq_getnperf)
#define CBQ_GETPERF		_IOW('Q', 12, struct cbq_getperf)

#endif

#ifdef __cplusplus
}
#endif

#endif /* !_NETINET_CBQIO_H */
