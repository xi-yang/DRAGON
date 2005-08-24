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
#include "RSVP_TrafficControl.h"
#include "RSVP.h"
#include "RSVP_API_Server.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_RSB.h"
#include "RSVP_Scheduler.h"
#include "RSVP_Session.h"
#include "RSVP_OutISB.h"

const uint8 TrafficControl::E_Police = 1;
const uint8 TrafficControl::M_Police = 2;
const uint8 TrafficControl::B_Police = 4;

TrafficControl::~TrafficControl() {
	if (localAdspec) localAdspec->destroy();
	if (scheduler) delete scheduler;
}
OutISB* TrafficControl::createOutISB( const LogicalInterface& OI ) const {
	return new OutISB( OI );
}

bool TrafficControl::configure( const LogicalInterface& lif ) {
	if (localAdspec) return true;
	if (scheduler) {
		localAdspec = scheduler->init( lif, serviceSupport );
		if (!localAdspec) {
			delete scheduler;
			scheduler = NULL;
		}
	}
	// don't use RSVP::getApiLif, since apiLif is not set there yet
#if defined(WITH_API)
	bool atAPI = ( &lif == RSVP_Global::rsvp->getApiServer().getApiLif() );
#else
	bool atAPI = false;
#endif
	if (!localAdspec) localAdspec = new ADSPEC_Object( atAPI ? 0 : 1, ieee32floatInfinite, 0, sint32Infinite );
	const_cast<ADSPEC_Object*>(localAdspec)->setBreakBitCL( !supportsService( ServiceHeader::Guaranteed ) );
	const_cast<ADSPEC_Object*>(localAdspec)->setBreakBitGS( !supportsService( ServiceHeader::ControlledLoad ) );
	return false;
}

bool TrafficControl::advertise( const ADSPEC_Object& adspec, const ADSPEC_Object*& newAdspec ) {
	if ( localAdspec ) {
		newAdspec = adspec.clone();
		const_cast<ADSPEC_Object*>(newAdspec)->updateBy( *localAdspec );
		if ( !supportsService( ServiceHeader::ControlledLoad ) ) {
			const_cast<ADSPEC_Object*>(newAdspec)->setBreakBitCL( true );
		}
		if ( !supportsService( ServiceHeader::Guaranteed ) ) {
			const_cast<ADSPEC_Object*>(newAdspec)->setBreakBitGS( true );
		}
	} else {
		newAdspec = adspec.borrow();
	}
	return (newAdspec != NULL);
}

