#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_

#include <string>
#include <vector>

#include "open_spiel/spiel.h"

// A card is identified by its index in [0, kNumCards), laid out as:
//   0..21  : tarokks (trumps); index 0 = pagát (I), ..., 20 = XXI, 21 = Skíz.
//            A tarokk's trick strength equals its index (Skíz is the
//            strongest).
//   22..41 : suit cards = 22 + suit * 5 + rank, with suit in
//            {hearts, diamonds, clubs, spades} (0..3) and rank low->high in
//            {ace/ten, jack, rider, queen, king} (0..4).
// The index doubles as the OpenSpiel action id
namespace open_spiel {
namespace hungarian_tarokk {

inline constexpr int kNumPlayers = 4;
inline constexpr int kNumTricks = 9;

inline constexpr int kNumCards = 42;
inline constexpr int kNumTarokks = 22;  // trumps, action ids 0..21
inline constexpr int kNumSuits = 4;     // hearts, diamonds, clubs, spades
inline constexpr int kCardsPerSuit = 5;
inline constexpr int kHandSize = 9;   // cards dealt to each player
inline constexpr int kTalonSize = 6;  // cards left over in the talon

// Total number of card points in the pack
inline constexpr int kTotalCardPoints = 94;

// Pseudo-"suit" returned by CardSuit() for tarokks (trumps).
inline constexpr int kTarokkSuit = kNumSuits;  // 4

// A card identified by its index.
struct Card {
  int index = 0;
  constexpr bool operator==(Card other) const { return index == other.index; }
  constexpr bool operator!=(Card other) const { return index != other.index; }
  constexpr bool operator<(Card other) const { return index < other.index; }
  constexpr bool operator>(Card other) const { return index > other.index; }
};

constexpr Action CardToAction(Card card) { return card.index; }
constexpr Card CardFromAction(Action action) {
  return Card{static_cast<int>(action)};
}
std::vector<Action> CardsToActions(const std::vector<Card>& cards);

// Sentinel "no card" value
inline constexpr Card kInvalidCard{-1};

enum Suit { kHearts = 0, kDiamonds = 1, kClubs = 2, kSpades = 3 };
enum SuitRank { kLow = 0, kJack = 1, kRider = 2, kQueen = 3, kKing = 4 };

inline constexpr Card SuitCard(Suit suit, SuitRank rank) {
  return Card{kNumTarokks + suit * kCardsPerSuit + rank};
}
inline constexpr Card MakeKing(Suit suit) { return SuitCard(suit, kKing); }
inline constexpr Card Tarokk(int numeral) { return Card{numeral - 1}; }

inline constexpr Card kCardPagat = Tarokk(1);
inline constexpr Card kCardXVIII = Tarokk(18);
inline constexpr Card kCardXIX = Tarokk(19);
inline constexpr Card kCardXX = Tarokk(20);
inline constexpr Card kCardXXI = Tarokk(21);
inline constexpr Card kCardSkiz = Tarokk(22);

// A fresh, sorted 42-card deck
std::vector<Card> NewSortedDeck();

bool IsTarokk(Card card);
// 0..3 for a suit card, kTarokkSuit for a tarokk.
int CardSuit(Card card);
// Rank within a suit, 0 (low: ace/ten) .. 4 (king). Only valid for suit cards.
int CardRank(Card card);
// Card-point value: king 5, queen 4, rider 3, jack 2, ace/ten 1; honours
// (Skíz, XXI, pagát) 5, every other tarokk 1.
int CardPoints(Card card);
bool HandHasCard(const std::vector<Card>& hand, Card card);
bool HandHasHonour(const std::vector<Card>& hand);
// High honours are the XXI and the Skíz.
bool HandHasHighHonour(const std::vector<Card>& hand);
bool IsKing(Card card);
// Whether a card may be put in the skart: it is illegal to discard an honour
// (Skíz, XXI, pagát), a king or the XX. (A cue-bidder additionally may not
// discard the promised tarokk -- that is enforced by the caller, which knows
// the bidding.)
bool IsDiscardableCard(Card card);
// Whether the hand qualifies to be thrown in (annulled), see rules.md §4.4:
// all four kings, a lone XXI, a lone pagát, the XXI + pagát only, or no
// tarokks.
bool HandIsAnnullable(const std::vector<Card>& hand);
bool CardBeats(Card best, Card candidate, int led_suit);

std::string CardToString(Card card);
std::string CardsToString(const std::vector<Card>& cards);

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_
