package edu.internet2.hopi.dragon.uni.rsvp.header;

import java.net.InetAddress;
import java.util.List;

import edu.internet2.hopi.dragon.uni.rsvp.header.tlv.IPv4TLV;
import edu.internet2.hopi.dragon.util.ByteUtil;

/**
 * RFC3473 Section 8.1.1
 * 
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class RSVPHop extends RSVPObject{
	private byte[] hopAddress;
	private byte[] logicalInterface;
	private byte[] tlvs;
	
	public RSVPHop(InetAddress hopAddress, int logicalInterface, List<IPv4TLV> tlvList) {
		super(0, 3, 3);
		
		this.hopAddress = hopAddress.getAddress();
		this.logicalInterface = ByteUtil.intToFourBytes(logicalInterface);
		this.tlvs = new byte[8 * tlvList.size()];
		int i = 0;
		int length = 12;
		for(IPv4TLV tlv : tlvList){
			byte[] rawTLV = tlv.toBytes();
			length += rawTLV.length;
			for(int j = 0; j < rawTLV.length; j++){
				tlvs[i] = rawTLV[j];
				i++;
			}
		}
		
		this.setLength(length);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i >= 4 && i <= 7){
				rawBytes[i] = this.hopAddress[i-4];
			}else if(i >= 8 && i <= 11){
				rawBytes[i] = this.logicalInterface[i-8];
			}else{
				rawBytes[i] = this.tlvs[i-12];
			}
		}
		
		return rawBytes;
	}

}
