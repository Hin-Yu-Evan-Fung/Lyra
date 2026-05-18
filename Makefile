EXE := Lyra
CXX := g++
CXXSTD := -std=c++23

NETWORK_PATH = $(realpath data/big.nnue)

COMMON_FLAGS := -DTUNE -DUSE_PEXT -DNETWORK_PATH=\"$(NETWORK_PATH)\" \
	-Iinclude -Innue -Wall -flto -fno-rtti \
	-msse2 -msse3 -msse4 -msse4.1 -mpopcnt -mavx2 \
	-mbmi -mbmi2 -mmmx -funroll-loops -finline -fomit-frame-pointer \
 -fopt-info-vec

RELEASE_FLAGS := -O3 -DNDEBUG
DEBUG_FLAGS := -O3 -g

CXXFLAGS := $(CXXSTD) $(COMMON_FLAGS) $(RELEASE_FLAGS) 

SRC := \
	src/bitboard.cpp \
	src/board.cpp \
	src/move.cpp \
	src/thread.cpp \
	src/movepick.cpp \
	src/clock.cpp \
	src/perft.cpp \
	src/search.cpp \
	src/search_utils.cpp \
	src/tt.cpp \
	src/engine.cpp \
	src/uci.cpp \
	nnue/network.cpp \
	src/main.cpp

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
