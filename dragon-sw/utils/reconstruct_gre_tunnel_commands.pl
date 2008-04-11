#!/usr/bin/perl
#
# reconstruct_gre_tunnel_commands.pl
#
# read the current GRE tunnel configuration (using 'ip' and 'netstat')
# and output the shell commands used to create the tunnels
#
# KNOWN LIMITATIONS:
#
#  * only works in Linux
#  * GRE tunnels are expected to match the regular expression 'gre\d+'
#
# $Id$
#

use strict;
use warnings;

my %gre;

my $IP_PATH       = "/sbin/ip";
my $NETSTAT_PATH  = "/bin/netstat";
my $IFCONFIG_PATH = "/sbin/ifconfig";

die "$IP_PATH does not exist"       if !-f $IP_PATH;
die "$NETSTAT_PATH does not exist"  if !-f $NETSTAT_PATH;
die "$IFCONFIG_PATH does not exist" if !-f $IFCONFIG_PATH;

open I, "$IP_PATH addr |";
while(<I>) {
    # output of '/sbin/ip addr' command should look like this:
    # 9: gre6@NONE: <POINTOPOINT,NOARP,UP> mtu 1476 qdisc noqueue
    #     link/gre 64.57.24.165 peer 64.57.17.8
    #     inet 10.100.80.50/30 scope global gre6
    if (/^\d+\:\s+(gre\d+)\@NONE:\s+/) {
        my $grename = $1;
        my $x = <I>;  
        my $y = <I>;  
        if ($x =~ /^\s+link\/gre\s+(\d+\.\d+\.\d+\.\d+)\s+peer\s+(\d+\.\d+\.\d+\.\d+)/) {
            $gre{$grename}{'pub_loc'} = $1;
            $gre{$grename}{'pub_rem'} = $2;
        } else {
            die "1 something bad happened!\n";
	    
        }
        if ($y =~ /^\s+inet\s+(\d+\.\d+\.\d+\.\d+)\/30\s+scope\s+global\s+/) {
            $gre{$grename}{'prv_loc'} = $1;
        } else {
            die "2 something bad happened!\n";
        }
    }
}
close I;

open I, "$NETSTAT_PATH -nr |";
while(<I>) {
    # the output of the 'netstat -nr' command should look like this:
    # 10.100.80.45    0.0.0.0         255.255.255.255 UH        0 0          0 gre5
    if (/^(\d+\.\d+\.\d+\.\d+)\s+0\.0\.0\.0\s+255\.255\.255\.255\s+UH\s+\d+\s+\d+\s+\d+\s+(gre\S+)/) {
        $gre{$2}{'prv_rem'} = $1;
    }
}
close I;

#
# print out the GRE tunnel config commands
#
foreach my $tun ( sort keys %gre ) {
    print <<END;
$IP_PATH tunnel del $tun
$IP_PATH tunnel add $tun mode gre remote $gre{$tun}{'pub_rem'} local $gre{$tun}{'pub_loc'} ttl 255
$IP_PATH link set $tun up
$IP_PATH addr add $gre{$tun}{'prv_loc'}/30 dev $tun
$IP_PATH route add $gre{$tun}{'prv_rem'}/32 dev $tun
$IFCONFIG_PATH $tun mtu 1476

END
}
