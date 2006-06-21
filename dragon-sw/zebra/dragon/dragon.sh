#!/bin/sh

PREFIX=/usr/local/dragon
ETC_DIR=$PREFIX/etc

ZEBRA_DAEMON=$PREFIX/sbin/zebra
ZEBRA_ARGS="-d -f $ETC_DIR/zebra.conf"

OSPF_DAEMON=$PREFIX/sbin/ospfd
OSPF_ARGS="-d -f $ETC_DIR/ospfd.conf"

RSVP_DAEMON=$PREFIX/bin/RSVPD
RSVP_ARGS="-c $ETC_DIR/RSVPD.conf -d -o /var/log/RSVPD.log"

DRAGON_DAEMON=$PREFIX/bin/dragon
DRAGON_ARGS="-d -f $ETC_DIR/dragon.conf"

#
# need to have a way for 'stop' to recognize/kill both ospfd instances...
#

#
# See which daemons are already running...
#

case "`uname`" in
        Linux* | *BSD* | Darwin*)
                zebra_pid=`ps  axwww | grep -v awk | awk '{if (match($5, ".*/zebra$")       || $5 == "zebra")  print $1}'`
                rsvp_pid=`ps   axwww | grep -v awk | awk '{if (match($5, ".*/RSVPD$")       || $5 == "RSVPD")  print $1}'`
                dragon_pid=`ps axwww | grep -v awk | awk '{if (match($5, ".*/dragon$")      || $5 == "dragon") print $1}'`

                # XXX ugh...there must be a better way to do this...this is a kludge
                # maybe search for OSPF_INTER_ARGS and OSPF_INTRA_ARGS...or OSPF_ARGS?
                ospfd_pid=`ps axwww | grep -v awk | awk '{if (match($0, ".*/ospfd.conf")) print $1}'`
                ;;
        *)
                zebra_pid=""
                ospfd_pid=""
                rsvp_pid=""
                dragon_pid=""
                ;;
esac

#
# Start or stop the daemons based upon the first argument to the script.
#

case $1 in
    start-vlsr | startvlsr | restart-vlsr)
	echo "dragon-sw: starting under VLSR mode."
	echo ""
        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	fi
	$ZEBRA_DAEMON $ZEBRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start zebra daemon."
	    exit 1
	fi
	echo "dragon-sw: started zebra daemon."
    
        # XXX again, a bit of a hack here...
        if test "$ospfd_pid" != ""; then
	    kill $ospfd_pid
	fi
	$OSPF_DAEMON $OSPF_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start ospf daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf daemon."
    
        if test "$rsvp_pid" != ""; then
	    kill $rsvp_pid
	fi
	$RSVP_DAEMON $RSVP_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start rsvp daemon."
	    exit 1
	fi
	echo "dragon-sw: started rsvp daemon."
    
        echo "sleeping for 2 seconds before starting dragon daemon..."
        sleep 2

        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	fi
	$DRAGON_DAEMON $DRAGON_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start dragon daemon."
	    exit 1
	fi
	echo "dragon-sw: started dragon daemon."
	;;

    # XXX running software in uni mode w/o zebra & ospfd
    start-uni | startuni | restart-uni)
	echo "dragon-sw: starting under UNI mode."
	echo ""
        if test "$rsvp_pid" != ""; then
	    kill $rsvp_pid
	fi
	$RSVP_DAEMON $RSVP_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start rsvp daemon."
	    exit 1
	fi
	echo "dragon-sw: started rsvp daemon."
    
        echo "sleeping for 2 seconds before starting dragon daemon..."
        sleep 2

        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	fi
	$DRAGON_DAEMON $DRAGON_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start dragon daemon."
	    exit 1
	fi
	echo "dragon-sw: started dragon daemon."
	;;
 
    stop)   
        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	    echo "dragon-sw: stopped zebra daemon."
	fi

        if test "$rsvp_pid" != ""; then
	    kill $rsvp_pid
	    echo "dragon-sw: stopped rsvp daemon."
	fi

        if test "$ospfd_pid" != ""; then
	    kill $ospfd_pid
	    echo "dragon-sw: stopped intra-domain ospf daemon."
	fi

        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	    echo "dragon-sw: stopped dragon daemon."
	fi

	;;
    
    status) 
        if test "$zebra_pid" != ""; then
	    echo "dragon-sw: zebra daemon is running, pid=$zebra_pid."
	else
	    echo "dragon-sw: zebra daemon is NOT running."
	fi

        if test "$rsvp_pid" != ""; then
	    echo "dragon-sw: rsvp daemon is running, pid=$rsvp_pid."
	else
	    echo "dragon-sw: rsvp daemon is NOT running."
	fi

        if test "$dragon_pid" != ""; then
	    echo "dragon-sw: dragon daemon is running, pid=$dragon_pid."
	else
	    echo "dragon-sw: dragon daemon is NOT running."
	fi

	;;
    
    *)
        echo "Usage: $0 {start-vlsr|restart-vlsr|start-uni|restart-uni|status|stop}"
	exit 1
	;;
esac

#
# Exit with no errors.
#

exit 0
