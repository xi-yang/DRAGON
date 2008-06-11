package edu.internet2.hopi.dragon;

public class DragonRCECLI extends DragonCSA{
	public DragonRCECLI(){
		super();
		this.promptPattern = "rce:cli.*[>#]";
		this.passwordPromptPattern = "password:";
	}
	
	public String getIntradomainToplogy(){
		String topology = this.command("show topology intradomain");
		String flush = this.flush();
		
		if(!flush.equals("")){
			topology = flush;
		}
		
		return topology;
	}
	
	public String getInterdomainToplogy(){
		String topology = this.command("show topology interdomain");
		String flush = this.flush();
		
		if(!flush.equals("")){
			topology = flush;
		}
		
		return topology;
	}
	
	
}
