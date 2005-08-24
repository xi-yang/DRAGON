/* IMPORTANT NOTICE:
	the configuration of link bandwidth is currently hard-coded as follows:
	signalling bandwidth:  3% of total bandwidth
	reservable bandwidth: 57% of total bandwidth
	default class:        40% of total bandwidth
*/

#include "RSVP_SchedulerCBQ_NS2.h"

#include "RSVP_Global.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_Daemon_Wrapper.h"

#include "tclcl.h"
#include "config.h"

#define FLOW_COUNT_CHUNK 100
void SchedulerCBQ_NS2::updateADSPEC() {
	if ( flowCounter >= flowAdspecCount ) {
		flowAdspecCount += FLOW_COUNT_CHUNK;
	} else if ( flowCounter < flowAdspecCount - FLOW_COUNT_CHUNK ) {
		flowAdspecCount -= FLOW_COUNT_CHUNK;
	} else {
		return;
	}
}

ADSPEC_Object* SchedulerCBQ_NS2::init( const LogicalInterface& lif, uint32 &serviceSupport ) {

	Tcl& tcl = Tcl::instance();
//	tcl.evalf( "%s replace-classifier", linkObj.chars() );

	// create basic classes (need to check the priority values!!)
	tcl.evalf( "%s cbq-create-class none false %f auto 5 3 0", linkObj.chars(), 1.0 );
	root_handle = tcl.result();
	tcl.evalf( "%s cbq-create-class %s true %f auto 7 1 0 [new Queue/DropTail]",
	        linkObj.chars(), root_handle.chars(), 0.40 );
	default_handle = tcl.result();
	tcl.evalf( "%s cbq-create-class %s true %f auto 3 1 0 [new Queue/DropTail]",
	        linkObj.chars(), root_handle.chars(), 0.03 );
	ctl_handle = tcl.result();
	tcl.evalf( "%s cbq-create-class %s true %f auto 5 2 0", linkObj.chars(), root_handle.chars(), 0.57 );
	resv_handle = tcl.result();

	tcl.evalf( "%s cbq-set-defaultclass %s", linkObj.chars(), default_handle.chars() );
	tcl.evalf( "%s cbq-set-ctlclass %s", linkObj.chars(), ctl_handle.chars() );

	serviceSupport = (1 << ServiceHeader::Default) | (1 << ServiceHeader::ControlledLoad);

	availBw = totalBw*0.57;
	adspec = new ADSPEC_Object( 1, availBw, latency, lif.getMTU() );
	updateADSPEC();
	LOG(2)( Log::CBQ, "SchedulerCBQ_NS2: localAdspec:", *adspec );
	return adspec;
}

SchedulerCBQ_NS2::~SchedulerCBQ_NS2() {
}

BaseScheduler::Reservation* SchedulerCBQ_NS2::internalMakeReservation( const RSB& ) {
	return new CBQ_Reservation;
}

bool SchedulerCBQ_NS2::modFlowspec( TrafficControl::RHandle* resv,
	const FLOWSPEC_Object& flowspec, uint8 pFlags,
	const FLOWSPEC_Object*& fwdFlowspec, const RSB&, TrafficControl::UpdateFlag,
	const FLOWSPEC_Object* replaceFlowspec ) {

	// add new class corresponding to parameters and possibly del old class

	ieee32float rate = flowspec.get_r() * 8;
            assert ( flowspec.getServiceNumber() != ServiceHeader::Guaranteed );

	if ( !admitSimple( *reinterpret_cast<CBQ_Reservation*>(resv), rate, (replaceFlowspec ? replaceFlowspec->getEffectiveRate() * 8 : 0) ) ) {
		return false;
	}

	LOG(2)( Log::CBQ, "CBQ: modifying Flowspec", flowspec );
	LOG(3)( Log::CBQ, "CBQ: setting rate to", rate, "bits/s");

	Tcl& tcl = Tcl::instance();
	if ( reinterpret_cast<CBQ_Reservation*>(resv)->rhandle != "" ) {
		// since we can't delete classes, we simply recycle them...
		tcl.evalf( "%s cbq-modify-class %s %f auto", linkObj.chars(),
		        reinterpret_cast<CBQ_Reservation*>(resv)->rhandle.chars(), rate/totalBw );
		LOG(2)( Log::CBQ, "* Modified class *  -> ", tcl.result() );
	} else {
		tcl.evalf( "%s cbq-create-class %s true %f auto 5 1 0 [new Queue/DropTail]",
		        linkObj.chars(), resv_handle.chars(), rate/totalBw );
		reinterpret_cast<CBQ_Reservation*>(resv)->rhandle = tcl.result();
		LOG(2)( Log::CBQ, "* Added class *  -> ", tcl.result() );
		flowCounter += 1;
		updateADSPEC();
	}

	fwdFlowspec = &flowspec;
	return true;
}

