#!/bin/sh

PREFIX=/usr/local

ZEBRA_DAEMON=$PREFIX/bin/zebra
ZEBRA_ARGS="-d -f /usr/local/etc/zebra.conf"

OSPF_DAEMON=$PREFIX/bin/ospfd
OSPF_ARGS="-d -f /usr/local/etc/ospfd.conf"

RSVP_DAEMON=$PREFIX/rsvp/bin/RSVPD
RSVP_ARGS="-c /usr/local/etc/RSVPD.conf -d select"

DRAGON_DAEMON=$PREFIX/bin/dragon
DRAGON_ARGS="-d -f /usr/local/etc/dragon.conf"

NARB_DAEMON=$PREFIX/bin/narb
NARB_ARGS="-d"

# in the case of the narb, start ospfd like this:
#
# ./ospfd -d -I -P 2604 -f ospfd-inter.conf
# ./ospfd -d -P 2614 -f ospfd-intra.conf
# sleep 10
# ./narb -d

#
# See which daemons are already running...
#

case "`uname`" in
        Linux* | *BSD* | Darwin*)
                zebra_pid =`ps ax | awk '{if (match($5, ".*/zebra$")  || $5 == "zebra")  print $1}'`
                ospf_pid  =`ps ax | awk '{if (match($5, ".*/ospfd$")  || $5 == "ospfd")  print $1}'`
                rsvp_pid  =`ps ax | awk '{if (match($5, ".*/RSVPD$")  || $5 == "RSVPD")  print $1}'`
                dragon_pid=`ps ax | awk '{if (match($5, ".*/dragon$") || $5 == "dragon") print $1}'`
                narb_pid  =`ps ax | awk '{if (match($5, ".*/narb$")   || $5 == "narb")   print $1}'`
                ;;
        *)
                zebra_pid=""
                ospf_pid=""
                rsvp_pid=""
                dragon_pid=""
                narb_pid=""
                ;;
esac

#
# Start or stop the daemons based upon the first argument to the script.
#

case $1 in
    start | restart)
        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	fi

	$DRAGON_DAEMON $DRAGON_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 dragon daemon."
	    exit 1
	fi
	echo "dragon-sw: ${1}ed dragon daemon."
	;;
    
    stop)   
        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	    echo "dragon-sw: stopped dragon daemon."
	fi
	;;
    
    status) 
        if test "$dragon_pid" != ""; then
	    echo "dragon-sw: dragon daemon is running."
	else
	    echo "dragon-sw: dragon daemon is not running."
	fi
	;;
    
    *)
        echo "Usage: $0 {restart|start|status|stop}"
	exit 1
	;;
esac

#
# Exit with no errors.
#

exit 0
