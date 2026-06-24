#include "open_spiel/games/hungarian_tarokk/bidding.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarokk {

// Weak-to-strong index of a bid (kThree -> 0 ... kSolo -> 3).
int BidStrength(Bid bid) { return static_cast<int>(bid); }

bool BidStronger(Bid a, Bid b) { return BidStrength(a) > BidStrength(b); }

int BidTalonExchange(Bid bid) {
  // three -> 3, two -> 2, one -> 1, solo -> 0 (the strongest bid draws fewest).
  return kNumBids - 1 - BidStrength(bid);
}

int BidGameValue(Bid bid) {
  // three -> 1, two -> 2, one -> 3, solo -> 4.
  return BidStrength(bid) + 1;
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

Action BidToAction(Bid bid) { return kActionBidThree + BidStrength(bid); }

Bid ActionToBid(Action action) {
  SPIEL_CHECK_TRUE(IsBidAction(action));
  return static_cast<Bid>(action - kActionBidThree);
}

bool IsBidAction(Action action) {
  return action >= kActionBidThree && action <= kActionBidSolo;
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
      player_bid_(info.size()) {}

int BiddingState::NumActive() const {
  int n = 0;
  for (bool p : passed_) {
    if (!p) ++n;
  }
  return n;
}

bool BiddingState::CanHold(Player p) const {
  return current_bid_.has_value() && HasBid(p) && !last_was_hold_ &&
         p != current_bidder_;
}

bool BiddingState::IsFourthAfterThreePasses() const {
  // No bid has been made and only the current player is still in -- i.e. the
  // first three seats all passed and the fourth seat is to act.
  return !current_bid_.has_value() && NumActive() == 1 &&
         !passed_[current_player_];
}

bool BiddingState::IsYieldPosition() const {
  // The standing bid is a two, only the three-bidder and the two-bidder remain,
  // and it is the three-bidder's turn.
  return current_bid_ == Bid::kTwo && NumActive() == 2 &&
         player_bid_[current_player_] == Bid::kThree &&
         current_bidder_ != current_player_;
}

CalledCard BiddingState::CueForBid(Player p, Bid bid) const {
  if (cue_bidder_ != kInvalidPlayer) return CalledCard::kNone;  // one cue only
  if (IsFourthAfterThreePasses()) return CalledCard::kNone;     // never a cue
  // The "floor" is the weakest bid the player could make to stay in.
  // A jump is measured from there -- one level up cues the XIX, two
  // levels up cues the XVIII. Anything else (a non-jump, or an opening
  // solo (the only wider jump possible)) carries no cue.
  int floor_strength;
  if (!current_bid_.has_value()) {
    floor_strength = BidStrength(kWeakestBid);
  } else if (CanHold(p)) {
    floor_strength = BidStrength(*current_bid_);
  } else {
    floor_strength = BidStrength(*current_bid_) + 1;
  }
  int jump = BidStrength(bid) - floor_strength;
  if (jump == 1) return CalledCard::kXIX;
  if (jump == 2) return CalledCard::kXVIII;
  return CalledCard::kNone;
}

std::vector<Action> BiddingState::LegalActions() const {
  SPIEL_CHECK_FALSE(finished_);
  if (awaiting_sole_raise_) {
    // Keep the three (pass) or raise to a more valuable game.
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
      for (Bid bid : kAllBids) legal.push_back(BidToAction(bid));
    } else {
      legal.push_back(BidToAction(kWeakestBid));  // trial bid -- three only
    }
    std::sort(legal.begin(), legal.end());
    return legal;
  }

  if (!info_[p].has_honour) {
    // Without an honour the player must pass.
    std::sort(legal.begin(), legal.end());
    return legal;
  }

  // Bids strictly stronger than the standing bid. A jump is a cue bid and is
  // only legal if the player actually holds the promised card.
  for (Bid bid : kAllBids) {
    if (current_bid_.has_value() && !BidStronger(bid, *current_bid_)) continue;
    CalledCard cue = CueForBid(p, bid);
    if (cue == CalledCard::kNone) {
      legal.push_back(BidToAction(bid));
    } else {
      bool holds =
          (cue == CalledCard::kXIX) ? info_[p].has_xix : info_[p].has_xviii;
      if (holds) legal.push_back(BidToAction(bid));
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
    player_bid_[p] = current_bid_;
    last_was_hold_ = true;
  } else {
    Bid bid = ActionToBid(action);
    CalledCard cue = CueForBid(p, bid);
    if (cue != CalledCard::kNone) {
      cue_bidder_ = p;
      cued_card_ = cue;
    }
    current_bid_ = bid;
    current_bidder_ = p;
    player_bid_[p] = bid;
    last_was_hold_ = false;
  }
  AdvanceOrFinish();
}

void BiddingState::ApplySoleRaise(Action action) {
  calls_.push_back({declarer_, action});
  awaiting_sole_raise_ = false;
  if (action == kActionPass) {
    Finish(declarer_, kWeakestBid);
  } else {
    Finish(declarer_, ActionToBid(action));
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
      // The cue-bidder does not participate in the bidding after making
      // the cue bid, as if they have passed
      if (q != current_bidder_ && !passed_[q] && q != cue_bidder_) {
        others_all_passed = false;
        break;
      }
    }
    if (others_all_passed) {
      // An uncontested plain three lets the sole bidder raise first (§3.2).
      int num_bidders = 0;
      for (const absl::optional<Bid>& b : player_bid_) {
        if (b.has_value()) ++num_bidders;
      }
      if (current_bid_ == Bid::kThree && cue_bidder_ == kInvalidPlayer &&
          num_bidders == 1 && info_[current_bidder_].has_honour) {
        awaiting_sole_raise_ = true;
        declarer_ = current_bidder_;
        winning_bid_ = Bid::kThree;
        current_player_ = current_bidder_;
        return;
      }
      Finish(current_bidder_, *current_bid_);
      return;
    }
  }

  // The cue-bidder does not participate in the bidding after making
  // the cue bid, as if they have passed
  Player q = current_player_;
  do {
    q = (q + 1) % NumPlayers();
  } while (passed_[q] || q == cue_bidder_);
  current_player_ = q;
}

void BiddingState::Finish(Player winner, Bid bid) {
  finished_ = true;
  declarer_ = winner;
  winning_bid_ = bid;
  if (yielded_) {
    obligatory_called_card_ = CalledCard::kXX;
  } else if (cue_bidder_ != kInvalidPlayer && cue_bidder_ != declarer_) {
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
