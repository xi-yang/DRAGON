/****************************************************************************

CLI Based Linux Switch Control Module header file SwitchCtrl_Session_Linux.h
Created by Lars Persson Fink @ 11/21/2006
Modified by Xi Yang, 2006

****************************************************************************/

#ifdef Linux

#ifndef _SwitchCtrl_Session_Linux_H_
#define _SwitchCtrl_Session_Linux_H_

#include <map>
#include <utility>
#include "SwitchCtrl_Global.h"
#include "CLI_Session.h"
#include "RSVP_Log.h"

class SwitchCtrl_Session_Linux: public CLI_Session
{
public:
  friend class ProcNetParser;
  friend class ProcNetVLANParser;

  SwitchCtrl_Session_Linux(): CLI_Session() {
    rfc2674_compatible = false;
    snmp_enabled = false;
  }

  SwitchCtrl_Session_Linux(const String& sName, const NetAddress& swAddr): CLI_Session(sName, swAddr) {
    rfc2674_compatible = false;
    snmp_enabled = false;
  }

  virtual ~SwitchCtrl_Session_Linux() {}
  virtual bool connectSwitch() { return connectSwitch("ogin: "); }
  virtual bool connectSwitch(const char *loginString);
  virtual void disconnectSwitch();
  virtual bool getSwitchVendorInfo();
  virtual bool readVLANFromSwitch();
  virtual bool verifyVLAN(uint32 vlan);
  virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
  virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
  virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
  virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);
  virtual bool hook_createVLAN(const uint32 vlanID);
  virtual bool hook_removeVLAN(const uint32 vlanID);
  virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
  virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars) {
    LOG(1) (Log::MPLS, "SwitchCtrl_Session::hook_getPortMapFromSnmpVars not implemented");
  }
  virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port) { 
    LOG(6) (Log::MPLS, "hook_hasPortinVlanPortMap vpm.vid=", vpm.vid, " vpm.portbits=", vpm.portbits, " port ",  port)
    if (HasPortBit(vpm.portbits, port))
      return true;
    return false;
  }

  class ProcNetParser : public CLICommandParser {
  public:
    ProcNetParser(SwitchCtrl_Session_Linux& session, unsigned int writeTimeout = 5,
		  unsigned int readTimeout = 15) : 
      CLICommandParser((char*)"cat /proc/net/dev\n", writeTimeout, readTimeout),
      _session(session) 
      {
      _portNum = 1;
    }

    void parseLine(const char *line, const int length);
  private:
    int _portNum;
    SwitchCtrl_Session_Linux& _session;
  }; 
  
  class ProcNetVLANParser : public CLICommandParser {
  public:
    ProcNetVLANParser(SwitchCtrl_Session_Linux& session, 
		  unsigned int writeTimeout = 5,
		  unsigned int readTimeout = 15) : CLICommandParser((char*)"sudo cat /proc/net/vlan/config\n", writeTimeout, readTimeout), _session(session) {
    }
    void parseLine(const char *line, const int length);
  private:
    SwitchCtrl_Session_Linux& _session;
  }; 

  class BrctlParser : public CLICommandParser {
  public:
    BrctlParser(SwitchCtrl_Session_Linux& session, unsigned int writeTimeout = 5,
		unsigned int readTimeout = 15) : 
      CLICommandParser((char*)"sudo brctl show\n", writeTimeout, readTimeout),
      _session(session) 
      {
	_curVid = -1;
    }

    void parseLine(const char *line, const int length);
  private:
    int getVid(const char *ifname);
    bool isTaggedIf(const char *ifname);
    int _curVid;
    SwitchCtrl_Session_Linux& _session;
  };

 protected:
  /**
   * Adds an port to interface mapping, if the port number already has
   * a mapping it will be replaced with a new mapping.
   */
  void addPortToIfMapping(int portNum, char *interface) {
    std::pair<int, char*> tmp(portNum,interface);
    _ports.insert(tmp);
  }

  void addIfnameToVLAN(const char *ifname, int vlanID, bool tagged);
  int getPortFromIfname(const char *ifname);
  bool getVLANsFromPortMapping();
  bool readIfPortMappingFromSwitch();
  int getVidFromIfName(char *ifname);
  void addEmptyVLAN(int vlanID);

 private:
  typedef std::map<int, char*> PortToIfMap;
  //The intefaces are those defined in /proc/net/dev
  //The port number of an interface is its one-based sequential index in the file.
  PortToIfMap _ports;
 
  /**
   * translates port number to interface name
   * @returns an interface name if port can be found, NULL otherwise.
   */
  char *portNumToInterface(int portNum);
};

#endif //ifndef _SwitchCtrl_Session_Linux_H_

#endif //ifdef Linux
