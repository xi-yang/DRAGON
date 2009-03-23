/****************************************************************************

SwitchCtrl_Session_JuniperEX module auxillary source file SwitchCtrl_JUNOScript.cc
Created by Xi Yang on 03/16/2009
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_JUNOScript.h"
#include "RSVP_Log.h"

//	set interfaces ge-0/0/0 unit 0 family ethernet-switching vlan members dynamic_vlan_300

const char *JUNOScriptMovePortVlanComposer::jsTemplate = "<rpc>\
    <load-configuration>\
      <configuration>\
         <interfaces>\
            <interface>\
		<name>ge-0/0/1</name>\
		<unit>\
		  <name>0</name>\
		  <family>\
		      <ethernet-switching>\
			    <port-mode>access</port-mode>\
			    <vlan><members>dynamic_vlan_300</members></vlan>\
		      </ethernet-switching>\
		  </family>\
		</unit>\
            </interface>\
         </interfaces>\
         <vlans>\
            <vlan>\
		<name>dyanmic_vlan_300</name>\
		<interface>ge-0/0/1.0</interface>\
            </vlan>\
         </vlans>\
       </configuration>\
    </load-configuration>\
</rpc>";

const char *JUNOScriptVlanComposer::jsTemplate = "<rpc>\
    <load-configuration>\
      <configuration>\
         <vlans>\
            <vlan>\
		<name>dyanmic_vlan_300</name>\
            </vlan>\
         </vlans>\
       </configuration>\
    </load-configuration>\
</rpc>";

const char *JUNOScriptBandwidthPolicyComposer::jsTemplate= "<rpc>\
<load-configuration>\
<configuration>\
    <firewall> \
        <policer>\
            <name>policier_vlan_300_in</name>\
        </policer>\
        <family>\
            <ethernet-switching>\
                <filter>\
                    <name>filter_vlan_300_in</name>\
                </filter>\
            </ethernet-switching>\
        </family> \
    </firewall>\
    <vlans>\
        <vlan>\
            <name>dynamic_vlan_300</name>\
            <filter>\
            </filter>\
        </vlan>\
    </vlans>\
</configuration>\
</load-configuration>\
</rpc>";
/*
//To add the policy
<rpc>
<load-configuration>
<configuration>
    <firewall> 
        <policer>
            <name>policier_vlan_300_in</name>
            <if-exceeding>
                <bandwidth-limit>100m</bandwidth-limit>
                <burst-size-limit>50k</burst-size-limit>
            </if-exceeding>
            <then><discard/></then>
        </policer>
        <family>
            <ethernet-switching>
                <filter>
                    <name>filter_vlan_300_in</name>
                    <term>
                        <name>traffic-limit</name>
                        <then> <policer>policier_vlan_300_in</policer> </then>
                    </term>
                </filter>
            </ethernet-switching>
        </family> 
    </firewall>
    <vlans>
        <vlan>
            <name>dynamic_vlan_300</name>
            <filter>
                <input>filter_vlan_300_in</input>
            </filter>
        </vlan>
    </vlans>
</configuration>
</load-configuration>
</rpc>

//To delete the policy
<rpc>
<load-configuration>
<configuration>
    <firewall> 
        <policer delete="delete">
            <name>policier_vlan_300_in</name>
        </policer>
        <family>
            <ethernet-switching>
                <filter delete="delete">
                    <name>filter_vlan_300_in</name>
                </filter>
            </ethernet-switching>
        </family> 
    </firewall>
    <vlans>
        <vlan>
            <name>dynamic_vlan_300</name>
            <filter delete="delete"/>
        </vlan>
    </vlans>
</configuration>
</load-configuration>
</rpc>
*/

///////////////////////////////////////////////////////////////////////


JUNOScriptComposer::~JUNOScriptComposer()
{
    freeScriptDoc();
    if (isScriptBufInternal && xmlScript != NULL)
        delete xmlScript;
}

