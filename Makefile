EXE := Lyra
CXX := g++
CXXSTD := -std=c++23

COMMON_FLAGS := -DUSE_PEXT -Wall -flto -fno-rtti \
	-msse2 -msse3 -msse4 -msse4.1 -mpopcnt -mavx2 \
	-mbmi -mbmi2 -mmmx -funroll-loops -finline -fomit-frame-pointer

RELEASE_FLAGS := -O3 -DNDEBUG
DEBUG_FLAGS := -O3 -g

CXXFLAGS := $(CXXSTD) $(COMMON_FLAGS) $(RELEASE_FLAGS) -Iinclude

SRC := \
	src/main.cpp \
	src/board/board.cpp \
	src/board/move.cpp \
	src/core/bitboard.cpp \
	src/engine/clock.cpp \
	src/engine/engine.cpp \
	src/engine/thread.cpp \
	src/engine/uci.cpp \
	src/search/eval.cpp \
	src/search/movepick.cpp \
	src/search/search.cpp \
	src/search/utils.cpp \
	src/utils/bench.cpp \
	src/utils/perft.cpp \
	src/utils/tt.cpp

OBJ := $(SRC:.cpp=.o)

all: $(EXE)

$(EXE): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS := $(CXXSTD) $(COMMON_FLAGS) $(DEBUG_FLAGS) -Iinclude
debug: clean all

clean:
	rm -f $(OBJ) $(EXE)

.PHONY: all clean debug
