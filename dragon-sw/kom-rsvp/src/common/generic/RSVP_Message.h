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
#ifndef _RSVP_Message_h_
#define _RSVP_Message_h_ 1

#include "RSVP_FlowDescriptor.h"
#include "RSVP_GeneralMemoryMachine.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Lists.h"
#include "RSVP_PolicyObjects.h"
#include "RSVP_ProtocolObjects.h"
#include "SwitchCtrl_Global.h"
//#include "SNMP_Session.h"
//#include "CLI_Session.h"

class Message {

public:
	enum Type { InitAPI = 0, Path = 1, Resv, PathErr, ResvErr, PathTear, ResvTear, ResvConf, Ack = 13, Srefresh = 15, Load = 126, PathResv = 127,
				  RemoveAPI = 255, /*hacked*/ AddLocalId = 201, DeleteLocalId = 202, };
	enum Flag { RefreshReduction = 0x01 };
	enum Status { Correct, Drop, Reject };

protected:
	uint8		versionFlags;
	uint8		msgType;
	uint8		ttl;
	uint16	length;

	enum ObjectFlags { 
		SESSION 				= (1 << 0),
		RSVP_HOP				= (1 << 1),
		TIME_VALUES			= (1 << 2),
		ERROR_SPEC			= (1 << 3),
		STYLE						= (1 << 4),
		FILTER_SPEC			= (1 << 5),
		SENDER_TEMPLATE	= (1 << 6),
		SENDER_TSPEC		= (1 << 7),
		RESV_CONFIRM		= (1 << 8),
		LABEL						= (1 << 9),
		LABEL_REQUEST		= (1 << 10),
		MESSAGE_ID			= (1 << 11),
		MESSAGE_ID_LIST = (1 << 12),
		LOAD_REPORT			= (1 << 13),
		DUPLEX					= (1 << 14),
		HELLO				= (1 << 15),
		SUGGESTED_LABEL 	= (1 << 16),
		SESSION_ATTRIBUTE = (1 << 17),
		UPSTREAM_LABEL	= (1 << 18),
		//DRAGON_UNI		= (1 << 19)
	};

	mutable Status status;
	mutable uint32 objectFlags;

	SESSION_Object               SESSION_Object_O;
	RSVP_HOP_Object              RSVP_HOP_Object_O;
	const INTEGRITY_Object*      INTEGRITY_Object_P;
	TIME_VALUES_Object           TIME_VALUES_Object_O;
	ERROR_SPEC_Object            ERROR_SPEC_Object_O;
	const SCOPE_Object*          SCOPE_Object_P;
	SENDER_TEMPLATE_Object       SENDER_TEMPLATE_Object_O;
	SENDER_TSPEC_Object          SENDER_TSPEC_Object_O;
	const ADSPEC_Object*         ADSPEC_Object_P;
	PolicyObjectList             policyList;
	RESV_CONFIRM_Object          RESV_CONFIRM_Object_O;
	STYLE_Object                 STYLE_Object_O;
	FlowDescriptorList           flowDescriptorList;
	UnknownObjectList            unknownObjectList;
	LABEL_REQUEST_Object         LABEL_REQUEST_Object_O;
	const EXPLICIT_ROUTE_Object* EXPLICIT_ROUTE_Object_P;
	uint16 EXPLICIT_ROUTE_Object_Length;
	const LABEL_SET_Object* LABEL_SET_Object_P;
	SUGGESTED_LABEL_Object SUGGESTED_LABEL_Object_O;
	UPSTREAM_LABEL_Object	UPSTREAM_LABEL_Object_O;
	SESSION_ATTRIBUTE_Object	SESSION_ATTRIBUTE_Object_O;
#if defined(ONEPASS_RESERVATION)
	DUPLEX_Object                DUPLEX_Object_O;
#endif
#if defined(REFRESH_REDUCTION)
	MESSAGE_ID_Object            MESSAGE_ID_Object_O;
	MESSAGE_ID_LIST_Object       MESSAGE_ID_LIST_Object_O;
	MESSAGE_ID_ACK_List          ackList;
	MESSAGE_ID_NACK_List         nackList;
#endif

	DRAGON_UNI_Object*		DRAGON_UNI_Object_P;

	bool checkFlowdescList() const;

	friend ostream& operator<< ( ostream&, const Message& );
	friend INetworkBuffer& operator>> ( INetworkBuffer&, Message& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const Message& );

	// forbid copy and assignment
	Message( const Message& msg );
	Message& operator= ( const Message& msg );

	void checkINTEGRITY_Object( const INTEGRITY_Object* );
	void checkSCOPE_Object( const SCOPE_Object* );
	void checkFLOWSPEC_Object( const FLOWSPEC_Object* );
	void checkADSPEC_Object( const ADSPEC_Object* );
	uint16 checkPOLICY_DATA_Object( const POLICY_DATA_Object* );
	void checkEXPLICIT_ROUTE_Object( const EXPLICIT_ROUTE_Object* );
	void checkLABEL_SET_Object(const LABEL_SET_Object*);

	void checkDRAGON_UNI_Object(DRAGON_UNI_Object*);

	bool checkForFilter();
	void checkStatus() const;

public:
	Message() : versionFlags(1<<4), msgType(0), ttl(0), length(0),
	status(Correct), objectFlags(0),
	INTEGRITY_Object_P(NULL), SCOPE_Object_P(NULL), ADSPEC_Object_P(NULL)
	, EXPLICIT_ROUTE_Object_P(NULL), EXPLICIT_ROUTE_Object_Length(0), LABEL_SET_Object_P(NULL)
	, DRAGON_UNI_Object_P(NULL)
	{}

	Message( uint8 msgType, uint8 ttl, const SESSION_Object& session, bool clearE_Police = false )
	: versionFlags(1<<4), msgType(msgType), ttl(ttl), length(headerSize()),
	status(Correct), objectFlags(SESSION), SESSION_Object_O(session),
	INTEGRITY_Object_P(NULL), SCOPE_Object_P(NULL), ADSPEC_Object_P(NULL)
	, EXPLICIT_ROUTE_Object_P(NULL) , EXPLICIT_ROUTE_Object_Length(0), LABEL_SET_Object_P(NULL)
	, DRAGON_UNI_Object_P(NULL)
	{
		length += SESSION_Object::total_size();
	}

#if defined(REFRESH_REDUCTION)
	Message( uint32 epoch ) : versionFlags(1<<4), msgType(Srefresh), ttl(15),
	length(headerSize()), status(Correct), objectFlags(MESSAGE_ID_LIST),
	INTEGRITY_Object_P(NULL), SCOPE_Object_P(NULL), ADSPEC_Object_P(NULL)
	, EXPLICIT_ROUTE_Object_P(NULL), EXPLICIT_ROUTE_Object_Length(0), LABEL_SET_Object_P(NULL)
	, MESSAGE_ID_LIST_Object_O( 0, epoch ), DRAGON_UNI_Object_P(NULL)
	{
		length += MESSAGE_ID_LIST_Object_O.total_size();
	}
	Message( uint8 msgType, uint8 ttl ) : versionFlags(1<<4), msgType(msgType), ttl(ttl), length(headerSize()),
	status(Correct), objectFlags(0),
	INTEGRITY_Object_P(NULL), SCOPE_Object_P(NULL), ADSPEC_Object_P(NULL)
	, EXPLICIT_ROUTE_Object_P(NULL), EXPLICIT_ROUTE_Object_Length(0), LABEL_SET_Object_P(NULL)
	, DRAGON_UNI_Object_P(NULL)
	{}
#endif

	~Message();

	void init();

	inline Status getStatus() const { return status; }

	static uint16 headerSize() { return 8; }

