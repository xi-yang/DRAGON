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
#include "rsvp-tab.h"

static class RSVPTabClassifierClass : public TclClass {
public:
	RSVPTabClassifierClass() : TclClass("Classifier/RSVPTab") {}
	TclObject* create(int, const char*const*) {
		return (new RSVPTabClassifier());
	}
} class_address_classifier;

int RSVPTabClassifier::classify(Packet *p) {
	hdr_cmn* cmnh = hdr_cmn::access(p);

	// Forward all rsvp traffic to local rsvp agent, except for InitAPI-messages
	if (   cmnh->ptype() == PT_RSVP_PATH
	    || cmnh->ptype() == PT_RSVP_PATH_TEAR
	    || cmnh->ptype() == PT_RSVP_RESV_CONF ) {
		return 1;
	}

	return 0;
};
