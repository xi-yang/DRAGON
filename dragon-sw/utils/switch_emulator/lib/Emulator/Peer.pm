package Emulator::Peer;

use Emulator qw( );
use vars qw( @ISA @REQUIRED );
use strict;
use warnings;

@REQUIRED   = qw( );
@ISA	    = 'Emulator';

sub new {
    my $class = shift;
    my $self = $class->SUPER::new( @_ );
    
    $self->socket( $self->{Socket} ) if defined $self->{Socket};
    return $self;
}

sub socket {
    my ( $self, $sock ) = @_;
    if ( defined $sock ) {
	$self->{Socket} = $sock;
	$self->server_ip( $sock->sockhost );
	$self->ip( $sock );  # Seed IP address.
    }
    return $self->{Socket};
}

sub server_ip {
    my ( $self, $addr ) = @_;
    $self->{ServerAddr} = $addr if defined $addr;
    return $self->{ServerAddr};
}

sub ip {
    my ( $self, $sock ) = @_;

    if ( $sock or not defined $self->{IP} ) {
	if ( $sock ||= $self->socket ) {
	    $self->{IP} = $sock->peerhost;
	}
    }

    return $self->{IP};
}

sub id {
    my $self = shift;
    return $self->ip;
}

1;
