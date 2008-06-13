#!/bin/sh
#

PREFIX=/usr/local/dragon

# This script must be run as root.
WHOAMI=`whoami`
if test "$WHOAMI" != "root"; then
  echo '' && \
   echo "You must be root to run this script." && \
   echo ''
  exit 1
fi  

case `uname` in
  *BSD)
    echo This is BSD Unix

    echo '' && \
        echo "installing kom-rsvp..."
    cd kom-rsvp
    gmake install
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake install error!"
	exit 1
    fi

    echo '' && \
        echo "installing zebra..."
    cd ../zebra
    make install
    if test $? != 0; then
	echo "dragon-sw: zebra make install error!"
	exit 1
    fi

    echo '' && \
     echo "dragon-sw install finished."

    echo '' && \
        echo "if log file rotation is desired, please do the following..." && \
        echo "  - install logrotate from /usr/ports/sysutils/logrotate" && \
        echo "  - cron script: utils/logrotate4dragon (run hourly)" && \
        echo "  - copy utils/logrotate4dragon.conf to /usr/local/etc/"
    ;;
  *[lL]inux*|*IX*)
    echo This is some other Unix, likely Linux

    echo '' && \
        echo "installing kom-rsvp..."
    cd kom-rsvp
    # Debian systems install GNU make as 'make' not 'gmake'
    if test -e /etc/debian_version; then
        make install
    else
        gmake install
    fi
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp GNU make install error!"
	exit 1
    fi

    echo '' && \
        echo "installing zebra..."
    cd ../zebra
    make install
    if test $? != 0; then
        echo "dragon-sw: zebra make install error!"
        exit 1
    fi

    echo '' && \
        echo "installing log file rotation scripts..."
    if test -x /etc/cron.hourly; then
        cd ..
        cp utils/logrotate4dragon /etc/cron.hourly
        chmod 744 /etc/cron.hourly/logrotate4dragon
        cp utils/logrotate4dragon.conf /usr/local/etc
    fi

    echo '' && \
     echo "dragon-sw install finished."
    ;;
  *Darwin)
    echo This is Darwin

    echo '' && \
        echo "installing kom-rsvp..."
    cd kom-rsvp
    gmake install
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake install error!"
	exit 1
    fi

    echo '' && \
        echo "installing zebra..."
    cd ../zebra
    make install
    if test $? != 0; then
	echo "dragon-sw: zebra make install error!"
	exit 1
    fi

    echo '' && \
     echo "dragon-sw install finished."

    echo '' && \
        echo "if log file rotation is desired, please do the following..." && \
        echo "  - install logrotate via darwinports (www.darwinports.com)" && \
        echo "  - cron script: utils/logrotate4dragon (run hourly)" && \
        echo "  - configure logrotate to use utils/logrotate4dragon.conf"
    ;;
  *)
    echo Do not know what kind of system this is, do it by hand
    ;;

esac

echo ""
echo "    #######################################################"
echo "    #                                                     #"
echo "    #      Instructions for configuration and running     #"
echo "    #                                                     #"
echo "    #######################################################"
echo ""
echo "Samples of configuratin files have been installed under $PREFIX/etc/."
echo "Before running, customize your configuration files following the samples."
echo ""
echo "on VLSR, you need zebra.conf, ospfd.conf, dragon.conf and RSVPD.conf."
echo "on CSA, you need dragon.conf and RSVPD.conf."
echo ""
echo "After configuration, use $PREFIX/bin/dragon.sh to start the service."
echo ""

