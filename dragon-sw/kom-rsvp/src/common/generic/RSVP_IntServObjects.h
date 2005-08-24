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
#ifndef _RSVP_IntServObjects_h_
#define _RSVP_IntServObjects_h_ 1

#include "RSVP_Helper.h"
#include "RSVP_IntServComponents.h"
#include "RSVP_ObjectHeader.h"
#include "RSVP_RefObject.h"

class INetworkBuffer;

class MessageHeader {
	static const uint8 version = 0;
	static const uint16 reserved = 0;
	uint16 length; // in words
	friend inline ostream& operator<<( ostream&, const MessageHeader& );
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, MessageHeader& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const MessageHeader& );
public:
	MessageHeader( uint16 length = 0 ) : length(length) {}
	static inline uint16 size() { return 4; }
	uint16 getLength() const { return length; }
};

class ServiceHeader {
	uint8 number;
	bool breakBit;
	uint16 length;  // in words
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, ServiceHeader& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const ServiceHeader& );
 public:
	static const uint8 Default = 1;
	static const uint8 Guaranteed = 2;
	static const uint8 ControlledLoad = 5;
	ServiceHeader( uint8 number = 0, bool breakBit = 0, uint16 length = 0 )
		: number(number), breakBit(breakBit), length(length) {}
	static inline uint16 size() { return 4; }
	bool hasBreakBit() const { return breakBit; }
	uint8 getNumber() const { return number; }
	uint16 getLength() const { return length; }
};

class Parameter {
	uint8 ID;
	uint8 flags;
	uint16 length; // in words
	friend inline ostream& operator<<( ostream&, const Parameter& );
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, Parameter& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const Parameter& );
public:
	Parameter( uint8 ID = 0, uint8 flags = 0, uint16 length = 0 )
		: ID(ID), flags(flags), length(length) {}
	uint8 getID() const { return ID; }
	uint16 getLength() { return length; }
	static inline uint16 size() { return 4; }
};

class AdSpecGeneralParameters {
protected:
	uint32 hopCount;
	ieee32float bandwidth;
	sint32 minPathLatency;
	uint32 MTU;
	friend inline ostream& operator<<( ostream&, const AdSpecGeneralParameters& );
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, AdSpecGeneralParameters& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const AdSpecGeneralParameters& );
public:
	AdSpecGeneralParameters( uint32 hopCount, ieee32float bandwidth,
		sint32 minPathLatency, uint32 MTU) : hopCount(hopCount),
			bandwidth(bandwidth), minPathLatency(minPathLatency), MTU(MTU) {}
	uint32 getHopCount() const { return hopCount; }
	ieee32float getBandwidth() const { return bandwidth; }
	sint32 getMinPathLatency() const { return minPathLatency; }
	uint32 getMTU() const { return MTU; }
	inline void updateBy( const AdSpecGeneralParameters& );
	static inline uint16 size() { return (3 * sizeof(uint32) + sizeof(ieee32float) + 4 * Parameter::size()); }
};

class AdSpecOverrideParameters : public AdSpecGeneralParameters {
	enum OverrideFlags { hopCountOverride = 1, bandwidthOverride = 2, minPathLatencyOverride = 4,  MTUOverride = 8 };
	uint8 flags;
	uint16 length;  // in bytes
	friend inline ostream& operator<<( ostream&, const AdSpecOverrideParameters& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const AdSpecOverrideParameters& );
public:
	AdSpecOverrideParameters() : AdSpecGeneralParameters( 0, 0, 0, 0 ), flags(0), length(0) {}
	void setHopCount( uint32 hopCount ) {
		this->hopCount = hopCount; flags |= hopCountOverride;
		length += Parameter::size() + sizeof(uint32);
	}
	bool hasHopCount() const { return flags & hopCountOverride; }
	void setBandwidth( const ieee32float& bandwidth) {
		this->bandwidth = bandwidth; flags |= bandwidthOverride;
		length += Parameter::size() + sizeof(uint32);
	}
	bool hasBandwidth() const { return flags & bandwidthOverride; }
	void setMinPathLatency( sint32 minPathLatency ) {
		this->minPathLatency = minPathLatency; flags |= minPathLatencyOverride;
		length += Parameter::size() + sizeof(sint32);
	}
	bool hasMinPathLatency() const { return flags & minPathLatencyOverride; }
	void setMTU( uint32 MTU ) {
		this->MTU = MTU; flags |= MTUOverride;
		length += Parameter::size() + sizeof(uint32);
	}
	bool hasMTU() const { return flags & MTUOverride; }
	void readFromBuffer( INetworkBuffer&, uint16 );
	inline void updateBy( const AdSpecOverrideParameters& );
	uint16 size() const { return length; }
};

