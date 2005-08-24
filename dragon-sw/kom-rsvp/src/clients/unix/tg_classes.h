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
#ifndef _tg_h_
#define _tg_h_ 1

#include "common.h"
#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"
#include "RSVP_Timer.h"
#include "RSVP_List.h"
#include "RSVP_TimeValue.h"

namespace TG {

class SummaryObject;
typedef SimpleList<SummaryObject*> SummaryObjectList;
class Flow;
typedef SimpleList<Flow*> FlowList;
class ActionObject;
typedef SimpleList<ActionObject*> ActionList;
class Sender;
class StopAction;
typedef SimpleList<StopAction*> StopActionList;
class TrafficGenerator;
typedef void (*Notification)( void* );

} // namespace

extern TG::TrafficGenerator* traffgen;
class Generator;

// reuse logging from RSVPD
static const uint32 LogActions = Log::PC;
static const uint32 LogOther = Log::TC;
static const uint32 LogStats = Log::RSRR;
static const uint32 LogDebug = Log::MPLS;
static const uint32 LogWarning = Log::CBQ;

namespace TG {

template <class T>
class OnceTimer : public BaseTimer {
protected:
	T& object;
	virtual void internalFire();
public:
	OnceTimer( T& o ) : BaseTimer(TimeValue(0,0)), object(o) {}
	OnceTimer( T& o, const TimeValue& t ) : BaseTimer(t), object(o) {}
	void startRel( const TimeValue& t ) {
		alarmTime = t + traffgen->getCurrentTime();
		start();
	}
	void startAbs( const TimeValue& t ) {
		alarmTime = t;
		start();
	}
	void startNow() {
		alarmTime = traffgen->getCurrentTime();
		start();
	}
};

template <class T>
class ManyTimer : public OnceTimer<T> {
protected:
	virtual void internalFire();
public:
	ManyTimer( T& o ) : OnceTimer<T>(o) {}
};

struct Packet {
	TimeValue packetTime;
	uint32 data;
	Packet( double t, int d = 0 ) : data(d) {
		packetTime.getFromFraction(t);
	}
	Packet( const TimeValue& t = TimeValue(0,0), int d = 0 )
	: packetTime(t), data(d) {}
};

struct PacketChunk {
	static const uint32 size = 10000;
	Packet packets[size];
	uint32 nextFree;
public:
	PacketChunk() : nextFree(0) {}
	void addPacket( const Packet& p ) {
		assert( nextFree < size ); packets[nextFree] = p; nextFree += 1;
	}
	bool full() const { return nextFree >= size; }
	uint32 next() const { return nextFree; }
	Packet& operator[] (int i) { return packets[i]; }
	const Packet& operator[] (int i) const { return packets[i]; }
};

typedef SimpleList<PacketChunk> PacketList;

class SummaryObject {
protected:
	String name;
public:
	SummaryObject( const String& n ) : name(n) {}
	virtual ~SummaryObject() {}
	virtual void showSummaryInformation() = 0;
	const String& getName() const { return name; }
};

class PacketCounter : public SummaryObject {
protected:
	uint32 packetCountRSVP;
	uint32 packetCountNoRSVP;
public:
	PacketCounter( const String& n ) : SummaryObject(n), packetCountRSVP(0), packetCountNoRSVP(0) {}
	void storePacket( bool rsvp ) { if (rsvp) packetCountRSVP += 1; else packetCountNoRSVP += 1; }
	virtual void showSummaryInformation();
	bool empty() { return packetCountRSVP == 0 && packetCountNoRSVP == 0; }
	uint32 getPacketCountRSVP() const { return packetCountRSVP; }
	uint32 getPacketCountNoRSVP() const { return packetCountNoRSVP; }
};

class RateEstimator : public SummaryObject {
	PacketList avgRateList;
	TimeValue avgInterval;
	TimeValue slotEnd;
	uint32 byteCounter;
	uint32 packetCounter;
	bool delayEstimator;
public:
	RateEstimator( const TimeValue& ai, const String& n, bool delay = false )
		: SummaryObject(n), avgInterval(ai), byteCounter(0), packetCounter(0),
			delayEstimator(delay) {
		avgRateList.push_back( PacketChunk() );
	}
	void storePacket( const TimeValue& currentTime, uint32 length );
	virtual void showSummaryInformation();
};

class DelayCounter : public PacketCounter {
	uint32* hashBins;
	uint32 hashBinCount;
	TimeValue hashBinSize;
	TimeValue maxDelay;
	RateEstimator* delayEstimator;
public:
	DelayCounter( const TimeValue& md, uint32 c, const String& n, const TimeValue& ai = TimeValue(0,0) )
		: PacketCounter(n), hashBins(NULL), hashBinCount(c), hashBinSize(md/c), delayEstimator(NULL) {
		hashBins = new uint32[hashBinCount+1];
		for ( uint32 i = 0; i <= hashBinCount; i += 1 ) hashBins[i] = 0;
		if (ai != TimeValue(0,0)) delayEstimator = new RateEstimator( ai, n, true );
	}
	virtual ~DelayCounter() { delete [] hashBins; if (delayEstimator) delete delayEstimator; }
	void storePacket( const TimeValue& delay, bool rsvp ) {
		uint32 slot = delay/hashBinSize;
		if ( slot > hashBinCount ) slot = hashBinCount;
		hashBins[slot] += 1;
		PacketCounter::storePacket( rsvp );
		if (delay > maxDelay) maxDelay = delay;
		if (delayEstimator) delayEstimator->storePacket( getCurrentSystemTime(), delay.tv_usec );
	}
	virtual void showSummaryInformation();
};

struct Flowset {
	uint16 basePort;
	uint16 portRange;
	Flow** flows;
	Flowset( uint16 bp, uint16 pr ) : basePort(bp), portRange(pr),
		flows(new Flow*[pr]) {}
	~Flowset() { delete flows; }
};

class Flow {
protected:
	int fd;
	static char buffer[];

	uint32 localAddr;
	uint16 localPort;
	uint32 remoteAddr;
	uint16 remotePort;
	uint8 proto;

	Sender* sender;

	RSVP_API* rsvpAPI;
	RSVP_API::SessionId rsvpSession;
	bool ownRSVP;
	RSpec rspec;
	double wtpFactor;
	Flowset* flowset;

	uint32 failedPackets;
	bool sendResvSuccess;
	uint32 lastSentSeq;
	RateEstimator* sendingRateEstimator;
	RateEstimator* receivingRateEstimator;
	PacketCounter* sendPacketCounter;
	DelayCounter* delayCounter;
	bool sequencing;

	struct PortStats {
		uint16 port;
		bool recvResvSuccess;
		uint32 lastReceivedSeq;
		uint32 lostPackets;
		uint32 duplicatePackets;
		uint32 reorderedPackets;
		uint32 delayPoisonedPackets;
		PortStats() : recvResvSuccess(false), lastReceivedSeq(0), lostPackets(0),
			duplicatePackets(0), reorderedPackets(0), delayPoisonedPackets(0) {}
	};
	uint16 portrange;
	PortStats* pstats;

	bool senderStamping;
	bool receiverStamping;
	bool kernelStamping;
	uint32 transportHeaderSize;
	uint32 headerOverhead;

	friend ostream& operator<< ( ostream& os, const Flow& f );

	void getLocalAddressing();
	void getRemoteAddressing();
	void bind();
	void connect();
	static void rsvpUpcallDispatcher( const GenericUpcallParameter& upcallPara, Flowset* flows );
	static void rsvpUpcall( const GenericUpcallParameter& upcallPara, Flow* This );
	void setTransportHeaderSize( uint32 t ) {
		transportHeaderSize = t;
		headerOverhead = ETHER_HEADER_SIZE + IP_HEADER_SIZE + transportHeaderSize;
	}

public:
	Flow( uint32 la, uint16 lp, uint32 ra, uint16 rp, uint8 p, uint16 pr = 1 );
	virtual ~Flow();
	void cloneFrom( Flow& f );
	int getFD() const { return fd; }
	uint32 getTransportHeaderSize() const { return transportHeaderSize; }
	uint32 getHeaderOverhead() const { return headerOverhead; }

	void setSender( Sender* s ) { assert(headerOverhead); sender = s; }
	bool hasSenderReservationEstablished() { return sendResvSuccess; }
	void enableReceiving();

	RSVP_API::SessionId registerSenderRSVP( RSVP_API& api, Flowset* );
	void unregisterRSVP();
	void signalSenderRSVP( RSVP_API& api, const TSpec& tspec );
	void signalReceiverRSVP( RSVP_API& api, const RSpec& r, double w );
	void signalEndRSVP() {
		if ( rsvpSession ) {
			rsvpAPI->releaseSender( rsvpSession, localAddr, localPort, 63 );
			if ( ownRSVP ) unregisterRSVP();
		}
	}
	void setRSVP( RSVP_API& api, const RSVP_API::SessionId& id ) {
		rsvpAPI = &api; rsvpSession = id;
	}
	void enableSenderTimestamping();
	void enableReceiverTimestamping();
	void attachSendingRateEstimator( RateEstimator& re ) {
		sendingRateEstimator = &re;
	}
	void attachReceivingRateEstimator( RateEstimator& re ) {
		receivingRateEstimator = &re;
	}
	void attachSendPacketCounter( PacketCounter& pc ) {
		sendPacketCounter = &pc;
	}
	void attachDelayCounter( DelayCounter& dc ) {
		delayCounter = &dc;
	}

	bool send( uint16 length ) { return send( buffer, length ); }
	virtual bool send( const char* const buffer, uint16 length );
	void showSendStats();
	void resetSend();
	virtual void receive( uint16 length = 65535 );
	virtual void showReceiveStats();
};

class TcpFlow : public Flow {
	OnceTimer<TcpFlow> connector;
	TimeValue connectTime;
public:
	TcpFlow( uint32 ra, uint16 rp, int f );
	TcpFlow( uint32 la, uint32 ra, uint16 rp, TimeValue ct = TimeValue(0,0) );
	inline void startConnection();
	inline void timeout();
	virtual bool send( const char* const buffer, uint16 length );
	virtual void receive( uint16 length = 65535 );
};

class TcpServer : public Flow {
	uint32 acceptedFlows;
protected:
	void listen( uint32 num );
	void accept();
public:
	TcpServer( uint32 la, uint16 lp );
	virtual void TcpServer::receive( uint16 length = 0 );
	virtual void showReceiveStats();
};

class UdpFlow : public Flow {
public:
	UdpFlow( uint32 la, uint16 lp, uint32 ra, uint16 rp, uint16 = 1 );
};

class ActionObject {
protected:
	Notification endNotification;
	void* context;
	bool running;
public:
	ActionObject() : endNotification(NULL), context(NULL), running(false) {}
	virtual ~ActionObject() {}
	virtual void start();
	virtual void stop();
	void registerNotification( Notification n, void* c ) {
		endNotification = n; context = c;
	}
	bool isRunning() { return running; }
	virtual void Print( ostream& os ) const = 0;
};

extern inline ostream& operator<< ( ostream& os, const ActionObject& a ) {
	a.Print(os);
	return os;
}

template <class T>
class TimeoutTimerRSVP : public OnceTimer<T> {
public:
	TimeoutTimerRSVP( T& o ) : OnceTimer<T>(o) {}
	virtual void internalFire();
};

class Sender : public ActionObject {
protected:
	ManyTimer<Sender> timer;
	TimeoutTimerRSVP<Sender> rsvpTimer;
	bool rsvpWaiting;
	Flow& flow;
	TimeValue startTime;
	TimeValue endTime;
	uint32 packetSize;
	TimeValue nextPacketTime;
	RSVP_API* rapi;
	TSpec tspec;
	TimeValue rsvpTime;
	TimeValue syncTime;
	virtual bool getNextPacket( bool sendSuccess ) = 0;
	static char buffer[];
public:
	Sender( Flow& f ) : timer(*this), rsvpTimer(*this), rsvpWaiting(false),
		flow(f), startTime(0,0), endTime(0,0), packetSize(0), nextPacketTime(0,0),
		rapi(NULL), rsvpTime(0,0), syncTime(0,0) {}
	virtual ~Sender() {}
	void startSending();
	virtual void start();
	virtual void stop();
	virtual void calculateTSpec() = 0;
	void suspend();
	void resume();
	void enableSignallingRSVP( RSVP_API& r, const TSpec& ts, const TimeValue& t, const TimeValue& s = TimeValue(-1,-1) );
	inline TimeValue timeout();
	virtual void Print( ostream& os ) const;
	inline void rsvpTimeout();
};

class TraceSender : public Sender {
	PacketList packetList;
	PacketList::Iterator iter;
	uint32 nextField;
	String filename;
protected:
	virtual bool getNextPacket( bool sendSuccess );
public:
	TraceSender( Flow& f, const String& filename );
	virtual void start();
	virtual void stop();
	virtual void calculateTSpec();
	virtual void Print( ostream& os ) const;
};

class CbrSender : public Sender {
	ifstream* inputFile;
	TimeValue interPacketTime;
	TimeValue duration;
	uint32 packetLength;
protected:
	virtual bool getNextPacket( bool sendSuccess );
public:
	CbrSender( Flow& f, uint32 pl, uint32 packetRate, const TimeValue& d )
	: Sender(f), inputFile(NULL), interPacketTime(TimeValue(1,0)/packetRate),
		duration(d), packetLength(pl) {}
	CbrSender( Flow& f, uint32 pl, uint32 packetRate, const String& filename )
	: Sender(f), inputFile(new ifstream(filename.chars())),
		interPacketTime(TimeValue(1,0)/packetRate), duration(0,0), packetLength(pl) {}
	virtual ~CbrSender() { if (inputFile) delete inputFile; }
	virtual void start();
	virtual void stop();
	virtual void calculateTSpec();
	void setPacketLength( uint32 pl ) { packetSize = packetLength = pl; }
	void setInterPacketTime( const TimeValue& ipt ) { interPacketTime = ipt; }
	void setDuration( const TimeValue& d ) { duration = d; }
	const TimeValue& getInterPacketTime() { return interPacketTime; }
	virtual void Print( ostream& os ) const;
};

class ParetoSender : public Sender {
	TimeValue duration;
	Generator* AG;
	double load;
	uint32 capacity;
	uint32 avgPacketLength;
protected:
	virtual bool getNextPacket( bool sendSuccess );
public:
	ParetoSender( Flow& f, uint32 packetLength, uint32 packetRate,
		const TimeValue& d, uint32 sourceCount, double hurst,
		uint32 interPacketGap = 8 );
	virtual ~ParetoSender();
	virtual void start();
	virtual void stop();
	virtual void calculateTSpec();
	void setDuration( const TimeValue& d ) { duration = d; }
	virtual void Print( ostream& os ) const;
};

class GreedySender : public Sender {
	TimeValue duration;
	TimeValue interPacketTime;
	uint32 totalSize;
	uint32 remainingSize;
	uint32 packetLength;
protected:
	virtual bool getNextPacket( bool sendSuccess );
public:
	GreedySender( Flow& f, uint32 pl, uint32 packetRate, uint32 ts = 0,
		const TimeValue& d = TimeValue(0,0) )
	: Sender(f), duration(d), interPacketTime(TimeValue(1,0)/packetRate),
		totalSize(ts), remainingSize(ts), packetLength(pl) {}
	virtual ~GreedySender() {}
	virtual void start();
	virtual void stop();
	virtual void calculateTSpec();
	void setDuration( const TimeValue& d ) { duration = d; }
	virtual void Print( ostream& os ) const;
};

class SenderVariation : public ActionObject {
	CbrSender& sender;
	SimpleList<uint32> packetRates;
	SimpleList<uint32>::ConstIterator currentRate;
	RefreshTimer<SenderVariation> timer;
	static inline void notify( SenderVariation *This );
public:
	SenderVariation( CbrSender& s, const TimeValue& period );
	virtual ~SenderVariation() {}
	virtual void start();
	virtual void stop();
	inline void refresh();
	void addPacketRate( uint32 p );
	void addPacketRates( const SimpleList<uint32>& rl );
	virtual void Print( ostream& os ) const;
};

class DelayAction : public ActionObject {
	ActionObject& action;
	TimeValue delay;
	OnceTimer<DelayAction> timer;
	static inline void notify( DelayAction* This );
public:
	DelayAction( ActionObject& o, const TimeValue& t );
	virtual ~DelayAction() {}
	virtual void start();
	virtual void stop();
	inline void timeout();
	virtual void Print( ostream& os ) const;
};

class RepeatAction : public ActionObject {
	ActionObject& action;
	int repeatCount;
	static inline void notify( RepeatAction* This );
public:
	RepeatAction( ActionObject& o, int rc ) : action(o), repeatCount(rc) {}
	virtual ~RepeatAction() {}
	virtual void start();
	virtual void stop();
	inline void repeat();
	virtual void Print( ostream& os ) const;
};

class ActionSet;

class StopAction {
	class ActionSet& actionSet;
	ActionObject& action;
	OnceTimer<StopAction> timer;
	StopActionList::Iterator iter;
	static inline void notify( StopAction* This );
public:
	StopAction( ActionSet& as, ActionObject& o, const TimeValue& d );
	StopAction( ActionSet& as, Sender& o );
	void start() { action.start(); }
	inline void timeout();
	void setIterator( StopActionList::Iterator i ) { iter = i; }
};

class ActionSet : public ActionObject {
	uint32 number;
	static uint32 counter;
	ActionList actions;
	StopActionList stopActions;
	uint32 createCount;
	TimeValue arrival;
	TimeValue duration;
	bool fixedArrival;
	bool fixedDuration;
	TimeValue nextStart;
	ManyTimer<ActionSet> timer;
	uint32 created;
	inline TimeValue getArrival();
	inline TimeValue getDuration();
public:
	ActionSet( uint32 cc, const TimeValue& a, const TimeValue& d, bool fa, bool fd );
	virtual ~ActionSet() {}
	virtual void start();
	virtual void stop();
	void addAction( ActionObject& a );
	inline void removeStopAction( SimpleList<StopAction*>::Iterator iter );
	inline TimeValue timeout();
	virtual void Print( ostream& os ) const;
};

class Housekeeping {
	ActionList actionList;
	FlowList flowList;
	SummaryObjectList summaryObjectList;
public:
	~Housekeeping();
	void addAction( ActionObject& a );
	void addFlow( Flow& f );
	void addSummaryObject( SummaryObject& so );
	SummaryObject* getSummaryObject( const String& name );
	void checkForConnectionFlows();
	void showStats();
};

class TrafficGenerator {
	bool endFlag;
	Flow* flowArray[FD_SETSIZE];
	FlowList flowReportList;
	ActionList actionList;
	int min_fd, max_fd;
	fd_set fdMasterMask;
	int rsvp_fd;
	Notification rsvpNotify;
	RSVP_API* rapi;
	uint32 actionsRunning;
	OnceTimer<TrafficGenerator> starter;
	Housekeeping* housekeeping;
	uint32 packetsSent, packetsSentRSVP;
	uint32 randomSeed;

	inline void setMask( fd_set& fdMask );
public:
	TrafficGenerator( uint32 = 0, uint32 = 0 );
	~TrafficGenerator();
	bool findInterface( uint32 );
	void addFlow( Flow& f );
	void addFlowHousekeeping( Flow& f ) { housekeeping->addFlow( f ); }
	void addAction( ActionObject& a );
	void addActionHousekeeping( ActionObject& a );
	void addSummaryObject( SummaryObject& so );
	SummaryObject* getSummaryObject( const String& s );
	void registerRSVP_API( RSVP_API& ra, Notification rn );
	void unregisterRSVP_API();
	void main( const TimeValue&, const uint32 );
	void finish() { endFlag = true; }
	void setRandomSeed( uint32 rs ) { randomSeed = rs; }
	inline void timeout();
	inline void removeFD( int fd );
	inline void actionStarted();
	inline void actionFinished();
	static inline const TimeValue& getCurrentTime();
};

} // namespace

#endif /* _tg_h_ */
