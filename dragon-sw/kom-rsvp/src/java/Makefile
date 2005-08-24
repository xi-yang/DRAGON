include ../../MakeBase
MODULES=java

JAVA_PATH=de/tud/kom/rsvp
JAVA_PATH_JDK=de.tud.kom.rsvp
GEN_FILES=$(GEN_DIR)/$(JAVA_PATH)/JRSVP.class $(GEN_DIR)/JRSVP.h

JRSVP.o : $(GEN_DIR)/JRSVP.h

$(GEN_DIR)/$(JAVA_PATH)/JRSVP.class: $(wildcard $(JAVA_PATH)/*.java)
	$(JAVAC) -d $(GEN_DIR) -classpath $(GEN_DIR) $(JAVA_PATH)/*.java

$(GEN_DIR)/JRSVP.h: $(GEN_DIR)/$(JAVA_PATH)/JRSVP.class
	$(JAVAH) -classpath $(GEN_DIR) -o $(GEN_DIR)/JRSVP.h $(JAVA_PATH_JDK).JRSVP
	[ -f $(INC_DIR)/JRSVP.h ] || ln -s $(GEN_DIR)/JRSVP.h $(INC_DIR)

JRSVP.jar: $(GEN_DIR)/$(JAVA_PATH)/JRSVP.class
	$(JAR) -cf $(LIBJAVA_DIR)/$@ $(JAVA_PATH)/*.class

include ../../MakeRecipes
