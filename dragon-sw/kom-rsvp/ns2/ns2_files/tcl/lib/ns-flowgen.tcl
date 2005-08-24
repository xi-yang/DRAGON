#- code for traffic generation support ------------------------------------------------------------

Class Flow

Flow instproc init {} {
	$self instvar started_ fid_
	set started_ false
	set fid_ 0
}

# syntax: connect <src node> <dest node> {UDP|TCP}
#
# creates the source and sink agents of transport protocol <type>;
# installs them on <src> node and <dst> node, respectively;
Flow instproc connect { src dst type } {
	$self instvar srcAgent_ dstAgent_ fid_ recvapp_

	# create transport agents
	switch $type {
		UDP {
			set srcAgent_ [new Agent/UDPSource] ;# use our own special UDP agents which support
			set dstAgent_ [new Agent/UDPSink]   ;# collecting flow statistics
		}
		TCP {
			set srcAgent_ [new Agent/TCP/Newreno]
			set dstAgent_ [new Agent/TCPSink]
		}
		default {
			puts "error: unknown flow type $type!"
			exit 1
		}
	}

	$srcAgent_ set class_ $fid_
	set ns [Simulator instance]
	$ns attach-agent $src $srcAgent_
	$ns attach-agent $dst $dstAgent_
	$ns connect $srcAgent_ $dstAgent_

	# set up receiver
	set recvapp_ [new Application/FlowReceiver]
	$recvapp_ attach-agent $dstAgent_

	# set up RSVP signalling at receiver
	if { [[$dstAgent_ set node_] get-rsvp-api] != "" } {
		$recvapp_ register-with-api [[$dstAgent_ set node_] get-rsvp-api] [[$dstAgent_ set node_] node-addr] $fid_
	}

	# set up statistic collection (when using TCP logs received rate only)
	set jofel "-[$src node-addr]-[$dst node-addr]"
	$recvapp_ estimator "rate.received$jofel" 0.5
#	$recvapp_ ect-estimator "rate.ect$jofel" 0.5
	if { $type == "UDP" } {
		$srcAgent_ estimator "rate.sent$jofel" 0.5
		$recvapp_ delay-counter "delay$jofel" 1.0 500
	}
}

# syntax: traffic { CBR <packet size> <bit rate> <randomJitter?> }
#               | { Trace <tracefile> }
#               | { Pareto <packet size> <bit rate> <burst time> <idle time> <shape factor> }
#               | { Greedy }
#
# set up the traffic generator for this flow
Flow instproc traffic { type args } {
	$self instvar tg_
	switch $type {
		CBR {
			set tg_ [new Application/Traffic/$type]
			$tg_ set packetSize_ [lindex $args 0]
			$tg_ set rate_       [lindex $args 1]
			$tg_ set random_     [lindex $args 2]
		}
		Trace {
			set tg_ [new Application/Traffic/$type]
			$tg_ attach-tracefile [lindex $args 0]
		}
		Pareto {
			set tg_ [new Application/Traffic/$type]
			$tg_ set packetSize_ [lindex $args 0]
			$tg_ set rate_       [lindex $args 1]
			$tg_ set burst_time_ [lindex $args 2]
			$tg_ set idle_time_  [lindex $args 3]
			$tg_ set shape_      [lindex $args 4]
		}
		Greedy {
			set tg_ [new Application/FTP]
		}
		default {
			puts "error: unsupported traffic type $type!"
			exit 1
		}
	}
}

# syntax: start {0|1|2 <delay>|3|4 <prio>}
#
# starts a flow in one of four modes:
# 0: traffic starts immediately; no PATH message is being sent
# 1: a PATH message is sent and traffic starts immediately afterwards
# 2: a PATH message is sent and traffic starts <delay> seconds afterwards
# 3: a PATH message is sent and traffic starts after the first reservation;
# 4: traffic starts immediately with priority class
Flow instproc start { mode args } {
	$self instvar started_ rng_ tg_ srcAgent_ dstAgent_ fid_

	if ![info exists tg_] {
		puts "error: need to specify a traffic type first!"
		exit 1
	}

	set ns [Simulator instance]

	set src [$srcAgent_ set node_]
	set dst [$dstAgent_ set node_]
	if { [$src get-rsvp-api] != "" } {
		$ns at-now "[$src get-rsvp-api] session [$dst node-addr] $fid_ 42"
	}
	if { [$dst get-rsvp-api] != "" } {
		$ns at-now "[$dst get-rsvp-api] session [$dst node-addr] $fid_ 42"
	}

	if [info exists rng_] { $tg_ set rng_ $rng }
	$tg_ attach-agent $srcAgent_

	switch $mode {
		0 {
			$tg_ start
		}
		1 {
			if { [$src get-rsvp-api] != "" } {
				set rate [$tg_ set rate_]
				$ns at-now "[$src get-rsvp-api] sender $fid_ $rate [expr $rate*0.2] $rate 100 1500"
			}
			$tg_ start
		}
		2 {
			if { [$src get-rsvp-api] != "" } {
				set rate [$tg_ set rate_]
				$ns at-now "[$src get-rsvp-api] sender $fid_ $rate [expr $rate*0.2] $rate 100 1500"
			}
			$ns after [lindex $args 0] "$tg_ start"
		}
		3 {
			if { [$src get-rsvp-api] != "" } {
				set rate [$tg_ set rate_]
				$ns at-now "[$src get-rsvp-api] sender $fid_ $rate [expr $rate*0.2] $rate 100 1500"
				[$src get-rsvp-api] add-upcall-once RESV [$dst node-addr] $fid_ 42 "$tg_ start"
			}
		}
		4 {
			$srcAgent_ set prio_ [lindex $args 0]
			$tg_ start
		}
		default {
			puts "error: unsupported flow start mode $mode!"
			exit 1
		}
	}
	set started_ true

#	puts "[format "%02.4f" [[Simulator instance] now]]|[$src node-addr]: flow $self started"
}

