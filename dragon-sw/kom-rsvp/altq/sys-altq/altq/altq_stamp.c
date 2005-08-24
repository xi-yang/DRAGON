/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

	Note: This code is based on and borrows heavily from the original ALTQ
	code written by Kenjiro Cho. Please see file altq.h for details.
	Martin Karsten is responsible for all bugs, though.

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
#if defined(__FreeBSD__) || defined(__NetBSD__)
#include "opt_altq.h"
#endif /* __FreeBSD__ || __NetBSD__ */
#ifdef ALTQ_STAMP	/* stamp is enabled by STAMP option in opt_altq.h */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <altq/altq.h>
#include <altq/altq_conf.h>
#include <altq/altq_stamp.h>

/* stamp_list keeps all stamp_state_t's allocated. */
static struct stamp_state_t *stamp_list = NULL;

/* internal function prototypes */
static int		stamp_enqueue __P((struct ifaltq *, struct mbuf *,
					   struct altq_pktattr *));
static struct mbuf 	*stamp_dequeue __P((struct ifaltq *, int));
static int		stamp_request __P((struct ifaltq *, int, void *));
static void 	stamp_purge __P((struct stamp_state_t *));

static int 		stamp_attach __P((struct stamp_interface *));
static int 		stamp_detach __P((struct stamp_state_t *));
static int 		stamp_enable __P((struct stamp_state_t *));
static int 		stamp_disable __P((struct stamp_state_t *));
static int		stamp_add_filter_in __P((struct stamp_state_t *, struct stamp_add_filter *));
static int		stamp_del_filter_in __P((struct stamp_state_t *, struct stamp_del_filter *));
static int		stamp_add_filter_out __P((struct stamp_state_t *, struct stamp_add_filter *));
static int		stamp_del_filter_out __P((struct stamp_state_t *, struct stamp_del_filter *));
static void		stamp_packet __P((struct mbuf*, struct ip*, u_long));
      

/* #define DEBUG */
/* #define STAMP_ON_DEQUEUE */

/*
 * stamp device interface
 */
altqdev_decl(stamp);

int
stampopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	if (machclk_freq == 0)
		init_machclk();

	if (machclk_freq == 0) {
		printf("cdnr: no cpu clock available!\n");
		return (ENXIO);
	}
	/* everything else will be done when the queueing scheme is attached. */
	return 0;
}

/*
 * there are 2 ways to act on close.
 *   detach-all-on-close:
 *	use for the daemon style approach.  if the daemon dies, all the
 *	resource will be released.
 *   no-action-on-close:
 *	use for the command style approach.  (e.g.  stamp on/off)
 *
 * note: close is called not on every close but when the last reference
 *       is removed (only once with multiple simultaneous references.)
 */
int
stampclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct stamp_state_t *q;
	int err, error = 0;

	while ((q = stamp_list) != NULL) {
		/* destroy all */
		err = stamp_detach(q);
		if (err != 0 && error == 0)
			error = err;
	}

	return error;
}

