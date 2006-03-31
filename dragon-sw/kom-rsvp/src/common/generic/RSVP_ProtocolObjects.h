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
#ifndef _RSVP_ProtocolObjects_h_
#define _RSVP_ProtocolObjects_h_ 1

#include "RSVP_Lists.h"
#include "RSVP_ObjectHeader.h"
#include "RSVP_SENDER_Object.h"
#include "RSVP_RefObject.h"
#include "RSVP_TimeValue.h"

//currently, only fixed-length label is accepted
class LABEL_Object {
protected:
	uint32 label;
	uint8 labelCType;
	friend ostream& operator<< ( ostream&, const LABEL_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const LABEL_Object& );
	uint16 size() const { return sizeof(uint32); }
	DECLARE_ORDER(LABEL_Object)
public:
	enum LabelCType{
		LABEL_MPLS = 1,
		LABEL_GENERALIZED = 2,
		LABEL_WAVEBAND = 3
	};
	LABEL_Object( uint32 label = 0 , uint8 C_Type = LABEL_GENERALIZED) : label(label), labelCType(C_Type) {}
	COPY_ASSIGN2(LABEL_Object,label,labelCType)
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	uint32 getLabel() const { return label; }
	void setLabel( uint32 l ) { label = l; }
	uint8 getLabelCType() const { return labelCType; }
	void setLabelCType(uint8 c) { labelCType = c; }
	void readFromBuffer(INetworkBuffer& buffer, uint16 len, uint8 C_Type);
};
IMPLEMENT_ORDER2(LABEL_Object,label, labelCType)

class SUGGESTED_LABEL_Object : public LABEL_Object {
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const SUGGESTED_LABEL_Object& ); 
public:
	SUGGESTED_LABEL_Object(uint32 label = 0, uint8 C_Type = LABEL_GENERALIZED) { 
		setLabel(label); 
		setLabelCType(C_Type);
	}
};

class UPSTREAM_LABEL_Object : public LABEL_Object {
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const UPSTREAM_LABEL_Object& ); 
public:
	UPSTREAM_LABEL_Object(uint32 label = 0, uint8 C_Type = LABEL_GENERALIZED) { 
		setLabel(label); 
		setLabelCType(C_Type);
	}
};

class LABEL_REQUEST_Object {
	uint8 lspEncodingType;
	uint8 switchingType;
	uint16 gPid;
	uint32 l3pid; //For MPLS
	uint8 labelType;
	friend ostream& operator<< ( ostream&, const LABEL_REQUEST_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const LABEL_REQUEST_Object& );
	DECLARE_ORDER(LABEL_REQUEST_Object)
	static uint16 size() { return 4; }
public:
	enum LSPEncType { L_Illegal = 0, 
					     L_Packet = 1, 
					     L_Eth = 2, 
					     L_ANSI_SDH = 3 , 
					     L_Reserved1 = 4,
					     L_ITU_SDH = 5,
					     L_Reserved2 = 6,
					     L_DigiWrapper = 7,
					     L_Lamda = 8,
					     L_Fiber = 9,
					     L_Reserved3 = 10,
					     L_FiberChannel = 11};
	enum SwitchingType { S_Illegal = 0, 
						  S_PSC_1 = 1,
						  S_PSC_2 = 2,
						  S_PSC_3 = 3,
						  S_PSC_4 = 4,
						  S_L2SC = 51,
						  S_TDM = 100,
						  S_LSC = 150,
						  S_FSC = 200};
	enum G_PID { G_Illegal = 0,
				   G_Reserved1 = 1,
				   G_Reserved2 = 2,
				   G_Reserved3 = 3,
				   G_Reserved4 = 4,
				   G_Asyn_E4 = 5,
				   G_Asyn_DS3 = 6,
				   G_Asyn_E3 = 7,
				   G_BitSyn_E3 = 8,
				   G_ByteSyn_E3 = 9,
				   G_Asyn_DS2 = 10,
				   G_BitSyn_DS2 = 11,
				   G_Reserved5 = 12,
				   G_Asyn_E1 = 13,
				   G_ByteSyn_E1 = 14,
				   G_ByteSyn_31DS0 = 15,
				   G_Asyn_DS1 = 16,
				   G_BitSyn_DS1 = 17,
				   G_ByteSyn_T1 = 18,
				   G_VC11 = 19,
				   G_Reserved6 = 20,
				   G_Reserved7 = 21,
				   G_DS1SFAsyn = 22,
				   G_DS1ESFAsyn = 23,
				   G_DS3M23Asyn = 24,
				   G_DS3CBitAsyn = 25,
				   G_VT_LOVC = 26,
				   G_STS_HOVC = 27,
				   G_POSUnscr16 = 28,
				   G_POSUnscr32 = 29,
				   G_POSScram16 = 30,
				   G_POSScram32 = 31,
				   G_ATM = 32,
				   G_Eth = 33,
				   G_SONET_SDH = 34,
				   G_Reserved = 35,
				   G_DigiWrapper = 36,
				   G_Lamda = 37
		};
	LABEL_REQUEST_Object( uint8 lspenc = L_Eth, uint8 swtype = S_LSC, uint16 gpid = G_Eth)
		: lspEncodingType(lspenc), switchingType(swtype), gPid(gpid), l3pid(0), labelType(LABEL_Object::LABEL_GENERALIZED){}
	LABEL_REQUEST_Object( uint32 l3pid)
		: lspEncodingType(0), switchingType(0), gPid(0), l3pid(l3pid), labelType(LABEL_Object::LABEL_MPLS){}
	COPY_ASSIGN5(LABEL_REQUEST_Object,lspEncodingType, switchingType, gPid, l3pid, labelType)
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	void readFromBuffer( INetworkBuffer& buffer, uint16 len, uint8 C_Type);
	uint8 getLspEncodingType() const { return lspEncodingType; }
	void setLspEncodingType( uint8 e ) { lspEncodingType = e; }
	uint8 getSwitchingType() const { return switchingType;}
	void setSwitchingType(uint8 s) {switchingType = s; }
	uint32 getL3Pid() const { return l3pid; }
	void setL3Pid(uint32 l) { l3pid = l; }
	uint8 getRequestedLabelType() const { return labelType; }
	void setRequestedLabelType(uint8 lt) { labelType = lt; }
	uint16 getGPid() const { return gPid; }
	void setGPid(uint16 g) { gPid = g;}
};
IMPLEMENT_ORDER4(LABEL_REQUEST_Object,lspEncodingType, switchingType, gPid, l3pid)


