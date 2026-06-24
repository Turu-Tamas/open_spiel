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

#include "open_spiel/games/hungarian_tarokk/bidding.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarokk {

int BidTalonExchange(Bid bid) {
  // three -> 3, two -> 2, one -> 1, solo -> 0.
  return kNumBids - 1 - static_cast<int>(bid);
}

int BidGameValue(Bid bid) {
  // three -> 1, two -> 2, one -> 3, solo -> 4.
  return static_cast<int>(bid) + 1;
}

std::string BidToString(Bid bid) {
  switch (bid) {
    case Bid::kThree:
      return "Three";
    case Bid::kTwo:
      return "Two";
    case Bid::kOne:
      return "One";
    case Bid::kSolo:
      return "Solo";
  }
  SpielFatalError("Unknown bid.");
}

std::string CalledCardToString(CalledCard card) {
  switch (card) {
    case CalledCard::kNone:
      return "none";
    case CalledCard::kXIX:
      return "XIX";
    case CalledCard::kXVIII:
      return "XVIII";
    case CalledCard::kXX:
      return "XX";
  }
  SpielFatalError("Unknown called card.");
}

bool IsBiddingAction(Action action) {
  return action >= kBiddingActionBase &&
         action < kBiddingActionBase + kNumBiddingActions;
}

std::string BiddingActionToString(Action action) {
  switch (action) {
    case kActionPass:
      return "Pass";
    case kActionBidThree:
      return "Three";
    case kActionBidTwo:
      return "Two";
    case kActionBidOne:
      return "One";
    case kActionBidSolo:
      return "Solo";
    case kActionHold:
      return "Hold";
  }
  SpielFatalError(absl::StrCat("Not a bidding action: ", action));
}

BiddingState::BiddingState(const std::vector<PlayerBidInfo>& info)
    : info_(info),
      current_player_(0),
      passed_(info.size(), false),
      bid_rank_(info.size(), -1) {}

int BiddingState::NumActive() const {
  int n = 0;
  for (bool p : passed_) {
    if (!p) ++n;
  }
  return n;
}

bool BiddingState::CanHold(Player p) const {
  return current_bid_rank_ >= 0 && HasBid(p) && !last_was_hold_ &&
         p != current_bidder_;
}

bool BiddingState::IsFourthAfterThreePasses() const {
  // No bid has been made and only the current player is still in -- i.e. the
  // first three seats all passed and the fourth seat is to act.
  return current_bid_rank_ == -1 && NumActive() == 1 &&
         !passed_[current_player_];
}

bool BiddingState::IsYieldPosition() const {
  // The standing bid is a two, only the three-bidder and the two-bidder remain,
  // and it is the three-bidder's turn.
  return current_bid_rank_ == 1 && NumActive() == 2 &&
         bid_rank_[current_player_] == 0 && current_bidder_ != current_player_;
}

CalledCard BiddingState::CueForBid(Player p, int rank) const {
  if (cue_bidder_ != kInvalidPlayer) return CalledCard::kNone;  // one cue only
  if (IsFourthAfterThreePasses()) return CalledCard::kNone;     // never a cue
  // The minimum way to stay in is to hold the standing bid if the player can,
  // otherwise to make the lowest legal overbid. A jump is measured from there.
  int reference = CanHold(p) ? current_bid_rank_ : (current_bid_rank_ + 1);
  int jump = rank - reference;
  SPIEL_CHECK_LE(jump, 2); // a greater jump is not possible
  if (jump == 0) return CalledCard::kNone;  // lowest legal overbid
  if (jump == 1) return CalledCard::kXIX;
  if (jump == 2) return CalledCard::kXVIII;
}

std::vector<Action> BiddingState::LegalActions() const {
  SPIEL_CHECK_FALSE(finished_);
  if (awaiting_sole_raise_) {
    // Keep the three (pass) or raise to two / one / solo.
    return {kActionPass, kActionBidTwo, kActionBidOne, kActionBidSolo};
  }

  Player p = current_player_;
  std::vector<Action> legal;

  // Pass. In the yield position a pass is a yield, legal only if the player can
  // yield (holds the XX and a high honour).
  if (IsYieldPosition()) {
    if (info_[p].has_xx && info_[p].has_high_honour) {
      legal.push_back(kActionPass);
    }
  } else {
    legal.push_back(kActionPass);
  }

  if (IsFourthAfterThreePasses()) {
    // The fourth seat after three passes: bids are never cue bids.
    if (info_[p].has_honour) {
      for (int r = 0; r < kNumBids; ++r) legal.push_back(kActionBidThree + r);
    } else {
      legal.push_back(kActionBidThree);  // trial bid -- three only
    }
    std::sort(legal.begin(), legal.end());
    return legal;
  }

  if (!info_[p].has_honour) {
    // Without an honour the player must pass.
    std::sort(legal.begin(), legal.end());
    return legal;
  }

  // Bids strictly higher than the standing bid. A jump is a cue bid and is only
  // legal if the player actually holds the promised card.
  for (int r = current_bid_rank_ + 1; r < kNumBids; ++r) {
    CalledCard cue = CueForBid(p, r);
    if (cue == CalledCard::kNone) {
      legal.push_back(kActionBidThree + r);  // a plain (non-cue) bid
    } else {
      bool holds =
          (cue == CalledCard::kXIX) ? info_[p].has_xix : info_[p].has_xviii;
      if (holds) legal.push_back(kActionBidThree + r);
    }
  }
  if (CanHold(p)) legal.push_back(kActionHold);

  std::sort(legal.begin(), legal.end());
  return legal;
}

