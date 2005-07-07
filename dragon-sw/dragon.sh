
#!/bin/sh

PREFIX=/usr/local

ZEBRA_DAEMON=$PREFIX/sbin/zebra
ZEBRA_ARGS="-d -f /usr/local/etc/zebra.conf"

OSPF_DAEMON=$PREFIX/sbin/ospfd
OSPF_ARGS="-d -f /usr/local/etc/ospfd.conf"

RSVP_DAEMON=$PREFIX/rsvp/bin/RSVPD
RSVP_ARGS="-c /usr/local/etc/RSVPD.conf -d select"

DRAGON_DAEMON=$PREFIX/bin/dragon
DRAGON_ARGS="-d -f /usr/local/etc/dragon.conf"

NARB_DAEMON=$PREFIX/sbin/narb
NARB_ARGS="-d"

# in the case of the narb, start ospfd like this:
#
# ./ospfd -d -I -P 2604 -f ospfd-inter.conf
# ./ospfd -d -P 2614 -f ospfd-intra.conf
# sleep 10
# ./narb -d
#
# need to have a way for 'stop' to kill both ospfd instances...

#
# See which daemons are already running...
#

case "`uname`" in
        Linux* | *BSD* | Darwin*)
                zebra_pid=`ps  ax | awk '{if (match($5, ".*/zebra$")  || $5 == "zebra")  print $1}'`
                ospf_pid=`ps   ax | awk '{if (match($5, ".*/ospfd$")  || $5 == "ospfd")  print $1}'`
                rsvp_pid=`ps   ax | awk '{if (match($5, ".*/RSVPD$")  || $5 == "RSVPD")  print $1}'`
                dragon_pid=`ps ax | awk '{if (match($5, ".*/dragon$") || $5 == "dragon") print $1}'`
                narb_pid=`ps   ax | awk '{if (match($5, ".*/narb$")   || $5 == "narb")   print $1}'`
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
    start-vlsr | startvlsr | restart-vlsr)
        # start the daemons in this order:
        #  zebra ospf rsvp dragon

        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	fi
	$ZEBRA_DAEMON $ZEBRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 zebra daemon."
	    exit 1
	fi
	echo "dragon-sw: started zebra daemon."
    
        if test "$ospf_pid" != ""; then
	    kill $ospf_pid
	fi
	$OSPF_DAEMON $OSPF_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 ospf daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf daemon."
    
        if test "$rsvp_pid" != ""; then
	    kill $rsvp_pid
	fi
	$RSVP_DAEMON $RSVP_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 rsvp daemon."
	    exit 1
	fi
	echo "dragon-sw: started rsvp daemon."
    
        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	fi
	$DRAGON_DAEMON $DRAGON_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 dragon daemon."
	    exit 1
	fi
	echo "dragon-sw: started dragon daemon."
	;;
    
    start-narb | startnarb | restart-narb)
        # start the daemons in this order:
        #  zebra ospf-inter ospfd-intra --sleep 10-- narb

        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	fi
	$ZEBRA_DAEMON $ZEBRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 zebra daemon."
	    exit 1
	fi
	echo "dragon-sw: started zebra daemon."

        if test "$ospf_pid" != ""; then
	    kill $ospf_pid
	fi
	$OSPF_DAEMON $OSPF_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 ospf daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf daemon."

        # XXX need a way to start both OSPF daemons here!
        echo "sleeping for 10 seconds before starting narb...please stand by."
        sleep 10
    
        if test "$narb_pid" != ""; then
	    kill $narb_pid
	fi
	$NARB_DAEMON $NARB_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to $1 narb daemon."
	    exit 1
	fi
	echo "dragon-sw: started narb daemon."
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

        if test "$ospf_pid" != ""; then
	    kill $ospf_pid
	    echo "dragon-sw: stopped ospf daemon."
	fi

        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	    echo "dragon-sw: stopped dragon daemon."
	fi

        if test "$narb_pid" != ""; then
	    kill $narb_pid
	    echo "dragon-sw: stopped narb daemon."
	fi
	;;
    
    status) 
        if test "$zebra_pid" != ""; then
	    echo "dragon-sw: zebra daemon is running."
	else
	    echo "dragon-sw: zebra daemon is NOT running."
	fi

        if test "$rsvp_pid" != ""; then
	    echo "dragon-sw: rsvp daemon is running."
	else
	    echo "dragon-sw: rsvp daemon is NOT running."
	fi

        if test "$ospf_pid" != ""; then
	    echo "dragon-sw: ospf daemon is running."
	else
	    echo "dragon-sw: ospf daemon is NOT running."
	fi

        if test "$dragon_pid" != ""; then
	    echo "dragon-sw: dragon daemon is running."
	else
	    echo "dragon-sw: dragon daemon is NOT running."
	fi

        if test "$narb_pid" != ""; then
	    echo "dragon-sw: narb daemon is running."
	else
	    echo "dragon-sw: narb daemon is NOT running."
	fi
	;;
    
    *)
        echo "Usage: $0 {start-vlsr|restart-vlsr|start-narb|restart-narb|status|stop}"
	exit 1
	;;
esac

#
# Exit with no errors.
#

exit 0
