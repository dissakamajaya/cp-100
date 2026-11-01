# Host "simulated board" for the CP-100 firmware: the real sources in src/
# compiled against the fake Arduino/ESP-IDF layer in fake/.
#   make -f sim.mk            build build/cp100sim
#   make -f sim.mk run     build and run every scenario (same as ./run.sh)
#   make -f sim.mk FW=/path/to/firmware-copy BUILD=/path/to/firmware-copy/simbuild
#                   build the harness against another firmware tree (mutation tests)
FW      ?= ../..
CXX     ?= g++
CXXFLAGS ?= -std=c++17 -O2 -pthread -Wall
SANITIZE ?=
FLAGS   := $(CXXFLAGS) $(SANITIZE) -Ifake -I$(FW)/include -I$(FW)/src -I. -DCP100_SRC_DIR='"$(abspath $(FW)/src)"'
LDFLAGS := -pthread $(SANITIZE) -Wl,--wrap=_Z14calibrationFor4Wavei9Harmonics
BUILD   ?= build
FW_SRC  := $(wildcard $(FW)/src/*.cpp)
FW_OBJ  := $(patsubst $(FW)/src/%.cpp,$(BUILD)/fw/%.o,$(FW_SRC))
SIM_OBJ := $(BUILD)/sim.o $(BUILD)/scenarios.o
HEADERS := $(wildcard fake/*.h fake/driver/*.h *.h ref/*.h $(FW)/src/*.h $(FW)/include/*.h)

all: $(BUILD)/cp100sim

$(BUILD)/cp100sim: $(FW_OBJ) $(SIM_OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(BUILD)/fw/%.o: $(FW)/src/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(FLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(FLAGS) -c $< -o $@

ref/reference.h: ref/gen_reference.mjs ref/dsp.mjs
	node ref/gen_reference.mjs > $@

ref/font.h: ref/gen_font.mjs ref/app.js
	node ref/gen_font.mjs > $@

run: all
	./run.sh

clean:
	rm -rf build build-asan out

.PHONY: all run clean