class AdSpecCLParameters {
	bool breakBit;
	friend inline ostream& operator<<( ostream&, const AdSpecCLParameters& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const AdSpecCLParameters& );
public:
	AdSpecOverrideParameters override;
	AdSpecCLParameters() : breakBit(false) {}
	AdSpecCLParameters( INetworkBuffer&, uint16  );
	void setBreakBit( bool b ) { breakBit = b; }
	bool hasBreakBit() const { return breakBit; }
	inline void updateBy( const AdSpecCLParameters& clp );
	uint16 size() const { return override.size(); }
};

class AdSpecGSParameters {
	sint32 Ctot, Dtot, Csum, Dsum;
	bool breakBit;
	friend inline ostream& operator<<( ostream&, const AdSpecGSParameters& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const AdSpecGSParameters& );
public:
	AdSpecOverrideParameters override;
	AdSpecGSParameters( INetworkBuffer&, uint16 );
	AdSpecGSParameters( sint32 Ctot = 0, sint32 Dtot = 0, sint32 Csum = 0, sint32 Dsum = 0 ) 
		: Ctot(Ctot), Dtot(Dtot), Csum(Csum), Dsum(Dsum), breakBit(false) {}
	uint16 size() const { 
		return 4 * (sizeof(sint32) + Parameter::size()) + override.size();
	}
	void setBreakBit( bool b ) { breakBit = b; }
	bool hasBreakBit() const { return breakBit; }
	const ErrorTerms getTotError() const { return ErrorTerms(Ctot,Dtot); }
	const ErrorTerms getSumError() const { return ErrorTerms(Csum,Dsum); }
	inline void updateBy( const AdSpecGSParameters& );
	void setShaped() { Csum = 0; Dsum = 0; }
};

class FLOWSPEC_Object : public RefObject<FLOWSPEC_Object>, public TSpec, public RSpec, public SONET_TSpec{
	uint16 length;  // in bytes
	uint8 serviceNumber;
	static const uint8 SonetSDH_FlowSpec = 255; //This value does not overlap with ServiceHeader::Default/CtrlLoad/Guaranteed
	friend ostream& operator<< ( ostream&, const FLOWSPEC_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const FLOWSPEC_Object& );
	REF_OBJECT_METHODS(FLOWSPEC_Object)
	friend inline bool operator== ( const FLOWSPEC_Object&, const FLOWSPEC_Object& );
	friend inline bool operator!= ( const FLOWSPEC_Object&, const FLOWSPEC_Object& );
	friend inline bool operator>= ( const FLOWSPEC_Object&, const FLOWSPEC_Object& );
	friend inline bool operator<= ( const FLOWSPEC_Object&, const FLOWSPEC_Object& );
	friend inline bool operator> ( const FLOWSPEC_Object&, const FLOWSPEC_Object& );
	friend inline bool operator< ( const FLOWSPEC_Object&, const FLOWSPEC_Object& );
	uint16 size() const { return length; }
	void readFromBuffer( INetworkBuffer& buffer, uint16 len, uint8 C_Type);
public:
	FLOWSPEC_Object() : RSpec( (ieee32float)0, sint32Infinite ),
		length( MessageHeader::size() + ServiceHeader::size() + Parameter::size() + TSpec::size() ),
		serviceNumber(ServiceHeader::Default) {
	}
	FLOWSPEC_Object( const TSpec& tspec ) : TSpec(tspec), RSpec( (ieee32float)0, sint32Infinite ),
		length( MessageHeader::size() + ServiceHeader::size() + Parameter::size() + TSpec::size() ),
		serviceNumber(ServiceHeader::ControlledLoad) {
	}
	FLOWSPEC_Object( const TSpec& tspec, const RSpec& rspec ) : TSpec(tspec), RSpec(rspec),
		length( MessageHeader::size() + ServiceHeader::size() + Parameter::size() + TSpec::size() + Parameter::size() + RSpec::size() ),
		serviceNumber(ServiceHeader::Guaranteed) {
	}
	FLOWSPEC_Object( const SONET_TSpec& sonetTspec ) : SONET_TSpec(sonetTspec),
		length( SONET_TSpec::size()),
		serviceNumber(SonetSDH_FlowSpec){
	}
	FLOWSPEC_Object( INetworkBuffer& buffer, uint16 len, uint8 C_Type ) : RSpec( (ieee32float)0, sint32Infinite ) { readFromBuffer(buffer, len, C_Type); }

	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	uint8 getServiceNumber() const { return serviceNumber; }

