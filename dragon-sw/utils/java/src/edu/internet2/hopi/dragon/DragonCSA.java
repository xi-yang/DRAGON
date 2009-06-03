package edu.internet2.hopi.dragon;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.StringTokenizer;
import java.util.regex.Pattern;
import java.net.InetAddress;

import org.apache.commons.net.telnet.TelnetClient;
import org.apache.log4j.*;

/**
 * Issue basic DRAGON CSA CLI commands via telnet. Can be used to setup, teardown, and poll information about LSPs.
 *
 * @author Andrew Lake (alake@internet2.edu)
 */
public class DragonCSA {
    protected TelnetClient tc;
    protected BufferedReader in;
    protected PrintStream out;
    protected String errorMsg;
    protected String promptPattern;
    protected String passwordPromptPattern;
    private boolean uniMode;
    private Logger log;
    /**
     * Constructor that creates telnet client and sets default values (UNI is set to false)
     *
     */
    public DragonCSA(){
        this(false);
    }

    /**
     * Constructor that creates telnet client and set UNI mode to specified value
     *
     */
    public DragonCSA(boolean uni){
        tc = new TelnetClient();
        in = null;
        out = null;
        uniMode = uni;
        promptPattern = "dragon.*[>#] ";
        passwordPromptPattern = "Password: ";
        log = null;
    }

    /**
     * Logs user into DRAGON CSA telnet
     * @param host hostname of machine running DRAGON CSA telnet server
     * @param port port that on which DRAGON telnet server is running
     * @param password password for CSA
     * @return boolean value that is true if login succeeds and false if it fails
     */
    public boolean login(String host, int port, String password){
        try {
            tc.connect(host, port);
            in = new BufferedReader(new InputStreamReader(tc.getInputStream()));
            out = new PrintStream(tc.getOutputStream());
            this.readUntil(passwordPromptPattern);
            out.println(password);
            out.flush();
            this.readUntil(promptPattern);
        } catch (SocketException e) {
            errorMsg = "Login Error: Unable to establish connection to " + host;
            return false;
        } catch (IOException e) {
            errorMsg = "Login Error: Unable to retrieve telnet I/O streams";
            return false;
        }

        return true;
    }

    /**
     * Issues commands to DRAGON CSA CLI
     * @param cmd command to run on DRAGON CSA CLI
     * @return output of command
     */
    public String command(String cmd){
        String result = "";

        out.println(cmd);
        out.flush();
        this.readUntil(promptPattern);//hopefully can remove this in the future
        result = this.readUntil(promptPattern);

        /* Remove command from output */
        result = result.replaceFirst(cmd + "\n", "");
        /* Remove prompt from output */
        result = result.replaceFirst(promptPattern + "\n*", "");

        return result;
    }

    /**
     * Reads output from telnet until the specified pattern is reached. promptPattern 
     * is often useful when used in conjunction with this method.
     * @param pattern Regular expression used to determine when output should stop being read
     * @return output read until pattern is found
     */
    public String readUntil(String pattern){
        String line = "";
        String allOutput = "";
        char c;
        Pattern regex = Pattern.compile(pattern);

        try {
            while((c = (char) in.read()) != -1){
                allOutput += c;
                if(c == '\n'){
                    if(log != null){ log.info(line); }
                    line = "";
                }else{
                    line += c;
                    if(regex.matcher(line).matches()){
                        if(log != null){ log.info(line); }
                        break;
                    }
                }
            }
        } catch (IOException e) {
            errorMsg = "I/O Error: Error reading telnet output";
        }

        return allOutput;
    }

    /**
     * Disconnects telnet session. Should be called when telnet session is complete.
     *
     */
    public void disconnect(){
        try {
            tc.disconnect();
        } catch (IOException e) {
            errorMsg = "I/O Error: Error disconnecting from host";
        }
    }

