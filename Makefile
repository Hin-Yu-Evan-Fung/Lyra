EXE := Lyra
CXX := g++
CXXSTD := -std=c++23

COMMON_FLAGS := -DTUNE -DUSE_PEXT -Wall -flto -fno-rtti \
	-msse2 -msse3 -msse4 -msse4.1 -mpopcnt -mavx2 \
	-mbmi -mbmi2 -mmmx -funroll-loops -finline -fomit-frame-pointer

RELEASE_FLAGS := -O3 -DNDEBUG
DEBUG_FLAGS := -O3 -g

CXXFLAGS := $(CXXSTD) $(COMMON_FLAGS) $(RELEASE_FLAGS) -Iinclude

SRC := \
	src/bitboard.cpp \
	src/board.cpp \
	src/move.cpp \
	src/thread.cpp \
	src/movepick.cpp \
	src/clock.cpp \
	src/eval.cpp \
	src/perft.cpp \
	src/search.cpp \
	src/search_utils.cpp \
	src/engine.cpp \
	src/uci.cpp \
	src/main.cpp 

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
