package edu.internet2.hopi.dragon.uni.rsvp.header.tlv;

import java.net.InetAddress;

public class IPv4TLV extends TLV{
	private byte[] value;
	
	public IPv4TLV(InetAddress value) {
		super(1, 8);
		this.value = value.getAddress();
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			rawBytes[i] = this.value[i-4];
		}
		return rawBytes;
	}

}
