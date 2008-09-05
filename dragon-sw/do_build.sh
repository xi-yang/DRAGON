#!/bin/sh
#

SNMP_COMMUNITY=dragon
PREFIX=/usr/local/dragon

target=$1

if test "$target" = ""; then
  target=default
fi

echo "Building softare target: $target ..."

rsvpconf=
zebraconf="--disable-bgpd --disable-ripd --disable-ripngd --disable-ospf6d"

case "$target" in
  vlsr|VLSR|csa|CSA|narb|NARB|default)
    rsvpconf="--with-switch-vendor-model=AutoDetect --with-switch-ctrl-port=255 --with-switch-snmp-community=$SNMP_COMMUNITY"
  ;;
  vlsr-force10|VLSR-FORCE10)
    rsvpconf="--enable-switch-cli-access --with-switch-vendor-model=Force10E600 --with-switch-ctrl-port=255 --with-switch-snmp-community=$SNMP_COMMUNITY --enable-switch-port-shutdown"
  ;;
  vlsr-force10-v6|VLSR-FORCE10-V6)
    rsvpconf="--enable-switch-cli-access --with-switch-vendor-model=Force10E600 --with-switch-ctrl-port=255 --with-switch-snmp-community=$SNMP_COMMUNITY --with-force10-software-v6 --enable-switch-port-shutdown"
  ;;
  vlsr-raptor|VLSR-RAPTOR)
    rsvpconf="--with-switch-vendor-model=RaptorER1010 --with-switch-ctrl-port=255 --with-switch-snmp-community=$SNMP_COMMUNITY"
  ;;
  vlsr-cat3750|vlsr-catalyst3750|VLSR-CATALYST3750|VLSR-Catalyst3750)
    rsvpconf="--with-switch-vendor-model=Catalyst3750 --with-switch-ctrl-port=255 --with-switch-snmp-community=$SNMP_COMMUNITY --enable-switch-port-shutdown"
  ;;
  vlsr-cat6500|vlsr-catalyst6500|VLSR-CATALYST6500|VLSR-Catalyst6500)
    rsvpconf="--with-switch-vendor-model=Catalyst6500 --with-switch-ctrl-port=255 --with-switch-snmp-community=$SNMP_COMMUNITY --enable-switch-port-shutdown"
  ;;
  vlsr-subnet|VLSR-SUBNET)
    rsvpconf="--enable-switch-cli-access --with-switch-vendor-model=AutoDetect --with-switch-ctrl-port=255 --with-switch-snmp-community=$SNMP_COMMUNITY"
  ;;
  vlsr-linux|VLSR-LINUX)
    rsvpconf="--enable-switch-cli-access --with-switch-vendor-model=LinuxSwitch --with-switch-ctrl-port=1"
  ;;
  vlsr-verbose|VLSR-VERBOSE|vlsr-interactive|VLSR-INTERACTIVE)
    rsvpconf="--enable-switch-cli-access"
  ;;
  cleanup)
    cd zebra
    if test -f Makefile; then
      if test -e /etc/debian_version; then
        make clean
      else
        gmake clean
      fi
    fi
    cd ../kom-rsvp
    if test -f Makefile; then
      if test -e /etc/debian_version; then
        make clean
      else
        gmake clean
      fi
    fi
    cd ..
    echo "dragon-sw: cleanup finished ... "
    exit 0
  ;;
  *)
    echo "Unknown target: $target ... abort!"
    exit 1
  ;;
esac

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

    CONFIG_SHELL=/bin/sh
    export CONFIG_SHELL

    echo '' && \
	echo 'configuring kom-rsvp...'
    cd kom-rsvp
    $CONFIG_SHELL ./configure --prefix=$PREFIX --disable-java-api --disable-altq --with-snmp=$SNMP_PATH $rsvpconf CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi

    echo '' && \
	echo 'making kom-rsvp...'
    gmake
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp GNU make error!"
	exit 1
    fi

    echo '' && \
	echo 'configuring zebra...'
    cd ../zebra
    $CONFIG_SHELL ./configure --prefix=$PREFIX --enable-dragon $zebraconf CFLAG=-g CPPFLAG=-g
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
    ./configure --prefix=$PREFIX --disable-java-api --disable-altq --with-snmp=$SNMP_PATH $rsvpconf CFLAG=-g CPPFLAG=-g
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi

    echo '' && \
	echo 'making kom-rsvp...'
    # Debian systems install GNU make as 'make' not 'gmake'
    if test -e /etc/debian_version; then
	make
    else
	gmake
    fi
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp GNU make error!"
	exit 1
    fi

    echo '' && \
	echo 'configuring zebra...'
    cd ../zebra
    ./configure --prefix=$PREFIX --enable-dragon $zebraconf CFLAG=-g CPPFLAG=-g
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
  *Darwin)
    echo This is Darwin

    echo '' && \
        echo 'configuring kom-rsvp...'
    cd kom-rsvp
    ./configure --prefix=$PREFIX --disable-java-api --disable-altq --with-snmp=$SNMP_PATH --disable-cbq --disable-traffgen $rsvpconf CFLAG=-g CPPFLAG=-g
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
    ./configure --prefix=$PREFIX --enable-dragon $zebraconf CFLAG=-g CPPFLAG=-g
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
