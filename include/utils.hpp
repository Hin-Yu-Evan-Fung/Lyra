#pragma once

#include <chrono>

#include "defs.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|       Pseudo Random Number Generator     |
|==========================================|
\******************************************/

struct PRNG {
 public:
  PRNG() : PRNG(0x6B51FF299F6A3AEE) {}
  PRNG(U64 seed) {
    s0 = seed;
    s1 = seed * 2;
    s2 = seed / 5;
    s3 = seed + seed / 2;
  }

  U64 random() {
    U64 tmp  = s1 << 17;
    s2      ^= s0;
    s3      ^= s1;
    s1      ^= s2;
    s0      ^= s3;
    s2      ^= tmp;
    s3       = std::rotl(s3, 45);

    return s0;
  }

 private:
  U64 s0, s1, s2, s3;
};

/******************************************\
|==========================================|
|           Miscellaneous Helpers          |
|==========================================|
\******************************************/

using Time = uint64_t;

inline Time now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::high_resolution_clock::now().time_since_epoch()
  )
    .count();
}

/******************************************\
|==========================================|
|                 IO helpers               |
|==========================================|
\******************************************/

namespace IOUtils {

constexpr std::string_view PIECE_STR = "PpNnBbRrQqKk ";

constexpr char        format_file(File f) { return static_cast<char>(f + 'a'); }
constexpr char        format_rank(Rank r) { return static_cast<char>(r + '1'); }
constexpr std::string format_sq(Square sq) { return {format_file(file_of(sq)), format_rank(rank_of(sq))}; }
constexpr char        format_piece(Piece pc) { return PIECE_STR.at(pc); }

constexpr File   parse_file(const char c) { return static_cast<File>(std::tolower(c) - 'a'); }
constexpr Rank   parse_rank(const char c) { return static_cast<Rank>(std::tolower(c) - '1'); }
constexpr Square parse_sq(const std::string& str) { return make_square(parse_file(str[0]), parse_rank(str[1])); }
constexpr Piece  parse_piece(const char c) { return static_cast<Piece>(PIECE_STR.find(c)); }

}  // namespace IOUtils

}  // namespace Lyra
