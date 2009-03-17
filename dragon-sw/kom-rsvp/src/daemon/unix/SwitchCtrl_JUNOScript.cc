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
		<name>dyanmic_vlan_XXXX</name>\
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
		<name>dyanmic_vlan_XXXX</name>\
            </vlan>\
         </vlans>\
       </configuration>\
    </load-configuration>\
</rpc>";
*/

#define XML_LOOP_NODESET(S, N) \
    if (S != NULL && S->nodeNr > 0)  \
        for (N=S->nodeTab[0]; N != NULL; N=N->next)

#define START_XPATH(D, X) \
    xpathCtx =  xmlXPathNewContext(D);\
    if(xpathCtx == NULL)  \
        return X

#define FREE_XPATH_EXIT(X) \
    if (xpathObj) xmlXPathFreeObject(xpathObj);\
    if (xpathCtx) xmlXPathFreeContext(xpathCtx); \
    return X


//////////////////////////////////////////////////////////////////////////////


JUNOScriptParser::~JUNOScriptParser() 
{ 
    if (xmlDoc != NULL)  
        xmlFreeDoc(xmlDoc);
}

bool JUNOScriptParser::loadAndVerifyScript(char* bufScript)
{
    if (bufScript != NULL)
        xmlScript = bufScript;
    if (xmlScript == NULL)
        return false;

    xmlDoc = xmlReadMemory(xmlScript, strlen(xmlScript), "junoscript.xml", NULL, 0);
    if (xmlDoc == NULL) {
        //$$$$ Log::
        return false;
    }
    return true;
}

bool JUNOScriptLockReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL)
        return false;

    xmlXPathContextPtr xpathCtx;
    START_XPATH(xmlDoc, false);

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/xnm:error/message", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL)
    {
        FREE_XPATH_EXIT(true);
    }

    errMessage = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);
    if (errMessage.empty() || errMessage.leftequal("Configuration database is already open"))
    {
        FREE_XPATH_EXIT(true);
    }

    FREE_XPATH_EXIT(false);
}

//<load-configuration-results>
//<load-success/>
//</load-configuration-results>

bool JUNOScriptLoadReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL)
        return false;


    xmlXPathContextPtr xpathCtx;
    START_XPATH(xmlDoc, false);

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/load-configuration-results/load-error-count", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL)
    {
        FREE_XPATH_EXIT(true);
    }

    int count;
    sscanf((const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]), "%d", &count);
    xmlXPathFreeObject(xpathObj);
    xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/load-configuration-results/xnm:error", xpathCtx);
    if(xpathObj == NULL)
    {
        FREE_XPATH_EXIT(false);
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

    FREE_XPATH_EXIT(false);
}

bool JUNOScriptCommitReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL)
        return false;

    xmlXPathContextPtr xpathCtx;
    START_XPATH(xmlDoc, false);

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/commit-results/xnm:error/message", xpathCtx);
    if (xpathObj == NULL || xpathObj->nodesetval == NULL)
    {
        FREE_XPATH_EXIT(true);
    }

    errMessage = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);
    FREE_XPATH_EXIT(false);
}


bool JUNOScriptUnlockReplyParser::isSuccessful()
{
    errMessage = "";
    if (xmlDoc == NULL)
        return false;

    xmlXPathContextPtr xpathCtx =  xmlXPathNewContext(xmlDoc);
    START_XPATH(xmlDoc, false);

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)"//rpc-reply/xnm:error/message", xpathCtx);
    if(xpathObj == NULL || xpathObj->nodesetval == NULL) 
    {
        FREE_XPATH_EXIT(true);
    }

    errMessage = (const char*)xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);

    if (errMessage.leftequal("Configuration database is not open"))
    {
        FREE_XPATH_EXIT(true);
    }

    FREE_XPATH_EXIT(false);
}


