package Emulator;

use constant VERSION	=> "0.1";

use FindBin;
use Exporter;
use Carp;
use vars qw( @ISA @EXPORT_OK *FILE );
use strict;
use warnings;

@ISA	    = "Exporter";
@EXPORT_OK  = qw( );

my %Defaults = (
    ### Server server networking values.
    ServerMode	    => "CoreDirectorTL1",
    ServerPort	    => 10201,
    PollInterval    => 0.25,
    ListenQueue	    => 10,
    HandleTimeout   => 3,
    ForkOff	    => 1,

    ### Default log level.
    Verbosity	    => 5,
    LogFacility     => "internal",
    SyslogSocket    => "unix",
    SyslogOptions   => "pid,cons,nowait",
    SyslogPriority  => "info",
    SyslogFacility  => "user",
    SyslogIdent     => "Emulator",

    ### Stamp the version (for use in templates)
    Version	    => VERSION
);

BEGIN {
    $ENV{PATH} = $FindBin::Bin . ":" . $ENV{PATH};
}

$SIG{__WARN__} = sub { Emulator->log( 0, @_ ) };

sub new {
    my $class = shift;
    my @default = %Defaults;
    my %args = @_;

    # A couple of ways to inherit parental values...
    push @default, %$class if ref $class;    
    push @default, %{$args{Parent}} if ref $args{Parent};

    my $self = bless { @default, %args }, ref( $class ) || $class;

    # Attempt to find emulator.conf if ConfigFile is provided but undefined.
    # (i.e. the EMULATOR environment variable was never set.)
    #
    if ( exists $self->{ConfigFile} ) {
	$self->{ConfigFile} ||= "$FindBin::Bin/../emulator.conf";
	$self->read_config( delete $self->{ConfigFile} );
    }

    $self->check_config;
    $self;
}

sub file {
    my ( $self, $filename ) = @_;

    $filename = $self->{$filename}
	if $self->{$filename};

    open( FILE, "<$filename" )
	or return $self->log( 1, "file $filename: $!" );

    if ( wantarray ) {
	return <FILE>;
    } else {
	local $/ = undef; 
	return <FILE>;
    }
}

sub parse {
    my ( $self, @text ) = @_;
    my @pairs;

    for my $arg ( @text ) {
	for my $line ( split /(?:\r?\n)+/, $arg ) {
	    # Strip leading & trailing whitespace.
	    $line =~ s/^\s+|\s+$//gos;

	    # If it doesn't start with an alphanumeric, it's a comment.
	    next unless $line =~ /^\w/o;

	    # Split key / value pairs.
	    my ($key, $val) = split( /\s+/, $line, 2 );
	    push @pairs, $key, $val;
	}
    }

    return @pairs;
}

sub deparse {
    my ( $self, @vars ) = @_;
    my $text = "";

    $text .= join("\t", splice( @vars, 0, 2 )) . "\n" while @vars;
    return $text;
}

sub read_config {
    my ( $self, $filename ) = @_;

    croak "No config file specified! Does \$EMULATOR point to your emulator.conf?\n"
	unless $filename;

    my $file	= $self->file( $filename ) 
	or croak "Can't read config file $filename: $!";

    my %args	= $self->parse( $file );

    $self->{$_} = $args{$_} for (keys %args);
    return $self;
}

sub check_config {
    my ( $self, @required ) = @_;
    my $class = ref( $self ) || $self;

    unless ( @required ) {
	# Try to get the @Emulator::Foo::REQUIRED list.
	no strict 'refs';
	my $req = "$class\::REQUIRED";
	@required = @$req if @$req;
    }

    # warn "CHECK $self (@required)\n";
    
    return not @required unless @required;

    my @missing = grep { not defined $self->{$_} }  @required;

    $self->log( 1, "check_config: Missing $_ directive required for $class object!" )
	for @missing;

    return not @missing;
}

sub log {
     my ( $self, $level, @msg ) = @_;

     # Bag if this message is too verbose.
     #
     if ( not ref $self or $level <= $self->{Verbosity} ) {
         if(ref $self and $self->{LogFacility} eq "syslog") {
             $self->syslog_log(@msg);
         } else {
             $self->internal_log(@msg);
         }
     }
}

sub syslog_log {
    require Sys::Syslog;

    import Sys::Syslog qw(:DEFAULT setlogsock);

    my ( $self, @msg ) = @_;

    setlogsock($self->{SyslogSocket});
    openlog($self->{SyslogIdent}, $self->{SyslogOptions}, $self->{SyslogFacility});
    syslog($self->{SyslogPriority}, "%s", "@msg");
    closelog();
}

sub internal_log {
    my ( $self, @msg ) = @_;

    # Get relevant time/date data.
    my ( $s, $m, $h, $d, $mo, $yr ) = (localtime())[0..5];
    $yr += 1900; $mo++; chomp @msg;

    # Log message takes form: [YYYY-MM-DD HH-MM-SS] *Your message here*
    print STDERR (sprintf( "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                           $yr, $mo, $d, $h, $m, $s, "@msg" ));
    return;
}

sub format {
    my ( $self, $string, $extra ) = @_;

    # Merge parameters from %$extra, if any.
    my %args = $extra ? ( %$self, %$extra ) : %$self;

    # Throughout $string, replace strings of form $var or ${var} with value of $args{var}.
    $string =~ s/\$\{?(\w+)\}?/ defined( $args{$1} ) ? $args{$1} : "" /egios;

    return $string;
}

sub template {
    my ( $self, $filename, $extra ) = @_;
    my $file = $self->file( $filename );
    return $self->format( $file, $extra ); 
}

sub instantiate {
    my $self	= shift;
    my $class 	= shift;
    my ( $super, $config );

    if ( $super = ref $self ) {
	# $self is an object, which presumably already has the config data.
	$config = $self;	
    } else {
	# Gotta instantiate a bootstrap object to load up the config data.
	$config = __PACKAGE__->new( @_ );
	$super  = $self;
    }

    $class = "$super\::$config->{$class}";

    croak "Source class $class contains invalid characters"
	if $class =~ y/A-Za-z0-9_://cd;

    eval "require $class" or
        croak "Can't load class '$class': $@";

    return $class->new( Parent => $self, @_ );
}

sub server {
    my $self	  = shift;
    require Emulator::Server;
    return Emulator::Server->new( Parent => $self, @_ );
}

sub peer {
    my $self = shift;
    unshift @_, "Socket" if @_ == 1;
    require Emulator::Peer;
    return Emulator::Peer->new( Parent => $self, @_ );
}

1;