	ieee32float getEffectiveRate() const {
		switch (serviceNumber) {
		case ServiceHeader::Guaranteed:			return get_R();
		case ServiceHeader::ControlledLoad:	return get_r();
		default: return 0;
		}
	}
	ieee32float getEffectiveBuffer( const ErrorTerms& et ) const {
		switch (serviceNumber) {
		case ServiceHeader::Guaranteed:			return calculateBuffer( et, get_R() );
		case ServiceHeader::ControlledLoad:	return calculateBuffer( et, get_r() );
		default: return 0;
		}
	}

	void doOperation( const FLOWSPEC_Object& f, TSpec & (TSpec::*TSpecOp)(const TSpec &), RSpec & (RSpec::*RSpecOp)(const RSpec &) ) {
		if ( serviceNumber == ServiceHeader::Guaranteed || f.serviceNumber == ServiceHeader::Guaranteed ) {
			serviceNumber = ServiceHeader::Guaranteed;
			length = MessageHeader::size() + ServiceHeader::size()
				+ Parameter::size() + TSpec::size() + Parameter::size() + RSpec::size();
			(static_cast<TSpec*>(this)->*TSpecOp)( static_cast<const TSpec&>(f) );
			(static_cast<RSpec*>(this)->*RSpecOp)( static_cast<const RSpec&>(f) );
		} else if ( serviceNumber == ServiceHeader::ControlledLoad || f.serviceNumber == ServiceHeader::ControlledLoad ) {
			serviceNumber = ServiceHeader::ControlledLoad;
			length = MessageHeader::size() + ServiceHeader::size()
				+ Parameter::size() + TSpec::size();
			(static_cast<TSpec*>(this)->*TSpecOp)( static_cast<const TSpec&>(f) );
		}
		else {
			serviceNumber = ServiceHeader::Default;
			length = MessageHeader::size() + ServiceHeader::size();
		}
	}

