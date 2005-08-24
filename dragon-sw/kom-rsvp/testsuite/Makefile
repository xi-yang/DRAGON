include ../MakeBase
CONFIGS=$(wildcard conf*)
RSVP_CONFIGS=$(addprefix rsvp.,$(CONFIGS))

OS_NAME:=$(shell uname -s)

conf: $(RSVP_CONFIGS) builddir.conf

builddir.conf:
	@echo $(RSVP_BUILD_DIR) > builddir.conf

$(RSVP_CONFIGS): rsvp.conf%: conf%
	@i=`echo $@ | cut -b10-` ; echo generating rsvp.conf$$i ; \
	./generateConfFile $$i > $@

confclean cc:
	rm -f rsvp.conf* -f builddir.conf

ifeq ($(OS_NAME),SunOS)
RSVPD_JOBS=ps -u $$USER | fgrep RSVPD | fgrep -v fgrep
API_JOBS=ps -u $$USER | fgrep api | fgrep -v fgrep
else
ifeq ($(OS_NAME),FreeBSD)
RSVPD_JOBS=ps -x | fgrep RSVPD | fgrep -v fgrep
API_JOBS=ps -x | fgrep api | fgrep -v fgrep
else
RSVPD_JOBS=ps -C RSVPD | tail +2
API_JOBS=ps -C api | tail +2
endif
endif
RSVPD_JOBS+= | awk '{print $$1}' | sort
API_JOBS+= | awk '{print $$1}' | sort

kill:
	@kill -SIGINT `$(RSVPD_JOBS); $(API_JOBS)` 2>/dev/null; exit 0

xkill: kill
	@kill -SIGKILL `$(RSVPD_JOBS); $(API_JOBS)` 2>/dev/null; exit 0

start:
	@kill -SIGUSR1 `$(API_JOBS)` 2>/dev/null; exit 0

count:
	@echo RSVPD: `$(RSVPD_JOBS)|wc|awk '{print $$2}'`
	@echo api: `$(API_JOBS)|wc|awk '{print $$2}'`

suiteclean sc:
	rm -f rsvp.log.* api.log.* rsvp.err.* */rsvp.log.* */api.log.* */rsvp.err.*

clean: confclean suiteclean

### old stuff ####

oldkill:
ifeq ($(OS_NAME),SunOS)
	@JOBS=`ps -u $$USER | fgrep RSVPD | fgrep -v fgrep | awk '{print $$1}' | sort`;\
	kill -2 $$JOBS 2>/dev/null; sleep 1;\
	JOBS=`ps -u $$USER | fgrep api | fgrep -v fgrep | awk '{print $$1}' | sort`;\
	kill -2 $$JOBS 2>/dev/null; exit 0;
else
ifeq ($(OS_NAME),FreeBSD)
	@JOBS=`ps -x | fgrep RSVPD | fgrep -v fgrep | awk '{print $$1}' | sort`;\
	kill -2 $$JOBS 2>/dev/null; sleep 1;\
	JOBS=`ps -x | fgrep api | fgrep -v fgrep | awk '{print $$1}' | sort`;\
	kill -2 $$JOBS 2>/dev/null; exit 0
else
	@JOBS=`ps -C RSVPD | tail +2 | awk '{print $$1}' | sort`;\
	for i in $$JOBS; do kill -2 $$i 2>/dev/null; done ; sleep 1;\
	JOBS=`ps -C api | tail +2 | awk '{print $$1}' | sort`;\
	for i in $$JOBS; do kill -2 $$i 2>/dev/null; done ; exit 0
endif
endif

oldxkill:
ifeq ($(OS_NAME),SunOS)
	@JOBS=`ps -u $$USER | fgrep RSVPD | fgrep -v fgrep | awk '{print $$1}' ; ps -u $$USER | fgrep api | fgrep -v fgrep | awk '{print $$1}' | sort`;\
	kill -9 $$JOBS 2>/dev/null; exit 0
else
ifeq ($(OS_NAME),FreeBSD)
	@JOBS=`ps -x | fgrep RSVPD | fgrep -v fgrep | awk '{print $$1}' ; ps -x | fgrep api | fgrep -v fgrep | awk '{print $$1}'`;\
	kill -9 $$JOBS 2>/dev/null; exit 0
else
	@JOBS=`ps -C RSVPD | tail +2 | awk '{print $$1}' | sort ; ps -C api | tail +2 | awk '{print $$1}' | sort`;\
	for i in $$JOBS; do kill -9 $$i 2>/dev/null; done ; exit 0
endif
endif

oldcount:
ifeq ($(OS_NAME),SunOS)
	@echo RSVPD: `ps -u $$USER | fgrep RSVPD | fgrep -v fgrep | awk '{print $$1}'|wc|awk '{print $$2}'`
	@echo api: `ps -u $$USER | fgrep api | fgrep -v fgrep | awk '{print $$1}'|wc|awk '{print $$2}'`
else
ifeq ($(OS_NAME),FreeBSD)
	@echo RSVPD: `ps -x | fgrep RSVPD | fgrep -v fgrep | awk '{print $$1}'|wc|awk '{print $$2}'`
	@echo api: `ps -x | fgrep api | fgrep -v fgrep | awk '{print $$1}'|wc|awk '{print $$2}'`
else
	@echo RSVPD: `ps -C RSVPD | tail +2 | wc | awk '{print $$1}'`
	@echo api: `ps -C api | tail +2 | wc | awk '{print $$1}'`
endif
endif
