#!/usr/bin/perl
#
# showSwitchVLANs.pl - shows the VLAN configuration of an RFC2674
# compliant switch
#
# (C) 2008 by Mid-Atlantic Crossroads (MAX).  All rights reserved.
#
# Time-stamp: <2008-04-09 11:41:00 EDT>
# $Id$
#
# Author: Chris Tracy <chris@maxgigapop.net>
#
# Description:
#
# Longer description
#
use strict;
use warnings;
use vars qw($ORIGINAL_SCRIPT $P $VERSION $VERBOSE $OPTS $USAGE $DESCR $AUTHOR
            $COPYRIGHT $ARGS_DESC $LOG_STDERR $LOG_FILE $LOG_FP $LOG_TSTAMP_FMT
            $DEFAULTS %VLANs %SWITCHES %PVIDs);
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
$DEFAULTS =
{
    HOSTS             => undef,
    COMMUNITY         => q{public},
    LOG               => undef,
    VLANPORTS         => q{.1.3.6.1.2.1.17.7.1.4.3.1.2},
    UNTAGGEDVLANPORTS => q{.1.3.6.1.2.1.17.7.1.4.3.1.4},
    PVID              => q{.1.3.6.1.2.1.17.7.1.4.5.1.1},
    SNMPWALK          => undef,
    MAXPORTS          => 255,
    TERMCOLUMNS       => 50,
    VLANS             => undef,
};
$VERSION = '0.1';
$DESCR = 'show VLAN mapping on RFC2674 compliant switch';
$AUTHOR = 'Chris Tracy <chris@maxgigapop.net>';
$VERBOSE = 0;
$COPYRIGHT = '(C) 2008 by Mid-Atlantic Crossroads (MAX).  All rights reserved.';
$LOG_STDERR = 1;
$LOG_FILE = undef;
$LOG_FP = undef;
$LOG_TSTAMP_FMT = '%Y-%m-%d %H:%M:%S';
$USAGE = <<__UsAGe__;
 options:  
           --hosts=[IP,{IP,..}]        list of IP addresses of hosts to query
           --community=[string]        SNMP community string for reading (default=public)
           --log=[filename]            log output to filename (default is no logging)
           --vlanports=[OID]           OID to get vlan port list from (tagged/untagged)
                                          (default=.1.3.6.1.2.1.17.7.1.4.3.1.2)
           --untaggedvlanports=[OID]   OID to get untagged vlan port list from 
                                          (default=.1.3.6.1.2.1.17.7.1.4.3.1.4)
           --pvid=[OID]                OID to get PVIDs (VLAN assigned to untagged frames)
                                          (default=.1.3.6.1.2.1.17.7.1.4.5.1.1)
           --snmpwalk=[string]         full path to snmpwalk binary
                                          (default searches /usr/bin and /usr/local/bin)
           --maxports=[number]         maximum number of ports on the switch (default=255)
           --showempty                 print empty VLANs (default is not to print)
           --vlans=[ID,{ID,..}]        only print these VLAN ID(s) (default=show all)
           -h                          print this message and exit
           -v                          be verbose
__UsAGe__
##

my %opts = ('log'               => $DEFAULTS->{LOG},
            'hosts'             => $DEFAULTS->{HOSTS},
            'community'         => $DEFAULTS->{COMMUNITY},
            'vlanports'         => $DEFAULTS->{VLANPORTS},
            'untaggedvlanports' => $DEFAULTS->{UNTAGGEDVLANPORTS},
            'pvid'              => $DEFAULTS->{PVID},
            'snmpwalk'          => $DEFAULTS->{SNMPWALK},
            'maxports'          => $DEFAULTS->{MAXPORTS},
            'termcolumns'       => $DEFAULTS->{TERMCOLUMNS},
            'vlans'             => $DEFAULTS->{VLANS},
            );

usage() unless scalar(@ARGV);
usage() unless
    GetOptions (\%opts,
                "h",         # help
                "v",         # verbose
                "log=s",
                "hosts=s",
                "community=s",
                "vlanports=s",
                "untaggedvlanports=s",
                "pvid=s",
                "snmpwalk=s",
                "maxports=s",
                "termcolumns=s",
                "showempty",
                "vlans=s",
                );
usage(undef, 1) if $opts{h};
$LOG_FILE = $opts{log};
# log_msg(1,qq{our args: }.join(' // ',@ARGV));

my @HOSTS = split(/,\s*/, $opts{'hosts'});

