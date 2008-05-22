#!/usr/bin/perl -W
#
# genDragonConfig.pl - generate DRAGON configuration files from information
# supplied on the command line
#
# (C) 2007 by Mid-Atlantic Crossroads (MAX).  All rights reserved.
#
# Time-stamp: <2007-12-13 14:24:07 EDT>
# $Id: genDragonConfig.pl,v 1.1 2007/12/13 14:24:07 ctracy Exp $
#
# Author: Chris Tracy <chris@maxgigapop.net>
#
# Description:
#
# Generates DRAGON configuration file from user-supplied values on the
# command line: ospfd.conf, RSVPD.conf, rce.conf, narb.conf,
# zebra.conf, narb.conf, setup_gre_tunnels.sh
#
use strict;
use vars qw($ORIGINAL_SCRIPT $P $VERSION $VERBOSE $OPTS $USAGE $DESCR $AUTHOR
            $COPYRIGHT $ARGS_DESC $LOG_STDERR $LOG_FILE $LOG_FP $LOG_TSTAMP_FMT
            $DEFAULTS $PREFIX);
BEGIN {
  $ORIGINAL_SCRIPT = $0;
  my(@P) = split("/", $0);
  $P = pop(@P);
  my $dir = join('/', @P);
  unshift(@INC, $dir);
  ## If we're being run out of a bin/ directory and there is ../lib, then
  ## add it to @INC
  if ($P[$#P] eq 'bin') {
    my @tmp = @P;
    pop(@tmp);
    my $tmp = join("/",@tmp)."/lib";
    unshift(@INC, $tmp) if (-d $tmp);
    $tmp .= "/perl";
    unshift(@INC, $tmp) if (-d $tmp);
  }
  my $ndir = "$dir/../lib/perl5/site_perl";
  unshift(@INC, $ndir) if (-d $ndir);
}
##
use POSIX;
use Getopt::Long;
use IO::File;
##
$PREFIX = "/usr/local/dragon/etc/";
$DEFAULTS =
  { element => 'vlsr',
    narbDomain => '1',
    switchIp => "0.0.0.0",
    DRAGON_CONF       => $PREFIX."dragon.conf",
    ZEBRA_CONF        => $PREFIX."zebra.conf",
    OSPFD_CONF        => $PREFIX."ospfd.conf",
    OSPFD_INTRA_CONF  => $PREFIX."ospfd-intra.conf",
    OSPFD_INTER_CONF  => $PREFIX."ospfd-inter.conf",
    RSVPD_CONF        => $PREFIX."RSVPD.conf",
    NARB_CONF         => $PREFIX."narb.conf",
    RCE_CONF          => $PREFIX."rce.conf",
    GRE_CONF          => $PREFIX."setup_gre_tunnels.sh",
    NARB_TOPOLOGIES   => $PREFIX."narb_topologies",
  };
$VERSION = '0.1.0';
$DESCR = 'generate DRAGON configuration files';
$AUTHOR = 'Chris Tracy <chris@maxgigapop.net>';
$VERBOSE = 1;
$COPYRIGHT = '(C) 2007 by Mid-Atlantic Crossroads (MAX).  All rights reserved.';
$LOG_STDERR = 0;
$LOG_FILE = "/var/log/genDragonConfig.log";
$LOG_FP = undef;
$LOG_TSTAMP_FMT = '%Y-%m-%d %H:%M:%S';
$USAGE = <<__UsAGe__;
  options:
           --element=[vlsr|narb|p2p-csa|uni-csa]
           --hostname=name_of_this_host
           --loopback=IP_address
           --gretunnel=GRE_name,pub_rem,pub_loc,priv_rem,priv_loc,network_num_mask,{...}
           --telink=GRE_name,te_local,sw_port,{...}
           --narb=IP_address
           --narbdomain=ID
           --narbintra=GRE_name
           --narbinter=GRE_name
           --inter-domain-te-link=IP_address
           --passiveint=GRE_name
           --switch-ip=IP_address
           --copy-hostfs
           --start
           -h    print this message and exit
           -v    be verbose
__UsAGe__
##

my %opts = ('element'    => $DEFAULTS->{element},
            'narbdomain' => $DEFAULTS->{narbDomain},
            'switch-ip'  => $DEFAULTS->{switchIp},
            'narbintra'  => '',
            );

usage() unless scalar(@ARGV);
usage() unless 
    GetOptions (\%opts, "h", "v",
                "element=s",    # string: either vlsr, narb, p2p-csa or uni-csa
                "hostname=s",   # host name, e.g. clpk-vlsr
                "loopback=s",   # loopback address (ospf router-id), e.g. 140.173.2.232
                "gretunnel=s",  # see format below**
                "telink:s",     # see format below**
                "narb=s",       # intra-domain NARB IP address for VLSR's dragon.conf/RSVPD.conf
                "narbdomain=s", # NARB domain IP -- e.g. 140.173.0.0 or 10.0.0.0
                "narbintra=s",  # intra-domain NARB GRE tunnel (GRE tunnel between NARB--VLSR)
                "narbinter=s",  # inter-domain NARB GRE tunnel (GRE tunnel between NARB--NARB)
                "inter-domain-te-link=s", # remote TE address for inter-domain TE link
                "passiveint=s", # passive interface for OSPFD -- for inter-domain TE links (VLSR--VLSR interdomain)
                "switch-ip=s",  # IP address for switch that VLSR is controlling (0.0.0.0 for no switch/simulation)
                "copy-hostfs",  # if this switch is passed, mount the hostfs and copy the config files over..
                "start",        # pass this option to automatically start dragon daemons
                );
usage(undef, 1) if $opts{h};

# **GRE tunnel config format:
#   GRE tunnel name (e.g. gre1)
#   GRE tunnel remote address (public)
#   GRE tunnel local address  (public)
#   GRE tunnel remote address (private)
#   GRE tunnel local address  (private)
#   GRE tunnel network (/30)
#
# **TE link config format:
#   GRE tunnel name (e.g. gre1)
#   TE address (local)
#   VLSR switch port assocation (local)
#
# %gre hash keys:
#     name = GRE tunnel name (gre1 or higher)
#     pub_rem = GRE tunnel remote address (public)
#     pub_loc = GRE tunnel local address  (public)
#     prv_rem = GRE tunnel remote address (private)
#     prv_loc = GRE tunnel local address  (private)
#     prv_net = GRE tunnel private network
#
# %te_link hash keys:
#     name = GRE tunnel name (gre1 or higher)
#     te_addr_local = TE address (local)
#     switch_port = VLSR switch port assocation (local)
#

my @gre_tunnel_config = split(/,\s*/, $opts{'gretunnel'});
my $num_gre_tunnels = scalar(@gre_tunnel_config) / 6;
my %gre;

for (1..$num_gre_tunnels) {
  my $name = shift @gre_tunnel_config;
  $gre{$name}{'name'} = $name;
  $gre{$name}{'pub_rem'} = shift @gre_tunnel_config;
  $gre{$name}{'pub_loc'} = shift @gre_tunnel_config;
  $gre{$name}{'prv_rem'} = shift @gre_tunnel_config;
  $gre{$name}{'prv_loc'} = shift @gre_tunnel_config;
  $gre{$name}{'prv_net'} = shift @gre_tunnel_config;
}

my @te_link_config = ();
my $num_te_links = 0;
my %te_link;

@te_link_config = split(/,\s*/, $opts{'telink'}) if defined($opts{'telink'});
$num_te_links = scalar(@te_link_config) / 3;

for (1..$num_te_links) {
  my $name = shift @te_link_config;
  $te_link{$name}{'name'} = $name;
  $te_link{$name}{'te_addr_local'} = shift @te_link_config;
  $te_link{$name}{'switch_port'} = shift @te_link_config;
}

#
# open the proper config files, depending on network element type
# (e.g. NARB requires different set of config files than VLSR)
#
if ($opts{'element'} =~ /^(vlsr|p2p-csa)$/i) {
  open DRAGON_CONF, "> $DEFAULTS->{DRAGON_CONF}";
  open ZEBRA_CONF,  "> $DEFAULTS->{ZEBRA_CONF}";
  open OSPFD_CONF,  "> $DEFAULTS->{OSPFD_CONF}";
  open RSVPD_CONF,  "> $DEFAULTS->{RSVPD_CONF}";
  open GRE_CONF,    "> $DEFAULTS->{GRE_CONF}";
}
if ($opts{'element'} =~ /^uni-csa$/i) {
  open DRAGON_CONF, "> $DEFAULTS->{DRAGON_CONF}";
  open RSVPD_CONF,  "> $DEFAULTS->{RSVPD_CONF}";
  open GRE_CONF,    "> $DEFAULTS->{GRE_CONF}";
}
if ($opts{'element'} =~ /^narb$/i) {
  open ZEBRA_CONF,       "> $DEFAULTS->{ZEBRA_CONF}";
  open OSPFD_CONF,       "> $DEFAULTS->{OSPFD_INTRA_CONF}";
  open OSPFD_INTER_CONF, "> $DEFAULTS->{OSPFD_INTER_CONF}";
  open NARB_CONF,        "> $DEFAULTS->{NARB_CONF}";
  open RCE_CONF,         "> $DEFAULTS->{RCE_CONF}";
  open GRE_CONF,         "> $DEFAULTS->{GRE_CONF}";
}

#
# standard beginnings for these files...
#
print ZEBRA_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);
hostname $opts{'hostname'}
password dragon
enable password dragon
log file /var/log/zebra.log
END

print DRAGON_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|uni-csa)$/i);
hostname $opts{'hostname'}
password dragon
END

print OSPFD_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);
hostname $opts{'hostname'}
password dragon
enable password dragon
log stdout
log file /var/log/ospfd.log
END

print OSPFD_INTER_CONF <<END if ($opts{'element'} =~ /^narb$/i and defined($opts{'narbinter'}));
hostname $opts{'hostname'}
password dragon
enable password dragon
log stdout
log file /var/log/ospfd-inter.log
END

print RSVPD_CONF "api 4000\n" if ($opts{'element'} =~ /^(vlsr|p2p-csa|uni-csa)$/i);

print NARB_CONF <<END if ($opts{'element'} =~ /^narb$/i);
domain-id { ip $opts{'narbdomain'} }
cli { host $opts{'hostname'} password dragon }
END

print RCE_CONF <<END if ($opts{'element'} =~ /^narb$/i);
domain-id {ip $opts{'narbdomain'}}
include-tedb-schema {path /usr/local/dragon/etc/schema_combo.rsd}
END

print GRE_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|uni-csa|narb)$/i);
#!/bin/sh
/sbin/modprobe ip_gre >/dev/null 2>&1
END

#
# required for proper operation when running under UML...
# i think the daemons had problems bind()'ing without this...
#
system("ifconfig lo inet 127.0.0.1");

#
# set hostname for UML...(cosmetic)
#
if (defined($opts{'loopback'})) {
  system("echo $opts{'hostname'} > /etc/hostname");
  system("hostname $opts{'hostname'}");
}

#
# configure the system's GRE tunnels on the fly...
# setup ospfd.conf and RSVPD.conf interface configurations...
#
foreach my $name ( sort keys %gre ) {
  print GRE_CONF <<END;
ip tunnel del $gre{$name}{'name'} >/dev/null 2>&1
ip tunnel add $gre{$name}{'name'} mode gre remote $gre{$name}{'pub_rem'} local $gre{$name}{'pub_loc'} ttl 255
ip link set $gre{$name}{'name'} up
ip addr add $gre{$name}{'prv_loc'}/30 dev $gre{$name}{'name'}
ip route add $gre{$name}{'prv_rem'}/32 dev $gre{$name}{'name'}

END

    if (defined($opts{'narbinter'}) and $opts{'narbinter'} =~ /$name/i) {
      print OSPFD_INTER_CONF <<END if ($opts{'element'} =~ /^narb$/i);
interface $gre{$name}{'name'}
  description GRE tunnel $gre{$name}{'name'}
  ip ospf network point-to-point
END
    } else {
      print OSPFD_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);
interface $gre{$name}{'name'}
  description GRE tunnel $gre{$name}{'name'}
  ip ospf network point-to-point
END
    }

    # skip RSVPD configuration on VLSR for VLSR--NARB GRE tunnel:
    next if defined($opts{'narbintra'}) and $name eq $opts{'narbintra'};

    # normal VLSR and end-system (in peer mode) operation
    print RSVPD_CONF "interface $name tc none mpls\n"     
        if ($opts{'element'} =~ /^(vlsr|p2p-csa)$/i and defined($te_link{$name}{'te_addr_local'}));

    # if this is a VLSR, an undefined local TE address signals that this GRE is used for servicing a UNI client
    print RSVPD_CONF "interface $name tc none mpls p/1\n" 
        if ($opts{'element'} =~ /^vlsr$/i and !defined($te_link{$name}{'te_addr_local'}));

    # if this is a UNI-mode end system, assume we are connected to VLSR port 1
    print RSVPD_CONF "interface $name tc none mpls p/1\n" 
        if ($opts{'element'} =~ /^uni-csa$/i);
}

print OSPFD_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);
  router ospf
    ospf router-id $opts{'loopback'}
END

print OSPFD_INTER_CONF <<END if ($opts{'element'} =~ /^narb$/i and defined($opts{'narbinter'}));
  router ospf
    ospf router-id $opts{'loopback'}
END

# 
# inter-domain GRE tunnels between 2 VLSRs need to be put into passive mode
# (we have to represent the TE link, but we don't want to flood LSAs across this interface)
#
if (defined($opts{'passiveint'})) {
  foreach my $passive_int ( split(/[, ]/, $opts{'passiveint'}) ) {
    print OSPFD_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);
    passive-interface $passive_int
END
    }
}

foreach my $name ( sort keys %gre ) {
  if (defined($opts{'narbinter'}) and $opts{'narbinter'} =~ /$name/i) {
    print OSPFD_INTER_CONF "    network $gre{$name}{'prv_net'} area 0.0.0.1\n" if ($opts{'element'} =~ /^narb$/i);
  } else {
    print OSPFD_CONF "    network $gre{$name}{'prv_net'} area 0.0.0.0\n" if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);
  }
}

print OSPFD_CONF "    ospf-te router-address $opts{'loopback'}\n" if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);

#
# eventually we may want to use switch-ip 127.0.0.1 and talk to a 'fake' switch 
# (implemented by a perl script) that is called via NET-SNMP's snmpd.
#
if ($opts{"element"} =~ /^vlsr$/i) {
  foreach my $name ( sort keys %gre ) {
    #
    # this section is not necessary for the GRE tunnel between the NARB and a VLSR.
    # the 'narbintra' option is used to specify this GRE tunnel, so it knows whether
    # or not this section can be skipped.  we want to bring up an OSPF adjacency
    # between the intra-domain NARB ospfd and the VLSR's ospfd, but the NARB should
    # not advertise any TE links to the VLSR...
    #
    # make sure that the switch IP address = 127.0.0.1 if using the linux software switch
    # otherwise, use the one supplied on the command-line option (--switch-ip)
    my $switch_ip_addr;
    if (defined($te_link{$name}{'switch_port'}) and 
        $te_link{$name}{'switch_port'} =~ /^(eth\d+)/) {
      my $int_name = $1;
      $switch_ip_addr = "127.0.0.1";
      $te_link{$name}{'switch_port'} = &mapInterfaceNameToNumber($int_name);
    } else {
	    $switch_ip_addr = $opts{"switch-ip"};
    }
    if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i and
        $name !~ $opts{'narbintra'} and
        defined($te_link{$name}{'te_addr_local'}) and
        defined($te_link{$name}{'switch_port'})) {
      print OSPFD_CONF <<END;
    ospf-te interface $gre{$name}{'name'}
      level gmpls
      data-interface ip $te_link{$name}{'te_addr_local'} protocol snmp switch-ip $switch_ip_addr switch-port $te_link{$name}{'switch_port'}
      swcap l2sc encoding ethernet
! 1Gbps = 125000000 bytes/sec
      max-bw 125000000
      max-rsv-bw 125000000
      max-lsp-bw 0 125000000
      max-lsp-bw 1 125000000
      max-lsp-bw 2 125000000
      max-lsp-bw 3 125000000
      max-lsp-bw 4 125000000
      max-lsp-bw 5 125000000
      max-lsp-bw 6 125000000
      max-lsp-bw 7 125000000
      vlan 100 to 200
      metric 10
     exit
END
    }
  }
} elsif ($opts{"element"} =~ /^p2p-csa$/i) {
  foreach my $name ( sort keys %gre ) {
    # the configuration for an end system in peer mode is relatively straightforward...
    print OSPFD_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|narb)$/i);
    ospf-te interface $gre{$name}{'name'}
      level gmpls
      data-interface ip $te_link{$name}{'te_addr_local'}
      swcap l2sc encoding ethernet
! 1Gbps = 125000000 bytes/sec
      max-bw 125000000
      max-rsv-bw 125000000
      max-lsp-bw 0 125000000
      max-lsp-bw 1 125000000
      max-lsp-bw 2 125000000
      max-lsp-bw 3 125000000
      max-lsp-bw 4 125000000
      max-lsp-bw 5 125000000
      max-lsp-bw 6 125000000
      max-lsp-bw 7 125000000
      metric 10
     exit
END
  }
}

print RSVPD_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa|uni-csa)$/i and defined($opts{'narb'}));
narb $opts{'narb'} 2609
narb_extra_options query-with-holding
narb_extra_options query-with-confirmation
END

#
# some example local IDs for testing...
#
print DRAGON_CONF <<END if ($opts{'element'} =~ /^vlsr$/i and defined($opts{'narb'}));
set local-id port 1
set local-id port 9
set local-id port 10
set local-id port 11
set local-id port 12
set local-id group 1000 add 9
set local-id group 1000 add 10
set local-id group 1001 add 11
set local-id group 1001 add 12
set local-id tagged-group 101 add 9
set local-id tagged-group 101 add 10
set local-id tagged-group 102 add 11
set local-id tagged-group 102 add 12
END

#
# configure UNI client as local-id port 1 (p/1 also hardcoded in RSVPD.conf above)
#
print DRAGON_CONF <<END if ($opts{'element'} =~ /^uni-csa$/i);
set local-id port 1
END

#
# configure intra-domain NARB if this is a VLSR or an end system in peer mode
# (so that the dragon daemon can contact the NARB to request an ERO)
#
# the extra NARB options ensure that the NARB holds the resources for a
# short period of time so that there is no VLAN contention when there
# are many simultaneous path requests
#
print DRAGON_CONF <<END if ($opts{'element'} =~ /^(vlsr|p2p-csa)$/i and defined($opts{'narb'}));
configure narb intra-domain ip-address $opts{'narb'} port 2609
set narb-extra-options query-with-confirmation
set narb-extra-options query-with-holding
END

#
# configure NARB linkage to intra-domain OSPFD API
#
print NARB_CONF <<END if ($opts{'element'} =~ /^narb$/i and defined($opts{'narbintra'}));
intra-domain-ospfd { address localhost port 2617 originate-interface $gre{$opts{'narbintra'}}{'prv_loc'} area 0.0.0.0 }
END

#
# configure NARB linkage to inter-domain OSPFD API
#
if (defined($opts{'narbinter'}) and $opts{'element'} =~ /^narb$/i) {
  # we don't need multiple inter-domain-ospfd stanzas, only one is required
  # foreach my $narbinter ( split(/[, ]/, $opts{'narbinter'}) ) { }
  my @narbinter = split(/[, ]/, $opts{'narbinter'});
  print NARB_CONF <<END;
inter-domain-ospfd { address localhost port 2607 originate-interface $gre{$narbinter[0]}{'prv_loc'} area 0.0.0.1 }
END
}

# import NARB topology from $NARB_TOPOLOGIES directory, based on hostname.
# eventually we will have a better way to automatically generate this...
if ($opts{'hostname'} =~ /^(red-narb|blue-narb|yellow-narb|green-narb|narb1|narb2)$/ and
    $opts{'element'} =~ /^narb$/i and defined($opts{'narbinter'})) {
  open I, "$DEFAULTS->{NARB_TOPOLOGIES}/$opts{'hostname'}.narbtopo.conf";
  while(<I>) {
    print NARB_CONF;
  }
  close I;
}

# need one of these statements for each inter-domain TE link.
# associate each inter-domain link with a peering NARB.
if (defined($opts{'narbinter'}) and $opts{'element'} =~ /^narb$/i) {
  my @inter_domain_te_links = split(/[, ]/, $opts{'inter-domain-te-link'});
  my $z = 0;
  foreach my $narbinter ( split(/[, ]/, $opts{'narbinter'}) ) {
    print NARB_CONF <<END;
inter-domain-te-link { id $inter_domain_te_links[$z] narb-peer $gre{$narbinter}{'prv_rem'} port 2609 }
END
        $z++;
  }
}

# close files
if ($opts{'element'} =~ /^(vlsr|p2p-csa)$/i) {
  close DRAGON_CONF;
  close ZEBRA_CONF;
  close OSPFD_CONF;
  close RSVPD_CONF;
  close GRE_CONF;
}
if ($opts{'element'} =~ /^uni-csa$/i) {
  close DRAGON_CONF;
  close RSVPD_CONF;
  close GRE_CONF;
}
if ($opts{'element'} =~ /^narb$/i) {
  close ZEBRA_CONF;
  close OSPFD_CONF;
  close OSPFD_INTER_CONF;
  close NARB_CONF;
  close RCE_CONF;
  close GRE_CONF;
}

#
# set $GRE_CONF to be executable
#
system("/bin/chmod +x $DEFAULTS->{GRE_CONF}");

#
# if the --copy-hostfs option was passed, copy the generated configs to the host filesystem
#
if ($opts{'copy-hostfs'}) {
  system("mount none /mnt -t hostfs -o /a/uml/hostfs >/dev/null 2>&1");
  mkdir "/mnt/$opts{'hostname'}" if (!-d "/mnt/$opts{'hostname'}");
  system("/bin/cp $DEFAULTS->{DRAGON_CONF}      /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{DRAGON_CONF}; 
  system("/bin/cp $DEFAULTS->{ZEBRA_CONF}       /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{ZEBRA_CONF};
  system("/bin/cp $DEFAULTS->{OSPFD_CONF}       /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{OSPFD_CONF};
  system("/bin/cp $DEFAULTS->{OSPFD_INTRA_CONF} /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{OSPFD_INTRA_CONF};
  system("/bin/cp $DEFAULTS->{OSPFD_INTER_CONF} /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{OSPFD_INTER_CONF};
  system("/bin/cp $DEFAULTS->{NARB_CONF}        /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{NARB_CONF};
  system("/bin/cp $DEFAULTS->{RCE_CONF}         /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{RCE_CONF};
  system("/bin/cp $DEFAULTS->{RSVPD_CONF}       /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{RSVPD_CONF};
  system("/bin/cp $DEFAULTS->{GRE_CONF}         /mnt/$opts{'hostname'}/") if -f $DEFAULTS->{GRE_CONF};
  open Z, "> /mnt/$opts{'hostname'}/genDragonConfig.sh";
  print Z "#!/bin/sh\n/usr/local/dragon/bin/genDragonConfig.pl ";
  foreach my $val ( sort keys %opts) {
    print Z "--$val=\"$opts{$val}\" ";
  }
  print Z "\n";
  close Z;
  system("umount /mnt");
}
foreach my $val ( sort keys %opts) {
  log_msg(1,qq{--$val=\"$opts{$val}\"});
}

#
# start the network element's daemons in the proper mode 
# (if --start was passed)
#
if ($opts{'start'}) {
  # disable TLS to prevent random segfault
  # http://uml.harlowhill.com/uml/Wiki.jsp?page=Troubleshooting#section-Troubleshooting-QProgramsRandomlySegfaultWhenITryToRunThemInUml.
  system ( "/bin/mv /lib/tls /lib/tls.bak" ) if -d '/lib/tls';
  #
  # setup GRE tunnels
  #
  system( "$DEFAULTS->{GRE_CONF} >/dev/null 2>&1" );
  #
  # start the proper set of daemons
  #
  if ($opts{'element'} =~ /^(vlsr|p2p-csa)$/i) {
    system("/usr/local/dragon/bin/dragon.sh start-vlsr >/dev/null 2>&1");
  }
  if ($opts{'element'} =~ /^uni-csa$/i) {
    system("/usr/local/dragon/bin/dragon.sh start-uni >/dev/null 2>&1");
  }
  if ($opts{'element'} =~ /^narb$/i) {
    system("/usr/local/dragon/bin/dragon.sh start-narb >/dev/null 2>&1");
  }
}

##
sub mapInterfaceNameToNumber {
  # convert 'ethX' to interface number (integer)
  my $int_name = shift;  # e.g. 'eth1'
  open(dev_file, '/proc/net/dev') || die "The impossible happened: cannot open /proc/net/dev!";
  my $index=1;
  my $int_num = 255;
  while (<dev_file>) {
    if (/^\s*([a-zA-Z0-9.]+):([ 0-9]+)/) {
	    $int_num = $index if $int_name eq $1;
      log_msg(1,qq{mapInterfaceNameToNumber: interface $1 --> port #$index (0/0/$index)});
	    $index++;
    }
  }
  return $int_num;
}
##
sub usage {
  my $msg = shift(@_);
  print STDERR sprintf("%9s: %s\n", "ERROR", $msg) if $msg;
  print STDERR sprintf("%9s: %s\n", $P, $DESCR);
  print STDERR sprintf("%9s: %s\n", "Version", $VERSION);
  print STDERR sprintf("%9s: %s\n", "Copyright", $COPYRIGHT);
  print STDERR sprintf("%9s: %s\n", "Author", $AUTHOR);
  print $USAGE;
  if (scalar(@_)) {
    my $nope = 0;
    open(ME, "<$0") || ($nope=1);
    unless ($nope) {
      my $in_history = 0;
      while (<ME>) {
        next unless ($in_history || /^=head1\s+VERSION/);
        if (/^=head1\s+VERSION/) {
          $in_history = 1;
          print STDERR "\n  ","-" x 20, "[ VERSION HISTORY ]", "-" x 20,"\n\n";
          print STDERR sprintf("  %-7s   %-9s   %-7s %s\n",
                               "VERS","WHEN","WHO","WHAT");
          next;
        } elsif ($in_history && /^=cut/) {
          last;
        } elsif ($in_history && ($_ !~ /^\s*$/)) {
          print STDERR $_;
        }
      }
      close(ME);
    }
  }
  exit(defined($msg));
}
##
sub ts {
  my $fmt = $LOG_TSTAMP_FMT || "%Y-%m-%d %H:%M:%S";
  return POSIX::strftime($fmt, localtime(time));
}
##
sub log_msg {
  my $lvl = shift(@_);
  return unless $VERBOSE >= $lvl;
  my $logmsg = "$P: " . ts() . " [$lvl] @_\n";
  print STDERR $logmsg if $LOG_STDERR;
  if ($LOG_FILE && !$LOG_FP) {
    $LOG_FP = new IO::File(">> $LOG_FILE")
        or die "$P: could not create log file $LOG_FILE: $!\n";
  }
  print $LOG_FP $logmsg if $LOG_FP;
}
__END__

=head1 VERSION HISTORY

  0.1.0   13 Dec 07     ctracy     started

=cut

# Local variables:
# tab-width: 2
# perl-indent-level: 2
# indent-tabs-mode: nil
# comment-column: 40
# End:
