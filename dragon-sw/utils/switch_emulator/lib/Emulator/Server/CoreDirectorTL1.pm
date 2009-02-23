#
# $Id$
#
package Emulator::Server::CoreDirectorTL1;

use Emulator qw( );
use Emulator::Server;
use IO::Select;
use Switch;
use vars qw(@ISA @REQUIRED %COMMANDS $IAC $WILL $WONT $DO $DONT $EC $EL $GA $SB $TN_NAWS
	    $TN_TRANSMIT_BINARY $TN_ECHO $TN_SUPPRESS_GA $TN_STATUS $TN_247 $TN_248
	    $TN_TIMING_MARK $TN_LINEMODE %TN_local_options %TN_remote_options %TN_options_desc
	    );
use strict;
use warnings;

@ISA        = 'Emulator::Server';
@REQUIRED   = (
    @Emulator::Server::REQUIRED, 
    qw( )
	       );

%COMMANDS = (
	     'act-user'       => \&act_user,
	     'canc-user'      => \&canc_user,
	     'inh-msg-all'    => \&inh_msg_all,
	     'dlt-crs-stspc'  => \&dlt_crs_stspc,
	     'dlt-dtl'        => \&dlt_dtl,
	     'dlt-dtl-set'    => \&dlt_dtl_set,
	     'dlt-eflow'      => \&dlt_eflow,
	     'dlt-gtp'        => \&dlt_gtp,
	     'dlt-snc-stspc'  => \&dlt_snc_stspc,
	     'dlt-vcg'        => \&dlt_vcg,
	     'ed-crs-stspc'   => \&ed_crs_stspc,
	     'ed-snc-stspc'   => \&ed_snc_stspc,
	     'ed-vcg'         => \&ed_vcg,
	     'ent-crs-stspc'  => \&ent_crs_stspc,
	     'ent-dtl'        => \&ent_dtl,
	     'ent-dtl-set'    => \&ent_dtl_set,
	     'ent-eflow'      => \&ent_eflow,
	     'ent-gtp'        => \&ent_gtp,
	     'ent-snc-stspc'  => \&ent_snc_stspc,
	     'ent-vcg'        => \&ent_vcg,
	     'rtrv-crs-stspc' => \&rtrv_crs_stspc,
	     'rtrv-eflow'     => \&rtrv_eflow,
	     'rtrv-gtp'       => \&rtrv_gtp,
	     'rtrv-ocn'       => \&rtrv_ocn,
	     'rtrv-snc-stspc' => \&rtrv_snc_stspc,
	     'rtrv-snc-diag'  => \&rtrv_snc_diag,
	     'rtrv-vcg'       => \&rtrv_vcg,
	     );

# TELNET protocol RFCs 854 - 861, 1184 (LINEMODE), 1073 (NAWS)

$IAC  = "\xFF";  # dec 255 - interpret as command
$EC   = "\xF7";  # dec 247 - erase character
$EL   = "\xF8";  # dec 248 - erase line
$GA   = "\xF9";  # dec 249 - go ahead signal
$SB   = "\xFA";  # dec 250 - subnegotiation of indicated option
$WILL = "\xFB";
$WONT = "\xFC";
$DO   = "\xFD";
$DONT = "\xFE";

# Telnet options
$TN_TRANSMIT_BINARY = "\x00";
$TN_ECHO            = "\x01";
$TN_SUPPRESS_GA     = "\x03";
$TN_STATUS          = "\x05";
$TN_TIMING_MARK     = "\x06";
$TN_LINEMODE        = "\x22"; # dec 34
$TN_NAWS            = "\x1F"; # dec 31
$TN_247             = "\xF7";
$TN_248             = "\xF8";

%TN_local_options = ();

%TN_remote_options = ();

