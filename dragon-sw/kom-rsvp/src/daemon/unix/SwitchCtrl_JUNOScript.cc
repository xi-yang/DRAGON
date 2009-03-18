/****************************************************************************

SwitchCtrl_Session_JuniperEX module auxillary source file SwitchCtrl_JUNOScript.cc
Created by Xi Yang on 03/16/2009
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_JUNOScript.h"
/*
static char *jsCreateVlan = "<rpc>\
    <load-configuration>\
      <configuration>\
         <vlans>\
            <vlan>\
		<name>dynamic_vlan_XXXX</name>\
		<description>DRAGON VLSR Created Dynamic VLAN (VID=XXXX)</description>\
		<vlan-id>300</vlan-id>\
            </vlan>\
         </vlans>\
       </configuration>\
    </load-configuration>\
</rpc>";

static char * jsDeleteVlan = "<rpc>\
    <load-configuration>\
      <configuration>\
         <vlans>\
            <vlan delete=\"delete\">\
		<name>dynamic_vlan_XXXX</name>\
            </vlan>\
         </vlans>\
       </configuration>\
    </load-configuration>\
</rpc>";
*/

#define XML_LOOP_NODESET(S, N) \
    if (S != NULL && S->nodeNr > 0)  \
        for (N=S->nodeTab[0]; N != NULL; N=N->next)


//////////////////////////////////////////////////////////////////////////////


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
        //$$$$ Log::
        return false;
    }
    xpathCtx =  xmlXPathNewContext(xmlDoc);
    if(xpathCtx == NULL) {
        //$$$$ Log::
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
            //$$$$ Log::
            return false;
        }
    }  

    if (xmlXPathRegisterNs(xpathCtx, (xmlChar*)"xnm", (xmlChar*)"http://xml.juniper.net/xnm/1.1/xnm") != 0){
        //$$$$ Log::
        return false;
    }

    return true;
}

bool JUNOScriptLockReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL || xpathCtx == NULL)
        return false;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/xnm:error/message", xpathCtx);
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

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/load-configuration-results/load-error-count", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL ||xpathObj->nodesetval->nodeNr == 0)
    {
        if (xpathObj) xmlXPathFreeObject(xpathObj);
        return true;
    }

    int count;
    sscanf((const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]), "%d", &count);
    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/load-configuration-results/xnm:error", xpathCtx);
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
            sprintf(buf, "Error(%d/%d): --@<", i, count);
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

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/commit-results/xnm:error/message", xpathCtx);
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

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/xnm:error/message", xpathCtx);
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


