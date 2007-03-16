#!/bin/sh

if test "$DRAGON_PREFIX" = ""; then
    PREFIX=/usr/local/dragon
else
    PREFIX=$DRAGON_PREFIX
fi

ETC_DIR=$PREFIX/etc

ZEBRA_DAEMON=$PREFIX/sbin/zebra
ZEBRA_ARGS="-d -f $ETC_DIR/zebra.conf"

OSPF_DAEMON=$PREFIX/sbin/ospfd
OSPF_ARGS="-d -f $ETC_DIR/ospfd.conf"

RSVP_DAEMON=$PREFIX/bin/RSVPD
RSVP_ARGS="-c $ETC_DIR/RSVPD.conf -d -o /var/log/RSVPD.log -L select"

DRAGON_DAEMON=$PREFIX/bin/dragon
DRAGON_ARGS="-d -f $ETC_DIR/dragon.conf"

NODE_AGENT=$PREFIX/bin/node_agent
NODE_AGENT_ARGS="-d -c $ETC_DIR/node_agent.conf"

# for NARBs we need 2 ospfd instances.  the above
# OSPF_DAEMON/OSPF_ARGS variables are not used for NARBs:

OSPF_INTER_DAEMON=$PREFIX/sbin/ospfd
OSPF_INTER_ARGS="-d -I -P 2614 -f $ETC_DIR/ospfd-inter.conf"

OSPF_INTRA_DAEMON=$PREFIX/sbin/ospfd
OSPF_INTRA_ARGS="-d -P 2604 -f $ETC_DIR/ospfd-intra.conf"

NARB_DAEMON=$PREFIX/bin/run_narb.sh
NARB_ARGS=""

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
                node_agent_pid=`ps axwww | grep -v awk | awk '{if (match($5, ".*/node_agent$")      || $5 == "node_agent") print $1}'`
                narb_pid=`ps   axwww | grep -v awk | awk '{if (match($5, ".*/narb$")        || $5 == "narb")   print $1}'`
                rce_pid=`ps   axwww | grep -v awk | awk '{if (match($5, ".*/rce$")        || $5 == "rce")   print $1}'`
                telnet_pid=`ps   axwww | grep -v awk | awk '{if ($5 == match($5, ".*/telnet$") ||   "telnet")   print $1}'`

                # XXX ugh...there must be a better way to do this...this is a kludge
                # maybe search for OSPF_INTER_ARGS and OSPF_INTRA_ARGS...or OSPF_ARGS?
                ospf_intra_pid=`ps axwww | grep -v awk | awk '{if (match($0, ".*/ospfd-intra")  || match($0, ".*/ospfd.*conf")) print $1}'`
                ospf_inter_pid=`ps axwww | grep -v awk | awk '{if (match($0, ".*/ospfd-inter")) print $1}'`
                ;;
        *)
                zebra_pid=""
                ospf_intra_pid=""
                ospf_inter_pid=""
                rsvp_pid=""
                dragon_pid=""
                node_agent_pid=""
                narb_pid=""
                rce_pid=""
                telnet_pid=""
                ;;
esac

#
# Start or stop the daemons based upon the first argument to the script.
#

case $1 in
    start-vlsr | startvlsr | restart-vlsr)
        echo "dragon-sw: turning on coredump with unlimited core size."
        ulimit -c unlimited

        # XXX for CLI based switch control only
        if test "$telnet_pid" != ""; then
	    killall -9 telnet
	fi

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
        if test "$ospf_intra_pid" != ""; then
	    kill $ospf_intra_pid
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
        echo "dragon-sw: turning on coredump with unlimited core size."
        ulimit -c unlimited

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

        if test "$node_agent_pid" != ""; then
            kill $node_agent_pid
        fi
	$NODE_AGENT $NODE_AGENT_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start node agent."
	    exit 1
	fi
	echo "dragon-sw: started node agent."
	;;

    start-narb | startnarb | restart-narb)
        echo "dragon-sw: turning on coredump with unlimited core size."
        ulimit -c unlimited

        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	fi
	$ZEBRA_DAEMON $ZEBRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start zebra daemon."
	    exit 1
	fi
	echo "dragon-sw: started zebra daemon."

        if test "$ospf_inter_pid" != ""; then
	    kill $ospf_inter_pid
	fi
	$OSPF_INTER_DAEMON $OSPF_INTER_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start ospf inter-domain daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf inter-domain daemon."

        if test "$ospf_intra_pid" != ""; then
	    kill $ospf_intra_pid
	fi
	$OSPF_INTRA_DAEMON $OSPF_INTRA_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start ospf intra-domain daemon."
	    exit 1
	fi
	echo "dragon-sw: started ospf intra-domain daemon."

        echo "sleeping for 10 seconds before starting narb (rce & narb daemons)...please stand by."
        sleep 10
    
	$NARB_DAEMON $NARB_ARGS
	if test $? != 0; then
	    echo "dragon-sw: unable to start narb daemon."
	    exit 1
	fi
	echo "dragon-sw: started narb daemons."
	;;
    
    stop)   
        if test "$telnet_pid" != ""; then
	    killall -9 telnet
	fi

        if test "$zebra_pid" != ""; then
	    kill $zebra_pid
	    echo "dragon-sw: stopped zebra daemon."
	fi

        if test "$rsvp_pid" != ""; then
	    kill $rsvp_pid
	    echo "dragon-sw: stopped rsvp daemon."
	fi

        if test "$ospf_intra_pid" != ""; then
	    kill $ospf_intra_pid
	    echo "dragon-sw: stopped intra-domain ospf daemon."
	fi

        if test "$ospf_inter_pid" != ""; then
	    kill $ospf_inter_pid
	    echo "dragon-sw: stopped inter-domain ospf daemon."
	fi

        if test "$dragon_pid" != ""; then
	    kill $dragon_pid
	    echo "dragon-sw: stopped dragon daemon."
	fi

        if test "$node_agent_pid" != ""; then
	    kill $node_agent_pid
	    echo "dragon-sw: stopped node agent."
	fi

        if test "$narb_pid" != ""; then
	    kill $narb_pid
	    echo "dragon-sw: stopped narb daemon."
	fi

        if test "$rce_pid" != ""; then
	    kill $rce_pid
	    echo "dragon-sw: stopped rce daemon."
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

        if test "$ospf_inter_pid" != ""; then
	    echo "dragon-sw: inter-domain ospf daemon is running, pid=$ospf_inter_pid."
	fi

        if test "$ospf_intra_pid" != ""; then
	    echo "dragon-sw: intra-domain ospf daemon is running, pid=$ospf_intra_pid."
	fi

        if test "$dragon_pid" != ""; then
	    echo "dragon-sw: dragon daemon is running, pid=$dragon_pid."
	else
	    echo "dragon-sw: dragon daemon is NOT running."
	fi

        if test "$node_agent_pid" != ""; then
	    echo "dragon-sw: node agent is running, pid=$node_agent_pid."
	else
	    echo "dragon-sw: node agent is NOT running."
	fi

        if test "$narb_pid" != ""; then
	    echo "dragon-sw: narb daemon is running, pid=$narb_pid."
	else
	    echo "dragon-sw: narb daemon is NOT running."
	fi

        if test "$rce_pid" != ""; then
	    echo "dragon-sw: rce daemon is running, pid=$narb_pid."
	else
	    echo "dragon-sw: rce daemon is NOT running."
	fi
	;;
    
    *)
        echo "Usage: $0 {start-vlsr|restart-vlsr|start-uni|restart-uni|start-narb|restart-narb|status|stop}"
	exit 1
	;;
esac

#
# Exit with no errors.
#

exit 0
