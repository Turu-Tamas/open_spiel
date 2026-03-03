#include <optional>
#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

namespace open_spiel {
namespace hungarian_tarok {

void HungarianTarokState::StartBiddingPhase() {
  bidding_ = BiddingState{};

  common_state_.winning_bid_ = -1;
  common_state_.full_bid_ = false;
  common_state_.declarer_side_ = CommonState::AnnouncementSide{};
  common_state_.opponents_side_ = CommonState::AnnouncementSide{};
  common_state_.player_sides_.fill(Side::kOpponents);
  common_state_.tricks_.clear();
  common_state_.trick_winners_.clear();

  bidding_.can_bid_[HandLocationPlayer(common_state_.deck_[kSkiz])] = true;
  bidding_.can_bid_[HandLocationPlayer(common_state_.deck_[kPagat])] = true;
  bidding_.can_bid_[HandLocationPlayer(common_state_.deck_[kXXI])] = true;
}

Player HungarianTarokState::BiddingCurrentPlayer() const {
  return bidding_.current_player_;
}

std::optional<Bid> Bid::NextBid(BidType bid_type, bool first_bid) const {
  int result_number;
  bool result_is_hold;

  if (!is_hold && !first_bid) {
    result_number = number;
    result_is_hold = true;
  } else {
    result_number = number - 1;
    result_is_hold = false;
  }

  int skipped_bids = 0;
  if (bid_type == BidType::kStandard) {
    skipped_bids = 0;
  } else if (bid_type == BidType::kInvitXIX) {
    skipped_bids = 1;
    result_is_hold = false;
  } else if (bid_type == BidType::kInvitXVIII) {
    skipped_bids = 2;
    result_is_hold = false;
  }

  result_number -= skipped_bids;
  if (result_number < 0) {
    return std::nullopt;
  }
  return Bid{result_number, result_is_hold};
}

BidType Bid::GetBidTypeOf(Action action, bool first_bid) const {
  Bid bid = FromAction(action);
  SPIEL_CHECK_TRUE(NextBidCanBe(action, first_bid));

  int diff = number - bid.number;
  if (!is_hold && !first_bid) {
    diff += 1;
  }
  SPIEL_CHECK_GE(diff, 1);
  SPIEL_CHECK_LE(diff, 4);
  if (diff == 1) {
    return BidType::kStandard;
  } else if (diff == 2) {
    return BidType::kInvitXIX;
  } else if (diff == 3) {
    return BidType::kInvitXVIII;
  } else {  // diff == 4
    return BidType::kStraightSolo;
  }
}

bool Bid::NextBidCanBe(Action action, bool player_first_bid) const {
  Bid new_bid = FromAction(action);
  if (new_bid.number < number && !new_bid.is_hold) {
    return true;  // new number, not hold
  }
  if (new_bid.number == number && !is_hold && new_bid.is_hold &&
      !player_first_bid) {
    return true;  // hold at same number
  }
  return false;
}

std::vector<Action> HungarianTarokState::BiddingLegalActions() const {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());

  bool no_bids_yet = bidding_.winning_bid_ == Bid::NewInitialBid();
  bool final_player = bidding_.current_player_ == 3;
  bool can_bid = bidding_.can_bid_[bidding_.current_player_];

  if (final_player && no_bids_yet && !can_bid) {
    // trial three or pass
    return {Bid{3, false}.ToAction(), Bid::PassAction()};
  }
  if (!can_bid) {
    return {Bid::PassAction()};
  }

  std::vector<Action> actions;
  bool any_bid_legal =
      bidding_.bid_type_ != BidType::kStandard || (no_bids_yet && final_player);
  bool player_first_bid = !bidding_.has_bid_[bidding_.current_player_];
  for (Action action = Bid::MinAction(); action <= Bid::MaxAction(); ++action) {
    if (!bidding_.winning_bid_.NextBidCanBe(action, player_first_bid)) {
      continue;
    }

    if (any_bid_legal) {
      actions.push_back(action);
      continue;
    }

    BidType bid_type =
        bidding_.winning_bid_.GetBidTypeOf(action, player_first_bid);
    std::optional<Card> card = IndicatedCard(bid_type);
    if (!card.has_value() ||
        PlayerHoldsCard(bidding_.current_player_, card.value())) {
      actions.push_back(action);
    }
  }

