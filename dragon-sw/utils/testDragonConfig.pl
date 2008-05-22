#!/usr/bin/perl -W
#
# testDragonConfig.pl - test DRAGON configuration by setting up LSPs 
# or performing path computation requests
#
# (C) 2007 by Mid-Atlantic Crossroads (MAX).  All rights reserved.
#
# Time-stamp: <2007-12-13 14:24:07 EDT>
# $Id: testDragonConfig.pl,v 1.1 2007/12/13 14:24:07 ctracy Exp $
#
# Author: Chris Tracy <chris@maxgigapop.net>
#
# Description:
#
# Longer description
#
use strict;
use vars qw($ORIGINAL_SCRIPT $P $VERSION $VERBOSE $OPTS $USAGE $DESCR $AUTHOR
            $COPYRIGHT $ARGS_DESC $LOG_STDERR $LOG_FILE $LOG_FP $LOG_TSTAMP_FMT
            $DEFAULTS);
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
use Net::Telnet;
use Getopt::Long;
use IO::File;
##
$DEFAULTS =
  { MODE       => q{random},
    TIMING     => q{slow},
    HOSTS      => q{10.0.1.11,10.0.1.12,10.0.1.21,10.0.1.22},
    BANDWIDTHS => q{eth100M},
    NARBS      => q{10.0.1.13,10.0.1.23},
    NARBTEST   => 1,
    RCETEST    => 0,
    LSPSETUP   => 0,
    LSPID      => q{1000},
    LOG        => undef,
    TIMESTAMPS => 1,
  };
$VERSION = '0.1.1';
$DESCR = 'test DRAGON configuaration with LSP or PCE requests';
$AUTHOR = 'Chris Tracy <chris@maxgigapop.net>';
$VERBOSE = 1;
$COPYRIGHT = '(C) 2007 by Mid-Atlantic Crossroads (MAX).  All rights reserved.';
$LOG_STDERR = 1;
$LOG_FILE = undef;
$LOG_FP = undef;
$LOG_TSTAMP_FMT = '%Y-%m-%d %H:%M:%S';
$USAGE = <<__UsAGe__;
 options:
           --mode=[random|iterative]   random or iterative src/dst (default=random)
           --hosts=[IP,{IP,..}]        list of IP addresses of hosts
           --narbs=[IP,{IP,..}]        IP addresses of NARB/RCE (default=127.0.0.1)
           --timing=[slow|fast]        set timing mode (default=slow)
           --bandwidths=[eth100M,{..}] randomly choose BW values from this list
           --lspsetup=[0|1]            setup/provision LSP (default=0)
           --narbtest=[0|1]            run narb_test and show results (default=1)
           --rcetest=[0|1]             run rce_test and show results (default=0)
           --lspid=[random|start_val]  lsp-id/tunnel-id: random or starting val (default=1000)
           --noreverse                 only test forward path setup (A->B only not B->A)
           --delete                    randomly delete LSPs (default is no deletion)
           --log=[filename]            log output to filename (default is no logging)
           --timestamps=[0|1]          include timestamps in output (default=1)
           -h                          print this message and exit
           -v                          be verbose
__UsAGe__
##

# TODO: also clean up all LSPs when finished ???
$SIG{INT} = "printstats"; # traps keyboard interrupt

my %opts = ('mode'       => $DEFAULTS->{MODE},
            'timing'     => $DEFAULTS->{TIMING},
            'hosts'      => $DEFAULTS->{HOSTS},
            'bandwidths' => $DEFAULTS->{BANDWIDTHS},
            'narbs'      => $DEFAULTS->{NARBS},
            'lspsetup'   => $DEFAULTS->{LSPSETUP},
            'narbtest'   => $DEFAULTS->{NARBTEST},
            'rcetest'    => $DEFAULTS->{RCETEST},
            'lspid'      => $DEFAULTS->{LSPID},
            'log'        => $DEFAULTS->{LOG},
            'timestamps' => $DEFAULTS->{TIMESTAMPS},
            );

usage() unless scalar(@ARGV);
usage() unless 
    GetOptions (\%opts, 
                "h",         # help
                "v",         # verbose
                "mode=s",    # mode = random or iterative
                "lspsetup=s",
                "narbtest=s",
                "rcetest=s",
                "noreverse",
                "delete",
                "timing=s",
                "hosts=s",
                "bandwidths=s",
                "narbs=s",
                "lspid=s",
                "log=s",
                "timestamps=s",
                );
usage(undef, 1) if $opts{h};
$LOG_FILE = $opts{log};
log_msg(1,qq{our args: }.join(' // ',@ARGV));

my %LSPs;
my @HOSTS = split(/,\s*/, $opts{'hosts'});
my @BANDWIDTHS = split(/,\s*/, $opts{'bandwidths'});
my @NARBS = split(/,\s*/, $opts{'narbs'});

# start randomly deleting an LSP after this # of LSPs is reached
my $start_deleting_thresh = 2;

# if random, pick 2 end systems and try to setup an LSP...
# if iterative mode, try all combinations/permutations...

