#include "bitboard.hpp"

#include "utils.hpp"

#include <print>

namespace Lyra::BBUtils {

using enum Direction;

/******************************************\
|==========================================|
|               Print Function             |
|==========================================|
\******************************************/

void print(BB bb) {
  std::println("\n     +---+---+---+---+---+---+---+---+");

  for (Rank r = Rank8; r >= Rank1; --r) {
    std::print("{}   |", format_rank(r));
    for (File f = FileA; f <= FileH; ++f)
      std::print(" {} |", bb & from(make_square(f, r)) ? '1' : '.');

    std::println("\n     +---+---+---+---+---+---+---+---+");
  }

  std::println("\n       A   B   C   D   E   F   G   H\n");
  std::println("Bitboard: {:#X}", bb);
}

/******************************************\
|==========================================|
|             Helper Functions             |
|==========================================|
\******************************************/

template <Direction D>
constexpr BB ray(Square sq, BB occ) {
  BB ray   = 0ULL;
  BB sq_bb = from(sq);
  do {
    ray |= (sq_bb = shift<D>(sq_bb));
  } while (sq_bb & ~occ);

  return ray;
}

template <Direction D1, Direction D2, Direction... Ds>
constexpr BB ray(Square sq, BB occ) {
  return ray<D1>(sq, occ) | ray<D2, Ds...>(sq, occ);
}

/******************************************\
|==========================================|
|              Piece Attacks               |
|==========================================|
\******************************************/

constexpr BB naive_knight_attacks(Square sq) {
  return shift<N>(shift<NE, NW>(from(sq))) | shift<E>(shift<NE, SE>(from(sq)))
         | shift<S>(shift<SE, SW>(from(sq))) | shift<W>(shift<NW, SW>(from(sq)));
}

template <PieceType pt>
constexpr BB naive_slider_attacks(Square sq, BB occ) {
  if constexpr (pt == B)
    return ray<NE, NW, SE, SW>(sq, occ);
  else
    return ray<N, S, E, W>(sq, occ);
}

/******************************************\
|==========================================|
|              LookUp Tables               |
|==========================================|
\******************************************/

BB    PAWN_ATK[NColour][NSquare];
BB    KNIGHT_ATK[NSquare];
BB    KING_ATK[NSquare];
Magic BISHOP_ATK[NSquare];
Magic ROOK_ATK[NSquare];
BB    LINE_BB[NSquare][NSquare];
BB    BTWN_BB[NSquare][NSquare];

static BB BISHOP_TBL[0x1480];
static BB ROOK_TBL[0x19000];

/******************************************\
|==========================================|
|              Magic Numbers               |
|==========================================|
\******************************************/

#ifndef USE_PEXT
// Precomputed magic numbers
constexpr NDArray<U64, NSquare> BISHOP_MAGICS = {{
    0x20020222040410,   0x28b00d02022008,   0x7062080840810200, 0x1208060048400020,
    0x2001104100240804, 0x48241008140800,   0x42021002082004,   0x210118200201,
    0x920178a690044100, 0xa02202220223,     0xa000104420404000, 0x140516000050,
    0x20004404200a0004, 0x104824802400000,  0x10004202202262,   0x80b1190429010800,
    0x40002002044102,   0x8a50008404980040, 0x84000a48108100,   0x108050082004020,
    0x8002000c12020200, 0x1000a00826100,    0x240800848480830,  0xd00201202020280,
    0x141840f224040890, 0x200402018490a400, 0x800208804080080,  0x402080014040408,
    0x2011010008104000, 0x509004082005010,  0x1000840241040200, 0x10309600048200a4,
    0x58084201040405,   0x58084201040405,   0x8002019000184042, 0x600040400080210,
    0x120008400858020,  0x2004010108a184,   0x100400500b0924,   0x24008204008040,
    0x2048580410814440, 0x2048580410814440, 0x4011010814212204, 0x3100106018024104,
    0xa000080208208401, 0x1004008082000100, 0x8210220a40480400, 0x4042080a2090100,
    0x42021002082004,   0x22c0108090040,    0x480010401040040,  0x4000205040000,
    0x200010630440001,  0x10020220a820520,  0x20020222040410,   0x28b00d02022008,
    0x210118200201,     0x80b1190429010800, 0x40441041,         0x40c181048800,
    0x50829250204848,   0x1000002002900640, 0x920178a690044100, 0x20020222040410,
}};

// Precomputed magic numbers
constexpr NDArray<U64, NSquare> ROOK_MAGICS = {{
    0x80104000208004,   0x1040002000401005, 0x100081041002000,  0xb00082030000500,
    0x2a0010a004084200, 0x900028801000400,  0x8200580200210084, 0x100108041000022,
    0xc000802080004000, 0xc064802000804001, 0x2001042228200,    0x800800800801002,
    0x2020800400800800, 0x1082000804020010, 0x8040001100408c2,  0x801000100008042,
    0x808000400020,     0x10004000200044,   0x2000220018820040, 0x10008008008010,
    0x4808008000400,    0x11010002080400,   0x4298040008011002, 0x414520001009044,
    0x2240800880204004, 0x9200400080802000, 0x40408200201202,   0x2010a00201140,
    0x600080280040080,  0x6a010200104488,   0x806040101000200,  0x8a20240200108041,
    0x434401082800020,  0x80200081804000,   0x8020408202002012, 0x41022009001000,
    0x2020800400800800, 0x441000401000208,  0x2444885004000102, 0x1004082000411,
    0x140802040008002,  0x500020004000,     0x880204200820010,  0x10008008008010,
    0x8000040008008080, 0x1082000804020010, 0x20001008080,      0x100048a10a000c,
    0x421088000204700,  0x20004000218180,   0x2001042228200,    0x4448801000080080,
    0x8000040008008080, 0x24e001004080200,  0x211042080400,     0x304c208041040200,
    0x1410292002082,    0x810080120021004a, 0xe00c2000110041,   0x2001002004100009,
    0x20030a0244802,    0x406000418101122,  0x2492009001020804, 0x400111002094004a,
}};
#endif

/******************************************\
|==========================================|
|               Init Magics                |
|==========================================|
\******************************************/

// Pieces on the edges at the end of a ray can be ignored
// since the piece is assumed to be capturable
constexpr BB edge_mask_excluding(Square sq) {
  const BB rank_edges = (from(Rank1) | from(Rank8)) & ~from(rank_of(sq));
  const BB file_edges = (from(FileA) | from(FileH)) & ~from(file_of(sq));
  return rank_edges | file_edges;
}

template <PieceType pt>
void init_magics(Square sq, BB table[], Magic entries[]) {
  static int offset = 0;

  Magic &m = entries[sq];
  m.mask   = naive_slider_attacks<pt>(sq, EmptyBB) & ~edge_mask_excluding(sq);
#ifndef USE_PEXT
  m.shift = 64 - count(m.mask);
#endif
  // Offset the pointer to the table and save the attacks
  m.attacks = &table[offset];
  offset += 1 << popcount(m.mask);
  // Calculate the attacks for each possible occupancy bitboard and store it at the index
  BB occ = EmptyBB;
  do {
    m.attacks[m.index(occ)] = naive_slider_attacks<pt>(sq, occ);
    // Carry Rippler Trick
    occ = (occ - m.mask) & m.mask;
  } while (occ);
}

/******************************************\
|==========================================|
|           Init Lookup tables             |
|==========================================|
\******************************************/

void init() {
  for (Square src = A1; src <= H8; ++src) {
    for (Square dst = A1; dst <= H8; ++dst) {
      if (naive_slider_attacks<B>(src, EmptyBB) & from(dst)) {
        LINE_BB[src][dst] =
            naive_slider_attacks<B>(src, EmptyBB) & naive_slider_attacks<B>(dst, EmptyBB);
        BTWN_BB[src][dst] =
            naive_slider_attacks<B>(src, from(dst)) & naive_slider_attacks<B>(dst, from(src));
      }
      if (naive_slider_attacks<R>(src, EmptyBB) & from(dst)) {
        LINE_BB[src][dst] =
            naive_slider_attacks<R>(src, EmptyBB) & naive_slider_attacks<R>(dst, EmptyBB);
        BTWN_BB[src][dst] =
            naive_slider_attacks<R>(src, from(dst)) & naive_slider_attacks<R>(dst, from(src));
      }
    }
  }

  for (Square sq = A1; sq <= H8; ++sq) {
    PAWN_ATK[Colour::White][sq] = shift<NE, NW>(from(sq));
    PAWN_ATK[Colour::Black][sq] = shift<SE, SW>(from(sq));
    KNIGHT_ATK[sq]              = naive_knight_attacks(sq);
    KING_ATK[sq]                = shift<N, NE, E, SE, S, SW, W, NW>(from(sq));
#ifndef USE_PEXT
    BISHOP_ATK[sq].magic = BISHOP_MAGICS[sq];
    ROOK_ATK[sq].magic   = ROOK_MAGICS[sq];
#endif
    init_magics<B>(sq, BISHOP_TBL, BISHOP_ATK);
    init_magics<R>(sq, ROOK_TBL, ROOK_ATK);
  }
}

} // namespace Lyra::BBUtils