  // yielding game is illegal without XX
  if (!bidding_.has_bid_[bidding_.current_player_] ||
      bidding_.winning_bid_ != Bid{2, false} ||
      (PlayerHoldsCard(bidding_.current_player_, MakeTarok(20)) &&
       PlayerHoldsOneOf(bidding_.current_player_, {kXXI, kSkiz}))) {
    actions.push_back(Bid::PassAction());
  }
  return actions;
}

void HungarianTarokState::BiddingDoApplyAction(Action action) {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());
  std::vector<Action> legal_actions = BiddingLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  if (action == Bid::PassAction()) {
    bidding_.can_bid_[bidding_.current_player_] = false;
    bidding_.has_passed_[bidding_.current_player_] = true;

    if (bidding_.has_bid_[bidding_.current_player_] &&
        bidding_.winning_bid_ == Bid{2, false}) {
      // Yielded game, XX must be called
      bidding_.bid_type_ = BidType::kYieldedGame;
      common_state_.mandatory_called_card_ = MakeTarok(20);
      common_state_.cue_bidder_ = bidding_.current_player_;
    }

    BiddingNextPlayer();
    return;
  }

  bool player_first_bid = !bidding_.has_bid_[bidding_.current_player_];
  BidType bid_type =
      bidding_.winning_bid_.GetBidTypeOf(action, player_first_bid);
  bidding_.winning_bid_ = Bid::FromAction(action);
  bidding_.has_bid_[bidding_.current_player_] = true;
  bidding_.last_bidder_ = bidding_.current_player_;

  if (bidding_.bid_type_ != BidType::kStandard) {
    // someone bid after cue bid, accepting it, making
    // calling the indicated card mandatory
    common_state_.mandatory_called_card_ = IndicatedCard(bidding_.bid_type_);
    // mandatory pagatulti after cue bid with pagat
    common_state_.mandatory_pagatulti_ =
        !PlayerHoldsOneOf(common_state_.cue_bidder_.value(), {kXXI, kSkiz});
  }
  // after a cue bid was already made, nothing counts as cue bid
  if (bid_type == BidType::kStraightSolo) {
    bidding_.bid_type_ = BidType::kStraightSolo;
  } else if (bid_type != BidType::kStandard &&
             bidding_.bid_type_ == BidType::kStandard) {
    // dont bid again after making a cue bid
    bidding_.can_bid_[bidding_.current_player_] = false;
    bidding_.has_passed_[bidding_.current_player_] = true;
    common_state_.cue_bidder_ = bidding_.current_player_;
    bidding_.bid_type_ = bid_type;
  }
  BiddingNextPlayer();
}

void HungarianTarokState::BiddingNextPlayer() {
  const Bid& current_bid = bidding_.winning_bid_;
  if (current_bid == Bid{0, true}) {
    // Maximum bid reached.
    bidding_.current_player_ = kTerminalPlayerId;
    return;
  }

  Player next_player = (bidding_.current_player_ + 1) % kNumPlayers;
  while (bidding_.has_passed_[next_player] &&
         next_player != bidding_.current_player_ &&
         next_player != bidding_.last_bidder_) {
    next_player = (next_player + 1) % kNumPlayers;
  }

  if (next_player == bidding_.last_bidder_) {
    // Back to last bidder, bidding over.
    bidding_.current_player_ = kTerminalPlayerId;
    return;
  }

  if (next_player == bidding_.current_player_) {
    // 4 passes, game over.
    bidding_.current_player_ = kTerminalPlayerId;
    bidding_.all_passed_ = true;
    return;
  }
  bidding_.current_player_ = next_player;
}

bool HungarianTarokState::BiddingPhaseOver() const {
  return bidding_.current_player_ == kTerminalPlayerId;
}

bool HungarianTarokState::BiddingGameOver() const {
  return bidding_.all_passed_;
}

std::string HungarianTarokState::BiddingActionToString(Player player,
                                                       Action action) const {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());
  std::vector<Action> legal_actions = BiddingLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  if (action == Bid::PassAction()) {
    return "Pass";
  }
  Bid bid = Bid::FromAction(action);
  const Bid& current_bid = bidding_.winning_bid_;
  if (bid.is_hold) {
    return absl::StrCat("Hold at ", bid.number);
  } else {
    std::string ivnit_str = " (Standard bid)";
    bool first_bid = !bidding_.has_bid_[bidding_.current_player_];
    if (bid == current_bid.NextBid(BidType::kInvitXIX, first_bid)) {
      ivnit_str = " (Cue bid XIX)";
    } else if (bid == current_bid.NextBid(BidType::kInvitXVIII, first_bid)) {
      ivnit_str = " (Cue bid XVIII)";
    }
    return absl::StrCat("Bid ", bid.number, ivnit_str);
  }
  return "Unknown action";
}

std::string HungarianTarokState::BiddingToString() const {
  return "Bidding Phase";
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