class LABEL_SET_Object : public RefObject<LABEL_SET_Object> {
	uint8 action;
	uint8 labelType;
	SortableList<uint32, uint32>  subChannelList;
	REF_OBJECT_METHODS(LABEL_SET_Object)
	friend ostream& operator<< ( ostream&, const LABEL_SET_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const LABEL_SET_Object& );
	void readFromBuffer( INetworkBuffer&, uint16 );
	uint16 size() const { return (4 + subChannelList.size() * 4); }
	
public:
	enum _action{
		InclusiveList = 0,
		ExclusiveList = 1,
		InclusiveRange = 2
	};
	LABEL_SET_Object( uint8 action = InclusiveList, uint8 labelType = 2)
		: action(action), labelType(labelType){}
	LABEL_SET_Object( INetworkBuffer& buf, uint16 len) { 
		readFromBuffer(buf, len);
	}
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	uint8 getAction() const { return action; }
	void setAction( uint8 a ) { action = a; }
	uint8 getLabelType() const { return labelType;}
	void setLabelType(uint8 l) {labelType = l; }
	void addSubChannel( const uint32 c ) {
		subChannelList.insert_unique(c);
	}
	void delSubChannel( const uint32 c ) {
		subChannelList.erase_key(c);
	}
	uint32 getASubChannel() { return subChannelList.front(); }
	bool operator==( const LABEL_SET_Object& o ) {
		return (action==o.action && labelType==o.labelType && subChannelList == o.subChannelList);
	}
	bool operator!=( const LABEL_SET_Object& o ) {
		return (*this==o);
	}
};
extern inline LABEL_SET_Object::~LABEL_SET_Object() {}

//Abstract Node Subobject of ERO as per RFC-3209, and
//Unnumbered Interface ID subobject of ERO as per RFC-3477
class AbstractNode {
	friend ostream& operator<< ( ostream&, const AbstractNode& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const AbstractNode& );
	friend INetworkBuffer& operator>> ( INetworkBuffer&, AbstractNode& );
protected:
	uint8 typeOrLoose;
	union {
		struct {
			uint32 addr;
			uint8 prefix;
		} ip4;
		struct {
			uint16 reserved;
			uint32 routeID;
			uint32 interfaceID;
		}uNumIfID;
		uint16 asNum;
	} data;
#define ip4_addr data.ip4.addr
#define ip4_prefix data.ip4.prefix
#define unum_rsv data.uNumIfID.reserved
#define unum_rtid data.uNumIfID.routeID
#define unum_ifid  data.uNumIfID.interfaceID
#define asnum data.asNum

public:
	enum Type { Illegal = 0, IPv4 = 1, IPv6 = 2, AS = 32 , UNumIfID = 4};
	AbstractNode() : typeOrLoose(Illegal) {}
	AbstractNode( bool loose, const NetAddress& addr, uint8 prefix )
		: typeOrLoose( IPv4 | (loose?0x80:0) ) {
		ip4_addr = addr.rawAddress();
		ip4_prefix = prefix;
	}
	AbstractNode( bool loose, uint16 asNum )
		: typeOrLoose( AS | (loose?0x80:0) ) {
		asnum = asNum;
	}
	AbstractNode( bool loose, const NetAddress& rtID, uint32 ifID )
		: typeOrLoose( UNumIfID | (loose?0x80:0) ) {
		unum_rsv = 0;
		unum_rtid = rtID.rawAddress();
		unum_ifid = ifID;
	}
	AbstractNode( INetworkBuffer& buf ) { buf >> *this; }
	uint8 getType() const { return typeOrLoose & 0x7f; }
	uint8 getLength() const { 
		switch( getType() ) {
			case IPv4: return 8;
			case AS: return 4;
			case UNumIfID: return 12;
			default : return 0;
		}
	}
	bool isLoose() const { return typeOrLoose & 0x80; }
	NetAddress getAddress() const { 
		switch( getType() ) {
			case IPv4: 
			case AS: 
				return ip4_addr;
			case UNumIfID: return unum_rtid;
			default : return 0;
		}
	}
	uint8 getPrefix() const { return ip4_prefix; }
	uint16 getNumber() const { return asnum; }
	uint32 getInterfaceID() const { return unum_ifid; }
	bool operator!=( const AbstractNode& a ) {
		if ( typeOrLoose != a.typeOrLoose ) return true;
		switch( getType() ) {
			case IPv4: return ip4_addr != a.ip4_addr;
			case AS: return asnum != a.data.asNum;
			case UNumIfID: return (unum_rtid != a.unum_rtid || unum_ifid != a.unum_ifid);
			default: return true;
		}
	}
};