void BiddingState::ApplyAction(Action action) {
  SPIEL_CHECK_FALSE(finished_);
  if (awaiting_sole_raise_) {
    ApplySoleRaise(action);
  } else {
    ApplyBiddingAction(action);
  }
}

void BiddingState::ApplyBiddingAction(Action action) {
  Player p = current_player_;
  calls_.push_back({p, action});
  if (action == kActionPass) {
    if (IsYieldPosition()) yielded_ = true;
    passed_[p] = true;
  } else if (action == kActionHold) {
    current_bidder_ = p;
    bid_rank_[p] = current_bid_rank_;
    last_was_hold_ = true;
  } else {
    int r = action - kActionBidThree;
    SPIEL_CHECK_GE(r, 0);
    SPIEL_CHECK_LT(r, kNumBids);
    CalledCard cue = CueForBid(p, r);
    if (cue != CalledCard::kNone) {
      cue_bidder_ = p;
      cued_card_ = cue;
    }
    current_bid_rank_ = r;
    current_bidder_ = p;
    bid_rank_[p] = r;
    last_was_hold_ = false;
  }
  AdvanceOrFinish();
}

void BiddingState::ApplySoleRaise(Action action) {
  calls_.push_back({declarer_, action});
  awaiting_sole_raise_ = false;
  if (action == kActionPass) {
    Finish(declarer_, 0);  // keep the three
  } else {
    int r = action - kActionBidThree;
    SPIEL_CHECK_GE(r, 1);
    SPIEL_CHECK_LT(r, kNumBids);
    Finish(declarer_, r);
  }
}

void BiddingState::AdvanceOrFinish() {
  if (NumActive() == 0) {
    finished_ = true;
    declarer_ = kInvalidPlayer;  // everybody passed -- the hand is thrown in
    return;
  }
  if (current_bidder_ != kInvalidPlayer) {
    bool others_all_passed = true;
    for (Player q = 0; q < NumPlayers(); ++q) {
      if (q != current_bidder_ && !passed_[q]) {
        others_all_passed = false;
        break;
      }
    }
    if (others_all_passed) {
      // An uncontested plain three lets the sole bidder raise first (§3.2).
      int num_bidders = 0;
      for (int r : bid_rank_) {
        if (r >= 0) ++num_bidders;
      }
      if (current_bid_rank_ == 0 && cue_bidder_ == kInvalidPlayer &&
          num_bidders == 1 && info_[current_bidder_].has_honour) {
        awaiting_sole_raise_ = true;
        declarer_ = current_bidder_;
        winning_bid_ = Bid::kThree;
        current_player_ = current_bidder_;
        return;
      }
      Finish(current_bidder_, current_bid_rank_);
      return;
    }
  }
  // Move to the next player who has not passed.
  Player q = current_player_;
  do {
    q = (q + 1) % NumPlayers();
  } while (passed_[q]);
  current_player_ = q;
}

void BiddingState::Finish(Player winner, int rank) {
  finished_ = true;
  declarer_ = winner;
  winning_bid_ = static_cast<Bid>(rank);
  if (yielded_) {
    obligatory_called_card_ = CalledCard::kXX;
  } else if (cue_bidder_ != kInvalidPlayer && cue_bidder_ != declarer_) {
    // A cue bid by another player obliges the declarer to call that card.
    obligatory_called_card_ = cued_card_;
  } else {
    obligatory_called_card_ = CalledCard::kNone;
  }
}

std::string BiddingState::ToString() const {
  std::vector<std::string> parts;
  parts.reserve(calls_.size());
  for (const std::pair<Player, Action>& call : calls_) {
    parts.push_back(
        absl::StrCat("P", call.first, ":", BiddingActionToString(call.second)));
  }
  std::string str = absl::StrJoin(parts, " ");
  if (finished_) {
    if (declarer_ == kInvalidPlayer) {
      absl::StrAppend(&str, " => passed out");
    } else {
      absl::StrAppend(&str, " => declarer P", declarer_, " (",
                      BidToString(winning_bid_), ")");
      if (obligatory_called_card_ != CalledCard::kNone) {
        absl::StrAppend(&str, " calls ",
                        CalledCardToString(obligatory_called_card_));
      }
    }
  }
  return str;
}

}  // namespace hungarian_tarokk
}  // namespace open_spiel
