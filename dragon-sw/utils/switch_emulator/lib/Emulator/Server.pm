#
# $Id$
#
package Emulator::Server;

use IO::Socket;
use IO::Select;
use IO::Pipe;
use Emulator qw( );
use vars qw( @ISA @REQUIRED @EXPORT_OK *FILE );
use strict;
use warnings;

@ISA	    = 'Emulator';
@EXPORT_OK  = @Emulator::EXPORT_OK;
@REQUIRED   = qw( ServerMode ServerPort ListenQueue PollInterval );

sub new {
    my $self	= shift;
    my $class	= ref( $self ) || $self;

    # We've been called as Emulator::Server->new, which means we need to
    # load emulator.conf and figure out which server plugin we're 
    # supposed to use.
    #
    return $self->instantiate( ServerMode => @_ )
	if $class eq __PACKAGE__;

    # We've been called as Emulator::Source::Foo->new, so pass arguments
    # to Emulator->new and get the new object.
    #
    $self = $class->SUPER::new( @_ );

    $self->{Peer} ||= {};
    return $self;
}

sub bind_socket {
    my $self = shift;
    my @address;

    return $self->{ListenSocket} if $self->{ListenSocket};

    # If no IP address is specified, try them all.
    if ( $self->{ServerAddr} ) {
	@address = ( LocalAddr => $self->{ServerAddr} );
    } else {
	@address = ( MultiHomed => 1 );
    }

    # Use a specified port if there is one.
    push @address, ( LocalPort => $self->{ServerPort} ) 
	if $self->{ServerPort};

    my $server = IO::Socket::INET->new(
	Listen	    => $self->{ListenQueue},
        Type        => SOCK_STREAM,
	Proto	    => "tcp",
	Reuse	    => 1,
        Blocking    => 1,
	@address
    );

    $self->log( 0, "bind_socket: Can't bind to port $self->{ServerPort}: $!.",
	"(Is another server already running?)" )
	unless $server;

    $self->log( 8, "bind_socket: Binding listener socket to ", $server->sockhost );

    return( $self->{ListenSocket} = $server );
}

sub pool {
    my $self = shift;
    $self->{SocketPool} ||= IO::Select->new( $self->bind_socket );
    return $self->{SocketPool};
}

sub clear_pool {
    my $self = shift;
    delete $self->{SocketPool};
}

sub run {
    my $self	= shift;
    my $kids	= 0;
    
    return unless $self->bind_socket;

    local $SIG{PIPE} = "IGNORE"; 
    local $SIG{CHLD} = sub { $kids++ };

    # Reset history.
    $self->{ServerStartTime}	= scalar localtime;
    $self->{LastConnectionTime}	= "none";
    $self->{TotalConnections}	= 0;

    # Handle connections as they come in.
    #
    while ( 1 ) {
	# Spend some time waiting for something to happen.
	# If poll_socket doesn't return true, we're a child who's done.
	#
	$self->poll_socket or return;

	# See if any kids have expired, reap zombies
	if ( $kids ) {
	    1 until ( wait == -1 );
	    $kids = 0;
	}

    } # loop forever
}

sub poll_socket {
    my $self	= shift;
    my $server	= $self->bind_socket;
    my @ready   = $self->pool->can_read( $self->{PollInterval} );

    if (@ready and not defined $ready[0]) {
	$self->log( 1, "poll_socket: $!" );
	return 1;
    }

    for my $listen (@ready) {
	$self->log( 10, "Ready in poll_socket: @ready" );

	# If the inet socket is ready for reading, spawn a child.
	my $is_parent;

	if ( $listen eq $server ) {
	    # Get the client socket to pass to the child.
	    my $client = $server->accept;

	    # Don't spawn a child process if ForkOff is false.
	    $is_parent = $self->spawn_child if $self->{ForkOff};

	    unless ($is_parent) {
		# We're the child (or we didn't fork), so process
		# the client's request.
		$self->accept_client( $client );
		
		# Exit iff we actually succeeded in forking.
		return 0 if defined $is_parent; 
	    }
	} else {
	    # Otherwise, this is a child reporting back via a pipe.
	    $self->accept_child( $listen );
	}
    }

    return 1;
}

sub parent {
    my ($self, $pipe) = @_;
    $self->{ParentPipe} = $pipe if $pipe;
    return $self->{ParentPipe};
}

sub notify_parent {
    my ($self, $action, $peer) = @_;
    if (my $parent = $self->parent) {
	my %args = %$peer; 

	# Don't pass any references back to the parent, they
	# wouldn't know what to do with it anyway.
	my @refs = grep( ref($args{$_}), keys %args );
	delete @args{@refs};

	# Notify the parent any special action we're taking about this peer.
	$args{Action} = $action if defined $action;	

	$self->log( 10, "notify_parent: Notifying parent of $action on peer", $peer->id );

	# Reformat the peer's basic info and send it to the parent process.
	print $parent $self->deparse( %args )
	    or $self->log( 1, "notify_parent: Can't notify parent of $action: $!" );
    }
}

sub spawn_child {
    my $self = shift;
    my $pipe = IO::Pipe->new;
    my $pid;

    if ($pid = fork) {
	# We're the parent. Poll for writes from the kid.
	$self->log( 10, "spawn_child: Spawning child process $pid." );
	$self->pool->add( $pipe->reader );

    } elsif (defined $pid) {
	# We're the kid. Get ready to write back to the parent.
	$self->parent( $pipe->writer );

	# Close any open listener sockets.
	$self->clear_pool;

    } else {
	$self->log( 1, "spawn_child: failure - $!" );
    }
    
    return $pid;
}

sub accept_child {
    my ($self, $listen) = @_;
    my $r = read( $listen, my $msg, 500_000 ); # arbitrary limit
    if ($r) {

    } elsif (not defined $r) {
	$self->log( 2, "accept_child: Can't read from child pipe: $!" );
    }

    # if $r returned false, but not undef, then the child quit 
    # normally, but with nothing to say?

    $self->pool->remove( $listen );

    my $result = $listen->close;
    $self->log( 10, "accept_child: Child process returned $result" ) if $r;
}

sub accept_client {
    my ($self, $sock)	= @_;
    my $peer	    = $self->peer( $sock );
    my $peerhost    = $sock->peerhost;    

    $self->log( 8, "accept_client: Connection to " . $sock->sockhost . " from $peerhost" );

    # ctt - we are not using HandleTimeout because we are
    # emulating a CLI-like environment, we might need some kind
    # of idle timeout though if a command is not sent after 300sec tho
    #
    # Set the UNIX alarm clock.
    # alarm( $self->{HandleTimeout} ) if $self->{HandleTimeout};
    # Wrap the call to handle() in eval{}, so we catch the
    # exception when the alarm goes off.
    #eval { 
    #	$self->handle( $peer );
    #	alarm 0 if $self->{HandleTimeout};
    #};

    $self->handle( $peer );

    # Note the warning if the call to handle() threw an exception.
    $self->log( 1, "accept_client: $peerhost: $@" ) if $@;
}

sub handle {
    die "Emulator::Server cannot handle connections on its own.";
}

1;
