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
#ifndef _RSVP_SystemTypes_h_
#define _RSVP_SystemTypes_h_ 1

using namespace std;

#if !defined(NS2)
#include "SystemCallCheck.h"
#include "_rand_MT.h"
#endif
#include "RSVP_String.h"

#include <sys/types.h>                         // fd_set, etc.
#include <netinet/in.h>                        // htonl, htons, ntohl, ntohl
#include <arpa/inet.h>                         // inet_addr and inet_ntoa
#include <sys/time.h>                          // timeval
#include <time.h>                              // localtime
#include <errno.h>                             // errno
#include <string.h>                            // strerror
#include <stdlib.h>                            // strtol
#include <unistd.h>                            // sleep
#include <assert.h>                            // assert
#if !defined(NS2)
#include <netdb.h>                             // gethostbyname, struct hostent
#include <sys/socket.h>                        // AF_INET
#endif
#include <math.h>                              // HUGE_VAL, rint
#include <limits.h>                            // UINT_MAX

#if defined(SunOS)
#include <sys/ethernet.h>                      // ETHERTYPE_IP
#else
#include <net/ethernet.h>                      // ETHERTYPE_IP
#endif

class NetAddress;

// we need that for a certain platform (which one?)
extern "C" long int strtol(const char *nptr, char **endptr, int base);

#ifndef IPPROTO_RSVP
#define IPPROTO_RSVP 46
#endif

#define USECS_PER_SEC  1000000
#define MSECS_PER_SEC  1000
#define USECS_PER_MSEC 1000

// various data types;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef signed int sint32;
typedef signed long long sint64;

const sint32 sint32Infinite = INT_MAX;
const uint32 uint32Infinite = UINT_MAX;
const float ieee32floatInfinite = HUGE_VAL;
const uint32 bitsInChar = 8;

// socket handles
typedef int InterfaceHandle;
typedef int VifHandle;
typedef fd_set InterfaceHandleMask;

// Assumption: internal type 'float' is IEEE 32 floating number
// this is true for GNU's gcc
typedef float ieee32float;
typedef double ieee32float_p;

// internal represantation of time values
typedef timeval timerep;

#if defined(NS2)
// implemented in RSVP_Wrapper.cc
extern void getSimulatorTime( timerep& t );
extern int getRandomNumber();
extern int getCurrentNodeNumber();
extern int getNodeFromIface( const NetAddress& );
#endif

void initSystem();

extern inline int convertStringToInt( const String& s ) {
	return strtol( s.chars(), NULL, 10 );
}

extern inline ieee32float_p convertStringToFloat( const String& s ) {
	return strtod( s.chars(), NULL );
}

extern inline String convertIntToString( int x ) {
	String result;
	static const int divisor = 10;
	bool negative = (x<0);
	do {
		result = String( (char)(x % divisor) + '0' ) + result;
		x = x / divisor;
	} while ( x != 0 );
	if (negative) return String("-") + result;
	else return result;
}

extern inline void getCurrentSystemTime( timerep& t ) {
#if defined(NS2)
	getSimulatorTime( t );
#else
	CHECK( gettimeofday( &t, NULL ) );
#endif
                                     assert( t.tv_sec >= 0 && t.tv_usec >= 0 );
}

extern inline timerep getCurrentSystemTime() {
	timerep t;
	getCurrentSystemTime(t);
	return t;
}

extern inline void convertToLocalTime( uint32 seconds, uint32& hours, uint32& minutes ) {
#if defined(NS2)
	hours = (seconds / 3600) % 24;
	seconds %= 3600;
	minutes = seconds / 60;
	seconds %= 60;
#else
	time_t s = seconds;
	tm* ltime = localtime( &s );
	hours = ltime->tm_hour;
	minutes = ltime->tm_min;
#endif
}

// Real floating point (Decimal) <---> IEEE 32 bit floating point (Hex/Dec) conversion
// Only supports positive floating point value
extern inline uint32 floatMbitsToBytesInNetworkOrder(ieee32float x)
{
	/*			assert(x>0);
	uint32 result = 0;
	
	uint32 intPortion = floor(x);
	ieee32float fractPortion = x-intPortion;
	uint8 bitMantissa = 0;
	while (bitMantissa<23){
		uint32 bit = (fractPortion*2)>=1?1:0;
		fractPortion = (fractPortion*2)>=1?(fractPortion*2-1):fractPortion*2;
		result |= bit << (22-bitMantissa);
		bitMantissa++;
	}

	//We have int portion stored in the variable  intPortion, and Mantissa portion stored in the variable result
	//Now do a normalization
	uint32 move = 0;
	if (intPortion>=1){
		move = 0;
		while (intPortion/pow(2, ++move)>1);
	}
	else if (
	
	*/
	if (x==0.064)					return 0x45FA0000; //TSpec::R_DS0;
	else if (x==1.544) 			return 0x483C7A00; //TSpec::R_DS1;
	else if (x==2.048) 			return 0x487A0000; //TSpec::R_E1;
	else if (x==6.312) 			return 0x4940A080; //TSpec::R_DS2;
	else if (x==8.448) 			return 0x4980E800; //TSpec::R_E2;
	else if (x==10.00) 			return 0x49989680; //TSpec::R_Eth;
	else if (x==34.368) 			return 0x4A831A80; //TSpec::R_E3;
	else if (x==44.736) 			return 0x4AAAA780; //TSpec::R_DS3;
	else if (x==51.84) 			return 0x4AC5C100; //TSpec::R_STS1;
	else if (x==100.00) 			return 0x4B3EBC20; //TSpec::R_Fast_Eth;
	else if (x==200.00) 			return 0x4BBEBC20; //TSpec::R_200M_Eth;
	else if (x==300.00) 			return 0x4C0F0D18; //TSpec::R_300M_Eth;
	else if (x==400.00) 			return 0x4C3EBC20; //TSpec::R_400M_Eth;
	else if (x==500.00) 			return 0x4C6E6B28; //TSpec::R_500M_Eth;
	else if (x==600.00) 			return 0x4C8F0D18; //TSpec::R_600M_Eth;
	else if (x==700.00) 			return 0x4CA6E49C; //TSpec::R_700M_Eth;
	else if (x==800.00) 			return 0x4CBEBC20; //TSpec::R_800M_Eth;
	else if (x==900.00) 			return 0x4CD693A4; //TSpec::R_900M_Eth;
	else if (x==139.264) 			return 0x4B84D000; //TSpec::R_E4;
	else if (x==133) 				return 0x4B7DAD68; //TSpec::R_FC0_133M;
	else if (x==155.52) 			return 0x4B9450C0; //TSpec::R_OC3;
	else if (x==266) 				return 0x4BFDAD68; //TSpec::R_FC0_266M;
	else if (x==531) 				return 0x4C7D3356; //TSpec::R_FC0_531M;
	else if (x==622.08) 			return 0x4C9450C0; //TSpec::R_OC12;
	else if (x==1000.00) 			return 0x4CEE6B28; //TSpec::R_Gig_E;
	else if (x==2000.00) 			return 0x4D6E6B28; //TSpec::R_2Gig_E;
	else if (x==3000.00) 			return 0x4DB2D05E; //TSpec::R_3Gig_E;
	else if (x==4000.00) 			return 0x4DEE6B28; //TSpec::R_4Gig_E;
	else if (x==5000.00) 			return 0x4E1502F9; //TSpec::R_5Gig_E;
	else if (x==6000.00) 			return 0x4E32D05E; //TSpec::R_6Gig_E;
	else if (x==7000.00) 			return 0x4E509DC3; //TSpec::R_7Gig_E;
	else if (x==8000.00) 			return 0x4E6E6B28; //TSpec::R_8Gig_E;
	else if (x==8000.00) 			return 0x4E861C46; //TSpec::R_9Gig_E;
	else if (x==1062) 			return 0x4CFD3356; //TSpec::R_FC0_1062M;
	else if (x==1250)				return 0x4D1502F9; //TSpec::R_Gig_E_OverFiber;
	else if (x==1485)				return 0x4D31069A; //TSpec::R_HDTV;
	else if (x==2488.32) 			return 0x4D9450C0; //TSpec::R_OC48;
	else if (x==9953.28) 			return 0x4E9450C0; //TSpec::R_OC192;
	else if (x==10000.00) 			return 0x4E9502F9; //TSpec::R_10Gig_E;
	else if (x==39813.12) 			return 0x4F9450C0; //TSpec::R_OC768;
	else{ 
		//LOG(2)( Log::MPLS, "Data rate not supported :", x);
		return 0;
	}
	
}

