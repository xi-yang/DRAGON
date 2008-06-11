package edu.internet2.hopi.dragon.uni.rsvp.header;

import edu.internet2.hopi.dragon.util.ByteUtil;

/**
 * RFC2961 Section 5.1
 * 
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class RSVPMessageIDList extends RSVPObject{
	private byte flags;
	private byte[] epoch;
	private byte[] messageID;
	
	public RSVPMessageIDList(int flags, int epoch, int[] messageID) {
		super(0, 25, 1);
		this.flags = (byte)(flags & 255);
		this.epoch = ByteUtil.intToThreeBytes(epoch);
		this.messageID = new byte[messageID.length * 4];
		for(int i = 0; i < messageID.length; i++){
			byte[] mID = ByteUtil.intToFourBytes(messageID[i]);
			for(int j = 0; j < 4; j++){
				this.messageID[i*4 + j] = mID[j];
			}
		}
		
		this.setLength(8 + this.messageID.length);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i == 4){
				rawBytes[i] = this.flags;
			}else if(i >= 5 && i <= 7){
				rawBytes[i] = this.epoch[i-5]; //reserved
			}else if(i >= 8){
				rawBytes[i] = this.messageID[i-8];
			}
		}
		
		return rawBytes;
	}
	
}
