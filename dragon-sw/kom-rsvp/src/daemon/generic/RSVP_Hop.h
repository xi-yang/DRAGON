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
#ifndef _RSVP_Hop_h_
#define _RSVP_Hop_h_ 1

#include "RSVP_LogicalInterface.h"
#include "RSVP_Lists.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_Timer.h"


class HopKey {
protected:
	const LogicalInterface* lif;
	NetAddress addr;
	DECLARE_ORDER(HopKey)
public:
	HopKey( const LogicalInterface& lif, const NetAddress& addr )
		: lif(&lif), addr(addr) {}
	const LogicalInterface& getLogicalInterface() const { return *lif; }
	uint32 getLIH() const { return lif->getLIH(); }
	const NetAddress& getAddress() const { return addr; }
};
IMPLEMENT_ORDER2(HopKey,lif,addr)

inline bool Less<HopKey*>::operator()( const HopKey* h1, const HopKey* h2 ) const {
	return *h1 < *h2;
}

class Hop : public HopKey {
	friend ostream& operator<< ( ostream& os, const Hop& h );
	uint32 sbCount;
	TimeoutTimer<Hop> timeoutTimer;
	bool discovered;
	uint32 mplsHopInfo;
#if defined(REFRESH_REDUCTION)
public:
	struct SendStorageID {
		sint32 id;
		enum { Path, Resv } type;
		union {
			PHopSB* sendPHopSB;
			PSB*    sendPSB;   
		} sb;
	};
	struct RecvStorageID {
		sint32 id;
		enum { Path, Resv } type;
		union {
			PSB* psb;
			RSB* rsb;
		} sb;
		RecvStorageID() { sb.psb = NULL; }
		RecvStorageID( sint32 id, PSB* psb ) : id(id), type(Path) { sb.psb = psb; }
		RecvStorageID( sint32 id, RSB* rsb ) : id(id), type(Resv) { sb.rsb = rsb; }
	};
	typedef SimpleList<RecvStorageID> ID_List;
private:
	bool refreshReductCapable;
	uint32 sendEpoch;
	sint32 sendID;
	SendStorageID* idSend;
	uint32 idSendCount;
	uint32 currentRefreshHash;
	uint32 idRefreshPerMessageCount;
	uint32 noOfRefreshMessages;
	ID_List* idRecv;
	uint32 idRecvCount;
	sint32 currentRecvID;
	uint32 currentRecvEpoch;
	uint32 maxNackSize;
	RandomRefreshTimer<Hop> refreshTimer;
	inline uint32 sendHash( sint32 id );
	inline uint32 recvHash( sint32 id );
	inline void recalcTimer( sint32 change );
	inline void increaseSendID();
	inline bool getNextSendStorageID();
	void sendMessageReliable( const Message& msg, const NetAddress& dest, const NetAddress& src, const NetAddress& gw = LogicalInterface::noGatewayAddress );
	void sendMessageReliable( const Message& msg, const NetAddress& dest ) {
		sendMessageReliable( msg, dest, getLogicalInterface().getAddress() );
	}
#endif
public:
	Hop( const LogicalInterface& lif, const NetAddress& addr );
	~Hop();
	void addSB() { sbCount += 1; if ( sbCount == 1 ) timeoutTimer.cancel(); }
	void removeSB() { sbCount -= 1; if (sbCount == 0 && discovered) timeoutTimer.restart( lif->getRefreshInterval() ); }
	void timeout();
	void setStatic() { discovered = false; }
	uint32 getHopInfoMPLS() const { return mplsHopInfo; }
#if defined(REFRESH_REDUCTION)
	inline const MESSAGE_ID_Object getNextSendID();
	uint32 getEpoch() { return sendEpoch; }
	bool isRefreshReductionCapable() const { return refreshReductCapable; }
	void setRefreshReductionCapable( bool c ) { refreshReductCapable = c; }
	inline Hop::SendStorageID* storeSendPSB( PSB* psb );
	inline Hop::SendStorageID* storeSendPHopSB( PHopSB* phop );
	inline PSB* getSendPSB( sint32 id );
	inline PHopSB* getSendPHopSB( sint32 id );
	inline void clearSendState( sint32 id );
	inline Hop::RecvStorageID* storeRecvPSB( PSB* psb, sint32 id );
	inline Hop::RecvStorageID* storeRecvRSB( RSB* rsb, sint32 id );
	inline PSB* getRecvPSB( sint32 id );
	inline RSB* getRecvRSB( sint32 id );
	inline void clearRecvPSB( sint32 id );
	inline void clearRecvRSB( sint32 id, RSB* rsb );
	void processSrefresh( const Message& msg );
	void processAckMessage( const Message& );
	bool checkMessageID( const Message& msg );
	void refresh();
#endif
};

#if defined(REFRESH_REDUCTION)
typedef Hop::RecvStorageID Hop_RecvStorageID;
typedef Hop::ID_List Hop_ID_List;
DEDICATED_LIST_MEMORY_MACHINE(Hop_RecvStorageID,Hop_ID_List,idListMemMachine)
#endif

#if defined(REFRESH_REDUCTION)
inline uint32 Hop::sendHash( sint32 id ) {
	sint32 hash = id % RSVP_Global::idHashCountSend;
	if ( hash < 0 ) hash += RSVP_Global::idHashCountSend;
	return hash;
}

inline uint32 Hop::recvHash( sint32 id ) {
	sint32 hash = id % RSVP_Global::idHashCountRecv;
	if ( hash < 0 ) hash += RSVP_Global::idHashCountRecv;
	return hash;
}

inline void Hop::recalcTimer( sint32 change ) {
	if ( idSendCount > 0 ) {
		uint32 new_noOfRefreshMessages = idSendCount / getLogicalInterface().getMaxIdCount() + 1;
		idRefreshPerMessageCount = idSendCount / new_noOfRefreshMessages;
		if ( noOfRefreshMessages != new_noOfRefreshMessages ) {
			noOfRefreshMessages = new_noOfRefreshMessages;
			TimeValue newTimerPeriod = getLogicalInterface().getRefreshInterval() / noOfRefreshMessages;
			if ( refreshTimer.isActive() && refreshTimer.getRemainingTime() < newTimerPeriod ) {
				if ( noOfRefreshMessages == 1 ) idRefreshPerMessageCount -= change;
				if ( idRefreshPerMessageCount > 0 ) refresh();
				if ( noOfRefreshMessages == 1 ) idRefreshPerMessageCount += change;
			}
			LOG(4)( Log::Reduct, "setting refresh timer to", newTimerPeriod, "at", *this );
			refreshTimer.restart( newTimerPeriod );
		}
	} else {
                                                  assert( idSendCount == 0 );
		noOfRefreshMessages = 0;
		idRefreshPerMessageCount = 0;
		refreshTimer.cancel();
	}
}

inline void Hop::increaseSendID() {
	sendID += 1;
	if (sendID == (1<<31)) {
		sendEpoch += 1;
		if ( sendEpoch > 16777215 ) sendEpoch = 0;
	}
}

inline bool Hop::getNextSendStorageID() {
	if ( idSendCount >= RSVP_Global::idHashCountSend ) {
		ERROR(2)( Log::Error, "send ID hash is full at", *this );
		return false;
	}
	do {
		increaseSendID();
	} while (idSend[sendHash(sendID)].sb.sendPSB != NULL);
	idSend[sendHash(sendID)].id = sendID;
	idSendCount += 1;
	recalcTimer(1);
	LOG(6)( Log::Reduct, "chose ID", sendID, "idSendCount is", idSendCount, "at", *this );
	return true;
}
#endif

#if defined(REFRESH_REDUCTION)
inline const MESSAGE_ID_Object Hop::getNextSendID() {
	increaseSendID();
	return MESSAGE_ID_Object( 0, sendEpoch, sendID );
}

inline Hop::SendStorageID* Hop::storeSendPSB( PSB* psb ) {
	if ( !getNextSendStorageID() ) return NULL;
	idSend[sendHash(sendID)].type = SendStorageID::Path;
	idSend[sendHash(sendID)].sb.sendPSB = psb;
	LOG(6)( Log::Reduct, "stored PSB with ID", sendID, "at send hash value", sendHash(sendID), "at", *this );
	addSB();
	return &idSend[sendHash(sendID)];
}

