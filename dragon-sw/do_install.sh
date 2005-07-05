#!/bin/sh
#

case `uname` in
  *BSD)
    echo This is BSD Unix
    cd kom-rsvp
    gmake install
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake install error!"
	exit 1
    fi
    cd ../zebra
    make install
    if test $? != 0; then
	echo "dragon-sw: zebra make install error!"
	exit 1
    fi

    echo '' && \
     echo 'dragon-sw install finished.'
    ;;
  *[lL]inux*|*IX*)
    echo This is some other Unix, likely Linux
    cd kom-rsvp
    gmake install
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp gmake install error!"
	exit 1
    fi
    cd ../zebra
    make install
    if test $? != 0; then
        echo "dragon-sw: zebra make install error!"
        exit 1
    fi

    echo '' && \
     echo 'dragon-sw install finished.'
    ;;
  *)
    echo Do not know what kind of system this is, do it by hand
    ;;
esac
