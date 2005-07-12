#!/bin/sh
#

PREFIX=/usr/local/dragon

# determine a valid path for net-snmp header files
if test -f /usr/local/include/net-snmp/net-snmp-config.h; then
    SNMP_PATH=/usr/local
elif test -f /usr/include/net-snmp/net-snmp-config.h; then
    SNMP_PATH=/usr
else
    echo 'dragon-sw: could not find net-snmp header files -- is Net-SNMP installed?'
    exit 1
fi

case `uname` in
  *BSD)
    echo This is BSD Unix

    echo '' && \
	echo 'configuring kom-rsvp...'
    cd kom-rsvp
    ./configure --prefix=$PREFIX --with-snmp=$SNMP_PATH CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi

    echo '' && \
	echo 'making kom-rsvp...'
    gmake
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake error!"
	exit 1
    fi

    echo '' && \
	echo 'configuring zebra...'
    cd ../zebra
    ./configure --prefix=$PREFIX --enable-dragon CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: zebra configure error!"
	exit 1
    fi

    echo '' && \
	echo 'making zebra...'
    make
    if test $? != 0; then
	echo "dragon-sw: zebra make error!"
	exit 1
    fi

    echo '' && \
	echo 'dragon-sw build finished.'

    echo '' && \
        echo "Now, as root, run 'sh do_install.sh' to complete the installation."
    ;;
  *[lL]inux*|*IX*)
    echo This is some other Unix, likely Linux

    echo '' && \
	echo 'configuring kom-rsvp...'
    cd kom-rsvp
    ./configure --prefix=$PREFIX --with-snmp=$SNMP_PATH CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi

    echo '' && \
	echo 'making kom-rsvp...'
    gmake
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake error!"
	exit 1
    fi

    echo '' && \
	echo 'configuring zebra...'
    cd ../zebra
    ./configure --prefix=$PREFIX --enable-dragon CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
        echo "dragon-sw: zebra configure error!"
        exit 1
    fi

    echo '' && \
	echo 'making zebra...'
    make
    if test $? != 0; then
        echo "dragon-sw: zebra make error!"
        exit 1
    fi

    echo '' && \
	echo 'dragon-sw build finished.'

    echo '' && \
        echo "Now, as root, run 'sh do_install.sh' to complete the installation."
    ;;
  *)
    echo Do not know what kind of system this is, do it by hand
    ;;
esac
