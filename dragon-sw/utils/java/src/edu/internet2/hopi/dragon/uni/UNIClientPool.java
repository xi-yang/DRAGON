package edu.internet2.hopi.dragon.uni;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.UnknownHostException;
import java.util.HashMap;

import org.savarese.rocksaw.net.RawSocket;

public class UNIClientPool {
	private RawSocket sock;
	private HashMap<String, UNIClient> clients;
	
	public UNIClientPool(){
		this.sock = new RawSocket();
		this.clients = new HashMap<String, UNIClient>();
	}
	
	public boolean addNewClient(String localAddress, String remoteAddress) throws UnknownHostException{
		boolean isNew;
		if(this.clients.containsKey(localAddress)){
			isNew = false;
		}else{
			UNIClient client = new UNIClient(localAddress, remoteAddress, sock);
			this.clients.put(localAddress, client);
			isNew = true;
		}
		
		return isNew;
	}
	
	public boolean removeClient(String localAddress) throws InterruptedIOException, IOException{
		/* Retrieve client */
		UNIClient client = this.clients.get(localAddress);
		if(client == null){
			return false;
		}
		
		/* End all currently running sessions and delete */
		client.endAllSessions();
		this.clients.remove(localAddress);
		
		return true;
	}
	
	public UNIClient getClient(String localAddress){
		return this.clients.get(localAddress);
	}
}
