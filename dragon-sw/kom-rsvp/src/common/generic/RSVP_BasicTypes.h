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
#ifndef _RSVP_BasicTypes_h_
#define _RSVP_BasicTypes_h_ 1

#include "RSVP_GeneralMemoryMachine.h"
#include "RSVP_Helper.h"
#include "RSVP_System.h"
#include "RSVP_String.h"

#include <iostream>                           // C++ I/O library
#include <fstream>                           // C++ I/O library

class INetworkBuffer;
class ONetworkBuffer;
class PacketHeader;

// helper functions

template <class T>
inline bool pointerContentsEqual( const T* p1, const T* p2 ) {
	return (p1 == (T*)0 || p2 == (T*)0) ? (p1 == p2) : (*p1 == *p2);
}

extern inline uint32 wordsof( uint32 x ) {
	return (x+3) / 4;
}
extern inline uint32 bytesof( uint32 x ) {
	return x * 4;
}

// reservation style type
enum FilterStyle { None = 0, FF = 10, WF = 17, SE = 18 };

// only for IPv4, extensions are needed for IPv6
class NetAddress {
	uint32 address;                                     // network format
	friend inline ostream& operator<< ( ostream&, const NetAddress& );
	friend inline istream& operator>> ( istream&, NetAddress& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, NetAddress& );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const NetAddress& );
	friend inline NetAddress operator& ( const NetAddress&, const NetAddress& );
	friend inline NetAddress operator| ( const NetAddress&, const NetAddress& );
	friend inline NetAddress operator^ ( const NetAddress&, const NetAddress& );
	friend inline NetAddress operator~ ( const NetAddress& );
	DECLARE_ORDER(NetAddress)
public:
	NetAddress( uint32 address = 0 ) : address(address) {}
	NetAddress( const String& addrString ) { readFromString( addrString ); }
	void readFromString( const String& addrString ) {
		convertStringToAddress( addrString.chars(), address );
	}
	operator bool() const { return (address != 0); }
	bool isMulticast() const { return ((ntohl(address) >> 28) == 14); };
	NetAddress& operator= ( const NetAddress& n ) { address = n.address; return *this; }
	uint32 rawAddress() const { return address; }
	uint32 getHashValue( uint32 hashSize ) const { return ntohl(address) % hashSize; }
	static uint16 size() { return 4; }
	static bool tryString( const String& addrString ) { uint32 dummy; return convertStringToAddress( addrString.chars(), dummy, true ); }
};
IMPLEMENT_ORDER1(NetAddress,address)
extern inline NetAddress operator& ( const NetAddress& n1, const NetAddress& n2 ) {
	return NetAddress(n1.address & n2.address);
}
extern inline NetAddress operator^ ( const NetAddress& n1, const NetAddress& n2 ) {
	return NetAddress(n1.address ^ n2.address);
}
extern inline NetAddress operator| ( const NetAddress& n1, const NetAddress& n2 ) {
	return NetAddress(n1.address | n2.address);
}
extern inline NetAddress operator~ ( const NetAddress& n1 ) {
	return NetAddress(~n1.address);
}

// base class Buffer is used to handle unknown objects.
// see class UNKNOWN_Object in RSVP_ProtocolObjects.{h,cc} for details
class Buffer {
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const Buffer& );
	Buffer( const Buffer& );
	Buffer& operator= ( const Buffer& );
public:
#if defined(RSVP_MEMORY_MACHINE)
	struct BufferNode128 {
		uint8 buf[128];
		static const char* const name;
	};
	static GeneralMemoryMachine<BufferNode128> buf128memMachine;
#endif
protected:
	uint8* buffer;
	uint16 size;
	enum { fromHeap, fromPool, unknown } status;
public:
	Buffer() : buffer(NULL), size(0) {}
	Buffer( uint8* packet, uint16 size )
		: buffer(packet), size(size), status(unknown) {}
#if defined(RSVP_MEMORY_MACHINE)
	Buffer( uint16 s ) {
		if ( s <= 128 ) {
			buffer = (uint8*)buf128memMachine.alloc( 128 );
			size = 128;
			status = fromPool;
		} else {
			buffer = new uint8[s];
			size = s;
			status = fromHeap;
		}
	}
#else
	Buffer( uint16 size ) : buffer(new uint8[size]), size(size), status(fromHeap) {}
#endif
	~Buffer() {
#if defined(RSVP_MEMORY_MACHINE)
		if ( status == fromPool ) {
			buf128memMachine.dealloc( buffer );
		} else
#endif
		if ( status == fromHeap ) {
			delete [] buffer;
		}
	}
	void cloneFrom( const uint8* packet, uint16 length ) {
                                                      assert( length <= size );
		copyMemory( buffer, packet, length );
	}
	void dump( ostream&, uint16 ) const;
	const uint8* getContents() const { return buffer; }
	uint16 getSize() const { return size; }
};

class NetworkBuffer : public Buffer {
private:
	NetworkBuffer() : checkSumStart(NULL), ptr(NULL) {}
	uint8* checkSumStart;
protected:
	uint8* ptr;
	NetworkBuffer( uint8* packet, uint16 size )
		: Buffer(packet,size), checkSumStart(buffer), ptr(buffer) {}
	NetworkBuffer( uint16 size ) : Buffer(size), checkSumStart(buffer), ptr(buffer) {}
public:
	void setChecksumStart() { checkSumStart = ptr; }
	uint16 calculateChecksumRSVP( uint16 ) const;
	void setChecksumRSVP( uint16 length ) {
		*(uint16*)(checkSumStart + 2) = calculateChecksumRSVP(length);
	}
	bool checkCheckSumRSVP( uint16 length ) {
		uint16 checkSum = *(uint16*)(checkSumStart + 2);
		if ( checkSum != 0 ) {
			*(uint16*)(checkSumStart + 2) = 0;
			return checkSum == calculateChecksumRSVP( length );
		}
		return true;
	}
	void init() { ptr = buffer; }
};

