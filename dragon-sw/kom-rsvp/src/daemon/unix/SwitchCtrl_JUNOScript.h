/****************************************************************************

SwitchCtrl_Session_JuniperEX module auxillary source file SwitchCtrl_JUNOScript.h
Created by Xi Yang @ 03/16/2009
To be incorporated into KOM-RSVP-TE package

****************************************************************************/
#ifndef _SwitchCtrl_JUNOScript_H_
#define _SwitchCtrl_JUNOScript_H_

using namespace std;

#include "RSVP_String.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

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

#endif

