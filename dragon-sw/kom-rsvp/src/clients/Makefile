include ../../MakeBase
MODULES=clients

ifeq ($(BUILD_TG),yes)
CLIENTS+=tg
endif
ifeq ($(BUILD_CLIENTS),yes)
CLIENTS+=systemLoad recvapi sendapi manySenders manyReceivers # sender receiver sendVideo
MTVP_FOUND:=$(shell type mtvp >/dev/null 2>&1 ; echo $$?)
ifeq ($(MTVP_FOUND),0)
PATH_TO_MTVP:=$(shell type mtvp | cut -f3 -d' ')
#CLIENTS+=receiveVideo
endif
endif

ifeq ($(BUILD_ACTIVE_API),yes)
CLIENTS+=api
endif

ADD_ONS:=# selectTest

OBJECTS=$(addsuffix .o,$(CLIENTS))
COBJECTS=

ifeq ($(BUILD_NS2),yes)
OBJECTS+=CommandParser.o tg_classes.o
else
EXTRA_OBJECTS+=CommandParser.o tg_classes.o
endif

receiveVideo.o-CXXFLAGS=-DPATH_TO_MTVP=\"$(PATH_TO_MTVP)\"
tg-ADD_OBJECTS=$(OBJECT_DIR)/../daemon/RSVP_BaseTimer.o\
	$(OBJECT_DIR)/tg_classes.o\
	$(OBJECT_DIR)/tg_parser.tab.o\
	$(OBJECT_DIR)/tg_parser.lex.o
api-ADD_OBJECTS+=$(OBJECT_DIR)/CommandParser.o
receiveVideo-ADD_LIBS=$(PTHREAD_LIB)

tg: $(OBJECT_DIR)/../daemon/RSVP_BaseTimer.o tg_classes.o tg_parser.tab.o tg_parser.lex.o

api: CommandParser.o

$(OBJECT_DIR)/../daemon/RSVP_BaseTimer.o:
	$(MAKE) -C ../daemon RSVP_BaseTimer.o

ifeq ($(BUILD_TG),yes)
GEN_OBJECTS=tg_parser.lex.o tg_parser.tab.o
GEN_FILES=$(GEN_DIR)/tg_parser.tab.cc $(GEN_DIR)/tg_parser.lex.cc $(GEN_DIR)/tg_parser.tab.h $(INC_DIR)/tg_parser.tab.h
endif

include ../../MakeRecipes

$(GEN_DIR)/tg_parser.tab.cc : unix/tg_parser.y
	$(YACC) -d -b $(GEN_DIR)/tg_parser unix/tg_parser.y && mv $(GEN_DIR)/tg_parser.tab.c $(GEN_DIR)/tg_parser.tab.cc

$(GEN_DIR)/tg_parser.tab.h : $(GEN_DIR)/tg_parser.tab.cc

$(INC_DIR)/tg_parser.tab.h: $(GEN_DIR)/tg_parser.tab.h
	[ -f $(INC_DIR)/tg_parser.tab.h ] || ln -s $(GEN_DIR)/tg_parser.tab.h $(INC_DIR)

$(GEN_DIR)/tg_parser.lex.cc : unix/tg_parser.l
	$(LEX) -o$(GEN_DIR)/tg_parser.lex.cc unix/tg_parser.l

selectTest: % : unix/%.cpp
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/$@ $^ $(LIBS)
