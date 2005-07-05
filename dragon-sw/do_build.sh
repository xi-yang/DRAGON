#!/bin/sh
#

case `uname` in
  *BSD)
    echo This is BSD Unix
    cd kom-rsvp
    ./configure --with-snmp=/usr/local CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi
    gmake
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake error!"
	exit 1
    fi
    cd ../zebra
    ./configure --enable-dragon CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: zebra configure error!"
	exit 1
    fi
    make
    if test $? != 0; then
	echo "dragon-sw: zebra make error!"
	exit 1
    fi

    echo '' && \
     echo 'dragon-sw build finished.'
    ;;
  *[lL]inux*|*IX*)
    echo This is some other Unix, likely Linux
    cd kom-rsvp
    ./configure --with-snmp=/usr/local CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi
    gmake
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake error!"
	exit 1
    fi
    cd ../zebra
    ./configure --enable-dragon CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
        echo "dragon-sw: zebra configure error!"
        exit 1
    fi
    make
    if test $? != 0; then
        echo "dragon-sw: zebra make error!"
        exit 1
    fi

    echo '' && \
	echo "dragon-sw: zebra make error!"
    ;;
  *)
    echo Do not know what kind of system this is, do it by hand
    ;;
esac
