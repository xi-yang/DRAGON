#!/usr/bin/perl -w
#

use POSIX qw(setsid);
use Getopt::Long;
use FindBin;
use sigtrap;
use FileHandle;
use Sys::Syslog qw(:DEFAULT setlogsock);
use Fcntl ':flock';
use Net::Telnet;

use lib "$FindBin::Bin/lib";
use strict;
#use warnings;

##############################
### Begin of main process
##############################

my %TASK_DOERs = (
        'query-mon-api' => \&do_task_mon_api,
        'query-dragon-cli' => \&do_task_dragon_cli,
        'lookup-dragon-log' => \&do_task_dragon_log,
        'lookup-rsvpd-log' => \&do_task_rsvpd_log,
        'check-vlsr-crash' => \&do_task_vlsr_crash,
        'check-narb-crash' => \&do_task_narb_crash,
        'check-vlsr-tl1' => \&do_task_vlsr_tl1,
        'check-tunnel-ping' => \&do_task_tunnel_ping,
        'query-rce-topology' => \&do_task_rce_topology,
        'test' => \&do_task_test,
);

my %cfg = (
	"config" => "/etc/dcn-mtk-agent.conf", 
	"cli_passwd" => 'dragon', 
	"mon_apiclient" => '/usr/local/dragon/bin/mon_apiclient', 
	"log" => "/var/log/dcn-mtk-agent.log"
);

my @tasks = ();

### Log related declaration
# Flags to Log::open specyfing where the output should go.
use constant SYSLOG => (2**0);
use constant ERRLOG    => (2**1);
use constant OUTLOG    => (2**2);
use constant FILELOG   => (2**3);

# levels of debug messages allowed, log all by default
use constant N => 32;

# mask used when logging (init from syslog)
my $syslog_mask = setlogmask(0);

# submask used only for debug messages (default to log all)
my $debug_mask = "";
vec($debug_mask, 0, N) = (2**N-1);

# log destination (default to STDERRLOG)
my $log_dest = ERRLOG;

# log file handle
my $log_fh;

sub log_open;
sub log_print;
###

# Get the command-line arguments:
sub usage() {
	print("usage: mtk-agent.pl [task1, task2 ...] (example tasks: 'query-dragon-cli', 'query-mon-api'\n");
	print("       [-h] [-u <username>] [-c <config_file>] [-p <password>]\n");
	print("       -L: <lsp_name> -M: <mon_api_cmd> -S: <start_time> -T: <end_time>\n");
	print("  Long options:\n");
	print("  \n");
	print("  Example runs:\n");
	print("   /usr/local/dragon/bin/mtk-agent.pl query-dragon-cli -L test1\n");
	print("   /usr/local/dragon/bin/mtk-agent.pl query-rce-topology\n");
	print("   /usr/local/dragon/bin/mtk-agent.pl query-mon-api -m '\-l'\n");
	print("  \n");
	exit;
}

sub process_opts($$) {
	my ($n, $v) = @_;
	my $k1 = undef;
	if(($n eq "c") || ($n eq "config")) 	{$k1 = "config";}
	if(($n eq "p") || ($n eq "password")) 	{$k1 = "cli_passwd";}
	if(($n eq "L") || ($n eq "lsp_name")) 	{$k1 = "lsp_name";}
	if(($n eq "M") || ($n eq "mon_api_cmd")) {$k1 = "mon_api_cmd";}
	if(($n eq "S") || ($n eq "start_time"))	{$k1 = "start_time";}
	if(($n eq "T") || ($n eq "end_time")) 	{$k1 = "end_time";}
	if(($n eq "G") || ($n eq "gre_tunnel"))	{$k1 = "gre_tunnel";}

	if(defined($k1)) {
		$cfg{$k1} = $v;
	}
	else {
		die "option error";
	}
}

sub process_tasks {
	push @tasks, @_;
}

$| = 1;

# Gracefully handle termination signals.
$SIG{HUP} = $SIG{INT} = sub { exit };


log_open(FILELOG, "info warning err", $cfg{'log'});
log_print ("info", "==================\n");
log_print ("info", "...starting MTK Agent...\n");
log_print ("info", "==================\n");

