package edu.internet2.hopi.dragon;

import java.net.InetAddress;
import java.util.ArrayList;

import edu.internet2.hopi.dragon.uni.rsvp.header.RSVPGeneralizedLabelRequest;

/**
 * A representation of an LSP managed by DRAGON.
 *
 * @author Andrew Lake (alake@internet2.edu)
 */
public class DragonLSP {
	private String name;
	private InetAddress srcIP;
	private InetAddress dstIP;  
	private DragonLocalID srcLocalID;
	private DragonLocalID dstLocalID;
	private String bandwidth;
	private String encoding;
	private String swcap;
	private String gpid;
	private int srcVtag;
	private int dstVtag;
	private int e2eVtag;
	private String status;
	private ArrayList<String> ero;
	private ArrayList<String> subnetEro;
	
	/* Constants */
	public static String BANDWIDTH_ETHERNET_100M = "eth100M";
	public static String BANDWIDTH_ETHERNET_150M = "eth150M";
	public static String BANDWIDTH_ETHERNET_200M = "eth200M";
	public static String BANDWIDTH_ETHERNET_300M = "eth300M";
	public static String BANDWIDTH_ETHERNET_400M = "eth400M";
	public static String BANDWIDTH_ETHERNET_500M = "eth500M";
	public static String BANDWIDTH_ETHERNET_600M = "eth600M";
	public static String BANDWIDTH_ETHERNET_700M = "eth700M";
	public static String BANDWIDTH_ETHERNET_800M = "eth800M";
	public static String BANDWIDTH_ETHERNET_900M = "eth900M";
	public static String BANDWIDTH_GIGE = "gige";
	public static String BANDWIDTH_GIGE_F = "gige_f";
	public static String BANDWIDTH_HDTV = "hdtv";
	public static String BANDWIDTH_2GIGE = "2gige";
	public static String BANDWIDTH_3GIGE = "3gige";
	public static String BANDWIDTH_4GIGE = "4gige";
	public static String BANDWIDTH_5GIGE = "5gige";
	public static String BANDWIDTH_6GIGE = "6gige";
	public static String BANDWIDTH_7GIGE = "7gige";
	public static String BANDWIDTH_8GIGE = "8gige";
	public static String BANDWIDTH_9GIGE = "9gige";
	public static String BANDWIDTH_10G = "10g";
	public static String BANDWIDTH_OC48 = "oc48";
	
	public static String SWCAP_L2SC = "l2sc";
	public static String SWCAP_PSC1 = "psc1";
	public static String SWCAP_LSC = "lsc";
	public static String SWCAP_TDM = "tdm";
	
	public static String ENCODING_PACKET = "packet";
	public static String ENCODING_ETHERNET = "ethernet";
	public static String ENCODING_LAMBDA = "lambda";
	public static String ENCODING_SDH = "sdh";
	
	public static String GPID_LAMBDA = "lambda";
	public static String GPID_ETHERNET = "ethernet";
	public static String GPID_SDH = "sdh";
	
	public static int VTAG_ANY = -2;
	public static int VTAG_NONE = -3;
	
	public static String STATUS_EDIT = "Edit";
	public static String STATUS_COMMIT = "Commit";
	public static String STATUS_DELETE = "Delete";
	public static String STATUS_INSERVICE = "In service";
	public static String STATUS_LISTENING = "Listening";
	
	public DragonLSP(InetAddress srcIP, DragonLocalID srcLocalID, InetAddress dstIP, DragonLocalID dstLocalID, String bandwidth){
		this(srcIP, srcLocalID, dstIP, dstLocalID, bandwidth, DragonLSP.VTAG_ANY);
	}
	
	public DragonLSP(InetAddress srcIP, DragonLocalID srcLocalID, InetAddress dstIP, DragonLocalID dstLocalID, String bandwidth, int vtag){
		this.srcIP = srcIP;
		this.srcLocalID = srcLocalID;
		this.dstIP = dstIP;
		this.dstLocalID = dstLocalID;
		this.bandwidth = bandwidth;
		this.dstVtag = vtag;
		this.srcVtag = vtag;
		this.encoding = DragonLSP.ENCODING_ETHERNET;
		this.swcap = DragonLSP.SWCAP_L2SC;
		this.gpid = DragonLSP.GPID_ETHERNET;
		this.status = DragonLSP.STATUS_EDIT;
		this.ero = null;
		this.subnetEro = null;
		generateLSPName();
	}
	
	public void setSrcIP(InetAddress srcIP){
		this.srcIP = srcIP;
	}
	
	public InetAddress getSrcIP(){
		return this.srcIP;
	}
	
	public void setDstIP(InetAddress dstIP){
		this.dstIP = dstIP;
	}
	
	public InetAddress getDstIP(){
		return this.dstIP;
	}
	
	public void setSrcLocalID(DragonLocalID srcLocalID){
		this.srcLocalID = srcLocalID;
	}
	
	public DragonLocalID getSrcLocalID(){
		return this.srcLocalID;
	}
	
	public void setDstLocalID(DragonLocalID dstLocalID){
		this.dstLocalID = dstLocalID;
	}
	
	public DragonLocalID getDstLocalID(){
		return this.dstLocalID;
	}
	
	public void setBandwidth(String bandwidth){
		this.bandwidth = bandwidth;
	}
	
	public String getBandwidth(){
		return this.bandwidth;
	}
	
	public void setEncoding(String encoding){
		this.encoding = encoding;
	}
	
	public String getEncoding(){
		return this.encoding;
	}
	
	public void setSWCAP(String swcap){
		this.swcap = swcap;
	}
	
	public String getSWCAP(){
		return this.swcap;
	}
	
	public void setGPID(String gpid){
		this.gpid = gpid;
	}
	
	public String getGPID(){
		return this.gpid;
	}
	
	public void setE2EVtag(int vtag){
		this.e2eVtag = vtag;
	}
	
	public int getE2EVtag(){
		return this.e2eVtag;
	}
	
	public void setLSPName(String name){
		this.name = name;
	}
	
	public String getLSPName(){
		return this.name;
	}
	
	public void setStatus(String status){
		this.status = status;
	}
	
	public String getStatus(){
		return this.status;
	}
	
	public void setEro(ArrayList<String> ero){
		this.ero = ero;
	}
	public ArrayList<String> getEro(){
		return this.ero;
	}
	
	public void setSubnetEro(ArrayList<String> subnetEro){
		this.subnetEro = subnetEro;
	}
	
	public ArrayList<String> getSubnetEro(){
		return this.subnetEro;
	}
	
	public int getEncodingAsGMPLS(){
		int n = 0;
		if(this.encoding.equals(ENCODING_PACKET)){
			n = RSVPGeneralizedLabelRequest.ENCODING_PACKET;
		}else if(this.encoding.equals(ENCODING_ETHERNET)){
			n = RSVPGeneralizedLabelRequest.ENCODING_ETHERNET;
		}else if(this.encoding.equals(ENCODING_LAMBDA)){
			n = RSVPGeneralizedLabelRequest.ENCODING_LAMBDA;
		}else if(this.encoding.equals(ENCODING_SDH)){
			n = RSVPGeneralizedLabelRequest.ENCODING_SDHSONET;
		}
		
		return n;
	}
	
	public int getSWCAPAsGMPLS(){
		int n = 0;
		
		if(this.swcap.equals(SWCAP_L2SC)){
			n = RSVPGeneralizedLabelRequest.SWCAP_L2SC;
		}else if(this.swcap.equals(SWCAP_PSC1)){
			n = RSVPGeneralizedLabelRequest.SWCAP_PSC1;
		}else if(this.swcap.equals(SWCAP_LSC)){
			n = RSVPGeneralizedLabelRequest.SWCAP_LAMBDA;
		}else if(this.swcap.equals(SWCAP_TDM)){
			n = RSVPGeneralizedLabelRequest.SWCAP_TDM;
		}
		
		return n;
	}
	
	public int getGPIDAsGMPLS(){
		int n = 0;
		if(this.gpid.equals(GPID_LAMBDA)){
			n = RSVPGeneralizedLabelRequest.GPID_LAMBDA;
		}else if(this.gpid.equals(GPID_ETHERNET)){
			n = RSVPGeneralizedLabelRequest.GPID_ETHERNET;
		}else if(this.gpid.equals(GPID_SDH)){
			n = RSVPGeneralizedLabelRequest.GPID_SDHSONET;
		}
		
		return n;
	}
	
	public float getBandwidthAsFloat(){
		float bw = 0;
		
		if(this.bandwidth.equals(BANDWIDTH_ETHERNET_100M)){
			bw = 12500000;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_200M)){
			bw = 12500000 * 2;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_300M)){
			bw = 12500000 * 3;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_400M)){
			bw = 12500000 * 4;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_500M)){
			bw = 12500000 * 5;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_600M)){
			bw = 12500000 * 6;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_700M)){
			bw = 12500000 * 7;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_800M)){
			bw = 12500000 * 8;
		}else if(this.bandwidth.equals(BANDWIDTH_ETHERNET_900M)){
			bw = 12500000 * 9;
		}else if(this.bandwidth.equals(BANDWIDTH_GIGE)){
			bw = 125000000;
		}
		
		return bw;
	}
	
	private void generateLSPName(){
		name = srcIP.getHostAddress() + ":" + srcLocalID.getNumber();
		name += "->" + dstIP.getHostAddress() + ":" + dstLocalID.getNumber();
	}

	/**
	 * @return the destVtag
	 */
	public int getDstVtag() {
		return dstVtag;
	}

	/**
	 * @param dstVtag the dstVtag to set
	 */
	public void setDstVtag(int dstVtag) {
		this.dstVtag = dstVtag;
	}

	/**
	 * @return the srcVtag
	 */
	public int getSrcVtag() {
		return srcVtag;
	}

	/**
	 * @param srcVtag the srcVtag to set
	 */
	public void setSrcVtag(int srcVtag) {
		this.srcVtag = srcVtag;
	}
	
	/* TODO: create verify bandwidth function */
}
