#!/bin/sh
#

CONF_FILE="do_build.conf"
PREFIX=/usr/local/dragon

if test -f $CONF_FILE; then
    . $CONF_FILE
fi

if test "x$1" != x; then
  target=$1
fi

if test "x$target" = x; then
  target="default"
fi

if test "x$snmp_community" = x; then
  snmp_community="dragon"
fi

if test "x$switch_ctrl_port" = x; then
  switch_ctrl_port="255"
fi

if test "x$debug_symbols" = x; then
  debug_symbols="yes"
fi

echo "Building softare target: $target ..."

rsvpconf="--prefix=$PREFIX --disable-java-api --disable-altq"
zebraconf="--prefix=$PREFIX --enable-dragon --disable-bgpd --disable-ripd --disable-ripngd --disable-ospf6d"

case "$target" in
  vlsr|VLSR|csa|CSA|narb|NARB|default)
    rsvpconf="$rsvpconf --with-switch-vendor-model=AutoDetect --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community"
  ;;
  vlsr-force10-v5|VLSR-FORCE10-V5)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=Force10E600 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-force10-v6|VLSR-FORCE10-V6)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=Force10E600 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --with-force10-software-v6 --enable-switch-port-shutdown"
  ;;
  vlsr-juniper|VLSR-JUNIPER)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=JUNOS --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-raptor|VLSR-RAPTOR)
    rsvpconf="$rsvpconf --with-switch-vendor-model=RaptorER1010 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community"
  ;;
  vlsr-raptor-qos|VLSR-RAPTOR-QOS)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=RaptorER1010 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-dell6024|VLSR-DELL6024)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=PowerConnect6024 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-dell6224|VLSR-DELL6224)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=PowerConnect6224 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-dell6248|VLSR-DELL6248)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=PowerConnect6248 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-dell8024|VLSR-DELL8024)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=PowerConnect8024 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-brocade|VLSR-BROCADE)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=BrocadeNetIron --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
    sed -i.orig "s/\\\n/\\\r/g" kom-rsvp/src/daemon/unix/CLI_Session.*
  ;;
  vlsr-cat3750|vlsr-catalyst3750|VLSR-CATALYST3750|VLSR-Catalyst3750)
    rsvpconf="$rsvpconf --with-switch-vendor-model=Catalyst3750 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community"
  ;;
  vlsr-cat3750-qos|vlsr-catalyst3750-qos|VLSR-CATALYST3750-QOS|VLSR-Catalyst3750-QoS)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=Catalyst3750 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-cat6500|vlsr-catalyst6500|VLSR-CATALYST6500|VLSR-Catalyst6500)
    rsvpconf="$rsvpconf --with-switch-vendor-model=Catalyst6500 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-cat6500-qos|vlsr-catalyst6500-qos|VLSR-CATALYST6500-QOS|VLSR-Catalyst6500-QoS)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=Catalyst6500 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community --enable-switch-port-shutdown"
  ;;
  vlsr-subnet|VLSR-SUBNET)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=AutoDetect --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community"
  ;;
  vlsr-cn4200|VLSR-CN4200)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=CienaCN4200 --with-switch-ctrl-port=$switch_ctrl_port --with-switch-snmp-community=$snmp_community"
  ;;
  vlsr-linux|VLSR-LINUX)
    rsvpconf="$rsvpconf --enable-switch-cli-access --with-switch-vendor-model=LinuxSwitch --with-switch-ctrl-port=1 --with-switch-snmp-community=$snmp_community"
  ;;
  vlsr-verbose|VLSR-VERBOSE|vlsr-interactive|VLSR-INTERACTIVE)
    rsvpconf="$rsvpconf --enable-switch-cli-access"
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

if test "x$debug_symbols" = "xyes"; then
  rsvpconf="$rsvpconf CFLAGS=-g CPPFLAGS=-g"
  zebraconf="$zebraconf CFLAGS=-g CPPFLAGS=-g"
fi

if test "x$switch_cli_type" != x; then
  rsvpconf="$rsvpconf --with-switch-cli-type=$switch_cli_type"
fi

if test "x$switch_cli_username" != x; then
  rsvpconf="$rsvpconf --with-switch-cli-username=$switch_cli_username"
fi

if test "x$switch_cli_password" != x; then
  rsvpconf="$rsvpconf --with-switch-cli-password=$switch_cli_password"
fi

case `uname` in
  *BSD)
    echo This is BSD Unix

    CONFIG_SHELL=/bin/sh
    export CONFIG_SHELL

    echo '' && \
	echo 'configuring kom-rsvp...'
    cd kom-rsvp
    $CONFIG_SHELL ./configure $rsvpconf
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi

    echo '' && \
	echo 'making kom-rsvp...'
    gmake depend
    gmake
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp GNU make error!"
	exit 1
    fi

    echo '' && \
	echo 'configuring zebra...'
    cd ../zebra
    $CONFIG_SHELL ./configure $zebraconf
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
    ./configure $rsvpconf
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp configure error!"
	exit 1
    fi

    echo '' && \
	echo 'making kom-rsvp...'
    # Debian systems install GNU make as 'make' not 'gmake'
    if test -e /etc/debian_version; then
	make depend
	make
    else
	gmake depend
	gmake
    fi
    if test $? != 0; then
	echo "dragon-sw: kom-rsvp GNU make error!"
	exit 1
    fi

    echo '' && \
	echo 'configuring zebra...'
    cd ../zebra
    ./configure $zebraconf
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
    ./configure --disable-cbq --disable-traffgen $rsvpconf
    if test $? != 0; then
        echo "dragon-sw: kom-rsvp configure error!"
        exit 1
    fi

    echo '' && \
        echo 'making kom-rsvp...'
    gmake depend
    gmake
    if test $? != 0; then
        echo "dragon-sw: kom-rsvp gmake error!"
        exit 1
    fi

    echo '' && \
        echo 'configuring zebra...'
    cd ../zebra
    ./configure $zebraconf
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
