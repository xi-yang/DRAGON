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
#ifndef _RSVP_TrafficControl_h_
#define _RSVP_TrafficControl_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_Lists.h"
#include "RSVP_ProtocolObjects.h"

class FLOWSPEC_Object;
class ADSPEC_Object;
class LogicalInterface;
class OutISB;
class BaseScheduler;
class OIatPSB;

class TrafficControl {
	friend ostream& operator<< ( ostream&, const TrafficControl& );

public:
	class FHandle {};
	typedef SimpleList<FHandle*> FilterList;
	class RHandle {};
	struct UpdateResult {
		bool changed;
		ERROR_SPEC_Object::ErrorCode error;
	};
	enum UpdateFlag { NewRSB, ModifiedRSB, RemovedRSB };

	static const uint8 E_Police, M_Police, B_Police;

protected:
	BaseScheduler* scheduler;
	ADSPEC_Object* localAdspec;

	bool policing;
	uint32 serviceSupport;

	SimpleList<OIatPSB*> addFilterList;
	SimpleList<FHandle*> removeFilterList;

	friend class Session;

	void removeFilters();
	bool updateFilters();

public:
	TrafficControl( BaseScheduler* scheduler ) : scheduler(scheduler),
		localAdspec(NULL), policing(false), serviceSupport(~0) {}
	~TrafficControl();

	OutISB* createOutISB( const LogicalInterface& ) const;
	bool configure( const LogicalInterface& );
	bool advertise( const ADSPEC_Object&, const ADSPEC_Object*& );
	UpdateResult updateTC( OutISB&, const RSB*, UpdateFlag );
	void removeFilter( FHandle* fHandle ) { removeFilterList.push_back( fHandle ); }
	void addFilter( OIatPSB* relOI ) { addFilterList.push_back( relOI ); }
	bool doesPolicing() { return policing; }
	bool supportsService( uint8 svcNr ) { return serviceSupport & (1 << svcNr); }
};

#endif /* _RSVP_TrafficControl_h_ */
