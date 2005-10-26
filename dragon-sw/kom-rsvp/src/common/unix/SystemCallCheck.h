/****************************************************************************

  KOM RSVP Engine (release version 3.0)
  Copyright (C) 1999-2002 Martin Karsten

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
#ifndef _SystemCallCheck_h_
#define _SystemCallCheck_h_ 1

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(CHECK)
#ifdef CHECK_ON
static	int	CHECK_result;
static  int CHECK_abort() { abort(); return 0; }
extern	int errno;
#define CHECK(function)	((CHECK_result = function) < 0 ? printf("CHECK ERROR %d occured in PID %ld File %s in Line %d\
 in Call :\n%s\nerrno is %d, %s\n",CHECK_result,(long)getpid(),__FILE__,__LINE__,#function,errno,strerror(errno)),CHECK_abort() : CHECK_result)
#define CHECK0(function)	((CHECK_result = function) != 0 ? printf("CHECK ERROR %d occured in PID %ld File %s in Line %d\
 in Call :\n%s\nerrno is %d, %s\n",CHECK_result,(long)getpid(),__FILE__,__LINE__,#function,errno,strerror(errno)),CHECK_abort() : CHECK_result)
#else
#define CHECK(function)	function
#define CHECK0(function)	function
#endif /* CHECK_ON */
#endif

#endif /* _SystemCallCheck_h_ */
