CXX = x86_64-w64-mingw32-g++
WINDRES = x86_64-w64-mingw32-windres

CXXFLAGS = -O2 -g -Wall -Wextra -std=c++17 -DWIN32_LEAN_AND_MEAN -DWINVER=0x0A00
LDFLAGS = -static -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup

INCLUDES = -Iinclude
LIBS = -ld3d11 -ldxgi -lole32

BUILD_DIR = build
COMMON_SOURCES = src/common/debug.cpp src/common/debug_symbols.cpp
COMMON_OBJECTS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(COMMON_SOURCES))

D3D11_SOURCES = $(wildcard src/d3d11_impl/*.cpp)
D3D11_OBJECTS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(D3D11_SOURCES))

MAIN_OBJECTS = $(BUILD_DIR)/main.o

TARGET = d3d12.dll
TARGET_PATH = $(BUILD_DIR)/$(TARGET)

.PHONY: all clean makedirs

all: makedirs $(TARGET_PATH)

makedirs:
	@mkdir -p $(BUILD_DIR)/common
	@mkdir -p $(BUILD_DIR)/d3d11_impl

$(TARGET_PATH): $(COMMON_OBJECTS) $(D3D11_OBJECTS) $(MAIN_OBJECTS)
	$(CXX) -static -shared -o $@ $^ $(LDFLAGS) $(LIBS) -Wl,--out-implib,$(BUILD_DIR)/lib$(TARGET).a def/d3d12.def

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
