package edu.internet2.hopi.dragon.uni.rsvp.header;

import edu.internet2.hopi.dragon.util.ByteUtil;

public class CommonHeader {
	private byte versionAndFlags;
	private byte type;
	private byte[] checksum;
	private byte sendTTL;
	private byte[] length;
	
	public CommonHeader(int version, int flags, int type, int ttl, int length){
		this.versionAndFlags = (byte)((version & 15) << 4);
		this.versionAndFlags += (byte)(flags & 15);
		this.type = (byte)(type & 255);
		this.checksum = ByteUtil.intToTwoBytes(0);
		this.sendTTL = (byte)(ttl & 255);
		this.length = ByteUtil.intToTwoBytes(length);
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = new byte[8];
		for(int i = 0; i < rawBytes.length; i++){
			if(i == 0){
				rawBytes[i] = this.versionAndFlags;
			}else if(i == 1){
				rawBytes[i] = this.type;
			}else if(i >= 2 && i <= 3){
				rawBytes[i] = this.checksum[i - 2];
			}else if(i == 4){
				rawBytes[i] = this.sendTTL;
			}else if(i == 5){
				rawBytes[i] = 0; //reserved
			}else if(i >= 6 && i <= 7){
				rawBytes[i] = this.length[i-6];
			}
		}
		return rawBytes;
	}

	/**
	 * @return the length
	 */
	public int getLength() {
		return ByteUtil.intFromTwoBytes(this.length);
	}

	/**
	 * @param length the length to set
	 */
	public void setLength(int length) {
		this.length = ByteUtil.intToTwoBytes(length);
	}

	/**
	 * @return the sendTTL
	 */
	public byte getSendTTL() {
		return sendTTL;
	}

	/**
	 * @param sendTTL the sendTTL to set
	 */
	public void setSendTTL(byte sendTTL) {
		this.sendTTL = sendTTL;
	}

	/**
	 * @return the type
	 */
	public byte getType() {
		return type;
	}

	/**
	 * @param type the type to set
	 */
	public void setType(byte type) {
		this.type = type;
	}

	/**
	 * @return the versionAndFlags
	 */
	public byte getVersionAndFlags() {
		return versionAndFlags;
	}

	/**
	 * @param versionAndFlags the versionAndFlags to set
	 */
	public void setVersionAndFlags(byte versionAndFlags) {
		this.versionAndFlags = versionAndFlags;
	}

	/**
	 * @return the checksum
	 */
	public int getChecksum() {
		return ByteUtil.intFromTwoBytes(checksum);
	}

	/**
	 * @param checksum the checksum to set
	 */
	public void setChecksum(int checksum) {
		this.checksum = ByteUtil.intToTwoBytes(checksum);
	}
	
	
}
