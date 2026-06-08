EXE := Lyra
CXX := g++
CXXSTD := -std=c++23

NETWORK_PATH = $(realpath src/weights/big.nnue)

COMMON_FLAGS := -DTUNE -DUSE_PEXT -DNETWORK_PATH=\"$(NETWORK_PATH)\" \
	-Isrc/cli -Isrc/core -Isrc/nnue -Wall -flto -fno-rtti \
	-msse2 -msse3 -msse4 -msse4.1 -mpopcnt -mavx2 \
	-mbmi -mbmi2 -mmmx -funroll-loops -finline -fomit-frame-pointer

RELEASE_FLAGS := -O3 -DNDEBUG
DEBUG_FLAGS := -O3 -g

CXXFLAGS := $(CXXSTD) $(COMMON_FLAGS) $(RELEASE_FLAGS) 

SRC := \
	src/core/bitboard.cpp \
	src/core/board.cpp \
	src/core/move.cpp \
	src/core/movepick.cpp \
	src/core/clock.cpp \
	src/core/search.cpp \
	src/core/search_utils.cpp \
	src/core/tt.cpp \
	src/core/perft.cpp \
	src/cli/thread.cpp \
	src/cli/engine.cpp \
	src/cli/uci.cpp \
	src/cli/main.cpp \
	src/nnue/network.cpp \

OBJ := $(SRC:.cpp=.o)

all: $(EXE)

$(EXE): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS := $(CXXSTD) $(COMMON_FLAGS) $(DEBUG_FLAGS) -Iinclude -Innue
debug: clean all

clean:
	rm -f $(OBJ) $(EXE)

.PHONY: all clean debug
