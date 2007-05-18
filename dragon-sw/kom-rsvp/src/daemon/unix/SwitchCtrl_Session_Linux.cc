/****************************************************************************

CLI Based Linux Switch Control Module source file SwitchCtrl_Session_Linux.cc
Created by Lars Persson Fink @ 11/21/2006

A VLAN is represented by a ethernet bridge with name=VID created with brctl.

Adding an interface to a VLAN is represented by adding a VLAN to the interface
and adding the VLAN interface, ie eth0.VID, to the ethernet bridge VID.

For this class to work the user CLI_USERNAME _must_ be able to run ifconfig, 
brctl & vconfig through sudo without password.
****************************************************************************/

#ifdef Linux

#include "SwitchCtrl_Session_Linux.h"
#include <string>
#include <iterator>
using namespace std;

#define BR_PREFIX "br"

bool SwitchCtrl_Session_Linux::connectSwitch(const char *loginString) 
{
  if(!engage(loginString)) {
    return false;
  }

  //Read Ethernet switch vendor info and verify against the compilation configuration
  if (!(getSwitchVendorInfo() && readIfPortMappingFromSwitch() && readVLANFromSwitch())){ 
    disconnectSwitch();
    return false;
  }

  active = true;

  return true;
}

void SwitchCtrl_Session_Linux::disconnectSwitch()
{
  for(PortToIfMap::iterator i = _ports.begin(); i != _ports.end(); i++)
    free(i->second);
  
  disengage();
}

bool SwitchCtrl_Session_Linux::getSwitchVendorInfo() {
  int n;
  DIE_IF_NEGATIVE(n = writeShell("uname -s\n", 5));
  DIE_IF_NEGATIVE(n = readShell("Linux", NULL, 1, 10));
  
  if(n == 1) {
    //We got back Linux
    vendor = Linux;
  }

  DIE_IF_NEGATIVE(n = readShell(SWITCH_PROMPT, NULL, 1, 10));
  
  return true;
}

bool SwitchCtrl_Session_Linux::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
  if(!active || port==SWITCH_CTRL_PORT)
    return false; //don't touch the control port!
		
  PortToIfMap::iterator i = _ports.find(port);
  if(i == _ports.end()) {
    LOG(2) (Log::MPLS, "could not find port to interface name mapping for port=", port);
    return false;
  }
  
  if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN) {
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged)
      SetPortBit(vpmUntagged->portbits, port);
    else
      {
	LOG(1) (Log::MPLS, "getVlanPortMapByID for vlanPortMapListUntagged returned NULL");
	return false;
      }

    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) 
      {
	SetPortBit(vpmAll->portbits, port);
	//clear address and activate interface before adding to bridge
	char command[100];
	snprintf(command, sizeof(command), "sudo /sbin/ifconfig %s 0.0.0.0 up\n", i->second);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5));
	DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
	//now add to new VLAN
	snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl addif %s%d %s\n", BR_PREFIX, vlanID, i->second);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5));
	DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
	return true;
      } else
	{
	  LOG(1) (Log::MPLS, "getVlanPortMapByID returned NULL");
	  return false;
	}
  } else {
    LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    return false;
  }
}

bool SwitchCtrl_Session_Linux::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
  if(!active || port==SWITCH_CTRL_PORT)
    return false; //don't touch the control port!
  
  PortToIfMap::iterator i = _ports.find(port);
  if(i == _ports.end()) {
    LOG(2) (Log::MPLS, "could not find port to interface name mapping for port=", port);
    return false;
  }
		
  if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN) {
    vlanPortMap * vpmAll = NULL;
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) 
      {
	SetPortBit(vpmAll->portbits, port);
	//now add to new VLAN
	char command[100];
	snprintf(command, sizeof(command), "sudo /sbin/vconfig add %s %d\n", i->second, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5));
	DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
	snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl addif %s%d %s.%d\n", BR_PREFIX, vlanID, i->second, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5));
	DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
	return true;
      } else
	{
	  LOG(1) (Log::MPLS, "getVlanPortMapById returned NULL");
	  return false;
	}
  } else {
    LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    return false;
  }
}

/**
 * Finds port from interface port mapping table.
 * 
 * @returns port number, or -1 if unsuccessful.
 */
int SwitchCtrl_Session_Linux::getPortFromIfname(const char *ifname) {
  if(ifname == NULL) {
    LOG(1) (Log::MPLS, "getPortFromIfname: ifname == NULL!");
    return -1;
  }

  for(PortToIfMap::iterator i = _ports.begin(); i != _ports.end(); i++) {
    if(strcmp(ifname, i->second) == 0) {
      return i->first;
    }
  }
  return -1;
}

