package edu.internet2.hopi.dragon.narb.api;

import java.net.InetAddress;

/**
 * Represents an IPv4 prefix subobject as specified in RFC 3209 section 4.3.3.
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class IPv4PrefixSubobject extends EROSubobject{
	private InetAddress ip;
	private byte ipv4Length;
	private byte padding;
	
	/**
	 * Create new IPv4Prefix subobject
	 * @param loose true if hop is loos, false if hop is strict
	 * @param tlvLength length of this subobject including length and type fields
	 * @param ip IPv4 address in this subobject
	 * @param ipv4Length length of the IPv4 address
	 * @param padding padding bits usually set to 0
	 */
	public IPv4PrefixSubobject(boolean loose, byte tlvLength, InetAddress ip, byte ipv4Length, byte padding){
		this.loose = loose;
		this.type = IPV4_PREFIX_SUBOBJ;
		this.tlvLength = tlvLength;
		this.ip = ip;
		this.ipv4Length = ipv4Length;
		this.padding = padding;
	}
	
	/**
	 * Returns the IP address in this subobject
	 * @return the IP address in this subobject
	 */
	public InetAddress getIp() {
		return ip;
	}

	/**
	 * Sets the IP address of this subobject
	 * @param ip the IP address to set
	 */
	public void setIp(InetAddress ip) {
		this.ip = ip;
	}

	/**
	 * Returns the length of the IPv4 address (should always be 4 bytes)
	 * @return the length of the IPv4 address (should always be 4 bytes)
	 */
	public byte getIpv4Length() {
		return ipv4Length;
	}

	/**
	 * Sets the length of the IPv4 address (should always be 4 bytes)
	 * @param ipv4Length the length of the IPv4 address (should always be 4 bytes)
	 */
	public void setIpv4Length(byte ipv4Length) {
		this.ipv4Length = ipv4Length;
	}

	/**
	 * Returns the padding byte
	 * @return the padding byte
	 */
	public byte getPadding() {
		return padding;
	}

	/**
	 * Sets the padding byte
	 * @param padding the padding byte to set
	 */
	public void setPadding(byte padding) {
		this.padding = padding;
	}
}