void SchedulerCBQ_NS2::delFlowspec( const TrafficControl::RHandle* resv ) {
	// we cannot delete classes using the current CBQ implementation of ns2
	LOG(2)( Log::CBQ, "Deleting CBQ class", reinterpret_cast<const CBQ_Reservation*>(resv)->rhandle );
	delete reinterpret_cast<const CBQ_Reservation*>(resv);
}

bool SchedulerCBQ_NS2::setFilter( TrafficControl::RHandle* resv,
	const SESSION_Object& session, const SENDER_Object& sender,
	SchedulerCBQ_NS2::CBQ_Filter& filter ) {

	nsaddr_t srcNode = reinterpret_cast<RSVP_Daemon_Wrapper*>(RSVP_Global::wrapper)->mapInterfaceToNode( sender.getSrcAddress() );
	nsaddr_t dstNode = reinterpret_cast<RSVP_Daemon_Wrapper*>(RSVP_Global::wrapper)->mapInterfaceToNode( session.getDestAddress() );

	Tcl& tcl = Tcl::instance();
	tcl.evalf( "%s cbq-add-filter %s %i %i %i", linkObj.chars(),
	        reinterpret_cast<CBQ_Reservation*>(resv)->rhandle.chars(), srcNode, dstNode, session.getTunnelId() );

	return true;
}

BaseScheduler::Filter* SchedulerCBQ_NS2::addFilter( TrafficControl::RHandle* resv,
	const SESSION_Object& session, const SENDER_Object& sender ) {

	SchedulerCBQ_NS2::CBQ_Filter* filter = new CBQ_Filter( sender, session, *reinterpret_cast<CBQ_Reservation*>(resv), 0 );
	if ( setFilter( resv, session, sender, *filter ) ) {
		LOG(2)( Log::CBQ, "CBQ: added filter for", sender );
		return filter;
	} else {
		ERROR(2)( Log::Error, "ERROR: CBQ could not add filter for", sender );
		delete filter;
		return NULL;
	}
	return NULL;
}

void SchedulerCBQ_NS2::delFilter( const TrafficControl::FHandle* filter ) {
	const CBQ_Filter* f = reinterpret_cast<const CBQ_Filter*>(filter);

	nsaddr_t srcNode = reinterpret_cast<RSVP_Daemon_Wrapper*>(RSVP_Global::wrapper)->mapInterfaceToNode( f->sender.getSrcAddress() );
	nsaddr_t dstNode = reinterpret_cast<RSVP_Daemon_Wrapper*>(RSVP_Global::wrapper)->mapInterfaceToNode( f->session.getDestAddress() );

	Tcl& tcl = Tcl::instance();
	tcl.evalf( "%s cbq-del-filter %i %i %i", linkObj.chars(), srcNode, dstNode, f->session.getTunnelId() );
	LOG(2)( Log::CBQ, "CBQ: deleted filter for", f->sender );

	delete f;
}