TrafficControl::UpdateResult TrafficControl::updateTC( OutISB& oisb,
	const RSB* activeRSB, TrafficControl::UpdateFlag uflag ) {

#if defined(ONEPASS_RESERVATION) 
	oisb.updateOnepassFlags();
#endif

	TrafficControl::UpdateResult result = { false, ERROR_SPEC_Object::Confirmation };
	if ( activeRSB && !supportsService( activeRSB->getFLOWSPEC_Object().getServiceNumber()) ) {
		LOG(1)( Log::TC, "TC: unsupported service class" );
		result.error = ERROR_SPEC_Object::TrafficControlError;
		removeFilterList.clear();
		addFilterList.clear();
		return result;
	}

	RHandle* rHandle = oisb.getRHandle();

	if (scheduler) scheduler->prepareRedo( rHandle );

	FLOWSPEC_Object* effectiveFlowspec = NULL;
	if ( !activeRSB || (uflag == NewRSB && activeRSB->getFLOWSPEC_Object() <= oisb.getEffectiveFlowspec()) ) {
		effectiveFlowspec = const_cast<FLOWSPEC_Object&>(oisb.getEffectiveFlowspec()).borrow();
		activeRSB = NULL;
	} else {
		effectiveFlowspec = new FLOWSPEC_Object;
	}

	const RSB_List& rsbList = oisb.getRSB_List();
	PSB_List effPSB_List;
	// calculation over matching RSBs: effective flowspec, all relevant PSBs
	RSB_List::ConstIterator rsbIter = rsbList.begin();
	for (; rsbIter != rsbList.end(); ++rsbIter ) {
		if ( uflag != RemovedRSB || *rsbIter != activeRSB ) {
			if ( activeRSB ) effectiveFlowspec->LUB( (*rsbIter)->getFLOWSPEC_Object() );
			effPSB_List.union_with( (*rsbIter)->getPSB_List() );
		}
	}

	// check for E_Police flag and sum up effective sender tspecs
	uint8 pFlags = 0;
	FLOWSPEC_Object* LUB_AllFlowspecs = new FLOWSPEC_Object;
	PSB_List::ConstIterator psbIter = effPSB_List.begin();
	for (; psbIter != effPSB_List.end(); ++psbIter ) {
		const PSB* psb = *psbIter;
		uint32 i;
		for ( i = 0; i < (uint32)RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
			if ( i != oisb.getOI().getLIH() && psb->getOutISB(i) ) {
				LUB_AllFlowspecs->LUB( psb->getOutISB(i)->getEffectiveFlowspec() );
			}
		}
	}

	// the following has been deliberately omitted: RFC 220x says to replace
	// tspec part of flowspec by effSenderTSpec if effSenderTSpec is smaller? 
	// I doubt that this is legitimate, because R and r might influence the
	// effective amount of resources (and thus the price), so the receiver(s)
	// should ultimately be in control

	// additional checks for B_Police and M_Police flags
	if ( ! ( *effectiveFlowspec >= *LUB_AllFlowspecs ) ) {
		pFlags |= TrafficControl::B_Police;
	}
	LUB_AllFlowspecs->destroy();
	if ( effPSB_List.size() > 1 ) {
		pFlags |= TrafficControl::M_Police;
	}

	removeFilters();

	const FLOWSPEC_Object* fwdFlowspec = &(oisb.getForwardFlowspec());
	bool tcsbChange = false;
	if ( !effPSB_List.empty() ) {
		if ( activeRSB ) {
		       // re-assure: already tested at the beginning of this function
		       // merging shouldn't change this
           assert( supportsService( effectiveFlowspec->getServiceNumber() ) );
			if ( scheduler ) {
				// call scheduler's admission control and process results
				if ( rHandle == NULL ) {
					rHandle = scheduler->addFlowspec( *effectiveFlowspec, pFlags, fwdFlowspec, *activeRSB, NULL );
					result.changed = (rHandle != NULL);
					// WF session -> only a single wildcard filter is needed
					if ( result.changed && activeRSB->getSession().getStyle() == WF && scheduler ) {
						FHandle* fHandle = scheduler->addFilter( rHandle, activeRSB->getSession(), FILTER_SPEC_Object::anyFilter );
						if ( fHandle ) {
							oisb.setFHandleWF( fHandle );
						} else {
							scheduler->delFlowspec( rHandle );
							rHandle = NULL;
							result.changed = false;
						}
					}
					if ( !result.changed ) {
						result.error = ERROR_SPEC_Object::AdmissionControlFailure;
					} else {
						tcsbChange = true;
						oisb.setRHandle( rHandle );
					}
				} else if ( oisb.getEffectiveFlowspec() != *effectiveFlowspec ) {
					if ( !scheduler->modFlowspec( rHandle, *effectiveFlowspec, pFlags, fwdFlowspec, *activeRSB, uflag, NULL ) ) {
						result.error = ERROR_SPEC_Object::AdmissionControlFailure;
					} else {
						result.changed = (*fwdFlowspec != oisb.getForwardFlowspec());
						tcsbChange = true;
					}
				}
			} else {
				fwdFlowspec = effectiveFlowspec;
				result.changed = (*fwdFlowspec != oisb.getForwardFlowspec());
				LOG(2)( Log::TC, "new local eff. flowspec:", *effectiveFlowspec );
			}
		}
	} else if ( rHandle != NULL ) {
		if ( oisb.getFHandleWF() ) {
			if ( scheduler ) scheduler->delFilter( oisb.getFHandleWF() );
			oisb.clearFHandleWF();
		}
		if ( scheduler ) scheduler->delFlowspec( rHandle );
		oisb.clearRHandle();
	}

	if ( result.error == ERROR_SPEC_Object::Confirmation ) {
		if ( updateFilters() ) {
			if ( activeRSB && !effPSB_List.empty() ) {
				if (tcsbChange) oisb.updateEffectiveFlowspec( *effectiveFlowspec );
				if (result.changed) {
                                                        assert( fwdFlowspec );
					oisb.updateForwardFlowspec( *fwdFlowspec );
				}
			}
		} else {
			if (scheduler) scheduler->redoLastReservation( rHandle );
			result.error = ERROR_SPEC_Object::AdmissionControlFailure;
		}
	}
	effectiveFlowspec->destroy();
          assert( result.error != ERROR_SPEC_Object::Confirmation || (removeFilterList.empty() && addFilterList.empty()) );
	removeFilterList.clear();
	addFilterList.clear();
	return result;
}

void TrafficControl::removeFilters() {
	SimpleList<FHandle*>::Iterator iter = removeFilterList.begin();
	while ( iter != removeFilterList.end() ) {
		if (scheduler) scheduler->delFilter( *iter );
		iter = removeFilterList.erase( iter );
	}
}

bool TrafficControl::updateFilters() {
	SimpleList<OIatPSB*>::Iterator filterIter = addFilterList.begin();
	for ( ; filterIter != addFilterList.end(); ++filterIter ) {
		PSB* psb = (*filterIter)->RelationshipOIatPSB_PSB::followRelationship();
		if (scheduler) {
			TrafficControl::FHandle* fhandle = scheduler->addFilter(
				(*filterIter)->RelationshipOIatPSB_OutISB::followRelationship()->getRHandle(),
				*psb->RelationshipPSB_Session::followRelationship(), *psb );
			if ( fhandle == NULL ) {
goto rollback;
			}
			(*filterIter)->setFHandle( fhandle );
		}
	}
	addFilterList.clear();
	return true;
rollback:
	SimpleList<OIatPSB*>::Iterator rollbackIter = addFilterList.begin();
	for ( ; rollbackIter != filterIter; ++rollbackIter ) {
		if (scheduler) scheduler->delFilter( (*rollbackIter)->getFHandle() );
		(*rollbackIter)->clearFHandle();
	}
	addFilterList.clear();
	return false;
}

ostream& operator<< ( ostream& os, const TrafficControl& tc ) {
	if ( tc.scheduler ) os << *tc.scheduler;
	return os;
}
