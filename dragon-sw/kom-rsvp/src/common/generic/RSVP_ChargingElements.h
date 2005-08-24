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
/***************************************************************************
 *  RSVP_ChargingElements.h                                                *
 *                                                                         *      
 *  Declaration of Policy elements for charging whitin a policy message    *      
 *  according to internet draft draft-ietf-rap-user-identity-00.txt and    *                       
 *  An embedded charging approach for RSVP                                 *      
 *                                                                         *      
 *  Author: Joe Betz  (Joachim.Betz@KOM.tu-darmstadt.de)                   *      
 *                                                                         *      
 ***************************************************************************/

#ifndef _RSVP_ChargingElements_h_
#define _RSVP_ChargingElements_h_ 1

#include "RSVP_PolicyElement.h"

class AccountInfo {
	uint32 accountNo;           // eventually, a different type is required
	friend ostream& operator<< ( ostream&, const AccountInfo& ); 
	friend istream& operator>> ( istream&, AccountInfo& ); 
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const AccountInfo& ); 
	friend INetworkBuffer& operator>> ( INetworkBuffer&, AccountInfo& );
public:
	AccountInfo( uint32 accountNo = 0 ) : accountNo(accountNo) {}
};

class UCPE {
	AccountInfo nextHopAccount;
	ieee32float payment;

	friend ostream& operator<< ( ostream&, const UCPE& ); 
	friend istream& operator>> ( istream&, UCPE& ); 
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const UCPE& ); 
	friend INetworkBuffer& operator>> ( INetworkBuffer&, UCPE& );

public:

	UCPE( const AccountInfo& nextHopAccount, ieee32float payment )
		: nextHopAccount(nextHopAccount), payment(payment) {}

	UCPE(INetworkBuffer& buffer ) { buffer >> *this; }
	
	ieee32float get_payment() const { return payment; }
	static uint16 size() { return 8; }
};

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const AccountInfo& o ) {
	buffer << o.accountNo; return buffer;
}

extern inline INetworkBuffer& operator>> (INetworkBuffer& buffer, AccountInfo& o) {
	buffer >> o.accountNo; return buffer;
}

extern inline ostream& operator<< ( ostream& os, const AccountInfo& o ) {
	os << o.accountNo; return os;
}
	
extern inline istream& operator>> ( istream& is, AccountInfo& o ) {
	is >> o.accountNo; return is;
}

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const UCPE& o ) {
	buffer << PolicyElement(o.size(), PolicyElement::UPSTREAM_CHARGING_POLICY)
		<< o.nextHopAccount << o.payment;
	return buffer;
}

extern inline INetworkBuffer& operator>> (INetworkBuffer& buffer, UCPE& o) {
	buffer >> o.nextHopAccount >> o.payment;
	return buffer;
}

extern inline ostream& operator<< ( ostream& os, const UCPE& o ) {
	os << "UCPE:" << "  " << o.nextHopAccount << "  " << o.payment;
	return os;
}
	
extern inline istream& operator>> ( istream& is, UCPE& o ) {
	is >> o.nextHopAccount >> o.payment;
	return is;
}

#endif /* _RSVP_ChargingElements_h_ */
