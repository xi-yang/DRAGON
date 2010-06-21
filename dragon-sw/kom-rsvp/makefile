SHORTCUT_TARGETS = standard performance emulation testsuite validation ns2sim

.PHONY : warning $(SHORTCUT_TARGETS) ac mrproper tgz

-include Makefile

warning:
	@[ -f $(RSVP_BUILD_DIR)/Makefile ] && (${MAKE} -C $(RSVP_BUILD_DIR) || exit 0) ||\
	(echo && \
	echo "The proper way to build this package is to run 'configure', which then" && \
	echo "produces a complete Makefile. Afterwards, run '${MAKE} dep' and then '${MAKE} all'." && \
	echo && \
	echo "You can also create a separate directory, e.g., if you plan to compile for" && \
	echo "multiple platforms and run '<path>/configure' from the new directory." && \
	echo && \
	echo "Please run 'configure --help' for configuration help." && \
	echo && \
	echo "The following shortcut targets are available in this makefile:" && \
	echo "- 'standard' to build a standard RSVP daemon" && \
	echo "- 'performance' to build a performance-optimized RSVP daemon" && \
	echo "- 'emulation' to build in emulation mode" && \
	echo "- 'testsuite' to build the test suite" && \
	echo "- 'validation' to build and run the validation suite" && \
	echo "- 'ns2sim' to build the ns-2 library (KOM local configuration)" && \
	echo)

standard: configure
	./configure --disable-java-api --disable-onepass --disable-mpls --disable-memopt --with-dmalloc=/usr/local && ${MAKE} dep && ${MAKE} all

performance: configure
	./configure --disable-java-api --enable-performance --disable-checking --disable-logging && ${MAKE} dep && ${MAKE} all

emulation: configure
	./configure --enable-virt --disable-real --with-dmalloc=/usr/local && ${MAKE} dep && ${MAKE} all

testsuite: configure
	./configure --disable-java-api --enable-virt --disable-real --enable-active-api --disable-api-clients --with-dmalloc=/usr/local && ${MAKE} dep && ${MAKE} all

validation: configure
	./configure --disable-java-api --enable-virt --disable-real --enable-active-api --disable-api-clients --disable-timers --with-dmalloc=/usr/local && ${MAKE} dep && ${MAKE} all
	export RSVP_BUILD_DIR=`pwd` ; cd testsuite ; ./validate

ns2sim: configure
	@echo ; echo configure for ns-allinone in /work/kalli/ns-allinone-2.1b9 ; echo
	./configure --with-ns2dir=/work/kalli/ns-allinone-2.1b9 --disable-virt --disable-real && ${MAKE} ns2prep && ${MAKE} ns2dep && ${MAKE} dep && ${MAKE} all

configure: configure.in $(wildcard autoconf_in/*)
	autoconf -I autoconf_in && autoheader -I autoconf_in
	rm -rf autom4te.cache

ac: configure

mrproper:
	test -f Makefile && ${MAKE} clean distclean || exit 0
	${MAKE} -C testsuite clean
	${MAKE} -C altq clean
	${MAKE} -C ns2 clean
	rm -f rsvp.tgz;\
	rm -f RSVP_config.h.in configure;\
	find . -name "*~" -exec rm -rf {} \; ;\
	find . -name "*jofel*" -exec rm -rf {} \;
	find . -name "core" -exec rm -rf {} \;
	find . -name "*.core" -exec rm -rf {} \;

tgz:
	rm -f rsvp.tgz
	CURRDIR=`pwd`;DIR=`basename $$CURRDIR`;\
	cd ..; find $$DIR -name CVS > $$DIR/archive-exclude;\
	echo archive-exclude >> $$DIR/archive-exclude;\
	tar cvf - -X $$DIR/archive-exclude -FF $$DIR | gzip > $$DIR.tgz;\
	rm -f $$DIR/archive-exclude;\
	mv $$DIR.tgz $$CURRDIR/$$DIR.tgz

clean distclean:
