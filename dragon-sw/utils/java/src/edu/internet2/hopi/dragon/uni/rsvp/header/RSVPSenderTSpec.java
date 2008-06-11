package edu.internet2.hopi.dragon.uni.rsvp.header;

import edu.internet2.hopi.dragon.util.ByteUtil;

/**
 * RFC2210 Section 3.1
 * 
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class RSVPSenderTSpec extends RSVPObject{
	private byte messageFormat;//also includes reserved
	private byte[] dataLength;
	private byte serviceHeader;
	private byte[] lengthOfService;
	private byte parameterId;
	private byte parameterFlags;
	private byte[] parameterDataLength;
	private byte[] bucketRate;
	private byte[] bucketSize;
	private byte[] peakDataRate;
	private byte[] minPolicedUnit;
	private byte[] maxPacketSize;
	
	public RSVPSenderTSpec(float bucketRate, float bucketSize, float peakDataRate, 
							int minPolicedUnit, int maxPacketSize) {
		super(36, 12, 2); //TODO: Why is subtype 2 not 1?
		
		this.messageFormat = 0;
		this.dataLength = ByteUtil.intToTwoBytes(7);
		this.serviceHeader = 1;
		this.lengthOfService = ByteUtil.intToTwoBytes(6);
		this.parameterId = 127;
		this.parameterFlags = 0;
		this.parameterDataLength = ByteUtil.intToTwoBytes(5);
		this.bucketRate = ByteUtil.floatToBytes(bucketRate);
		this.bucketSize = ByteUtil.floatToBytes(bucketSize);
		this.peakDataRate = ByteUtil.floatToBytes(peakDataRate);
		this.minPolicedUnit = ByteUtil.intToFourBytes(minPolicedUnit);
		this.maxPacketSize = ByteUtil.intToFourBytes(maxPacketSize);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i == 4){
				rawBytes[i] = this.messageFormat;
			}else if(i == 5){
				rawBytes[i] = 0;
			}else if(i >= 6 && i <= 7){
				rawBytes[i] = this.dataLength[i-6];
			}else if(i == 8){
				rawBytes[i] = this.serviceHeader;
			}else if(i == 9){
				rawBytes[i] = 0;
			}else if(i >= 10 && i <= 11){
				rawBytes[i] = this.lengthOfService[i-10];
			}else if(i == 12){
				rawBytes[i] = this.parameterId;
			}else if(i == 13){
				rawBytes[i] = this.parameterFlags;
			}else if(i >= 14 && i <= 15){
				rawBytes[i] = this.parameterDataLength[i-14];
			}else if(i >= 16 && i <= 19){
				rawBytes[i] = this.bucketRate[i-16];
			}else if(i >= 20 && i <= 23){
				rawBytes[i] = this.bucketSize[i-20];
			}else if(i >= 24 && i <= 27){
				rawBytes[i] = this.peakDataRate[i-24];
			}else if(i >= 28 && i <= 31){
				rawBytes[i] = this.minPolicedUnit[i-28];
			}else if(i >= 32 && i <= 35){
				rawBytes[i] = this.maxPacketSize[i-32];
			}
		}
		
		return rawBytes;
	}

}
