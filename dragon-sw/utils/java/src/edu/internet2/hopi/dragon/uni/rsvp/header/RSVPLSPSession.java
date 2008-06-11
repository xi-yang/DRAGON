package edu.internet2.hopi.dragon.uni.rsvp.header;

import java.net.InetAddress;

import edu.internet2.hopi.dragon.util.ByteUtil;

public class RSVPLSPSession extends RSVPObject{
	private byte[] destAddress;
	private byte[] tunnelId;
	private byte[] extendedTunnelId;
	
	public RSVPLSPSession(InetAddress destAddress, int tunnelId, InetAddress extendedTunnelId) {
		super(16, 1, 7);
		
		this.destAddress = destAddress.getAddress();
		this.extendedTunnelId = extendedTunnelId.getAddress();
		this.tunnelId = ByteUtil.intToTwoBytes(tunnelId);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i >= 4 && i <= 7){
				rawBytes[i] = this.destAddress[i-4];
			}else if(i >= 8 && i <= 9){
				rawBytes[i] = 0; //reserved
			}else if(i >= 10 && i <=11){
				rawBytes[i] = this.tunnelId[i-10];
			}else if(i >= 12 && i <= 15){
				rawBytes[i] = this.extendedTunnelId[i-12];
			}
		}
		
		return rawBytes;
	}

}
