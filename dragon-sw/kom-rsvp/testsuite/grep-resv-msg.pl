#!/usr/local/bin/perl

$state=0;
while(<STDIN>)	{
	if($_=~/(sends)|(received)/)	
	{ 
		print $_;
		if($_=~/RCONF|RESV|RTEAR/)	{
			$state=1;
		}
		else	{ $state=0 }
	}
	elsif ($_=~/^[0-9]{2}:[0-9]{2}:[0-9]{2}/)	{
		$state=0;
	}
	elsif ($state==1)	{
		print $_;
	}
}
