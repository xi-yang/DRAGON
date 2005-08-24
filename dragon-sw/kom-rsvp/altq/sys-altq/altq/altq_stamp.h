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
#ifndef _ALTQ_ALTQ_STAMP_H_
#define _ALTQ_ALTQ_STAMP_H_

#include <altq/altq.h>

struct stamp_interface {
	char ifname[IFNAMSIZ];
};

struct stamp_config {
	struct stamp_interface iface;
	u_int stamp_limit;
};

struct stamp_add_filter {
	struct stamp_interface iface;
	struct flow_filter filter;
	u_long stamp_offset;

	u_long filter_handle;								/* return value */
};

struct stamp_del_filter {
	struct stamp_interface iface;
	u_long filter_handle;
};

/* 
 * IOCTLs for STAMP
 */
#define STAMP_ENABLE					_IOW('Q', 1, struct stamp_interface)
#define STAMP_DISABLE					_IOW('Q', 2, struct stamp_interface)
#define	STAMP_IF_ATTACH				_IOW('Q', 3, struct stamp_interface)
#define	STAMP_IF_DETACH				_IOW('Q', 4, struct stamp_interface)
#define STAMP_CONFIG					_IOW('Q', 5, struct stamp_config)
#define STAMP_ADD_FILTER_IN		_IOWR('Q', 7, struct stamp_add_filter)
#define STAMP_DEL_FILTER_IN		_IOW('Q', 8, struct stamp_del_filter)
#define STAMP_ADD_FILTER_OUT	_IOWR('Q', 12, struct stamp_add_filter)
#define STAMP_DEL_FILTER_OUT	_IOW('Q', 13, struct stamp_del_filter)

#ifdef _KERNEL

#define	STAMP_LIMIT	50	/* default max queue length */

extern int altq_stamp_input __P((struct mbuf *, int));

struct stamp_state_t {
	struct stamp_state_t *q_next;	/* next stamp_state in the list */
	struct ifaltq *q_ifq;					/* backpointer to ifaltq */

	struct mbuf *q_head;		/* head of queue */
	struct mbuf *q_tail;		/* tail of queue */
	int	q_len;			/* queue length */
	int	q_limit;		/* max queue length */

	struct acc_classifier classifier_in;
	struct acc_classifier classifier_out;
};

#endif /* _KERNEL */

#endif /* _ALTQ_ALTQ_STAMP_H_ */
