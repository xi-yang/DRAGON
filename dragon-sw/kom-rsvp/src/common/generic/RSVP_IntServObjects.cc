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

#include "RSVP_IntServObjects.h"

/*********************** MessageHeader ******************************/

inline INetworkBuffer& operator>>( INetworkBuffer& buffer, MessageHeader& m ) {
	uint16 dummy;
	buffer >> dummy >> m.length;
	assert ( MessageHeader::version == dummy / (1<<12)
				&& MessageHeader::reserved == dummy % (1<<12) );
	return buffer;
}

inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const MessageHeader& m ) {
	buffer << (uint16)(m.reserved % (1<<12) + m.version * 16) << m.length;
	return buffer;
}

inline ostream& operator<<( ostream& os, const MessageHeader& m ) {
	os << (uint32)m.version << " " << (uint32)m.reserved << " " << (uint32)m.length;
	return os;
}

/*********************** ServiceHeader ******************************/

inline INetworkBuffer& operator>>( INetworkBuffer& buffer, ServiceHeader& m ) {
	uint8 reserved;
	buffer >> m.number >>	reserved >>	m.length;
	m.breakBit = reserved / (1 << 7);
	return buffer;
}

inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const ServiceHeader& m ) {
	buffer << m.number << (m.breakBit ? (uint8)(1<<7) : (uint8)0) << m.length;
	return buffer;
}

/*********************** Parameter ******************************/

inline INetworkBuffer& operator>>( INetworkBuffer& buffer, Parameter& m ) {
	buffer >> m.ID >> m.flags >> m.length;
	return buffer;
}

inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const Parameter& m ) {
	buffer << m.ID << m.flags << m.length;
	return buffer;
}

inline ostream& operator<<( ostream& os, const Parameter& s ) {
	os << (uint32)s.ID << " " << (uint32)s.flags << " " << (uint32)s.length;
	return os;
}

/*********************** AdSpecGeneralParameters ******************************/

inline INetworkBuffer& operator>>( INetworkBuffer& buffer, AdSpecGeneralParameters& g ) {
	Parameter parameter;
	buffer >> parameter >> g.hopCount >> parameter >> g.bandwidth;
	buffer >> parameter >> g.minPathLatency >> parameter >> g.MTU;
	return buffer;
}
inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const AdSpecGeneralParameters& g ) {
	buffer << Parameter(4,0,1) << g.hopCount << Parameter(6,0,1) << g.bandwidth;
	buffer << Parameter(8,0,1) << g.minPathLatency << Parameter(10,0,1) << g.MTU;
	return buffer;
}
inline ostream& operator<<( ostream& os, const AdSpecGeneralParameters& g ) {
	os << "hops: " << g.hopCount << " bw: " << g.bandwidth;
	os << " lat: " << g.minPathLatency << " MTU: " << g.MTU;
	return os;
}

inline void AdSpecGeneralParameters::updateBy( const AdSpecGeneralParameters& gp ) {
	hopCount += gp.hopCount;
	bandwidth = min( bandwidth, gp.bandwidth );
	minPathLatency += gp.minPathLatency;
	MTU = min( MTU, gp.MTU );
}

/*********************** AdSpecOverrideParameters ******************************/

inline void AdSpecOverrideParameters::readFromBuffer( INetworkBuffer& buffer, uint16 length ) {
	Parameter p(0,0,0);
	while ( length > 0 ) {
		buffer >> p;
		length -= bytesof(p.getLength()) + Parameter::size();
		this->length += bytesof(p.getLength()) + Parameter::size();
		switch(p.getID()) {
			case 4:
				buffer >> hopCount;
				flags |= hopCountOverride;
				break;
			case 6:
				buffer >> bandwidth;
				flags |= bandwidthOverride;
				break;
			case 8:
				buffer >> minPathLatency;
				flags |= minPathLatencyOverride;
				break;
			case 10:
				buffer >> MTU;
				flags |= MTUOverride;
				break;
			default:
				this->length -= bytesof(p.getLength()) + Parameter::size();
				LOG(2)( Log::Error, " ERROR: unknown adspec override parameter ", p.getID() );
		}
	}
}

inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const AdSpecOverrideParameters& g ) {
	if ( g.hasHopCount() ) buffer << Parameter(4,0,1) << g.hopCount;
	if ( g.hasBandwidth() ) buffer << Parameter(6,0,1) << g.bandwidth;
	if ( g.hasMinPathLatency() ) buffer << Parameter(8,0,1) << g.minPathLatency;
	if ( g.hasMTU() ) buffer << Parameter(10,0,1) << g.MTU;
	return buffer;
}

inline ostream& operator<<( ostream& os, const AdSpecOverrideParameters& g ) {
	if ( g.length != 0 ) os << "OVRIDE: ";
	if ( g.hasHopCount() ) os << "hops: " << g.hopCount << " ";
	if ( g.hasBandwidth() ) os << "bw: " << g.bandwidth << " ";
	if ( g.hasMinPathLatency() ) os << "lat: " << g.minPathLatency << " ";
	if ( g.hasMTU() ) os << "MTU: " << g.MTU << " ";
	return os;
}

inline void AdSpecOverrideParameters::updateBy( const AdSpecOverrideParameters& o ) {
	if ( hasHopCount() ) {
		if ( o.hasHopCount() ) hopCount += o.hopCount;
	} else if ( o.hasHopCount() ) setHopCount( o.hopCount );

	if ( hasBandwidth() ) {
		if ( o.hasBandwidth() ) bandwidth = min( bandwidth, o.bandwidth );
	} else if ( o.hasBandwidth() ) setBandwidth( o.bandwidth );

	if ( hasMinPathLatency() ) {
		if (o.hasMinPathLatency() ) minPathLatency += o.minPathLatency;
	} else if ( o.hasMinPathLatency() ) setMinPathLatency( o.minPathLatency );

	if ( hasMTU() ) {
		if ( o.hasMTU() ) MTU = min( MTU, o.MTU );
	} else if ( o.hasMTU() ) setMTU( o.MTU );
}

/*********************** AdSpecCLParameters ******************************/

inline AdSpecCLParameters::AdSpecCLParameters ( INetworkBuffer& buffer, uint16 length )
	: breakBit(false) {
	override.readFromBuffer( buffer, length );
}

inline ostream& operator<<( ostream& os, const AdSpecCLParameters& c ) {
	os << "CLS: ";
	if (c.breakBit) os << "BREAK ";
	os << c.override;
	return os;
}

inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const AdSpecCLParameters& c ) {
	buffer << c.override; return buffer;
}

inline void AdSpecCLParameters::updateBy( const AdSpecCLParameters& clp ) {
	breakBit = breakBit || clp.breakBit;
	override.updateBy( clp.override );
}

/*********************** AdSpecGSParameters ******************************/

inline AdSpecGSParameters::AdSpecGSParameters( INetworkBuffer& buffer, uint16 length )
	: breakBit(false) {
	Parameter parameter;
	buffer >> parameter >> Ctot >> parameter >> Dtot;
	buffer >> parameter >> Csum >> parameter >> Dsum;
	if ( length > size() ) {
		override.readFromBuffer( buffer, length - size() );
	}
}

inline ostream& operator<<( ostream& os, const AdSpecGSParameters& c ) {
	os << "GS: ";
	if (c.breakBit) os << "BREAK ";
	os << "Ctot: " << c.Ctot << " Dtot: " << c.Dtot << " Csum: " << c.Csum << " Dsum: " << c.Dsum << " " << c.override;
	return os;
}

inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const AdSpecGSParameters& c ) {
	buffer << Parameter(133,0,1) << c.Ctot << Parameter(134,0,1) << c.Dtot;
	buffer << Parameter(135,0,1) << c.Csum << Parameter(136,0,1) << c.Dsum;
	buffer << c.override;
	return buffer;
}

inline void AdSpecGSParameters::updateBy( const AdSpecGSParameters& gsp ) {
	breakBit = breakBit || gsp.breakBit;
	Ctot += gsp.Ctot;
	Dtot += gsp.Dtot;
	Csum += gsp.Csum;
	Dsum += gsp.Dsum;
	override.updateBy( gsp.override );
}

