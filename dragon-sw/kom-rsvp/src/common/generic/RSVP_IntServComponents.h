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
#ifndef _RSVP_IntServComponents_h_
#define _RSVP_IntServComponents_h_ 1

#include "RSVP_BasicTypes.h"

#if __GNUC__ < 3
template <class T>
extern inline const T& min(const T& a, const T& b) {
	return b < a ? b : a;
}

template <class T>
extern inline const T& max(const T& a, const T& b) {
	return  a < b ? b : a;
}
#endif

/********************************* ErrorTerms *****************************/

class ErrorTerms {
	sint32 C;              // rate-dependent error in bytes
	sint32 D;              // rate-independent error in microsecs
	friend inline istream& operator>>( istream&, ErrorTerms& );
	friend inline ostream& operator<<( ostream&, const ErrorTerms& );
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, ErrorTerms& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const ErrorTerms& );
	friend class TSpec;
public:
	ErrorTerms( sint32 C = 0, sint32 D = 0 ) : C(C), D(D) {}
	ErrorTerms( INetworkBuffer& buf ) { buf >> *this; }
	static uint16 size() { return 8; }
	sint32 get_C() { return C; }
	sint32 get_D() { return D; }
};
extern inline istream& operator>>( istream& is, ErrorTerms& et ) {
	is >> et.C >> et.D; return is;
}
extern inline ostream& operator<<( ostream& os, const ErrorTerms& et ) {
	os << "C:" << et.C << " D:" << et.D; return os;
}
extern inline INetworkBuffer& operator>>( INetworkBuffer& buf, ErrorTerms& et ) {
	buf >> et.C >> et.D; return buf;
}
extern inline ONetworkBuffer& operator<<( ONetworkBuffer& buf, const ErrorTerms& et ) {
	buf << et.C << et.D; return buf;
}

#if defined(NS2)
class InstVar {
public:
	static double bw_atof(const char* s);
	static double time_atof(const char* s);
};
#endif

/********************************* TSpec *****************************/

class TSpec {
protected:
	uint32 d_r;               // Decimal Rate in bytes/sec
	uint32 d_b;               // Decimal Depth in bytes
	uint32 d_p;               // Decimal Peak in bytes/sec
	ieee32float r;               // Rate in bytes/sec
	ieee32float b;               // Depth in bytes
	ieee32float p;               // Peak in bytes/sec
	sint32 m;                    // minimum policed unit in bytes
	sint32 M;                    // MTU in bytes
	bool checkIntegrity() const { return p >= r; }
	bool checkDelay( const ErrorTerms& et, ieee32float_p Qd ) const { return Qd > et.D; }
	bool checkDelay( const ErrorTerms& et, ieee32float_p Qd, ieee32float_p R ) const {
		return Qd >= et.D + (et.C / R) * USECS_PER_SEC;
	}
	bool checkRate( ieee32float_p R ) const { return R >= r; }
	friend inline istream& operator>>( istream&, TSpec& );
	friend inline ostream& operator<<( ostream&, const TSpec& );
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, TSpec& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const TSpec& );
	friend inline bool operator== ( const TSpec&, const TSpec& );
	friend inline bool operator!= ( const TSpec&, const TSpec& );
	friend inline bool operator>= ( const TSpec&, const TSpec& );
	friend inline bool operator<= ( const TSpec&, const TSpec& );
	friend inline bool operator> ( const TSpec&, const TSpec& );
	friend inline bool operator< ( const TSpec&, const TSpec& );
