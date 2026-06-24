// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_

#include <string>
#include <vector>

#include "open_spiel/spiel.h"

// The 42-card tarokk pack: its identity, point values, names and trick
// comparison. This file is self-contained (it depends only on the core
// OpenSpiel types) so the main game header can include it freely.
//
// A card is an Action id laid out as:
//   0..21  : tarokks (trumps); id 0 = pagát (I), ..., id 20 = XXI, id 21 = Skíz.
//            A tarokk's trick strength equals its id (Skíz is the strongest).
//   22..41 : suit cards = 22 + suit * 5 + rank, with suit in
//            {hearts, diamonds, clubs, spades} (0..3) and rank low->high in
//            {ace/ten, jack, rider, queen, king} (0..4).

namespace open_spiel {
namespace hungarian_tarokk {

inline constexpr int kNumCards = 42;
inline constexpr int kNumTarokks = 22;    // trumps, action ids 0..21
inline constexpr int kNumSuits = 4;       // hearts, diamonds, clubs, spades
inline constexpr int kCardsPerSuit = 5;
inline constexpr int kHandSize = 9;       // cards dealt to each player
inline constexpr int kTalonSize = 6;      // cards left over in the talon

// Total number of card points in the pack (15 per suit, 15 for the honours and
// 19 for the other tarokks); used to bound utilities.
inline constexpr int kTotalCardPoints = 94;

// Pseudo-"suit" returned by CardSuit() for tarokks (trumps).
inline constexpr int kTarokkSuit = kNumSuits;  // 4

// Named tarokks referenced by the bidding conventions (a tarokk's action id is
// one less than its Roman numeral; the pagát is the I).
inline constexpr Action kCardPagat = 0;   // tarokk I
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
// Whether the hand holds the given card.
bool HandHasCard(const std::vector<Action>& hand, Action card);
// Whether the hand holds at least one honour (the pagát/I, the XXI or the Skíz).
bool HandHasHonour(const std::vector<Action>& hand);
// Whether the hand holds at least one high honour (the XXI or the Skíz).
bool HandHasHighHonour(const std::vector<Action>& hand);
// Whether `candidate` beats `best` (the card currently winning the trick) given
// the suit that was led (the CardSuit of the first card played to the trick).
bool CardBeats(Action best, Action candidate, int led_suit);

std::string CardToString(Action card);
std::string CardsToString(const std::vector<Action>& cards);

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_CARDS_H_
