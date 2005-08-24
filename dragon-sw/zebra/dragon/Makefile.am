## Process this file with Automake to create Makefile.in

RSVP_ROOT = ../../kom-rsvp
RSVP_LIB    =   ${RSVP_ROOT}/lib
RSVP_INC   =   ${RSVP_ROOT}/include

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(RSVP_INC)
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"

LIBS = @LIBS@ @CURSES@ @LIBPAM@

INSTALL_SDATA=@INSTALL@ -m 600

bin_PROGRAMS = dragon

dragon_SOURCES = dragon_main.c dragon_vty.c dragon_packet.c
noinst_HEADERS = dragond.h
dragon_LDADD = ../lib/libzebra.a -L$(RSVP_LIB) -lc -lstdc++ -lRSVP

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