    /**
     * Creates and commits LSP with the parameters of the LSP object passed to it. 
     * LSP is created in UNI mode with implicit ingress and egress channels.
     * @param lsp LSP object with parameters of the LSP to be setup
     * @return boolean that is true if setup succeeds and false if fails
     */
    public boolean setupLSP(DragonLSP lsp){
        if(!lspExists(lsp.getLSPName())){
            String cmdResult = "";
            ArrayList<String> ero = lsp.getEro();
            ArrayList<String> subnetEro = lsp.getSubnetEro();

            /* Enter edit mode */
            command("edit lsp " + lsp.getLSPName());

            /* Set UNI mode */
            if(uniMode){
                command("set uni client ingress implicit egress implicit");
            }

            /* Set source and destination */
            cmdResult = command("set source ip-address " + lsp.getSrcIP().getHostAddress() + " " + 
                    lsp.getSrcLocalID().getType() + " " + lsp.getSrcLocalID().getNumber() + 
                    " destination ip-address " + lsp.getDstIP().getHostAddress() + " " +
                    lsp.getDstLocalID().getType() + " " + lsp.getDstLocalID().getNumber());

            /* Set bandwidth and capabilities */
            cmdResult += command("set bandwidth " + lsp.getBandwidth() + " swcap " + lsp.getSWCAP() + 
                    " encoding " + lsp.getEncoding() + " gpid " + lsp.getGPID());

            /* Set ERO */
            int srcVtag = lsp.getSrcVtag();
            int destVtag = lsp.getDstVtag();
            int e2eVtag = lsp.getE2EVtag();
            int id = (0x4<<16);
            String unumInterfaceId = "";
            String ipv4Hop = "";
            if(e2eVtag > 0){
                id += e2eVtag;
            }else if(srcVtag > 0){
                id += srcVtag;
            }else if(destVtag > 0){
                id += destVtag;
            }
            if(!lsp.getSrcLocalID().getType().equals(DragonLocalID.SUBNET_INTERFACE)){
                unumInterfaceId = " interface-id " + id;
            }else{
                ipv4Hop = "-ipv4";
            }
            if(ero != null){
                for(String hop : ero){
                    cmdResult += command("set ero-hop"+ ipv4Hop + 
                            " strict ip-address " + hop + unumInterfaceId);
                }
            }

            /* Set Subnet ERO */
            if(subnetEro != null){
                for(String hop : subnetEro){
                    cmdResult += command("set subnet-ero-hop-ipv4 ip-address " + hop);
                }
            }

            /* Set VLAN tag */
            String srcLocalIdType = lsp.getSrcLocalID().getType();
            String destLocalIdType = lsp.getDstLocalID().getType();
            if(DragonLocalID.SUBNET_INTERFACE.equals(srcLocalIdType) && 
                    DragonLocalID.SUBNET_INTERFACE.equals(destLocalIdType)){
                cmdResult += command("set vtag subnet-ingress " + vtagToString(srcVtag));
                cmdResult += command("set vtag subnet-egress " + vtagToString(destVtag));
            }else if(e2eVtag > 0){
                cmdResult += command("set vtag " + e2eVtag);
            }else if(srcVtag > 0){
                cmdResult += command("set vtag " + srcVtag);
            }else if(destVtag > 0){
                cmdResult += command("set vtag " + destVtag);
            }else if(srcVtag == DragonLSP.VTAG_ANY){
                cmdResult += command("set vtag any");
            }else if(destVtag == DragonLSP.VTAG_ANY){
                cmdResult += command("set vtag any");
            }

            /* Verify parameters set and commit lsp */
            if(cmdResult.equals("")){
                command("exit");
                cmdResult = command("commit lsp " + lsp.getLSPName());

                if(!cmdResult.equals("")){
                    errorMsg = "LSP commit error: " + cmdResult;
                    return false;
                }
            }else{
                errorMsg = "LSP Setup error: " + cmdResult;
                return false;
            }
        }else{
            errorMsg = "LSP Setup error: LSP between source and destination already exists";
            return false;
        }

        return true;
    }

