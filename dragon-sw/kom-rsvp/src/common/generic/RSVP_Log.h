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
#ifndef _RSVP_Log_h_
#define _RSVP_Log_h_ 1

#include "RSVP_System.h"
#include "RSVP_String.h"

class Log {
public:
	enum {
		None =				       0,            // 0x00000000
		Fatal =				(1 << 0),            // 0x00000001
		Error =				(1 << 1),            // 0x00000002
		Timer =				(1 << 2),            // 0x00000004
		Ref =					(1 << 3),            // 0x00000008
		PC =					(1 << 4),            // 0x00000010
		TC =					(1 << 5),            // 0x00000020
		CBQ =					(1 << 6),            // 0x00000040
		Msg =					(1 << 7),            // 0x00000080
		API =					(1 << 8),            // 0x00000100
		Packet =			(1 << 9),            // 0x00000200
		Session =			(1 << 10),           // 0x00000400
		SB =					(1 << 11),           // 0x00000800
		Routing =			(1 << 12),           // 0x00001000
		RSRR =				(1 << 13),           // 0x00002000
		Config =			(1 << 14),           // 0x00004000
		Process =			(1 << 15),           // 0x00008000
		Parser =			(1 << 16),           // 0x00010000
		Select =			(1 << 17),           // 0x00020000
		HFSC =				(1 << 18),           // 0x00040000
		Scheduler =		(1 << 19),           // 0x00080000
		Unused1 =			(1 << 20),           // 0x00100000
		Unused2 =			(1 << 21),           // 0x00200000
		Unused3 =			(1 << 22),           // 0x00400000
		Unused4 =			(1 << 23),           // 0x00800000
		Reduct =			(1 << 24),           // 0x01000000
		MPLS =				(1 << 24),           // 0x02000000
		NS =					(1 << 25),           // 0x04000000
		Short =				(1 << 26),           // 0x08000000
		All = 				~0                   // 0xffffffff
	};
private:
	struct debugOption {
		char* name;
		uint32 level;
		uint32 nchars;
	};
	static debugOption options[];
	static void internalInit( const String&, bool logErrorsInStdLog );
	static void parse( const String& s, bool disable = false );
public:
	static uint32 loglevel;
	static ostream* log;
	static ostream* stdlog;
	static ostream* errlog;
	static bool virtualTime;
	Log( uint32 loglevel = Fatal, const String& filename = "", bool logErrorsInStdLog = false ) {
		init( loglevel, filename, logErrorsInStdLog );
	}
	Log( const String& enable, const String& disable = "", const String& filename = "", bool logErrorsInStdLog = false ) {
		init( enable, disable, filename, logErrorsInStdLog );
	}
	static void init( uint32 loglevel = Fatal|Error, const String& filename = "", bool logErrorsInStdLog = false );
	static void init( const String& enable, const String& disable = "", const String& filename = "", bool logErrorsInStdLog = false );
	static void outInfo( ostream& os );
	static void usage( ostream& os );
	static void close();
};

inline ostream& operator<< ( ostream& os, void (*func)(void) ) {
	func();
	return os;
}

#define LOG_BASE1( level, o1 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << endl; }

#define LOG_BASE2( level, o1, o2 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << endl; }

#define LOG_BASE3( level, o1, o2, o3 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << endl; }

#define LOG_BASE4( level, o1, o2, o3, o4 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << " " << o4 << endl; }

#define LOG_BASE5( level, o1, o2, o3, o4, o5 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << " " << o4 << " " << o5 << endl; }

#define LOG_BASE6( level, o1, o2, o3, o4, o5, o6 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << " " << o4 << " " << o5 << " " << o6 << endl; }

#define LOG_BASE7( level, o1, o2, o3, o4, o5, o6, o7 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << " " << o4 << " " << o5 << " " << o6 << " " << o7 << endl; }

#define LOG_BASE8( level, o1, o2, o3, o4, o5, o6, o7, o8 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << " " << o4 << " " << o5 << " " << o6 << " " << o7 << " " << o8 << endl; }

#define LOG_BASE9( level, o1, o2, o3, o4, o5, o6, o7, o8, o9 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << " " << o4 << " " << o5 << " " << o6 << " " << o7 << " " << o8 << " " << o9 << endl; }

#define LOG_BASE10( level, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1 << " " << o2 << " " << o3 << " " << o4 << " " << o5 << " " << o6 << " " << o7 << " " << o8 << " " << o9 << " " << o10 << endl; }

#define LOG_BASES( level, o1 ) \
	if ( (level & Log::loglevel) && Log::log ) { Log::outInfo( *Log::log ); *Log::log << o1; }

#define LOG_BASEC( level, o1 ) \
	if ( (level & Log::loglevel) && Log::log ) { *Log::log << o1; }


#define LOG_NONE1( level, o1 ) ;
#define LOG_NONE2( level, o1, o2 ) ;
#define LOG_NONE3( level, o1, o2, o3 ) ;
#define LOG_NONE4( level, o1, o2, o3, o4 ) ;
#define LOG_NONE5( level, o1, o2, o3, o4, o5 ) ;
#define LOG_NONE6( level, o1, o2, o3, o4, o5, o6 ) ;
#define LOG_NONE7( level, o1, o2, o3, o4, o5, o6, o7 ) ;
#define LOG_NONE8( level, o1, o2, o3, o4, o5, o6, o7, o8 ) ;
#define LOG_NONE9( level, o1, o2, o3, o4, o5, o6, o7, o8, o9 ) ;
#define LOG_NONE10( level, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10 ) ;
#define LOG_NONES( level, o1 ) ;
#define LOG_NONEC( level, o1 ) ;

#ifdef FATAL_ON
#define FATAL(x)	Log::virtualTime = false; Log::log = Log::errlog; LOG_BASE ## x
#else
#define FATAL(x)	LOG_NONE ## x
#endif

#ifdef ERROR_ON
#define ERROR(x)	Log::virtualTime = false; Log::log = Log::errlog; LOG_BASE ## x
#else
#define ERROR(x)	LOG_NONE ## x
#endif

#ifdef LOG_ON
#define LOG(x)		Log::virtualTime = false; Log::log = Log::stdlog; LOG_BASE ## x
#define LOGV(x)		Log::virtualTime = true;  Log::log = Log::stdlog; LOG_BASE ## x
#else
#define LOG(x)	LOG_NONE ## x
#define LOGV(x)	LOG_NONE ## x
#endif

#endif /* _RSVP_Log_h_ */