if(!GetOptions ('c=s' =>		\&process_opts,
		'config=s' =>		\&process_opts,
		'p:s' =>		\&process_opts,
		'password:s' =>		\&process_opts,
		'L=s' =>		\&process_opts,
		'lsp_name=s' =>		\&process_opts,
		'M:s' =>		\&process_opts,
		'mon_api_cmd:s' =>	\&process_opts,
		'S=s' =>		\&process_opts,
		'start_time=s' =>	\&process_opts,
		'T=s' =>		\&process_opts,
		'end_time=s' =>		\&process_opts,
		'G=s' =>		\&process_opts,
		'gre_tunnel=s' =>	\&process_opts,
		'h' => 			\&usage,
		'help' => 		\&usage,
		'<>' =>			\&process_tasks,)) {
	usage();
}

die "###Option error: please give me at least one task, such as 'query-dragon-cli'!###" unless (@tasks);

foreach my $task (@tasks) {
        unless (defined($TASK_DOERs{$task})) {
		print "\n###Unknown task name: $task! ###\n";
		next;
	}
	print "\n===Starting Task: '$task' ===\n";
	$TASK_DOERs{$task}(\%cfg);
}


######### TASK Functions #####

#Task-1

sub do_task_mon_api($) {
        my ($cfg) = @_;
        my $cr = `$cfg->{'mon_apiclient'} $cfg->{'mon_api_cmd'}`;
        print $cr;
        return 1;
}

#Task-2

sub do_task_dragon_cli($) {
        my ($cfg) = @_;
        my $session = connect_dragon_cli($cfg->{'cli_passwd'});
        unless ($session) {
                print "###Failed to login to DRAGON CLI!###\n";
                return 0;
        }
        my $cr = execute_cli_cmd($session, "show lsp $cfg->{'lsp_name'}");
        print "DRAGON CLI:  $cr\n";
        disconnect_dragon_cli($session);
        return 1;
}

#Task-3

sub do_task_dragon_log($) {
        my ($cfg) = @_;
        grep_logging_msgs('/var/log/dragon.log', $cfg->{'lsp_name'}, $cfg->{'start_time'}, $cfg->{'end_time'});
        return 1;
}

sub do_task_rsvpd_log($) {
        my ($cfg) = @_;
        opendir my ($dir), "/var/log/";
        my @files = sort { eval('-M "/var/log/$a" <=> -M "/var/log/$b"') } grep { -f "/var/log/$_" } readdir $dir;
        foreach my $file (@files) {
                if ($file eq 'RSVPD.log') {
                        `cp -rf /var/log/$file /tmp/RSVPD.log.tmp`;
                }
                elsif ($file =~/(RSVPD\.log\.(\d)+\.gz)/) {
                        `gunzip -c /var/log/$1  > /tmp/RSVPD.log.tmp`;
                        $file = '/tmp/RSVPD.log.tmp';
                }
                else { next; }
                grep_logging_msgs('/tmp/RSVPD.log.tmp', $cfg->{'lsp_name'}, $cfg->{'start_time'}, $cfg->{'end_time'});
        }
        closedir $dir;
        return 1;
}

sub grep_logging_msgs {
        my ($file_name, $lsp_name, $start_time, $end_time) = @_;
        my $ok = open(FILE, "<$file_name");
        unless ($ok) {
                print "###Cannot open : $file_name ###\n";
                log_print ("err", "cannot open log file ( $file_name ) for parsing.\n");
                return 0;
        }
        my @lines = grep { /LSP= $lsp_name :/ } <FILE>;
        foreach my $line (reverse @lines) {
                $line =~ /(\d{2}:\d{2}:\d{2})/;
                if ($1 ge $start_time && $1 lt $end_time) {
                        print $line;
                }
        }
        close FILE;
        return 1;
}

#Task-5:
# checking if any VLSR daemon has crashed

sub do_task_vlsr_crash($) {
        my ($cfg) = @_;
        my $stdout = `/usr/local/dragon/bin/dragon.sh status`;
        if ($stdout =~ /zebra daemon is NOT running/) {
                print ("Detected crash: zebra is NOT running!\n");
                log_print ("err", "Detected crash: zebra is NOT running!\n");
                return 0;
        }
        if ($stdout =~ /intra-domain ospf daemon is NOT running/) {
                print ("Detected crash: ospfd is NOT running!\n");
                log_print ("err", "Detected crash: ospfd is NOT running!\n");
                return 0;
      }
        if ($stdout =~ /rsvp daemon is NOT running/) {
                print ("Detected crash: rsvpd is NOT running!\n");
                log_print ("err", "Detected crash: rsvpd is NOT running!\n");
                return 0;
        }
        if ($stdout =~ /dragon daemon is NOT running/) {
                print ("Detected crash: dragon is NOT running!\n");
                log_print ("err", "Detected crash: dragon is NOT running!\n");
                return 0;
        }

        $stdout = `/usr/local/dragon/bin/mon_apiclient -s`;
        if ($stdout =~ /Error Code: 0xf0f0/) {
                print ("Detected hangup: rsvpd is stuck!\n");
                log_print ("err", "Detected hangup: rsvpd got stuck!\n");
                return 0;
        }

        return 1;
}

#Task-6:
# checking if any NARB daemon has crashed

sub do_task_narb_crash($) {
        my ($cfg) = @_;
        my $stdout = `/usr/local/dragon/bin/dragon.sh status`;
        if ($stdout =~ /zebra daemon is NOT running/) {
                print ("Detected crash: zebra is NOT running!\n");
                log_print ("err", "Detected crash: zebra is NOT running!\n");
                return 0;
        }
        if ($stdout =~ /intra-domain ospf daemon is NOT running/) {
                print ("Detected crash: ospfd-intra is NOT running!\n");
                log_print ("err", "Detected crash: ospfd-intra is NOT running!\n");
                return 0;
        }
        if ($stdout =~ /inter-domain ospf daemon is NOT running/) {
                print ("Detected crash: ospfd-inter is NOT running!\n");
                log_print ("err", "Detected crash: ospfd-inter is NOT running!\n");
                return 0;
        }
        if ($stdout =~ /narb daemon is NOT running/) {
                print ("Detected crash: narb is NOT running!\n");
                log_print ("err", "Detected crash: narb is NOT running!\n");
                return 0;
        }
        if ($stdout =~ /rce daemon is NOT running/) {
                print ("Detected crash: rce is NOT running!\n");
                log_print ("err", "Detected crash: rce is NOT running!\n");
                return 0;
        }
        return 1;

}

# Task-7
# check tl1 connectivity for all core directors configured with the vlsr
sub do_task_vlsr_tl1($) {
        my ($cfg) = @_;
        my $ok = open(FILE, '/usr/local/dragon/etc/ospfd.conf');
        unless ($ok) {
                print "###Cannot open : /usr/local/dragon/etc/ospfd.conf ###\n";
                log_print ("err", "cannot open /usr/local/dragon/etc/ospfd.conf \n");
                return 0;
        }
        my %CDs;
        foreach my $line (<FILE>) {
                if ($line =~ /subnet-uni\s(\d)+\s(\w+)\s.+uni-n-ipv4\s([^\s]+)\s/){
                        $CDs{$1} = $2;
                }
        }
        close FILE;

        foreach my $cd_name (keys %CDs) {
                my $cd_ip = $CDs{$cd_name};
                my $tl1 = connect_tl1($cd_ip, $cfg->{'tl1_port'}, $cfg->{'tl1_user'}, $cfg->{'tl1_passwd'});
                unless ($tl1) {
                        print ("Detected loss of TL1 connectivity to $cd_name ($cd_ip)!\n");
                        log_print ("err", "Detected loss of tl1 connectivity to $cd_name ($cd_ip)!\n");
                        return 0;
                }
                disconnect_tl1($tl1, $cfg->{'tl1_passwd'});
        }
        return 1;
}

#Task-8
# check control channel (gre tunnel) connectivity

