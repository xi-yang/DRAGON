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
#ifndef _RSVP_MessageProcessor_h_
#define _RSVP_MessageProcessor_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_FilterSpecList.h"
#include "RSVP_Global.h"
#include "RSVP_Lists.h"
#include "RSVP_Message.h"
#include "RSVP_PacketHeader.h"

class SESSION_Object;
class SCOPE_Object;
class FlowDescriptor;
class FLOWSPEC_Object;
class LogicalInterface;
class OutISB;
class OIatPSB;
class PHopSB_Refresh;
class LogicalInterfaceSet;

typedef SortableList<PHopSB_Refresh,RSVP_HOP_Object> PHOP_RefreshList;

//@@@@hack: Xi2007>>
class MessageEntry {

	static ONetworkBuffer obuffer;
	INetworkBuffer ibuffer;
	LogicalInterface* currentLif;
	Session* currentSession;

public:
	MessageEntry():ibuffer(65000), currentLif(NULL), currentSession(NULL) {}
	LogicalInterface* getCurrentLif() { return currentLif; }
	Session* getCurrentSession() { return currentSession; }
	void PreserveMessage(LogicalInterface *lif,  Session *session, Message& msg) {
		currentLif = lif;
		currentSession = session;
		MessageEntry::obuffer.init();
		MessageEntry::obuffer << msg;
		ibuffer.init();
		ibuffer.cloneFrom(MessageEntry::obuffer.getContents(), MessageEntry::obuffer.getSize());
	}
	void RestoreMessage(LogicalInterface* &lif, Session* &session, Message& msg) {
		assert(currentLif);
		lif = currentLif;
		assert(currentSession);
		session = currentSession;
		assert(ibuffer.getSize() > 0);
		msg.init();
		ibuffer >> msg;
	}
};

typedef SimpleList<MessageEntry*> MessageQueue;

//@@@@hack: Xi2007<<



class MessageProcessor {

	// "status info" for currently processed message
	INetworkBuffer ibuffer;
	Message currentMessage;
	Hop* sendingHop;
	const LogicalInterface* currentLif;
	const LogicalInterface* incomingLif;
	PacketHeader currentHeader;
	Session* currentSession;

	// OutISB where RSB has asked for RCONF
	OutISB* confirmOutISB;
	// confirm message that is built
	Message* confirmMsg;
	uint16 confirmLength;

	// list of PHOPS that need RESV, RTEAR or RERR
	PHOP_RefreshList phopRefreshList;

	// outgoing reservation message
	Message* resvMsg;
	uint16 resvLength;
#if defined(USE_SCOPE_OBJECT)
	SCOPE_Object* resvScope;
#endif
	POLICY_DATA_Object* resvPolicy;

	bool B_Merge;
	bool fullRefresh;

//@@@@hack: Xi2007>>
	MessageQueue* msgQueue;
//@@@@hack: Xi2007<<

public:
#if defined(ONEPASS_RESERVATION)
	PSB* onepassPSB;
#endif
private:

	void refreshReservations();
	void forwardCurrentMessage();

	inline bool checkCurrentLif();
	inline void findSendingHop();
	bool checkPathMessage();

	void sendTearMessage( PHopSB_Refresh& phop );

	// return value indicates whether confirmation request has to be forwarded
	bool createResvMessageFF( PHopSB_Refresh& phop );
	// return value indicates whether confirmation request has to be forwarded
	bool createResvMessageShared( PHopSB_Refresh& phop );

	inline void addToMessage( Message&, const FLOWSPEC_Object&, const FilterSpecList& );
	inline void prepareConfirmMsg();
	inline void finishAndSendConfirmMsg();

public:
	MessageProcessor();
	~MessageProcessor();

	void readCurrentMessage( const LogicalInterface& );
	void processMessage();
	// for multicast sessions
	void processAsyncRoutingEvent( Session*, const NetAddress&, const LogicalInterface&, LogicalInterfaceSet );
	// for unicast sessions
	void processAsyncRoutingEvent( Session*, const LogicalInterface&, const NetAddress& );
	void internalResvRefresh( Session*, PHopSB& );

	void prepareExit() { fullRefresh = false; }

	// implemented in respective class files
	inline void refreshOIatPSB( OIatPSB& );
	inline void refreshPHopSB( PHopSB&, Session* );
	inline void timeoutPSB( PSB* );
	inline void timeoutRSB( RSB* );
	inline void timeoutFilter( OIatPSB* );

#if defined(WITH_API)
	void deregisterAPI( const SESSION_Object&, const NetAddress&, uint16 apiPort );
	void registerAPI( const SESSION_Object& );
#endif
#if defined(ONEPASS_RESERVATION)
	PSB* getOnepassPSB() { return onepassPSB; }
	void setOnepassPSB( PSB* p ) { onepassPSB = p; }
#endif

	void markForResvRefresh( PSB& psb );
	void markForResvRemove( PSB& psb );
	void setConfirmOutISB( OutISB& oisb ) { confirmOutISB = &oisb; }
	void noteThatOutISBisBlockaded( OutISB& oisb ) {
		if ( &oisb == confirmOutISB ) confirmOutISB = NULL;
	}

	void addToConfirmMsg( const FLOWSPEC_Object&, const FilterSpecList& );

	void sendResvErrMessage( uint8 errorFlags, uint8 errorCode, uint16 errorValue, const FlowDescriptor& );
	void sendResvErrMessage( uint8 errorFlags, uint8 errorCode, uint16 errorValue );
	void sendPathErrMessage( uint8 errorCode, uint16 errorValue );

//@@@@hack: Xi2007>>
	bool queryEnqueuedMessages();
//@@@@hack: Xi2007<<

};

#endif /* _RSVP_MessageProcessor_h_ */
