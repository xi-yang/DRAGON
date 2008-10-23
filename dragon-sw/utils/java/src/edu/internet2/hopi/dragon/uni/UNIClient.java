package edu.internet2.hopi.dragon.uni;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.HashMap;

import org.savarese.rocksaw.net.RawSocket;

import edu.internet2.hopi.dragon.DragonLSP;
import edu.internet2.hopi.dragon.DragonLocalID;
import edu.internet2.hopi.dragon.uni.rsvp.RSVPPath;
import edu.internet2.hopi.dragon.uni.rsvp.header.*;
import edu.internet2.hopi.dragon.uni.rsvp.header.tlv.IPv4TLV;
import edu.internet2.hopi.dragon.util.ByteUtil;

public class UNIClient {
	private RawSocket sock;
	private InetAddress localAddress;
	private InetAddress remoteAddress;
	private ReceiveThread recv;
	private HashMap<String,UNISession> sessions;
	private boolean isRecvStarted;
	
	public UNIClient(String localAddress, String remoteAddress) throws UnknownHostException{
		this.sock = new RawSocket();
		this.initialize(localAddress, remoteAddress);
	}
	
	protected UNIClient(String localAddress, String remoteAddress, RawSocket sock) throws UnknownHostException{
		this.sock = sock;
		this.initialize(localAddress, remoteAddress);
	}
	
	private void initialize(String localAddress, String remoteAddress) throws UnknownHostException{
		this.localAddress = InetAddress.getByName(localAddress);
		this.remoteAddress = InetAddress.getByName(remoteAddress);
		this.recv = new ReceiveThread();
		this.recv.setDaemon(true);
		this.sessions = new HashMap<String,UNISession>();
		this.isRecvStarted = false;
	}
	
	public String startNewSession(RSVPPath pathMsg) throws IllegalStateException, IOException{
		String sessionName = pathMsg.getSessionAttribute().getSessionName();
		UNISession session = new UNISession(this.sock, remoteAddress.getHostAddress(), pathMsg);
		
		/* Open socket */
		if(!this.sock.isOpen()){
			this.sock.open(RawSocket.PF_INET, RawSocket.getProtocolByName("rsvp"));
		}
		
		/* Start receive thread */
		if(!this.isRecvStarted){
			this.recv.start();
			this.isRecvStarted = true;
		}
		
		/* Schedule path message to send and refresh */
		session.start();
		this.sessions.put(sessionName, session);
		
		return sessionName;
	}
	
	public String startNewSession(DragonLSP lsp) throws IllegalStateException, IOException{
		String sessionName = lsp.getLSPName();
		RSVPPath pathMsg = new RSVPPath();
		
		/* Create Session object */
		InetAddress extendedTunnelId = lsp.getSrcIP();
		//Reverse source IP to generate session extended tunnel ID
		extendedTunnelId = InetAddress.getByAddress(ByteUtil.reverse(extendedTunnelId.getAddress()));
		RSVPLSPSession session = new RSVPLSPSession(lsp.getDstIP(), 
													lsp.getSrcLocalID().getNumber(), 
													extendedTunnelId);
		/* Create Hop object */
		ArrayList<IPv4TLV> tlvs = new ArrayList<IPv4TLV>();
		tlvs.add(new IPv4TLV(InetAddress.getByName("0.0.0.0")));	//data-te address (all 0s since UNI)
		RSVPHop hop = new RSVPHop(this.localAddress, 4, tlvs);
		
		RSVPTimeValues timeValues = new RSVPTimeValues(30000);		//default 30 seconds
		RSVPGeneralizedLabelRequest labelRequest = new RSVPGeneralizedLabelRequest(
														lsp.getEncodingAsGMPLS(), 
														lsp.getSWCAPAsGMPLS(), 
														lsp.getGPIDAsGMPLS());
		RSVPDragonUNI dragonUNI = new RSVPDragonUNI(lsp.getSrcIP(),
													lsp.getSrcLocalID().getTypeAsGMPLS(), 
													lsp.getSrcLocalID().getNumber(), 
													lsp.getDstIP(), 
													lsp.getDstLocalID().getTypeAsGMPLS(), 
													lsp.getDstLocalID().getNumber(), 
													lsp.getE2EVtag(), 
													"implicit", "implicit");
		
		RSVPSessionAttribute sessionAttribute = new RSVPSessionAttribute(7, 7, 0, sessionName); //default 7 priorities
		RSVPSenderTemplate senderTemplate = new RSVPSenderTemplate(lsp.getSrcIP(), lsp.getSrcLocalID().getNumber());
		RSVPSenderTSpec senderTSpec = new RSVPSenderTSpec(lsp.getBandwidthAsFloat(), 
														lsp.getBandwidthAsFloat(), 
														lsp.getBandwidthAsFloat(), 
														100, 1500);	//default value
		RSVPAdspec adspec = new RSVPAdspec(false, 1, Float.POSITIVE_INFINITY, 0, 2147483647);	//default values
		
		pathMsg.setSession(session);
		pathMsg.setHop(hop);
		pathMsg.setTimeValues(timeValues);
		pathMsg.setLabelRequest(labelRequest);
		pathMsg.setDragonUNI(dragonUNI);
		pathMsg.setSessionAttribute(sessionAttribute);
		pathMsg.setSenderTemplate(senderTemplate);
		pathMsg.setSenderTSpec(senderTSpec);
		pathMsg.setAdspec(adspec);
		
		return this.startNewSession(pathMsg);
	}
	
	public boolean endSession(String sessionName) throws InterruptedIOException, IOException{
		/* Retrieve session */
		UNISession session = this.sessions.get(sessionName);
		if(session == null){
			return false;
		}
		
		/* Send path tear and delete session */
		session.cancel();
		this.sessions.remove(sessionName);
		
		return true;
		
	}
	
	public void endAllSessions() throws InterruptedIOException, IOException{
		for(String sessionName: this.sessions.keySet()){
			this.endSession(sessionName);
		}
		
	}
	
	private class ReceiveThread extends Thread{
		public void run() {
			while(true){
				byte[] data = new byte[1500];
				try {
					sock.read(remoteAddress,data);
					System.out.println("RSVP INCOMING:");
					System.out.println(ByteUtil.hexStringFromBytes(data));
				} catch (InterruptedIOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
			
		}
		
	}
	
	public static void main(String[] args){
		try {
			UNIClient uni = new UNIClient("10.0.1.2", "10.0.1.1");
			DragonLocalID srcId = new DragonLocalID(13, DragonLocalID.UNTAGGED_PORT);
			DragonLocalID dstId = new DragonLocalID(13, DragonLocalID.UNTAGGED_PORT);
			DragonLSP lsp = new DragonLSP(InetAddress.getByName("207.75.164.207"), 
											srcId, InetAddress.getByName("207.75.164.204"), 
											dstId, DragonLSP.BANDWIDTH_ETHERNET_100M, 3000);
			String sessionName = uni.startNewSession(lsp);
			
			Thread.sleep(10000);
			
			if(uni.endSession(sessionName)){
				System.out.println(sessionName + " deleted.");
			}else{
				System.out.println(sessionName + " NOT deleted.");
			}
		} catch (IllegalStateException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		} catch (IOException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
}