sub do_task_tunnel_ping($) {
        my ($cfg) = @_;
        my @tunnels;
        if (lc($cfg->{'gre_tunnel'}) eq 'all') {
                my $ok = open(FILE, '/usr/local/dragon/etc/RSVPD.conf');
                unless ($ok) {
                        print "###Cannot open : /usr/local/dragon/etc/RSVPD.conf ###\n";
                        log_print ("err", "cannot open /usr/local/dragon/etc/RSVPD.conf \n");
                        return 0;
                }
                foreach my $line (<FILE>) {
                        if ($line =~ /interface\s([^\s]+)\s/) {
                                push @tunnels, $1;
                        }
                }
                close FILE;
        }
        else {

                push @tunnels, $cfg->{'gre_tunnel'};
        }

        # ifconfig to get tunnel ip
        foreach my $tunnel (@tunnels) {
                my $output = `/sbin/ifconfig $tunnel`;
                if ($output =~ /Device not found/) {
                        print ("Detected unknown control channel $tunnel!\n");
                        log_print ("err", "Detected unknown control channel $tunnel!\n");
                        return 0;
                }
                # check if tunnel is connected
                if ($output =~ /inet\s[^\d]*(\d+\.\d+\.\d+\.\d+)\s/){
                        my $tunnel_peer = get_slash30_peer($1);
                        $output = `ping -c3 $tunnel_peer`;
                        unless ($output =~ /0% packet loss/) {
                                print ("Detected unpingable control channel $tunnel!\n");
                                log_print ("err", "Detected unpingable control channel $tunnel!\n");
                                return 0;
                        }
                }
        }

        return 1;
}

sub get_slash30_peer($) {
        my ($A, $B, $C, $D) = split(/\./, shift);
        if ($D % 4 == 1) {
                $D += 1;
        } elsif ($D % 4 == 2) {
                $D -= 1;
        }
        return ($A.'.'.$B.'.'.$C.'.'. $D);
}

#Task-9

sub do_task_rce_topology($) {
        my ($cfg) = @_;
        my $session = &connect('localhost', '2688', '', $cfg->{'cli_passwd'}, 'dragon', '');
        unless ($session) {
                print "###Failed to login to RCE CLI!###\n";
                return 0;
        }
        my $cr = execute_cli_cmd($session, "show topology intradomain");
        print "RCE CLI:  $cr\n";
        disconnect($session, "exit\n");
        return 1;
}


#Test Task

sub do_task_test($) {
        my ($cfg) = @_;
        return 1;
}


######### CLI Functions ######

my $global_ctag = 123; #Globally unique CMD CTAG
sub get_ctag {
        return $global_ctag++;
}

sub connect {
        my ($host, $port, $user, $pass, $mode, $login_str) = @_;
        my $t = new Net::Telnet(Binmode => 0);
        log_print ("info", "CLI connecting to $host:$port'\n");
        my $res = $t->open(Host => $host,
                Port => $port,
                Timeout => 10,
                Errmode => 'return'
        );

        if (!defined($res)) {
                log_print ("err", "Failed to connect to $host:$port -- " . $t->errmsg . "\n");
                return 0;
        }

        if ($mode eq 'telnet') {
                $t->login($user, $pass) or return 0;
        } elsif ($mode eq 'dragon') {
                read_shell($t, '/[pP]assword:.*$/', 10) or return 0;
                $t->print($pass . "\n");
                read_shell($t, '/>.*$/', 5) or return 0;
        } elsif ($mode eq 'tl1') {
                my $ctag = get_ctag();
                read_shell($t, '/;.*$/', 5) or return 0;
                $t->print("act-user::$user:" . $ctag . "::$pass;");
                my $r = read_shell($t, '/;.*$/', 10) or return 0;
                unless ($r =~ /Successful/) {
                        log_print ("err", "failed in TL1 login -- wrong user/password?\n");
                        return 0;
                }
                $ctag = get_ctag();
                $t->print("inh-msg-all:::" . $ctag . ";");
                $r = read_shell($t, '/;.*$/', 5) or return 0;
        } else {
                log_print ("err", "Unknown CLI mode\n");
                return 0;
        }

        return $t;
}

sub disconnect {
        my ($session, $exit_str) = @_;
        $session->print($exit_str);
        $session->close();
}

sub connect_dragon_cli {
        my ($passwd) = @_;
        return &connect('localhost', '2611', '', $passwd, 'dragon', '');
}

sub disconnect_dragon_cli {
        my ($session) = @_;
        disconnect($session, "exit\n\exit\n");
}