bool JUNOScriptComposer::initScriptDoc(const char* xmlBuf)
{
    freeScriptDoc();
    xmlDoc = xmlReadMemory((const char*)xmlBuf, strlen((const char*)xmlBuf), (const char*)"junoscript.xml", NULL, 0);
    if (xmlDoc == NULL) {
        LOG(2)(Log::MPLS, "Error: JUNOScriptComposer::initScriptDoc() failed to parse memory: ", xmlBuf);
        return false;
    }
    xpathCtx =  xmlXPathNewContext(xmlDoc);
    if(xpathCtx == NULL) {
        LOG(2)(Log::MPLS, "Error: JUNOScriptComposer::initScriptDoc() failed to create XPath context: ", xmlBuf);
        return false;
    }
    return true;
}

bool JUNOScriptComposer::makeScript()
{
    assert(xmlDoc && xmlScript);

    xmlChar* xmlScriptNew = NULL;
    int scriptLenNew = 0;
    xmlDocDumpFormatMemory(xmlDoc, &xmlScriptNew, &scriptLenNew, 1);
    if (xmlScriptNew == NULL || scriptLenNew <= 0)
        return false;
    if (isScriptBufInternal)
    {
        if (xmlScript != NULL)
            xmlFree(xmlScript);
        xmlScript = xmlScriptNew;
        scriptLen = scriptLenNew;
    }
    else
    {
        if (scriptLenNew > scriptLen)
        {
            LOG(1)(Log::MPLS, "Error: JUNOScriptComposer::makeScript(): external buffer too small!");
            scriptLenNew = scriptLen;
        }
        memcpy((char*)xmlScript, (char*)xmlScriptNew, scriptLenNew);
        xmlScript[scriptLenNew] = '\0';
    }
    return true;
}

bool JUNOScriptComposer::finishScript()
{
    bool ret = makeScript();
    freeScriptDoc();
    return ret;
}

void JUNOScriptComposer::freeScriptDoc()
{
    if (xpathCtx != NULL)
    {
        xmlXPathFreeContext(xpathCtx);
        xpathCtx = NULL;
    }
    if (xmlDoc != NULL)  
    {
        xmlFreeDoc(xmlDoc);
        xmlDoc = NULL;
    }
}

/*
tagged port: set interfaces ge-0/0/0 unit 0 family ethernet-switching port-mode trunk
	set interfaces ge-0/0/0 unit 0 family ethernet-switching vlan members dynamic_vlan100
	set vlans vlan100 interface ge-0/0/0.0     
	//set interfaces ge-0/0/0 unit 0 family ethernet-switching native-vlan-id 100 (?)

untagged port: set interfaces ge-0/0/0 unit 0 family ethernet-switching port-mode access
	set vlans vlan100 interface ge-0/0/0.0
	set interfaces ge-0/0/0 unit 0 family ethernet-switching native-vlan-id 100
*/


bool JUNOScriptMovePortVlanComposer::setPortAndVlan(uint32 portId, uint32 vlanId, bool isTrunking, bool isToDelete)
{
    if (xmlDoc == NULL)
        if (!initScriptDoc(JUNOScriptMovePortVlanComposer::jsTemplate))
        {
            LOG(5)(Log::MPLS, "Error: initScriptDoc(JUNOScriptMovePortVlanComposer::jsTemplate) failed in JUNOScriptMovePortVlanComposer::setPortAndVlan(", portId, ", ", vlanId, ").");
            freeScriptDoc();
            return false;
        }

    char portName[32], vlanName[32];
    sprintf(portName, "ge-%d/%d/%d", (portId & 0xf000)>>12, (portId & 0x0f00)>>8, portId&0xff);
    sprintf(vlanName, "dynamic_vlan_%d", vlanId);

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/interfaces/interface/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)portName);
    //do not delete this node

    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/interfaces/interface/unit/family/ethernet-switching/vlan/members", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)vlanName);
    if (isToDelete)
        xmlNewProp(xpathObj->nodesetval->nodeTab[0], (xmlChar*)"delete", (xmlChar*)"delete");

    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/interfaces/interface/unit/family/ethernet-switching/port-mode", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    if (isToDelete)
    {
        xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)"");
        xmlNewProp(xpathObj->nodesetval->nodeTab[0], (xmlChar*)"delete", (xmlChar*)"delete");
    }
    else 
    {
        if (isTrunking)
        {
            xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)"trunk");
             /*PVID may be set for trunk-mode port
            xmlNodePtr node1 = xmlNewNode(NULL, (xmlChar*)"native-vlan-id");
            char vlanIdStr[8]; sprintf(vlanIdStr, "%d", vlanId);
            xmlNodeSetContent(node1, (xmlChar*)vlanIdStr);
            if (isToDelete)
                xmlNewProp(node1, (xmlChar*)"delete", (xmlChar*)"delete");
            xmlAddNextSibling(xpathObj->nodesetval->nodeTab[0], node1);  // create /rpc/load-configuration/configuration/interfaces/interface/family/ethernet-switching/native-vlan-id
            */
        }
        else
        {
            xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)"access");
        }
    }

    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/vlans/vlan/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)vlanName);
    //do not delete this node

    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/vlans/vlan/interface", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    strcat(portName, ".0");
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)portName);
    if (isToDelete)
        xmlNewProp(xpathObj->nodesetval->nodeTab[0], (xmlChar*)"delete", (xmlChar*)"delete");

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return finishScript();
}


