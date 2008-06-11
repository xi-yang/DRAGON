package edu.internet2.hopi.dragon.uni.rsvp.header;

import edu.internet2.hopi.dragon.util.ByteUtil;

/**
 * RFC2205
 * 
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class RSVPTimeValues extends RSVPObject{
	private byte[] refreshPeriod;
	
	public RSVPTimeValues(int refreshPeriod) {
		super(8, 5, 1);
		this.refreshPeriod = ByteUtil.intToFourBytes(refreshPeriod);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			rawBytes[i] = this.refreshPeriod[i-4];
		}
		
		return rawBytes;
	}
	
	public int getRefreshPeriod(){
		return ByteUtil.intFromFourBytes(refreshPeriod);
	}

}