public:
	//Bandwidth encoding in IEEE 32 floating point format. Refer to RFC-3473
	enum BwEnc{
			R_DS0 			= 0x45FA0000,
			R_DS1 			= 0x483C7A00,
			R_E1		 	= 0x487A0000,
			R_DS2 			= 0x4940A080,
			R_E2			= 0x4980E800,
			R_Eth			= 0x49989680,
			R_E3			= 0x4A831A80,
			R_DS3			= 0x4AAAA780,
			R_STS1			= 0x4AC5C100,
			R_Fast_Eth		= 0x4B3EBC20,
			R_200M_Eth		= 0x4BBEBC20,
			R_300M_Eth		= 0x4C0F0D18,
			R_400M_Eth		= 0x4C3EBC20,
			R_500M_Eth		= 0x4C6E6B28,
			R_600M_Eth		= 0x4C8F0D18,
			R_700M_Eth		= 0x4CA6E49C,
			R_800M_Eth		=0x4CBEBC20,
			R_900M_Eth		= 0x4CD693A4,
			R_E4			= 0x4B84D000,
			R_FC0_133M		= 0x4B7DAD68,
			R_OC3			= 0x4B9450C0,
			R_FC0_266M		= 0x4BFDAD68,
			R_FC0_531M		= 0x4C7D3356,
			R_OC12			= 0x4C9450C0,
			R_Gig_E			= 0x4CEE6B28,
			R_2Gig_E		= 0x4D6E6B28,
			R_3Gig_E		= 0x4DB2D05E,
			R_4Gig_E		= 0x4DEE6B28,
			R_5Gig_E		= 0x4E1502F9,
			R_6Gig_E		= 0x4E32D05E,
			R_7Gig_E		= 0x4E509DC3,
			R_8Gig_E		= 0x4E6E6B28,
			R_9Gig_E		= 0x4E861C46,
			R_Gig_E_OverFiber = 0x4D1502F9,  // Movaz specific value
			R_HDTV			= 0x4D31069A,	//Movz specific value
			R_FC0_1062M	= 0x4CFD3356,
			R_OC48			= 0x4D9450C0,
			R_OC192		= 0x4E9450C0,
			R_10Gig_E		= 0x4E9502F9,
			R_OC768		= 0x4F9450C0
	};

	TSpec( uint32 r, uint32 b, uint32 p,
			sint32 m = sint32Infinite, sint32 M = 0 ){
		d_r = r;	d_b = b; d_p = p; this->m = m; this->M = M;
		this->r = ieee32FloatToRealFloat(r);
		this->b = ieee32FloatToRealFloat(b);
		this->p = ieee32FloatToRealFloat(p);
	}

	TSpec( ieee32float r = 0, ieee32float b = 0, ieee32float p = 0,
			sint32 m = sint32Infinite, sint32 M = 0 ){
		this->r = r; this->b = b; this->p = p; this->m = m; this->M = M;
		this->d_r = realFloatToIEEE32Float(r);
		this->d_b = realFloatToIEEE32Float(b);
		this->d_p = realFloatToIEEE32Float(p);
	}
	TSpec( const TSpec& t ) : d_r(t.d_r), d_b(t.d_b), d_p(t.d_p), r(t.r), b(t.b) , p(t.p), m(t.m), M(t.M) {} 
	TSpec( INetworkBuffer& buf ) { buf >> *this; }
	ieee32float calculateRate( const ErrorTerms&, ieee32float_p ) const;
	ieee32float calculateDelay( const ErrorTerms&, ieee32float_p ) const;
	ieee32float calculateBuffer( const ErrorTerms&, ieee32float_p ) const;
	static ieee32float calculateErrorDelay( const ErrorTerms&, ieee32float_p );
	static ieee32float calculateErrorRate( const ErrorTerms&, ieee32float_p );
	static uint16 size() { return 20; }
	inline TSpec& merge( const TSpec& );
	inline TSpec& LUB( const TSpec& );
	inline TSpec& GLB( const TSpec& );
	inline TSpec& operator=( const TSpec& );
	inline TSpec& operator+=( const TSpec& );
	inline TSpec& operator-=( const TSpec& );
	ieee32float get_r() const { return r; }
	ieee32float get_b() const { return b; }
	ieee32float get_p() const { return p; }
	sint32 get_m() const { return m; }
	sint32 get_M() const { return M; }
	void set_r( ieee32float r ) { this->r = r; this->d_r = realFloatToIEEE32Float(r); }
	void set_r1(uint32 r ) { this->d_r = r; this->r = ieee32FloatToRealFloat(r); }
	void set_b( ieee32float b ) { this->b = b; this->d_b = realFloatToIEEE32Float(b); }
	void set_b1( uint32 b ) { this->d_b = b; this->b = ieee32FloatToRealFloat(b); }
	void set_p( ieee32float p ) { this->p = p; this->d_p = realFloatToIEEE32Float(p); }
	void set_p1( uint32 p ) { this->d_p = p; this->p = ieee32FloatToRealFloat(p); }
	void set_m( sint32 m ) { this->m = m; }
	void set_M( sint32 M ) { this->M = M; }
};
extern inline istream& operator>>( istream& is, TSpec& tb ) {
#if defined(NS2)
	String temp;
	is >> temp; tb.r = InstVar::bw_atof( temp.chars() ) / 8;
	is >> temp; tb.b = InstVar::bw_atof( temp.chars() ) / 8;
	is >> temp; tb.p = InstVar::bw_atof( temp.chars() ) / 8;
	is >> tb.m >> tb.M; 
#else
	is >> tb.r >> tb.b >> tb.p >> tb.m >> tb.M; 
#endif
	tb.d_r = realFloatToIEEE32Float(tb.r);
	tb.d_b = realFloatToIEEE32Float(tb.b);
	tb.d_p = realFloatToIEEE32Float(tb.p);
	return is;
}
extern inline ostream& operator<<( ostream& os, const TSpec& tb ) {
	os << "r:" << tb.r << " b:" << tb.b << " p:" << tb.p << " m:" << tb.m;
	os << " M:" << tb.M; return os;
}
extern inline INetworkBuffer& operator>>( INetworkBuffer& buf, TSpec& tb ) {
	buf >> tb.d_r >> tb.d_b >> tb.d_p >> tb.m >> tb.M; 
	tb.r = ieee32FloatToRealFloat(tb.d_r);
	tb.b = ieee32FloatToRealFloat(tb.d_b);
	tb.p = ieee32FloatToRealFloat(tb.d_p);
	return buf;
}
extern inline ONetworkBuffer& operator<<( ONetworkBuffer& buf, const TSpec& tb ) {
	buf << tb.d_r << tb.d_b << tb.d_p << tb.m << tb.M; 
	return buf;
}
extern inline TSpec& TSpec::operator= ( const TSpec& t ) {
	p=t.p; b=t.b; r=t.r; M=t.M; m=t.m; 
	d_p=t.d_p; d_b=t.d_b; d_r=t.d_r; return *this;
}
extern inline bool operator== ( const TSpec& t1, const TSpec& t2 ) {
	return (t1.d_p==t2.d_p) && (t1.d_b==t2.d_b) && (t1.d_r==t2.d_r) && (t1.M==t2.M) && (t1.m==t2.m);
}
extern inline bool operator!= ( const TSpec& t1, const TSpec& t2 ) {
	return (t1.d_p!=t2.d_p) || (t1.d_b!=t2.d_b) || (t1.d_r!=t2.d_r) || (t1.M!=t2.M) || (t1.m!=t2.m);
}
extern inline bool operator>= ( const TSpec& t1, const TSpec& t2 ) {
	return (t1.p>=t2.p) && (t1.b>=t2.b) && (t1.r>=t2.r) && (t1.M>=t2.M) && (t1.m<=t2.m);
}
extern inline bool operator<= ( const TSpec& t1, const TSpec& t2 ) {
	return (t1.p<=t2.p) && (t1.b<=t2.b) && (t1.r<=t2.r) && (t1.M<=t2.M) && (t1.m>=t2.m);
}
extern inline bool operator> ( const TSpec& t1, const TSpec& t2 ) {
	return (t1.p>=t2.p) && (t1.b>=t2.b) && (t1.r>t2.r) && (t1.M>=t2.M) && (t1.m<=t2.m);
}
extern inline bool operator< ( const TSpec& t1, const TSpec& t2 ) {
	return (t1.p<=t2.p) && (t1.b<=t2.b) && (t1.r<t2.r) && (t1.M<=t2.M) && (t1.m>=t2.m);
}
extern inline TSpec& TSpec::operator+=( const TSpec& tb ) {
	p += tb.p; b += tb.b; r += tb.r; M = max(M,tb.M); m = min(m,tb.m);
	d_p = realFloatToIEEE32Float(p); d_b = realFloatToIEEE32Float(b); d_r = realFloatToIEEE32Float(r);
	return *this;
}
extern inline TSpec& TSpec::operator-=( const TSpec& tb ) {
	p -= tb.p; b -= tb.b; r -= tb.r; M = max(M,tb.M); m = min(m,tb.m);
	d_p = realFloatToIEEE32Float(p); d_b = realFloatToIEEE32Float(b); d_r = realFloatToIEEE32Float(r);
	return *this;
}
extern inline TSpec& TSpec::merge ( const TSpec& tb ) {
	p = max(p,tb.p); b = max(b,tb.b); r = max(r,tb.r); M = min(M,tb.M); m = min(m,tb.m);
	d_p = realFloatToIEEE32Float(p); d_b = realFloatToIEEE32Float(b); d_r = realFloatToIEEE32Float(r);
	return *this;
}
extern inline TSpec& TSpec::LUB ( const TSpec& tb ) {
	p = max(p,tb.p); b = max(b,tb.b); r = max(r,tb.r); M = max(M,tb.M); m = min(m,tb.m);
	d_p = realFloatToIEEE32Float(p); d_b = realFloatToIEEE32Float(b); d_r = realFloatToIEEE32Float(r);
	return *this;
}
extern inline TSpec& TSpec::GLB ( const TSpec& tb ) {
	p = min(p,tb.p); b = min(b,tb.b); r = min(r,tb.r); M = min(M,tb.M); m = max(m,tb.m);
	d_p = realFloatToIEEE32Float(p); d_b = realFloatToIEEE32Float(b); d_r = realFloatToIEEE32Float(r);
	return *this;
}
extern inline TSpec operator+ ( const TSpec& tb1, const TSpec& tb2 ) {
	TSpec tb = tb1; tb += tb2; return tb;
}
extern inline TSpec operator- ( const TSpec& tb1, const TSpec& tb2 ) {
	TSpec tb = tb1; tb -= tb2; return tb;
}
extern inline bool ordered( const TSpec& tb1, const TSpec& tb2 ) {
	return tb1 <= tb2 || tb2 >= tb1;
}

/********************************* RSpec *****************************/

class RSpec {
protected:
	uint32 d_R;
	ieee32float R;
	sint32 S;
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, RSpec& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const RSpec& );
	friend inline ostream& operator<<( ostream&, const RSpec& );
	friend inline istream& operator>>( istream&, RSpec& );
	friend inline bool operator== ( const RSpec&, const RSpec& );
	friend inline bool operator!= ( const RSpec&, const RSpec& );
	friend inline bool operator>= ( const RSpec&, const RSpec& );
	friend inline bool operator<= ( const RSpec&, const RSpec& );
	friend inline bool operator> ( const RSpec&, const RSpec& );
	friend inline bool operator< ( const RSpec&, const RSpec& );
public:
	RSpec( uint32 R, sint32 S = 0 ) {
		d_R = R; this->S = S;		
		this->R = ieee32FloatToRealFloat(R); 
	}
	RSpec( ieee32float R = 0, sint32 S = 0 ){ 
		this->R = R; this->S = S;
		d_R = realFloatToIEEE32Float(R); 
	}
	RSpec( INetworkBuffer& buf ) { buf >> *this; }
	RSpec( const  RSpec& r ) : d_R(r.d_R), R(r.R), S(r.S){}
	RSpec& operator= ( const  RSpec& r ) { R = r.R; S = r.S; d_R = r.d_R; return *this; }
	static uint16 size() { return 8; }
	inline RSpec& merge( const RSpec& );
	inline RSpec& LUB( const RSpec& r ) { return merge(r); }
	inline RSpec& GLB( const RSpec& r );
	ieee32float get_R() const { return R; }
	sint32 get_S() const { return S; }
	void set_R( ieee32float R ) { this->R = R; this->d_R = realFloatToIEEE32Float(R); }
	void set_R1( uint32 R ) { this->d_R = R; this->R = ieee32FloatToRealFloat(R); }
	void set_S( sint32 S ) { this->S = S; }
};
extern inline istream& operator>>( istream& is, RSpec& rs ) {
#if defined(NS2)
	String temp;
	is >> temp; rs.R = InstVar::bw_atof( temp.chars() ) / 8;
	is >> temp; rs.S = (sint32) InstVar::time_atof( temp.chars() ) * 1000000;
#else
	is >> rs.R >> rs.S; 
#endif
	rs.d_R = realFloatToIEEE32Float(rs.R); return is;
}
extern inline ostream& operator<<( ostream& os, const RSpec& rs ) {
	os << "R:" << rs.R << " S:" << rs.S; return os;
}
extern inline INetworkBuffer& operator>>( INetworkBuffer& buf, RSpec& rs ) {
	buf >> rs.d_R >> rs.S; 
	rs.R = ieee32FloatToRealFloat(rs.d_R);
	return buf;
}
extern inline ONetworkBuffer& operator<<( ONetworkBuffer& buf, const RSpec& rs ) {
	buf << rs.d_R << rs.S; return buf;
}
extern inline bool operator== ( const RSpec& rs1, const RSpec& rs2 ) {
	return ( rs1.d_R == rs2.d_R ) && ( rs1.S == rs2.S );
}
extern inline bool operator!= ( const RSpec& rs1, const RSpec& rs2 ) {
	return ( rs1.d_R != rs2.d_R ) || ( rs1.S != rs2.S );
}
extern inline bool operator>= ( const RSpec& rs1, const RSpec& rs2 ) {
	return ( rs1.R >= rs2.R ) && ( rs1.S <= rs2.S );
}
extern inline bool operator<= ( const RSpec& rs1, const RSpec& rs2 ) {
	return ( rs1.R <= rs2.R ) && ( rs1.S >= rs2.S );
}
extern inline bool operator> ( const RSpec& rs1, const RSpec& rs2 ) {
	return ( rs1.R > rs2.R ) && ( rs1.S <= rs2.S );
}
extern inline bool operator< ( const RSpec& rs1, const RSpec& rs2 ) {
	return ( rs1.R < rs2.R ) && ( rs1.S >= rs2.S );
}
extern inline RSpec& RSpec::merge ( const RSpec& rs ) {
	R = max(R,rs.R); S = min(S,rs.S); 
	d_R = realFloatToIEEE32Float(rs.R);
	return *this;
}
extern inline RSpec& RSpec::GLB ( const RSpec& rs ) {
	R = min(R,rs.R); S = max(S,rs.S); 
	d_R = realFloatToIEEE32Float(rs.R);
	return *this;
}
extern inline bool ordered( const RSpec& rs1, const RSpec& rs2 ) {
	return rs1 <= rs2 || rs1 >= rs2;
}

/********************************* SONET_TSpec *****************************/
/********************draft-ietf-ccamp-gmpls-sonet-sdh-08.txt********************/
class SONET_TSpec {
protected:
	uint8 signalType;  //Signal type
	uint8 RCC;  		//Requested Contiguous Concatenation
	uint16 NCC; 		//Number of Contiguous Components
	uint16 NVC;		//Number of Virtual Components
	uint16 MT;		//Multiplier
	uint32 T; 		//Transparency
	uint32 P;			//Profile
	
	//friend inline istream& operator>>( istream&, TSpec& );
	friend inline ostream& operator<<( ostream&, const SONET_TSpec& );
	friend inline INetworkBuffer& operator>>( INetworkBuffer&, SONET_TSpec& );
	friend inline ONetworkBuffer& operator<<( ONetworkBuffer&, const SONET_TSpec& );
	friend inline bool operator== ( const SONET_TSpec&, const SONET_TSpec& );
	friend inline bool operator!= ( const SONET_TSpec&, const SONET_TSpec& );
	friend inline bool operator>= ( const SONET_TSpec&, const SONET_TSpec& );
	friend inline bool operator<= ( const SONET_TSpec&, const SONET_TSpec& );
	friend inline bool operator> ( const SONET_TSpec&, const SONET_TSpec& );
	friend inline bool operator< ( const SONET_TSpec&, const SONET_TSpec& );
public:
	enum SignalType{
			S_VT15SPE_VC11 	= 1,
			S_VT2SPE_VC12		= 2, 
			S_VT3SPE			= 3,
			S_VT6SPE_VC2		= 4,
			S_STS1SPE_VC3		= 5,
			S_STS3CSPE_VC4	= 6,
			S_STS1_STM0		= 7,
			S_STS3_STM1		= 8,
			S_STS12_STM4		= 9,
			S_STS48_STM16		= 10,
			S_STS192_STM64	= 11,
			S_STS768_STM256	= 12
	};

