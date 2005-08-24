/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  Contact:	Martin Karsten
		TU Darmstadt, FG KOM
		Merckstr. 25
		64283 Darmstadt
		Germany
		Martin.Karsten@KOM.tu-darmstadt.de

  Other copyrights might apply to parts of this package and are so
  noted when applicable. Please see file COPYRIGHT.other for details.

****************************************************************************/
%{
#include "parser.tab.h"
#include "RSVP_System.h"
extern void yyerror(const char*);
String yy_string;
uint32 yy_int;
ieee32float_p yy_float;
%}

%option yylineno

%%

"interface"		return INTERFACE;
"refresh"		return REFRESH;
"encap"			return ENCAP;
"virt"			return VIRT;
"disable"		return DISABLE;
"tc"			return TC_C;
"none"			return NONE;
"cbq"			return CBQ_C;
"hfsc"			return HFSC_C;
"rate"			return RATE;
"api"			return API_C;
"route"			return ROUTE;
"peer"			return PEER;
"timer"			return TIMER;
"session_hash"		return SESSION_HASH;
"api_hash"		return API_HASH;
"id_hash_send"		return ID_HASH_SEND;
"id_hash_recv"		return ID_HASH_RECV;
"rapid"			return RAPID;
"loss"			return LOSS;
"mpls_all"		return MPLS_ALL;
"nompls_all"		return NOMPLS_ALL;
"mpls"			return MPLS_C;
"nompls"		return NOMPLS;
"label_hash"		return LABEL_HASH;
"explicit_route"	return EXPLICIT_ROUTE;
"list_alloc"		return LIST_ALLOC;
"sb_alloc"		return SB_ALLOC;


[0-9][0-9]?[0-9]?\.[0-9][0-9]?[0-9]?\.[0-9][0-9]?[0-9]?\.[0-9][0-9]?[0-9]?	{ yy_string = yytext; return IP_ADDRESS; }

[0-9]+\.[0-9]+		{ yy_float = convertStringToFloat(yytext); return FLOAT; }
[0-9]+  		{ yy_int = convertStringToInt(yytext); return INTEGER; }


[A-Za-z0-9\-\+]+	{ yy_string = yytext; return STRING; }

[ \t\n]+	;	/* ignore whitespace */
#.+\n		;	/* ignore comment: from '#' to end of line */
.		yyerror("Unknown character");

%%

int yywrap(void) {
    return 1;
}
