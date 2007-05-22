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
%{

#include "tg_classes.h"

using namespace TG;

extern int yyparse();
extern int yylex();
extern int yylineno;
extern void yyerror(const char*);
extern FILE* yyin;
extern String yy_string;
extern uint32 yy_int;
extern ieee32float_p yy_float;

static String yy_ip_address;
static TimeValue yy_timeval;
static String yy_name;
static RSVP_API* api = NULL;
static TSpec tspec;
static TimeValue rsvpSignalWaitTime;
static TimeValue rsvpSyncBackoffTime;
static RSpec rspec;
static bool stamp,rsvp,flowMapping;
static uint32 flowCount, createCount;
static NetAddress localAddr,remoteAddr;
static uint16 localPort, remotePort;
static uint32 packetRate,packetSize,totalSize,sourceCount;
static double hurstParameter,wtpFactor;
static TimeValue flowArrival,flowDuration;
static enum FlowType { tg_UDP, tg_TCP, tg_TCP_Server } flowType;
static enum SenderType { tg_CBR, tg_TRACE, tg_PARETO, tg_GREEDY } senderType;
static ActionObject* action = NULL;
static Flow* flow = NULL;
static Flow** flows = NULL;
static RateEstimator* rateEstimator = NULL;
static DelayCounter* delayCounter = NULL;
static PacketCounter* packetCounter = NULL;
static SimpleList<uint32> varList;
static TimeValue varTime, cbrTime, connectTime;
static bool fixedArrival, fixedDuration;
static bool syntaxError = true, fatalError = false;
static const char* configfile = NULL;

static void createSummaryObject( SummaryObject& so ) {
	if ( traffgen->getSummaryObject( so.getName() ) ) {
		cerr << "ERROR: summary object " << so.getName() << "already exists" << endl;
		exit(1);
	}
	traffgen->addSummaryObject( so );
}

static SummaryObject* findSummaryObject( const String& name ) {
	SummaryObject* so = traffgen->getSummaryObject( name );
	if ( !so ) {
		cerr << "WARNING: cannot find summary object named " << name << endl;
	}
	return so;
}

struct Alias {
	String name;
	String addr;
	Alias( const String& n = "", const String& a = "" ) : name(n), addr(a) {}
};
static SimpleList<Alias> aliasList;

static void findAlias( const String& name ) {
	SimpleList<Alias>::ConstIterator iter = aliasList.begin();
	for ( ; iter != aliasList.end(); ++iter ) {
		if ( (*iter).name == name ) {
			yy_ip_address = (*iter).addr;
			return;
		}
	}
	yy_ip_address = name;
}

static void cleanup() {
	flows = NULL;
	flow = NULL;
	action = NULL;
}
static void doSendFlow() {
	if ( stamp ) flow->enableSenderTimestamping();
	if ( rsvp && api ) reinterpret_cast<Sender*>(action)->enableSignallingRSVP( *api, tspec, rsvpSignalWaitTime, rsvpSyncBackoffTime );
	if ( rateEstimator ) flow->attachSendingRateEstimator( *rateEstimator );
	if ( packetCounter ) flow->attachSendPacketCounter( *packetCounter );
}
static void doReceiveFlow() {
	if ( stamp ) flow->enableReceiverTimestamping();
	if ( rsvp && api ) flow->signalReceiverRSVP( *api, rspec, wtpFactor );
	if ( rateEstimator ) flow->attachReceivingRateEstimator( *rateEstimator );
	if ( delayCounter ) flow->attachDelayCounter( *delayCounter );
	traffgen->addFlow( *flow );
}
static void finishFlowset() {
	uint32 i = 0;
	for ( ; i < flowCount; ++i ) {
		traffgen->addFlowHousekeeping( *flows[i] );
	}
	delete [] flows;
	flows = NULL;
}
static void doReceiveFlowset() {
	uint32 i = 0;
	for ( ; i < flowCount; ++i ) {
		traffgen->addFlow( *flows[i] );
		if ( stamp ) flows[i]->enableReceiverTimestamping();
		if ( rsvp && api ) {
			flows[i]->signalReceiverRSVP( *api, rspec, wtpFactor );
			if (i/10 == 0) usleep(1000);
		}
		if ( rateEstimator ) flows[i]->attachReceivingRateEstimator( *rateEstimator );
		if ( delayCounter ) flows[i]->attachDelayCounter( *delayCounter );
	}
}
static void doSendFlowset() {
	ActionSet* as = new ActionSet( createCount, flowArrival, flowDuration, fixedArrival, fixedDuration );
	uint32 i = 0;
	uint32 avgFlowCount = (uint32)(flowDuration/flowArrival);
	if ( createCount > flowCount && avgFlowCount * 1.5 > flowCount ) {
		LOG(4)( LogWarning, "WARNING: estimated avg. flow count is", avgFlowCount, "given flow count:", flowCount );
	}
	for ( ; i < flowCount; ++i ) {
                                                             assert(flows[i]);
		Sender* s;
		if ( senderType == tg_CBR ) {
			s = new CbrSender( *flows[i], packetSize, packetRate, TimeValue( 0, 0 ) );
		} else if ( senderType == tg_TRACE ) {
			s = new TraceSender( *flows[i], yy_string );
		} else if ( senderType == tg_PARETO ) {
			s = new ParetoSender( *flows[i], packetSize, packetRate, TimeValue( 0, 0 ), sourceCount, hurstParameter );
		} else {
			s = new GreedySender( *flows[i], packetSize, packetRate, totalSize, TimeValue( 0, 0 ) );
		}
		traffgen->addActionHousekeeping( *s );
		as->addAction( *s );
		if ( stamp ) flows[i]->enableSenderTimestamping();
		if ( rsvp && api ) s->enableSignallingRSVP( *api, tspec, rsvpSignalWaitTime, rsvpSyncBackoffTime );
		if ( rateEstimator ) flows[i]->attachSendingRateEstimator( *rateEstimator );
		if ( packetCounter ) flows[i]->attachSendPacketCounter( *packetCounter );
	}
	if ( flowMapping && rsvp && api ) {
		Flowset* flowset = new Flowset( localPort, flowCount);
		flowset->flows[0] = flows[0];
		RSVP_API::SessionId id = flows[0]->registerSenderRSVP( *api, flowset );
		for ( i = 1; i < flowCount; ++i ) {
			flows[i]->setRSVP( *api, id );
			flowset->flows[i] = flows[i];
		}
		
	}
	action = as;
}
static void doCreateFlowset() {
	flows = new Flow*[flowCount];
	uint32 i = 0;
	for ( ; i < flowCount; ++i ) {
		switch ( flowType ) {
		case tg_UDP:
			if ( flowMapping ) {
				flows[i] = new UdpFlow( localAddr.rawAddress(), localPort + i, remoteAddr.rawAddress(), remotePort );
			} else {
				flows[i] = new UdpFlow( localAddr.rawAddress(), localPort + i, remoteAddr.rawAddress(), remotePort + i );
			}
			break;
		case tg_TCP:
			if ( flowMapping ) {
				flows[i] = new TcpFlow( localAddr.rawAddress(), remoteAddr.rawAddress(), remotePort, connectTime );
			} else {
				flows[i] = new TcpFlow( localAddr.rawAddress(), remoteAddr.rawAddress(), remotePort + i, connectTime );
			}
			break;
		case tg_TCP_Server:
			flows[i] = new TcpServer( localAddr.rawAddress(), localPort + i );
			break;
		default:
			assert(0);
		}
	}
}
static void createCbrVariation() {
	CbrSender* sender = new CbrSender( *flow, packetSize, packetRate, cbrTime );
	traffgen->addActionHousekeeping( *sender );
	SenderVariation* sv = new SenderVariation( *sender, varTime );
	sv->addPacketRates(varList);
	varList.clear();
	action = sv;
}
static bool checkLocalAddress() {
	if ( !traffgen->findInterface(localAddr.rawAddress()) ) {
		cerr << "ignoring unknown local address: " << localAddr << endl;
		syntaxError = false;
		return false;
	}
	return true;
}

%}

