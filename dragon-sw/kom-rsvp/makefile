.PHONY : rsvpd daemon clients java common api extern lib \
	ns2 ns2prep ns2test ns2dep ns2clean ns2code ns2lib \
	depend dep gen genclean test clean distclean install

include MakeConfig

VPATH := $(VPATH_DIR)

ifeq ($(BUILD_NS2),yes)
all : ns2
else
all : rsvpd clients java
endif

$(LIB_DIR) $(LIBJAVA_DIR) $(BIN_DIR) $(GEN_DIR):
	test -d $@ || mkdir -p $@

common daemon extern main :
	${MAKE} -C $(SRC_DIR)/$@ all

api :
ifeq ($(BUILD_API),yes)
	${MAKE} -C $(SRC_DIR)/$@ all
endif

common :

daemon : common $(BIN_DIR)

rsvpd : api daemon
	${MAKE} -C $(SRC_DIR)/main all

lib : common api $(LIB_DIR)
ifeq ($(BUILD_API),yes)
ifeq ($(BUILD_NS2),no)
	${MAKE} libRSVP.a
#	${MAKE} libRSVP.so
endif
endif

clients : lib extern $(BIN_DIR)
	${MAKE} -C $(SRC_DIR)/clients all
ifeq ($(BUILD_TESTSUITE),yes)
	${MAKE} -C /Users/xyang/Work/DRAGON/dragon-sw/kom-rsvp/testsuite conf
endif

ifeq ($(BUILD_JAVA_API),yes)
java : api common $(LIBJAVA_DIR)
	${MAKE} -C $(SRC_DIR)/java all
	${MAKE} -C $(SRC_DIR)/java JRSVP.jar
	${MAKE} libJRSVP.so
else
java :
endif

ifeq ($(BUILD_NS2),yes)
ns2dep :
	cd $(NS2_DIR_NS2) && ${MAKE} depend

ns2clean :
	cd $(NS2_DIR_NS2) && ${MAKE} clean

ns2prep :
	${MAKE} -C $(BASE_DIR)/ns2 prep

ns2test :
	${MAKE} -C $(BASE_DIR)/ns2 test

ns2code :
	${MAKE} -C $(SRC_DIR)/ns2 all

ns2lib : ns2code daemon api clients $(LIB_DIR)
	${MAKE} libRSVP_ns2.a

ns2 : ns2prep ns2lib
	cd $(NS2_DIR_NS2) && ${MAKE}
	@echo && echo The ns-2 executable is at $(NS2_DIR_NS2)/ns
	@echo "Suggestion: alias ns='$(NS2_DIR_NS2)/ns'" && echo
else
ns2prep ns2 :
endif

depend dep test : ns2prep
	for i in $(SUB_DIRS); do ${MAKE} -C $(SRC_DIR)/$$i $@; if [ $$? -ne 0 ]; then break; fi; done

clean :
	rm -f $(LIB_DIR)/libRSVP.a $(LIB_DIR)/libRSVP.so $(LIBJAVA_DIR)/libJRSVP.so $(LIBJAVA_DIR)/JRSVP.jar
	for i in $(SUB_DIRS); do ${MAKE} -C $(SRC_DIR)/$$i $@; if [ $$? -ne 0 ]; then break; fi; done
ifeq ($(BUILD_TESTSUITE),yes)
	${MAKE} -C /Users/xyang/Work/DRAGON/dragon-sw/kom-rsvp/testsuite suiteclean
endif

gen :
	for i in $(SUB_DIRS); do ${MAKE} -C $(SRC_DIR)/$$i $@; if [ $$? -ne 0 ]; then break; fi; done

genclean :
	for i in $(SUB_DIRS); do ${MAKE} -C $(SRC_DIR)/$$i $@; if [ $$? -ne 0 ]; then break; fi; done

libRSVP.a : $(LIB_OBJECTS)
	rm -f $(LIB_DIR)/$@
	$(AR) ru $(LIB_DIR)/$@ $^
	$(RANLIB) $(LIB_DIR)/$@

