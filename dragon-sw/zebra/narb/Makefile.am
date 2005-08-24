## Automake.am for NARB server

RSVP_ROOT = ../../kom-rsvp
RSVP_LIB    =   ${RSVP_ROOT}/lib
RSVP_INC   =   ${RSVP_ROOT}/include

INCLUDES = -I$(top_srcdir)/lib -I$(top_srcdir)/ -I$(top_srcdir)/dragon/ -I$(RSVP_INC)

lib_LIBRARIES = libospfapiclient.a libnarbapiserver.a 
sbin_PROGRAMS = narb narb_test

libospfapiclient_a_SOURCES = \
	ospf_apiclient.c

libnarbapiserver_a_SOURCES = \
	narb_apiserver.c

## ospfapiheaderdir = $(pkgincludedir)/ospfapi

## ospfapiheader_HEADERS = \
##	ospf_apiclient.h narb_apiserver.h narb_config.h narb_summary.h narb_vty.h narb_rceapi.h ../dragon/dragond.h

narb_SOURCES = \
	narb.c narb_config.c narb_summary.c narb_vty.c narb_rceapi.c $(top_srcdir)/dragon/dragon_vty.c $(top_srcdir)/dragon/dragon_packet.c $(libospfapiclient_a_SOURCES) $(libnarbapiserver_a_SOURCES)

narb_test_SOURCES = \
	narb_test.c narb_config.c narb_summary.c narb_vty.c narb_rceapi.c $(top_srcdir)/dragon/dragon_vty.c $(top_srcdir)/dragon/dragon_packet.c $(libospfapiclient_a_SOURCES) $(libnarbapiserver_a_SOURCES)

narb_LDADD = $(top_srcdir)/ospfd/libospf.a $(top_srcdir)/lib/libzebra.a -L$(RSVP_LIB) -lc -lstdc++ -lRSVP

narb_test_LDADD = $(top_srcdir)/ospfd/libospf.a $(top_srcdir)/lib/libzebra.a -L$(RSVP_LIB) -lc -lstdc++ -lRSVP

