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
#include "RSVP_Scheduler.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_OutISB.h"
#include "RSVP_Log.h"

BaseScheduler::Filter::Filter( const SENDER_Object& sender, const SESSION_Object& session, Reservation& resv )
	: sender(sender), session(session), resv(resv) {
	iter = resv.filterList.insert( resv.filterList.end(), this );	
}

BaseScheduler::Filter::~Filter() {
	resv.filterList.erase( iter );
}

bool BaseScheduler::admitSimple( Reservation& resv, ieee32float rate, ieee32float oldrate ) {
	ieee32float oldRate = resv.bandwidth + oldrate;
	bool retval = true;
	if ( rate != oldRate ) {
		if ( totalBw == 0 || availBw + oldRate >= rate ) {
			LOG(5)( Log::Scheduler, "Scheduler: admitting", rate, "bit/s out of", availBw + oldRate, "bit/s." );
			availBw += oldRate;
			resv.bandwidth = rate;
			availBw -= rate;
		} else {
			LOG(5)( Log::Scheduler, "Scheduler: rejecting", rate, "bit/s, available:", availBw + oldRate, "bit/s." );
			retval = false;
		}
 	}  else {
    LOG(3)( Log::Scheduler, "Scheduler: ignoring unchanged", rate, "bit/s.");
  }
  LOG(3)( Log::Scheduler, "Scheduler: netto available rate:", availBw, "bit/s." );
  return retval;
}

void BaseScheduler::releaseSimple( const Reservation& resv ) {
  LOG(2)( Log::Scheduler, "Scheduler: deleting reservation with rate:", resv.bandwidth );
  availBw += resv.bandwidth;
  LOG(3)( Log::Scheduler, "Scheduler: netto available rate:", availBw, "bit/s." );
}
	
ADSPEC_Object* SchedulerDummy::init( const LogicalInterface& lif, uint32& serviceSupport ) {
	serviceSupport = (1 << ServiceHeader::Default)
									|(1 << ServiceHeader::Guaranteed)
									|(1 << ServiceHeader::ControlledLoad);
	availBw = totalBw;
	ADSPEC_Object* localAdspec = new ADSPEC_Object( 1, availBw, latency, lif.getMTU() );
	LOG(2)( Log::Scheduler, "Scheduler: localAdspec:", *localAdspec );
	return localAdspec;
}

bool SchedulerDummy::modFlowspec( TrafficControl::RHandle* resv, const FLOWSPEC_Object& effFlowspec,
	uint8 pFlags, const FLOWSPEC_Object*& fwdFlowspec, const RSB&,
	TrafficControl::UpdateFlag, const FLOWSPEC_Object* replaceFlowspec ) {

	fwdFlowspec = &effFlowspec;
	return admitSimple( *reinterpret_cast<Reservation*>(resv), effFlowspec.getEffectiveRate() * 8, (replaceFlowspec ? replaceFlowspec->getEffectiveRate() * 8 : 0) );
}

void SchedulerDummy::delFlowspec( const TrafficControl::RHandle* resv ) {
	releaseSimple( *reinterpret_cast<const Reservation*>(resv) );
  delete reinterpret_cast<const Reservation*>(resv);
}

BaseScheduler::Filter* SchedulerDummy::addFilter( TrafficControl::RHandle* resv,
	const SESSION_Object& session, const SENDER_Object& sender ) {

	Filter* newFilter = new Filter( sender, session, *reinterpret_cast<Reservation*>(resv) );
	LOG(4)( Log::Scheduler, "Scheduler: adding filter for:", sender, "->", session );
	return newFilter;
}

void SchedulerDummy::delFilter( const TrafficControl::FHandle* filter ) {
	LOG(4)( Log::Scheduler, "Scheduler: deleting filter for", reinterpret_cast<const Filter*>(filter)->sender, "->", reinterpret_cast<const Filter*>(filter)->session );
	delete reinterpret_cast<const Filter*>(filter);
}
