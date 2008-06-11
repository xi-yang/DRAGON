package edu.internet2.hopi.dragon.uni;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

import org.savarese.rocksaw.net.RawSocket;

import edu.internet2.hopi.dragon.uni.rsvp.RSVPPath;
import edu.internet2.hopi.dragon.uni.rsvp.RSVPPathTear;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPHop;
import edu.internet2.hopi.dragon.uni.rsvp.header.tlv.IPv4TLV;

public class UNISession {
	private RSVPPath pathMessage;
	private Timer sendPathThread;
	private RawSocket sock;
	private InetAddress address;
	
	public UNISession(RawSocket sock, String address, RSVPPath pathMessage) throws UnknownHostException{
		this.pathMessage = pathMessage;
		this.sendPathThread= new Timer();
		this.address = InetAddress.getByName(address);
		this.sock = sock;
	}
	
	public void start(){
		SendPathTask task = new SendPathTask(this.pathMessage);
		sendPathThread.schedule(task, 0, pathMessage.getTimeValues().getRefreshPeriod());
	}
	
	public void cancel() throws InterruptedIOException, IOException{
		RSVPPathTear pathTearMsg = new RSVPPathTear();
		
		/* Set objects */
		pathTearMsg.setSession(this.pathMessage.getSession());
		pathTearMsg.setDragonUNI(this.pathMessage.getDragonUNI());
		pathTearMsg.setSenderTemplate(this.pathMessage.getSenderTemplate());
		pathTearMsg.setSenderTSpec(this.pathMessage.getSenderTSpec());
		ArrayList<IPv4TLV> tlvs = new ArrayList<IPv4TLV>();
		tlvs.add(new IPv4TLV(InetAddress.getByName("0.0.0.0")));//data-te address (all 0s since UNI)
		RSVPHop hop = new RSVPHop(this.pathMessage.getSenderTemplate().getSenderAddress(), 4, tlvs);
		pathTearMsg.setHop(hop);
		
		/* Stop send thread */
		sendPathThread.cancel();
		
		/* Send PATH TEAR message */
		sock.write(this.address, pathTearMsg.toBytes());
	}
	
	private class SendPathTask extends TimerTask{
		private RSVPPath pathMsg;
		
		public SendPathTask(RSVPPath msg){
			this.pathMsg = msg;
		}
		
		public void run() {
			try {
				System.out.println(pathMsg);
				sock.write(address, pathMsg.toBytes());
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