/*********************** FLOWSPEC_Object ******************************/
void FLOWSPEC_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len, uint8 C_Type){
	MessageHeader mheader;
	ServiceHeader sheader;
	Parameter parameter;

	switch (C_Type){
		case 2:  //Generalized FLowSpec
			buffer >> mheader >> sheader;
			serviceNumber = sheader.getNumber();
			length = bytesof( mheader.getLength() ) + MessageHeader::size();
			if ( serviceNumber != ServiceHeader::Default ) {
				buffer >> parameter >> static_cast<TSpec&>(*this);
			}
			if ( serviceNumber == ServiceHeader::Guaranteed ) {
				buffer >> parameter >> static_cast<RSpec&>(*this);
				// TODO: this is not really correct, but currently easier than rejecting the message
				if ( r > R ) R = r;
			}
			break;
		case 4: //SONET/SDH FLowSpec
			buffer >> static_cast<SONET_TSpec&>(*this);
			serviceNumber = SonetSDH_FlowSpec;
			length = SONET_TSpec::size();
			break;
		default: buffer.skip(len); return;
	}
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const FLOWSPEC_Object& o ) {
	switch( o.serviceNumber ) {
	case ServiceHeader::Guaranteed:
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::FLOWSPEC, 2 );
		buffer << MessageHeader(10) << ServiceHeader(o.serviceNumber,0,9);
		buffer << Parameter(127,0,5) << static_cast<const TSpec&>(o);
		buffer << Parameter(130,0,2) << static_cast<const RSpec&>(o);
		break;
	case ServiceHeader::ControlledLoad:
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::FLOWSPEC, 2 );
		buffer << MessageHeader(7) << ServiceHeader(o.serviceNumber,0,6);
		buffer << Parameter(127,0,5) << static_cast<const TSpec&>(o);
		break;
	case ServiceHeader::Default:
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::FLOWSPEC, 2 );
		buffer << MessageHeader(7) << ServiceHeader(o.serviceNumber,0,0);
		break;
	case FLOWSPEC_Object::SonetSDH_FlowSpec:
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::FLOWSPEC, 4 );
		buffer << static_cast<const SONET_TSpec&>(o);
		break;
	default:
		FATAL(2)( Log::Fatal, "FATAL INTERNAL ERROR: illegal service number: ", o.serviceNumber );
		abortProcess();
	}
	return buffer;
}

ostream& operator<< ( ostream& os, const FLOWSPEC_Object& o ) {
	os << "length:" << o.length;
	switch (o.serviceNumber) {
		case ServiceHeader::Guaranteed: os << " GS " << static_cast<const TSpec&>(o) << " " << static_cast<const RSpec&>(o); break;
		case ServiceHeader::ControlledLoad: os << " CLS " << static_cast<const TSpec&>(o); break;
		case FLOWSPEC_Object::SonetSDH_FlowSpec: os << " SonetSDH " << static_cast<const SONET_TSpec&>(o); break;
		default: os << " DS"; break;
	}
	return os;
}

/*********************** SENDER_TSPEC_Object ***************************/

void SENDER_TSPEC_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len, uint8 C_Type){
	MessageHeader mheader;
	ServiceHeader sheader;
	Parameter parameter;

	switch (C_Type){
		case 2:  //Generalized FLowSpec
			buffer >> mheader >> sheader >> parameter >> static_cast<TSpec&>(*this);
			length = bytesof( mheader.getLength() ) + MessageHeader::size();
			service = GMPLS_Sender_Tspec;
			break;
		case 4: //SONET/SDH FLowSpec
			buffer >> static_cast<SONET_TSpec&>(*this);
			length = SONET_TSpec::size();
			service = SonetSDH_Sender_Tspec;
			break;
		default: buffer.skip(len); return;
	}
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const SENDER_TSPEC_Object& o ) {
	
	switch( o.service ) {
		case SENDER_TSPEC_Object::GMPLS_Sender_Tspec: 
			buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SENDER_TSPEC, 2 );
			buffer << MessageHeader(7) << ServiceHeader(1,0,6);
			buffer << Parameter(127,0,5) << static_cast<const TSpec&>(o);
			break;
		case SENDER_TSPEC_Object::SonetSDH_Sender_Tspec: 	
			buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SENDER_TSPEC, 4 );
			buffer << static_cast<const SONET_TSpec&>(o);
			break;
		default : 
			FATAL(2)( Log::Fatal, "FATAL INTERNAL ERROR: unrecognized SENDER_TSPEC_Object: ", o.service );
			abortProcess();
	}
	return buffer;
}

