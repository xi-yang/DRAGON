package edu.internet2.hopi.dragon.uni.rsvp.header.tlv;

public abstract class TLV {
	private byte[] type;
	private byte[] length;
	private int intLength;
	
	public TLV(int type, int length){
		this.type = new byte[2];
		this.type[0] = (byte)((type & 65280) >> 8);
		this.type[1] = (byte)(type & 255);
		
		this.length = new byte[2];
		this.length[0] = (byte)((length & 65280) >> 8);
		this.length[1] = (byte)(length & 255);
		
		intLength = length;
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = new byte[intLength];
		for(int i = 0; i < rawBytes.length; i++){
			if(i >= 0 && i <= 1){
				rawBytes[i] = this.type[i];
			}else if(i >= 2 && i <= 3){
				rawBytes[i] = this.length[i - 2];
			}
		}
		
		return rawBytes;
	}
}
