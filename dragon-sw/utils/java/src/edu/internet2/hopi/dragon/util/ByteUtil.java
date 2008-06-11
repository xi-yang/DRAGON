package edu.internet2.hopi.dragon.util;

public class ByteUtil {
	public static byte[] intToTwoBytes(int n){
		byte[] bytes = new byte[2];
		bytes[0] = (byte)((n & 65280) >> 8);
		bytes[1] = (byte)(n & 255);
		
		return bytes;
	}
	
	public static byte[] intToThreeBytes(int n){
		byte[] bytes = new byte[3];
		bytes[0] = (byte) ((n & 16711680) >> 16);
		bytes[1] = (byte) ((n & 65280) >> 8);
		bytes[2] = (byte) (n & 255);
		
		return bytes;
	}
	
	public static byte[] intToFourBytes(int n){
		byte[] bytes = new byte[4];
		bytes[0] = (byte) (n >> 24);
		bytes[1] = (byte) ((n & 16711680) >> 16);
		bytes[2] = (byte) ((n & 65280) >> 8);
		bytes[3] = (byte) (n & 255);
		
		return bytes;
	}
	
	public static byte[] floatToBytes(float f){
		byte[] bytes = new byte[4];
		bytes[0] = (byte) (Float.floatToRawIntBits(f) >> 24);
		bytes[1] = (byte) ((Float.floatToRawIntBits(f) & 16711680) >> 16);
		bytes[2] = (byte) ((Float.floatToRawIntBits(f) & 65280) >> 8);
		bytes[3] = (byte) (Float.floatToRawIntBits(f) & 255);
		
		return bytes;
	}
	
	public static int intFromTwoBytes(byte[] bytes){
		int n = ((bytes[0] << 8) & 65535);
		n += (bytes[1] & 255);
		
		return n;
	}
	
	public static int intFromFourBytes(byte[] bytes){
		int n = ((bytes[0] << 24) & 0xFF000000);
		n = ((bytes[1] << 16) & 0xFF0000);
		n = ((bytes[2] << 8) & 65535);
		n += (bytes[3] & 255);
		
		return n;
	}
	
	public static String hexStringFromBytes(byte[] buf){
		String str = "";
		for(int i = 0; i < buf.length; i++){
			String hex = Integer.toHexString(buf[i]);
			if(hex.length() == 1){
				hex = ("0" + hex);
			}else if(hex.length() > 2){
				hex = hex.substring(6);
			}
			
			str += (hex + " ");
			if(((i+1) % 8) == 0){
				str += "\n";
			}
		}
		
		return str;
	}
	
	public static byte[] reverse(byte[] origBytes){
		int n = origBytes.length;
		byte[] newBytes = new byte[n];
		
		for(int i = 0; i < n; i++){
			newBytes[n-i-1] = origBytes[i];
		}
		
		return newBytes;
	}
}
