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
#ifndef _RSVP_Lists_h_
#define _RSVP_Lists_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_SortableHash.h"
#include "RSVP_SortedList.h"

typedef SortableList<NetAddress,NetAddress> AddressList;

class LogicalInterface;
template <> struct Less<LogicalInterface*> {
	inline bool operator()( const LogicalInterface*, const LogicalInterface* ) const;
};
typedef SimpleList<LogicalInterface*> LogicalInterfaceList;

class Hop; class HopKey;
template <> struct Less<HopKey*> {
	inline bool operator()( const HopKey*, const HopKey* ) const;
};
typedef SortableList<Hop*,HopKey*> HopList;

class RSB; class RSB_Key;
template <> struct Less<RSB_Key*> {
	inline bool operator()( const RSB_Key*, const RSB_Key* ) const;
};
typedef SortableList<RSB*,RSB_Key*> RSB_List;

class PSB; class SENDER_Object;
template <> struct Less<SENDER_Object*> {
	inline bool operator()( const SENDER_Object*, const SENDER_Object* ) const;
};
typedef SortableList<PSB*,SENDER_Object*> PSB_List;

class Session; class SESSION_Object;
template <> struct Less<SESSION_Object*> {
	inline bool operator()( const SESSION_Object*, const SESSION_Object* ) const;
};
template <> struct GetHash<SESSION_Object*> {
	inline uint32 operator()( const SESSION_Object*, uint32 hashCount ) const;
};
typedef SortableHash<Session*,SESSION_Object*> SessionHash;

class PHopSB; class PHopSBKey;
template <> struct Less<PHopSBKey*> {
	inline bool operator()( const PHopSBKey*, const PHopSBKey* ) const;
};
typedef SortableList<PHopSB*,PHopSBKey*> PHOP_List;

class POLICY_DATA_Object;
typedef SimpleList<const POLICY_DATA_Object*> PolicyObjectList;

class UNKNOWN_Object;
typedef SimpleList<const UNKNOWN_Object*> UnknownObjectList;

class OIatPSB;
typedef SortableList<OIatPSB*,OIatPSB*> OIatPSB_List;

//class SNMP_Session;
//typedef SimpleList<const SNMP_Session*> SNMPSessionList;

#endif /* _RSVP_Lists_h_ */