sub read_shell {
        my ($session, $pattern, $timeout) = @_;
        my ($r, $s);
        ($r, $s) = $session->waitfor(Match => $pattern, Timeout => $timeout, Errmode => 'return');
        unless ($s) {
                log_print ("err", "failure in CLI::read_shell (pattern: $pattern)\n");
                return 0;
        }
        return $r;
}

sub execute_cli_cmd {
        my ($session, $cmd) = @_;
        $session->print($cmd);
        my ($r, $m);
        ($r, $m) = $session->waitfor(Match => '/[>#].*$/', Timeout => 10, Errmode => 'return');
        unless ($m) {
                log_print ("err", "failure in CLI::execute_cli_cmd (command: $cmd)\n");
                return ('');
        }
        return $r;
}

sub connect_tl1 {
        my ($host, $port, $user, $pass) = @_;
        return &connect ($host, $port, $user, $pass, 'tl1', '');
}

sub disconnect_tl1 {
        my ($session, $user) = @_;
        disconnect($session, "canc-user::$user:" . get_ctag() . ";");
}

######### LOG Functions ######

sub log_open {
  my ($mask, $file);
  ($log_dest, $mask, $file) = @_;
  # set mask value(s)
  $syslog_mask = setlogmask(0);
  if(defined $mask) {
    # if a mask was passed, clear mask
    vec($debug_mask, 0, N) = 0;
    foreach my $i (split /\W+/, $mask) {
      if($i =~ /(\D+)(\d+)?/) {
        # for each level in the mask, set the corresponding bit
        my $sublevel = ($2 ? $2 : 0);
        $syslog_mask |= Sys::Syslog::LOG_MASK(Sys::Syslog::xlate($1));
        # if we have a debug sublevel, add it to the mask
        if($sublevel) { vec($debug_mask, $sublevel, 1) = 1; }
      } else { die "Log::open"; }
    }
  } else { vec($debug_mask, 0, N) = (2**N-1); }
  # if we're logging to syslog, initialize it
  if($log_dest & SYSLOG) {
    setlogsock('unix');
    openlog "dcn_mtk", "pid,cons", "user";
    setlogmask $syslog_mask;
  }
  # if we are (also) logging to as file, open it
  if($log_dest & FILELOG) {
    if($file) {
      $log_fh = new FileHandle;
      open $log_fh, ">>$file" or die "Log::open";
      $log_fh->autoflush;
    } else { die "Log::open"; }
  }
  return 1;
}

sub log_close {
  # close syslog and log file, if we were using them
  if($log_dest & SYSLOG) { closelog; }
  if($log_dest & FILELOG) { close $log_fh or die "Log::close"; }
  return 1;
}

sub log_print {
  my ($level, @args) = @_;
  my $sublevel = 0;
  my $at = $@;
  unless(defined $level and defined $args[0]) { die "log"; }
  # if we have one of our homegrown debug levels to log, grab the sublevel
  if($level =~ /debug(\d+)/) { ($level, $sublevel) = ("debug", $1); }
  # are we suppposed to print this?
  if(not defined $syslog_mask or
     $syslog_mask & Sys::Syslog::LOG_MASK Sys::Syslog::xlate $level and
     not $sublevel or vec($debug_mask, $sublevel, 1)) {
    # ok, print it to log(s)
    if($log_dest & SYSLOG) { syslog $level, @args; }
    # now that we have syslogged, feel free to clobber @args
    my $msg = ($#args ? sprintf shift @args, @args : $args[0]);
    chomp $msg;
    if($log_dest & ERRLOG) { print STDERR "$msg\n"; }
    if($log_dest & OUTLOG) { print STDOUT "$msg\n"; }
    # if we print to a file, add syslog-like timestamp
    $msg = scalar(localtime) . " $0\[$$\]: $msg\n";
    if($log_dest & FILELOG) {
      flock $log_fh, LOCK_EX or die "log";
      print $log_fh $msg;
      flock $log_fh, LOCK_UN or die "log";
    }
  }
  # restore global variables
  $@ = $at;
  return 1;
}


##############################
### End of main process
##############################

END {
	log_print ("info", "...terminating MTK Agent...\n\n\n");
	close();
}
