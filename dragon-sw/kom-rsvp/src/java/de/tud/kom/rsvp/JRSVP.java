package de.tud.kom.rsvp;
import java.net.*;
import java.util.*;

public class JRSVP extends Thread {
final static int NONE = 0;
final static int FF = 10;
final static int WF = 17;
final static int SE = 18;

	private int api;
	private Thread locker;
	private java.util.Vector listeners = new java.util.Vector();
	static {
//		System.out.println( System.getProperties().get("java.library.path") );
		System.loadLibrary("JRSVP");
		System.out.println("JRSVP library loaded");
	}
	private native int createAPI( int port, String rsvpdHost );
	private native void destroyAPI( int api );
	private native void runAPI( int api );
	private native int createSessionInternal( int api, String addr, int protocol, int port );
	private native void releaseSessionInternal( int api, int session );
	private native void createSenderInternal( int api, int session, String addr, int port, TSpec tspec, int TTL );
	private native void createReservationInternal( int api, int session, Flowspec flowspec, boolean confirmReservation, int filterStyle );

	public JRSVP( int port, String rsvpdHost ) {
		try {
			api = createAPI( port,InetAddress.getByName(rsvpdHost).getHostAddress() );
		} catch (UnknownHostException ex) {
			ex.printStackTrace();
		}
		locker = null;
		start();
	}

	public JRSVP( int port ) {
		this ( port, "localhost");
	}

	public JRSVP() {
		this ( 4000, "localhost");
	}

	public void finalize() {
		destroyAPI( api );
	}

	public void run() {
		runAPI( api );
	}

	public synchronized void preUpcall() {
		System.out.println("preUpcall called");
		locker = Thread.currentThread();
	}

	public synchronized void postUpcall() {
		System.out.println("postUpcall called");
		locker = null;
		notifyAll();
	}

	public synchronized void upcallPathEvent() {
		System.out.println("PATH message received");
		Enumeration e = listeners.elements();
		while (e.hasMoreElements()) {
			((UpcallListener)e.nextElement()).handlePATHMessages(null);
		}
	}

	public synchronized void upcallResvEvent() {
		System.out.println("RESV message received");
		Enumeration e = listeners.elements();
		while (e.hasMoreElements()) {
			((UpcallListener)e.nextElement()).handleRESVMessages(null);
		}
	}

	public synchronized void createSession( SessionID s, InetAddress addr, int protocol, int port, UpcallListener l ) {
		try {
			while ( locker != null && locker != Thread.currentThread() )
				wait();
		} catch (InterruptedException i) {}
		if ( l != null ) listeners.add(l);
		s.id = createSessionInternal( api, addr.getHostAddress(), protocol, port );
	}

	public void createSession( SessionID s, InetAddress addr, int protocol, int port ) {
		createSession( s, addr, protocol, port, null );
	}

	public synchronized void createSender( SessionID s, int port, TSpec tspec, int TTL ) {
		try {
			while ( locker != null && locker != Thread.currentThread() )
				wait();
		} catch (InterruptedException i) {}
		createSenderInternal( api, s.id, "0.0.0.0", port, tspec, TTL );
	}

	public synchronized void createSender( SessionID s, InetAddress addr, int port, TSpec tspec, int TTL ) {
		try {
			while ( locker != null && locker != Thread.currentThread() )
				wait();
		} catch (InterruptedException i) {}
		createSenderInternal( api, s.id, addr.getHostAddress(), port, tspec, TTL );
	}

	public synchronized void createReservation( SessionID s, Flowspec flowspec, boolean confirmReservation, int filterStyle ) {
		try {
			while ( locker != null && locker != Thread.currentThread() )
				wait();
		} catch (InterruptedException i) {}
		createReservationInternal( api, s.id, flowspec, confirmReservation, filterStyle );
	}

	public synchronized void createReservation( SessionID s, Flowspec flowspec ) {
		createReservation(s, flowspec, true, FF);
	}

	public synchronized void releaseSession( SessionID s ) {
		try {
			while ( locker != null && locker != Thread.currentThread() )
				wait();
		} catch (InterruptedException i) {}
		releaseSessionInternal( api, s.id );
	}

	public void unregisterUpcallListener(UpcallListener l) {
		listeners.remove(l);
	}
}
