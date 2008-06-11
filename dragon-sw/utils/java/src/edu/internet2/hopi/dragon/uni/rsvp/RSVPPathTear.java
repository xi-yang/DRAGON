package edu.internet2.hopi.dragon.uni.rsvp;

import java.net.UnknownHostException;
import java.util.ArrayList;
import edu.internet2.hopi.dragon.uni.rsvp.header.CommonHeader;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPAdspec;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPDragonUNI;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPHop;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPIntegrityObject;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPLSPSession;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPObject;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPSenderTSpec;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPSenderTemplate;

public class RSVPPathTear extends RSVPMessage{
	private final int INTEGRITY_OBJECT = 0;
	private final int SESSION_OBJECT = 1;
	private final int HOP_OBJECT = 2;
	private final int DRAGON_UNI_OBJECT = 3;
	private final int SENDER_TEMPLATE_OBJECT = 4;
	private final int SENDER_TSPEC_OBJECT = 5;
	private final int ADSPEC_OBJECT = 6;
	private final int MAX_OBJECTS = 7;
	
	public RSVPPathTear() throws UnknownHostException {
		super("PATHTEAR");
		this.header = new CommonHeader(1, 1, 5, 50, 0);
		this.objects = new ArrayList<RSVPObject>(MAX_OBJECTS);
		
		/* Initialize objects */
		for(int i = 0; i < MAX_OBJECTS; i++){
			this.objects.add(null);
		}	
	}
	
	public RSVPIntegrityObject getIntegrity(){
		return (RSVPIntegrityObject) this.objects.get(this.INTEGRITY_OBJECT);
	}
	
	public void setIntegrity(RSVPIntegrityObject integrityObject){
		this.setObject(this.INTEGRITY_OBJECT, integrityObject);
	}
	
	public RSVPLSPSession getSession(){
		return (RSVPLSPSession) this.objects.get(this.SESSION_OBJECT);
	}
	
	public void setSession(RSVPLSPSession session){
		this.setObject(this.SESSION_OBJECT, session);
	}
	
	public RSVPHop getHop(){
		return (RSVPHop) this.objects.get(this.HOP_OBJECT);
	}
	
	public void setHop(RSVPHop hop){
		this.setObject(this.HOP_OBJECT, hop);
	}
	
	public void setDragonUNI(RSVPDragonUNI dragonUNI){
		this.setObject(this.DRAGON_UNI_OBJECT, dragonUNI);
	}
	
	public RSVPDragonUNI getDragonUNI(){
		return (RSVPDragonUNI) this.objects.get(this.DRAGON_UNI_OBJECT);
	}
	
	public RSVPSenderTemplate getSenderTemplate(){
		return (RSVPSenderTemplate) this.objects.get(this.SENDER_TEMPLATE_OBJECT);
	}
	
	public void setSenderTemplate(RSVPSenderTemplate senderTemplate){
		this.setObject(this.SENDER_TEMPLATE_OBJECT, senderTemplate);
	}
	
	public RSVPSenderTSpec getSenderTSpec(){
		return (RSVPSenderTSpec) this.objects.get(this.SENDER_TSPEC_OBJECT);
	}
	
	public void setSenderTSpec(RSVPSenderTSpec senderTSpec){
		this.setObject(this.SENDER_TSPEC_OBJECT, senderTSpec);
	}
	
	public RSVPAdspec getAdSpec(){
		return (RSVPAdspec) this.objects.get(this.ADSPEC_OBJECT);
	}
	
	public void setAdspec(RSVPAdspec adspec){
		this.setObject(this.ADSPEC_OBJECT, adspec);
	}

}