%token FLOW FLOWSET ESTIMATOR DCOUNTER PCOUNTER ALIAS SEED SEND RECV
%token UDP TCP CBR TRACE PARETO GREEDY MAP
%token FIXED REPEAT AT VARY SERVER STAMP RSVP_S WTP RATE DELAY PACKETS SYNC
%token INTEGER FLOAT STRING IP_ADDRESS

%%

program:
	program command		{ if (action) { traffgen->addActionHousekeeping( *action ); traffgen->addAction( *action ); } cleanup(); }
	|
	;

command:
	flow_command		{ traffgen->addFlowHousekeeping( *flow ); }
	| flowset_command	{ finishFlowset(); }
	| alias_command
	| estimator_command
	| dcounter_command
	| pcounter_command
	| SEED INTEGER		{ traffgen->setRandomSeed( yy_int ); }
	| error			{ if (syntaxError) yyerror("error"); else syntaxError = true; }
	;

flow_command:
	FLOW flow_descr flow_send add_actions
	| FLOW ext_flow_descr flow_recv
	| FLOW flow_descr flow_recv
	| FLOW flow_descr flow_recv flow_send add_actions
	| FLOW flow_descr flow_send add_actions flow_recv
	;

ext_flow_descr:
	TCP SERVER local			{ flow = new TcpServer( localAddr.rawAddress(), localPort ); }
	;

flow_descr:
	UDP local remote			{ flow = new UdpFlow( localAddr.rawAddress(), localPort, remoteAddr.rawAddress(), remotePort ); }
	| UDP local remote map INTEGER		{ flow = new UdpFlow( localAddr.rawAddress(), localPort, remoteAddr.rawAddress(), remotePort, yy_int ); }
	| TCP local_addr remote connect_time	{ flow = new TcpFlow( localAddr.rawAddress(), remoteAddr.rawAddress(), remotePort, connectTime ); }
	;

flow_send:
	SEND sendertype send_conf			{ doSendFlow(); }
	;

sendertype:
	CBR packetsize packetrate timeval		{ action = new CbrSender( *flow, packetSize, packetRate, yy_timeval ); }
	| CBR packetsize packetrate timeval VARY vary	{ cbrTime = yy_timeval; createCbrVariation(); }
	| CBR packetsize packetrate STRING		{ action = new CbrSender( *flow, packetSize, packetRate, yy_string ); }
	| TRACE STRING					{ action = new TraceSender( *flow, yy_string ); }
	| PARETO packetsize packetrate timeval srccount hurst	{ action = new ParetoSender( *flow, packetSize, packetRate, yy_timeval, sourceCount, hurstParameter ); }
	| GREEDY packetsize packetrate totalsize timeval { action = new GreedySender( *flow, packetSize, packetRate, totalSize, yy_timeval ); }
	;

flow_recv:
	RECV recv_conf		{ doReceiveFlow(); }
	;

add_actions:
	add_action add_actions
	|
	;

add_action:
	REPEAT INTEGER		{ assert(action); ActionObject* saveAction = action; action = new RepeatAction( *saveAction, yy_int ); traffgen->addActionHousekeeping( *saveAction ); }
	| AT timeval		{ assert(action); ActionObject* saveAction = action; action = new DelayAction( *saveAction, yy_timeval ); traffgen->addActionHousekeeping( *saveAction ); }
	;

flowset_command:
	FLOWSET flowset_descr flowset_send add_actions
	| FLOWSET ext_flowset_descr flowset_recv
	| FLOWSET flowset_descr flowset_recv
	| FLOWSET flowset_descr flowset_recv flowset_send add_actions
	| FLOWSET flowset_descr flowset_send add_actions flowset_recv
	;

ext_flowset_descr:
	flowcount TCP SERVER local				{ flowType = tg_TCP_Server; doCreateFlowset(); }
	;

flowset_descr:
	flowcount UDP local map remote				{ flowType = tg_UDP; doCreateFlowset(); }
	| flowcount TCP local_addr map remote connect_time	{ flowType = tg_TCP; doCreateFlowset(); }
	;

flowset_send:
	SEND CBR packetsize packetrate fs_conf send_conf			{ senderType = tg_CBR; doSendFlowset(); }
	| SEND TRACE STRING fs_conf send_conf					{ senderType = tg_TRACE; doSendFlowset(); }
	| SEND PARETO packetsize packetrate srccount hurst fs_conf send_conf	{ senderType = tg_PARETO; doSendFlowset(); }
	| SEND GREEDY packetsize packetrate totalsize fs_conf send_conf				{ senderType = tg_GREEDY; doSendFlowset(); }
	;

flowset_recv:
	RECV recv_conf	{ doReceiveFlowset(); }
	;

send_conf:
	stamp rate_estimator pcounter rsvp_send { delayCounter = NULL; }
	;

recv_conf:
	stamp rate_estimator dcounter rsvp_recv	{ packetCounter = NULL; }
	;

fs_conf:
	fixed_a arrival fixed_d duration fixed create_count
	;

create_count:
	INTEGER			{ createCount = yy_int; }
	;

flowcount:
	INTEGER			{ flowCount = yy_int; }
	;

local:
	ip_address INTEGER	{ localAddr = yy_ip_address; localPort = yy_int; if ( !checkLocalAddress() ) YYERROR; }
	;

map:
	MAP			{ flowMapping = true; }
	|			{ flowMapping = false; }
	;

remote:
	ip_address INTEGER	{ remoteAddr = yy_ip_address; remotePort = yy_int; }
	;

local_addr:
	ip_address		{ localAddr = yy_ip_address; if ( !checkLocalAddress() ) YYERROR; }
	;

connect_time:
	timeval			{ connectTime = yy_timeval; }
	|			{ connectTime = TimeValue(0,0); }
	;

stamp:
	STAMP			{ stamp = true; }
	|			{ stamp = false; }
	;

rsvp_send:
	RSVP_S rsvpwait tspec sync	{ rsvp = true; }
	| RSVP_S rsvpwait tspec SYNC	{ rsvp = true; rsvpSyncBackoffTime = TimeValue(0,0); }
	|				{ rsvp = false; }
	;

rsvpwait:
	timeval		{ rsvpSignalWaitTime = yy_timeval; }
	;

