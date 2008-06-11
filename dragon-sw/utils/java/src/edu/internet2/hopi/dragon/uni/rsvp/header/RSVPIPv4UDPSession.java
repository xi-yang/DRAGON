package edu.internet2.hopi.dragon.uni.rsvp.header;

import java.net.InetAddress;

import edu.internet2.hopi.dragon.util.ByteUtil;

public class RSVPIPv4UDPSession extends RSVPObject{
	private byte[] destAddress;
	private byte protocolId;
	private byte flags;
	private byte[] destPort;
	
	public RSVPIPv4UDPSession(InetAddress destAddress, int protocolId, int flags, int dstPort) {
		super(12, 1, 1);
		
		this.destAddress = destAddress.getAddress();
		this.protocolId = (byte)(protocolId & 255);
		this.flags = (byte)(flags & 255);
		this.destPort = ByteUtil.intToTwoBytes(dstPort);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i >= 4 && i <= 7){
				rawBytes[i] = this.destAddress[i-4];
			}else if(i == 8){
				rawBytes[i] = this.protocolId;
			}else if(i == 9){
				rawBytes[i] = this.flags;
			}else if(i >= 10 && i <= 11){
				rawBytes[i] = this.destPort[i-10];
			}
		}
		
		return rawBytes;
	}

}
