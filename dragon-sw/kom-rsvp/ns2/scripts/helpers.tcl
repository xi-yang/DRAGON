proc add_link { ld o bw d ts } {
	global links orient bandwidths delays timestamps
	lappend links $ld
	lappend orient $o
	lappend bandwidths $bw
	lappend delays $d
	lappend timestamps $ts
}

proc createTopology {} {
	global ns n links orient bandwidths delays timestamps rsvpnodes apinodes

	set i 0
	while { $i < [llength $links] } {
		set linkdata [lindex $links $i]
		set a [lindex $linkdata 0]
		set b [lindex $linkdata 1]
		if ![info exists n($a)] { set n($a) [$ns node] }
		if ![info exists n($b)] { set n($b) [$ns node] }

		if { [lindex $linkdata 2] == "" } { set queueType "DropTail"  } else { set queueType  [lindex $linkdata 2] }
		if { [lindex $linkdata 3] == "" } { set queueType2 $queueType } else { set queueType2 [lindex $linkdata 3] }

		set bandwidth 10Mb
		if { [lindex $bandwidths $i] != "" } {
			set bandwidth [lindex $bandwidths $i]
		}
		set delay 10ms
		if { [lindex $delays $i] != "" } {
			set delay [lindex $delays $i]
		}
		if { $queueType == $queueType2 } {
			$ns duplex-link $n($a) $n($b) $bandwidth $delay $queueType
		} else {
			$ns simplex-link $n($a) $n($b) $bandwidth $delay $queueType
			$ns simplex-link $n($b) $n($a) $bandwidth $delay $queueType2
		}

		set ts [lindex $timestamps $i]
		set queue [[$ns link $n($a) $n($b)] queue]
		$queue set limit_ 20
		if { [lindex $ts 0] == "yes" } { $queue set timestamp_ true }
		set queue [[$ns link $n($b) $n($a)] queue]
		$queue set limit_ 20
		if { [lindex $ts 1] == "yes" } { $queue set timestamp_ true }

		if { [lindex $orient $i] != "" } {
			switch [lindex $orient $i] {
				NW { set o "left-up" }
				N  { set o "up" }
				NE { set o "right-up" }
				E  { set o "right" }
				SE { set o "right-down" }
				S  { set o "down" }
				SW { set o "left-down" }
				W  { set o "left" }
				default { set o "right" }
			}
			$ns duplex-link-op $n($a) $n($b) orient $o
		}
		incr i
	}
	foreach r $rsvpnodes {
		$n($r) add-rsvp-daemon
	}
	foreach a $apinodes {
		$n($a) add-rsvp-api
	}
}

proc showProgress { sim_length } {
	global ns

	set percent 0
	while { $percent < 100 } {
		$ns at [expr $sim_length * $percent / 100] "puts -nonewline \"\r$percent% done\";flush stdout"
		incr percent
	}
	$ns at $sim_length "puts \"\r100% done\""
}

proc cleanTemp { tempfile_prefix } {
	set extensions "tr"
	foreach ext $extensions {
		exec rm -f $tempfile_prefix.*.$ext
	}
}