int
stampioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	ioctlcmd_t cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct stamp_state_t *q;
	int	error = 0;

	/* check super-user privilege */
	switch (cmd) {
	default:
#if (__FreeBSD_version > 400000)
		if ((error = suser(p)) != 0)
			return (error);
#else
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
#endif
		break;
	}
    
	switch (cmd) {

	case STAMP_IF_ATTACH:
		error = stamp_attach((struct stamp_interface *)addr);
		break;

	case STAMP_IF_DETACH:
	case STAMP_ENABLE:
	case STAMP_DISABLE:
	case STAMP_CONFIG:
	case STAMP_ADD_FILTER_IN:
	case STAMP_DEL_FILTER_IN:
	case STAMP_ADD_FILTER_OUT:
	case STAMP_DEL_FILTER_OUT:
		if ((q = altq_lookup(((struct stamp_interface *)addr)->ifname, ALTQT_STAMP)) == NULL) {
			error = EBADF;
			break;
		}

		switch (cmd) {
		case STAMP_IF_DETACH:
			error = stamp_detach(q);
			break;
		case STAMP_CONFIG:
			q->q_limit = ((struct stamp_config*)addr)->stamp_limit;
			break;
		case STAMP_ENABLE:
			error = stamp_enable(q);
			break;
		case STAMP_DISABLE:
			error = stamp_disable(q);
			break;
		case STAMP_ADD_FILTER_IN:
			error = stamp_add_filter_in(q,(struct stamp_add_filter *)addr);
			break;
		case STAMP_DEL_FILTER_IN:
			error = stamp_del_filter_in(q,(struct stamp_del_filter *)addr);
			break;
		case STAMP_ADD_FILTER_OUT:
			error = stamp_add_filter_out(q,(struct stamp_add_filter *)addr);
			break;
		case STAMP_DEL_FILTER_OUT:
			error = stamp_del_filter_out(q,(struct stamp_del_filter *)addr);
			break;
		}
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * stamp support routines
 */

/*
 * enqueue routine:
 *
 *	returns: 0 when successfully queued.
 *		 ENOBUFS when drop occurs.
 */
static int
stamp_enqueue(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	struct stamp_state_t *q = (struct stamp_state_t *)ifq->altq_disc;

	/* if the queue is full, drop the incoming packet(drop-tail) */
	if (q->q_len >= q->q_limit) {
		m_freem(m);
		return (ENOBUFS);
	}

#ifndef STAMP_ON_DEQUEUE
	if ( pktattr && pktattr->pattr_class && pktattr->pattr_hdr ) {
		struct mbuf* m0;
		struct ip* ip = (struct ip *)(pktattr->pattr_hdr);
		for ( m0 = m; m0 != NULL; m0 = m0->m_next ) {
			if ( (caddr_t)ip >= m0->m_data && (caddr_t)ip < m0->m_data + m0->m_len ) {
				stamp_packet(m0, ip, (u_long)(pktattr->pattr_class) );
		break;
			}
		}
#ifdef DEBUG
		if (!m0) printf("stamp_enqueue: WARNING - cannot find proper mbuf\n" );
#endif
	}
#endif

	/* enqueue the packet at the taile of the queue */
	m->m_nextpkt = NULL;
	if (q->q_tail == NULL)
		q->q_head = m;
	else
		q->q_tail->m_nextpkt = m;
	q->q_tail = m;
	q->q_len++;
	ifq->ifq_len++;
	return 0;
}

/*
 * dequeue routine:
 *	must be called in splimp.
 *
 *	returns: mbuf dequeued.
 *		 NULL when no packet is available in the queue.
 */
/*
 * ALTDQ_PEEK is provided for drivers which need to know the next packet
 * to send in advance.
 * when ALTDQ_PEEK is specified, the next packet to be dequeued is
 * returned without dequeueing the packet.
 * when ALTDQ_DEQUEUE is called *immediately after* an ALTDQ_PEEK
 * operation, the same packet should be returned.
 */
static struct mbuf *
stamp_dequeue(ifq, op)
	struct ifaltq *ifq;
	int op;
{
	struct stamp_state_t *q = (struct stamp_state_t *)ifq->altq_disc;
	struct mbuf *m = NULL;

	if (op == ALTDQ_POLL)
		return (q->q_head);
		
	if ((m = q->q_head) == NULL)
		return (NULL);

	if ((q->q_head = m->m_nextpkt) == NULL)
		q->q_tail = NULL;
	m->m_nextpkt = NULL;
	q->q_len--;
	ifq->ifq_len--;

#ifdef STAMP_ON_DEQUEUE
	{
		struct ether_header *eh;
		u_long offset;
		eh = mtod(m, struct ether_header *);
		if ( ntohs(eh->ether_type) == ETHERTYPE_IP ) {
			struct mbuf* m0;
			struct ip* ip = (struct ip *)(eh+1);
			if ( (caddr_t)ip >= m->m_data + m->m_len ) {
				ip = (struct ip *)m->m_next->m_data;
			}
			for ( m0 = m; m0 != NULL; m0 = m0->m_next ) {
				if ( (caddr_t)ip >= m0->m_data && (caddr_t)ip < m0->m_data + m0->m_len ) {
					if ( m0 == m ) {
						m->m_data += sizeof(struct ether_header);
						m->m_len -= sizeof(struct ether_header);
					}
					offset = (u_long)acc_classify(&q->classifier_out, m, AF_INET);
					if ( m0 == m ) {
						m->m_data -= sizeof(struct ether_header);
						m->m_len += sizeof(struct ether_header);
					}
					if ( offset != 0 ) stamp_packet(m0, ip, offset);
			break;
				}
			}
#ifdef DEBUG
			if (!m0) printf("stamp_dequeue: WARNING - cannot find proper mbuf\n" );
#endif
		}
	}
#endif

	return (m);
}

static int
stamp_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	struct stamp_state_t *q = (struct stamp_state_t *)ifq->altq_disc;

	switch (req) {
	case ALTRQ_PURGE:
		stamp_purge(q);
		break;
	}
	return (0);
}

