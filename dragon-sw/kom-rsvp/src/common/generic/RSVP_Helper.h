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
#ifndef _RSVP_Helper_h_
#define _RSVP_Helper_h_ 1

#if defined(RSVP_MEMORY_MACHINE)
#define DECLARE_MEMORY_MACHINE_IN_CLASS(classname) \
	inline void* operator new( size_t s ); \
	inline void operator delete(void *pnt); \
	static const char* const name;
#else
#define DECLARE_MEMORY_MACHINE_IN_CLASS(classname)
#endif

#if defined(RSVP_MEMORY_MACHINE)    
#define DECLARE_MEMORY_MACHINE_OUT_CLASS(classname,machinename) \
extern GeneralMemoryMachine<classname> machinename; \
inline void* classname::operator new( size_t s ) { \
	return machinename.alloc( s ); \
} \
inline void classname::operator delete(void *pnt) { \
	machinename.dealloc( pnt ); \
}
#else
#define DECLARE_MEMORY_MACHINE_OUT_CLASS(classname,machinename)
#endif

#if defined(RSVP_MEMORY_MACHINE)
#define DEDICATED_LIST_MEMORY_MACHINE(classname,listtype,machinename) \
struct classname ## ListMemNode { \
	void* prev; \
	void* next; \
	classname data; \
	static const char* const name; \
}; \
extern GeneralMemoryMachine<classname ## ListMemNode> machinename; \
inline void* listtype ::ListNode::operator new( size_t s ) { \
	return machinename .alloc(s); \
} \
inline void listtype ::ListNode::operator delete( void* pnt ) { \
	machinename.dealloc(pnt); \
}
#else
#define DEDICATED_LIST_MEMORY_MACHINE(classname,listtype,machinename)
#endif

#if defined(RSVP_MEMORY_MACHINE)    
#define DEFINE_MEMORY_MACHINE(classname,machinename) \
	GeneralMemoryMachine<classname> machinename; \
	const char* const classname::name = #classname;
#else
#define DEFINE_MEMORY_MACHINE(classname,machinename)
#endif