# stops traffic generation for a flow and tears the path
Flow instproc stop {} {
	$self instvar started_ tg_ srcAgent_ dstAgent_ fid_

	$tg_ stop
	set started_ false

	if { [[$srcAgent_ set node_] get-rsvp-api] != "" } {
		# send PATH_TEAR
		set ns [Simulator instance]
		$ns at-now "[[$srcAgent_ set node_] get-rsvp-api] session [[$dstAgent_ set node_] node-addr] $fid_ 42"
		$ns at-now "[[$srcAgent_ set node_] get-rsvp-api] close"
	}

#			puts "[format "%02.4f" [[Simulator instance] now]]|[[$srcAgent_ set node_] node-addr]: flow $self stopped ($fid_)"
}

# makes sure the api informs the flow receiver application of RSVP events
Application/FlowReceiver instproc register-with-api { api node fid } {
	$self instvar api_ node_ fid_
	set api_ $api
	set node_ $node
	set fid_ $fid

	if { $api != "" } {
		$api add-upcall ALL $node $fid 42 "$self api-upcall \$msg \$rate"
	}
}

# handles RSVP events
Application/FlowReceiver instproc api-upcall { msg rate } {
	$self instvar api_ node_ fid_

	switch $msg {
		PATH {
			$api_ session $node_ $fid_ 42
			$api_ reserve ff cl -1 0 $rate [expr $rate*2] $rate 100 1500
		}
		RESV_CONFIRM {
			$self upcall-resv_conf
		}
		PATH_TEAR {
			$self upcall-path_tear
		}
	}
}

Class FlowGenerator

# connects a source and a sink
FlowGenerator instproc connect { src dst } {
	$self instvar src_ dst_
	set src_ $src
	set dst_ $dst
}

# syntax: initFlows <first flow id> <numFlows> { UDP CBR <packetSize> <rate> <randomJitter?> }
#                                            | { UDP Trace <tracefile> }
#                                            | { UDP Pareto <packetSize> <rate> <burstTime> <idleTime> <shape> }
#                                            | { TCP Greedy }
#
# sets up the traffic characteristic of the generated flows
# TODO: start array at 1 => need static variable to maintain fidstart. howto?
FlowGenerator instproc initFlows { fidstart flowcount flowtype tgtype args } {
	$self instvar rng_ src_ dst_ flows_ flows_total_ flows_created_
	if ![info exists src_] {
		puts "error: need to connect source and sink first!"
		exit 1
	}

	set api [$dst_ get-rsvp-api]
	if { $api == "" } {
		puts "initFlows: cannot find RSVP API at node [$dst_ id]"
	} else {
		$api set offered_sessions_ 0 
		$api set accepted_sessions_ 0
	}

	set flows_total_ $flowcount
	set flows_created_ 0
	for {set i 0} {$i < $flowcount} {incr i} {
		set f [new Flow]
		$f set fid_ [expr $fidstart + $i]
		$f connect $src_ $dst_ $flowtype
		eval $f traffic $tgtype $args
		if [info exists rng_] { $f set rng_ $rng $rng_ }
		set flows_($i) $f
		if { $api != "" } {
			$api set session_active_([expr $i + $fidstart]) false
		}
	}
}

