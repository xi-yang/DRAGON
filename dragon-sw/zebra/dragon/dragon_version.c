#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* dragon_version_string() {
    static char dvString[100];
    char* revString = "$Rev$";
    char* dateString = "$LastChangedDate$";
    sprintf(dvString, "Code Revision: %s", revString+strlen("$Rev: "));
    sprintf(dvString + strlen(dvString) - 2, " - Last Changed: %s", dateString+strlen("$LastChangedDate: "));
    dvString[strlen(dvString) - 2] = '\000';
    return dvString;
}
