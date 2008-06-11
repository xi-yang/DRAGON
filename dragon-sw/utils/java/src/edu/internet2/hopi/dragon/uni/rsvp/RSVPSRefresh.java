package edu.internet2.hopi.dragon.uni.rsvp;

import java.util.ArrayList;

import edu.internet2.hopi.dragon.uni.rsvp.header.CommonHeader;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPMessageIDList;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPObject;

/**
 * RFC2961 Section 5.2
 * 
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class RSVPSRefresh extends RSVPMessage{	
	private final int MESSAGE_ID_LIST_OBJECT = 0;
	private final int MAX_OBJECTS = 1;
	
	public RSVPSRefresh(){
		super("SREFRESH");
		this.header = new CommonHeader(1, 1, 15, 16, 0);//TODO: check TTL
		this.objects = new ArrayList<RSVPObject>(this.MAX_OBJECTS);
	}
	
	public RSVPMessageIDList getMessageIDList(){
		return (RSVPMessageIDList) this.objects.get(this.MESSAGE_ID_LIST_OBJECT);
	}
	
	public void setMessageIDList(RSVPMessageIDList messageIDList){
		this.setObject(this.MESSAGE_ID_LIST_OBJECT, messageIDList);
	}
}
