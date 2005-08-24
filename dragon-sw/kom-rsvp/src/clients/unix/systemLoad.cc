#include "common.h"
#include "RSVP_List.h"
#include "RSVP_TimeValue.h"
#include <sys/time.h>
#include <iostream>
#include <fstream>

static bool end = false;
static bool tick = false;
static TimeValue period_start;
static TimeValue period_end;
static uint64 counter = 0;

static void exitHandler( int ) {
	end = true;
}

static void alarmHandler( int ) {
	tick = true;
}

struct LoadInfo {
	uint64 count;
	TimeValue timestamp;
	LoadInfo( const uint64& c = 0, const TimeValue& t = TimeValue(0,0) )
		: count(c), timestamp(t) {}
};

typedef SimpleList<LoadInfo> CountList;
static CountList countList;

int main( int argc, char** argv ) {
	TimeValue period(1,0);
	if ( argc > 1 && strcmp(argv[1],"calibrate") ) {
		if ( !strcmp(argv[1],"-?") || !strcmp(argv[1],"-h") ) {
			cout << "usage: " << argv[0] << " <period in secs> [calibrate]" << endl;
			return 0;
		}
		period.getFromFraction( strtod( argv[1], NULL ) );
		argc--;
		argv++;
	}
	installExitHandler( exitHandler );
	installAlarmHandler( alarmHandler );
	getCurrentSystemTime( period_start );
	struct itimerval it;
	it.it_interval.tv_sec = period.tv_sec;
	it.it_interval.tv_usec = period.tv_usec;
	it.it_value.tv_sec = period.tv_sec;
	it.it_value.tv_usec = period.tv_usec;
	CHECK( setitimer( ITIMER_REAL, &it, NULL ) );
	while (!end) {
		if (tick) {
			getCurrentSystemTime( period_end );
			countList.push_back( LoadInfo(uint64(double(counter) / (period_end-period_start).getFractionalValue()), period_end) );
			counter = 0;
			period_start = period_end;
			tick = false;
		}
		counter += 1;
	}
	uint64 sum = 0;
	CountList::ConstIterator iter = countList.begin();
	for ( ; iter != countList.end(); ++iter ) {
		sum += (*iter).count;
	}
	if ( argc > 1 && !strcmp(argv[1],"calibrate") ) {
		ofstream os("/etc/systemLoad.conf");
		if (os.bad()) {
			cerr << "ERROR: cannot write to /etc/systemLoad.conf" << endl;
		} else {
			os << sum / (uint64)(countList.size() * period.getFractionalValue()) << endl;
		}
	}
	uint64 calibratedLoad = 0;
	ifstream is("/etc/systemLoad.conf");
	if ( !is.bad() ) {
		is >> calibratedLoad;
		is.close();
	}
	if ( calibratedLoad == 0 ) {
		cerr << "WARNING: cannot read calibration info for period " << period << " from /etc/systemLoad.conf, using average" << endl;
		calibratedLoad = sum / (uint64)(countList.size() * period.getFractionalValue());
	}
	iter = countList.begin();
	for ( ; iter != countList.end(); ++iter ) {
		cout << (DaytimeTimeValue&)(*iter).timestamp << " " << ( 100 - ((*iter).count*100) / (uint64)(calibratedLoad*period.getFractionalValue()) ) << " % system load" << endl;
	}
}
