package edu.internet2.hopi.dragon.narb.api;

import java.net.InetAddress;

/**
 * Represents an unnumbered interface subobject
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class UnumInterfaceSubobject extends EROSubobject {
	private byte[] reserved;
	private InetAddress ip;
	private int interfaceId;
	
	/**
	 * Constructor that creates UnumInterfaceSubobject from passed parameters
	 * @param loose true if loose hop, false if strict hop
	 * @param tlvLength length of subobject including length and type fields
	 * @param reserved reserved bytes
	 * @param ip IPv4 address of this subobject
	 * @param interfaceId interface ID of this subobject
	 */
	public UnumInterfaceSubobject(boolean loose, byte tlvLength, byte[] reserved, InetAddress ip, int interfaceId){
		this.loose = loose;
		this.type = UNUM_IF_SUBOBJ;
		this.tlvLength = tlvLength;
		this.reserved = reserved;
		this.ip = ip;
		this.interfaceId = interfaceId;
	}

	/**
	 * Returns the interface ID
	 * @return the interface ID
	 */
	public int getInterfaceId() {
		return interfaceId;
	}

	/**
	 * Sets the interface ID
	 * @param interfaceId the interface ID to set
	 */
	public void setInterfaceId(int interfaceId) {
		this.interfaceId = interfaceId;
	}

	/**
	 * Returns the IPv4 address of this subobject
	 * @return the IPv4 address of this subobject
	 */
	public InetAddress getIp() {
		return ip;
	}

	/**
	 * Sets the IPv4 address of this subobject
	 * @param ip the IPv4 address of this subobject
	 */
	public void setIp(InetAddress ip) {
		this.ip = ip;
	}

	/**
	 * Returns the reserved byte
	 * @return the reserved byte
	 */
	public byte[] getReserved() {
		return reserved;
	}

	/**
	 * Sets the reserved bytes
	 * @param reserved the reserved bytes to set
	 */
	public void setReserved(byte[] reserved) {
		this.reserved = reserved;
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		
		byte[] rawIP = this.ip.getAddress();
		byte[] rawInterfaceId = new byte[4];
		rawInterfaceId[0] = (byte)(interfaceId >> 24);
		rawInterfaceId[1] = (byte)((interfaceId & 16711680) >> 16);
		rawInterfaceId[2] = (byte)((interfaceId & 65280) >> 8);
		rawInterfaceId[3] = (byte)(interfaceId & 255);
		for(int i = 2; i < rawBytes.length; i++){
			if(i >= 2 && i <= 3){
				rawBytes[i] = 0; //reserved
			}else if(i >= 4 && i <= 7){
				rawBytes[i] = rawIP[i-4];
			}else if(i >= 8 && i <= 11){
				rawBytes[i] = rawInterfaceId[i-8];
			}
		}
		return rawBytes;
	}
	
	
}