# syntax: start <avg. inter arrival time> {fixed|exp|uni|vary} <avg. flow duration> {fixed|exp|uni|vary} <flow start mode> [<args>]
#
# starts the generation of flows
FlowGenerator instproc start { iat_avg iat_mode fd_avg fd_mode startmode args } {
	$self instvar rng_ lastflow_ flows_total_ flows_created_
	$self instvar iat_src_ fd_src_ flowstartmode_ flowstartargs_
	$self instvar min_duration_ max_duration_

	set flows_created_ 0
	set flowstartmode_ $startmode
	set flowstartargs_ $args
	set min_duration_ 0
	set max_duration_ 0

	# create random variables
	switch $iat_mode {
		exp {
			set iat_src_ [new RandomVariable/Exponential]
			$iat_src_ set avg_ $iat_avg
		}
		fixed {
			set iat_src_ [new RandomVariable/Constant]
			$iat_src_ set val_ $iat_avg
		}
		vary {
			set iat_src_ [new RandomVariable/Uniform]
			$iat_src_ set min_ [expr 0.9 * $iat_avg]
			$iat_src_ set max_ [expr 1.1 * $iat_avg]
		}
		uni {
			set iat_src_ [new RandomVariable/Uniform]
			$iat_src_ set min_ [expr 0.0 * $iat_avg]
			$iat_src_ set max_ [expr 2.0 * $iat_avg]
		}
		default {
			puts; puts "ERROR: unknown inter-arrival mode $iat_mode"
		}
	}
	switch $fd_mode {
		exp {
			set fd_src_ [new RandomVariable/Exponential]
			$fd_src_ set avg_ $fd_avg
			set min_duration_ [expr 0.1 * $fd_avg]
			set max_duration_ [expr 4.0 * $fd_avg]
		}
		fixed {
			set fd_src_ [new RandomVariable/Constant]
			$fd_src_ set val_ $fd_avg
		}
		vary {
			set fd_src_ [new RandomVariable/Uniform]
			$fd_src_ set min_ [expr 0.8 * $fd_avg]
			$fd_src_ set max_ [expr 1.2 * $fd_avg]
		}
		uni {
			set fd_src_ [new RandomVariable/Uniform]
			$fd_src_ set min_ 0.0
			$fd_src_ set max_ [expr 2 * $fd_avg]
		}
		default {
			puts; puts "ERROR: unknown duration mode $fd_mode"
		}
	}

	if [info exists rng_] {
		$iat_src_ set rng_ $rng_
		$fd_src_ set rng_ $rng_
	}

	# start first flow
	set lastflow_ -1
	$self start_next_flow
}

# starts a flow and re-schedules itself
FlowGenerator instproc start_next_flow { { method 1 } } {
	$self instvar flows_ flows_total_ flows_created_ lastflow_ src_
	$self instvar iat_src_ fd_src_ flowstartmode_ flowstartargs_
	$self instvar min_duration_ max_duration_

	set createflow_ -1

	switch $method {
		1 {
			# creates <flows_total_> flows, starting each flow exactly once;
			# flow generation stops automatically after all flows have been started
			if { $flows_created_ >= $flows_total_ } {
				$self stop
				return
			}
			incr lastflow_
			set createflow_ $lastflow_
		}
		2 {
			# creates up to <flows_total_> flows, restarting flows which stopped before;
			# flow generation continues until 'stop' is invoked on flow generator
			# search for a flow which is not started
			set i $lastflow_
			incr i
			if { $i >= $flows_total_ } { set i 0 }
			while { $i != $lastflow_ } {
				if { [$flows_($i) set started_] == "false" } {
					set lastflow_ $i
					set createflow_ $lastflow_
					break
				}
				incr i
				if { $i >= $flows_total_ } { set i 0 }
			}
		}
	}

	if { $createflow_ != -1 } {
		eval $flows_($createflow_) start $flowstartmode_ $flowstartargs_
		set duration [$fd_src_ value]
		if { $min_duration_ != 0 && $duration < $min_duration_ } {
			set duration $min_duration_
		} elseif { $max_duration_ != 0 && $duration > $max_duration_ } {
			set duration $max_duration_
		}
		[Simulator instance] after $duration "$flows_($createflow_) stop"
		incr flows_created_
	}

	# schedule next flow start
	set nextflowstart [ expr [[Simulator instance] now] + [$iat_src_ value] ]
	[Simulator instance] at $nextflowstart "$self start_next_flow $method"

#	puts -nonewline "[format "%02.4f" [[Simulator instance] now]]|[$src_ node-addr]: "
#	if { $duration > 0 } {
#		puts "$flows_created_ of $flows_total_ flows created (fid: [$flows_($createflow_) set fid_], duration $duration, next at $nextflowstart)"
#	} else {
#		puts "$flows_created_ of $flows_total_ flows created (maximum reached)"
#	}
}

# syntax: stop [immediately]
#
# stops the creation of new flows; by default existing flows continue for the remainder of
# their lifetime (duration); if the option 'immediately' is given, flows are cancelled.
FlowGenerator instproc stop { args } {
	$self instvar flows_ flows_total_
	if {[lindex $args 0] == "immediately"} {
		for {set i 0} {$i < $flows_total_} {incr i} { $flows_($i) stop }
	}
}