	SONET_TSpec() : signalType(0), RCC(0), NCC(0), NVC(0), MT(0), T(0), P(0){}
	SONET_TSpec( uint8 st, uint8 rcc, uint16 ncc, uint16 nvc, uint16 mt, uint32 t, uint32 p):
		signalType(st), RCC(rcc), NCC(ncc), NVC(nvc), MT(mt), T(t), P(p){}

	SONET_TSpec( const SONET_TSpec& t ) : signalType(t.signalType), RCC(t.RCC), NCC(t.NCC), NVC(t.NVC), MT(t.MT) , T(t.T), P(t.P){} 
	SONET_TSpec( INetworkBuffer& buf ) { buf >> *this; }
	static uint16 size() { return 16; }
	inline SONET_TSpec& operator=( const SONET_TSpec& );

	uint8 getSignalType() const { return signalType; }
	uint8 getRCC() const { return RCC; }
	uint16 getNCC() const { return NCC; }
	uint16 getNVC() const { return NVC; }
	uint16 getMT() const { return MT; }
	uint32 getTransparency() const { return T; }
	uint32 getProfile() const { return P; }

	void setSignalType(uint8 st) { signalType = st; }
	void setRCC(uint8 rcc) { RCC = rcc; }
	void setNCC(uint16 ncc) { NCC = ncc; }
	void setNVC(uint16 nvc) { NVC = nvc; }
	void setMT(uint16 mt) { MT = mt; }
	void setTransarency(uint32 tr) { T = tr; }
	void setProfile(uint32 pr) { P = pr; }

	inline SONET_TSpec& merge( const SONET_TSpec& );
	inline SONET_TSpec& LUB( const SONET_TSpec& );
	inline SONET_TSpec& GLB( const SONET_TSpec& );
	
};
extern inline ostream& operator<<( ostream& os, const SONET_TSpec& tb ) {
	os << "SignalType:" << (uint32)tb.signalType<< " RCC:" << (uint32)tb.RCC << " NCC:" << (uint32)tb.NCC << " NVC:" << (uint32)tb.NVC;
	os << " MT:" << (uint32)tb.MT << " Transparency:" << tb.T << " Profile:" << tb.P; return os;
}
extern inline INetworkBuffer& operator>>( INetworkBuffer& buf, SONET_TSpec& tb ) {
	buf >> tb.signalType >> tb.RCC >> tb.NCC >> tb.NVC >> tb.MT >> tb.T >> tb.P;  
	return buf;
}
extern inline ONetworkBuffer& operator<<( ONetworkBuffer& buf, const SONET_TSpec& tb ) {
	buf << tb.signalType << tb.RCC << tb.NCC << tb.NVC << tb.MT << tb.T << tb.P;  
	return buf;
}
extern inline SONET_TSpec& SONET_TSpec::operator= ( const SONET_TSpec& t ) {
	signalType=t.signalType; RCC=t.RCC; NCC=t.NCC; NVC=t.NVC; MT=t.MT; T=t.T; P=t.P; 
	return *this;
}
extern inline SONET_TSpec& SONET_TSpec::merge ( const SONET_TSpec& tb ) {
	signalType = max(signalType,tb.signalType); RCC = max(RCC,tb.RCC); NCC = max(NCC,tb.NCC); NVC = max(NVC,tb.NVC); MT = max(MT,tb.MT);
	T = max(T, tb.T); P = max(P, tb.P);
	return *this;
}
extern inline SONET_TSpec& SONET_TSpec::LUB ( const SONET_TSpec& tb ) {
	signalType = max(signalType,tb.signalType); RCC = max(RCC,tb.RCC); NCC = max(NCC,tb.NCC); NVC = max(NVC,tb.NVC); MT = max(MT,tb.MT);
	T = max(T, tb.T); P = max(P, tb.P);
	return *this;
}
extern inline SONET_TSpec& SONET_TSpec::GLB ( const SONET_TSpec& tb ) {
	signalType = min(signalType,tb.signalType); RCC = min(RCC,tb.RCC); NCC = min(NCC,tb.NCC); NVC = min(NVC,tb.NVC); MT = min(MT,tb.MT);
	T = min(T, tb.T); P = min(P, tb.P);
	return *this;
}

#define SONET_TS_COMPARE(OP) \
extern inline bool operator OP( const SONET_TSpec& t1, const SONET_TSpec& t2 ) { \
	return (t1.signalType OP t2.signalType) && (t1.RCC OP t2.RCC) && (t1.NCC OP t2.NCC) && (t1.NVC OP t2.NVC) && (t1.MT OP t2.MT) \
		     && (t1.T OP t2.T) && (t1.P OP t2.P); \
}

SONET_TS_COMPARE(==)
SONET_TS_COMPARE(!=)	
SONET_TS_COMPARE(>=)
SONET_TS_COMPARE(<=)
SONET_TS_COMPARE(>)
SONET_TS_COMPARE(<)

#endif /* _RSVP_IntServComponents_h_ */