extern inline ieee32float bytesInNetworkOrderToFloatMbits(uint32 x)
{
	if (x==0x45FA0000) 			return  0.064;  
	else if (x==0x483C7A00) 		return  1.544;   
	else if (x==0x487A0000) 		return  2.048;   
	else if (x==0x4940A080) 		return  6.312;   
	else if (x==0x4980E800) 		return  8.448;   
	else if (x==0x49989680) 		return  10.00;   
	else if (x==0x4A831A80) 		return  34.368;  
	else if (x==0x4AAAA780) 		return  44.736;  
	else if (x==0x4AC5C100) 		return  51.84;   
	else if (x==0x4B3EBC20) 		return  100.00;  
	else if (x==0x4BBEBC20) 		return  200.00;
	else if (x==0x4C0F0D18) 		return  300.00;
	else if (x==0x4C3EBC20) 		return  400.00;
	else if (x==0x4C6E6B28) 		return  500.00;
	else if (x==0x4C8F0D18) 		return  600.00;
	else if (x==0x4CA6E49C) 		return  700.00;
	else if (x==0x4CBEBC20) 		return  800.00;
	else if (x==0x4CD693A4) 		return  900.00;
	else if (x==0x4B84D000) 		return  139.264; 
	else if (x==0x4B7DAD68) 		return  133;     
	else if (x==0x4B9450C0) 		return  155.52;  
	else if (x==0x4BFDAD68) 		return  266;    
	else if (x==0x4C7D3356) 		return  531;     
	else if (x==0x4C9450C0) 		return  622.08;  
	else if (x==0x4CEE6B28) 		return  1000.00; 
	else if (x==0x4D6E6B28) 		return  2000.00; 
	else if (x==0x4DB2D05E) 		return  3000.00; 
	else if (x==0x4DEE6B28) 		return  4000.00; 
	else if (x==0x4E1502F9) 		return  5000.00; 
	else if (x==0x4E32D05E) 		return  6000.00; 
	else if (x==0x4E509DC3) 		return  7000.00; 
	else if (x==0x4E6E6B28) 		return  8000.00; 
	else if (x==0x4E861C46) 		return  9000.00; 
	else if (x==0x4CFD3356)		return  1062;    
	else if (x==0x4D1502F9) 		return  1250;
	else if (x==0x4D31069A)		return  1485;
	else if (x==0x4D9450C0) 		return  2488.32; 
	else if (x==0x4E9450C0) 		return  9953.28;
	else if (x==0x4E9502F9) 		return  10000.00;
	else if (x==0x4F9450C0) 		return  39813.12;
	else{ 
		//LOG(2)( Log::MPLS, "Data rate not supported :", x);
		return 0;
	}
}


extern inline void copyMemory( void* to, const void* from, unsigned int length ) {
	memcpy( (unsigned char*)to, (const unsigned char*)from, length );
}

extern inline void initMemoryWithZero( void* addr, int length ) {
	memset( (unsigned char*)addr, 0, length );
}

extern inline uint32 processId() {
	return getpid();
}

extern inline uint32 drawRandomNumber( uint32 maximum ) {
#if defined(NS2)
	return getRandomNumber() % (maximum+1);
#else
	return _uniform_int_( 0, maximum );
#endif
}

extern inline sint32 roundFloat( ieee32float x ) {
	return sint32(rint(x));
}

// address is returned in network format
bool convertStringToAddress( const char* s, uint32& address, bool = false );

// address must be given in network format
String convertAddressToString( const NetAddress& address );

// safe printout, e.g. during execution of a signal handler
void printSafe( const char*, ... );

void abortProcess();

timerep measureTimerResolution();

#if 0
// problems with profiling on FreeBSD 4.x
extern inline void* operator new( size_t size ) {
	return malloc(size);
}

extern inline void operator delete( void* pnt ) {
	free(pnt);
}
#endif

#endif /* _RSVP_SystemTypes_h_ */