void SwitchCtrl_Session_Linux::addIfnameToVLAN(const char *ifname, int vlanID, bool tagged) {
  int port = getPortFromIfname(ifname);
  if(port < 0) {
    LOG(2) (Log::MPLS, "could not get port for ifname=", ifname);
    return;
  }
  
  if(getVlanPortMapById(vlanPortMapListUntagged, vlanID) == NULL ||
     getVlanPortMapById(vlanPortMapListAll, vlanID) == NULL)
    addEmptyVLAN(vlanID);
 
  vlanPortMap *vpm = getVlanPortMapById(vlanPortMapListAll, vlanID);
  if(vpm == NULL) {
    LOG(1) (Log::MPLS, "addIfnameToVLAN getVlanPortMapById for vlanPortMapListAll return null");
    return;
  }
  
  SetPortBit(vpm->portbits, port);

  if(!tagged) {
    vpm = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if(vpm == NULL) {
      LOG(1) (Log::MPLS, "addIfnameToVLAN getVlanPortMapById for vlanPortMapListUntagged return null");
      return;
    }
  
    SetPortBit(vpm->portbits, port);
  }
  
}

/**
 * Reads interfaces and creates mapping from /proc/net/dev 
 */
bool SwitchCtrl_Session_Linux::readIfPortMappingFromSwitch() {
  
  ProcNetParser parser(*this);
  DIE_IF_EQUAL(parseShellCommand(parser), false);
  return true;

}

bool SwitchCtrl_Session_Linux::readVLANFromSwitch() {
  vlanPortMapListAll.clear();
  vlanPortMapListUntagged.clear();
  
  /* get tagged vlans */
  ProcNetVLANParser parser(*this);
  LOG(2) (Log::MPLS, "ProcNetVLANParser.getCommand()=", parser.getCommand());
  DIE_IF_EQUAL(parseShellCommand(parser), false);

  /* get untagged vlans */
  BrctlParser brParser(*this);
  LOG(2) (Log::MPLS, "ProcNetVLANParser.getCommand()=", brParser.getCommand());
  DIE_IF_EQUAL(parseShellCommand(brParser), false);

  return true;
}

bool SwitchCtrl_Session_Linux::verifyVLAN(uint32 vlanID) {
  char command[100];
  
  snprintf(command, sizeof(command), "/sbin/ifconfig %s%d\n", BR_PREFIX, vlanID);
  LOG(1) (Log::MPLS, command);
  DIE_IF_NEGATIVE(writeShell(command, 5));
  
  //if we don't get "Device not found" the device is found
  int rval = readShell("Device not found", SWITCH_PROMPT, true, 1, 10);
  DIE_IF_NEGATIVE(rval);
  if(rval == 1) {
    //need to get the switch prompt 'out of the way' for next command
    DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
    return false;
  }
  return true;
  
}

bool SwitchCtrl_Session_Linux::removePortFromVLAN(uint32 port, uint32 vlanID) {

  if ((!active) || port==SWITCH_CTRL_PORT)
    return false; //don't touch the control port!

  PortToIfMap::iterator i = _ports.find(port);
  if(i == _ports.end()) {
    LOG(2) (Log::MPLS, "could not find port to interface name mapping for port=", port);
    return false;
  }

  if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN) {
    bool isTagged = false;
    vlanPortMap *vpmAll = NULL, *vpmUntagged = NULL;
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if(vpmUntagged) {
      isTagged = (HasPortBit(vpmUntagged->portbits, port) == 0);
      ResetPortBit(vpmUntagged->portbits, port);
    }
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(vpmAll) {
      ResetPortBit(vpmAll->portbits, port);
    }

    /* run brctl to remove interface from bridge representing vlan */	
    char command[100], *ifname = i->second;
    if(isTagged) 
      snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl delif %s%d %s.%d\n", BR_PREFIX, vlanID, ifname, vlanID);
    else
      snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl delif %s%d %s\n", BR_PREFIX, vlanID, ifname);
    LOG(1) (Log::MPLS, command);
    DIE_IF_NEGATIVE(writeShell(command, 5));
    DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
    
    if(isTagged) {
      snprintf(command, sizeof(command), "sudo /sbin/vconfig rem %s.%d\n", ifname, vlanID);
      LOG(1) (Log::MPLS, command);
      DIE_IF_NEGATIVE(writeShell(command, 5));
      DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
    }
    
    return true;
  } else {
    LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    return false;
  }
}

bool SwitchCtrl_Session_Linux::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
    uint32 port;
    vlanPortMap* vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(!vpmAll)
        return false;

    portList.clear();
    for (port = 1; port <= 32; port++)
    {
        if ((vpmAll->ports)&(1<<(32-port)) != 0)
            portList.push_back(port);
    }

    if (portList.size() == 0)
        return false;
    return true;
}

bool SwitchCtrl_Session_Linux::hook_removeVLAN(const uint32 vlanID)
{
	DIE_IF_EQUAL(vlanID, 0);	
	
	char command[100];
	//need to take the interface down first
	snprintf(command, sizeof(command), "sudo /sbin/ifconfig %s%d down\n", BR_PREFIX, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5)) ;
	DIE_IF_NEGATIVE(readShell( SWITCH_PROMPT, NULL, 1, 10));
	
	snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl delbr %s%d\n", BR_PREFIX, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5)) ;
	DIE_IF_NEGATIVE(readShell( SWITCH_PROMPT, NULL, 1, 10));

	return true; 
}

