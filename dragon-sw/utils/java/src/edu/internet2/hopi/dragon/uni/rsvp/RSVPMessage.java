package edu.internet2.hopi.dragon.uni.rsvp;

import java.util.ArrayList;

import edu.internet2.hopi.dragon.uni.rsvp.header.CommonHeader;
import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPObject;
import edu.internet2.hopi.dragon.util.ByteUtil;

public abstract class RSVPMessage {
	protected CommonHeader header;
	protected ArrayList<RSVPObject> objects;
	private String messageType;
	
	public RSVPMessage(String messageType){
		this.messageType = messageType;
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = new byte[header.getLength()];
		int offset = 0;
		
		/* Add common header */
		byte[]headerBytes = header.toBytes();
		rawBytes = this.addRawPacketHeader(offset, rawBytes, headerBytes);
		offset += headerBytes.length;
		
		/* Add objects */
		for(RSVPObject obj: objects){
			if(obj != null){
				byte[] objBytes = obj.toBytes();
				rawBytes = this.addRawPacketHeader(offset, rawBytes, objBytes);
				offset += objBytes.length;
			}
		}
			
		return rawBytes;
	}
	
	public RSVPMessage fromBytes(byte[] rawMessage, boolean includesIPHeader){
		if(includesIPHeader){
			
		}
		
		return null;
	}
	
	protected void setObject(int objectIndex, RSVPObject object){
		this.objects.set(objectIndex, object);
		this.calculateSize();
		this.calculateChecksum();
	}
	
	private byte[] addRawPacketHeader(int offset, byte[] packet, byte[] header){
		if(header != null){
			for(int i = offset; (i - offset) < header.length; i++){
				packet[i] = header[i - offset];
			}
		}
		
		return packet;
	}
	
	private void calculateSize(){
		int size = 8;	/* size of header = 8 */
		for(RSVPObject obj: objects){
			if(obj != null){
				size += obj.toBytes().length;
			}
		}
		
		this.header.setLength(size);
	}
	
	private void calculateChecksum(){
		/* Reset checksum */
		this.header.setChecksum(0);
		
		/* Initialize */
		byte[] rawBytes = this.toBytes();
		int checksum = 0;
		
		/* Calculate sum */
		byte[] tmp = new byte[2];
		for(int i = 0; i < rawBytes.length; i++){
			if((i % 2) == 0){
				tmp = new byte[2];
				tmp[0] = rawBytes[i];
			}else{
				tmp[1] = rawBytes[i];
				checksum += ByteUtil.intFromTwoBytes(tmp);
			}
		}
		
		/* Adjust for carries to make one's complement sum */
		int carries = ((checksum & 0xFFFF0000) >> 16);
		checksum &= 0xFFFF;
		checksum += carries;
		
		/* Convert to bytes and perform one's complement on sum */
		rawBytes = ByteUtil.intToTwoBytes(checksum);
		rawBytes[0] = (byte) ((~rawBytes[0]) & 255);
		rawBytes[1] = (byte) ((~rawBytes[1]) & 255);
		
		/* set check sum in header */
		this.header.setChecksum(ByteUtil.intFromTwoBytes(rawBytes));
	}
	
	public String toString(){
		String str = "RSVP " + this.messageType + " MESSAGE: \n";
		byte[] buf = this.toBytes();
		str += ByteUtil.hexStringFromBytes(buf);
		
		return str;
	}
}
