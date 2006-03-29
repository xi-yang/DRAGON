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
#ifndef _RSVP_API_h
#define _RSVP_API_h 1

#include "RSVP_API_StateBlock.h"
#include "RSVP_BasicTypes.h"
#include "RSVP_FlowDescriptor.h"
#include "RSVP_SortableList.h"
#include "RSVP_String.h"

class TSpec; class RSpec;
class SESSION_Object;
class ADSPEC_Object;
class POLICY_DATA_Object;
class LogicalInterfaceUDP;
class Message;

typedef SortableList<API_StateBlock*,SESSION_Object*> ApiStateBlockList;

struct _rsvp_upcall_parameter {
	in_addr destAddr;	//tunnelAddress
	uint16 destPort;		//tunnelID
	in_addr srcAddr;	//extendedTunnelID
	uint16 srcPort;	//lsp-id
	const char* name;		//Name of the LSP
	uint32 upstreamLabel;		//!=0 if bi-dir
	uint32 bandwidth;	//bandwidth
	uint8 lspEncodingType; //LSP encoding type
	uint8 switchingType; // LSP switching type
	uint16 gPid;		//G-Pid
	void* sendTSpec;  //Sender TSpec
	void* adSpec;
	void* session;	//RSVP_API::SessionId
	void* senderTemplate;
	uint8 code;			//error/success code
};
typedef void (*zUpcall)(void* para);

struct _Session_Para {
	in_addr srcAddr;	
	uint16 srcPort;
	in_addr destAddr;
	uint16 destPort;
};
struct _ADSpec_Para {
	uint32 ADSpecHopCount;
	uint32 ADSpecBandwidth;
	sint32 ADSpecMinPathLatency;
	uint32 ADSpecMTU;
	uint32 ADSpecCLHopCount;
	sint32 ADSpecCLMinPathLatency;
	sint32 ADSpecGSCtot;
	sint32 ADSpecGSDtot;
	sint32 ADSpecGSCsum;
	sint32 ADSpecGSDsum;
	sint32 ADSpecGSMinPathLatency;
};
struct _GenericTSpec_Para {
	uint32 R;
	uint32 B;
	uint32 P;
	sint32 m;
	sint32 M;
};
struct _SonetTSpec_Para {
	uint8 Sonet_ST;		//"Sonet_" is added to avoid compilation problem in gcc 3.4.x
	uint8 Sonet_RCC;
	uint16 Sonet_NCC;
	uint16 Sonet_NVC;
	uint16 Sonet_MT;
	uint32 Sonet_T;
	uint32 Sonet_P;
};
struct _EROAbstractNode_Para {
	uint8 type;
	uint8 isLoose;
	union {
		struct {
			struct in_addr addr;
			uint8 prefix;
		} ip4;
		struct {
			uint16 reserved;
			struct in_addr routerID;
			uint32 interfaceID;
		}uNumIfID;
		uint16 asNum;
	} data;
};
struct _SessionAttribute_Para {
	uint32 excludeAny;
	uint32 includeAny;
	uint32 includeAll;
	uint8 setupPri;
	uint8 holdingPri;
	uint8 flags;
	uint8 nameLength;
	char* sessionName;
};

struct _LabelRequest_Para {
	union {
		struct {
			uint8 lspEncodingType;
			uint8 switchingType;
			uint16 gPid;
		} gmpls;
		uint32 mpls_l3pid; //For MPLS
	}data;
	uint8 labelType;
};

struct _Protection_Para {
	uint8 secondary;
	uint8 pro_type;
};

struct _Dragon_Uni_Para {
	uint32 srcLocalId;
	uint32 destLocalId;
};


struct _sessionParameters {
	//Mandatory parameters
	struct _LabelRequest_Para LabelRequest_Para;
	struct _Session_Para Session_Para;
	//Optional parameters
	struct _ADSpec_Para* ADSpec_Para;
	struct _GenericTSpec_Para* GenericTSpec_Para;
	struct _SonetTSpec_Para* SonetTSpec_Para;
	uint8 ERONodeNumber;	// 32 in Maximum
	struct _EROAbstractNode_Para* EROAbstractNode_Para;
	struct _Dragon_Uni_Para* Dragon_Uni_Para;
	uint8 labelSetSize;	// 8 in maximum
	uint32* labelSet;
	struct _SessionAttribute_Para* SessionAttribute_Para;
	uint32* upstreamLabel;
	struct _Protection_Para* Protection_Para;
};

class RSVP_API {
public:
	typedef ApiStateBlockList::ConstIterator SessionId;
private:
	uint32 sessionHash;
	TimeValue apiRefresh;
	uint16 apiPort;
	ApiStateBlockList* stateList;
	static LogicalInterfaceUDP* apiLif;

	void process( Message& , zUpcall upcall = NULL);
	static void refreshSession( const SESSION_Object& session, const TimeValue& = 0 );
	void constructor( uint16 apiPort, NetAddress apiHost );
	inline ApiStateBlockList& getStateList( const SESSION_Object& s );
	friend class API_StateBlock;                       // access: refreshSession
#if defined(NS2)
	friend class RSVP_API_Wrapper;
	friend class LogicalInterface;
#endif
#if defined(WITH_JAVA_API)
protected:
	virtual void preUpcall() {}
	virtual void postUpcall() {}
#endif
public:
	RSVP_API( const String&, uint32 sessionHash = 1 );
	RSVP_API( uint16 apiPort = 0, const NetAddress& apiHost = 0 ) : sessionHash(1)
		{ constructor( apiPort, apiHost ); }
#if defined(WITH_JAVA_API)
	virtual ~RSVP_API();
#else
	~RSVP_API();
#endif
	ApiStateBlockList::Iterator findSession(SESSION_Object &session, UpcallProcedure upcall=NULL, void* clientData=NULL );
	SessionId createSession( const NetAddress&, uint16, uint32, UpcallProcedure = NULL, void* clientData = NULL );
	void createSender( SessionId, const NetAddress&, uint16 port, const SENDER_TSPEC_Object&,
		const LABEL_REQUEST_Object&  labelReqObj, EXPLICIT_ROUTE_Object* ero, 
		DRAGON_UNI_Object* uni,
		LABEL_SET_Object* labelSet, SESSION_ATTRIBUTE_Object* ssAttrib,
		UPSTREAM_LABEL_Object* upstreamLabel,
		uint8 TTL, const ADSPEC_Object* = NULL, const POLICY_DATA_Object* = NULL,
		bool reserve = false, uint16 senderRecvPort = 0, uint16 recvSendPort = 0);
	void createSender( SessionId session, uint16 port, const SENDER_TSPEC_Object& tspec,
		const LABEL_REQUEST_Object&  labelReqObj, EXPLICIT_ROUTE_Object* ero, 
		DRAGON_UNI_Object* uni,
		LABEL_SET_Object* labelSet, 
		SESSION_ATTRIBUTE_Object* ssAttrib,
		UPSTREAM_LABEL_Object* upstreamLabel,
		uint8 TTL, const ADSPEC_Object* adSpec = NULL,
		const POLICY_DATA_Object* policyData = NULL, 
		bool reserve = false,
		uint16 senderRecvPort = 0, uint16 recvSendPort = 0 ) {
		createSender( session, 0, port, tspec, labelReqObj, ero, uni, labelSet, ssAttrib, upstreamLabel, TTL, adSpec, policyData, reserve, senderRecvPort, recvSendPort );
	}
	void createReservation( SessionId, bool confRequest, FilterStyle,
		const FlowDescriptorList&, const POLICY_DATA_Object* policyData = NULL );
	void releaseSession( SessionId );
	void releaseSession( SESSION_Object& session ) ;
	void releaseSender( SessionId, const NetAddress&, uint16 port, uint8 TTL );
	void releaseSender( SessionId session, uint16 port, uint8 TTL ) {
		releaseSender( session, 0, port, TTL );
	}
	void releaseReservation( SessionId, FilterStyle, const FlowDescriptorList& );
	void releaseReservation( SESSION_Object& session, FilterStyle style, const FlowDescriptorList& fdList );
	InterfaceHandle getFileDesc() const;
	void receiveAndProcess(zUpcall upcall = NULL);
	void sleep( TimeValue, const bool& endFlag = false );
	void run( const bool& endFlag = false );
	void changeRefreshTimeout( const TimeValue& ar ) { apiRefresh = ar; }
       //@@@@ hacked
       void addLocalId(uint16 type, uint16 value, uint16 tag);
       //@@@@ hacked
       void deleteLocalId(uint16 type, uint16 value, uint16 tag);
};

//The following functions are called by outside C applications such as Zebra-OSPF-TE
extern "C" {
	extern void zInitRsvpPathRequest(void *thisApi, struct _sessionParameters* para, uint8 isSender);
	extern void zTearRsvpPathRequest(void *api, struct _sessionParameters* para);
	extern void zTearRsvpResvRequest(void* api, struct _sessionParameters* para);
	extern int zGetApiFileDesc(void *api);
	extern void zApiReceiveAndProcess(void *api, zUpcall upcall);
	extern void* zInitRsvpApiInstance();
	extern void zInitRsvpResvRequest(void* api, struct _rsvp_upcall_parameter* upcallPara);
	extern void zAddLocalId(void* api, uint16 type, uint16 value, uint16 tag);
	extern void zDeleteLocalId(void* api, uint16 type, uint16 value, uint16 tag);
}

#endif /* _RSVP_API_h */
