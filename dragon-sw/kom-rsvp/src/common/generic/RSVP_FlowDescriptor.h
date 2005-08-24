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
#ifndef _RSVP_FlowDescriptor_h_
#define _RSVP_FlowDescriptor_h_ 1

#include "RSVP_IntServObjects.h"
#include "RSVP_FilterSpecList.h"

class FlowDescriptor {
	const FLOWSPEC_Object* flowspec;
	friend ostream& operator<< ( ostream&, const FlowDescriptor& );
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const FlowDescriptor& );
public:
	FilterSpecList filterSpecList;
public:
	FlowDescriptor( const FLOWSPEC_Object* flowspec = NULL ) : flowspec(flowspec) {}
	FlowDescriptor( const FlowDescriptor& f )
	: flowspec(f.flowspec), filterSpecList(f.filterSpecList) {
		if (flowspec) flowspec->borrow();
	}
	~FlowDescriptor() { if (flowspec) flowspec->destroy(); }
	FlowDescriptor& operator=( const FlowDescriptor& f ) {
		if (flowspec) flowspec->destroy();
		flowspec = f.flowspec;
		if (flowspec) flowspec->borrow();
		filterSpecList = f.filterSpecList;
		return *this;
	}
	void setFlowspec( const FLOWSPEC_Object* flowspec ) {
		if (this->flowspec) this->flowspec->destroy();
		this->flowspec = flowspec;
		if (flowspec) flowspec->borrow();
	}
	const FLOWSPEC_Object* getFlowspec() const {
		return flowspec;
	}
	bool operator!= ( const FlowDescriptor& ) const;
};

typedef SimpleList<FlowDescriptor> FlowDescriptorList;

DEDICATED_LIST_MEMORY_MACHINE(FlowDescriptor,FlowDescriptorList,flowDescListMemMachine)

#endif /* _RSVP_FlowDescriptor_h_ */
