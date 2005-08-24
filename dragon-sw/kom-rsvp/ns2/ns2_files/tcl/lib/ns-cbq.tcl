#- code for CBQ support ---------------------------------------------------------------------------

# adds a new class to the class-tree
CBQLink instproc cbq-create-class { parent okborrow allot maxidle prio level xdelay { queue "" } } {
	set newclass [new CBQClass]

	# install a queue for this class; only for leaf-classes!
	if { $queue != "" } {
		$queue set limit_ 10
		$newclass install-queue $queue
	}

	$newclass setparams $parent $okborrow $allot $maxidle $prio $level $xdelay
	$self insert $newclass

	return $newclass
}

# modifies a class's allotment
CBQLink instproc cbq-modify-class { cbqclass newallot newmaxidle } {
	$self instvar link_ queue_
	$cbqclass instvar maxidle_

	$cbqclass allot $newallot
	if { $newmaxidle == "auto" } {
		$cbqclass automaxidle [$link_ set bandwidth_] [$queue_ set maxpkt_]
		set maxidle_ [$cbqclass set maxidle_]
	} else {
		set maxidle_ $newmaxidle
	}
	$cbqclass maxidle $maxidle_

	return $class
}

# sets class for packets w/o reservation
CBQLink instproc cbq-set-defaultclass { defaultclass } {
	$self instvar classifier_
	$classifier_ install 0 $defaultclass
	$classifier_ set default_ 0
}

# sets class for rsvp control traffic
CBQLink instproc cbq-set-ctlclass { ctlclass } {
	$self instvar classifier_
	$classifier_ install [Agent/RSVP set class_] $ctlclass
	$classifier_ set-hash auto 0 0 [Agent/RSVP set class_] [Agent/RSVP set class_]
}

# adds a filter for a flow
CBQLink instproc cbq-add-filter { cbqclass srcid destid flowid } {
	$self instvar classifier_

	# clear old hash-entries
	if { [$classifier_ lookup auto 0 0 $flowid] != "" } {
		$classifier_ del-hash 0 0 $flowid
	}

	# find the slot where <cbqclass> is installed, or create new one
	set slot [$classifier_ findslot $cbqclass]
	if { $slot == -1 } {
		set slot [$classifier_ installNext $cbqclass]
	}

	$classifier_ set-hash auto 0 0 $flowid $slot
}

# deletes a filter for a flow
CBQLink instproc cbq-del-filter { srcid destid flowid } {
	$self instvar classifier_

	if { [$classifier_ lookup auto 0 0 $flowid] != "" } {
		$classifier_ del-hash 0 0 $flowid
	}
}
