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
#ifndef _common_h_
#define _common_h_

#include <stdlib.h>                            // abort
#include <string.h>                            // strerror
#include <errno.h>                             // errno
#if !defined(NS2)
#include <netdb.h>                             // gethostbyname, struct hostent
#endif
#include <arpa/inet.h>                         // inet_addr and inet_ntoa
#include <stdio.h>                             // printf, fprintf
#include <unistd.h>                            // getpid
#include <signal.h>                            // sigaction
#if defined(SunOS)
#include <sys/ethernet.h>                      // ether_header struct
#include <bsm/libbsm.h>                        // Solaris: Problems with netinet/ip.h
#else
#include <net/ethernet.h>                      // ether_header struct
#endif
#if defined(FreeBSD)
#include <netinet/in_systm.h>
#include <netinet/in.h>                        // field types for iphdr struct
#endif
#if defined(Darwin)
#include <unistd.h>                            // getpid
#endif
#include <netinet/ip.h>                        // iphdr struct
#include <netinet/udp.h>                       // udphdr struct
#include <netinet/tcp.h>                       // tcphdr struct

#define ETHER_HEADER_SIZE	(sizeof(ether_header))             // 14
#define ETHER_TRAILER_SIZE 4                                 //  4
#define IP_HEADER_SIZE	(sizeof(struct ip))                  // 20 + options
#define UDP_HEADER_SIZE	(sizeof(struct udphdr))              //  8
#define TCP_HEADER_SIZE (sizeof(struct tcphdr))              // 20 + options

#include "SystemCallCheck.h"

static inline unsigned int getInetAddr( const char* name ) {
	unsigned int addr = inet_addr( name );
#if !defined(NS2)
	if ( addr == ~0u ) {
		struct hostent* host = gethostbyname( name );
		if ( host && host->h_length >= 4 ) {
			addr = *(unsigned int*)(host->h_addr_list[0]);
		} else {
			addr = 0;
		}
	}
#endif
	return addr;
}

typedef void (*SigHandler)(int);

static inline void installExitHandler( SigHandler handler ) {
	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	CHECK( sigaction( SIGHUP, &sa, NULL ) );
	CHECK( sigaction( SIGINT, &sa, NULL ) );
	CHECK( sigaction( SIGQUIT, &sa, NULL ) );  
	CHECK( sigaction( SIGTERM, &sa, NULL ) );
}

static inline void installAlarmHandler( SigHandler handler ) {
	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	CHECK( sigaction( SIGALRM, &sa, NULL ) );
}

#endif
