package edu.internet2.hopi.dragon.uni.rsvp.header;

import edu.internet2.hopi.dragon.util.ByteUtil;

public abstract class RSVPObject {
	protected byte[] length;
	protected byte classNum;
	protected byte cType;
	protected int intLength;
	
	public RSVPObject(int length, int classNum, int cType){
		this.setLength(length);
		this.cType = (byte)(cType & 255);
		this.classNum = (byte)(classNum & 255);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = new byte[intLength];
		for(int i = 0; i < 4; i++){
			if(i >= 0 && i <= 1){
				rawBytes[i] = this.length[i];
			}else if(i == 2){
				rawBytes[i] = this.classNum;
			}else if(i == 3){
				rawBytes[i] = this.cType;
			} 
		}
		
		return rawBytes;
	}
	
	protected void setLength(int length){
		this.length = ByteUtil.intToTwoBytes(length);
		this.intLength = length;
	}
}