if (!defined($opts{snmpwalk})) {
  if (-e "/usr/local/bin/snmpwalk") {
    $opts{snmpwalk} = "/usr/local/bin/snmpwalk";
  } elsif (-e "/usr/bin/snmpwalk") {
    $opts{snmpwalk} = "/usr/bin/snmpwalk";
  } else {
    die "couldn't find snmpwalk binary, try specifying with --snmpwalk: $!";
  }
} else {
  if (!-e $opts{snmpwalk}) {
    die "couldn't find snmpwalk binary at $opts{snmpwalk}: $!";
  }
}

#
# VLANs hash:
#  $VLANs{$HOST}{vlan id}{'T'} = [tagged port list in binary]
#  $VLANs{$HOST}{vlan id}{'U'} = [untagged port list in binary]
#
# SWITCHES hash:
#  $SWITCHES{$HOST}{'model'} = sysDescr field
#
# PVIDs hash:
#  $PVIDs{$HOST}{port} = PVID
#

foreach my $HOST (@HOSTS) {
  log_msg( 2, "querying switch $HOST" );

  #
  # get switch model by querying sysDescr
  #
  my $cmd = "$opts{snmpwalk} -Oxn -v 1 -c $opts{community} $HOST sysDescr";
  log_msg( 2, "running command $cmd" );
  open I, "$cmd |";
  while(<I>) {
    if (/STRING:\s+Ether\-Raptor/) {
      log_msg( 1, "sysDescr field indicates this is a Raptor switch" );
      $SWITCHES{$HOST}{'model'} = "Raptor";
    } elsif (/STRING:\s+(PowerConnect\s+5224|Ethernet\s+Switch|Neyland\s+24T)/) {
      log_msg( 1, "sysDescr field indicates this is a Dell PowerConnect switch" );
      $SWITCHES{$HOST}{'model'} = "Dell PowerConnect";
    } else {
      $SWITCHES{$HOST}{'model'} = "Unknown";
    }
  }
  close I;

  #
  # get port-to-vlan mappings (tagged & untagged)
  #
  foreach my $OID_WALK ( $opts{vlanports}, $opts{untaggedvlanports} ) {
    my $oid;
    my $type;
    my $vlan;
    my $cmd = "$opts{snmpwalk} -Oxn -v 1 -c $opts{community} $HOST $OID_WALK";
    log_msg( 2, "running command $cmd" );
    open I, "$cmd |";
    while(<I>) {
      chomp;
      my $data;
      if (/^($opts{vlanports}|$opts{untaggedvlanports})\.(\d+)\s+\=\s+Hex\-STRING:\s+([0-9A-F ]+)\s*/i) {
        $oid  = $1;
        $vlan = $2;
        $data = $3;
        $type = "T" if $oid eq $opts{vlanports};
        $type = "U" if $oid eq $opts{untaggedvlanports};
        log_msg( 2, "vlan=$vlan, type=$type" );
        processHexString( $HOST, $vlan, $type, $data );
      } elsif (/^([0-9A-F ]+)$/) {
        # looks like a continuation of the data for a previous line..
        $data = $1;
        processHexString( $HOST, $vlan, $type, $data );
      } elsif (/^($opts{vlanports}|$opts{untaggedvlanports})\.(\d+)\s+\=\s+\"\"\s*/i) {
        $oid  = $1;
        $vlan = $2;
        $data = 0;
        $type = "T" if $oid eq $opts{vlanports};
        $type = "U" if $oid eq $opts{untaggedvlanports};
        log_msg( 2, "vlan=$vlan, type=$type" );
        processHexString( $HOST, $vlan, $type, $data );
      } else {
        log_msg( 1, "unrecognized line='$_'" );
      }
    }
    close I;
  }

  #
  # get PVID settings (the VLAN ID associated with untagged frames)
  #
  $cmd = "$opts{snmpwalk} -On -v 1 -c $opts{community} $HOST $opts{pvid}";
  log_msg( 2, "running command $cmd" );
  open I, "$cmd |";
  while(<I>) {
    chomp;
    if (/^$opts{pvid}\.(\d+)\s+\=\s+Gauge32\:\s+(\d+)/) {
      my $port = $1;
      my $pvid = $2;
      log_msg( 2, "port $1 pvid is $2" );
      $PVIDs{$HOST}{$port} = $pvid;
    }
  }
}

#
# build hashes for easier printing..
#
my %vlan_ports;
my %untagged_vlan_ports;
foreach my $host ( sort keys %VLANs ) {
  foreach my $vlan ( sort {$a <=> $b} keys %{ $VLANs{$host} } ) {
    foreach my $type ( 'T', 'U' ) {
      my @members = split(/ */, $VLANs{$host}{$vlan}{$type});
      my $port = 1;
      foreach my $val (@members) {
        $vlan_ports{$host}{$vlan}{$port}          = $val if $type eq 'T';
        $untagged_vlan_ports{$host}{$vlan}{$port} = $val if $type eq 'U';
        $port++;
      }
    }
  }
}

