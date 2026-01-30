#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_CARD_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_CARD_H_

#include <array>

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
    bool CardBeats(Card a, Card b);
    constexpr int CardPointValue(const Card card) {
        SPIEL_CHECK_LT(card, kDeckSize);
        if (CardSuit(card) == Suit::kTarok) {
            // taroks
            if (card == static_cast<Card>(kPagat)) return 5;
            if (card == static_cast<Card>(kXXI)) return 5;
            if (card == static_cast<Card>(kSkiz)) return 5;
            return 1;
        } else {
            // suit cards
            SuitRank rank = static_cast<SuitRank>((card - kNumTaroks) % kNumRanksPerSuit);
            switch (rank) {
                case SuitRank::kKing: return 5;
                case SuitRank::kQueen: return 4;
                case SuitRank::kRider: return 3;
                case SuitRank::kJack: return 2;
                case SuitRank::kAceTen: return 1;
                default:
                    SpielFatalError("Invalid card rank");
                    return 0;
            }
        }
    }
    typedef std::array<Player, kDeckSize> Deck;
    const Player kTalon = -1;
    const Player kDeclarerSkart = -2;
    const Player kOpponentsSkart = -3;
    const Player kCurrentTrick = -4;
    constexpr Player WonCardsLocation(Player p) { return p + kNumPlayers; }
    constexpr Player WonCardsLocationToPlayer(Player p) {
        SPIEL_CHECK_GE(p, kNumPlayers);
        return p - kNumPlayers;
    }
    std::string DeckToString(const Deck &deck);

    std::ostream& operator<<(std::ostream& os, const Suit& suit);
    std::ostream& operator<<(std::ostream& os, const SuitRank& rank);

    std::string CardToString(Card card);
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_CARD_H_