bool SwitchCtrl_Session_Linux::hook_createVLAN(const uint32 vlanID)
{
	DIE_IF_EQUAL(vlanID, 0);

	char command[100];
	snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl addbr %s%d\n", BR_PREFIX, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5)) ;
	DIE_IF_NEGATIVE(readShell( SWITCH_PROMPT, NULL, 1, 10));
	
	//set bridge parameters
	snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl setfd %s%d 0\n", BR_PREFIX, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5)) ;
	DIE_IF_NEGATIVE(readShell( SWITCH_PROMPT, NULL, 1, 10));

	snprintf(command, sizeof(command), "sudo /usr/local/sbin/brctl stp %s%d off\n", BR_PREFIX, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5)) ;
	DIE_IF_NEGATIVE(readShell( SWITCH_PROMPT, NULL, 1, 10));

	snprintf(command, sizeof(command), "sudo /sbin/ifconfig %s%d up\n", BR_PREFIX, vlanID);
	LOG(1) (Log::MPLS, command);
	DIE_IF_NEGATIVE(writeShell(command, 5)) ;
	DIE_IF_NEGATIVE(readShell( SWITCH_PROMPT, NULL, 1, 10));

	//add the new *empty* vlan into PortMapListAll and portMapListUntagged
	addEmptyVLAN(vlanID);

	return true;
}

/** Adds an empty VLAN to the lists where it is not already. */
void SwitchCtrl_Session_Linux::addEmptyVLAN(int vlanID) {

  if(vlanID <= 0 || vlanID > MAX_VLAN)
    return;

  vlanPortMap vpm;
  memset(&vpm, 0, sizeof(vlanPortMap));
  vpm.vid = vlanID;

  if(getVlanPortMapById(vlanPortMapListAll, vlanID) == NULL)
    vlanPortMapListAll.push_back(vpm);

  if(getVlanPortMapById(vlanPortMapListUntagged, vlanID) == NULL)
    vlanPortMapListUntagged.push_back(vpm);

}

bool SwitchCtrl_Session_Linux::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    vlanPortMap vpm_empty;
    memset(&vpm_empty, 0, sizeof(vlanPortMap));

    return (memcmp(&vpm.portbits, &vpm_empty.portbits, MAX_VLAN_PORT_BYTES) == 0);
}

void SwitchCtrl_Session_Linux::ProcNetParser::parseLine(const char *line, const int length) {
  int ret, garbage;
  char *ifname = (char*) malloc(sizeof(char) * (LINELEN + 1));
  if(ifname == NULL) {
    LOG(1) (Log::MPLS, "could not allocate memory for ifname mapping");
    return;
  }
  ret = sscanf(line, " %[a-zA-Z0-9.]: %i ", ifname, &garbage);
  if(ret >= 2) {
    _session.addPortToIfMapping(_portNum, ifname);
    _portNum++;
  }
}

void SwitchCtrl_Session_Linux::ProcNetVLANParser::parseLine(const char *line, const int length) {
  int ret;
  char ifname[LINELEN + 1], vlanif[LINELEN + 1];
  int vid;
  ret = sscanf(line, " %s | %i | %s", vlanif, &vid, ifname);
  if(ret == 1) {
    if(strcmp(vlanif, "Password:") == 0) {
      LOG(3) (Log::MPLS, "Switch user is not configured to run sudo ", getCommand(), " without password!");
      return;
    }
  }
  if(ret >= 2) {
    _session.addIfnameToVLAN(ifname, vid, true);
  }
}

void SwitchCtrl_Session_Linux::BrctlParser::parseLine(const char *line, const int length) {
  int ret;
  char brname[LINELEN + 1], brid[LINELEN + 1], stp[LINELEN + 1], ifname[LINELEN + 1];
  
  ret = sscanf(line, " %s %s %s %s", brname, brid, stp, ifname);
  switch(ret) {
  case 1:
    /* in case of only interface on row, then it is stored in brname
       instead of ifname */
    if(_curVid > 0 && !isTaggedIf(brname))
      _session.addIfnameToVLAN(brname, _curVid, false);
    break;
  case 3:
    //fallthrough
  case 4:
    /* this could be a bridge configuration */
    _curVid = getVid(brname);
    if(_curVid > 0) {
      if(ret == 3)
	/* no interfaces */
	_session.addEmptyVLAN(_curVid);
      else if(ret == 4 && !isTaggedIf(ifname))
	_session.addIfnameToVLAN(ifname, _curVid, false);
    }
    break;
  default:
    LOG(2) (Log::MPLS, "BrctlParser: did not parse line", line);
    break;
  }
}

int SwitchCtrl_Session_Linux::BrctlParser::getVid(const char *ifname) {
  char format[LINELEN + 1];
  snprintf(format, sizeof(format), "%s%%d", BR_PREFIX);
  int vid;
  if((sscanf(ifname, format, &vid) == 1) && vid > 0) {
    return vid;
  } else {
    return -1;
  }
}

bool SwitchCtrl_Session_Linux::BrctlParser::isTaggedIf(const char *ifname) {
  for(unsigned int i = 0; i < strlen(ifname); i++) {
    if(ifname[i] == '.')
      return true;
  }
  return false;
}

#endif
