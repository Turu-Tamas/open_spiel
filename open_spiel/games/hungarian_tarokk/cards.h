#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_

#include <string>
#include <vector>

#include "open_spiel/spiel.h"

// A card is an Action id laid out as:
//   0..21  : tarokks (trumps); id 0 = pagát (I), ..., id 20 = XXI, id 21 =
//   Skíz.
//            A tarokk's trick strength equals its id (Skíz is the strongest).
//   22..41 : suit cards = 22 + suit * 5 + rank, with suit in
//            {hearts, diamonds, clubs, spades} (0..3) and rank low->high in
//            {ace/ten, jack, rider, queen, king} (0..4).

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

// a tarokk's action id is one less than its Roman numeral; the pagát is the I.
inline constexpr Action kCardPagat = 0;
inline constexpr Action kCardXVIII = 17;
inline constexpr Action kCardXIX = 18;
inline constexpr Action kCardXX = 19;
inline constexpr Action kCardXXI = 20;
inline constexpr Action kCardSkiz = 21;

// A fresh, sorted 42-card deck (action ids 0..41).
std::vector<Action> NewSortedDeck();

bool IsTarokk(Action card);
// 0..3 for a suit card, kTarokkSuit for a tarokk.
int CardSuit(Action card);
// Rank within a suit, 0 (low: ace/ten) .. 4 (king). Only valid for suit cards.
int CardRank(Action card);
// Card-point value: king 5, queen 4, rider 3, jack 2, ace/ten 1; honours
// (Skíz, XXI, pagát) 5, every other tarokk 1.
int CardPoints(Action card);
bool HandHasCard(const std::vector<Action>& hand, Action card);
bool HandHasHonour(const std::vector<Action>& hand);
// High honours are the XXI and the Skíz.
bool HandHasHighHonour(const std::vector<Action>& hand);
bool IsKing(Action card);
// Whether a card may be put in the skart: it is illegal to discard an honour
// (Skíz, XXI, pagát), a king or the XX. (A cue-bidder additionally may not
// discard the promised tarokk -- that is enforced by the caller, which knows
// the bidding.)
bool IsDiscardableCard(Action card);
// Whether the hand qualifies to be thrown in (annulled), see rules.md §4.4:
// all four kings, a lone XXI, a lone pagát, the XXI + pagát only, or no
// tarokks.
bool HandIsAnnullable(const std::vector<Action>& hand);
bool CardBeats(Action best, Action candidate, int led_suit);

std::string CardToString(Action card);
std::string CardsToString(const std::vector<Action>& cards);

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_
