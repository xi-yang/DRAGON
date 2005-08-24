include ../MakeBase

NS2_FILES=$(subst ns2_files,,$(shell find ns2_files -type f|fgrep -v CVS))
TARGET_FILES=$(addprefix $(NS2_DIR_NS2),$(NS2_FILES))

.PHONY: prep all clean test ns2_patches nsMakefile.in nsMakefile nam_netmodel

all:

test:
	@echo NS2_DIR: $(NS2_DIR)
	@echo NS2_DIR_NS2: $(NS2_DIR_NS2)
	@echo NS2_VERSION: $(NS2_VERSION)
	@echo NAM_VERSION: $(NAM_VERSION)
	@echo NS2_FILES: $(NS2_FILES)
	@echo TARGET_FILES: $(TARGET_FILES)

ifeq ($(strip $(NS2_DIR)),)

prep:
	@echo "You must configure this package first. Please call 'configure' in the main"
	@echo "directory with the options '--disable-virt --disable-real' and set the path"
	@echo "to the ns-allinone package by using the option '--with-ns2dir'"

else

prep: $(TARGET_FILES) ns2_patches nsMakefile.in nsMakefile nam_netmodel

$(TARGET_FILES) : $(NS2_DIR_NS2)/%: ns2_files/%
	[ -d `dirname $@` ] || mkdir -p `dirname $@`
	cp $^ $@

nsMakefile.in:
	@[ -f $(NS2_DIR_NS2)/Makefile.in ] && fgrep -s ns-rsvp.tcl $(NS2_DIR_NS2)/Makefile.in >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	echo patching $(NS2_DIR_NS2)/Makefile.in &&\
	echo "CFLAGS+=-DNS2 -O2 -g" >> $(NS2_DIR_NS2)/Makefile.in &&\
	echo "INCLUDES+=-I$(INC_DIR) $(NEWMAT_INCLUDE)" >> $(NS2_DIR_NS2)/Makefile.in &&\
	echo "LIB+=-O2 -L$(LIB_DIR) -lRSVP_ns2 $(NEWMAT_LIB)" >> $(NS2_DIR_NS2)/Makefile.in &&\
	echo "\$$(NS) : $(LIB_DIR)/libRSVP_ns2.a" >> $(NS2_DIR_NS2)/Makefile.in &&\
	sed -e "/BUILD_NSE =/s//#BUILD_NSE =/" $(NS2_DIR_NS2)/Makefile.in >> /tmp/jofel.$$$$ && mv /tmp/jofel.$$$$ $(NS2_DIR_NS2)/Makefile.in &&\
	sed -e "/simulator.o /s//simulator.o rsvp\/rsvp-tab.o rsvp\/rsvp-agent.o rsvp\/rsvp-daemon-agent.o rsvp\/rsvp-api-agent.o rsvp\/flowgenerator.o /" $(NS2_DIR_NS2)/Makefile.in >> /tmp/jofel.$$$$ && mv /tmp/jofel.$$$$ $(NS2_DIR_NS2)/Makefile.in &&\
	sed -e "/NS_TCL_LIB = /s//NS_TCL_LIB = tcl\/lib\/ns-rsvp.tcl tcl\/lib\/ns-cbq.tcl tcl\/lib\/ns-flowgen.tcl /" $(NS2_DIR_NS2)/Makefile.in >> /tmp/jofel.$$$$ && mv /tmp/jofel.$$$$ $(NS2_DIR_NS2)/Makefile.in; else true; fi

