package de.tud.kom.rsvp;
import java.net.*;

public class Test implements UpcallListener {
	InetAddress addr;
	JRSVP jrsvp;
	SessionID session;
	public static void main(String[] args) throws Exception {
		Test test = new Test();
		if ( args.length < 1 ) {
			System.err.println("usage: Test dest-host [r]");
			System.exit(1);
		}
		test.addr = InetAddress.getByName(args[0]);
		test.jrsvp = new JRSVP();
		test.session = new SessionID();
		String recv = new String( "r" );
		if ( args.length > 1 && args[1].startsWith(recv) ) {
			test.receive();
		} else {
			test.send();
		}
		System.out.println("sleeping 1 second");
		Thread.sleep( 1000 );
		if ( args.length > 1 && args[1].startsWith(recv) ) {
			test.receive();
		} else {
			test.send();
		}
		System.exit(0);
	}
	public void send() throws Exception {
		jrsvp.createSession( session, addr, 17, 4711, this );
		TSpec tspec = new TSpec();
		tspec.r = 1000;
		tspec.p = 2000;
		tspec.b = 1000;
		tspec.m = 100;
		tspec.M = 1500;
		jrsvp.createSender( session, 4711, tspec, 127 );
		System.out.println("sleeping 1 second");
		Thread.sleep( 1000 );
		jrsvp.unregisterUpcallListener( this );
		jrsvp.releaseSession( session );
		System.out.println("test done");
	}
	public void receive() throws Exception {
		jrsvp.createSession( session, addr, 17, 4711, this );
		System.out.println("sleeping 10 seconds");
		Thread.sleep( 10000 );
		jrsvp.unregisterUpcallListener( this );
		jrsvp.releaseSession( session );
		System.out.println("test done");
	}
	public void handlePATHMessages( PATHMessage p ) {
		System.out.println("Test: PATH message received");
		TSpec tspec = new TSpec();
		tspec.r = 1000;
		tspec.p = 2000;
		tspec.b = 1000;
		tspec.m = 100;
		tspec.M = 1500;
		Flowspec f = new Flowspec();
		f.tspec = tspec;
		jrsvp.createReservation( session, f );
	}
	public void handleRESVMessages( RESVMessage p ) {
		System.out.println("Test: RESV message received");
	}
}