    private String vtagToString(int vtag){
        String result = "";
        if(vtag == -1){
            result = "tunnel-mode";
        }else if(vtag == 0){
            result = "untagged";
        }else{
            result = vtag + "";
        }

        return result;
    }

    /**
     * Removes LSP with the given LSP name 
     * @param lspName Name of LSP to be deleted
     * @return Returns true if LSP teardown succeeds and false if it fails. 
     * Most likely failure occurs when LSP does not exist.
     */
    public boolean teardownLSP(String lspName){
        String cmdResult = "";
        boolean success = true;

        /* Make delete command */
        cmdResult = command("delete lsp " + lspName);

        /* Check for errors */
        if(!cmdResult.equals("")){
            errorMsg = "LSP teardown error: " + cmdResult;
            success = false;
        }

        return success;
    }
    /**
     * Checks whether an LSP with a given name exists. Does so by running the 'show lsp lspName' command
     * @param lspName Name of LSP whose existence needs to be verified.
     * @return truw if lsp exists, false if it does not exists
     */
    public boolean lspExists(String lspName){
        boolean exists = true;
        String cmdResult = "";

        cmdResult = command("show lsp " + lspName);
        if(cmdResult.contains("No matching LSP")){
            errorMsg = cmdResult;
            exists = false;
        }

        return exists;
    }

    /**
     * Maps already existing LSP to an LSP object. LSP is 
     * specified by LSP name. Uses 'show lsp lspName' CLI command to retrieve paramters.
     * @param lspName name of LSP to map to object
     * @return returns LSP object with parameters of the specified LSP. null if LSP fails to be retrieved (i.e. LSP does not exist)
     */
    public DragonLSP getLSPByName(String lspName){
        DragonLSP lsp = null;
        String rawLSPData = command("show lsp " + lspName);

        /* Determine if lsp exists and parse */
        if(!rawLSPData.contains("No matching LSP")){
            StringTokenizer st = new StringTokenizer(rawLSPData);

            /* Define LSP params */
            InetAddress srcIP = null, dstIP = null;
            DragonLocalID srcLocalID = null, dstLocalID = null;
            String bandwidth = null, encoding = null, swcap = null, gpid = null, status = null;
            int vtag = 0;

            /* Parse output */
            while(st.hasMoreTokens()){
                String token = st.nextToken();
                if(token.matches("Src")){ /* Get source IP address */
                    String strIP = st.nextToken();
                    int end = strIP.lastIndexOf("/");
                    try {
                        srcIP = InetAddress.getByName(strIP.substring(0, end));
                    } catch (UnknownHostException e) {
                        errorMsg = "Unknown host exception";
                        return null;
                    }
                }else if(token.matches("dest")){ /* Get destination IP address */
                    String strIP = st.nextToken();
                    int end = strIP.lastIndexOf("/");
                    try {
                        dstIP = InetAddress.getByName(strIP.substring(0, end));
                    } catch (UnknownHostException e) {
                        errorMsg = "Unknown host exception";
                        return null;
                    }
                }else if(token.matches("B=.*")){ /* Get bandwidth */
                    int es = token.lastIndexOf('=');
                    bandwidth = token.substring(es+1, token.length() - 1);
                }else if(token.matches("Encoding")){ /*Get encoding */
                    encoding = st.nextToken();
                    encoding = encoding.substring(0, encoding.length() - 1);
                }else if(token.matches("Switching")){ /*Get swcap */
                    swcap = st.nextToken();
                    swcap = swcap.substring(0, swcap.length() -1);
                }else if(token.matches("G-Pid")){ /*Get gpid */
                    gpid = st.nextToken();
                }else if(token.matches("Subnet")){
                    st.nextToken(); //discard stray Ingress/Egress token
                }else if(token.matches("Ingress")){ /*Get source local ID */
                    /* Clear unimportant fields */
                    for(int i = 0; i < 3; i++){st.nextToken();}

                    /* Get Type */
                    String type = st.nextToken();
                    if(type.equals("single")){
                        st.nextToken();
                        st.nextToken();
                        srcLocalID = new DragonLocalID(Integer.parseInt(st.nextToken()), DragonLocalID.UNTAGGED_PORT);
                    }
                    //TODO: add other types
                }else if(token.matches("Egress")){ /*Get destination local ID */
                    /* Clear unimportant fields */
                    for(int i = 0; i < 3; i++){st.nextToken();}

                    /* Get Type */
                    String type = st.nextToken();
                    if(type.equals("single")){
                        st.nextToken();
                        st.nextToken();
                        String num = st.nextToken();
                        num = num.substring(0, num.length() - 1);
                        dstLocalID = new DragonLocalID(Integer.parseInt(num), DragonLocalID.UNTAGGED_PORT);
                    }
                    //TODO: add other types

                }else if(token.matches("Tag:")){ /*Get VLAN tag number */
                    String strVtag = st.nextToken();
                    strVtag = strVtag.substring(0, strVtag.length() - 1);
                    vtag = Integer.parseInt(strVtag);
                }else if(token.matches("Status:")){ /*Get status of LSP */
                    status = st.nextToken();
                    if(status.equals("In")){
                        status += " " + st.nextToken();
                    }
                }
            }

            /* Verify source local ID set */
            if(srcLocalID == null){
                srcLocalID = new DragonLocalID(0, DragonLocalID.UNTAGGED_PORT);
            }

            /* Verify destination local ID set */
            if(dstLocalID == null){
                dstLocalID = new DragonLocalID(0, DragonLocalID.UNTAGGED_PORT);
            }

            /* Create LSP */
            lsp = new DragonLSP(srcIP, srcLocalID, dstIP, dstLocalID, bandwidth, vtag);

            /* Set other LSP parameters */
            lsp.setEncoding(encoding);
            lsp.setSWCAP(swcap);
            lsp.setGPID(gpid);
            lsp.setLSPName(lspName);
            lsp.setStatus(status);
        }else{
            errorMsg = "Can't Get LSP: " + rawLSPData;;
        }

        return lsp;
    }

