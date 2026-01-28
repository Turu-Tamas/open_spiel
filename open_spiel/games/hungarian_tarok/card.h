#include "spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarok {
    typedef int Card;
    const int kNumTaroks = 22;
    const int kNumSuits = 4;
    const int kNumRanksPerSuit = 5;
    const int kDeckSize = 42;
    
    enum class Suit {
        kHearts = 0,
        kDiamonds = 1,
        kClubs = 2,
        kSpades = 3
    };
    enum class Rank {
        kAceTen = 0,
        kJack = 1,
        kRider = 2,
        kQueen = 3,
        kKing = 4
    };

    constexpr Card MakeTarok(int rank) {
        SPIEL_CHECK_GE(rank, 1);
        SPIEL_CHECK_LE(rank, 22);
        return rank - 1;
    }
    const Card kSkiz = MakeTarok(22);
    const Card kPagat = MakeTarok(1);
    const Card kXXI = MakeTarok(21);
    constexpr Card MakeSuitCard(int suit, int rank) {
        return kNumTaroks + suit * kNumRanksPerSuit + rank;
    }
} // namespace hungarian_tarok
} // namespace open_spiel