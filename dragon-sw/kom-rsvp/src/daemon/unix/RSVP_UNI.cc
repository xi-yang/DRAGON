/****************************************************************************

DRAGON-UNI source file
Created by Xi Yang @ 03/24/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#include "RSVP_BasicTypes.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_UNI.h"


UNI::UNI(const NetAddress& ipc, const NetAddress& ipn, const LogicalInterface* lif) : ip_c(ip_c), ip_n(ipn), ctrlChannel(lif)
{

}