    /**
     * Maps all LSPs a CSA currently participates in and stores them in a list. LSPs of all statuses are included in the list.
     * @return list of lsps currently enetered into DRAGON 
     */
    public ArrayList<DragonLSP> getAllLSPs(){
        ArrayList<DragonLSP> lsps = new ArrayList<DragonLSP>();
        ArrayList<String> lspNameList = listLSPs();

        for(int i = 0; i < lspNameList.size(); i++){
            String lspName = lspNameList.get(i);
            DragonLSP lsp = getLSPByName(lspName);
            if(lsp != null)
                lsps.add(lsp);
        }

        return lsps;
    }

    /**
     * Returns a list of the names of all the LSPs in which a DRAGON CSA participates
     * @return A list of the names of all the LSPs in which a CSA currently participates
     */
    public ArrayList<String> listLSPs(){
        ArrayList<String> nameList = new ArrayList<String>();

        /* Retrieve List */
        String rawLSPList = command("show lsp");
        StringTokenizer st = new StringTokenizer(rawLSPList);

        /* remove first three lines */
        for(int i = 0; i < 3; i++){
            st.nextToken("\n");
        }

        /* get LSP Name */
        while(st.hasMoreTokens()){
            /* Add name to list */
            nameList.add(st.nextToken("\n \t"));

            /* Remove trailing tokens */
            for(int i = 0; st.hasMoreTokens() && i < 6; i++){
                st.nextToken("\n \t");
            }
        }
        return nameList;
    }

