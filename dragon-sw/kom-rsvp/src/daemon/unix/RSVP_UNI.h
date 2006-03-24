/****************************************************************************

DRAGON-UNI header file
Created by Xi Yang @ 03/24/2006
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _RSVP_UNI_H_
#define _RSVP_UNI_H_

class NetAddress;
class LogicalInterface;

class UNI {
public:
	enum Type { UNI_C = 1, UNI_N = 2 } type;
	NetAddress ip_c, ip_n;
	LogicalInterface* ctrlChannel;

	UNI(enum Type t, NetAddress& ipc, NetAddress& ipn, LogicalInterface* lif);
	~UNI() { }
};


#endif