%TN_options_desc = (
		   "\x00" => "transmit binary",
		   "\x01" => "echo",
		   "\x03" => "suppress go ahead (ga)",
		   "\x05" => "status",
		   "\x06" => "timing mark",
		   "\x22" => "linemode",
		   "\x1F" => "negotiate about window size (naws)",
		   "\xF7" => "telnet option 247 Ciena-specific??",
		   "\xF8" => "telnet option 248 Ciena-specific??",
		   );

sub handle {
    my ( $self, $peer ) = @_;

    my $socket = $peer->socket;

    $self->initialTelnetOptions( $peer );
    $self->initialPrompt( $peer );

    my $char = '';
    my $line = '';
    while ( defined( my $n = $peer->socket->sysread($char, 1)) ) {
	last if $n == 0; # client disconnected
	if ($char ne $IAC) {
            if ($char eq ';') {
		$self->log( 10, "handle: got semi-colon character" );
                $peer->socket->print($char);
                # TL1 end-of-line is a semi-colon, process the line
                $peer->socket->print("\r\n");
                $self->processCommand( $peer, $line );
                $peer->socket->print(";");
                $line = '';
            }
            if ($char eq "\x0D") {  # control-M (carriage return, dec 13)
		$self->log( 10, "handle: got carriage-return on empty line" );
		$peer->socket->print("\r\n;") if $line eq '';
            }
            if ($char =~ /^[a-zA-Z0-9\-_!&,=:]$/) {
		# $self->log( 10, "handle: character='$char'" );
                $peer->socket->print($char);
                # ordinary character (not IAC character or TL1 EOL)
                $line .= $char;
            }
	} else {
	    # IAC received, get command sequence
	    $self->log( 10, "handle: got IAC character" );
	    $peer->socket->sysread($char, 1);
	    switch($char) {
		case "$WILL" { 
		    $peer->socket->sysread($char, 1);
		    $self->willopt($peer, $char);
		}
		case "$WONT" { 
		    $peer->socket->sysread($char, 1);
		    $self->wontopt($peer, $char);
		}
		case "$DO" { 
		    $peer->socket->sysread($char, 1);
		    $self->doopt($peer, $char);
		}
		case "$DONT" { 
		    $peer->socket->sysread($char, 1);
		    $self->dontopt($peer, $char);
		}
		case "$IAC" {  # escaped IAC
		    $line .= $char;
		}
	    }
	}
    }
    $self->log( 10, "handle: client disconnected, closing socket" );
    $peer->socket->close;
}

sub processCommand {
    my ( $self, $peer, $line ) = @_;

    # strip any high-ASCII characters coming in, also strip CR and LF (for testing)
    # shouldn't really be necessary but makes for easy testing
    $line =~ s/[\r\n\x80-\xFF]//g;
    
    $self->log( 5, "handle: client sent: '$line'" );
    
    if ($line =~ /^([a-zA-Z-]+):/) {
	my $cmd = lc($1);
	if (defined($COMMANDS{$cmd})) {
	    $self->log( 5, "handle: running command '$cmd' ($COMMANDS{$cmd})" );
	    $COMMANDS{$cmd}->( $self, $peer, $line );
	} else {
	    $self->log( 5, "handle: unsupported command '$cmd'" );
	    $self->invalidCommand( $peer );
	}
    } else {
	$self->log( 5, "handle: unrecognized line '$line'" );
	$self->invalidCommand( $peer );
    }
}

sub initialTelnetOptions {
    my ( $self, $peer ) = @_;
    $peer->socket->print($IAC . $DONT . $TN_ECHO);
    $peer->socket->print($IAC . $DONT . $TN_LINEMODE);
    $peer->socket->print($IAC . $DO   . $TN_SUPPRESS_GA);
    # $peer->socket->print($IAC . $DO   . $TN_247);  # Ciena-specific telnet option?
    # $peer->socket->print($IAC . $DO   . $TN_248);  # Ciena-specific telnet option?
    # $peer->socket->print($IAC . $DO   . $TN_NAWS); # not really necessary..
    $peer->socket->print($IAC . $WILL . $TN_SUPPRESS_GA);
    $peer->socket->print($IAC . $WONT . $TN_LINEMODE);
    $peer->socket->print($IAC . $WILL . $TN_ECHO);
    # $peer->socket->print($IAC . $WONT . $TN_247);  # Ciena-specific telnet option?
    # $peer->socket->print($IAC . $WONT . $TN_248);  # Ciena-specific telnet option?
}

sub initialPrompt {
    my ( $self, $peer ) = @_;
    $peer->socket->print("\r\nTXN TL1 Agent Copyright Sienna Korporashun\r\n\r\n\r\n;");
}

sub act_user {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "act-user: running function" );

    # ACT-USER:[TID]:uid:CTAG::pid
    # example: act-user::dragon:123::dragon
    if ($line =~ /act-user:([^:]*):([^:]+):([^:]+)::([^:]+)/i) {
	my $tid = $1;
	my $uid = $2;
        my $ctag = $3;
	my $pid = $4;
	$self->log( 10, "act-user: tid=$tid, uid=$uid, ctag=$ctag, pid=$pid" );
	my $msg = $self->commonResponse($ctag);
	$msg .= <<END;
   "$uid:LASTLOGIN=01/01/2001 01-01-01,INTRUSIONS=0,"
;

   NODE 01-01-01 01:01:01
A  100000 REPT EVT SESSION
   "NODE:NO,"
   /* Local Authentication Successful.
   !!! This is a private network. Any unauthorized access or use will lead to prosecution!!! */
END
        $peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub canc_user {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "canc_user: running function" );

    # CANC-USER:[TID]:userid:CTAG;
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub inh_msg_all {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "inh_msg_all: running function" );

    # INH-MSG-ALL:[TID]::CTAG::[NTFCNCDE=ntfcncde],CONDTYPE=condtype],[TMPER=tmper]
    # example: inh-msg-all:::123
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub dlt_crs_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "dlt_crs_stspc: running function" );

    # DLT-CRS-STSPC:[TID]:[NAME=crsName],[FROMENDPOINT=fromTermination],[TOENDPOINT=toTermination]:CTAG
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub dlt_dtl {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "dlt_dtl: running function" );

    # DLT-DTL:[TID]:dtlName:CTAG
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub dlt_dtl_set {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "dlt_dtl_set: running function" );

    # DLT-DTL-SET:[TID]:setName:CTAG
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub dlt_eflow {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "dlt_eflow: running function" );

    # DLT-EFLOW:[TID]:eflowName:CTAG
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub dlt_gtp {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "dlt_gtp: running function" );

    # DLT-GTP:[TID]:gtpName:CTAG; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub dlt_snc_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "dlt_snc_stspc: running function" );

    # DLT-SNC-STSPC:[TID]:sncName:CTAG
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub dlt_vcg {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "dlt_vcg: running function" );

    # DLT-VCG:[TID]:[NAME=name]:CTAG
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ed_crs_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ed_crs_stspc: running function" );

    # ED-CRS-STSPC:[TID]:[NAME=crsName],[FROMENDPOINT=fromTermination], 
    #  [TOENDPOINT=toTermination]:CTAG::[ALIAS=alias], 
    #  [PST=primaryState],[SST=secondaryState]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ed_snc_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ed_snc_stspc: running function" );

    # ED-SNC-STSPC:[TID]:sncName:CTAG::[ALIAS=alias],[DTLSN=dtlSetName], 
    #  [DTLEXCL=dtlSetExclusivity],[REGROOM=regroomAllowed], 
    #  [DTLRMVSN=dtlRemoveDtlSet],[MESHRST=meshRestorable], 
    #  [PRTT=protectionType],[BCKOP=backOffPeriod],[RVRTT=revertTimeType], 
    #  [TRVRT=timeToRevert],[PST=primaryState],[RTEOPR=routeOperation], 
    #  [SNCINTEGRITYCHECKADMINST=sncintegritycheckadminst], 
    #  [MAXADMINWEIGHT=<maxadminweight>]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ed_vcg {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ed_vcg: running function" );

    # ED-VCG:[TID]:NAME=name:CTAG::[ALIAS=alias], 
    #  [PST=primary state],[SST=secondary state],[CRCTYPE=CRCtype], 
    #  [DEGRADETHRESHOLD=DegradeThreshold],[FRAMINGMODE=FrameMode], 
    #  [TUNNELPEERTYPE=TunnelPeerType],[TUNNELPEERNAME=TunnelPeerName], 
    #  [MEMBERFAILCRITERIA=memberfailcriteria], 
    #  [GFPFCSENABLED=GfpFcsEnabled], 
    #  [DEFAULTJ1ENABLED=DefaultJ1PathTraceEnabled], 
    #  [LCASENABLED=LcasEnabled], 
    #  [MONITORINGCHANNEL=MonitoringChannel], 
    #  [LCASHOLDOFFTIMER=LcasHoldOffTimer], 
    #  [LCASRSACKTIMER=LcasRsAckTimer], 
    #  [SCRAMBLINGBITENABLED=scramblingbitenabled], 
    #  [GROUPMEM=GroupMembers]>],[SUSPENDMEM=SuspendMembers], 
    #  [EFFIBASESEV=EffiBaseSev],[VCGFAILUREBASESEV=VcgFailureBaseSev]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ent_crs_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ent_crs_stspc: running function" );

    # ENT-CRS-STSPC:[TID]:FROMENDPOINT=fromTermination, 
    #  TOENDPOINT=toTermination:CTAG::[NAME=crsName], 
    #  [FROMTYPE=fromTerminationType], 
    #  [TOTYPE=toTerminationType],[ALIAS=alias]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ent_dtl {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ent_dtl: running function" );

    # ENT-DTL:[TID]:dtlName:CTAG::NODENAME1=nodeName1, 
    #  OSRPLTPID1=osrpLtpId1,[NODENAME2=nodeName2], 
    #  [OSRPLTPID2=osrpLtpId2],[NODENAME3=nodeName3], 
    #  ...
    #  [OSRPLTPID19=osrpLtpId19],[NODENAME20=nodeName20], 
    #  [OSRPLTPID20=osrpLtpId20],TERMNODENAME=termNodeName; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ent_dtl_set {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ent_dtl_set: running function" );

    # ENT-DTL-SET:[TID]:setName:CTAG::WRKNM=workingDtlName, 
    #  [PRTNMLST=protectionDtlNameList]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ent_eflow {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ent_eflow: running function" );

    # ENT-EFLOW:[TID]:eflowName:CTAG:::INGRESSPORTTYPE=ingressPortType, 
    #  INGRESSPORTNAME=ingressPortName,[PKTTYPE=packetType], 
    #  [OUTERVLANIDRANGE=outerVlanIdRange], 
    #  [SECONDVLANIDRANGE=secondVlanIdRange],[PRIORITY=userPriority], 
    #  [EGRESSPORTTYPE=egressPortType,EGRESSPORTNAME=egressPortName, 
    #  [COSMAPPING=classOfServiceMapping],[ENABLEPOLICING=enablePolicing], 
    #  [BWPROFILE=bandwidthProfile],[TAGSTOREMOVE=tagsToRemove], 
    #  [TAGSTOADD=tagsToAdd],[OUTERTAGTYPE=outerTagType], 
    #  [OUTERVLANID=outerVlanId],[SECONDTAGTYPE=secondTagType], 
    #  [SECONDVLANID=secondVlanId],[INHERITPRIORITY=inheritUserPriority], 
    #  [NEWPRIORITY=newUserPriority],[COLLECTPM=collectPm]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ent_gtp {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ent_gtp: running function" );

    # ENT-GTP:[TID]:gtpName:CTAG::[LBL=Label],[OWN=Owner],[CTP=CtpList]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ent_snc_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ent_snc_stspc: running function" );

    # ENT-SNC-STSPC:[TID]:fromTermination,toTermination:CTAG:: 
    #  NAME=sncName,TYPE=sncType,RMNODE=remoteEndNodeName, 
    #  LEP=localEndPointType,[ALIAS=alias],[DTLSN=dtlSetName], 
    #  [DTLEXCL=dtlSetExclusivity],[CONNDIR=connectDirection], 
    #  [SDHSE=sdhSetExclusivity],[MESHRST=meshRestorable], 
    #  PRTT=protectionType,[BCKOP=backOffPeriod],[RVRTT=revertTimeType], 
    #  [TRVRT=timeToRevert],[BLKLL=blockedLinkListName], 
    #  [EXLL=exclusiveLinkListName],[PST=primaryState],[PEERSNC=peersnc], 
    #  [PEERORIGIN=peerorigin],[SNCLINETYPE=snclinetype], 
    #  [ORIGINDSPS=origindsps],[ORIGINNSPS=originnsps], 
    #  [TERMINDSPS=termindsps],[TERMINNSPS=terminnsps], 
    #  [SNCINTEGRITYCHECKADMINST=sncintegritycheckadminst], 
    #  [REMOTEPATHPROTECTION=remotepathprotection], 
    #  [MAXADMINWEIGHT=maxadminweight]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub ent_vcg {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "ent_vcg: running function" );

    # ENT-VCG:[TID]:[NAME=name]:[CTAG]::[ALIAS=alias],[PST=primaryState], 
    #  [SST=secondaryState],[SUPPTTP=suppTtp],[CRCTYPE=CRCtype], 
    #  [DEGRADETHRESHOLD=DegradeThreshold], 
    #  [SCRAMBLINGBITENABLED=ScramblingBitEnabled],[FRAMINGMODE=FrameMode], 
    #  [TUNNELPEERTYPE=TunnelPeerType],[TUNNELPEERNAME=TunnelPeerName], 
    #  [MEMBERFAILCRITERIA=MemberFailCriteria], 
    #  [GFPFCSENABLED=GfpFcsEnabled],[DEFAULTJ1ENABLED=DefaultJ1Enabled], 
    #  [LCASENABLED=LcasEnabled],[GROUPMEM=GroupMembers], 
    #  [EFFIBASESEV=EffiBaseSev],[VCGFAILUREBASESEV=VcgFailureBaseSev]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub rtrv_crs_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "rtrv_crs_stspc: running function" );

    # RTRV-CRS-STSPC:[TID]:[NAME=crsName], 
    #  [FROMENDPOINT=fromTermination], 
    #  [TOENDPOINT=toTermination],[AID=aid]:CTAG::[fetchAttrs], 
    #  [fetchFilter]:[stateFilter]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub rtrv_eflow {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "rtrv_eflow: running function" );

    # RTRV-EFLOW:[TID]:[eflowName]:CTAG; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub rtrv_gtp {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "rtrv_gtp: running function" );

    # RTRV-GTP:[TID]:gtpName:CTAG; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub rtrv_ocn {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "rtrv_ocn: running function" );

    # RTRV-OCN:[TID]:ControlAid:CTAG::[FetchAttribs],[fetchState]:[stateFilter]; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
    my $msg = <<END;
   "$tl1[2],ITYPE=SONET,RCVTRC=,TRC=,EXPTRC=,FMTSTRC=64_BYTE,ETRC=NO,DCC=NO,UCC=NO,LDCC=YES,XDCC=NO,ISCCMD=Unprotected,,,,PST=IS-NR,SST=ALM,FERFNC=YES,AISIA=YES,LOFIA=NO,LOSIA=NO,SDIA=NO,SFIA=NO,TIMSIA=YES,,,,,,,,,,,,,LOSDT=100,SDT=7,SDTC=8,SFT=4,SFTC=5,TSTMD=NONE,RATE=OC192,LMTYPE=UNKNOWN,FSSM=NO,TSSM=ST3E,RSSM=STU,TIMESLOTMAP=,NOACTP=0,UTILIZATION=0/192,PTYPE=None,LTYPE=Unprotected,PPPADMIN=LOCKED,PPPLINESTATE=PPP_NEGOTIATING,PPPPROTSTATE=NOT_INSERVICE,HDLCXSUM=16_BIT,CONFIGRATE=OC192,TRSCT=NO,SDCCTRNSP=NO,LDCCTRNSP=NO,DROPSIDE=YES,KBYTERESIL=NO,OSIENABLE=NO,,OSILAPDMODE=NETWORK,,OSILAPDPST=NULL,OSILAPDSST=NULL,OSRPLAPD=YES,MAXAPBSTS=,LWPCENABLED=YES,,TXCIRCID=,TXCIRCDESC=,RXCIRCID=,RXCIRCDESC=,SIGNALST=Normal,AU4ONLY=NO,LDCCRATE=576,ALS=NO,CONCATENATION=Virtual 50MBPS,MAPPING=AU3,MAXFRAMESIZE=9600 Bytes,PAUSEOFFWM=4,PAUSEONWM=12,,,OSPFHELLO=,OSPFDEAD=,OSPFMTU=,OSPFCOST="
END
    $peer->socket->print($msg);
}

sub rtrv_snc_diag {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "rtrv_snc_diag: running function" );

    # RTRV-SNC-DIAG:[TID]:sncName:CTAG; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
}

sub rtrv_snc_stspc {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "rtrv_snc_stspc: running function" );

    # RTRV-SNC-STSPC:[TID]:sncName:CTAG; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
    $peer->socket->print("   /* Empty list returned for entered parameter. */\r\n");
}

sub rtrv_vcg {
    my ( $self, $peer, $line ) = @_;
    $self->log( 10, "rtrv_vcg: running function" );

    # RTRV-VCG:[TID]:src:CTAG; 
    my @tl1 = split(/:/, $line);
    my $ctag = $tl1[3];
    if (defined($ctag) and $ctag =~ /\d+/) {
	my $msg = $self->commonResponse($ctag);
	$peer->socket->print($msg);
    } else {
	$self->invalidCommand( $peer );
    }
    $peer->socket->print("   /* Empty list returned for entered parameter. */\r\n");
}

sub invalidCommand {
    my ( $self, $peer ) = @_;
    $self->log( 10, "invalidCommand: running function" );
    $peer->socket->print("\r\n");
    $peer->socket->print("   NODE 01-01-01 01:01:01\r\n");
    $peer->socket->print("M  1 DENY\r\n");
    $peer->socket->print("   IICM\r\n");
    $peer->socket->print("   /* Input, Invalid Command */\r\n");
}