#
# print out the port-to-VLAN mapping + PVID in a nice way
#  (inspired by Force10 CLI output)
#
foreach my $host ( sort keys %vlan_ports ) {
  print "VLAN configuration on '$host' (model=$SWITCHES{$host}{'model'}):\n";
  print "\n";
  print "VLAN  Ports (U=untagged, T=tagged)\n";
  print "----  " . "-" x ($opts{termcolumns} - 6) . "\n";
  foreach my $vlan ( sort {$a <=> $b} keys %{ $vlan_ports{$host} } ) {
    my $line = "";
    my $p = 7;
    $line .= sprintf("%4d  ", $vlan);
    log_msg( 2, "host $host: vlan $vlan" );
    my $count = 0;
    foreach my $port ( sort {$a <=> $b} keys %{ $vlan_ports{$host}{$vlan} } ) {
      last if $port > $opts{maxports};
      my $name = getPortNameByNumber($SWITCHES{$host}{'model'}, $port);
      
      next if $vlan_ports{$host}{$vlan}{$port} != 1;
      next if !defined($name);

      my $str;
      # untagged ports are always printed:
      # only print a port as tagged if it has not already been printed as untagged!
      if (defined($untagged_vlan_ports{$host}{$vlan}{$port}) and
          $untagged_vlan_ports{$host}{$vlan}{$port} == 1 and
          $vlan_ports{$host}{$vlan}{$port} == 1) {
        log_msg( 2, "host $host: port $name is in VLAN $vlan as untagged" );
        $str = "U $name [pvid=$PVIDs{$host}{$port}], ";
        $count++;
      } elsif ((!defined($untagged_vlan_ports{$host}{$vlan}{$port}) or
                $untagged_vlan_ports{$host}{$vlan}{$port} == 0) and
               $vlan_ports{$host}{$vlan}{$port} == 1) {
        log_msg( 2, "host $host: port $name is in VLAN $vlan as tagged" );
        $str = "T $name [pvid=$PVIDs{$host}{$port}], ";
        $count++;
      } else {
        log_msg( 2, "something unexpected happened!" );
      }
      if (defined($str)) {
        $p += length($str);
        if ($p > $opts{termcolumns}) {
          $line .= sprintf("\n" . " " x 6);
          $p = 7 + length($str);
        }
        $line .= $str;
      }
    }
    my @print_vlans;
    if (defined($opts{vlans})) {
      @print_vlans = split(/,/, $opts{vlans});
    }
    next if defined($opts{vlans}) and grep(/^$vlan$/, @print_vlans) == 0;
    $line .= "[empty]" if $count == 0;
    print "$line\n" unless !defined($opts{showempty}) and $count == 0;
  }
  print "\n";
}

sub processHexString {
  my ( $host, $vlan, $type, $data ) = @_;

  $data =~ s/\s*//g;
  log_msg( 9, "hex string='$data'" );
  my $binary = unpack("B*", pack("H*", $data));
  log_msg( 10, "binary='$binary'" );
  $VLANs{$host}{$vlan}{$type} .= $binary;
}

sub getPortNameByNumber {
  my ( $model, $port ) = @_;

  if ($model eq "Raptor") {
    return getRaptorPortNameByNumber($port);
  } elsif ($model eq "Dell PowerConnect") {
    return getDellPortNameByNumber($port);
  } elsif ($model eq "Unknown") {
    return $port;
  } else {
    return undef;
  }
}

sub getRaptorPortNameByNumber {
  my ( $port_num ) = @_;

  # Unit and port numbering is 1-based while slot numbering is 0-based
  my $unit = int(($port_num-1) / 48) + 1;
  my $slot = int((($port_num-1) % 48) / 12);
  my $port = int((($port_num-1) % 48) % 12) + 1;

  if (($slot >= 2 and $slot <= 3) and ($port > 3 or $port < 1)) {
    # slots 2 and 3 can only have 3 ports each (ports are numbered 1-3)
    # print "invalid port - $port_num - $unit/$slot/$port\n";
    return undef;
  } elsif (($slot >= 0 and $slot <= 1) and ($port > 12 or $port < 1)) {
    # slots 0 and 1 can only have 12 ports each (ports are numbered 1-12)
    # print "invalid port - $port_num - $unit/$slot/$port\n";
    return undef;
  }
  return "$unit/$slot/$port";
}

sub getDellPortNameByNumber {
  my ( $port_num ) = @_;
  return undef if $port_num < 1 or $port_num > 24;
  return "g$port_num";
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

  0.1.0   9 Apr 08     ctracy     started

=cut

# Local variables:
# tab-width: 2
# perl-indent-level: 2
# indent-tabs-mode: nil
# comment-column: 40
# End:

