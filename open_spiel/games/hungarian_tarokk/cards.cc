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

#include "open_spiel/games/hungarian_tarokk/cards.h"

#include <numeric>
#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarokk {
namespace {

const char* const kSuitNames[kNumSuits] = {"Hearts", "Diamonds", "Clubs",
                                           "Spades"};

std::string RomanNumeral(int n) {
  static const char* const kRomans[] = {
      "0",   "I",   "II",   "III",   "IV",  "V",   "VI",  "VII",
      "VIII", "IX", "X",    "XI",    "XII", "XIII", "XIV", "XV",
      "XVI", "XVII", "XVIII", "XIX", "XX",  "XXI"};
  SPIEL_CHECK_GE(n, 1);
  SPIEL_CHECK_LE(n, 21);
  return kRomans[n];
}

}  // namespace

std::vector<Action> NewSortedDeck() {
  std::vector<Action> deck(kNumCards);
  std::iota(deck.begin(), deck.end(), 0);
  return deck;
}

bool IsTarokk(Action card) { return card < kNumTarokks; }

int CardSuit(Action card) {
  if (IsTarokk(card)) return kTarokkSuit;
  return (card - kNumTarokks) / kCardsPerSuit;
}

int CardRank(Action card) { return (card - kNumTarokks) % kCardsPerSuit; }

int CardPoints(Action card) {
  if (IsTarokk(card)) {
    // Honours: pagát (0), XXI (20) and Skíz (21) are worth 5; the other
    // tarokks (II..XX) are worth 1.
    if (card == 0 || card == 20 || card == 21) return 5;
    return 1;
  }
  // Suit cards: ace/ten 1, jack 2, rider 3, queen 4, king 5.
  return CardRank(card) + 1;
}

bool HandHasCard(const std::vector<Action>& hand, Action card) {
  for (Action c : hand) {
    if (c == card) return true;
  }
  return false;
}

bool HandHasHonour(const std::vector<Action>& hand) {
  return HandHasCard(hand, kCardPagat) || HandHasCard(hand, kCardXXI) ||
         HandHasCard(hand, kCardSkiz);
}

bool HandHasHighHonour(const std::vector<Action>& hand) {
  return HandHasCard(hand, kCardXXI) || HandHasCard(hand, kCardSkiz);
}

bool CardBeats(Action best, Action candidate, int led_suit) {
  bool best_t = IsTarokk(best);
  bool cand_t = IsTarokk(candidate);
  if (cand_t && best_t) return candidate > best;
  if (cand_t) return true;   // a tarokk beats any non-tarokk
  if (best_t) return false;  // a non-tarokk cannot beat a tarokk
  // Both are suit cards; only a card of the led suit can win.
  if (CardSuit(candidate) != led_suit) return false;
  if (CardSuit(best) != led_suit) return true;
  return CardRank(candidate) > CardRank(best);
}

std::string CardToString(Action card) {
  if (IsTarokk(card)) {
    if (card == 21) return "Skiz";
    return RomanNumeral(card + 1);  // id 0 -> "I", id 20 -> "XXI".
  }
  static const char* const kRankNames[kCardsPerSuit] = {"Low", "Jack", "Rider",
                                                        "Queen", "King"};
  int suit = CardSuit(card);
  int rank = CardRank(card);
  std::string rank_name = kRankNames[rank];
  // The lowest card is the ace in the red suits and the ten in the black suits.
  if (rank == 0) rank_name = (suit < 2) ? "Ace" : "Ten";
  return absl::StrCat(rank_name, "Of", kSuitNames[suit]);
}

std::string CardsToString(const std::vector<Action>& cards) {
  std::vector<std::string> names;
  names.reserve(cards.size());
  for (Action c : cards) names.push_back(CardToString(c));
  return absl::StrCat("[", absl::StrJoin(names, " "), "]");
}

}  // namespace hungarian_tarokk
}  // namespace open_spiel
