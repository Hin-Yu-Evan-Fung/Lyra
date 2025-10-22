#pragma once

#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "utils.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|          Chess 960 Castling Mask         |
|==========================================|
\******************************************/

struct CastleMask {
public:
    NDArray<Castle, NSquare> rights;
    NDArray<Square, 4>       rook_sq;

    void           add_rights(Square ksq, Square rsq, Castle cr);
    constexpr void reset();
    template <Colour C>
    constexpr Square rook_src(bool queen_side) const;
    template <Colour C>
    static constexpr Square rook_dst(bool queen_side);
    template <Colour C>
    static constexpr Square king_dst(bool queen_side);
    static constexpr Castle get_mask(Colour c, bool queen_side);
    std::string             to_str(Castle cr, bool chess960 = false) const;

private:
    static constexpr int index(Colour c, bool queen_side);
};

/******************************************\
|==========================================|
|           Castle Mask Helpers            |
|==========================================|
\******************************************/

inline void CastleMask::add_rights(Square ksq, Square rsq, Castle cr) {
    int idx       = BBUtils::lsb(cr);
    rook_sq[idx]  = rsq;
    rights[ksq]  &= ~cr;
    rights[rsq]  &= ~cr;
}

constexpr void CastleMask::reset() {
    for (Square sq = A1; sq <= H8; ++sq)
        rights[sq] = AnyCastle;
    for (int i = 0; i < 4; i++)
        rook_sq[i] = NoSquare;
}

constexpr int    CastleMask::index(Colour c, bool queen_side) { return (c << 1) | queen_side; }
constexpr Castle CastleMask::get_mask(Colour c, bool queen_side) { return Castle(WhiteOO << index(c, queen_side)); }

template <Colour C>
constexpr Square CastleMask::rook_src(bool queen_side) const {
    return rook_sq[index(C, queen_side)];
}

template <Colour C>
constexpr Square CastleMask::rook_dst(bool queen_side) {
    constexpr Square ROOK_DST_MAP[4] = {F1, D1, F8, D8};
    return ROOK_DST_MAP[index(C, queen_side)];
}

template <Colour C>
constexpr Square CastleMask::king_dst(bool queen_side) {
    constexpr Square KING_DST_MAP[4] = {G1, C1, G8, C8};
    return KING_DST_MAP[index(C, queen_side)];
}

inline std::string CastleMask::to_str(Castle cr, bool chess960) const {
    std::ostringstream out;
    if (cr & WhiteOO)
        out << (chess960 ? Lyra::to_str(file_of(rook_sq[0])) : "K");
    if (cr & WhiteOOO)
        out << (chess960 ? Lyra::to_str(file_of(rook_sq[1])) : "Q");
    if (cr & BlackOO)
        out << (chess960 ? Lyra::to_str(file_of(rook_sq[2])) : "k");
    if (cr & BlackOOO)
        out << (chess960 ? Lyra::to_str(file_of(rook_sq[3])) : "q");
    return out.str();
}

}  // namespace Lyra
