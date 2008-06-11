package edu.internet2.hopi.dragon.narb.api;

/**
 * Abstract class representing an ERO subobject.
 * 
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public abstract class EROSubobject {
	protected boolean loose;
	protected byte type;
	protected byte tlvLength;
	
	/* Subobject types */
	public final static byte IPV4_PREFIX_SUBOBJ = 1;
	public final static byte IPV6_PREFIX_SUBOBJ = 2;
	public final static byte AS_SUBOBJ = 32;
	public final static byte UNUM_IF_SUBOBJ = 4; //unnumbered interface
	
	/**
	 * Returns whether this hop is loose or strict
	 * @return true if loose, false if strict
	 */
	public boolean isLoose() {
		return loose;
	}

	/**
	 * Set whether this hop is loos or strict
	 * @param loose true if loose, false if strict
	 */
	public void setLoose(boolean loose) {
		this.loose = loose;
	}

	/**
	 * Returns the subobject type
	 * @return the subobject type
	 */
	public byte getType() {
		return type;
	}

	/**
	 * Sets the subobject type
	 * @param type the subobject type to set
	 */
	public void setType(byte type) {
		this.type = type;
	}
	
	/**
	 * Returns the length of this subobject including the type and length
	 * @return the length of this subobject including the type and length
	 */
	public byte getTLVLength() {
		return tlvLength;
	}

	/**
	 * Sets the length of this subobject including the type and length
	 * @param tlvLength the length of this subobject (including the type and length) to set
	 */
	public void setTLVLength(byte tlvLength) {
		this.tlvLength = tlvLength;
	}
	
	/**
	 * Creates the start of an ERO subobject in raw byte form
	 * @return raw byte representation of loose-type-length fields and empty value
	 */
	public byte[] toBytes(){
		byte[] rawBytes = new byte[tlvLength];
		for(int i = 0; i < 2; i++){
			if(i == 0){
				if(this.loose){
					rawBytes[0] = (byte) 128;
				}
				rawBytes[0] += type;
			}else if(i == 1){
				rawBytes[1] = this.tlvLength;
			}
		}
		return rawBytes;
	}
}
