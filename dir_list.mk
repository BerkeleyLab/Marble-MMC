MAKEF_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MAKEF_DIR := $(dir $(MAKEF_PATH))

BUILD_DIR := build
SOURCE_DIR := src
INCLUDE_DIR := inc
BSP_DIR := board_support
PROG_DIR := prog_support
DOC_DIR := doc
SCRIPTS_DIR := $(MAKEF_DIR)scripts
SUBMODULE_DIR := submodules
OLED_DIR := $(SUBMODULE_DIR)/oled