my $total_narb_requests = 0;
my $failed_narb_requests = 0;
my $successful_narb_requests = 0;

my $tot_lsps = 0;
my $num_lsps = 0;

if ($opts{'mode'} =~ /^random$/i) {
  my $lsp_id = ""; 
  $lsp_id = $opts{'lspid'} if $opts{'lspid'} ne "random";
  while ( 1 ) {
    my $a = int(rand($#HOSTS));
    my $z = int(rand($#HOSTS));
    if ($opts{'lspid'} eq "random") {
      $lsp_id = int(rand(65535));
    }
    my $bw = int(rand($#BANDWIDTHS));
    next if $a == $z;
    next if defined($LSPs{$lsp_id});
    
    my $found_rev = 0;
    foreach my $t (keys %LSPs) {
	    $found_rev = 1 if ($LSPs{$t}{'src'} eq $HOSTS[$z] and 
                         $LSPs{$t}{'dst'} eq $HOSTS[$a]);
    }
    next if $opts{'noreverse'} and $found_rev;
    
    &setup_lsp($a, $z, $lsp_id, $bw);
    if ($opts{'lspid'} ne "random") {
      $lsp_id++;
    }
  }
} elsif ($opts{'mode'} =~ /^iterative$/i) {
  my $lsp_id = ""; 
  $lsp_id = $opts{'lspid'} if $opts{'lspid'} ne "random";
  for my $a (0..$#HOSTS) {
    for my $z (0..$#HOSTS) {
      if ($opts{'lspid'} eq "random") {
        $lsp_id = int(rand(65535));
      }
	    my $bw = int(rand($#BANDWIDTHS));
	    next if $a == $z;
	    next if defined($LSPs{$lsp_id});
	    
	    my $found_rev = 0;
	    foreach my $t (keys %LSPs) {
        $found_rev = 1 if ($LSPs{$t}{'src'} eq $HOSTS[$z] and 
                           $LSPs{$t}{'dst'} eq $HOSTS[$a]);
	    }
	    next if $opts{'noreverse'} and $found_rev;
      
	    &setup_lsp($a, $z, $lsp_id, $bw);
      if ($opts{'lspid'} ne "random") {
        $lsp_id++;
      }
    }
  }
}
##
sub setup_lsp {
  my $a = shift;
  my $z = shift;
  my $lsp_id = shift;
  my $bw = shift;
  
  $LSPs{$lsp_id}{'name'} = "test$lsp_id";
  $LSPs{$lsp_id}{'src'} = $HOSTS[$a];
  $LSPs{$lsp_id}{'dst'} = $HOSTS[$z];
  $LSPs{$lsp_id}{'bandwidth'} = $BANDWIDTHS[$bw];
  
  log_msg(1,"\nA: $HOSTS[$a]\nZ: $HOSTS[$z]\n");
  
#    print "skip this lsp? (YES/no): ";
#    my $resp = <STDIN>;
#    return unless $resp =~ /^no$/i;
  
  $total_narb_requests++;

  my $narb_fail = 0;

  if ($opts{'narbtest'}) {
    $narb_fail = 1;
  NARBTEST: foreach my $narb_ip (@NARBS) {
    log_msg(1,"** Asking NARB $narb_ip for the path with narb_test ...");

    open I, "/usr/local/dragon/sbin/narb_test -H $narb_ip -S $HOSTS[$a] -D $HOSTS[$z] -V -a -b100|";
    my @lines = <I>;
    close I;
    foreach (@lines) {
      chomp;
      s/\[\d+\/\d+\/\d+\s+\d+:\d+:\d+\]// if !$opts{timestamps};
      log_msg(1,$_);
    }
    if (grep(/request\s+successful/i, @lines)) {
      $narb_fail = 0;
      last NARBTEST;
    }
  }
  } else {
    $narb_fail = 0;
  }

  my $rce_fail = 0;

  if ($opts{'rcetest'}) {
    $rce_fail = 1;
  RCETEST: foreach my $narb_ip (@NARBS) {
    log_msg(1,"** Asking RCE $narb_ip for the path with rce_test ...");
    open I, "/usr/local/dragon/sbin/rce_test -H $narb_ip -S $HOSTS[$a] -D $HOSTS[$z] -V -a -b100|";
    my @lines = <I>;
    close I;
    foreach (@lines) {
      chomp;
      s/\[\d+\/\d+\/\d+\s+\d+:\d+:\d+\]// if !$opts{timestamps};
      log_msg(1,$_);
    }
    if (grep(/request\s+successful/i, @lines)) {
      $rce_fail = 0;
      last RCETEST;
    }
  }
  } else {
    $rce_fail = 0;
  }
  
  if ($narb_fail or $rce_fail) {
    log_msg(1,"*** NARB or RCE could not calculate a path...");
    $failed_narb_requests++;
  } else {
    $successful_narb_requests++;
  }
  
  if ($opts{'narbtest'} or $opts{'rcetest'}) {
    sleep 1 if $opts{'timing'} eq "slow";
  }
  
  if ($opts{'lspsetup'}) {
    log_msg(1,"*** in 1 sec, will telnet $HOSTS[$a] 2611 to setup this LSP");
    sleep 1 if $opts{'timing'} eq "slow";
    my $t = new Net::Telnet;
    $t->open(Host => $HOSTS[$a],
             Port => 2611,
             Timeout => 3,
             Errmode => 'return'
             );
    
    if ($t->errmsg) {
      log_msg(1,"ERROR: " . $t->errmsg);
      return;
    }
    
    my $res;
    my ($r, $s);
    ($r, $s) = $t->waitfor('/Password:.*$/');
#    $res .= $r . $s;
    $t->print('dragon');
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    $t->print("edit lsp test$lsp_id");
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    $t->print("set source ip-address $HOSTS[$a] lsp-id $lsp_id destination ip-address $HOSTS[$z] tunnel-id $lsp_id");
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    $t->print("set bandwidth $BANDWIDTHS[$bw] swcap l2sc encoding ethernet gpid ethernet");
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    $t->print('set vtag any');
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    $t->print('exit');
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    $t->print("commit lsp test$lsp_id");
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    
    log_msg(1,"$res");
    $res = "";
    if ($opts{'timing'} eq "slow") {
      log_msg(1,"...waiting 15 secs for LSP to setup...");
      sleep 15;
    } else {
      log_msg(1,"...waiting 5 secs for LSP to setup...");
      sleep 5;
    }
    
    $t->print("sh lsp");
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    $t->print("sh lsp test$lsp_id");
    
    ($r, $s) = $t->waitfor('/(>|#).*$/');
    $res .= $r . $s;
    
    log_msg(1,"$res");
    
    $t->close;
    
    $num_lsps++;
    $tot_lsps++;
  
    log_msg(1,"...pausing for 10 secs...");
    sleep 10 if $opts{'timing'} eq "slow";
  
    # delete an LSP if we are above the threshold and --delete was specified
    if (defined($opts{'delete'}) and ($num_lsps > $start_deleting_thresh)) {
      
      my @lsp_list = keys %LSPs;
      my $rand_lsp = int(rand($#lsp_list));
      my $rand_lsp_name = $lsp_list[$rand_lsp];
      
      next if !defined($LSPs{$rand_lsp_name}{'src'});
      log_msg(1,"*** going to delete LSP test$rand_lsp_name from end system $LSPs{$rand_lsp_name}{'src'}");
      
      log_msg(1,"*** telnet $LSPs{$rand_lsp_name}{'src'} 2611");
      my $t = new Net::Telnet;
      $t->open(Host => $LSPs{$rand_lsp_name}{'src'},
               Port => 2611,
               Timeout => 10,
               Errmode => 'return'
               );
      
      if ($t->errmsg) {
        log_msg(1,"ERROR: " . $t->errmsg . "");
        return;
      }
      
      my $res;
      my ($r, $s);
      ($r, $s) = $t->waitfor('/Password:.*$/');
#	$res .= $r . $s;
      $t->print('dragon');
      
      ($r, $s) = $t->waitfor('/(>|#).*$/');
      $res .= $r . $s;
      $t->print("delete lsp test$rand_lsp_name");
      
      ($r, $s) = $t->waitfor('/(>|#).*$/');
      $res .= $r . $s;
      $t->print("delete lsp test$rand_lsp_name");
      
      ($r, $s) = $t->waitfor('/(>|#).*$/');
      $res .= $r . $s;
      $t->print("sh lsp test$rand_lsp_name");
      
      ($r, $s) = $t->waitfor('/(>|#).*$/');
      $res .= $r . $s;
      log_msg(1,"$res");
      
      undef $LSPs{$lsp_id};
      $num_lsps--;
      
      log_msg(1,"...pausing for 10 secs...");
      sleep 10 if $opts{'timing'} eq "slow";
    }
  }
}

&printstats;

exit(0);

sub printstats {
  
  log_msg(1,"\n\n");
  
  foreach my $n (sort keys %LSPs) {
    log_msg(1,"LSP name: $LSPs{$n}{'name'}");
    log_msg(1,"     src: $LSPs{$n}{'src'}");
    log_msg(1,"     dst: $LSPs{$n}{'dst'}");
    log_msg(1,"      bw: $LSPs{$n}{'bandwidth'}");
    log_msg(1,"");
  }
  
  log_msg(1,"number of LSPs setup now: $num_lsps");
  log_msg(1,"total LSPs: $tot_lsps");
  
  log_msg(1,"narb requests (good/bad/total): $successful_narb_requests / $failed_narb_requests / $total_narb_requests");
  
  exit(0);
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
#  my $logmsg = "$P: " . ts() . " [$lvl] @_\n";
  my $logmsg = "@_\n";
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
  0.1.1   04 Mar 08     ctracy     support for logging to file

=cut

# Local variables:
# tab-width: 2
# perl-indent-level: 2
# indent-tabs-mode: nil
# comment-column: 40
# End:
