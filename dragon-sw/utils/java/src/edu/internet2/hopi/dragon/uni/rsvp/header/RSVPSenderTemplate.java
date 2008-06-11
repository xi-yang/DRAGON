package edu.internet2.hopi.dragon.uni.rsvp.header;

import java.net.InetAddress;
import java.net.UnknownHostException;

import edu.internet2.hopi.dragon.util.ByteUtil;

/**
 * RFC3209 Section 4.6.2
 * 
 * @author arlake
 *
 */
public class RSVPSenderTemplate extends RSVPObject{
	private byte[] senderAddress;
	private byte[] lspId;
	
	public RSVPSenderTemplate(InetAddress senderAddress, int lspId) {
		super(12, 11, 7);
		
		this.senderAddress = senderAddress.getAddress();
		this.lspId = ByteUtil.intToTwoBytes(lspId);
	}

	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i >= 4 && i <= 7){
				rawBytes[i] = this.senderAddress[i-4];
			}else if(i >= 8 && i <= 9){
				rawBytes[i] = 0; //reserved
			}else if(i >= 10 && i <= 11){
				rawBytes[i] = this.lspId[i-10];
			}
		}
		
		return rawBytes;
	}
	
	public InetAddress getSenderAddress(){
		try {
			return InetAddress.getByAddress(senderAddress);
		} catch (UnknownHostException e) {
			return null;
		}
	}
}
