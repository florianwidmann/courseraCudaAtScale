NVCC := /usr/local/cuda/bin/nvcc
CXX := g++

NVCCFLAGS := -ccbin $(CXX)
CXXFLAGS := -std=c++17
INCLUDEFLAGS := -I./lib -I./lib/UtilNPP
LDFLAGS := -lnppisu_static -lnppig_static -lnppc_static -lculibos -lfreeimage

SRC_DIR = src
BIN_DIR = bin

SRC := $(SRC_DIR)/rotatedThumbnailCreator.cpp
TARGET := $(BIN_DIR)/rotatedThumbnailCreator

.PHONY: help build clean install

help:
	@echo "Available make commands:"
	@echo "  make build  - Build the project."
	@echo "  make clean  - Clean up the build files."
	@echo "  make install- Install the project (not applicable here)."
	@echo "  make help   - Display this help message."

build: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p $(BIN_DIR)
	$(NVCC) $(NVCCFLAGS) $(CXXFLAGS) $(SRC) -o $(TARGET) $(INCLUDEFLAGS) $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)/*

install:
	@echo "No installation required."
