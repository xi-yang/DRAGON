#- code for basic RSVP support --------------------------------------------------------------------

# hack to get ns-2 to install interface labeling on links
# (does not activate multicasting in nodes)
Simulator instproc enable-rsvp {} {
	$self set multiSim_ 1
}

# adds an RSVP daemon agent to a node
Node instproc add-rsvp-daemon { { port "" } } {
	$self instvar rsvpdaemonagent_

	if [info exists rsvpdaemonagent_] {
		puts "$self add-rsvp-daemon: RSVP daemon agent already installed!"
		return $rsvpdaemonagent_
	}

	set rsvpdaemonagent_ [new Agent/RSVP]
	$self attach $rsvpdaemonagent_ $port
	$self install-rsvp-tab

#			puts "$self add-rsvp-daemon: $rsvpdaemonagent_ installed on port [$rsvpdaemonagent_ port]."
	return $rsvpdaemonagent_
}

# returns a node's RSVP-Daemon agent (or nothing if there is none)
Node instproc get-rsvp-daemon {} {
	$self instvar rsvpdaemonagent_

	if [info exists rsvpdaemonagent_] {
		return $rsvpdaemonagent_
	} else {
		return ""
	}
}


# adds an RSVP API-agent to a node; if necessary, installs a daemon agent as well.
# (Until now api and daemon have to reside on same host, because LIFMapping is
# only done for daemons. Might change this later, but we need to set a flag, so
# that LIFs are not registered twice. Might bind a variable and set flag here...)
Node instproc add-rsvp-api { { port "" } } {
	$self instvar rsvpdaemonagent_ rsvpapiagent_

	if ![info exists rsvpdaemonagent_] {
		$self add-rsvp-daemon
	}

	set rsvpapiagent_ [new Agent/RSVPApi]
	$self attach $rsvpapiagent_ $port

	[Simulator instance] connect $rsvpapiagent_ $rsvpdaemonagent_

#			puts "$self add-rsvp-api: $rsvpapiagent_ installed on port [$rsvpapiagent_ port]."
	return $rsvpapiagent_
}

# returns a node's RSVP API-agent (or nothing if there is none)
Node instproc get-rsvp-api {} {
	$self instvar rsvpapiagent_

	if [info exists rsvpapiagent_] {
		return $rsvpapiagent_
	} else {
		return ""
	}
}

# installs a tab at the node's entry point, which passes PATH, PATH_TEAR, and
# RESV_CONF messages directly to the rsvp-daemon
# (private: only called by 'add-rsvp-daemon')
Node instproc install-rsvp-tab {} {
	$self instvar rsvpagent_ rsvptab_ inLink_ outLink_ classifier_ switch_ multiclassifier_

	set rsvpAgent [$self get-rsvp-daemon]
	set oldEntry  [$self entry]

	# Install the RSVP-tab
	set rsvptab_ [new Classifier/RSVPTab]
	$rsvptab_ install 0 $oldEntry
	$rsvptab_ install 1 $rsvpAgent
	set classifier_ $rsvptab_

	# During link creation the link's target is set. If it is already connected to the node when
	# this method is called, we need to update the target, or the tab will be bypassed.
	foreach linkIdx [array names inLink_] {
		set iif [$inLink_($linkIdx) set iif_]
		if { [$iif target] == $oldEntry } [
			$iif target [$self entry]
		]
	}

	return $rsvptab_
}

# returns the outgoing interface (oif) towards node <dstAddr>
Agent/RSVP instproc get-unicast-route dstAddr {
	set ns [Simulator instance]

	set this_node [$self set node_]
	set dest_node [$ns get-node-by-addr $dstAddr]
	if { $this_node == $dest_node } {
		return "api"
	}

	set next_hop_id [ [$ns get-routelogic] lookup [$this_node id] [$dest_node id] ]
	set out_link [ $ns link $this_node $next_hop_id ]
	if { $out_link == "" } {
		return ""
	} else {
		return [$this_node link2oif $out_link]
	}
}

