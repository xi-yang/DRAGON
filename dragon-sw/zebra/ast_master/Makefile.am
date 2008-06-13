## Process this file with Automake to create Makefile.in

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I/usr/include -I/usr/local/include -I/usr/include/libxml2 -I/usr/local/include/libxml2 
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
XML_LIBS = -L/usr/lib -L/usr/local/lib

LIBS = @LIBS@ @CURSES@ @LIBPAM@ $(XML_LIBS)

INSTALL_SDATA=@INSTALL@ -m 600

bin_PROGRAMS = ast_master node_agent
noinst_LIBRARIES = libastb.a

noinst_HEADERS = adt.h ast_master.h dragon_app.h
ast_master_SOURCES = main.c ast_master_vty.c dragon_app.c
ast_master_LDADD =  libastb.a ../lib/libzebra.a  $(XML_LIBS) -lxml2 -lm
node_agent_SOURCES = node_agent.c dragon_app.c
node_agent_LDADD =  libastb.a ../lib/libzebra.a $(XML_LIBS) -lxml2 -lm 
libastb_a_SOURCES = adt.c ast_master.c local_id_cfg.c 

sysconf_DATA = ast_master.conf.sample
# RC_SCRIPT = ast_master.sh

ASTCONFDIR = /usr/local/ast_file
EXTRA_DIST = service_template.xml \
             res_def.xml \
             xml_schema/resource_def.rng \
             xml_schema/dragon_link.rng \
             xml_schema/dragon_node_pc.rng \
             xml_schema/setup_req.rng

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
	$(mkinstalldirs) $(ASTCONFDIR) $(ASTCONFDIR)/xml_schema
	@list='$(EXTRA_DIST)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_SDATA) $(srcdir)/$$p $(ASTCONFDIR)/$$p"; \
	    $(INSTALL_SDATA) $(srcdir)/$$p $(ASTCONFDIR)/$$p; \
	  else if test -f $$p; then \
	    echo " $(INSTALL_SDATA) $$p $(ASTCONFDIR)/$$p"; \
	    $(INSTALL_SDATA) $$p $(ASTCONFDIR)/$$p; \
	  fi; fi; \
	done
#	@$(INSTALL) $(RC_SCRIPT) $(DESTDIR)$(bindir)/$(RC_SCRIPT);
