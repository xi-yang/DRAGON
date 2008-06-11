package edu.internet2.hopi.dragon.uni.rsvp.header;

import java.util.List;

import edu.internet2.hopi.dragon.narb.api.EROSubobject;

public class RSVPExplicitRoute extends RSVPObject{
	byte[] subobjects;
	
	public RSVPExplicitRoute(List<EROSubobject> subobjects) {
		super(0, 20, 1);
		int lengthSum = 0;
		
		/* sum length */
		for(EROSubobject subobject : subobjects){
			lengthSum += subobject.getTLVLength();
		}
		this.setLength(lengthSum + 4);
		
		/* Add subobjects */
		this.subobjects = new byte[lengthSum];
		int i = 0;
		for(EROSubobject subobject : subobjects){
			byte[] rawSubobject = subobject.toBytes();
			for(int j = 0; j < rawSubobject.length; j++){
				this.subobjects[i] = rawSubobject[j];
				i++;
			}
		}
	}
	
	public byte[] toBytes(){
		byte[] rawBytes = super.toBytes();
		for(int i = 4; i < rawBytes.length; i++){
			rawBytes[i] = this.subobjects[i-4];
		}
		
		return rawBytes;
	}

}
