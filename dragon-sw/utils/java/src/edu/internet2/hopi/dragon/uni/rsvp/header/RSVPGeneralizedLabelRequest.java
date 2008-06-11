package edu.internet2.hopi.dragon.uni.rsvp.header;

import edu.internet2.hopi.dragon.util.ByteUtil;

/**
 * RFC3473 Section 2.1
 * @author arlake
 *
 */
public class RSVPGeneralizedLabelRequest extends RSVPObject{
	private byte encodingType;
	private byte switchingType;
	private byte[] gpid;
	
	/* Type constants - RFC 3471 Section 3.1.1 */
	public static final int ENCODING_PACKET = 1;
	public static final int ENCODING_ETHERNET = 2;
	public static final int ENCODING_PDH = 3;
	public static final int ENCODING_SDHSONET = 5;
	public static final int ENCODING_DIGITALWRAPPER = 7;
	public static final int ENCODING_LAMBDA = 8;
	public static final int ENCODING_FIBER = 9;
	public static final int ENCODING_FIBERCHANNEL = 11;
	
	public static final int SWCAP_PSC1 = 1;
	public static final int SWCAP_PSC2 = 2;
	public static final int SWCAP_PSC3 = 3;
	public static final int SWCAP_PSC4 = 4;
	public static final int SWCAP_L2SC = 51;
	public static final int SWCAP_TDM = 100;
	public static final int SWCAP_LAMBDA = 150;
	public static final int SWCAP_FIBER = 200;
	
	public static final int GPID_ETHERNET = 33;
	public static final int GPID_SDHSONET = 34;
	public static final int GPID_LAMBDA = 37;
	
	public RSVPGeneralizedLabelRequest(int encodingType, int switchingType, int gpid) {
		super(8, 19, 4);
		
		this.encodingType = (byte)(encodingType & 255);
		this.switchingType = (byte)(switchingType & 255);
		this.gpid = ByteUtil.intToTwoBytes(gpid);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i == 4){
				rawBytes[i] = this.encodingType;
			}else if(i == 5){
				rawBytes[i] = this.switchingType;
			}else if(i >= 6 && i <= 7){
				rawBytes[i] = this.gpid[i-6];
			}
		}
		
		return rawBytes;
	}

}
