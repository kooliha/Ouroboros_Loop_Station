TARGET = main

CPP_SOURCES = main.cpp looper_layer.cpp

LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile