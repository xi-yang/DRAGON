## Process this file with Automake to create Makefile.in

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I/usr/include -I/usr/local/include -I/usr/include/libxml2 -I/usr/local/include/libxml2 
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
XML_LIBS = -L/usr/lib -L/usr/local/lib

LIBS = @LIBS@ @CURSES@ @LIBPAM@ $(XML_LIBS)

INSTALL_SDATA=@INSTALL@ -m 600

bin_PROGRAMS = ast_master node_agent astb
noinst_LIBRARIES = libastb.a

noinst_HEADERS = adt.h ast_master.h
ast_master_SOURCES = adt.c ast_master.c main.c
ast_master_LDADD =  ../lib/libzebra.a  $(XML_LIBS) -lcrypt -lxml2 -lz -lm
node_agent_SOURCES = node_agent.c adt.c ast_master.c
node_agent_LDADD =  ../lib/libzebra.a $(XML_LIBS) -lcrypt -lxml2 -lz -lm
astb_SOURCES = adt.c ast_master.c astb.c
astb_LDADD =  ../lib/libzebra.a  $(XML_LIBS) -lcrypt -lxml2 -lz -lm
libastb_a_SOURCES = adt.c ast_master.c 

sysconf_DATA = ast_master.conf.sample

RC_SCRIPT = ast_master.sh

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
#	@$(INSTALL) $(RC_SCRIPT) $(DESTDIR)$(bindir)/$(RC_SCRIPT);
