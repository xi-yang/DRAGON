package edu.internet2.hopi.dragon.narb.api;

import java.net.InetAddress;
import java.net.UnknownHostException;

/**
 * Contains the parameters of a NARB API application request. 
 * NARB application requests are used to retrieve a path between 
 * two endpoints and optionally available VLAN tags. 
 * App2NARBRequests are issued to the NARB server by NARB clients.
 * 
 * @author Andrew Lake (alake@internet2.edu)
 */
public class App2NARBRequest {
	private NARBMessageHeader header;
	private byte[] type;
	private byte[] length;
	private byte[] source;
	private byte[] destination;
	private byte encodingType;
	private byte switchingType;
	private byte[] gpid;
	private byte[] bandwidth;
	private int packetSize;
	
	/* Encoding values */
	public final static byte ENCODING_ETHERNET = 2;
	
	/* Switching capability values */
	public final static byte SWCAP_L2SC = 51;
	
	/* GPID values */
	public final static byte[] GPID_ETHERNET = {0, 8};
	
	/**
	 * Constructor that generates a request using specific values for encoding, switching capability, and gpid
	 * @param src the address or hostname of the source to be used in the path computation
	 * @param dst the address or hostname of the destionation to be used in the path computation
	 * @param encoding the encoding to use along this path
	 * @param swcap the swicthing capability of the path
	 * @param gpidNum the GPID of the path
	 * @param bwidth the bandwidth required along the path
	 * @throws UnknownHostException returned when an invalid source or destination is given
	 */
	public App2NARBRequest(String src, String dst, byte encoding, byte swcap, byte[] gpidNum, float bwidth) throws UnknownHostException{
		int bodyLength = 0;
		/* Set Type */
		type = new byte[2];
		type[0] = 0x00;
		type[1] = 0x02; //TLV Request
		
		/* Set source and destination IP */
		source = InetAddress.getByName(src).getAddress();
		destination = InetAddress.getByName(dst).getAddress();
		bodyLength = source.length + destination.length;
		
		/* Set encoding */
		encodingType = encoding;
		bodyLength++;
		
		/* Set switching type */
		switchingType = swcap;
		bodyLength++;
		
		/* Set gpid */
		gpid = gpidNum;
		bodyLength += gpid.length;
		
		/* Bandwidth - reverse byte order */
		bandwidth = new byte[4];
		bandwidth[3] = (byte) (Float.floatToRawIntBits(bwidth) >> 24);
		bandwidth[2] = (byte) ((Float.floatToRawIntBits(bwidth) & 16711680) >> 16);
		bandwidth[1] = (byte) ((Float.floatToRawIntBits(bwidth) & 65280) >> 8);
		bandwidth[0] = (byte) (Float.floatToRawIntBits(bwidth) & 255);
		bodyLength +=4;
		
		/* Calculate length */
		int l = bodyLength;
		length = new byte[2];
		length[0] = (byte) ((l & 65280) >> 2);
		length[1] = (byte) (l & 255);

		/* Create header */
		header = new NARBMessageHeader(NARBMessageHeader.TYPE_LSPQ, bodyLength + 4, null, NARBMessageHeader.VTAG_ANY);
		
		/* Sum total size of packet */
		packetSize = bodyLength + NARBMessageHeader.SIZE + 4;
	}
	
	/**
	 * Constructor that generates a request using default values for encoding(ethernet), switching capability(l2sc), and gpid(ethernet)
	 * @param src the address or hostname of the source to be used in the path computation
	 * @param dst the address or hostname of the destionation to be used in the path computation
	 * @param bwidth the bandwidth required along the path
	 * @throws UnknownHostException returned when an invalid source or destination is given
	 */
	public App2NARBRequest(String src, String dst, float bwidth) throws UnknownHostException{
		this(src, dst, ENCODING_ETHERNET, SWCAP_L2SC, GPID_ETHERNET, bwidth);
	}
	
