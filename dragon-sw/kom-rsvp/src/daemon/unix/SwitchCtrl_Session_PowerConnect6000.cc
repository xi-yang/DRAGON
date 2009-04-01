/****************************************************************************

Dell (vendor) PowerConnect 6024/6224/6248 (model) Control Module source file SwitchCtrl_Session_PowerConnect6000.cc
Created by Xi Yang 04/01/2009
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_PowerConnect6000.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_PowerConnect6000_CLI::preAction()
{
    if (!active || vendor<PowerConnect6024&&vendor>PowerConnect6024 || !pipeAlive())
        return false;
    DIE_IF_NEGATIVE(writeShell( "configure\n", 5));
    DIE_IF_NEGATIVE(readShell( SWITCH_PROMPT, NULL, 1, 10));
    return true;
}

bool SwitchCtrl_Session_PowerConnect6000_CLI::postAction()
{
    if (fdout < 0 || fdin < 0)
        return false;
    DIE_IF_NEGATIVE(writeShell("end\n", 5));
    return true;
}

//committed_rate mbps--> kbps, burst_size kbyes-> bytes
bool SwitchCtrl_Session_PowerConnect6000_CLI::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    int n;
    char vlanNum[8], action[32], vlanClassMap[32], vlanPolicyMap[32];
    int committed_rate_int = (int)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    sprintf(vlanNum, "%d", vlan_id);
    sprintf(vlanClassMap, "class-map-vlan-%d", vlan_id);
    sprintf(vlanPolicyMap, "policy-map-vlan-%d", vlan_id);
    if (do_undo)
    {
        //create vlan-level class-map
        bool hasClassMap = false;
        DIE_IF_NEGATIVE(n= writeShell( "show class-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, "Match access-group dragon-default", true, 1, 10)) ;
        if (n == 2)
        {
            hasClassMap = true;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        }
        if (!hasClassMap)
        {
            DIE_IF_NEGATIVE(n= writeShell( "class-map ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
            DIE_IF_NEGATIVE(n= writeShell( "match access-group dragon-default\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
            DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        }
        // configure vlan-level policy-map
        committed_rate_int *= 1000;
        if (SWITCH_VENDOR_MODEL == PowerConnect6024)
        {
            burst_size *= 1000; // in bytes
            if (burst_size < 3000) burst_size = 3000;
            sprintf(action, "police %d %d exceed-action drop", committed_rate_int, burst_size); // no excess or peak burst size setting
        }
        else if (SWITCH_VENDOR_MODEL == PowerConnect6224 ||SWITCH_VENDOR_MODEL == PowerConnect6248)
        {
            if (burst_size < 32) burst_size = 32; //in Kbytes
            sprintf(action, "police-siimple %d %d conform-action transmit violate-action drop", committed_rate_int, burst_size); // no excess or peak burst size setting
        }
        else
            return (false&&postAction());
        DIE_IF_NEGATIVE(n= writeShell( "policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( "class ", 5)) ;
        if (SWITCH_VENDOR_MODEL == PowerConnect6224 ||SWITCH_VENDOR_MODEL == PowerConnect6248)
            DIE_IF_NEGATIVE(n= writeShell( "match-all ", 5)); //madatory option
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // enter interface vlan configuration mode 
        DIE_IF_NEGATIVE(n= writeShell( "end\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "configure\n\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "interface vlan ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanNum, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // apply vlan-level policy map
        DIE_IF_NEGATIVE(n= writeShell( "no shutdown\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "service-policy input ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }
    else
    {
        // remove vlan-level policy map
        DIE_IF_NEGATIVE(n= writeShell( "no policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanPolicyMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // remove vlan-level class-map
        DIE_IF_NEGATIVE(n= writeShell( "no class-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanClassMap, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", DELL_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
    }

    // end
    if (!postAction())
        return false;
    return true;
}

bool SwitchCtrl_Session_PowerConnect6000_CLI::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (!postAction())
        return false;
    return true;
}

bool SwitchCtrl_Session_PowerConnect6000_CLI::postConnectSwitch()
{
    int n;
    bool missingDefaultACL = false;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( ">", "#", true, 1, 10)) ;
    if (n == 1)
    {
        DIE_IF_NEGATIVE(n= writeShell( "enable\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "Password:", NULL, 0, 10)) ;
        if (strcmp(CLI_PASSWORD, "unknown") != 0)
            DIE_IF_NEGATIVE(n= writeShell( CLI_PASSWORD, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    }
    if (SWITCH_VENDOR_MODEL == PowerConnect6024)
    {
        DIE_IF_NEGATIVE(n= writeShell( "show access-lists dragon-default\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "permit  any any any", "does not exist", true, 1, 10)) ;
        if (n == 2)
            missingDefaultACL = true;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10));
        //configure default QoS mode
        DIE_IF_NEGATIVE(n= writeShell( "configure\n", 5));
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10));
        DIE_IF_NEGATIVE(n= writeShell( "qos advanced\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        if (missingDefaultACL)
        {
            //create default ACL
            DIE_IF_NEGATIVE(n= writeShell( "ip access-list dragon-default\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
            DIE_IF_NEGATIVE(n= writeShell( "permit any any any\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
            DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        }
    }
    else if (SWITCH_VENDOR_MODEL == PowerConnect6224 ||SWITCH_VENDOR_MODEL == PowerConnect6248)
    {
        DIE_IF_NEGATIVE(n= writeShell( "show ip access-lists dragon-default\n", 5)) ;
        //$TODO: find out output patterns and following-up commands when we have access to a real 62xx switch
    }
    else
        return false;
    
    DIE_IF_NEGATIVE(n= writeShell( "end\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;

    return true;
}

////////////////////////////////////////////////

bool SwitchCtrl_Session_PowerConnect6000::connectSwitch()
{
    if (SwitchCtrl_Session::connectSwitch() == false)
        return false;

    if (CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH)
    {
        cliSession.vendor = this->vendor;
        cliSession.active = true;
        LOG(2)( Log::MPLS, "VLSR: CLI connecting to PowerConnect6000 Switch: ", switchInetAddr);
        if (!cliSession.engage(NULL)) //CLI login password-only (w/ prompt for username)
            return false;
    }

    return cliSession.postConnectSwitch();
}

void SwitchCtrl_Session_PowerConnect6000::disconnectSwitch()
{
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        char logout[50];
        sprintf (logout, "end\nexit\n"); //logout without saving 'n' 
        LOG(2)( Log::MPLS, "VLSR: CLI disconnecting from PowerConnect6000 Switch: ", switchInetAddr);
        cliSession.disengage(logout);
        cliSession.active = false;
    }
}

bool SwitchCtrl_Session_PowerConnect6000::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH)
    {
        return cliSession.policeInputBandwidth(do_undo, input_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size); 
    }
    return false;
}

bool SwitchCtrl_Session_PowerConnect6000::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH)
    {
        return cliSession.limitOutputBandwidth(do_undo, output_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size);
    }
    return false;
}