#define RSVP_OBJECT_METHODS(XXX) \
	friend ostream& operator<< ( ostream&, const XXX ## _Object& ); \
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const XXX ## _Object& ); \
	friend INetworkBuffer& operator>> ( INetworkBuffer&, XXX ## _Object& );

#define COPY_ASSIGN1(XXX,M1) \
	XXX( const XXX& o ) : M1(o.M1) {} \
	const XXX& operator=( const XXX& o ) { M1 = o.M1; return *this; }

#define COPY_ASSIGN2(XXX,M1,M2) \
	XXX( const XXX& o ) : M1(o.M1), M2(o.M2) {} \
	const XXX& operator=( const XXX& o ) { M1 = o.M1; M2 = o.M2; return *this; }

#define COPY_ASSIGN3(XXX,M1,M2,M3) \
	XXX( const XXX& o ) : M1(o.M1), M2(o.M2), M3(o.M3) {} \
	const XXX& operator=( const XXX& o ) { M1 = o.M1; M2 = o.M2; M3 = o.M3; return *this; }

#define COPY_ASSIGN4(XXX,M1,M2,M3,M4) \
	XXX( const XXX& o ) : M1(o.M1), M2(o.M2), M3(o.M3), M4(o.M4) {} \
	const XXX& operator=( const XXX& o ) { M1 = o.M1; M2 = o.M2; M3 = o.M3; M4 = o.M4; return *this; }

#define COPY_ASSIGN5(XXX,M1,M2,M3,M4,M5) \
	XXX( const XXX& o ) : M1(o.M1), M2(o.M2), M3(o.M3), M4(o.M4), M5(o.M5) {} \
	const XXX& operator=( const XXX& o ) { M1 = o.M1; M2 = o.M2; M3 = o.M3; M4 = o.M4; M5 = o.M5; return *this; }

#define DECLARE_ORDER(XXX) \
	friend inline bool operator==( const XXX &, const XXX & ); \
	friend inline bool operator!=( const XXX &, const XXX & ); \
	friend inline bool operator>=( const XXX &, const XXX & ); \
	friend inline bool operator<=( const XXX &, const XXX & ); \
	friend inline bool operator>( const XXX &, const XXX & ); \
	friend inline bool operator<( const XXX &, const XXX & );

#define HELPER_ORDER1(OP,XXX,M) \
extern inline bool operator OP ( const XXX & o1, const XXX & o2 ) { \
	return ( o1. M OP o2. M ); \
}

#define IMPLEMENT_ORDER1(XXX,M1) \
HELPER_ORDER1(==,XXX,M1) \
HELPER_ORDER1(!=,XXX,M1) \
HELPER_ORDER1(>=,XXX,M1) \
HELPER_ORDER1(<=,XXX,M1) \
HELPER_ORDER1(>,XXX,M1) \
HELPER_ORDER1(<,XXX,M1)


#define HELPER_ORDER2(OP,XXX,M1,M2) \
extern inline bool operator OP ( const XXX & o1, const XXX & o2 ) { \
	if ( o1. M1 != o2. M1 ) \
		return o1. M1 OP o2. M1; \
	else return o1. M2 OP o2. M2; \
}

#define IMPLEMENT_ORDER2(XXX,M1,M2) \
extern inline bool operator == ( const XXX & o1, const XXX & o2 ) { \
	return ( o1. M1 == o2. M1 \
				&& o1. M2 == o2. M2 ); \
} \
extern inline bool operator != ( const XXX & o1, const XXX & o2 ) { \
 	return !operator==(o1,o2); \
} \
HELPER_ORDER2(>=,XXX,M1,M2) \
HELPER_ORDER2(<=,XXX,M1,M2) \
HELPER_ORDER2(>,XXX,M1,M2) \
HELPER_ORDER2(<,XXX,M1,M2)

#define HELPER_ORDER3(OP,XXX,M1,M2,M3) \
extern inline bool operator OP ( const XXX & o1, const XXX & o2 ) { \
	if ( o1. M1 != o2. M1 ) \
		return o1. M1 OP o2. M1; \
	else if ( o1. M2 != o2. M2 ) \
		return o1. M2 OP o2. M2; \
	else return o1. M3 OP o2. M3; \
}

#define IMPLEMENT_ORDER3(XXX,M1,M2,M3) \
extern inline bool operator == ( const XXX & o1, const XXX & o2 ) { \
	return ( o1. M1 == o2. M1 \
				&& o1. M2 == o2. M2 \
				&& o1. M3 == o2. M3 ); \
} \
extern inline bool operator != ( const XXX & o1, const XXX & o2 ) { \
	return !operator==(o1,o2); \
} \
HELPER_ORDER3(>=,XXX,M1,M2,M3) \
HELPER_ORDER3(<=,XXX,M1,M2,M3) \
HELPER_ORDER3(>,XXX,M1,M2,M3) \
HELPER_ORDER3(<,XXX,M1,M2,M3)

#define HELPER_ORDER4(OP,XXX,M1,M2,M3,M4) \
extern inline bool operator OP ( const XXX & o1, const XXX & o2 ) { \
	if ( o1. M1 != o2. M1 ) \
		return o1. M1 OP o2. M1; \
	else if ( o1. M2 != o2. M2 ) \
		return o1. M2 OP o2. M2; \
	else if ( o1. M3 != o2. M3 ) \
		return o1. M3 OP o2. M3; \
	else return o1. M4 OP o2. M4; \
}

#define IMPLEMENT_ORDER4(XXX,M1,M2,M3,M4) \
extern inline bool operator == ( const XXX & o1, const XXX & o2 ) { \
	return ( o1. M1 == o2. M1 \
				&& o1. M2 == o2. M2 \
				&& o1. M3 == o2. M3 \
				&& o1. M4 == o2. M4 ); \
} \
extern inline bool operator != ( const XXX & o1, const XXX & o2 ) { \
	return !operator==(o1,o2); \
} \
HELPER_ORDER4(>=,XXX,M1,M2,M3,M4) \
HELPER_ORDER4(<=,XXX,M1,M2,M3,M4) \
HELPER_ORDER4(>,XXX,M1,M2,M3,M4) \
HELPER_ORDER4(<,XXX,M1,M2,M3,M4)

#endif /* _RSVP_Helper_h_ */