	/**
	 * Generates a NARB API message packet (including header) in raw byte form from this object.
	 * @return byte array containing a NARB API message packet (including header)
	 */
	public byte[] toBytes(){
		byte[] packet = new byte[packetSize];
		byte[] headerBytes = header.toBytes();
		int offset = headerBytes.length;
		
		for(int i = 0; i < packet.length; i++){
			if(i >= 0 && i <= (offset - 1)){
				packet[i] = headerBytes[i];
			}else if(i >= offset && i <= (offset+1)){
				packet[i] = type[i-offset];
			}else if(i >= (offset+2) && i <= (offset+3)){
				packet[i] = length[i-(offset + 2)];
			}else if(i >= (offset+4) && i <= (offset+7)){
				packet[i] = source[i-(offset+4)];
			}else if(i >= (offset+8) && i <= (offset+11)){
				packet[i] = destination[i-(offset+8)];
			}else if(i == (offset+12)){
				packet[i] = encodingType;
			}else if(i == (offset+13)){
				packet[i] = switchingType;
			}else if(i >= (offset+14) && i <= (offset+15)){
				packet[i] = gpid[i-(offset+14)];
			}else if(i >= (offset+16) && i <= (offset+19)){
				packet[i] = bandwidth[i-(offset+16)];
			}
		}
		
		return packet;
	}

	/**
	 * Returns the bandwidth in byte form. Bytes represent an IEEE754 4 byte float.
	 * @return the bandwidth in byte form
	 */
	public byte[] getBandwidth() {
		return bandwidth;
	}

	/**
	 * Sets the bandwidth using byte form. Bytes represent an IEEE754 4 byte float.
	 * @param bandwidth the bandwidth in IEEE754 byte form to set
	 */
	public void setBandwidth(byte[] bandwidth) {
		this.bandwidth = bandwidth;
	}

	/**
	 * Returns the destination IP address of this path computation request in byte form
	 * @return the destination IP address of this path computation request in byte form
	 */
	public byte[] getDestination() {
		return destination;
	}

	/**
	 * Sets the destination IP address of this path computation request using byte form
	 * @param destination the destination IP address to set in byte form
	 */
	public void setDestination(byte[] destination) {
		this.destination = destination;
	}

	/**
	 * Returns the encoding type in byte form
	 * @return the encoding type in byte form
	 */
	public byte getEncodingType() {
		return encodingType;
	}

	/**
	 * Sets the encoding type using byte form
	 * @param encodingType encoding type to set in byte form
	 */
	public void setEncodingType(byte encodingType) {
		this.encodingType = encodingType;
	}

	/**
	 * Returns the GPID in byte form
	 * @return sets the gpid in byte form
	 */
	public byte[] getGpid() {
		return gpid;
	}

	/**
	 * Sets the GPID using byte form
	 * @param gpid the GPID to set in byte form
	 */
	public void setGpid(byte[] gpid) {
		this.gpid = gpid;
	}

	/**
	 * An object representation of the header of this request
	 * @return the NARB API header of this message
	 */
	public NARBMessageHeader getHeader() {
		return header;
	}

	/**
	 * @return the length of the message body excluding the type and length fields
	 */
	public byte[] getLength() {
		return length;
	}

	/**
	 * @return the size of the packet including headers
	 */
	public int getPacketSize() {
		return packetSize;
	}

	/**
	 * Returns the source IP address of this request in byte form
	 * @return the source IP address of this request in byte form
	 */
	public byte[] getSource() {
		return source;
	}

	/**
	 * Sets the source IP address using byte form
	 * @param source the source IP address of to set in byte form
	 */
	public void setSource(byte[] source) {
		this.source = source;
	}

	/**
	 * Returns the switching capability in byte form
	 * @return the switchingType
	 */
	public byte getSwitchingType() {
		return switchingType;
	}

	/**
	 * Sets the swicthing capability using byte form
	 * @param switchingType the swicthing capability to set in byte form
	 */
	public void setSwitchingType(byte switchingType) {
		this.switchingType = switchingType;
	}

	/**
	 * Returns the TLV type in the body of this request. It is always 2 for this type of request.
	 * @return the TLV type in the body of this request (always 2)
	 */
	public byte[] getType() {
		return type;
	}

}
