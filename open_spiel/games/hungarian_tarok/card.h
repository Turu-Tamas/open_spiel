#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_CARD_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_CARD_H_

#include <array>
#include <ostream>
#include <string>

#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarok {
using Card = int;
constexpr int kNumTaroks = 22;
constexpr int kNumSuits = 4;
constexpr int kNumRanksPerSuit = 5;
constexpr int kDeckSize = 42;
constexpr int kNumPlayers = 4;

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
  kKing = 4,
};

constexpr Card MakeTarok(int rank) {
  SPIEL_CHECK_GE(rank, 1);
  SPIEL_CHECK_LE(rank, 22);
  return rank - 1;
}
const Card kSkiz = MakeTarok(22);
const Card kPagat = MakeTarok(1);
const Card kXXI = MakeTarok(21);
constexpr bool IsHonour(Card card) {
  return card == kPagat || card == kXXI || card == kSkiz;
}
constexpr Card MakeSuitCard(Suit suit, SuitRank rank) {
  SPIEL_CHECK_NE(suit, Suit::kTarok);
  return kNumTaroks + static_cast<int>(suit) * kNumRanksPerSuit +
         static_cast<int>(rank);
}
constexpr Suit CardSuit(Card card) {
  SPIEL_CHECK_LT(card, kDeckSize);
  return card < kNumTaroks
             ? Suit::kTarok
             : static_cast<Suit>((card - kNumTaroks) / kNumRanksPerSuit);
}
constexpr SuitRank CardSuitRank(Card card) {
  SPIEL_CHECK_LT(card, kDeckSize);
  SPIEL_CHECK_NE(CardSuit(card), Suit::kTarok);
  return static_cast<SuitRank>((card - kNumTaroks) % kNumRanksPerSuit);
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
    SuitRank rank = CardSuitRank(card);
    switch (rank) {
      case SuitRank::kKing:
        return 5;
      case SuitRank::kQueen:
        return 4;
      case SuitRank::kRider:
        return 3;
      case SuitRank::kJack:
        return 2;
      case SuitRank::kAceTen:
        return 1;
      default:
        SpielFatalError("Invalid card rank");
        return 0;
    }
  }
}

enum class CardLocation {
  kPlayer0Hand = 0,
  kPlayer1Hand = 1,
  kPlayer2Hand = 2,
  kPlayer3Hand = 3,

  kPlayer0WonCards = 4,
  kPlayer1WonCards = 5,
  kPlayer2WonCards = 6,
  kPlayer3WonCards = 7,

  kTalon = 8,            // Undealt cards in the talon
  kDeclarerSkart = 9,    // Cards discarded by the declarer
  kOpponentsSkart = 10,  // Cards discarded by opponents
  kCurrentTrick = 11,    // Cards currently in play on the table
};

// Deck maps each card to its current location
using Deck = std::array<CardLocation, kDeckSize>;

// Helper functions to create CardLocations from Player indices
constexpr CardLocation PlayerHandLocation(Player p) {
  SPIEL_CHECK_GE(p, 0);
  SPIEL_CHECK_LT(p, kNumPlayers);
  return static_cast<CardLocation>(p);
}

constexpr CardLocation PlayerWonCardsLocation(Player p) {
  SPIEL_CHECK_GE(p, 0);
  SPIEL_CHECK_LT(p, kNumPlayers);
  return static_cast<CardLocation>(p + kNumPlayers);
}

// Helper functions to check location types
constexpr bool IsPlayerHand(CardLocation loc) {
  int val = static_cast<int>(loc);
  return val >= 0 && val < kNumPlayers;
}

constexpr bool IsWonCards(CardLocation loc) {
  int val = static_cast<int>(loc);
  return val >= kNumPlayers && val < 2 * kNumPlayers;
}

// Helper functions to extract Player from CardLocation
// These should only be called after checking with IsPlayerHand/IsWonCards
constexpr Player HandLocationPlayer(CardLocation loc) {
  SPIEL_CHECK_TRUE(IsPlayerHand(loc));
  return static_cast<Player>(loc);
}

constexpr Player WonCardsLocationPlayer(CardLocation loc) {
  SPIEL_CHECK_TRUE(IsWonCards(loc));
  return static_cast<Player>(loc) - kNumPlayers;
}
std::string DeckToString(const Deck& deck);

std::ostream& operator<<(std::ostream& os, const CardLocation& location);
std::ostream& operator<<(std::ostream& os, const Suit& suit);
std::ostream& operator<<(std::ostream& os, const SuitRank& rank);

std::string CardToString(Card card);
}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_CARD_H_