sync:
	SYNC timeval	{ rsvpSyncBackoffTime = yy_timeval; }
	|		{ rsvpSyncBackoffTime = TimeValue(-1,-1); }
	;

tspec:
	avg depth peak min MTU
	|			{ TSpec x; tspec = x; }
	;

avg:
	FLOAT		{ tspec.set_r(yy_float); }
	| INTEGER	{ tspec.set_r(yy_int); }
	;

depth:
	FLOAT		{ tspec.set_b(yy_float); }
	| INTEGER       { tspec.set_b(yy_int); }
	;

peak:
	FLOAT		{ tspec.set_p(yy_float); }
	| INTEGER	{ tspec.set_p(yy_int); }
	;

min:
	INTEGER		{ tspec.set_m(yy_int); }  
	;

MTU:
	INTEGER		{ tspec.set_M(yy_int); }
	;

rsvp_recv:
	RSVP_S rate slack	{ rsvp = true; wtpFactor = 1.0; }
	| RSVP_S INTEGER	{ rsvp = true; wtpFactor = 1.0; rspec.set_R(0); rspec.set_S(yy_int); }
	| RSVP_S wtp		{ rsvp = true; rspec.set_R(0); rspec.set_S(0); }
	|			{ rsvp = false; }
	;

rate:
	FLOAT           { rspec.set_R(yy_float); }
	| INTEGER	{ rspec.set_R(yy_int); }
	;

slack:
	INTEGER		{ rspec.set_S(yy_int); }  
	;

wtp:
	WTP FLOAT	{ wtpFactor = yy_float; }
	| WTP INTEGER	{ wtpFactor = yy_int; }
	|		{ wtpFactor = 1.0; }
	;

rate_estimator:
	RATE STRING	{ rateEstimator = (RateEstimator*)findSummaryObject( yy_string ); }
	|		{ rateEstimator = NULL; }
	;

dcounter:
	DELAY STRING	{ delayCounter = (DelayCounter*)findSummaryObject( yy_string ); }
	|		{ delayCounter = NULL; }
	;

pcounter:
	PACKETS STRING	{ packetCounter = (PacketCounter*)findSummaryObject( yy_string ); }
	|		{ packetCounter = NULL; }
	;

packetsize:
	INTEGER			{ packetSize = yy_int; }
	;

packetrate:
	INTEGER			{ packetRate = yy_int; }
	;

totalsize:
	INTEGER			{ totalSize = yy_int; }
	;

srccount:
	INTEGER			{ sourceCount = yy_int; }
	;

hurst:
	FLOAT			{ hurstParameter = yy_float; }
	;

vary:
	timeval variation	{ varTime = yy_timeval; }
	;

variation:
	variation INTEGER	{ varList.push_back( yy_int ); }
	| INTEGER		{ varList.push_back( yy_int ); }
	;

arrival:
	timeval			{ flowArrival = yy_timeval; }
	;

duration:
	timeval			{ flowDuration = yy_timeval; }
	;

fixed:
	FIXED			{ fixedArrival = fixedDuration = true; }
	|
	;

fixed_a:
	FIXED			{ fixedArrival = true; }
	|			{ fixedArrival = false; }
	;

fixed_d:
	FIXED			{ fixedDuration = true; }
	|			{ fixedDuration = false; }
	;

timeval:
	INTEGER			{ yy_timeval = TimeValue(yy_int,0); }
	| FLOAT			{ yy_timeval.getFromFraction(yy_float); }
	;

ip_address:
	IP_ADDRESS		{ yy_ip_address = yy_string; }
	| STRING		{ findAlias( yy_string ); }
	;

alias_command:
	ALIAS name ip_address	{ aliasList.push_back( Alias(yy_name,yy_ip_address) ); }
	;

name:
	STRING			{ yy_name = yy_string; }
	;

estimator_command:
	ESTIMATOR STRING timeval	{ createSummaryObject( *new RateEstimator( yy_timeval, yy_string ) ); }
	;

dcounter_command:
	DCOUNTER STRING timeval INTEGER { createSummaryObject( *new DelayCounter( yy_timeval, yy_int, yy_string ) ); }
	;

pcounter_command:
	PCOUNTER STRING			{ createSummaryObject( *new PacketCounter( yy_string ) ); }
	;

%%

void yyerror(const char *s) {
	cerr << s << " in config file: " << configfile << " in line " << yylineno << endl;
	fatalError = true;
}

bool tg_parser_parse( const String& filename, RSVP_API* rapi = NULL ) {
	configfile = filename.chars();
	api = rapi;
	yyin = fopen( filename.chars(), "r" );
	if ( !yyin ) {
		cerr << "ERROR: cannot access configuration file " << filename << endl;
		exit(1);
	} else {
		yyparse();
		fclose( yyin );
	}
	return !fatalError;
}
