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
#ifndef _RSVP_ObjectHeader_h_
#define _RSVP_ObjectHeader_h_ 1

#include "RSVP_BasicTypes.h"

class INetworkBuffer;
class ONetworkBuffer;

class RSVP_ObjectHeader {
public:
	enum ClassNum {
		null = 0,
		SESSION = 1,
		RSVP_HOP = 3,
		INTEGRITY = 4,
		TIME_VALUES = 5,
		ERROR_SPEC = 6,
		SCOPE = 7,
		STYLE = 8,
		FLOWSPEC = 9,
		FILTER_SPEC = 10,
		SENDER_TEMPLATE = 11,
		SENDER_TSPEC = 12,
		ADSPEC = 13,
		POLICY_DATA = 14,
		RESV_CONFIRM = 15,
		LABEL = 16,
		LABEL_REQUEST = 19,
		EXPLICIT_ROUTE = 20,
		HELLO = 22,
#if defined(REFRESH_REDUCTION)
		MESSAGE_ID = 23,
		MESSAGE_ID_ACK = 24,
		MESSAGE_ID_LIST = 25,
		UPSTREAM_LABEL = 35,
		LABEL_SET = 36,
		SUGGESTED_LABEL = 129,
		SESSION_ATTRIBUTE = 207,
#endif
#if defined(ONEPASS_RESERVATION)
		DUPLEX = 208,								// uses one of the reserved value
#endif
		GENERALIZED_UNI = 229, //OIF GENERALIZED UNI Object
		DRAGON_UNI = 253, //DRAGON UNI Object with a private class number
		DRAGON_EXT_INFO = 254, //DRAGON Extension Information Object with a private class number
		};
protected:
	uint16 length;
	uint8 classNum;
	uint8 C_Type;
	friend ostream& operator<< ( ostream&, const RSVP_ObjectHeader& ); \
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const RSVP_ObjectHeader& ); \
	friend INetworkBuffer& operator>> ( INetworkBuffer&, RSVP_ObjectHeader& );
public:
	RSVP_ObjectHeader( uint16 length = 0, uint8 classNum = 0, uint8 C_Type = 0 )
		: length(4+length), classNum(classNum), C_Type(C_Type) {}
	static uint16 size() { return 4; }
	uint16 getLength() const { return length; }
	uint8 getClassNum() const { return classNum; }
	uint8 getC_Type() const { return C_Type; }
};

extern inline ostream& operator<< ( ostream& os, const RSVP_ObjectHeader& o ) {
	os << (uint32)o.length << " " << (uint32)o.classNum << " " << (uint32)o.C_Type << " ";
	return os;
}

extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, RSVP_ObjectHeader& o ) {
	buf >> o.length >> o.classNum >> o.C_Type;
	return buf;
}

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const RSVP_ObjectHeader& o ) {
	buf << o.length << o.classNum << o.C_Type;
	return buf;
}

#endif /* _RSVP_ObjectHeader_h_ */
