# Build process for config_rom.h
# Depends on tools found in bedrock
#   https://github.com/BerkeleyLab/Bedrock
# and "we" (the marble_mmc repo) don't really know if or where that is
# checked out.  In the rare event that config_rom.h needs rebuilding, do
#   rm config_rom.h
#   make -f rom.mk BEDROCK_DIR=/your/path/to/bedrock
#   make -f rom.mk clean
# Please consider the following line as only a wild guess.
BEDROCK_DIR = ../../bedrock
TOOLS_DIR = $(BEDROCK_DIR)/build-tools
PYTHON = python3

all: config_rom.h

sim_regmap.json: static_regmap.json
	$(PYTHON) $(TOOLS_DIR)/merge_json.py -i $< -o $@

config_rom.h: sim_regmap.json
	$(PYTHON) $(TOOLS_DIR)/build_rom.py -j $< -d "marble_mmc sim" -c $@

clean:
	rm -f sim_regmap.json