ostream& operator<< ( ostream& os, const SENDER_TSPEC_Object& o ) {
	switch( o.service ) {
		case SENDER_TSPEC_Object::GMPLS_Sender_Tspec: 	os << " Generalized " << static_cast<const TSpec&>(o);	break;
		case SENDER_TSPEC_Object::SonetSDH_Sender_Tspec: 	os << " SonetSDH " << static_cast<const SONET_TSpec&>(o);	break;
		default : os << " Unknown "; break;
	}
	return os;
}

/*********************** ADSPEC_Object ******************************/

INetworkBuffer& operator>>( INetworkBuffer& buffer, ADSPEC_Object& m ) {
	MessageHeader mheader;
	ServiceHeader sheader;
	buffer >> mheader >> sheader >> m.gp;
	m.length = MessageHeader::size() + ServiceHeader::size() + AdSpecGeneralParameters::size();
	uint16 rest = bytesof(mheader.getLength()) - (ServiceHeader::size() + AdSpecGeneralParameters::size());
	while (rest > 0) {
		buffer >> sheader;
		rest -= bytesof(sheader.getLength()) + ServiceHeader::size();
		m.length += bytesof(sheader.getLength()) + ServiceHeader::size();
		switch (sheader.getNumber()) {
			case ServiceHeader::ControlledLoad:
				m.clp = new AdSpecCLParameters( buffer, bytesof(sheader.getLength()) );
				m.clp->setBreakBit( sheader.hasBreakBit() );
				break;
			case ServiceHeader::Guaranteed:
				m.gsp = new AdSpecGSParameters( buffer, bytesof(sheader.getLength()) );
				m.gsp->setBreakBit( sheader.hasBreakBit() );
				break;
			default:
				LOG(2)( Log::Error, "ERROR: wrong or unknown service header ", (uint16)sheader.getNumber() );
				m.length -= bytesof(sheader.getLength()) + ServiceHeader::size();
		}
	}
	if ( m.length != bytesof(mheader.getLength()) + MessageHeader::size() ) {
		LOG(4)( Log::Error, "ERROR: length in ADSPEC wrong", m.length, " != ", bytesof(mheader.getLength()) + MessageHeader::size() );
	}
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const ADSPEC_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::ADSPEC, 2 );
	buffer << MessageHeader(wordsof(o.length)-1) << ServiceHeader(1,0,8);
	buffer << o.gp;
	if ( o.clp ) {
		buffer << ServiceHeader(ServiceHeader::ControlledLoad, o.clp->hasBreakBit(), wordsof(o.clp->size())) << *o.clp;
	}
	if ( o.gsp ) {
		buffer << ServiceHeader(ServiceHeader::Guaranteed, o.gsp->hasBreakBit(), wordsof(o.gsp->size())) << *o.gsp;
	}
	return buffer;
}

ostream& operator<< ( ostream& os , const ADSPEC_Object& o ) {
	os << "length:" << o.length << " ";
	os << o.gp;
	if ( o.clp ) os << " " << *o.clp;
	if ( o.gsp ) os << " " << *o.gsp;
	return os;
}

void ADSPEC_Object::addCL( const AdSpecCLParameters& c ) {
	if ( clp ) {
		length -= clp->size();
		delete clp;
	} else {
		length += ServiceHeader::size();
	}
	clp = new AdSpecCLParameters(c);
	length += clp->size();
}

void ADSPEC_Object::addGS( const AdSpecGSParameters& g ) {
	if ( gsp ) {
		length -= gsp->size();
		delete gsp;
	} else {
		length += ServiceHeader::size();
	}
	gsp = new AdSpecGSParameters(g);
	length += gsp->size();
}

void ADSPEC_Object::updateBy( const ADSPEC_Object& adspec ) {

	gp.updateBy( adspec.gp );

	if ( clp ) {
		if ( adspec.clp ) {
			length -= clp->size();
			clp->updateBy( *adspec.clp );
			length += clp->size();
		}
	} else if ( adspec.clp ) addCL( *adspec.clp );

	if ( gsp ) {
		if ( adspec.gsp ) {
			length -= gsp->size();
			gsp->updateBy( *adspec.gsp );
			length += gsp->size();
		}
	} else if ( adspec.gsp ) addGS( *adspec.gsp );

}
