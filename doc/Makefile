# Auto-Generated Documentation

include dir_list.mk

.PHONY: all
all: doc

# =============== Auto-Generated Mailbox Interface ================
MKMBOX=$(SCRIPTS_DIR)/mkmbox.py
MBOX_DEF=$(INCLUDE_DIR)/mbox.def
MBOX_DOC=mailbox_map.md
MBOX_JSON=mailbox_map.json
MBOX_VH=mailbox_map.vh
MBOX_H=mailbox_map.h
OFFSET ?= 0

$(DOC_DIR)/$(MBOX_DOC): $(MBOX_DEF)
	python3 $(MKMBOX) -d $< -o $@

$(DOC_DIR)/$(MBOX_JSON): $(MBOX_DEF)
	python3 $(MKMBOX) --map -d $< -o $@ --offset $(OFFSET)

$(DOC_DIR)/$(MBOX_VH): $(MBOX_DEF)
	python3 $(MKMBOX) --map -d $< -o $@ --offset $(OFFSET)

$(DOC_DIR)/$(MBOX_H): $(MBOX_DEF)
	python3 $(MKMBOX) --map -d $< -o $@ --offset $(OFFSET)

# Deliberately omit $(MBOX_DOC) because it's checked into git
CLEANS += $(DOC_DIR)/$(MBOX_JSON) $(DOC_DIR)/$(MBOX_VH) $(DOC_DIR)/$(MBOX_H)

.PHONY: doc
doc: $(DOC_DIR)/$(MBOX_DOC) $(DOC_DIR)/$(MBOX_JSON) $(DOC_DIR)/$(MBOX_VH) $(DOC_DIR)/$(MBOX_H)

.PHONY: clean
clean:
	$(RM) $(CLEANS)

