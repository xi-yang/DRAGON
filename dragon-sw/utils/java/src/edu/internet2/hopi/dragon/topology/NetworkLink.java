package edu.internet2.hopi.dragon.topology;

import java.util.HashMap;

public class NetworkLink {
	private HashMap<String, String> links;
	
	public NetworkLink(){
		this.links = new HashMap<String, String>();
	}
	
	public void addLink(String localIF, String remoteIF){
		this.links.put(localIF, remoteIF);
	}
	
	public boolean hasLocalIP(String localIP){
		return this.links.containsKey(localIP);
	}
	
	public HashMap<String, String> getLinks(){
		return this.links;
	}
}