sub commonResponse {
    my ( $self, $ctag ) = @_;
    my $msg = <<END;
IP $ctag
<

   NODE 01-01-01 01:01:01
M  $ctag COMPLD
END
    return $msg;
}

sub willopt {
    my ( $self, $peer, $opt ) = @_;

    if (defined($TN_options_desc{$opt})) {
	$self->log( 10, "willopt: recv: will $TN_options_desc{$opt}" );
    } else {
	$self->log( 10, "willopt: recv: will $opt" );
    }

    if (defined($TN_remote_options{$opt}) and $TN_remote_options{$opt} == 1) {
	return;  # Already set, ignore to prevent loop
    }
    my $ack = $WONT;
    switch ($opt) {
	case "$TN_TRANSMIT_BINARY" { $TN_remote_options{$opt} = 1; $ack = $WILL; }
	case "$TN_ECHO"            { $TN_remote_options{$opt} = 1; $ack = $WILL; }
	case "$TN_SUPPRESS_GA"     { return; }
	case "$TN_LINEMODE"        { $TN_remote_options{$opt} = 1; $ack = $WILL; }
	case "$TN_NAWS"            { return; }
    }
    $self->answer($peer,$ack,$opt);
}

sub wontopt {
    my ( $self, $peer, $opt ) = @_;

    if (defined($TN_options_desc{$opt})) {
	$self->log( 10, "wontopt: recv: wont $TN_options_desc{$opt}" );
    } else {
	$self->log( 10, "wontopt: recv: wont $opt" );
    }

    if (defined($TN_remote_options{$opt}) and $TN_remote_options{$opt} == 0) {
	return;  # Already clear, ignore to prevent loop
    }
    $TN_remote_options{$opt} = 0; 
    $self->answer($peer,$WONT, $opt);   # Must always accept
}

sub doopt {
    my ( $self, $peer, $opt ) = @_;

    if (defined($TN_options_desc{$opt})) {
	$self->log( 10, "doopt: recv: do $TN_options_desc{$opt}" );
    } else {
	$self->log( 10, "doopt: recv: do $opt" );
    }

    if (defined($TN_local_options{$opt}) and $TN_local_options{$opt} == 1) {
	return;  # Already set, ignore to prevent loop
    }
    my $ack = $WONT;
    switch($opt) {
	case "$TN_SUPPRESS_GA" { return; } # Ciena returning nothing when it receives this
	case "$TN_ECHO"        { return; } # Ciena returning nothing when it receives this
    }
    $self->answer($peer,$ack, $opt);
}

sub dontopt {
    my ( $self, $peer, $opt ) = @_;

    if (defined($TN_options_desc{$opt})) {
	$self->log( 10, "dontopt: recv: dont $TN_options_desc{$opt}" );
    } else {
	$self->log( 10, "dontopt: recv: dont $opt" );
    }
    
    if (defined($TN_local_options{$opt}) and $TN_local_options{$opt} == 0) {
	return;  # Already clear, ignore to prevent loop
    }
    $TN_local_options{$opt} = 0; 
    switch($opt) {
	case "$TN_247" { return; }
	case "$TN_248" { return; }
    }
    $self->answer($peer,$WONT, $opt);
}

sub answer {
    my ( $self, $peer, $r1, $r2 ) = @_;

    switch ($r1) {
	case "$WILL" { $self->log( 10, "answer: sent: will $TN_options_desc{$r2}" ); }
	case "$WONT" { $self->log( 10, "answer: sent: wont $TN_options_desc{$r2}" ); }
	case "$DO"   { $self->log( 10, "answer: sent: do $TN_options_desc{$r2}"   ); }
	case "$DONT" { $self->log( 10, "answer: sent: dont $TN_options_desc{$r2}" ); }
    }
    $peer->socket->print($IAC);
    $peer->socket->print($r1);
    $peer->socket->print($r2);
}

1;


=pod
NOTES:

* negotiation of TELNET options
  from perspective of telnet client, logging into coredirector:

% telnet 
telnet> toggle options
Will show option processing.
telnet> open 1.1.1.1 10201
Trying 1.1.1.1...
...
RCVD DONT ECHO
RCVD DONT LINEMODE
RCVD DO SUPPRESS GO AHEAD
SENT WILL SUPPRESS GO AHEAD
RCVD DO 247
SENT WONT 247
RCVD DO 248
SENT WONT 248
RCVD DO NAWS
SENT WILL NAWS
SENT IAC SB NAWS 0 141 (141) 0 40 (40)
RCVD WILL SUPPRESS GO AHEAD
SENT DO SUPPRESS GO AHEAD
RCVD WONT LINEMODE
RCVD WILL ECHO
SENT DO ECHO
RCVD WONT 247
RCVD WONT 248
RCVD WILL 247
SENT DONT 247
RCVD WILL 248
SENT DONT 248

...however, this seems to be sufficient:

RCVD DONT ECHO
RCVD DONT LINEMODE
RCVD DO SUPPRESS GO AHEAD
SENT WILL SUPPRESS GO AHEAD
RCVD WILL SUPPRESS GO AHEAD
SENT DO SUPPRESS GO AHEAD
RCVD WONT LINEMODE
RCVD WILL ECHO
SENT DO ECHO

=cut