# a helper to find the highest address assigned to a node
# (works with both flat and hierarchical addressing)
Simulator instproc get-highest-assigned-addr {} {
	$self instvar Node_

	if { [Simulator hier-addr?] } {
		set highestaddr -1
		foreach nn [array names Node_] {
			set addr [ AddrParams addr2id [$Node_($nn) node-addr] ]
			if { $addr > $highestaddr } {
				set highestaddr $addr
			}
		}
	} else {
		set highestaddr [expr [Node set nn_] - 1]
	}

	return $highestaddr
}


#- code for api upcalls ---------------------------------------------------------------------------

# adds the upcall <upcallproc>, which is invoked once when this api-agent instance
# receives the message <msg> for session <dst> <fid> <proto> and is then being deleted.
Agent/RSVPApi instproc add-upcall-once { msg dst fid proto upcallproc } {
	$self instvar upcallsonce_
	set upcallsonce_($msg:$dst:$fid:$proto) $upcallproc

#			set node [$self set node_]
#			puts "[format "%02.4f" [[Simulator instance] now]]|[$node node-addr]: $self added \[$upcallproc\] to upcallsonce_($msg:$dst:$fid:$proto)"
}

# adds the upcall <upcallproc>, which is invoked every time this api-agent instance
# receives the message <msg> for session <dst> <fid> <proto>.
Agent/RSVPApi instproc add-upcall { msg dst fid proto upcallproc } {
	$self instvar upcalls_
	set upcalls_($msg:$dst:$fid:$proto) $upcallproc

#			set node [$self set node_]
#			puts "[format "%02.4f" [[Simulator instance] now]]|[$node node-addr]: $self added \[$upcallproc\] to upcalls_($msg:$dst:$fid:$proto)"
}

# handles all one-time and permanent upcalls specific for this api-agent instance;
# calls user_upcall for any general handling of rsvp messages
# (private: called by wrapper when the api-agent receives an rsvp-message)
Agent/RSVPApi instproc upcall { msg args } {

	set node [[$self set node_] node-addr]
	if {[Agent/RSVPApi set debug_]} {
		puts "[format "%02.4f" [[Simulator instance] now]]|$node: *** UPCALL: $msg ***"
		puts "          $args"
	}

	# extract the flows's advertised rate
	if [regexp {r:([0-9.e\+]+)} $args dummy rate] {
		set rate [ expr 8 * $rate ]
	} else {
		set rate 0
	}

	# extract the session info
	if ![ regexp {\(node ([0-9]+)\)/([0-9]+)\(([0-9]+)\)} $args dummy dst fid proto ] {
		puts "error extracting session info!"
		exit 1
	}

	$self instvar upcalls_ upcallsonce_

	# perform any one-time upcall for this session
	if ![info exists upcallsonce_] {
		if {[Agent/RSVPApi set debug_]} {
			puts "          no one-time upcalls"
		}
	} else {
		set idx "$msg:$dst:$fid:$proto"
		if [info exists upcallsonce_($idx)] {
			if {[Agent/RSVPApi set debug_]} {
				puts "          calling \"$upcallsonce_($idx)\" once"
			}
			eval $upcallsonce_($idx)
			unset upcallsonce_($idx)
		}
	}

	# perform any permanent upcall for this session
	if ![info exists upcalls_] {
		if {[Agent/RSVPApi set debug_]} {
			puts "          no permanent upcalls"
		}
	} else {
		set idx "$msg:$dst:$fid:$proto"
		if [info exists upcalls_($idx)] {
			if {[Agent/RSVPApi set debug_]} {
				puts "          calling \"$upcalls_($idx)\""
			}
			eval $upcalls_($idx)
		}

		set idx "ALL:$dst:$fid:$proto"
		if [info exists upcalls_($idx)] {
			if {[Agent/RSVPApi set debug_]} {
				puts "          calling \"$upcalls_($idx)\""
			}
			eval $upcalls_($idx)
		}
	}

	# handle user upcalls
	if {[Agent/RSVPApi info instprocs user-upcall] != ""} {
		$self user-upcall $msg $args
	}
}
