include ../../MakeBase
MODULES=daemon

GEN_OBJECTS=lex.yy.o parser.tab.o
GEN_FILES=$(GEN_DIR)/lex.yy.cc $(GEN_DIR)/parser.tab.cc $(GEN_DIR)/parser.tab.h $(INC_DIR)/parser.tab.h

include ../../MakeRecipes

$(GEN_DIR)/parser.tab.cc : unix/parser.y
	$(YACC) -d -b $(GEN_DIR)/parser unix/parser.y && mv $(GEN_DIR)/parser.tab.c $(GEN_DIR)/parser.tab.cc

$(GEN_DIR)/parser.tab.h : $(GEN_DIR)/parser.tab.cc

$(INC_DIR)/parser.tab.h : $(GEN_DIR)/parser.tab.h
	[ -f $(INC_DIR)/parser.tab.h ] || ln -s $(GEN_DIR)/parser.tab.h $(INC_DIR)

$(GEN_DIR)/lex.yy.cc : unix/parser.l
	$(LEX) -o$(GEN_DIR)/lex.yy.cc unix/parser.l

.PHONY: VersionString.o

VersionString.o: unix/VersionString.cxx
	$(CXX) -o $(OBJECT_DIR)/$@ -DBUILD_DATE="\"build date: $(shell date)\"" -c $^