bool JUNOScriptVlanComposer::setVlan(uint32 vlanId, bool isToDelete)
{
    if (xmlDoc == NULL)
        if (!initScriptDoc(JUNOScriptVlanComposer::jsTemplate))
        {
            LOG(3)(Log::MPLS, "Error: initScriptDoc(JUNOScriptVlanComposer::jsTemplate) failed in JUNOScriptVlanComposer::setVlan(", vlanId, ").");
            freeScriptDoc();
            return false;
        }

    char vlanName[32];
    sprintf(vlanName, "dynamic_vlan_%d", vlanId);

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/vlans/vlan/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)vlanName);
    if (isToDelete)
    {
        xmlNewProp(xpathObj->nodesetval->nodeTab[0]->parent, (xmlChar*)"delete", (xmlChar*)"delete");
    }
    else 
    {
        char vlanString[64];
        sprintf(vlanString, "%d", vlanId);
        xmlNodePtr node1 = xmlNewNode(NULL, (xmlChar*)"vlan-id");
        xmlNodeSetContent(node1, (xmlChar*)vlanString);
        xmlAddNextSibling(xpathObj->nodesetval->nodeTab[0], node1);
        sprintf(vlanString, "DRAGON VLSR Created Dynamic VLAN (VID=%d)", vlanId);
        node1 = xmlNewNode(NULL, (xmlChar*)"description");
        xmlNodeSetContent(node1, (xmlChar*)vlanString);
        xmlAddNextSibling(xpathObj->nodesetval->nodeTab[0], node1);
    }

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return finishScript();
}



bool JUNOScriptVlanComposer::getVlan(uint32 vlanId)
{
    if (xmlDoc == NULL)
        if (!initScriptDoc(JUNOScriptVlanComposer::jsTemplate))
        {
            LOG(3)(Log::MPLS, "Error: initScriptDoc(JUNOScriptVlanComposer::jsTemplate) failed in JUNOScriptVlanComposer::setVlan(", vlanId, ").");
            freeScriptDoc();
            return false;
        }

    char vlanName[32];
    sprintf(vlanName, "dynamic_vlan_%d", vlanId);

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/vlans/vlan/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)vlanName);
    //change "/rpc/load-configuration/..." into "/rpc/get-configuration/..."
    xmlNodeSetName(xpathObj->nodesetval->nodeTab[0]->parent->parent->parent->parent, (xmlChar*)"get-configuration");
    
    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return finishScript();
}


bool JUNOScriptBandwidthPolicyComposer::setVlanPolicier(uint32 vlanId, float bwLimit, int burstSize, bool isToDelete)
{
    if (xmlDoc == NULL)
        if (!initScriptDoc(JUNOScriptBandwidthPolicyComposer::jsTemplate))
        {
            LOG(3)(Log::MPLS, "Error: initScriptDoc(JUNOScriptVlanComposer::jsTemplate) failed in JUNOScriptVlanComposer::setVlan(", vlanId, ").");
            freeScriptDoc();
            return false;
        }

    xmlNodePtr node1, node2;
    char vlanName[32], policierName[32], filterName[32], aValue[32];
    sprintf(vlanName, "dynamic_vlan_%d", vlanId);
    sprintf(policierName, "policier_vlan_%d_in", vlanId);
    sprintf(filterName, "filter_vlan_%d_in_in", vlanId);
    //set policier
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/firewall/policer/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)policierName);
    if (isToDelete)
    {
        xmlNewProp(xpathObj->nodesetval->nodeTab[0]->parent, (xmlChar*)"delete", (xmlChar*)"delete");
    }
    else
    {
        node1 = xmlNewNode(NULL, (xmlChar*)"if-exceeding");
        xmlAddNextSibling(xpathObj->nodesetval->nodeTab[0], node1);
        node2 = xmlNewNode(NULL, (xmlChar*)"bandwidth-limit");
        sprintf(aValue, "%dm", (int)bwLimit);
        xmlNodeSetContent(node2, (xmlChar*)aValue);
        xmlAddChild(node1, node2);
        node2 = xmlNewNode(NULL, (xmlChar*)"burst-size-limit");
        sprintf(aValue, "%dk", burstSize);
        xmlNodeSetContent(node2, (xmlChar*)aValue);
        xmlAddChild(node1, node2);        
        node2 = xmlNewNode(NULL, (xmlChar*)"then");
        xmlAddNextSibling(node1, node2);
        node1 = node2;
        node2 = xmlNewNode(NULL, (xmlChar*)"discard");
        xmlAddChild(node1, node2);
    }
    //set filter
    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/firewall/family/ethernet-switching/filter/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)filterName);
    if (isToDelete)
    {
        xmlNewProp(xpathObj->nodesetval->nodeTab[0]->parent, (xmlChar*)"delete", (xmlChar*)"delete");
    }
    else
    {
        node1 = xmlNewNode(NULL, (xmlChar*)"term");
        xmlAddNextSibling(xpathObj->nodesetval->nodeTab[0], node1);
        node2 = xmlNewNode(NULL, (xmlChar*)"name");
        sprintf(aValue, "traffic_limit_vlan_%d", vlanId);
        xmlNodeSetContent(node2, (xmlChar*)aValue);
        xmlAddChild(node1, node2);
        node2 = xmlNewNode(NULL, (xmlChar*)"then");
        xmlAddChild(node1, node2);
        node1 = node2;
        node2 = xmlNewNode(NULL, (xmlChar*)"policer");
        xmlNodeSetContent(node2, (xmlChar*)policierName);
        xmlAddChild(node1, node2);
    }
    //apply filter to vlan
    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/vlans/vlan/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    xmlNodeSetContent(xpathObj->nodesetval->nodeTab[0], (xmlChar*)vlanName);
    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc/load-configuration/configuration/vlans/vlan/filter", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        freeScriptDoc();
        return false;
    }  
    if (isToDelete)
    {
        xmlNewProp(xpathObj->nodesetval->nodeTab[0], (xmlChar*)"delete", (xmlChar*)"delete");
    }
    else
    {
        node1 = xpathObj->nodesetval->nodeTab[0];
        node2 = xmlNewNode(NULL, (xmlChar*)"input");
        xmlNodeSetContent(node2, (xmlChar*)filterName);
        xmlAddChild(node1, node2);
    }

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return finishScript();
}

////////////////////////////////////////////////////////////////////////////

JUNOScriptParser::~JUNOScriptParser() 
{ 
    if (xpathCtx != NULL)
       xmlXPathFreeContext(xpathCtx);
    if (xmlDoc != NULL)  
        xmlFreeDoc(xmlDoc);
}

bool JUNOScriptParser::loadAndVerifyScript(char* bufScript)
{
    if (bufScript != NULL)
        xmlScript = bufScript;
    if (xmlScript == NULL)
        return false;

    //pre-process the script to remove default name-space attribute (for xpath use)
    char* ps = xmlScript;
    while ((ps = strstr(ps, "xmlns=")) != NULL)
    {
        while(*ps != ' ' && *ps != '\t' && *ps != '>')
            *(ps++) = ' ';
    }
    
    xmlDoc = xmlReadMemory(xmlScript, strlen(xmlScript), "junoscript.xml", NULL, 0);
    if (xmlDoc == NULL) {
        LOG(2)(Log::MPLS, "Error: JUNOScriptComposer::initScriptDoc() failed to parse memory: ", xmlScript);
        return false;
    }
    xpathCtx =  xmlXPathNewContext(xmlDoc);
    if(xpathCtx == NULL) {
        LOG(2)(Log::MPLS, "Error: JUNOScriptComposer::initScriptDoc() failed to create XPath context: ", xmlScript);
        return false;
    }

    int i;
    char junos_ns[64];
    if ((ps = strstr(xmlScript, "xmlns:junos=\"")) != NULL)
    {
        i = 0;
        ps += 13; 
        while(*ps != ' ' && *ps != '\t' && *ps != '\"')
            junos_ns[i++] = *(ps++);
        junos_ns[i] = '\0';
        if (xmlXPathRegisterNs(xpathCtx, (xmlChar*)"junos", (xmlChar*)junos_ns) != 0){
            LOG(2)(Log::MPLS, "Error: JUNOScriptComposer failed to regiser namespace ", junos_ns);
            return false;
        }
    }  

    if (xmlXPathRegisterNs(xpathCtx, (xmlChar*)"xnm", (xmlChar*)"http://xml.juniper.net/xnm/1.1/xnm") != 0){
        LOG(1)(Log::MPLS, "Error: JUNOScriptComposer failed to regiser namespace: http://xml.juniper.net/xnm/1.1/xnm");
        return false;
    }

    return true;
}

bool JUNOScriptLockReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/xnm:error/message", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }

    errMessage = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);
    if (errMessage.empty() || errMessage.leftequal("Configuration database is already open"))
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }
    //$$$$ handling "configuration database modified" ?

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return false;
}

//<load-configuration-results>
//<load-success/>
//</load-configuration-results>

bool JUNOScriptLoadReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/load-configuration-results/load-error-count", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }

    int count;
    sscanf((const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]), "%d", &count);
    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/load-configuration-results/xnm:error | /rpc-reply/load-configuration-results/xnm:warning", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return false;
    }

    xmlNodePtr node;
    int i = 0;
    char buf[10];
    XML_LOOP_NODESET(xpathObj->nodesetval, node)
    {
        if (strstr((char*)node->name, "token") != NULL || strstr((char*)node->name, "edit-path") != NULL)
        {
            i++;
            sprintf(buf, "Error|Warning(%d/%d): --@<", i, count);
            errMessage += (const char*)xmlNodeGetContent(node);
            errMessage += "> --: ";
        }
        if (strcasecmp((char*)node->name, "message") == 0)
        {
            errMessage += (const char*)xmlNodeGetContent(node);
            errMessage += "\n";
        }
    }

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return false;
}

bool JUNOScriptCommitReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/commit-results/xnm:error/message", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }

    errMessage = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);
    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return false;
}

bool JUNOScriptUnlockReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/xnm:error/message", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }

    errMessage = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);

    if (errMessage.leftequal("Configuration database is not open"))
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return false;
}


bool JUNOScriptVlanConfigParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/xnm:error/message | /rpc-reply/xnm:warning/message", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }

    errMessage = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return false;

}

bool JUNOScriptVlanConfigParser::getInterfaces(SimpleList<String>& interfaces)
{
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/configuration/vlan/interface/name", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return false;
    }
    interfaces.clear();
    for (int i = 0; i < xpathObj->nodesetval->nodeNr; i++)
    {
        String anInterface = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[i]);
        interfaces.push_back(anInterface);
    }

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return true;

}

bool JUNOScriptVlanConfigParser::getFilter(String& filter)
{
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"/rpc-reply/configuration/vlan/filter", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return false;
    }

    filter = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);

    if (xpathObj) xmlXPathFreeObject(xpathObj);
    return true;

}