	const SESSION_Object& getSESSION_Object() const { assert(objectFlags & SESSION); return SESSION_Object_O; }
	const RSVP_HOP_Object& getRSVP_HOP_Object() const { assert(objectFlags & RSVP_HOP); return RSVP_HOP_Object_O; }
	const INTEGRITY_Object* getINTEGRITY_Object() const { return INTEGRITY_Object_P; }
	const TIME_VALUES_Object& getTIME_VALUES_Object() const { assert(objectFlags & TIME_VALUES); return TIME_VALUES_Object_O; }
	bool hasTIME_VALUES_Object() const { return (objectFlags & TIME_VALUES); }
	const ERROR_SPEC_Object& getERROR_SPEC_Object() const { assert(objectFlags & ERROR_SPEC); return ERROR_SPEC_Object_O; }
	const SCOPE_Object* getSCOPE_Object() const { return SCOPE_Object_P; }
	const SENDER_TEMPLATE_Object& getSENDER_TEMPLATE_Object() const { assert(objectFlags & SENDER_TEMPLATE); return SENDER_TEMPLATE_Object_O; }
	const SENDER_TSPEC_Object& getSENDER_TSPEC_Object() const { assert(objectFlags & SENDER_TSPEC); return SENDER_TSPEC_Object_O; }
	const ADSPEC_Object* getADSPEC_Object() const { return ADSPEC_Object_P; }
	const PolicyObjectList& getPolicyList() const { return policyList; }
	const RESV_CONFIRM_Object& getRESV_CONFIRM_Object() const { assert(objectFlags & RESV_CONFIRM); return RESV_CONFIRM_Object_O; }
	bool hasRESV_CONFIRM_Object() const { return (objectFlags & RESV_CONFIRM); }
	const STYLE_Object& getSTYLE_Object() const { assert(objectFlags & STYLE); return STYLE_Object_O; }
	const FlowDescriptorList& getFlowDescriptorList() const { return flowDescriptorList; }
	const UnknownObjectList& getUnknownObjectList() const { return unknownObjectList; }
	const LABEL_REQUEST_Object& getLABEL_REQUEST_Object() const { assert(objectFlags & LABEL_REQUEST); return LABEL_REQUEST_Object_O; }
	bool hasLABEL_REQUEST_Object() const { return (objectFlags & LABEL_REQUEST); }
	const EXPLICIT_ROUTE_Object* getEXPLICIT_ROUTE_Object() const { return EXPLICIT_ROUTE_Object_P; }
	const LABEL_SET_Object* getLABEL_SET_Object() const { return LABEL_SET_Object_P; }
	const SUGGESTED_LABEL_Object& getSUGGESTED_LABEL_Object() const { assert(objectFlags & SUGGESTED_LABEL); return SUGGESTED_LABEL_Object_O; }
	bool hasSUGGESTED_LABEL_Object() const { return (objectFlags & SUGGESTED_LABEL); }
	const UPSTREAM_LABEL_Object& getUPSTREAM_LABEL_Object() const { assert(objectFlags & UPSTREAM_LABEL); return UPSTREAM_LABEL_Object_O; }
	bool hasUPSTREAM_LABEL_Object() const { return (objectFlags & UPSTREAM_LABEL); }
	const SESSION_ATTRIBUTE_Object& getSESSION_ATTRIBUTE_Object() const { assert(objectFlags & SESSION_ATTRIBUTE); return SESSION_ATTRIBUTE_Object_O; }
	bool hasSESSION_ATTRIBUTE_Object() const { return (objectFlags & SESSION_ATTRIBUTE); }
#if defined(ONEPASS_RESERVATION) 
	const DUPLEX_Object& getDUPLEX_Object() const { assert(objectFlags & DUPLEX); return DUPLEX_Object_O; }
	bool hasDUPLEX_Object() const { return (objectFlags & DUPLEX); }
#endif
#if defined(REFRESH_REDUCTION)
	bool hasMESSAGE_ID_Object() const { return (objectFlags & MESSAGE_ID); }
	const MESSAGE_ID_Object& getMESSAGE_ID_Object() const { assert(objectFlags & MESSAGE_ID); return MESSAGE_ID_Object_O; }
	const MESSAGE_ID_ACK_List& getMESSAGE_ID_ACK_List() const { return ackList; }
	const MESSAGE_ID_NACK_List& getMESSAGE_ID_NACK_List() const { return nackList; }
	const MESSAGE_ID_LIST_Object& getMESSAGE_ID_LIST_Object() const { assert(objectFlags & MESSAGE_ID_LIST); return MESSAGE_ID_LIST_Object_O; }
#endif

        //@@@@ hacked
        LocalId* getLocalIdObject() { 
            LocalId* lid = new LocalId; 
            lid->type = SESSION_Object_O.getTunnelId(); 
            lid->value = (uint16)(SESSION_Object_O.getExtendedTunnelId() >> 16); 
            lid->group = new SimpleList<uint16>;
            if ((SESSION_Object_O.getExtendedTunnelId() & 0xffff) != 0)
                lid->group->push_back((uint16)(SESSION_Object_O.getExtendedTunnelId() & 0xffff));
            return lid; 
            }

#define Message_CHECK_OBJECT(XXX) \
                                              assert( !(objectFlags & XXX) ); \
	XXX ## _Object_O = o; \
	length += o.total_size(); \
	objectFlags |= XXX;

	void setRSVP_HOP_Object( const RSVP_HOP_Object& o ) {
		Message_CHECK_OBJECT(RSVP_HOP)
	}
	void clearRSVP_HOP_Object(const RSVP_HOP_Object& o) {
                                            assert( (objectFlags & RSVP_HOP) );
		objectFlags &= ~RSVP_HOP;
		length -= o.total_size();
	}
	void setINTEGRITY_Object( const INTEGRITY_Object& o ) {
		checkINTEGRITY_Object( o.borrow() );
	}
	void setTIME_VALUES_Object( const TIME_VALUES_Object& o ) {
		Message_CHECK_OBJECT(TIME_VALUES)
	}
	void setERROR_SPEC_Object( const ERROR_SPEC_Object& o ) {
		Message_CHECK_OBJECT(ERROR_SPEC)
	}
	void setSCOPE_Object( const SCOPE_Object& o ) {
		checkSCOPE_Object( o.borrow() );
	}
	void setSENDER_TEMPLATE_Object( const SENDER_TEMPLATE_Object& o ) {
		Message_CHECK_OBJECT(SENDER_TEMPLATE)
	}
	void setSENDER_TSPEC_Object( const SENDER_TSPEC_Object& o ) {
		Message_CHECK_OBJECT(SENDER_TSPEC)
	}
	void setADSPEC_Object( const ADSPEC_Object& o ) {
		checkADSPEC_Object( o.borrow() );
	}
	void addPOLICY_DATA_Object( const POLICY_DATA_Object& o ) {
		checkPOLICY_DATA_Object( o.borrow() );
	}
	void setRESV_CONFIRM_Object( const RESV_CONFIRM_Object& o ) {
		Message_CHECK_OBJECT(RESV_CONFIRM)
	}
	void clearRESV_CONFIRM_Object() {
		if ( objectFlags & RESV_CONFIRM ) {
			objectFlags &= ~RESV_CONFIRM;
			length -= RESV_CONFIRM_Object::total_size();
		}
	}
	void setSTYLE_Object( const STYLE_Object& o ) {
		Message_CHECK_OBJECT(STYLE)
	}
	void addFLOWSPEC_Object( const FLOWSPEC_Object& o ) {
		checkFLOWSPEC_Object( o.borrow() );
	}
	void setFLOWSPEC_Object( const FLOWSPEC_Object& o );
	void addFILTER_SPEC_Objects( const FilterSpecList& o );
	void addUnknownObjects( const UnknownObjectList& );
	void setLABEL_REQUEST_Object( const LABEL_REQUEST_Object& o ) {
		Message_CHECK_OBJECT(LABEL_REQUEST)
	}
	void setEXPLICIT_ROUTE_Object( const EXPLICIT_ROUTE_Object& o ) {
		checkEXPLICIT_ROUTE_Object( o.borrow() );
	}
	void updateEXPLICIT_ROUTE_Object_Length(uint32 l){
		EXPLICIT_ROUTE_Object_Length = l;
	}
	void clearEXPLICIT_ROUTE_Object() {
		if (EXPLICIT_ROUTE_Object_P){
			EXPLICIT_ROUTE_Object_P->destroy();
			length -=  EXPLICIT_ROUTE_Object_Length;
			EXPLICIT_ROUTE_Object_P = NULL;
		}
	}
	void setLABEL_SET_Object( const LABEL_SET_Object& o ) {
		checkLABEL_SET_Object( o.borrow() );
	}
	void setSUGGESTED_LABEL_Object( const SUGGESTED_LABEL_Object& o ) {
		Message_CHECK_OBJECT(SUGGESTED_LABEL)
	}
	void setUPSTREAM_LABEL_Object( const UPSTREAM_LABEL_Object& o ) {
		Message_CHECK_OBJECT(UPSTREAM_LABEL)
	}
	void setSESSION_ATTRIBUTE_Object( const SESSION_ATTRIBUTE_Object& o ) {
		Message_CHECK_OBJECT(SESSION_ATTRIBUTE)
	}
#if defined(ONEPASS_RESERVATION) 
	void setDUPLEX_Object( const DUPLEX_Object& o ) {
		Message_CHECK_OBJECT(DUPLEX)
	}
	void clearDUPLEX_Object() {
		if ( objectFlags & DUPLEX ) {
			objectFlags &= ~DUPLEX;
			length -= DUPLEX_Object::total_size();
		}
	}
#endif
#if defined(REFRESH_REDUCTION)
	void setMESSAGE_ID_Object( const MESSAGE_ID_Object& o ) {
		Message_CHECK_OBJECT(MESSAGE_ID)
	}
	void clearMESSAGE_ID_Object() {
                                            assert( objectFlags & MESSAGE_ID );
		objectFlags &= ~MESSAGE_ID;
		length -= MESSAGE_ID_Object::total_size();
	}
	void addMESSAGE_ID_ACK_Object( const MESSAGE_ID_ACK_Object& o ) {
		ackList.push_back( o );
		length += o.total_size();
	}
	void addMESSAGE_ID_NACK_Object( const MESSAGE_ID_NACK_Object& o ) {
		nackList.push_back( o );
		length += o.total_size();
	}
	void add_ID_for_MESSAGE_ID_LIST( const sint32 id ) {
                                       assert( objectFlags & MESSAGE_ID_LIST );
		MESSAGE_ID_LIST_Object_O.addID( id );
		length += sizeof(sint32);
	}
#endif

	DRAGON_UNI_Object* getDRAGON_UNI_Object() {
		return DRAGON_UNI_Object_P;
	}
	void setDRAGON_UNI_Object( DRAGON_UNI_Object& o ) {
		checkDRAGON_UNI_Object( o.borrow() );
	}
	void clearDRAGON_UNI_Object() {
		if (DRAGON_UNI_Object_P){
			DRAGON_UNI_Object_P->destroy();
			length -=  DRAGON_UNI_Object_P->total_size();
			DRAGON_UNI_Object_P = NULL;
		}
	}

	void revertToError( const ERROR_SPEC_Object& );

#if defined(ONEPASS_RESERVATION) 
	void switchDuplex( const NetAddress& = NetAddress(0) );
#endif

	uint8 getTTL() const { return ttl; }
	void decrementTTL() { ttl -= 1; }
	void setTTL( uint8 t ) { ttl = t; }

	uint8 getFlags() const { return versionFlags % 16; }
	void setFlags( uint8 flags ) { versionFlags = (versionFlags & 0xf0) | (flags & 0x0f); }
	uint8 getVersion() const { return versionFlags / 16; }
	uint8 getMsgType() const { return msgType; }
	uint16 getLength() const { return length; }

	DECLARE_MEMORY_MACHINE_IN_CLASS(Message)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(Message, msgMemMachine)

#if defined(LOG_ON)
struct MessagePrintShort : public Message {};
extern ostream& operator<< ( ostream&, const MessagePrintShort& );
#endif

#endif /* _RSVP_Message_h_ */
