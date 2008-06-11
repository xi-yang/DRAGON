package edu.internet2.hopi.dragon.uni.rsvp.header;

import edu.internet2.hopi.dragon.util.ByteUtil;

/**
 * RFC2210 Section 3.3
 * 
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class RSVPAdspec extends RSVPObject{
	private byte messageFormat;
	private byte[] dataLength;
	private byte serviceHeader;
	private byte breakBit;	//includes reserved
	private byte[] genParamDataLength;
	private byte[] isHopCount;
	private byte isParamId;
	private byte isParamFlags;
	private byte[] isParamDataLength;
	private byte bwParamId;
	private byte bwParamFlags;
	private byte[] bwParamDataLength;
	private byte[] pathBandwidthEstimate;
	private byte latencyParamId;
	private byte latencyParamFlags;
	private byte[] latencyParamDataLength;
	private byte[] minPathLatency;
	private byte mtuParamId;
	private byte mtuParamFlags;
	private byte[] mtuParamDataLength;
	private byte[] composedMTU;
	
	public RSVPAdspec(boolean breakBit, int isHopCount, float pathBandwidthEstimate, 
						int minPathLatency, int composedMTU) {
		super(44, 13, 2);
		
		this.messageFormat = 0;
		this.dataLength = ByteUtil.intToTwoBytes(9);
		this.serviceHeader = 1;
		if(breakBit){
			this.breakBit = (byte)((byte)1 << 7);
		}else{
			this.breakBit = 0;
		}
		this.genParamDataLength = ByteUtil.intToTwoBytes(8);
		
		this.isParamId = 4;
		this.isParamFlags = 0;
		this.isParamDataLength = ByteUtil.intToTwoBytes(1);
		this.isHopCount = ByteUtil.intToFourBytes(isHopCount);
	
		this.bwParamId = 6;
		this.bwParamFlags = 0;
		this.bwParamDataLength = ByteUtil.intToTwoBytes(1);
		this.pathBandwidthEstimate = ByteUtil.floatToBytes(pathBandwidthEstimate);
		
		this.latencyParamId = 8;
		this.latencyParamFlags = 0;
		this.latencyParamDataLength = ByteUtil.intToTwoBytes(1);
		this.minPathLatency = ByteUtil.intToFourBytes(minPathLatency);
		
		this.mtuParamId = 10;
		this.mtuParamFlags = 0;
		this.mtuParamDataLength = ByteUtil.intToTwoBytes(1);
		this.composedMTU = ByteUtil.intToFourBytes(composedMTU);
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
				rawBytes[i] = this.breakBit;
			}else if(i >= 10 && i <= 11){
				rawBytes[i] = this.genParamDataLength[i-10];
			}else if(i == 12){
				rawBytes[i] = this.isParamId;
			}else if(i == 13){
				rawBytes[i] = this.isParamFlags;
			}else if(i >= 14 && i <= 15){
				rawBytes[i] = this.isParamDataLength[i-14];
			}else if(i >= 16 && i <= 19){
				rawBytes[i] = this.isHopCount[i-16];
			}else if(i == 20){
				rawBytes[i] = this.bwParamId;
			}else if(i == 21){
				rawBytes[i] = this.bwParamFlags;
			}else if(i >= 22 && i <= 23){
				rawBytes[i] = this.bwParamDataLength[i-22];
			}else if(i >= 24 && i <= 27){
				rawBytes[i] = this.pathBandwidthEstimate[i-24];
			}else if(i == 28){
				rawBytes[i] = this.latencyParamId;
			}else if(i == 29){
				rawBytes[i] = this.latencyParamFlags;
			}else if(i >= 30 && i <= 31){
				rawBytes[i] = this.latencyParamDataLength[i-30];
			}else if(i >= 32 && i <= 35){
				rawBytes[i] = this.minPathLatency[i-32];
			}else if(i == 36){
				rawBytes[i] = this.mtuParamId;
			}else if(i == 37){
				rawBytes[i] = this.mtuParamFlags;
			}else if(i >= 38 && i <= 39){
				rawBytes[i] = this.mtuParamDataLength[i-38];
			}else if(i >= 40 && i <= 43){
				rawBytes[i] = this.composedMTU[i-40];
			}
		}
		
		return rawBytes;
	}
}
