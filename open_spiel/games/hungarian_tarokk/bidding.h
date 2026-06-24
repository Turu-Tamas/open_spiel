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

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_BIDDING_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_BIDDING_H_

#include <string>
#include <utility>
#include <vector>

#include "open_spiel/spiel.h"

// The bidding (auction) phase of Hungarian Tarokk, see rules.md §3.
//
// This file is self-contained: it depends only on the core OpenSpiel types and
// NOT on cards.h / hungarian_tarokk.h. The relevant facts about each player's
// hand are passed in via PlayerBidInfo, and any tarokk the auction obliges the
// declarer to call is reported abstractly as a CalledCard, so the file never
// needs to know concrete card ids.
//
// Implemented (rules.md §3.1 - §3.5):
//   * the four bids three / two / one / solo, plus pass and hold (tartom);
//   * the honour entry requirement -- a player with no honour must pass (§3.1);
//   * bids strictly higher than the standing bid, except a hold matching it,
//     each standing bid held at most once (§3.1);
//   * conventional cue bids / invitations (§3.3): a single jump above the
//     minimum available bid promises the XIX, a double jump promises the XVIII;
//     such a bid is legal only if the player holds the promised card, there is
//     at most one cue bid per auction, the 4th seat after three passes never cue
//     bids, and an opening solo (a triple jump) is not a cue bid;
//   * the yielded game (§3.4): in the Three-Two-pass-pass position a pass by the
//     three-bidder is a yield promising the XX plus a high honour (so it is only
//     legal then), and obliges the declarer to call the XX;
//   * the 4th-seat trial bid (§3.5): after three passes the fourth player may
//     bid three even without an honour;
//   * the sole-bidder raise (§3.2): a player left in an uncontested plain three
//     may raise the contract (to two / one / solo) before play.
//
// Left for later: enforcing the downstream consequences of the conventions
// (calling the promised partner card, the pagát-ultimó obligation of a pagát cue
// bid, and the trial bid's talon-honour penalty) -- those belong to the talon /
// announcement phases, which are not modelled yet. The auction records what it
// determined (declarer, contract, and ObligatoryCalledCard()) for later use.

namespace open_spiel {
namespace hungarian_tarokk {

// Bids ordered from lowest (kThree) to highest (kSolo). The position in this
// enum is the bid's *rank*, which is what the auction compares; lower rank =
// weaker bid = more talon cards exchanged.
enum class Bid { kThree = 0, kTwo = 1, kOne = 2, kSolo = 3 };
inline constexpr int kNumBids = 4;

// Number of talon cards the declarer exchanges (three->3 ... solo->0).
int BidTalonExchange(Bid bid);
// Base payment for the game (three->1 ... solo->4).
int BidGameValue(Bid bid);
std::string BidToString(Bid bid);

// The tarokk that a cue bid or a yield obliges the declarer to call. kNone means
// no obligation, in which case the declarer calls the XX by default (decided in
// the announcement phase, which is not modelled yet).
enum class CalledCard { kNone, kXIX, kXVIII, kXX };
std::string CalledCardToString(CalledCard card);

// The facts about a player's nine-card hand that the auction needs.
struct PlayerBidInfo {
  bool has_honour = false;       // holds the pagát, the XXI or the Skíz
  bool has_high_honour = false;  // holds the XXI or the Skíz
  bool has_xx = false;
  bool has_xix = false;
  bool has_xviii = false;
};

// Action ids for the auction. They are placed immediately after the 42 card
// actions; hungarian_tarokk.cc static_asserts that kBiddingActionBase equals
// the game's kNumCards so the two action ranges never overlap.
inline constexpr Action kBiddingActionBase = 42;
inline constexpr Action kActionPass = kBiddingActionBase + 0;      // 42
inline constexpr Action kActionBidThree = kBiddingActionBase + 1;  // 43
inline constexpr Action kActionBidTwo = kBiddingActionBase + 2;    // 44
inline constexpr Action kActionBidOne = kBiddingActionBase + 3;    // 45
inline constexpr Action kActionBidSolo = kBiddingActionBase + 4;   // 46
inline constexpr Action kActionHold = kBiddingActionBase + 5;      // 47
inline constexpr int kNumBiddingActions = 6;

bool IsBiddingAction(Action action);
std::string BiddingActionToString(Action action);

// A self-contained, value-semantic auction. Copyable (so the owning game state
// can be cloned cheaply). The forehand (player 0) always bids first.
class BiddingState {
 public:
  BiddingState() = default;
  explicit BiddingState(const std::vector<PlayerBidInfo>& info);

  Player CurrentPlayer() const { return current_player_; }
  std::vector<Action> LegalActions() const;
  void ApplyAction(Action action);

  bool IsFinished() const { return finished_; }
  // True once finished with nobody having bid (the hand is thrown in).
  bool PassedOut() const { return finished_ && declarer_ == kInvalidPlayer; }
  Player Declarer() const { return declarer_; }
  Bid WinningBid() const { return winning_bid_; }

  // Conventional outcomes (see file comment). CueBidder() is kInvalidPlayer if
  // no cue bid was made; Yielded() is true for a yielded game;
  // ObligatoryCalledCard() is what the declarer must call (kNone = free / XX).
  Player CueBidder() const { return cue_bidder_; }
  CalledCard CuedCard() const { return cued_card_; }
  bool Yielded() const { return yielded_; }
  CalledCard ObligatoryCalledCard() const { return obligatory_called_card_; }

  std::string ToString() const;

 private:
  int NumPlayers() const { return static_cast<int>(info_.size()); }
  int NumActive() const;  // players who have not passed
  bool HasBid(Player p) const { return bid_rank_[p] >= 0; }
  bool CanHold(Player p) const;
  bool IsFourthAfterThreePasses() const;
  bool IsYieldPosition() const;
  // If a bid of `rank` by player `p` would be a cue bid (given the current
  // auction state), returns the promised card, otherwise kNone.
  CalledCard CueForBid(Player p, int rank) const;

  void ApplyBiddingAction(Action action);
  void ApplySoleRaise(Action action);
  void AdvanceOrFinish();
  void Finish(Player winner, int rank);

  std::vector<PlayerBidInfo> info_;
  Player current_player_ = kInvalidPlayer;
  std::vector<bool> passed_;
  std::vector<int> bid_rank_;   // highest bid each player has made (-1 = none)
  int current_bid_rank_ = -1;   // -1 = no bid has been made yet
  Player current_bidder_ = kInvalidPlayer;
  bool last_was_hold_ = false;  // the last positive call was a hold
  bool awaiting_sole_raise_ = false;

  Player cue_bidder_ = kInvalidPlayer;
  CalledCard cued_card_ = CalledCard::kNone;
  bool yielded_ = false;

  bool finished_ = false;
  Player declarer_ = kInvalidPlayer;
  Bid winning_bid_ = Bid::kThree;
  CalledCard obligatory_called_card_ = CalledCard::kNone;
  std::vector<std::pair<Player, Action>> calls_;  // public log, for ToString()
};

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_BIDDING_H_
