package edu.internet2.hopi.dragon.uni.rsvp;

import java.net.UnknownHostException;
import java.util.ArrayList;
import edu.internet2.hopi.dragon.uni.rsvp.header.*;

public class RSVPPath extends RSVPMessage{
	/* Define constants */
	private final int INTEGRITY_OBJECT = 0;
	private final int SESSION_OBJECT = 1;
	private final int HOP_OBJECT = 2;
	private final int TIME_VALUES_OBJECT = 3;
	private final int LABEL_REQUEST_OBJECT = 4;
	private final int DRAGON_UNI_OBJECT = 5;
	private final int EXPLICIT_ROUTE_OBJECT = 6;
	private final int SESSION_ATTRIBUTE_OBJECT = 7;
	private final int SENDER_TEMPLATE_OBJECT = 8;
	private final int SENDER_TSPEC_OBJECT = 9;
	private final int ADSPEC_OBJECT = 10;
	private final int RECORD_ROUTE_OBJECT = 11;
	private final int MAX_OBJECTS = 12;
	
	public RSVPPath() throws UnknownHostException{
		super("PATH");
		this.header = new CommonHeader(1, 1, 1, 50, 0);
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
	
	public RSVPTimeValues getTimeValues(){
		return (RSVPTimeValues) this.objects.get(this.TIME_VALUES_OBJECT);
	}
	
	public void setTimeValues(RSVPTimeValues timeValues){
		this.setObject(this.TIME_VALUES_OBJECT, timeValues);
	}
	
	public RSVPGeneralizedLabelRequest getLabelRequest(){
		return (RSVPGeneralizedLabelRequest) this.objects.get(this.LABEL_REQUEST_OBJECT);
	}
	
	public void setLabelRequest(RSVPGeneralizedLabelRequest labelRequest){
		this.setObject(this.LABEL_REQUEST_OBJECT, labelRequest);
	}
	
	public void setDragonUNI(RSVPDragonUNI dragonUNI){
		this.setObject(this.DRAGON_UNI_OBJECT, dragonUNI);
	}
	
	public RSVPDragonUNI getDragonUNI(){
		return (RSVPDragonUNI) this.objects.get(this.DRAGON_UNI_OBJECT);
	}
	
	public RSVPExplicitRoute getExplicitRoute(){
		return (RSVPExplicitRoute) this.objects.get(this.EXPLICIT_ROUTE_OBJECT);
	}
	
	public void setExplicitRoute(RSVPExplicitRoute explicitRoute){
		this.setObject(this.EXPLICIT_ROUTE_OBJECT, explicitRoute);
	}
	
	public RSVPSessionAttribute getSessionAttribute(){
		return (RSVPSessionAttribute) this.objects.get(this.SESSION_ATTRIBUTE_OBJECT);
	}
	
	public void setSessionAttribute(RSVPSessionAttribute sessionAttribute){
		this.setObject(this.SESSION_ATTRIBUTE_OBJECT, sessionAttribute);
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
	
	public RSVPRecordRoute getRecordRoute(){
		return (RSVPRecordRoute) this.objects.get(this.RECORD_ROUTE_OBJECT);
	}
	
	public void setRecordRoute(RSVPRecordRoute recordRoute){
		this.setObject(this.RECORD_ROUTE_OBJECT, recordRoute);
	}
}
