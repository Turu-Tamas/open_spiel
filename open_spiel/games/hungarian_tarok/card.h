#include "spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarok {
    typedef int Card;
    const int kNumTaroks = 22;
    const int kNumSuits = 4;
    const int kNumRanksPerSuit = 5;
    const int kDeckSize = 42;
    const int kNumPlayers = 4;
    
    enum class Suit {
        kHearts = 0,
        kDiamonds = 1,
        kClubs = 2,
        kSpades = 3,
        kTarok = 4,
    };
    enum class SuitRank {
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
    constexpr Card MakeSuitCard(Suit suit, SuitRank rank) {
        SPIEL_CHECK_NE(suit, Suit::kTarok);
        return kNumTaroks + static_cast<int>(suit) * kNumRanksPerSuit + static_cast<int>(rank);
    }
    constexpr Suit CardSuit(Card card) {
        SPIEL_CHECK_LT(card, kDeckSize);
        return card < kNumTaroks ? Suit::kTarok
               : static_cast<Suit>((card - kNumTaroks) / kNumRanksPerSuit);
    }
    constexpr bool CardBeats(Card a, Card b) {
        SPIEL_CHECK_LT(a, kDeckSize);
        SPIEL_CHECK_LT(b, kDeckSize);
        
        Suit suit_a = CardSuit(a);
        Suit suit_b = CardSuit(b);
        if (suit_a == Suit::kTarok && suit_b != Suit::kTarok) {
            return true;
        } else if (suit_a != Suit::kTarok && suit_b == Suit::kTarok) {
            return false;
        } else if (suit_a == Suit::kTarok && suit_b == Suit::kTarok) {
            return a > b; // higher tarok wins
        } else {
            // both are suit cards
            if (suit_a != suit_b) {
                return false; // different suits, first does not win
            } else {
                return a > b; // higher rank wins
            }
        }
    }

    typedef std::array<Player, kDeckSize> Deck;
    const Player kTalon = -1;
    const Player kDeclarerSkart = -2;
    const Player kOpponentsSkart = -3;
    const Player kCurrentTrick = -4;
    constexpr Player WonCards(Player p) { return p + kNumPlayers; }
} // namespace hungarian_tarok
} // namespace open_spiel