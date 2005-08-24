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
#ifndef _RSVP_PacketHeader_h_
#define _RSVP_PacketHeader_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_Log.h"

// struct modified from /usr/include/netinet/ip.h from Linux 2.0 kernel
struct __rsvp_ip4_header {
#if defined(WORDS_BIGENDIAN)
	uint8 ip_v:4;				/* version */
	uint8 ip_hl:4;			/* header length */
#else
	uint8 ip_hl:4;			/* header length */
	uint8 ip_v:4;				/* version */
#endif
	uint8 ip_tos;				/* type of service */
	uint16 ip_len;			/* total length */
	uint16 ip_id;				/* identification */
	uint16 ip_off;			/* fragment offset field */
	uint8 ip_ttl;				/* time to live */
	uint8 ip_p;					/* protocol */
	uint16 ip_sum;			/* checksum */
	uint32 ip_src;			/* source address */
	uint32 ip_dst;			/* destination address */
};	

class PacketHeader : private __rsvp_ip4_header {
	char options[4];
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, PacketHeader& );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const PacketHeader& );
	friend inline ostream& operator<< ( ostream&, const PacketHeader& );
public:
	inline PacketHeader() { init(); }
	inline uint16 outputSize() const;
	inline static uint16 maxOutputSize();
	inline void init();
	inline void setFurtherInfo( uint16 length, uint8 ttl, bool );
	uint8 getHeaderLength() const { return bytesof(ip_hl); }
	uint8 getTTL() const { return ip_ttl; }
	void decrementTTL() { ip_ttl -= 1; }
	NetAddress getSrcAddress() const { return ip_src; }
	void setSrcAddress( const NetAddress &addr ) { ip_src = addr.rawAddress(); }
	NetAddress getDestAddress() const { return ip_dst; }
	void setDestAddress( const NetAddress &addr ) { ip_dst = addr.rawAddress(); }
};

// Setting the IP options by including them in supplied header doesn't work
// on Solaris. Therefore, they are excluded here and set using 'setsockopt'
inline uint16 PacketHeader::outputSize() const {
#if defined(NEED_RA_SOCKOPT)
	return sizeof(__rsvp_ip4_header);
#else
	return (options[0] == 0) ? sizeof(__rsvp_ip4_header) : sizeof(PacketHeader);
#endif
}

inline uint16 PacketHeader::maxOutputSize() {
	return sizeof(PacketHeader);
}

inline void PacketHeader::init() {
	ip_v = 4;
	ip_hl = 5;
	ip_tos = 0;
	ip_len = 0;
	ip_id = 0;
	ip_off = 0;
	ip_ttl = 0;
	ip_p = IPPROTO_RSVP;
	ip_sum = 0;
	ip_src = 0;
	ip_dst = 0;
}

inline void PacketHeader::setFurtherInfo( uint16 length, uint8 ttl, bool routerAlert ) {
	*(uint32*)options = 0;
#if !defined(NEED_RA_SOCKOPT)
	if ( routerAlert ) {
		options[0] = 148;
		options[1] = 4;
	}
#endif
	LOG(2)( Log::Packet, "setting length in header field to", length + outputSize() );
#if defined(HTONS_IP_HEADER)
	ip_len = htons(length + outputSize());
#else
	ip_len = length + outputSize();
#endif
	ip_hl = wordsof(outputSize());
	ip_ttl = ttl;
}

inline INetworkBuffer& operator>> ( INetworkBuffer& buf, PacketHeader& header ) {
																	assert( buf.getRemainingSize() >= sizeof(__rsvp_ip4_header) );
	copyMemory( &header, buf.buffer, sizeof(__rsvp_ip4_header) );
	buf.ptr += sizeof(__rsvp_ip4_header);
	if ( (unsigned)bytesof(header.ip_hl) > sizeof(__rsvp_ip4_header) ) {
																	assert( buf.getRemainingSize() >= sizeof(header.options) );
		*(uint32*)&header.options = *(uint32*)buf.buffer;
		buf.ptr += sizeof(header.options);
	} else {
		*(uint32*)&header.options = 0;
	}
	return buf;
}

inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const PacketHeader& header ) {
																	assert( buf.size >= buf.getUsedSize() + header.outputSize() );
	copyMemory( buf.buffer, &header, header.outputSize() );
	buf.ptr += header.outputSize();
	return buf;
}

inline ostream& operator<< ( ostream& os, const PacketHeader& header ) {
	os << "HeadLen: " << (uint16)header.ip_hl << " " << (uint16)header.ip_v << " " << header.ip_tos << " Len:  "
		<< header.ip_len << " " << header.ip_id << " Off:  " << header.ip_off << " TTL: "
		<< (uint16)header.ip_ttl << " " << (uint16)header.ip_p << " " << header.ip_sum << " SRC: "
		<< NetAddress(header.ip_src) << " DST:" << NetAddress(header.ip_dst);
	return os;
}

#endif /* _RSVP_PacketHeader_h_ */
