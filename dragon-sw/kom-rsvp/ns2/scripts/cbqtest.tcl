#- this will be called when an api-agent receives an upcall -----------------------------------
Agent/RSVPApi instproc user-upcall { msg args } {
	if { $msg == "PATH" } {
		$self reserve ff cl -1 0 400kb 800kb 400kb 100 1500
#		puts "          reserve ff cl -1 0 400kb 800kb 400kb 100 1500"
	} else {
#		puts "          upcall: $msg"
	}
}

#- start of simulation script -----------------------------------------------------------------

# remove unneeded headers   
remove-all-packet-headers   
add-packet-header IP Message

# include helpers
source [file dirname $argv0]/helpers.tcl

set ns [new Simulator]
$ns enable-rsvp

# open a trace file for nam
set nf [open cbqtest.nam w]
$ns namtrace-all $nf

set flowid 66

# could change defaults here (defaults in ns-default.tcl)
# Agent/RSVP set class_ 1000
# Agent/RSVP set defaultRefresh_ 3s
Agent/RSVP logfile all ref,packet,select,threads rsvp.log
Agent/RSVPApi set debug_ false

# set packet color in nam
$ns color 0  Blue
$ns color $flowid  Red
$ns color [Agent/RSVP set class_] green

puts "*** creating topology"
# create topology
#
#     5                   6       X                   X
#      \                 /         9                 11
#       \               /           \               /
#        \             /             10            12
#         1~~~~~2-----3               X4~~~3X6---5X
#        /             \             1             8
#       /               \           /               \
#      /                 \         2                 7
#     0                   4       X                   X
#                               (internal intf numbering)
#   -: duplex-link w/DropTail
#   ~: duplex-link w/CBQ
#
set links      { {0 1} {1 2 CBQ DropTail} {2 3} {3 4} {1 5} {3 6} }
set orient     {  NE    E                  E     SE    NW    NE   }
set bandwidths {  1Mb   1Mb                1Mb   1Mb   1Mb   1Mb  }
set delays     {  10ms  10ms               10ms  10ms  10ms  10ms }
set rsvpnodes  { 0 1 2 3 4 }
set apinodes   { 0 4 }
createTopology

# display queue in nam
$ns duplex-link-op $n(1) $n(2) queuePos 0.5

# create background traffic n(5)->n(6) (uses 800kbps)
set src0 [new Agent/UDP]
set dst0 [new Agent/Null]
$n(5) attach $src0
$n(6) attach $dst0
$ns connect $src0 $dst0
set cbr0 [new Application/Traffic/CBR]
$cbr0 set random_ 1
$cbr0 set packetSize_ 500
$cbr0 set rate_ 800kb
$cbr0 attach-agent $src0
$ns at 0.1 "$cbr0 start"

# create flow for reservation scenario n(0)->n(4)
set src1 [new Agent/UDP]
set dst1 [new Agent/Null]
$n(0) attach $src1
$n(4) attach $dst1
$ns connect $src1 $dst1
set cbr1 [new Application/Traffic/CBR]
$cbr1 set random_ 1
$cbr1 set packetSize_ 500
$cbr1 set rate_ 800kb
$cbr1 attach-agent $src1
$ns at 0.2 "$cbr1 start"
$src1 set class_ $flowid

# a little reservation scenario
$ns at 0.05 "[$n(4) get-rsvp-api] session [$n(4) node-addr] $flowid 17"
$ns at 0.05 "[$n(0) get-rsvp-api] session [$n(4) node-addr] $flowid 17"
$ns at 0.5 "[$n(0) get-rsvp-api] sender $flowid 800kb 1600kb 800kb 100 1500"
$ns at 6.0 "[$n(0) get-rsvp-api] close"

# flow generator sample code
# see ns-rsvp.tcl for details
#set fg0 [new FlowGenerator]
#$fg0 connect $n(5) $n(6)
#$fg0 initFlows 1 400 UDP CBR 242 [expr 40*242*8] 0
#$ns at 0.1 "$fg0 start 0.5 fixed 50 fixed 3"

# set simulation end
proc finish {} {
	global ns nf
	$ns flush-trace
	close $nf
puts "*** DONE"
	exit 0
}

showProgress 8.0
$ns at 8.0 "finish"

# run simulation
puts "*** starting simulation"
$ns run
