package edu.internet2.hopi.dragon.uni.rsvp.header;

import java.net.InetAddress;

import edu.internet2.hopi.dragon.util.ByteUtil;

public class RSVPDragonUNI extends RSVPObject{
	private byte[] srcLocalID;
	private byte[] dstLocalID;
	private byte[] vlan;
	private byte[] ingressChannel;
	private byte[] egressChannel;
	
	public static int LOCAL_ID_NONE = 0;
	public static int LOCAL_ID_PORT = 0x010000;
	public static int LOCAL_ID_GROUP = 0x020000;
	public static int LOCAL_ID_TAGGED_GROUP = 0x030000;
	public static int LOCAL_ID_TAGGED_GROUP_GLOBAL = 0x040000;
	public static int LOCAL_ID_UNI_SRC= 0x100000;
	public static int LOCAL_ID_UNI_DST= 0x110000;
	
	public RSVPDragonUNI(InetAddress src, int srcLocalIDType, int srcLocalIDVal, 
							InetAddress dst, int dstLocalIDType, int dstLocalIDVal,
							int vlan, String ingressChannel, String egressChannel) {
		super(68, 253, 1);
		this.srcLocalID = this.generateLocalID(src, 1, srcLocalIDType, srcLocalIDVal);
		this.dstLocalID = this.generateLocalID(dst, 2, dstLocalIDType, dstLocalIDVal);
		this.vlan = this.generateVLAN(vlan);
		this.ingressChannel = this.generateChannel(11, 1, ingressChannel);
		//TODO: Identify why its not type=11,subtype=2
		this.egressChannel = this.generateChannel(2, 0, egressChannel); 
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			if(i >= 4 && i <= 15){
				rawBytes[i] = this.srcLocalID[i-4];
			}else if(i >= 16 && i <= 27){
				rawBytes[i] = this.dstLocalID[i-16];
			}else if(i >= 28 && i <= 35){
				rawBytes[i] = this.vlan[i-28];
			}else if(i >= 36 && i <= 51){
				rawBytes[i] = this.ingressChannel[i-36];
			}else if(i >= 52 && i <= 67){
				rawBytes[i] = this.egressChannel[i-52];
			}
		}
		
		return rawBytes;
	}
	
	private byte[] generateLocalID(InetAddress address, int type, int localIDType, int val){
		byte[] rawAddress = address.getAddress();
		byte[] rawID = ByteUtil.intToFourBytes(localIDType + val);
		byte[] localID = new byte[12];
		
		/* Set length, type, sub-type */
		localID[0] = 0;
		localID[1] = 12;	//length
		localID[2] = (byte)(type & 255); 	//type = source
		localID[3] = 4;		//subtype = local-id
		
		/* Set address */
		for(int i = 0; i < rawAddress.length; i++){
			localID[4 + i] = rawAddress[3-i]; //reverse byte order
		}
		
		/* Set local-id */
		for(int i = 0; i < rawID.length; i++){
			localID[8+i] = rawID[i];
		}
		
		return localID;
	}
	
	private byte[] generateVLAN(int vlan){
		byte[] vlanObject = new byte[8];
		byte[] rawVLAN = ByteUtil.intToFourBytes(vlan);
		
		/* Set length, type, sub-type */
		vlanObject[0] = 0;
		vlanObject[1] = 8;	//length
		vlanObject[2] = 10; 	//type = vlan tag
		vlanObject[3] = 0;		//subtype = none
		
		/* Set vlan tag */
		for(int i = 0; i < rawVLAN.length; i++){
			vlanObject[4 + i] = rawVLAN[i]; 
		}
		
		return vlanObject;
	}
	
	private byte[] generateChannel(int type, int subtype, String name){
		byte[] channelObject = new byte[16];
		byte[] rawName = name.getBytes();
		
		channelObject[0] = 0;
		channelObject[1] = 16;	//length
		channelObject[2] = (byte)(type & 255); //type
		channelObject[3] = (byte)(subtype & 255); //subtype
		for(int i = 4; i < channelObject.length; i++){
			channelObject[i] = ( ((i-4) < rawName.length) ? rawName[i-4] : 0);
		}
		
		return channelObject;
	}


}
