package edu.internet2.hopi.dragon;

import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPDragonUNI;

/**
 * Represents local id used by DRAGON when running in UNI mode.
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class DragonLocalID {
	private int number;
	private String type;

	public static String UNTAGGED_PORT = "port";
	public static String UNTAGGED_PORT_GROUP = "group";
	public static String TAGGED_PORT_GROUP = "tagged-group";
	public static String SUBNET_INTERFACE = "subnet-interface";
	public static String LSP_ID = "lsp-id";
	public static String TUNNEL_ID = "tunnel-id";
	
	public DragonLocalID(int number, String type){
		this.number = number;
		this.type = type;
	}
	
	public int getNumber(){
		return number;
	}
	
	public String getType(){
		return type;
	}
	
	public void setNumber(int number){
		this.number = number;
	}
	
	public void setType(String type){
		this.type = type;
	}
	
	public int getTypeAsGMPLS(){
		int n = RSVPDragonUNI.LOCAL_ID_NONE;
		
		if(this.type.equals(UNTAGGED_PORT)){
			n = RSVPDragonUNI.LOCAL_ID_PORT;
		}else if(this.type.equals(UNTAGGED_PORT_GROUP)){
			n = RSVPDragonUNI.LOCAL_ID_GROUP;
		}else if(this.type.equals(TAGGED_PORT_GROUP)){
			n = RSVPDragonUNI.LOCAL_ID_GROUP;
		}
		
		return n;
	}
}