libRSVP.so : $(LIB_OBJECTS)
	rm -f $(LIB_DIR)/$@
	$(SHARED_LD) -o $(LIB_DIR)/$@ $^

libJRSVP.so : $(JAVA_OBJECTS) $(LIB_OBJECTS)
ifeq ($(BUILD_JAVA_API),yes)
	rm -f $(LIBJAVA_DIR)/$@
	$(SHARED_LD) -o $(LIBJAVA_DIR)/$@ $^
endif

ifeq ($(BUILD_NS2),yes)
libRSVP_ns2.a : $(LIB_OBJECTS) $(DAEMON_OBJECTS) $(NS2_OBJECTS)
	rm -f $(LIB_DIR)/$@
	$(AR) ru $(LIB_DIR)/$@ $^
	$(RANLIB) $(LIB_DIR)/$@
endif

install :
	@for i in $(BIN_DIR)/* ; do\
	  echo installing `basename $$i` into $(bindir);\
	  test -d $(bindir) || mkdir -p $(bindir);\
	  $(INSTALL) -m 755 $$i $(bindir);\
	done
	@echo installing RSVPD.conf.sample into $(etcdir)
	@test -d $(etcdir) || mkdir -p $(etcdir)
	@$(INSTALL) -m 755 $(EXAMPLES_DIR)/RSVPD.conf.sample $(etcdir)
ifeq ($(BUILD_JAVA_API),yes)
	@echo installing libJRSVP.so into $(libdir)
	@test -d $(libdir) || mkdir -p $(libdir)
	@$(INSTALL) -m 755 $(LIBJAVA_DIR)/libJRSVP.so $(libdir)
	@echo installing JRSVP.jar files into $(libdir)
	@test -d $(libdir) || mkdir -p $(libdir)
	@$(INSTALL) -m 644 $(LIBJAVA_DIR)/java/JRSVP.jar $(libdir)
endif
ifeq ($(BUILD_API),yes)
	@echo installing libRSVP.a into $(libdir)
	@test -d $(libdir) || mkdir -p $(libdir)
	@$(INSTALL) -m 644 $(LIB_DIR)/libRSVP.a $(libdir)
	@echo installing libRSVP.so into $(libdir)
	@test -d $(libdir) || mkdir -p $(libdir)
	@test -x $(LIB_DIR)/libRSVP.so && $(INSTALL) -m 755 $(LIB_DIR)/libRSVP.so $(libdir) || true
	@echo installing headers into $(incdir)
	@test -d $(incdir) || mkdir -p $(incdir)
	@echo "#include \"RSVP_API_Upcall.h\"" > /tmp/confIncludeTest.cc;\
	files=`$(CXX) $(CXXFLAGS) -MM /tmp/confIncludeTest.cc`;\
	rm -f /tmp/confIncludeTest.cc;\
	files=`ls $$files 2>/dev/null |\
	fgrep -v confIncludeTest | fgrep -v dmalloc | fgrep -v SystemCallCheck`;\
	for i in $$files; do $(INSTALL) -m 644 $$i $(incdir)/`basename $$i`; done
endif
ifeq ($(BUILD_NS2),yes)
	@echo installing libRSVP_ns2.a into $(libdir)
	@test -d $(libdir) || mkdir -p $(libdir)
	@$(INSTALL) -m 644 $(LIB_DIR)/libRSVP_ns2.a $(libdir)
endif

distclean : clean
	rm -rf $(BIN_DIR) $(INC_DIR) $(OBJECT_DIR) $(GEN_DIR) $(LIB_DIR) $(LIBJAVA_DIR)
ifeq ($(BUILD_TESTSUITE),yes)
	${MAKE} -C /Users/xyang/Work/DRAGON/dragon-sw/kom-rsvp/testsuite clean
endif
	rm -f config.status config.log config.cache RSVP_config.h
	rm -f MakeConfig Makefile

$(SHORTCUT_TARGETS) : distclean