nsMakefile:
	@[ -f $(NS2_DIR_NS2)/Makefile ] && fgrep -s ns-rsvp.tcl $(NS2_DIR_NS2)/Makefile >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	echo patching $(NS2_DIR_NS2)/Makefile &&\
	echo "CFLAGS+=-DNS2 -O2 -g" >> $(NS2_DIR_NS2)/Makefile &&\
	echo "INCLUDES+=-I$(INC_DIR) $(NEWMAT_INCLUDE)" >> $(NS2_DIR_NS2)/Makefile &&\
	echo "LIB+=-L$(LIB_DIR) -lRSVP_ns2 $(NEWMAT_LIB)" >> $(NS2_DIR_NS2)/Makefile &&\
	echo "\$$(NS) : $(LIB_DIR)/libRSVP_ns2.a" >> $(NS2_DIR_NS2)/Makefile &&\
	sed -e "/BUILD_NSE =/s//#BUILD_NSE =/" $(NS2_DIR_NS2)/Makefile >> /tmp/jofel.$$$$ && mv /tmp/jofel.$$$$ $(NS2_DIR_NS2)/Makefile &&\
	sed -e "/simulator.o /s//simulator.o rsvp\/rsvp-tab.o rsvp\/rsvp-agent.o rsvp\/rsvp-daemon-agent.o rsvp\/rsvp-api-agent.o rsvp\/flowgenerator.o /" $(NS2_DIR_NS2)/Makefile >> /tmp/jofel.$$$$ && mv /tmp/jofel.$$$$ $(NS2_DIR_NS2)/Makefile &&\
	sed -e "/NS_TCL_LIB = /s//NS_TCL_LIB = tcl\/lib\/ns-rsvp.tcl tcl\/lib\/ns-cbq.tcl tcl\/lib\/ns-flowgen.tcl /" $(NS2_DIR_NS2)/Makefile >> /tmp/jofel.$$$$ && mv /tmp/jofel.$$$$ $(NS2_DIR_NS2)/Makefile; else true; fi

nam_netmodel:
	@[ -f $(NS2_DIR)/$(NAM_VERSION)/netmodel.cc ] && fgrep -s "memcpy((char*)p" $(wildcard $(NS2_DIR)/$(NAM_VERSION)/netmodel.cc) >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	patch $(wildcard $(NS2_DIR)/$(NAM_VERSION)/netmodel.cc) < $(wildcard patches-$(NAM_VERSION)/netmodel.cc.patch); else true; fi

ns2_patches:
	@[ -f $(NS2_DIR_NS2)/common/packet.h ] && fgrep -s PT_RSVP_UNKNOWN $(NS2_DIR_NS2)/common/packet.h >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	patch $(NS2_DIR_NS2)/common/packet.h < patches-$(NS2_VERSION)/packet.h.patch; else true; fi
	@[ -f $(NS2_DIR_NS2)/common/connector.cc ] && fgrep -s PT_RSVP_UNKNOWN $(NS2_DIR_NS2)/common/connector.cc >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	patch $(NS2_DIR_NS2)/common/connector.cc < patches-$(NS2_VERSION)/connector.cc.patch; else true; fi
	@[ -f $(NS2_DIR_NS2)/queue/drop-tail.h ] && fgrep -s timestamp $(NS2_DIR_NS2)/queue/drop-tail.h >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	patch $(NS2_DIR_NS2)/queue/drop-tail.h < patches-$(NS2_VERSION)/drop-tail.h.patch; else true; fi
	@[ -f $(NS2_DIR_NS2)/queue/drop-tail.cc ] && fgrep -s timestamp $(NS2_DIR_NS2)/queue/drop-tail.cc >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	patch $(NS2_DIR_NS2)/queue/drop-tail.cc < patches-$(NS2_VERSION)/drop-tail.cc.patch; else true; fi
	@[ -f $(NS2_DIR_NS2)/tcl/lib/ns-default.tcl ] && fgrep -s RSVP $(NS2_DIR_NS2)/tcl/lib/ns-default.tcl >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	patch $(NS2_DIR_NS2)/tcl/lib/ns-default.tcl < patches-$(NS2_VERSION)/ns-default.tcl.patch; else true; fi
	@[ -f $(NS2_DIR_NS2)/tcl/lib/ns-lib.tcl ] && fgrep -s RSVP $(NS2_DIR_NS2)/tcl/lib/ns-lib.tcl >/dev/null 2>&1; if [ $$? -ne 0 ]; then\
	patch $(NS2_DIR_NS2)/tcl/lib/ns-lib.tcl < patches-$(NS2_VERSION)/ns-lib.tcl.patch; else true; fi

endif