typedef SimpleList<AbstractNode> AbstractNodeList;
class EXPLICIT_ROUTE_Object : public RefObject<EXPLICIT_ROUTE_Object> {
	uint16 length;
	AbstractNodeList abstractNodeList;
	
	REF_OBJECT_METHODS(EXPLICIT_ROUTE_Object)
	friend ostream& operator<< ( ostream&, const EXPLICIT_ROUTE_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const EXPLICIT_ROUTE_Object& );
	void readFromBuffer( INetworkBuffer&, uint16 );
	uint16 size() const { return length; }
public:
	EXPLICIT_ROUTE_Object() : length(0) {}
	EXPLICIT_ROUTE_Object( INetworkBuffer& b, uint16 len ) {
		readFromBuffer( b, len );
	}
	const AbstractNodeList& getAbstractNodeList() const {
		return abstractNodeList;
	}
	void pushBack( const AbstractNode& a ) {
		abstractNodeList.push_back( a );
		length += a.getLength();
	}
	void pushFront( const AbstractNode& a ) {
		abstractNodeList.push_front( a );
		length += a.getLength();
	}
	void popFront (){
                                           assert( !abstractNodeList.empty() );
		length -= abstractNodeList.front().getLength();
		abstractNodeList.pop_front();
	}
	void popBack (){
                                           assert( !abstractNodeList.empty() );
		length -= abstractNodeList.back().getLength();
		abstractNodeList.pop_back();
	}
	bool operator!=( const EXPLICIT_ROUTE_Object& o ) {
		return (abstractNodeList != o.abstractNodeList);
	}
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
};
extern inline EXPLICIT_ROUTE_Object::~EXPLICIT_ROUTE_Object() {}

class SESSION_Object {
	NetAddress tunnelAddress;
	uint16 tunnelID;
	uint32 extendedTunnelID; 
	RSVP_OBJECT_METHODS(SESSION)
	DECLARE_ORDER(SESSION_Object)
	static uint16 size() { return NetAddress::size() + 8 ; }
public:
	SESSION_Object() {}
	SESSION_Object( const NetAddress& tunnelAddress, const uint16 tunnelID, const uint32 extendedTunnelID) : 
		tunnelAddress(tunnelAddress), tunnelID(tunnelID), extendedTunnelID(extendedTunnelID){}
	SESSION_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	const NetAddress& getDestAddress() const { return tunnelAddress; }
	void setDestAddress( const NetAddress& a ) { tunnelAddress = a; }
	uint16 getTunnelId() const { return tunnelID; }
	void setTunnelId( uint16 p ) { tunnelID = p; }
	uint32 getExtendedTunnelId() const { return extendedTunnelID; }
	void setExtendedTunnelId (uint32 e) {extendedTunnelID = e; }
	uint32 getHashValue( uint32 hashSize ) const {
		uint32 hash = tunnelAddress.getHashValue( hashSize );
		return hash;
	}
};
IMPLEMENT_ORDER3(SESSION_Object,tunnelAddress,tunnelID, extendedTunnelID)

inline bool Less<SESSION_Object*>::operator()( const SESSION_Object* s1, const SESSION_Object* s2 ) const {
	return *s1 < *s2;
}

inline uint32 GetHash<SESSION_Object*>::operator()( const SESSION_Object* s, uint32 hashCount ) const {
	return s->getHashValue( hashCount );
}

class SESSION_ATTRIBUTE_Object {
	uint32 excludeAny;
	uint32 includeAny;
	uint32 includeAll;
	uint8 setupPri;
	uint8 holdingPri;
	uint8 flags;
	uint8 nameLength;
	String sessionName;
	friend ostream& operator<< ( ostream&, const SESSION_ATTRIBUTE_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const SESSION_ATTRIBUTE_Object& );
	uint16 size() const{ 
		if (excludeAny || includeAny || includeAll) return (nameLength + 16);
		else return (nameLength+4);
	}
public:
	SESSION_ATTRIBUTE_Object(): excludeAny(0), includeAny(0), includeAll(0), setupPri(7), holdingPri(7), flags(0) {};
	SESSION_ATTRIBUTE_Object( const String ssName, const uint32 eAny = 0, const uint32 iAny = 0, const uint32 iAll = 0, 
								           const uint8 sPr = 7, const uint8 hPr = 7, const uint8 flags = 0) : 
		excludeAny(eAny), includeAny(iAny), includeAll(iAll), setupPri(sPr), holdingPri(hPr), flags(flags){
			sessionName = ssName;
			if (ssName.length()%4==0) 
				nameLength = ssName.length();
			else 
				nameLength = (ssName.length()/4+1)*4;
		}
	void readFromBuffer( INetworkBuffer& buffer, uint16 len, uint8 C_Type);
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	const bool hasRA() const{ return (excludeAny || includeAny || includeAll);}
	const String& getSessionName() const { return sessionName; }
	bool operator==(const SESSION_ATTRIBUTE_Object& s){
		return (sessionName == s.sessionName);
	}
};

//currently does not support IPv6 and bundle link
class RSVP_HOP_TLV_SUB_Object{
	uint16 type;
	uint16 length;
	union {
		uint32 ip4;
		struct {
			uint32 addr;
			uint32 interfaceID;
		}ifIndex;
	} value;
	RSVP_OBJECT_METHODS(RSVP_HOP_TLV_SUB)
	DECLARE_ORDER(RSVP_HOP_TLV_SUB_Object)
public:
	enum Type { Illegal = 0, IPv4 = 1, IPv6 = 2, IfIndex = 3 , CompIfDownStream = 4, CompIfUpStream = 5};
	RSVP_HOP_TLV_SUB_Object():type(Illegal), length(0) {}
	RSVP_HOP_TLV_SUB_Object(const NetAddress& addr)
		: type(IPv4), length(8) {
			value.ip4 = ntohl(addr.rawAddress());
		}
	RSVP_HOP_TLV_SUB_Object(const NetAddress& addr, const uint32 ifID)
		: type(IfIndex), length(12) {
			value.ifIndex.addr = ntohl(addr.rawAddress());
			value.ifIndex.interfaceID = ifID;
		}
	RSVP_HOP_TLV_SUB_Object(RSVP_HOP_TLV_SUB_Object& tlv) { *this = tlv; }
	RSVP_HOP_TLV_SUB_Object( INetworkBuffer& buf ) { buf >> *this; }
	RSVP_HOP_TLV_SUB_Object& operator=(const RSVP_HOP_TLV_SUB_Object & t) { 
		type = t.type;
		length = t.length;
		switch (type){
			case IPv4: value.ip4 = t.value.ip4;break;
			case IfIndex: value.ifIndex.addr = t.value.ifIndex.addr; value.ifIndex.interfaceID = t.value.ifIndex.interfaceID;break;
			default: break;
		}
		return *this;
	}
	NetAddress getAddress() const { 
		switch (type){
			case IPv4: return value.ip4;
			case IfIndex: return value.ifIndex.addr;
			default: return 0;
		}
	}
	uint16 size() const { return length; } 
	uint16 getType() const { return type; }
	uint32 getIfID() const { 
		if (type!=IfIndex) return 0;
		else return (value.ifIndex.interfaceID);
	}
	
};
#define IMPLEMENT_ORDER_RSVP_HOP_TLV(OP) \
extern inline bool operator OP(const RSVP_HOP_TLV_SUB_Object& o1, const RSVP_HOP_TLV_SUB_Object& o2) {\
		if (o1.type!=o2.type) return (o1.type OP o2.type); \
		else switch(o2.type){ \
			case RSVP_HOP_TLV_SUB_Object::IPv4: return (o1.value.ip4 OP o2.value.ip4); \
			case RSVP_HOP_TLV_SUB_Object::IfIndex: if (o1.value.ifIndex.addr!=o2.value.ifIndex.addr) return (o1.value.ifIndex.addr OP o2.value.ifIndex.addr); \
						else return (o1.value.ifIndex.interfaceID OP o2.value.ifIndex.interfaceID); \
			default: return false; \
		} \
	}
IMPLEMENT_ORDER_RSVP_HOP_TLV(==)
IMPLEMENT_ORDER_RSVP_HOP_TLV(!=)
IMPLEMENT_ORDER_RSVP_HOP_TLV(>=)
IMPLEMENT_ORDER_RSVP_HOP_TLV(<=)
IMPLEMENT_ORDER_RSVP_HOP_TLV(>)
IMPLEMENT_ORDER_RSVP_HOP_TLV(<)

class RSVP_HOP_Object {
	NetAddress hopAddress;
	uint32 LIH;
	RSVP_HOP_TLV_SUB_Object tlv;
	friend ostream& operator<< ( ostream&, const RSVP_HOP_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const RSVP_HOP_Object& );
	DECLARE_ORDER(RSVP_HOP_Object)
public:
	RSVP_HOP_Object() : 
		hopAddress(0), LIH(0)
	{	
		RSVP_HOP_TLV_SUB_Object t;
		tlv = t;
	}
	RSVP_HOP_Object( const NetAddress& hopAddress, uint32 LIH )
	        : hopAddress(hopAddress), LIH(LIH) {
		RSVP_HOP_TLV_SUB_Object t;
		tlv = t;
	}
	RSVP_HOP_Object( const NetAddress& hopAddress, uint32 LIH, RSVP_HOP_TLV_SUB_Object& t )
                : hopAddress(hopAddress), LIH(LIH) { 
	        tlv = t;
	}
	RSVP_HOP_Object(const RSVP_HOP_Object& t) { *this = t; }
	const uint16 size() const { return NetAddress::size() + 
							tlv.size() +
							4; }
	const uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	void readFromBuffer( INetworkBuffer&, uint16 len,  uint8 C_Type);
	const NetAddress& getAddress() const { return hopAddress; }
	void setAddress( const NetAddress& a ) { hopAddress = a; }
	uint32 getLIH()	const { return LIH; }
	void setLIH( uint32 LIH ) { this->LIH = LIH; }
	void setTLV(RSVP_HOP_TLV_SUB_Object& t) { tlv = t; }
	const RSVP_HOP_TLV_SUB_Object& getTLV() const { return tlv; }
	RSVP_HOP_Object& operator=(const RSVP_HOP_Object & t) {
		hopAddress = t.hopAddress;
		LIH = t.LIH;
		tlv = t.tlv;
		return *this;
	}
};
IMPLEMENT_ORDER2(RSVP_HOP_Object,hopAddress,LIH)


class TIME_VALUES_Object {
	uint32 refreshPeriod;
	RSVP_OBJECT_METHODS(TIME_VALUES)
	DECLARE_ORDER(TIME_VALUES_Object)
	static uint16 size() { return 4; }
public:
	TIME_VALUES_Object( const TimeValue& refreshPeriod = 0 )
		: refreshPeriod(refreshPeriod.tv_sec*MSECS_PER_SEC+refreshPeriod.tv_usec/USECS_PER_MSEC) {}
	TIME_VALUES_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	uint32 getRefreshPeriod() const { return refreshPeriod; }
	TimeValue getRefreshTime() const { return TimeValue( refreshPeriod/MSECS_PER_SEC, (refreshPeriod%MSECS_PER_SEC)*USECS_PER_MSEC ); }
};
IMPLEMENT_ORDER1(TIME_VALUES_Object,refreshPeriod)

class ERROR_SPEC_Object {
public:
	enum ErrorCode {
		Confirmation = 0,
		AdmissionControlFailure = 1,
		PolicyControlFailure = 2,
		NoPathInformation = 3,
		NoSenderInformation = 4,
		ConflictingReservationStyle = 5,
		UnknownReservationStyle = 6,
		ConflictingDestPorts = 7,
		ConflictingSenderPorts = 8,
		ServicePreempted = 12,
		UnknownObjectClass = 13,
		UnknownObjectCType = 14,
		ErrorAPI = 20,
		TrafficControlError = 21,
		TrafficControlSystemError = 22,
		RSVPSystemError = 23,
	 	RoutingProblem = 24,
		Notify = 25
	};
	enum ErrorFlag { InPlace = 1, NotGuilty = 2 };
	enum RoutingError {
		BadExplicitRoute = 1, 
		BadStrictNode = 2,
		BadLooseNode = 3,
		BadInitiaSubobject = 4,
		NoRouteAvailToDest = 5,
		UnacceptableLabel = 6,
		RROIndicatedRoutingLoops = 7,
		NonRSVPCapableRouterPresent = 8,
		MPLSLabelAllocationFailure = 9,
		UnsupportedL3PID = 10
	};
	enum NotifyError {
		RROtooLargeForMTU = 1,
		RRONotification = 2,
		TunnelLocallyRepaired = 3
	};
private:
	NetAddress nodeAddress;
	uint8 flags;
	uint8 errorCode;
	uint16 errorValue;
	RSVP_OBJECT_METHODS(ERROR_SPEC)
	static uint16 size() { return NetAddress::size() + 4; }
public:
	ERROR_SPEC_Object() {}
	ERROR_SPEC_Object( NetAddress nodeAddress, uint8 flags, uint8 errorCode, uint16 errorValue )
		: nodeAddress(nodeAddress), flags(flags),
			errorCode(errorCode), errorValue(errorValue) {}
	ERROR_SPEC_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	const NetAddress& getAddress() const { return nodeAddress; }
	uint8 getFlags() const { return flags; }
	void addFlag( ErrorFlag f ) { flags |= f; }
	void removeFlag( ErrorFlag f ) { flags &= ~f; }
	uint8 getCode() const { return errorCode; }
	uint16 getValue() const { return errorValue; }
};

class SCOPE_Object : public RefObject<SCOPE_Object> {
	AddressList addressList;
	friend ostream& operator<< ( ostream&, const SCOPE_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const SCOPE_Object& );
	void readFromBuffer( INetworkBuffer&, uint16 );
	REF_OBJECT_METHODS(SCOPE_Object)
	uint16 size() const { return (addressList.size() * NetAddress::size()); }
public:
	SCOPE_Object() {}
	SCOPE_Object( INetworkBuffer& b, uint16 len ) { readFromBuffer( b, len ); }
	bool operator==( const SCOPE_Object& s ) const {
		return addressList == s.addressList;
	}
	void addAddress( const NetAddress& addr ) {
		addressList.insert_unique( addr );
	}
	const AddressList& getAddressList() const { return addressList; }
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
};
extern inline SCOPE_Object::~SCOPE_Object() {}

