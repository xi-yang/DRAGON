#ifndef _RSVP_MPLS_h_
#define _RSVP_MPLS_h_ 1


#include "RSVP_BasicTypes.h"
#include "RSVP_Global.h"
#include "RSVP_Log.h"
#include "RSVP_SortableHash.h"
#include "RSVP_SortedList.h"

class PSB;
class OIatPSB;
class Message;
class SESSION_Object;
class SENDER_Object;
class EXPLICIT_ROUTE_Object;
class LogicalInterface;
class Hop;
class LABEL_Object;

class MPLS_Classifier {
	NetAddress destAddress;
	uint32 refCount;
	friend class MPLS;
	DECLARE_ORDER(MPLS_Classifier)
	MPLS_Classifier( const NetAddress& d ) : destAddress(d), refCount(1) {}
public:
	uint32 getHash( uint32 x ) const { return destAddress.getHashValue(x); }
};
IMPLEMENT_ORDER1(MPLS_Classifier,destAddress)

template <> struct Less<MPLS_Classifier*> {
	bool operator()( const MPLS_Classifier* f1, const MPLS_Classifier* f2 ) const {
		return *f1 < *f2;
	}
};

template <> struct GetHash<MPLS_Classifier*> {
	inline uint32 operator()( const MPLS_Classifier* f, uint32 hashCount ) const {
		return f->getHash( hashCount );
	}
};

class MPLS_InLabel {
	uint32 label;
	uint8 labelCType;
	uint32 handle;			// unused (Wisc) resp. port (Camb)
	bool egressBinding;
	friend class MPLS;
	MPLS_InLabel( uint32 label, uint8 C_Type = 2) : label(label), labelCType(C_Type), egressBinding(false) {}
public:
	uint32 getLabel() const { return label; }
	uint8 getLabelCType() const { return labelCType; }
	void setLabelCType(uint8 t) { labelCType = t; } 
#if defined(MPLS_CAMBRIDGE)
	uint32 getPort() const { return handle; }
#endif
};

class MPLS_OutLabel {
	uint32 label;
	uint8 labelCType;
	uint32 handle;			// iface sys index (Wisc) resp. port (Camb)
	const MPLS_Classifier* filter;
	friend class MPLS;
	MPLS_OutLabel( uint32 label, uint8 C_Type = 2) : label(label), labelCType(C_Type), filter(NULL) {}
public:
	uint32 getLabel() const { return label; }
	uint8 getLabelCType() const { return labelCType; }
	void setLabelCType(uint8 t) { labelCType = t; } 
#if defined(MPLS_WISCONSIN)
	uint32 getLifSysIndex() const { return handle; }
#elif defined(MPLS_CAMBRIDGE)
	uint32 getPort() const { return handle; }
#endif
};

struct ExplicitRoute : public NetAddress {
	SimpleList<NetAddress> anList;
	uint32 sessionID;	//@@@@
	ExplicitRoute( const NetAddress& a = 0, const uint32 sid = 0 ) : NetAddress(a), sessionID (sid) {}
};
ostream& operator<<( ostream&, const ExplicitRoute& );

typedef SortedList<ExplicitRoute,NetAddress> ExplicitRouteList;

class MPLS {
	static const uint32 filterHashSize;
	static const uint32 minLabel;
	static const uint32 maxLabel;
	uint32 labelSpaceNum;
	uint32 labelSpaceBegin;
	uint32 labelSpaceEnd;
	uint32 currentLabel;
	uint32 numberOfAllocatedLabels;
	uint32* labelHash;
	SortableHash<MPLS_Classifier*> ingressClassifiers;

	ExplicitRouteList erList;

#if defined(MPLS_WISCONSIN)
	int netlink;
#elif defined(MPLS_CAMBRIDGE)
	inline void arpLookup( const LogicalInterface&, const NetAddress&, char r_addr[] );
#endif

	inline uint32 allocateInLabel();
	inline void freeInLabel( uint32 label );
	inline MPLS_Classifier* internCreateClassifier( const SESSION_Object&, const SENDER_Object&, uint32 );
	inline void internDeleteClassifier( const MPLS_Classifier* );
public:
	MPLS( uint32 labelSpaceNum = 0, uint32 labelSpaceBegin = 0, uint32 labelSpaceEnd = 0 );
	~MPLS();
	bool init();
	void handleOutLabel( OIatPSB&, uint32 label, const Hop& );
	const MPLS_InLabel* setInLabel( PSB& );
	bool bindInAndOut( PSB& psb, const MPLS_InLabel&, const MPLS_OutLabel&, const MPLS* = NULL );
	void createIngressClassifier( const SESSION_Object&, const SENDER_Object&, const MPLS_OutLabel& );
	void createEgressBinding( const MPLS_InLabel&, const LogicalInterface& );
	bool bindUpstreamInAndOut( PSB& psb);
	bool createUpstreamIngressClassifier( const SESSION_Object& session, PSB& psb);
	bool createUpstreamEgressBinding( PSB& psb, const LogicalInterface& lif);
	void deleteOutLabel( const MPLS_OutLabel* );
	void deleteInLabel( PSB& psb, const MPLS_InLabel* );
	void deleteUpstreamInLabel(PSB& psb);
	void deleteUpstreamOutLabel(PSB& psb);
	uint32 allocUpstreamInLabel() { return allocateInLabel();}
#if defined(MPLS_CAMBRIDGE)
	uint32 createHopInfo( const LogicalInterface&, const NetAddress& );
	void removeHopInfo( uint32 );
#else
	uint32 createHopInfo( const LogicalInterface&, const NetAddress& ) { return 0; }
	void removeHopInfo( uint32 ) {}
#endif
	// TODO: elaborated interface needed (maybe COPS, maybe whatever)
	EXPLICIT_ROUTE_Object* getExplicitRoute( NetAddress& dest );
	EXPLICIT_ROUTE_Object* updateExplicitRoute( const NetAddress& dest, EXPLICIT_ROUTE_Object* = NULL );
	void addExplicitRoute( const NetAddress& dest, const SimpleList<NetAddress>& aList, const uint32 sid = 0 );
	void deleteExplicitRouteBySession( uint32 sid );
};


#endif /* _RSVP_MPLS_h_ */