inline Hop::SendStorageID* Hop::storeSendPHopSB( PHopSB* phop ) {
	if ( !getNextSendStorageID() ) return NULL;
	idSend[sendHash(sendID)].type = SendStorageID::Resv;
	idSend[sendHash(sendID)].sb.sendPHopSB = phop;
	LOG(6)( Log::Reduct, "stored PHopSB with ID", sendID, "at send hash value", sendHash(sendID), "at", *this );
	addSB();
	return &idSend[sendHash(sendID)];
}

inline PSB* Hop::getSendPSB( sint32 id ) {
	if ( idSend[sendHash(id)].id == id && idSend[sendHash(id)].type == SendStorageID::Path ) {
		return idSend[sendHash(id)].sb.sendPSB;
	}
	return NULL;
}

inline PHopSB* Hop::getSendPHopSB( sint32 id ) {
	if ( idSend[sendHash(id)].id == id && idSend[sendHash(id)].type == SendStorageID::Resv ) {
		return idSend[sendHash(id)].sb.sendPHopSB;
	}
	return NULL;
}

inline void Hop::clearSendState( sint32 id ) {
	idSend[sendHash(id)].sb.sendPHopSB = NULL;
	idSendCount -= 1;
	LOG(6)( Log::Reduct, "cleared send hash value", sendHash(id), "idSendCount is", idSendCount, "at", *this );
	recalcTimer(0);
	removeSB();
}

inline Hop::RecvStorageID* Hop::storeRecvPSB( PSB* psb, sint32 id ) {
	idRecv[recvHash(id)].push_back( RecvStorageID( id, psb ) );
	LOG(6)( Log::Reduct, "stored PSB with ID", id, "at recv hash value", recvHash(id), "from", *this );
	return &idRecv[recvHash(id)].back();
}

inline Hop::RecvStorageID* Hop::storeRecvRSB( RSB* rsb, sint32 id ) {
	idRecv[recvHash(id)].push_back( RecvStorageID( id, rsb ) );
	LOG(6)( Log::Reduct, "stored RSB with ID", id, "at recv hash value", recvHash(id), "from", *this );
	return &idRecv[recvHash(id)].back();
}

inline PSB* Hop::getRecvPSB( sint32 id ) {
	ID_List& idList = idRecv[recvHash(id)];
	ID_List::ConstIterator iter = idList.begin();
	for ( ; iter != idList.end(); ++iter ) {
		if ( (*iter).type == RecvStorageID::Path && (*iter).id == id ) {
			return (*iter).sb.psb;
		}
	}
	return NULL;
}

inline RSB* Hop::getRecvRSB( sint32 id ) {
	ID_List& idList = idRecv[recvHash(id)];
	ID_List::ConstIterator iter = idList.begin();
	for ( ; iter != idList.end(); ++iter ) {
		if ( (*iter).type == RecvStorageID::Resv && (*iter).id == id ) {
			return (*iter).sb.rsb;
		}
	}
	return NULL;
}

inline void Hop::clearRecvPSB( sint32 id ) {
	ID_List& idList = idRecv[recvHash(id)];
	ID_List::ConstIterator iter = idList.begin();
	for ( ; iter != idList.end(); ++iter ) {
		if ( (*iter).id == id && (*iter).type == RecvStorageID::Path ) {
			LOG(6)( Log::Reduct, "cleared ID", id, "from recv hash value", recvHash(id), "from", *this );
			idList.erase( iter );
	return;
		}
	}
}

inline void Hop::clearRecvRSB( sint32 id, RSB* rsb ) {
	ID_List& idList = idRecv[recvHash(id)];
	ID_List::ConstIterator iter = idList.end();
                                                   assert( !idList.empty() );
	do {
		--iter;
		if ( (*iter).id == id && (*iter).sb.rsb == rsb ) {
			LOG(6)( Log::Reduct, "cleared ID", id, "from recv hash value", recvHash(id), "from", *this );
			idList.erase( iter );
	return;
		}
	} while ( iter != idList.begin() );
}

#endif /* REFRESH_REDUCTION */

#endif /* _RSVP_Hop_h_ */
