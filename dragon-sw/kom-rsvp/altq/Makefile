.PHONY: prep modules kernel

include ../MakeBase

SYS_ALTQ=/usr/src/sys-altq
ALTQ_HEADER_DIR=/usr/include/altq
KERNEL_NAME=KERNEL

VPATH=$(BIN_DIR)

all:
	@echo "Please specify one of the targets: prep, kernel, modules"

$(SYS_ALTQ)/altq/altq.h: sys-altq/altq/altq.h
	cp $^ $(SYS_ALTQ)/altq

$(SYS_ALTQ)/altq/altq_conf.c: sys-altq/altq/altq_conf.c
	cp $^ $(SYS_ALTQ)/altq

$(SYS_ALTQ)/altq/altq_stamp.h: sys-altq/altq/altq_stamp.h
	cp $^ $(SYS_ALTQ)/altq

$(SYS_ALTQ)/altq/altq_stamp.c: sys-altq/altq/altq_stamp.c
	cp $^ $(SYS_ALTQ)/altq

$(SYS_ALTQ)/conf/files: sys-altq/conf/files.patch
	if ! fgrep -i altq_stamp >/dev/null 2>/dev/null $@; then patch $@ $^; fi

$(SYS_ALTQ)/conf/options: sys-altq/conf/options.patch
	if ! fgrep -i altq_stamp >/dev/null 2>/dev/null $@; then patch $@ $^; fi

$(SYS_ALTQ)/modules/altq/Makefile: sys-altq/makefiles/Makefile.sys.modules.altq
	cp $^  $(SYS_ALTQ)/modules/altq/Makefile

$(SYS_ALTQ)/modules/altq/altq_stamp/Makefile: sys-altq/makefiles/Makefile.sys.modules.altq.altq_stamp
	[ -d $(SYS_ALTQ)/modules/altq/altq_stamp ] || mkdir -p $(SYS_ALTQ)/modules/altq/altq_stamp
	cp $^  $(SYS_ALTQ)/modules/altq/altq_stamp/Makefile

$(ALTQ_HEADER_DIR)/altq_stamp.h: test_altq_header_dir
	[ -L $@ ] || ln -s $(SYS_ALTQ)/altq/altq_stamp.h $(ALTQ_HEADER_DIR)

test_altq_header_dir:
	@[ -d $(ALTQ_HEADER_DIR) ] || { \
	echo "Please create $(ALTQ_HEADER_DIR)"; \
	echo "and copy or softlink the ALTQ header files there."; \
	exit 1; }

prep: $(SYS_ALTQ)/altq/altq.h\
	$(SYS_ALTQ)/altq/altq_conf.c\
	$(SYS_ALTQ)/altq/altq_stamp.h\
	$(SYS_ALTQ)/altq/altq_stamp.c\
	$(SYS_ALTQ)/modules/altq/Makefile\
	$(SYS_ALTQ)/modules/altq/altq_stamp/Makefile\
	$(SYS_ALTQ)/conf/files\
	$(SYS_ALTQ)/conf/options\
	$(ALTQ_HEADER_DIR)/altq_stamp.h

modules: prep
	cd $(SYS_ALTQ)/modules/altq; env MAKEOBJDIRPREFIX=$(SYS_ALTQ)/compile/$(KERNEL_NAME)/modules /usr/bin/make obj all install

kernel: prep
	sh MAKEDEV.altq.new all
	cd $(SYS_ALTQ)/compile/$(KERNEL_NAME); /usr/bin/make depend all install

clean:
