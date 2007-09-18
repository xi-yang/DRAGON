#!/usr/bin/perl

$os_name=`uname`;
chop($os_name);
if ($os_name ne "Linux") {
    print "Warning: Linux Software Switch cannot run on a non-Linux system!\n"
}
open(dev_file, '/proc/net/dev') || die "The impossible happened: cannot open /proc/net/dev!";
my $index=1;
while (<dev_file>) {
    if (/(^[ a-zA-Z0-9.]+):([ 0-9]+)/) {
	print "interface $1 --> port #$index (0/0/$index)\n";
	$index++;
    }
}