class STYLE_Object {
public:
private:
	uint32 optionVector;
	RSVP_OBJECT_METHODS(STYLE)
	DECLARE_ORDER(STYLE_Object)
	static uint16 size() { return 4; }
public:
	STYLE_Object() {}
	STYLE_Object( FilterStyle filterStyle ) : optionVector(filterStyle) {}
	STYLE_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	bool isCompatible(const STYLE_Object& style) const { return ( optionVector == style.optionVector ); }
	bool isDistinct() const { return ( (optionVector & 24 ) == 8 );	}
	bool isShared() const	{ return ( (optionVector & 24 ) == 16);	}
	bool hasExplicitSenderSelection() const	{ return ( (optionVector & 7 ) == 2);	}
	FilterStyle getStyle() const { return (FilterStyle)optionVector; }
};
IMPLEMENT_ORDER1(STYLE_Object,optionVector)

class SENDER_TEMPLATE_Object : public SENDER_Object {
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const SENDER_TEMPLATE_Object& );
public:
	SENDER_TEMPLATE_Object( NetAddress srcAddress = 0, uint16 srcPort = 0 )
		: SENDER_Object( srcAddress, srcPort ) {}
	SENDER_TEMPLATE_Object( INetworkBuffer& buffer ) : SENDER_Object(buffer) {}
	SENDER_TEMPLATE_Object( const SENDER_Object& o ) : SENDER_Object(o) {}
};

class FILTER_SPEC_Object : public SENDER_Object {
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const FILTER_SPEC_Object& );
	friend ostream& operator<< ( ostream&, const FILTER_SPEC_Object& );
	LABEL_Object label;
public:
	FILTER_SPEC_Object( NetAddress srcAddress = 0, uint16 srcPort = 0 )
		: SENDER_Object( srcAddress, srcPort ), label(0) {}
	FILTER_SPEC_Object( const SENDER_Object& o ) : SENDER_Object(o), label(0) {}
	FILTER_SPEC_Object( const SENDER_Object& o, const LABEL_Object& l )
		: SENDER_Object(o), label(l) {}
	FILTER_SPEC_Object( INetworkBuffer& buffer ) : SENDER_Object(buffer), label(0) {}
	bool hasLabel() const { return label.getLabel() != 0; }
	const LABEL_Object& getLABEL_Object() const { return label; }
	void setLabel( INetworkBuffer& buffer, uint16 len, uint8 C_Type ) { label.readFromBuffer(buffer, len, C_Type); }
	static const FILTER_SPEC_Object anyFilter;
};

class RESV_CONFIRM_Object {
	NetAddress recvAddress;
	RSVP_OBJECT_METHODS(RESV_CONFIRM)
	DECLARE_ORDER(RESV_CONFIRM_Object)
	static uint16 size() { return NetAddress::size(); }
public:
	RESV_CONFIRM_Object() {}
	RESV_CONFIRM_Object( NetAddress recvAddress ) : recvAddress(recvAddress) {}
	RESV_CONFIRM_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	const NetAddress& getAddress() const { return recvAddress; }
	void setAddress( const NetAddress& addr ) { recvAddress = addr; }
};
IMPLEMENT_ORDER1(RESV_CONFIRM_Object,recvAddress)

class UNKNOWN_Object : public RefObject<UNKNOWN_Object> {
	RSVP_ObjectHeader header;
	Buffer content;
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const UNKNOWN_Object& );
	friend ostream& operator<< ( ostream&, const UNKNOWN_Object& );
	REF_OBJECT_METHODS(UNKNOWN_Object)
	uint16 size() const { return header.getLength() - RSVP_ObjectHeader::size(); }
public:
	UNKNOWN_Object( const RSVP_ObjectHeader& header, INetworkBuffer& buffer )
	: header(header), content(buffer.getCurrentPosition(),header.getLength()-RSVP_ObjectHeader::size()) {}
	uint16 total_size() const { return header.getLength(); }
};
extern inline UNKNOWN_Object::~UNKNOWN_Object() {}

#if defined(ONEPASS_RESERVATION)
class DUPLEX_Object {
	uint16 senderReceivePort;
	uint16 receiverSendPort;
	RSVP_OBJECT_METHODS(DUPLEX)
	DECLARE_ORDER(DUPLEX_Object)
	static uint16 size() { return 4; }
public:
	DUPLEX_Object() {}
	DUPLEX_Object( uint16 srp, uint16 rsp )
		: senderReceivePort(srp), receiverSendPort(rsp) {}
	DUPLEX_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	uint16 getSenderReceivePort() const { return senderReceivePort; }
	void setSenderReceivePort( uint16 p ) { senderReceivePort = p; }
	uint16 getReceiverSendPort() const { return receiverSendPort; }
	void setReceiverSendPort( uint16 p ) { receiverSendPort = p; }
};
IMPLEMENT_ORDER2(DUPLEX_Object,senderReceivePort,receiverSendPort)
#endif

#if defined(REFRESH_REDUCTION)
class MESSAGE_ID_BASE_Object {
protected:
	uint32 flagsEpoch;
	sint32 id;
	friend ostream& operator<< ( ostream&, const MESSAGE_ID_BASE_Object& );
	friend INetworkBuffer& operator>> ( INetworkBuffer&, MESSAGE_ID_BASE_Object& );
	static uint16 size() { return 8; }
public:
	MESSAGE_ID_BASE_Object() {}
	MESSAGE_ID_BASE_Object( uint8 flags, uint32 epoch, sint32 id )
		: flagsEpoch(flags*16777216+epoch%16777216), id(id) {}
	MESSAGE_ID_BASE_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	uint8 getFlags() const { return flagsEpoch / 16777216; }
	uint32 getEpoch() const { return flagsEpoch % 16777216; }
	sint32 getID() const { return id; }
};

class MESSAGE_ID_Object : public MESSAGE_ID_BASE_Object {
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const MESSAGE_ID_Object& );
public:
	enum Flags { ACK_Desired = 0x01 };
	MESSAGE_ID_Object() {}
	MESSAGE_ID_Object( uint8 flags, uint32 epoch, uint32 id )
		: MESSAGE_ID_BASE_Object( flags, epoch, id ) {}
	MESSAGE_ID_Object( INetworkBuffer& buffer ) : MESSAGE_ID_BASE_Object(buffer) {}
	MESSAGE_ID_Object( const MESSAGE_ID_BASE_Object& o ) : MESSAGE_ID_BASE_Object(o) {}
	void setFlags( uint8 flags ) { flagsEpoch = flags*16777216+flagsEpoch%16777216; }
};

class MESSAGE_ID_ACK_Object : public MESSAGE_ID_BASE_Object {
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const MESSAGE_ID_ACK_Object& );
public:
	MESSAGE_ID_ACK_Object() {}
	MESSAGE_ID_ACK_Object( uint8 flags, uint32 epoch, uint32 id )
		: MESSAGE_ID_BASE_Object( flags, epoch, id ) {}
	MESSAGE_ID_ACK_Object( INetworkBuffer& buffer ) : MESSAGE_ID_BASE_Object(buffer) {}
	MESSAGE_ID_ACK_Object( const MESSAGE_ID_BASE_Object& o ) : MESSAGE_ID_BASE_Object(o) {}
};
DEDICATED_LIST_MEMORY_MACHINE(MESSAGE_ID_ACK_Object,SimpleList<MESSAGE_ID_ACK_Object>,msgIdAckListMemMachine)
	
class MESSAGE_ID_NACK_Object : public MESSAGE_ID_BASE_Object {
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const MESSAGE_ID_NACK_Object& );
public:
	MESSAGE_ID_NACK_Object() {}
	MESSAGE_ID_NACK_Object( uint8 flags, uint32 epoch, uint32 id )
		: MESSAGE_ID_BASE_Object( flags, epoch, id ) {}
	MESSAGE_ID_NACK_Object( INetworkBuffer& buffer ) : MESSAGE_ID_BASE_Object(buffer) {}
	MESSAGE_ID_NACK_Object( const MESSAGE_ID_BASE_Object& o ) : MESSAGE_ID_BASE_Object(o) {}
};
DEDICATED_LIST_MEMORY_MACHINE(MESSAGE_ID_NACK_Object,SimpleList<MESSAGE_ID_NACK_Object>,msgIdNackListMemMachine)

typedef SimpleList<MESSAGE_ID_ACK_Object> MESSAGE_ID_ACK_List;
typedef SimpleList<MESSAGE_ID_NACK_Object> MESSAGE_ID_NACK_List;
	
class MESSAGE_ID_LIST_Object {
	uint32 flagsEpoch;
	SimpleList<sint32> idList;
	friend ostream& operator<< ( ostream&, const MESSAGE_ID_LIST_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const MESSAGE_ID_LIST_Object& );
	uint16 size() const { return 4 + 4 * idList.size(); }
public:
	MESSAGE_ID_LIST_Object() {}
	MESSAGE_ID_LIST_Object( uint8 flags, uint32 epoch )
		: flagsEpoch(flags*16777216+epoch) {}
	void init() { idList.clear(); }
	void readFromBuffer( INetworkBuffer&, uint16 );
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	uint8 getFlags() const { return flagsEpoch / 16777216; }
	uint32 getEpoch() const { return flagsEpoch % 16777216; }
	void addID( sint32 id ) { idList.push_back( id ); }
	const SimpleList<sint32>& getID_List() const { return idList; }
	static uint32 minSize() { return 4; }
	static uint32 idSize() { return sizeof(sint32); }
};
#endif

//////////////////////////////////////////////////////////////////////////
/////                             UNI Object definitions                                                   /////
/////////////////////////////////////////////////////////////////////////

#define UNI_SUBOBJ_SRCTNA 1
#define UNI_SUBOBJ_DESTTNA 2
#define UNI_SUBOBJ_DIVERSITY 3
#define UNI_SUBOBJ_EGRESSLABEL 4
#define UNI_SUBOBJ_VLANTAG 10
#define UNI_SUBOBJ_CTRLCHAN 11

#define UNI_SUBTYPE_NONE 0

#define UNI_TNA_SUBTYPE_IPV4 1
#define UNI_TNA_SUBTYPE_IPV6 2
#define UNI_TNA_SUBTYPE_NSAP 3
#define UNI_TNA_SUBTYPE_LCLID 4

#define UNI_SUBOBJ_CTRLCHAN_INGRESS 1
#define UNI_SUBOBJ_CTRLCHAN_EGRESS 1

struct IPv4TNA {
	uint16 length;
	uint8 type;
	uint8 sub_type;
	struct in_addr addr;
};

struct LocalIdTNA {
	uint16 length;
	uint8 type;
	uint8 sub_type;
	struct in_addr addr;
	uint32 local_id;
};

struct VlanTag {
	uint16 length;
	uint8 type;
	uint8 sub_type;
	uint32 vtag;
};

struct CtrlChannel {
	uint16 length;
	uint8 type;
	uint8 sub_type;
	uint8 name[12];
};

class DRAGON_UNI_Object: public RefObject<DRAGON_UNI_Object> {
	LocalIdTNA srcTNA;
	LocalIdTNA destTNA;
	struct VlanTag vlanTag;
	struct CtrlChannel ingressChannelName;
	struct CtrlChannel egressChannelName;
	friend ostream& operator<< ( ostream&, const DRAGON_UNI_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const DRAGON_UNI_Object& );
	uint16 size() const{ 
		return (sizeof(struct LocalIdTNA)*2 + sizeof(struct VlanTag) + sizeof(struct CtrlChannel)*2);
	}
	REF_OBJECT_METHODS(DRAGON_UNI_Object)

public:
	DRAGON_UNI_Object() {
		srcTNA.length = sizeof(struct LocalIdTNA);
		srcTNA.type = UNI_SUBOBJ_SRCTNA;
		srcTNA.sub_type = UNI_TNA_SUBTYPE_LCLID;
		srcTNA.addr.s_addr = srcTNA.local_id = 0;
		destTNA = srcTNA; destTNA.type = UNI_SUBOBJ_DESTTNA;
		vlanTag.length = sizeof(struct VlanTag);
		vlanTag.type = UNI_SUBOBJ_VLANTAG;
		vlanTag.sub_type = UNI_SUBTYPE_NONE;
		vlanTag.vtag = 0;
		memset (&ingressChannelName, 0, sizeof(struct CtrlChannel));
		memset (&egressChannelName, 0, sizeof(struct CtrlChannel));
	}
	DRAGON_UNI_Object( struct in_addr src_addr, uint32 src_lclid, struct in_addr dest_addr, 
	uint32 dest_lclid, uint32 vtag, char* ing_chan_name, char* eg_chan_name ) {
		srcTNA.length = sizeof(struct LocalIdTNA);
		srcTNA.type = UNI_SUBOBJ_SRCTNA;
		srcTNA.sub_type = UNI_TNA_SUBTYPE_LCLID;
		destTNA = srcTNA; destTNA.type = UNI_SUBOBJ_DESTTNA;
		srcTNA.addr = src_addr;
		srcTNA.local_id = src_lclid;
		destTNA.addr = dest_addr;
		destTNA.local_id = dest_lclid;
		vlanTag.length = sizeof(struct VlanTag);
		vlanTag.type = UNI_SUBOBJ_VLANTAG;
		vlanTag.sub_type = UNI_SUBTYPE_NONE;
		vlanTag.vtag = vtag;
		strncpy((char*)ingressChannelName.name, ing_chan_name, 12);
		ingressChannelName.length= sizeof(struct CtrlChannel);
		ingressChannelName.type = UNI_SUBOBJ_CTRLCHAN;
		ingressChannelName.sub_type = UNI_SUBOBJ_CTRLCHAN_INGRESS;
		strncpy((char*)egressChannelName.name, eg_chan_name, 12);
		egressChannelName.length= sizeof(struct CtrlChannel);
		egressChannelName.type = UNI_SUBOBJ_CTRLCHAN_EGRESS;
		egressChannelName.sub_type = UNI_SUBTYPE_NONE;
	}
	DRAGON_UNI_Object(INetworkBuffer& buffer, uint16 len) {
		readFromBuffer(buffer, len );
	}
	void readFromBuffer( INetworkBuffer& buffer, uint16 len);
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	LocalIdTNA& getSrcTNA() { return srcTNA; }
	LocalIdTNA& getDestTNA(){ return destTNA; }
	CtrlChannel& getIngressCtrlChannel(){ return ingressChannelName; }
	CtrlChannel& getEgressCtrlChannel(){ return egressChannelName; }
	bool operator==(const DRAGON_UNI_Object& s){
		return (memcmp(&srcTNA, &s.srcTNA, sizeof(struct LocalIdTNA)) == 0 && memcmp(&destTNA, &s.destTNA, sizeof(struct LocalIdTNA)) == 0);
	}
};
extern inline DRAGON_UNI_Object::~DRAGON_UNI_Object() { }

#endif /* _RSVP_ProtocolObjects_h_ */