    /**
     * Checks if a given local-id already exists on a VLSR
     * 
     * @param localId the local-id to be checked
     * @return true if local-id exists, false otherwise
     */
    public boolean localIdExists(DragonLocalID localId){
        String value = localId.getNumber() + "";
        String type = localId.getType();

        String rawList = this.command("show local-id");
        if(rawList != null && this.getError() == null){
            String[] lines = rawList.split("\n");
            for(int i = 0; i < lines.length; i++){
                StringTokenizer st = new StringTokenizer(lines[i], " ");
                if(!(st.hasMoreTokens() && st.nextToken().matches(value + "(\\(.*\\))?"))){
                    continue;
                }

                while(st.hasMoreTokens()){
                    String token = st.nextToken();
                    if((token.equals("[tagged") && type.equals(DragonLocalID.TAGGED_PORT_GROUP)) ||
                            (token.equals("[single") && type.equals(DragonLocalID.UNTAGGED_PORT))){
                        return true;
                    }
                }
            }
        }else{
            this.errorMsg = "Unable to retrieve list of local-ids";
        }
        return false;
    }

    /**
     * Creates a give local-id
     * 
     * @param localId the local-id to create
     * @param port if this is a tagged-goup the port is the port to add to the group. ignored otherwise.
     * @return true if no errors occurred, false otherwise
     */
    public boolean createLocalId(DragonLocalID localId, int port){
        String value = localId.getNumber() + "";
        String type = localId.getType();
        String result = null;

        if(type.equals(DragonLocalID.UNTAGGED_PORT) || type.equals(DragonLocalID.SUBNET_INTERFACE)){
            if(this.localIdExists(localId)){
                this.errorMsg = "Local ID already exists";
                return false;
            }

            result = this.command("set local-id " + type + " " + value);
        }else if(type.equals(DragonLocalID.TAGGED_PORT_GROUP)){
            result = this.command("set local-id " + type + " " + value + " add " + port);
        }

        if(!result.equals("")){
            this.errorMsg = result;
            return false;
        }

        return true;
    }

    /**
     * Deleted a given local ID.
     * 
     * @param localId the local-id to delete
     * @return true if no errors occurred, false otherwise
     */
    public boolean deleteLocalId(DragonLocalID localId){
        String value = localId.getNumber() + "";
        String type = localId.getType();
        String result = null;

        if(!this.localIdExists(localId)){
            return false;
        }

        result = this.command("delete local-id " + type + " " + value);
        if(!result.equals("")){
            this.errorMsg = result;
            return false;
        }

        return true;
    }

    /**
     * Get error message reported by any of the methods in this class. Error message may 
     * also contain errors returned by DRAGON CSA CLI.
     * @return Last error message reported by a method call
     */
    public String getError(){
        return errorMsg;
    }

    /**
     * Sets pattern used to identify the CLI command prompt. This is used to determine when a command is complete.
     * @param pattern Regular expression pattern used to identify command prompt
     */
    public void setPromptPattern(String pattern){
        promptPattern = pattern;
    }

    /**
     * Returns pattern used to identify command prompt
     * @return Pattern used to identify command prompt
     */
    public String getPromptPattern(){
        return promptPattern;
    }

    /**
     * Sets pattern used to identify the password prompt on login. This is used to determine when a command is complete.
     * @param pattern Regular expression pattern used to identify password prompt on login
     */
    public void setPasswordPromptPattern(String pattern){
        passwordPromptPattern = pattern;
    }

    /**
     * Returns pattern used to identify password prompt on login
     * @return Pattern used to identify password prompt on login
     */
    public String getPasswordPromptPattern(){
        return passwordPromptPattern;
    }

    /**
     * Sets UNI mode
     * @param uni boolean value that is true if you would like to run the CSA in UNI mode
     */
    public void setUNIMode(boolean uni){
        uniMode = uni;
    }

    /**
     * Returns whether CSA is set to run in UNI mode
     * @return boolean that is true if UNI mode will be used
     */
    public boolean getUNIMode(){
        return uniMode;
    }

    /**
     * Sets logger
     * 
     * @param logType log type to use
     */
    public void setLogger(String logType){
        this.log = Logger.getLogger(logType);
    }

    public String flush(){
        String buffer = "";
        try {
            if(in.ready()){
                buffer += readUntil(promptPattern);
                /* Remove prompt from output */
                buffer = buffer.replaceFirst(promptPattern + "\n*", "");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        return buffer;
    }
}