extern inline INetworkBuffer& operator>> ( INetworkBuffer&, PacketHeader& );
class INetworkBuffer : public NetworkBuffer {
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, ieee32float& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, char& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, uint8& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, uint16& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, uint32& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, sint32& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, PacketHeader& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, NetAddress& );
	friend inline ostream& operator<< ( ostream&, const INetworkBuffer& );
	uint8* end;
public:
	INetworkBuffer( uint16 size ) : NetworkBuffer(size), end(buffer) {}
	uint8* getWriteBuffer() { return buffer; }
	void setWriteLength( uint16 length ) { end = buffer + length; }
	void init() { NetworkBuffer::init(); end = buffer; }
	void skip( uint16 count ) { ptr += count; }
	uint16 getRemainingSize() const { return end - ptr; }
	uint8* getCurrentPosition() const { return ptr; }
	void cloneFrom( const uint8* packet, uint16 length ) {
		Buffer::cloneFrom( packet, length );
		ptr = buffer;
		end = buffer + length;
	}
};

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const PacketHeader& );
class ONetworkBuffer : public NetworkBuffer {
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const ieee32float );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const char );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const uint8 );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const uint16 );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const uint32 );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const sint32 );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const PacketHeader& );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const NetAddress& );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const Buffer& );
	friend inline ostream& operator<< ( ostream&, const ONetworkBuffer& );
public:
	ONetworkBuffer( uint16 size ) : NetworkBuffer(size) {}
	uint16 getUsedSize() const { return ptr-buffer; }
	DECLARE_MEMORY_MACHINE_IN_CLASS(ONetworkBuffer)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(ONetworkBuffer, obufMemMachine)

/************************ NetworkBuffer, etc *****************************/

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const char data ) {
																	assert( buf.size >= buf.getUsedSize() + sizeof(data) );
	*buf.ptr = (uint8)data;
	buf.ptr += sizeof(data);
	return buf;
}

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const uint8 data ) {
																	assert( buf.size >= buf.getUsedSize() + sizeof(data) );
	*buf.ptr = data;
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const uint16 data ) {
																	assert( buf.size >= buf.getUsedSize() + sizeof(data) );
	*(uint16*)buf.ptr = htons( data );
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const uint32 data ) {
																	assert( buf.size >= buf.getUsedSize() + sizeof(data) );
	*(uint32*)buf.ptr = htonl( data );
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const sint32 data ) {
																	assert( buf.size >= buf.getUsedSize() + sizeof(data) );
	*(uint32*)buf.ptr = htonl( data );
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const ieee32float data ) {
																	assert( buf.size >= buf.getUsedSize() + sizeof(ieee32float) );
	*(uint32*)buf.ptr = htonl(*(uint32*)&data);
	buf.ptr += sizeof(data);
	return buf;
}

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const Buffer& buffer ) {
																	assert( buf.size >= buf.getUsedSize() + buffer.size );
	copyMemory( buf.ptr, buffer.buffer, buffer.size );
	buf.ptr += buffer.size;
	return buf;
}

extern inline ostream& operator<< ( ostream& os, const ONetworkBuffer& b ) {
	b.dump( os, b.getUsedSize() ); return os;
}

extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, char& data ) {
																	assert( buf.getRemainingSize() >= sizeof(data) );
	data = (char)*buf.ptr;
	buf.ptr += sizeof(data);
	return buf;
}

extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, uint8& data ) {
																	assert( buf.getRemainingSize() >= sizeof(data) );
	data = *buf.ptr;
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, uint16& data ) {
																	assert( buf.getRemainingSize() >= sizeof(data) );
	data = ntohs( *(uint16*)buf.ptr );
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, uint32& data ) {
																	assert( buf.getRemainingSize() >= sizeof(data) );
	data = ntohl( *(uint32*)buf.ptr );
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, sint32& data ) {
																	assert( buf.getRemainingSize() >= sizeof(data) );
	data = ntohl( *(uint32*)buf.ptr );
	buf.ptr += sizeof(data);
	return buf;
}
	
extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, ieee32float& data ) {
																	assert( buf.getRemainingSize() >= sizeof(data) );
	uint32 dummy = ntohl(*(uint32*)buf.ptr);
	data = *(ieee32float*)&dummy;
	buf.ptr += sizeof(data);
	return buf;
}

extern inline ostream& operator<< ( ostream& os, const INetworkBuffer& b ) {
	b.dump( os, b.end - b.buffer ); return os;
}

/*************************** NetAddress ********************************/

extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, NetAddress& addr ) {
																	assert( buf.getRemainingSize() >= sizeof(uint32) );
	addr.address = *(uint32*)buf.ptr;
	buf.ptr += NetAddress::size();
	return buf;
}

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const NetAddress& addr ) {
																	assert( buf.size >= buf.getUsedSize() + sizeof(uint32) );
	*(uint32*)buf.ptr = addr.address;
	buf.ptr += NetAddress::size();
	return buf;
}

extern inline ostream& operator<< ( ostream& os, const NetAddress& netAddress ) {
	os << convertAddressToString(netAddress);
#if defined(NS2)
	os << "(node " << getNodeFromIface( netAddress ) << ")";
#endif
	return os;
}

extern inline istream& operator>> ( istream& is, NetAddress& netAddress ) {
	String addrString;
	is >> addrString;
	if ( !addrString.empty() ) {
		netAddress.readFromString( addrString );
	}
	return is;
}

#endif /* _RSVP_BasicTypes_h_*/
