package edu.internet2.hopi.dragon.uni.rsvp.header;

/**
 * RFC3209 Section 4.7.1
 * 
 * @author arlake
 */
public class RSVPSessionAttribute extends RSVPObject{
	private byte setupPriority;
	private byte holdingPriority;
	private byte flags;
	private byte nameLength;
	private byte[] sessionName;
	
	public RSVPSessionAttribute(int setupPriority, int holdingPriority, int flags, String sessionName) {
		super(0, 207, 7);
		
		this.setupPriority = (byte)(setupPriority & 255);
		this.holdingPriority = (byte)(holdingPriority & 255);
		this.flags = (byte)(flags & 255);
		byte[] unpadName = sessionName.getBytes();
		int padNameSize = sessionName.length();
		
		if((padNameSize % 4) != 0){
			padNameSize += (4 - (padNameSize % 4));
		}
		this.nameLength = (byte) (padNameSize & 255);
		this.setLength(8 + padNameSize);
		this.sessionName = new byte[padNameSize];
		for(int i = 0; i < padNameSize; i++){
			if(i < unpadName.length){
				this.sessionName[i] = unpadName[i];
			}else{
				this.sessionName[i] = 0;
			}
		}
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i == 4){
				rawBytes[i] = this.setupPriority;
			}else if(i == 5){
				rawBytes[i] = this.holdingPriority;
			}else if(i == 6){
				rawBytes[i] = this.flags;
			}else if(i == 7){
				rawBytes[i] = this.nameLength;
			}else if(i >= 8){
				rawBytes[i] = sessionName[i - 8];
			}
		}
		
		return rawBytes;
	}
	
	public String getSessionName(){
		String name = new String(this.sessionName);
		
		return name;
	}

}
