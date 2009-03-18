/****************************************************************************

SwitchCtrl_Session_JuniperEX module auxillary source file SwitchCtrl_JUNOScript.h
Created by Xi Yang @ 03/16/2009
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#ifndef _SwitchCtrl_JUNOScript_H_
#define _SwitchCtrl_JUNOScript_H_

using namespace std;

#include "RSVP_System.h"
#include "RSVP_String.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>



#define XML_LOOP_NODESET(S, N) \
    if (S != NULL && S->nodeNr > 0)  \
        for (N=S->nodeTab[0]; N != NULL; N=N->next)

/////////////////////////////////////////////////////////////////

class JUNOScriptParser
{
public:
    JUNOScriptParser(): xmlScript(NULL), xmlDoc(NULL), xpathCtx(NULL) {}
    JUNOScriptParser(char* buf): xmlScript(buf), xmlDoc(NULL), xpathCtx(NULL) {}
    virtual ~JUNOScriptParser();
    virtual bool isSuccessful() = 0;

    bool loadAndVerifyScript(char* bufScript = NULL);
    String& getErrorMessage() { return errMessage; }

protected:
    char* xmlScript;
    xmlDocPtr xmlDoc;
    xmlXPathContextPtr xpathCtx;
    String errMessage;
};

class JUNOScriptLockReplyParser: public JUNOScriptParser
{
public:
    JUNOScriptLockReplyParser(char* buf): JUNOScriptParser(buf) {}
    bool isSuccessful();
};


class JUNOScriptLoadReplyParser: public JUNOScriptParser
{
public:
    JUNOScriptLoadReplyParser(char* buf): JUNOScriptParser(buf) {}
    bool isSuccessful();
};

class JUNOScriptCommitReplyParser: public JUNOScriptParser
{
public:
    JUNOScriptCommitReplyParser(char* buf): JUNOScriptParser(buf) {}
    bool isSuccessful();
};

class JUNOScriptUnlockReplyParser: public JUNOScriptParser
{
public:
    JUNOScriptUnlockReplyParser(char* buf): JUNOScriptParser(buf) {}
    bool isSuccessful();
};

///////////////////////////////////////////////////////////

class JUNOScriptComposer
{
public:
   JUNOScriptComposer(): xmlScript(NULL), xmlDoc(NULL), xpathCtx(NULL), isScriptBufInternal(true) { }
   JUNOScriptComposer(char* buf, int len): xmlScript((xmlChar*)buf), scriptLen(len), xmlDoc(NULL), xpathCtx(NULL) , isScriptBufInternal(false) { }
    virtual ~JUNOScriptComposer();
    bool initScriptDoc(const char* xmlBuf);
    void freeScriptDoc();
    bool makeScript();
    bool finishScript();
    char* getScript() { return (char*)xmlScript; }

protected:
    xmlChar* xmlScript;
    int scriptLen;
    xmlDocPtr xmlDoc;
    xmlXPathContextPtr xpathCtx;
    bool isScriptBufInternal;

private:

};

class JUNOScriptMovePortVlanComposer: public JUNOScriptComposer
{
public:
    JUNOScriptMovePortVlanComposer() {}
    virtual ~JUNOScriptMovePortVlanComposer() {}
    bool setPortAndVlan(uint32 portId, uint32 vlanId, bool isTrunking, bool isToDelete);

private:
    static const char* jsTemplate;
};

class JUNOScriptVlanComposer: public JUNOScriptComposer
{
public:
    JUNOScriptVlanComposer() {}
    virtual ~JUNOScriptVlanComposer() {}
    bool setVlan(uint32 vlanId, bool isToDelete);
    
private:
    static const char* jsTemplate;

};
#endif

