## Process this file with Automake to create Makefile.in

RSVP_ROOT = ../../kom-rsvp
RSVP_LIB    =   ${RSVP_ROOT}/lib
RSVP_INC   =   ${RSVP_ROOT}/include
XML_LIBS = -L/usr/lib -L/usr/local/lib

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(RSVP_INC) -I/usr/include -I/usr/local/include -I/usr/include/libxml2 -I/usr/local/include/libxml2
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"

LIBS = @LIBS@ @CURSES@ @LIBPAM@

INSTALL_SDATA=@INSTALL@ -m 600

bin_PROGRAMS = dragon mon_apiclient

dragon_SOURCES = ../ast_master/dragon_app.c dragon_main.c dragon_version.c dragon_vty.c dragon_packet.c dragon_xml.c dragon_mon_apiserver.c 
noinst_HEADERS = dragond.h
dragon_LDADD = ../ast_master/libastb.a ../lib/libzebra.a -L$(RSVP_LIB) -lc -lstdc++ -lRSVP $(XML_LIBS) -lxml2

mon_apiclient_SOURCES = ../ast_master/dragon_app.c dragon_version.c dragon_vty.c dragon_packet.c dragon_xml.c dragon_mon_apiserver.c dragon_mon_apiclient.c
mon_apiclient_LDADD = ../ast_master/libastb.a ../lib/libzebra.a -lc -lstdc++ $(XML_LIBS) -lxml2

sysconf_DATA = dragon.conf.sample

RC_SCRIPT = dragon.sh

install-sysconfDATA: $(sysconf_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	@list='$(sysconf_DATA)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_SDATA) $(srcdir)/$$p $(DESTDIR)$(sysconfdir)/$$p"; \
	    $(INSTALL_SDATA) $(srcdir)/$$p $(DESTDIR)$(sysconfdir)/$$p; \
	  else if test -f $$p; then \
	    echo " $(INSTALL_SDATA) $$p $(DESTDIR)$(sysconfdir)/$$p"; \
	    $(INSTALL_SDATA) $$p $(DESTDIR)$(sysconfdir)/$$p; \
	  fi; fi; \
	done
	@$(INSTALL) $(RC_SCRIPT) $(DESTDIR)$(bindir)/$(RC_SCRIPT);