/*
 * stamp_purge
 * should be called in splimp or after disabling the stamp.
 */
static void
stamp_purge(q)
	struct stamp_state_t *q;
{
	struct mbuf *m;

	while ((m = q->q_head) != NULL) {
		q->q_head = m->m_nextpkt;
		m_freem(m);
	}
	q->q_tail = NULL;
	q->q_len = 0;
	if (ALTQ_IS_ENABLED(q->q_ifq))
		q->q_ifq->ifq_len = 0;
}

static int
stamp_attach(eif)
	struct stamp_interface *eif;
{
	struct ifnet *ifp;
	struct stamp_state_t *q;
	int error = 0;

	if ((ifp = ifunit(eif->ifname)) == NULL) {
		return (ENXIO);
	}

	/* allocate and initialize stamp_state_t */
	MALLOC(q, struct stamp_state_t *, sizeof(struct stamp_state_t),
	       M_DEVBUF, M_WAITOK);
	if (q == NULL) {
		return (ENOMEM);
	}
	bzero(q, sizeof(struct stamp_state_t));

	q->q_ifq = &ifp->if_snd;
	q->q_head = q->q_tail = NULL;
	q->q_len = 0;
	q->q_limit = STAMP_LIMIT;

	/*
	 * set STAMP to this ifnet structure.
	 */
	error = altq_attach(q->q_ifq, ALTQT_STAMP, q,
			    stamp_enqueue, stamp_dequeue, stamp_request,
#ifdef STAMP_ON_DEQUEUE
			    NULL, NULL);
#else
					&q->classifier_out, acc_classify);
#endif

	if (error) {
		FREE(q, M_DEVBUF);
		return error;
	}

	/* add this state to the stamp list */
	q->q_next = stamp_list;
	stamp_list = q;
	return (error);
}

static int
stamp_detach(q)
	struct stamp_state_t *q;
{
	struct stamp_state_t *tmp;
	int error = 0;

	if (ALTQ_IS_ENABLED(q->q_ifq))
		stamp_disable(q);

	stamp_purge(q);
	acc_discard_filters( &q->classifier_out, NULL, 1 );
	acc_discard_filters( &q->classifier_in, NULL, 1 );

	if ((error = altq_detach(q->q_ifq)))
		return (error);

	if (stamp_list == q)
		stamp_list = q->q_next;
	else {
		for (tmp = stamp_list; tmp != NULL; tmp = tmp->q_next)
			if (tmp->q_next == q) {
				tmp->q_next = q->q_next;
				break;
			}
		if (tmp == NULL)
			printf("stamp_detach: no state in stamp_list!\n");
	}

	FREE(q, M_DEVBUF);
	return (error);
}

static int
stamp_enable (q)
	struct stamp_state_t *q;
{
	if (altq_input == NULL)
		altq_input = altq_stamp_input;
	return altq_enable(q->q_ifq);
}