	void doOperationSonet( const FLOWSPEC_Object& f, SONET_TSpec & (SONET_TSpec::*TSpecOp)(const SONET_TSpec &)) {
		serviceNumber = SonetSDH_FlowSpec;
		length = SONET_TSpec::size();
		(static_cast<SONET_TSpec*>(this)->*TSpecOp)( static_cast<const SONET_TSpec&>(f) );
	}
	void LUB( const FLOWSPEC_Object& f ) {
		if (serviceNumber == SonetSDH_FlowSpec || f.serviceNumber == SonetSDH_FlowSpec)
			doOperationSonet(f, &SONET_TSpec::LUB);
		else
			doOperation( f, &TSpec::LUB, &RSpec::LUB );
	}
	void GLB( const FLOWSPEC_Object& f ) {
		if (serviceNumber == SonetSDH_FlowSpec || f.serviceNumber == SonetSDH_FlowSpec)
			doOperationSonet(f, &SONET_TSpec::GLB);
		else
			doOperation( f, &TSpec::GLB, &RSpec::GLB );
	}
	void merge( const FLOWSPEC_Object& f ) {
		if (serviceNumber == SonetSDH_FlowSpec || f.serviceNumber == SonetSDH_FlowSpec)
			doOperationSonet(f, &SONET_TSpec::merge);
		else
			doOperation( f, &TSpec::merge, &RSpec::merge );
	}
	DECLARE_MEMORY_MACHINE_IN_CLASS(FLOWSPEC_Object)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(FLOWSPEC_Object,flowspecMemMachine)
extern inline FLOWSPEC_Object::~FLOWSPEC_Object() {}
extern inline bool ordered( const FLOWSPEC_Object& f1, const FLOWSPEC_Object& f2 ) {
	return f1 >= f2 || f1 <= f2;
}
extern inline bool operator==( const FLOWSPEC_Object& f1 , const FLOWSPEC_Object& f2 ) {
	if (f1.serviceNumber != f2.serviceNumber) 
		return false;
	if (f1.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec || f2.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec)
		return static_cast<const SONET_TSpec&>(f1) == static_cast<const SONET_TSpec&>(f2);
	else
		return static_cast<const TSpec&>(f1) == static_cast<const TSpec&>(f2)
				&& static_cast<const RSpec&>(f1) == static_cast<const RSpec&>(f2);
}
extern inline bool operator!=( const FLOWSPEC_Object& f1 , const FLOWSPEC_Object& f2 ) {
	if (f1.serviceNumber != f2.serviceNumber) 
		return true;
	if (f1.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec || f2.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec)
		return static_cast<const SONET_TSpec&>(f1) != static_cast<const SONET_TSpec&>(f2);
	else
		return static_cast<const TSpec&>(f1) != static_cast<const TSpec&>(f2)
				|| static_cast<const RSpec&>(f1) != static_cast<const RSpec&>(f2);
}
extern inline bool operator<=( const FLOWSPEC_Object& f1 , const FLOWSPEC_Object& f2 ) {
	if (f1.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec || f2.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec)
		return static_cast<const SONET_TSpec&>(f1) <= static_cast<const SONET_TSpec&>(f2);
	else
		return static_cast<const TSpec&>(f1) <= static_cast<const TSpec&>(f2)
				&& static_cast<const RSpec&>(f1) <= static_cast<const RSpec&>(f2);
}
extern inline bool operator>=( const FLOWSPEC_Object& f1 , const FLOWSPEC_Object& f2 ) {
	if (f1.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec || f2.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec)
		return static_cast<const SONET_TSpec&>(f1) >= static_cast<const SONET_TSpec&>(f2);
	else
		return static_cast<const TSpec&>(f1) >= static_cast<const TSpec&>(f2)
				&& static_cast<const RSpec&>(f1) >= static_cast<const RSpec&>(f2);
}
extern inline bool operator< ( const FLOWSPEC_Object& f1 , const FLOWSPEC_Object& f2 ) {
	if ( f1.serviceNumber == ServiceHeader::Guaranteed || f2.serviceNumber == ServiceHeader::Guaranteed ) {
		return static_cast<const TSpec&>(f1) <= static_cast<const TSpec&>(f2)
				&& static_cast<const RSpec&>(f1) < static_cast<const RSpec&>(f2);
	}
	else if (f1.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec || f2.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec){
		return static_cast<const SONET_TSpec&>(f1) <= static_cast<const SONET_TSpec&>(f2);
	}
	else {
		return static_cast<const TSpec&>(f1) < static_cast<const TSpec&>(f2);
	}
}
extern inline bool operator> ( const FLOWSPEC_Object& f1 , const FLOWSPEC_Object& f2 ) {
	if ( f1.serviceNumber == ServiceHeader::Guaranteed || f2.serviceNumber == ServiceHeader::Guaranteed ) {
		return static_cast<const TSpec&>(f1) >= static_cast<const TSpec&>(f2)
				&& static_cast<const RSpec&>(f1) > static_cast<const RSpec&>(f2);
	}
	else if (f1.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec || f2.serviceNumber == FLOWSPEC_Object::SonetSDH_FlowSpec){
		return static_cast<const SONET_TSpec&>(f1) >= static_cast<const SONET_TSpec&>(f2);
	}
	else {
		return static_cast<const TSpec&>(f1) > static_cast<const TSpec&>(f2);
	}
}

class SENDER_TSPEC_Object : public TSpec , public SONET_TSpec{
	uint16 length;  // in bytes
	uint8 service;
	friend ostream& operator<< ( ostream&, const SENDER_TSPEC_Object& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const SENDER_TSPEC_Object& );
	uint16 size() const { return length; }
public:
	static const uint8 GMPLS_Sender_Tspec = 2;
	static const uint8 SonetSDH_Sender_Tspec = 4;
	SENDER_TSPEC_Object( const TSpec& tspec = TSpec(0) ) : TSpec(tspec),
		length(MessageHeader::size()+ ServiceHeader::size() + Parameter::size() + TSpec::size()),
		service(GMPLS_Sender_Tspec){}
	SENDER_TSPEC_Object( const SONET_TSpec& tspec) : SONET_TSpec(tspec),
		length(SONET_TSpec::size()),
		service(SonetSDH_Sender_Tspec){}
	uint16 total_size() const { return size()  + RSVP_ObjectHeader::size(); }
	void readFromBuffer( INetworkBuffer& buffer, uint16 len, uint8 C_Type);
	uint8 getService() const { return service; }
	SENDER_TSPEC_Object& operator=(const SENDER_TSPEC_Object & t) {
		length = t.length;
		service = t.service;
		if (service == GMPLS_Sender_Tspec){
			static_cast<TSpec&>(*this) = static_cast<const TSpec&>(t);
		}
		else if (service == SonetSDH_Sender_Tspec){
			static_cast<SONET_TSpec&>(*this) = static_cast<const SONET_TSpec&>(t);
		}
		return *this;
	}
};

class ADSPEC_Object : public RefObject<ADSPEC_Object> {
	uint16 length;  // in bytes
	AdSpecGeneralParameters gp;
	AdSpecCLParameters* clp;
	AdSpecGSParameters* gsp;
	RSVP_OBJECT_METHODS(ADSPEC)
	REF_OBJECT_METHODS(ADSPEC_Object)
	uint16 size() const { return length; }
public:
	ADSPEC_Object( uint32 hopCount, ieee32float bandwidth, sint32 minPathLateny, uint32 MTU )
		: length( MessageHeader::size() + ServiceHeader::size() + AdSpecGeneralParameters::size() ),
			gp( hopCount, bandwidth, minPathLateny, MTU ), clp(NULL), gsp(NULL) {
	}
	ADSPEC_Object( const AdSpecGeneralParameters& gp )
		: length( MessageHeader::size() + ServiceHeader::size() + AdSpecGeneralParameters::size() ),
			gp( gp ), clp(NULL), gsp(NULL) {
	}
	ADSPEC_Object( INetworkBuffer& buffer )
		: gp( 0, ieee32floatInfinite, 0, uint32Infinite ), clp(NULL), gsp(NULL) { buffer >> *this; }
	void addCL( const AdSpecCLParameters &cl );
	void addGS( const AdSpecGSParameters &gs );
	void updateBy( const ADSPEC_Object& );
	ADSPEC_Object* clone() const {
		ADSPEC_Object* newAdspec = new ADSPEC_Object( gp );
		if (clp) newAdspec->clp = new AdSpecCLParameters(*clp);
		if (gsp) newAdspec->gsp = new AdSpecGSParameters(*gsp);
		newAdspec->length = length;
		return newAdspec;
	}
	const AdSpecGeneralParameters& getAdSpecGeneralParameters() const { return gp; }
	bool supportsCL() const { return clp && !clp->hasBreakBit(); }
	const AdSpecCLParameters& getAdSpecCLParameters() const { assert(clp); return *clp; }
	void setBreakBitCL( bool b ) { if (clp) clp->setBreakBit( b ); }
	bool supportsGS() const { return gsp && !gsp->hasBreakBit(); }
	const AdSpecGSParameters& getAdSpecGSParameters() const { assert(gsp); return *gsp; }
	void setBreakBitGS( bool b ) { if (gsp) gsp->setBreakBit( b ); }
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }

DECLARE_MEMORY_MACHINE_IN_CLASS(ADSPEC_Object)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(ADSPEC_Object,adspecMemMachine)

extern inline ADSPEC_Object::~ADSPEC_Object() {}

#endif /* _RSVP_IntServObjects_h_ */