static int
stamp_disable(q)
	struct stamp_state_t *q;
{
	struct stamp_state_t *tmp = stamp_list;

	int error = altq_disable(q->q_ifq);
	if (error)
		return error;

	while (tmp) {
		if (ALTQ_IS_ENABLED(tmp->q_ifq))
	break;
		tmp = tmp->q_next;
	}
	if (tmp == NULL)
		altq_input = NULL;

	return 0;
}

static int
stamp_add_filter_in(q,af)
	struct stamp_state_t* q;
	struct stamp_add_filter *af;
{
	if ( q == NULL ) {
		return (EBADF);
	}
	return acc_add_filter(&q->classifier_in, &af->filter, (void*)af->stamp_offset, &af->filter_handle );
}

static int
stamp_del_filter_in(q,df)
	struct stamp_state_t* q;
	struct stamp_del_filter *df;
{
	if (q == NULL) {
		return (EBADF);
	}
	return acc_delete_filter(&q->classifier_in, df->filter_handle);
}

static int
stamp_add_filter_out(q,af)
	struct stamp_state_t* q;
	struct stamp_add_filter* af;
{
	if ( q == NULL ) {
		return (EBADF);
	}
	return acc_add_filter( &q->classifier_out, &af->filter, (void*)af->stamp_offset, &af->filter_handle );
}

static int
stamp_del_filter_out(q,df)
	struct stamp_state_t* q;
	struct stamp_del_filter* df;
{
	if ( q == NULL ) {
		return (EBADF);
	}
	return acc_delete_filter( &q->classifier_out, df->filter_handle );
}

int
altq_stamp_input(m, af)
	struct mbuf	*m;
	int	af;	/* address family */
{
	struct ifnet *ifp;
	struct stamp_state_t* q;
	u_long offset;

	ifp = m->m_pkthdr.rcvif;

	if (!ALTQ_IS_ENABLED(&ifp->if_snd))
		return 1;

	q = ifp->if_snd.altq_disc;

	offset = (u_long)acc_classify(&q->classifier_in, m, af);
	if ( offset != 0 ) {
/*  experimental code: bounce back packet
		struct ip* ip = mtod(m, struct ip *);
		u_long tmp = ip->ip_src.s_addr;
		ip->ip_src.s_addr = ip->ip_dst.s_addr;
		ip->ip_dst.s_addr = tmp;
*/
		stamp_packet(m, mtod(m, struct ip *), offset );
	}
	return 1;
}

static void stamp_packet(m, ip, offset)
	struct mbuf *m;
	struct ip* ip;
	u_long offset;
{
	struct timeval tv;
	caddr_t tph = ((caddr_t)ip) + (ip->ip_hl)*4;
	caddr_t data = tph + offset;
	if ( data >= (m->m_data + m->m_len) ) {
		data = m->m_next->m_data + (data - (m->m_data + m->m_len));
	}
#ifdef DEBUG
	printf("stamp_packet: mbuf %p-%p, offset %d, data %p\n", m->m_data, m->m_data + m->m_len, offset, data );
	if ( m->m_next )
		printf("stamp_packet: next mbuf %p-%p\n", m->m_next->m_data, m->m_next->m_data + m->m_next->m_len );
#endif
	microtime(&tv);
	*(u_long*)(data) = htonl(tv.tv_sec);
	*(u_long*)(data + 4) = htonl(tv.tv_usec);
	/* set transport checksum to zero to avoid packet being thrown away */
	if ( ip->ip_p == IPPROTO_UDP ) {
		((struct udphdr*)tph)->uh_sum = 0;
	} else if ( ip->ip_p == IPPROTO_TCP ) {
		((struct tcphdr*)tph)->th_sum = 0;
	}
}	

#ifdef KLD_MODULE

static struct altqsw stamp_sw =
	{"stamp", stampopen, stampclose, stampioctl};

ALTQ_MODULE(altq_stamp, ALTQT_STAMP, &stamp_sw);

#endif /* KLD_MODULE */

#endif /* ALTQ_STAMP */
