#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"
#include "open_spiel/games/hungarian_tarok/scoring.h"

namespace open_spiel {
namespace hungarian_tarok {

std::ostream& operator<<(std::ostream& os, const PhaseType& phase) {
  switch (phase) {
    case PhaseType::kSetup:
      os << "Setup";
      break;
    case PhaseType::kBidding:
      os << "Bidding";
      break;
    case PhaseType::kTalon:
      os << "Talon";
      break;
    case PhaseType::kSkart:
      os << "Skart";
      break;
    case PhaseType::kAnnouncements:
      os << "Announcements";
      break;
    case PhaseType::kPlay:
      os << "Play";
      break;
  }
  os << " Phase";
  return os;
}

// Generic phase dispatch.
Player HungarianTarokState::PhaseCurrentPlayer() const {
  switch (current_phase_) {
    case PhaseType::kSetup:
      return SetupCurrentPlayer();
    case PhaseType::kBidding:
      return BiddingCurrentPlayer();
    case PhaseType::kTalon:
      return TalonCurrentPlayer();
    case PhaseType::kSkart:
      return SkartCurrentPlayer();
    case PhaseType::kAnnouncements:
      return AnnouncementsCurrentPlayer();
    case PhaseType::kPlay:
      return PlayCurrentPlayer();
  }
  SpielFatalError("Unknown phase type");
}

std::vector<Action> HungarianTarokState::PhaseLegalActions() const {
  switch (current_phase_) {
    case PhaseType::kSetup:
      return SetupLegalActions();
    case PhaseType::kBidding:
      return BiddingLegalActions();
    case PhaseType::kTalon:
      return TalonLegalActions();
    case PhaseType::kSkart:
      return SkartLegalActions();
    case PhaseType::kAnnouncements:
      return AnnouncementsLegalActions();
    case PhaseType::kPlay:
      return PlayLegalActions();
  }
  SpielFatalError("Unknown phase type");
}

void HungarianTarokState::PhaseDoApplyAction(Action action) {
  switch (current_phase_) {
    case PhaseType::kSetup:
      return SetupDoApplyAction(action);
    case PhaseType::kBidding:
      return BiddingDoApplyAction(action);
    case PhaseType::kTalon:
      return TalonDoApplyAction(action);
    case PhaseType::kSkart:
      return SkartDoApplyAction(action);
    case PhaseType::kAnnouncements:
      return AnnouncementsDoApplyAction(action);
    case PhaseType::kPlay:
      return PlayDoApplyAction(action);
  }
  SpielFatalError("Unknown phase type");
}

bool HungarianTarokState::PhaseOver() const {
  switch (current_phase_) {
    case PhaseType::kSetup:
      return SetupPhaseOver();
    case PhaseType::kBidding:
      return BiddingPhaseOver();
    case PhaseType::kTalon:
      return TalonPhaseOver();
    case PhaseType::kSkart:
      return SkartPhaseOver();
    case PhaseType::kAnnouncements:
      return AnnouncementsPhaseOver();
    case PhaseType::kPlay:
      return PlayPhaseOver();
  }
  SpielFatalError("Unknown phase type");
}

bool HungarianTarokState::GameOver() const {
  switch (current_phase_) {
    case PhaseType::kBidding:
      return BiddingGameOver();
    case PhaseType::kPlay:
      return PlayGameOver();
    case PhaseType::kTalon:
      return TalonGameOver();
    default:
      return false;
  }
}

std::vector<double> HungarianTarokState::PhaseReturns() const {
  switch (current_phase_) {
    case PhaseType::kPlay:
      return PlayReturns();
    case PhaseType::kTalon:
      return TalonReturns();  // for trial three ending
    default:
      return std::vector<double>(kNumPlayers, 0.0);
  }
}

std::string HungarianTarokState::PhaseActionToString(Player player,
                                                     Action action) const {
  switch (current_phase_) {
    case PhaseType::kSetup:
      return SetupActionToString(player, action);
    case PhaseType::kBidding:
      return BiddingActionToString(player, action);
    case PhaseType::kTalon:
      return TalonActionToString(player, action);
    case PhaseType::kSkart:
      return SkartActionToString(player, action);
    case PhaseType::kAnnouncements:
      return AnnouncementsActionToString(player, action);
    case PhaseType::kPlay:
      return PlayActionToString(player, action);
  }
  SpielFatalError("Unknown phase type");
}

std::string HungarianTarokState::PhaseToString() const {
  switch (current_phase_) {
    case PhaseType::kSetup:
      return SetupToString();
    case PhaseType::kBidding:
      return BiddingToString();
    case PhaseType::kTalon:
      return TalonToString();
    case PhaseType::kSkart:
      return SkartToString();
    case PhaseType::kAnnouncements:
      return AnnouncementsToString();
    case PhaseType::kPlay:
      return PlayToString();
  }
  SpielFatalError("Unknown phase type");
}

void HungarianTarokState::AdvancePhase() {
  SPIEL_CHECK_TRUE(PhaseOver());
  switch (current_phase_) {
    case PhaseType::kSetup:
      current_phase_ = PhaseType::kBidding;
      StartBiddingPhase();
      return;
    case PhaseType::kBidding: {
      int bidder_count = absl::c_count(bidding_.has_bid, true);
      common_state_.full_bid_ = (bidder_count == 3);
      common_state_.winning_bid_ = bidding_.winning_bid_.number;
      common_state_.trial_three_ =
          common_state_.declarer_ == 3 && !bidding_.can_bid[3];
      current_phase_ = PhaseType::kTalon;
      StartTalonPhase();
      return;
    }
    case PhaseType::kTalon:
      current_phase_ = PhaseType::kSkart;
      StartSkartPhase();
      return;
    case PhaseType::kSkart:
      current_phase_ = PhaseType::kAnnouncements;
      StartAnnouncementsPhase();
      return;
    case PhaseType::kAnnouncements:
      current_phase_ = PhaseType::kPlay;
      StartPlayPhase();
      return;
    case PhaseType::kPlay:
      SpielFatalError("No next phase after play");
  }
  SpielFatalError("Unknown phase type");
}

// Setup.
Player HungarianTarokState::SetupCurrentPlayer() const {
  return SetupPhaseOver() ? kTerminalPlayerId : kChancePlayerId;
}

std::vector<Action> HungarianTarokState::SetupLegalActions() const {
  SPIEL_CHECK_FALSE(SetupPhaseOver());
  std::vector<Action> actions;
  for (int player = 0; player < kNumPlayers; ++player) {
    if (setup_.player_hands_sizes[player] < kPlayerHandSize) {
      actions.push_back(player);
    }
  }
  return actions;
}

void HungarianTarokState::SetupDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kNumPlayers);
  SPIEL_CHECK_FALSE(SetupPhaseOver());

  if (setup_.current_card == kPagat) {
    common_state_.pagat_holder_ = action;
  }

  // Action is the player ID who receives the next card.
  common_state_.deck_[setup_.current_card] = PlayerHandLocation(action);
  setup_.player_hands_sizes[action]++;
  setup_.current_card++;
}

bool HungarianTarokState::SetupPhaseOver() const {
  return setup_.current_card >= kPlayerHandSize * kNumPlayers;
}

std::string HungarianTarokState::SetupActionToString(Player player,
                                                     Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kNumPlayers);
  return absl::StrCat("Deal card ", CardToString(setup_.current_card),
                      " to player ", action);
}

std::string HungarianTarokState::SetupToString() const { return "Setup Phase"; }

// Bidding.
void HungarianTarokState::StartBiddingPhase() {
  bidding_ = BiddingState{};

  common_state_.declarer_ = 0;
  common_state_.winning_bid_ = -1;
  common_state_.full_bid_ = false;
  common_state_.declarer_side_ = CommonState::AnnouncementSide{};
  common_state_.opponents_side_ = CommonState::AnnouncementSide{};
  common_state_.player_sides_.fill(Side::kOpponents);
  common_state_.tricks_.clear();
  common_state_.trick_winners_.clear();

  bidding_.can_bid[HandLocationPlayer(common_state_.deck_[kSkiz])] = true;
  bidding_.can_bid[HandLocationPlayer(common_state_.deck_[kPagat])] = true;
  bidding_.can_bid[HandLocationPlayer(common_state_.deck_[kXXI])] = true;
}

Player HungarianTarokState::BiddingCurrentPlayer() const {
  return bidding_.current_player;
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
  bool final_player = bidding_.current_player == 3;
  bool can_bid = bidding_.can_bid[bidding_.current_player];

  if (final_player && no_bids_yet && !can_bid) {
    // trial three or pass
    return {Bid{3, false}.ToAction(), Bid::PassAction()};
  }
  if (!can_bid) {
    return {Bid::PassAction()};
  }

  std::vector<Action> actions;
  bool any_bid_legal =
      bidding_.bid_type != BidType::kStandard || (no_bids_yet && final_player);
  bool player_first_bid = !bidding_.has_bid[bidding_.current_player];
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
        PlayerHoldsCard(bidding_.current_player, card.value())) {
      actions.push_back(action);
    }
  }

  // yielding game is illegal without XX
  if (!bidding_.has_bid[bidding_.current_player] ||
      bidding_.winning_bid_ != Bid{2, false} ||
      (PlayerHoldsCard(bidding_.current_player, MakeTarok(20)) &&
       PlayerHoldsOneOf(bidding_.current_player, {kXXI, kSkiz}))) {
    actions.push_back(Bid::PassAction());
  }
  return actions;
}

void HungarianTarokState::BiddingDoApplyAction(Action action) {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());
  std::vector<Action> legal_actions = BiddingLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  if (action == Bid::PassAction()) {
    bidding_.can_bid[bidding_.current_player] = false;
    bidding_.has_passed[bidding_.current_player] = true;

    if (bidding_.has_bid[bidding_.current_player] &&
        bidding_.winning_bid_ == Bid{2, false}) {
      // Yielded game, XX must be called
      bidding_.bid_type = BidType::kYieldedGame;
      common_state_.mandatory_called_card_ = MakeTarok(20);
      common_state_.cue_bidder_ = bidding_.current_player;
    }

    BiddingNextPlayer();
    return;
  }

  bool player_first_bid = !bidding_.has_bid[bidding_.current_player];
  BidType bid_type =
      bidding_.winning_bid_.GetBidTypeOf(action, player_first_bid);
  bidding_.winning_bid_ = Bid::FromAction(action);
  bidding_.has_bid[bidding_.current_player] = true;
  common_state_.declarer_ = bidding_.current_player;

  if (bidding_.bid_type != BidType::kStandard) {
    // someone bid after cue bid, accepting it, making
    // calling the indicated card mandatory
    common_state_.mandatory_called_card_ = IndicatedCard(bidding_.bid_type);
    // mandatory pagatulti after cue bid with pagat
    common_state_.mandatory_pagatulti_ =
        !PlayerHoldsOneOf(common_state_.cue_bidder_.value(), {kXXI, kSkiz});
  }
  // after a cue bid was already made, nothing counts as cue bid
  if (bid_type != BidType::kStandard &&
      bidding_.bid_type == BidType::kStandard) {
    // dont bid again after making a cue bid
    bidding_.can_bid[bidding_.current_player] = false;
    bidding_.has_passed[bidding_.current_player] = true;
    common_state_.cue_bidder_ = bidding_.current_player;
    bidding_.bid_type = bid_type;
  }
  BiddingNextPlayer();
}

void HungarianTarokState::BiddingNextPlayer() {
  const Bid& current_bid = bidding_.winning_bid_;
  if (current_bid == Bid{0, true}) {
    // Maximum bid reached.
    bidding_.current_player = kTerminalPlayerId;
    return;
  }

  Player next_player = (bidding_.current_player + 1) % kNumPlayers;
  while (bidding_.has_passed[next_player] &&
         next_player != bidding_.current_player &&
         next_player != common_state_.declarer_) {
    next_player = (next_player + 1) % kNumPlayers;
  }

  if (next_player == common_state_.declarer_) {
    // Back to declarer, bidding over.
    bidding_.current_player = kTerminalPlayerId;
    return;
  }

  if (next_player == bidding_.current_player) {
    // 4 passes, game over.
    bidding_.current_player = kTerminalPlayerId;
    bidding_.all_passed = true;
    return;
  }
  bidding_.current_player = next_player;
}

bool HungarianTarokState::BiddingPhaseOver() const {
  return bidding_.current_player == kTerminalPlayerId;
}

bool HungarianTarokState::BiddingGameOver() const {
  return bidding_.all_passed;
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
    bool first_bid = !bidding_.has_bid[bidding_.current_player];
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

// Talon dealing.
void HungarianTarokState::StartTalonPhase() {
  talon_ = TalonState{};

  const Player declarer = common_state_.declarer_;
  const int declarer_cards_to_take = common_state_.winning_bid_;

  talon_.cards_to_take.fill(0);
  talon_.cards_to_take[declarer] = declarer_cards_to_take;
  talon_.current_player = declarer;

  int remaining_cards = kTalonSize - declarer_cards_to_take;
  Player player = (declarer + 1) % kNumPlayers;
  while (remaining_cards > 0) {
    if (player != declarer) {
      talon_.cards_to_take[player]++;
      remaining_cards--;
    }
    player = (player + 1) % kNumPlayers;
  }

  int index = 0;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] == CardLocation::kTalon) {
      talon_.talon_cards[index++] = card;
    }
  }
  SPIEL_CHECK_EQ(index, kTalonSize);
}

Player HungarianTarokState::TalonCurrentPlayer() const {
  return TalonPhaseOver() ? kTerminalPlayerId : kChancePlayerId;
}

std::vector<Action> HungarianTarokState::TalonLegalActions() const {
  SPIEL_CHECK_FALSE(TalonPhaseOver());
  std::vector<Action> actions;
  for (int i = 0; i < kTalonSize; ++i) {
    if (!talon_.talon_taken[i]) {
      actions.push_back(i);
    }
  }
  return actions;
}

bool HungarianTarokState::TrialThreeGameEnded() {
  if (talon_.current_player == common_state_.declarer_ &&
      common_state_.trial_three_) {
    if (PlayerHoldsCard(common_state_.declarer_, kPagat) ||
        PlayerHoldsCard(common_state_.declarer_, kSkiz) ||
        PlayerHoldsCard(common_state_.declarer_, kXXI)) {
      // declarer took an honour, continue
      return false;
    } else {
      return true;
    }
  }
  return false;
}

void HungarianTarokState::TalonDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kTalonSize);
  SPIEL_CHECK_FALSE(talon_.talon_taken[action]);
  SPIEL_CHECK_FALSE(TalonPhaseOver());

  talon_.talon_taken[action] = true;
  common_state_.deck_[talon_.talon_cards[action]] =
      PlayerHandLocation(talon_.current_player);
  talon_.talon_taken_count++;
  talon_.cards_to_take[talon_.current_player]--;

  if (talon_.talon_taken_count == kTalonSize) {
    talon_.current_player = kTerminalPlayerId;
  } else if (talon_.cards_to_take[talon_.current_player] == 0) {
    if (TrialThreeGameEnded()) {
      talon_.current_player = kTerminalPlayerId;
      talon_.rewards.assign(kNumPlayers, +3.0);
      talon_.rewards[common_state_.declarer_] = -9.0;
      talon_.game_over = true;
    } else {
      talon_.current_player = (talon_.current_player + 1) % kNumPlayers;
    }
  }
}

std::vector<double> HungarianTarokState::TalonReturns() const {
  return talon_.rewards;
}

bool HungarianTarokState::TalonGameOver() const {
  return talon_.current_player == kTerminalPlayerId && talon_.game_over;
}

bool HungarianTarokState::TalonPhaseOver() const {
  return talon_.current_player == kTerminalPlayerId;
}

std::string HungarianTarokState::TalonActionToString(Player player,
                                                     Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kTalonSize);
  return absl::StrCat("Take talon card ", action);
}

std::string HungarianTarokState::TalonToString() const {
  return "Dealing Talon Phase";
}

// Skart.
void HungarianTarokState::StartSkartPhase() {
  skart_ = SkartState{};
  skart_.hand_sizes.fill(0);
  for (Card card = 0; card < kDeckSize; ++card) {
    CardLocation location = common_state_.deck_[card];
    if (IsPlayerHand(location)) {
      skart_.hand_sizes[HandLocationPlayer(location)]++;
    }
  }
  skart_.current_player = common_state_.declarer_;
}

Player HungarianTarokState::SkartCurrentPlayer() const {
  return skart_.current_player;
}

std::vector<Action> HungarianTarokState::SkartLegalActions() const {
  SPIEL_CHECK_FALSE(SkartPhaseOver());
  std::vector<Action> actions;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] ==
        PlayerHandLocation(skart_.current_player)) {
      actions.push_back(card);
    }
  }
  return actions;
}

void HungarianTarokState::SkartDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  SPIEL_CHECK_EQ(common_state_.deck_[action],
                 PlayerHandLocation(skart_.current_player));
  SPIEL_CHECK_FALSE(SkartPhaseOver());

  if (skart_.current_player == common_state_.declarer_) {
    common_state_.deck_[action] = CardLocation::kDeclarerSkart;
  } else {
    common_state_.deck_[action] = CardLocation::kOpponentsSkart;
  }
  skart_.cards_discarded++;

  skart_.hand_sizes[skart_.current_player]--;
  if (skart_.hand_sizes[skart_.current_player] == kPlayerHandSize) {
    skart_.current_player = (skart_.current_player + 1) % kNumPlayers;
    if (skart_.hand_sizes[skart_.current_player] == 0) {
      skart_.current_player = kTerminalPlayerId;
    }
  }
}

bool HungarianTarokState::SkartPhaseOver() const {
  return skart_.cards_discarded == kTalonSize;
}

std::string HungarianTarokState::SkartActionToString(Player player,
                                                     Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  return absl::StrCat("Discard card ", CardToString(static_cast<Card>(action)));
}

std::string HungarianTarokState::SkartToString() const {
  return absl::StrCat("Skart Phase, ", skart_.cards_discarded,
                      "/6 cards discarded\n",
                      DeckToString(common_state_.deck_));
}

// Announcements.
namespace {

constexpr std::array<const char*, kNumAnnouncementTypes> kAnnouncementTypeNames{
    "Four Kings",   "Tuletroa",    "Double Game",  "Volat",
    "Pagat Ultimo", "XXI Capture", "Eight Taroks", "Nine Taroks"};

const char* AnnouncementLevelPrefix(AnnouncementAction::Level level) {
  switch (level) {
    case AnnouncementAction::Level::kAnnounce:
      return "Announce ";
    case AnnouncementAction::Level::kContra:
      return "Contra ";
    case AnnouncementAction::Level::kReContra:
      return "Re-Contra ";
  }
  SpielFatalError("Unknown announcement level");
}

bool IsContraAllowedFor(AnnouncementType type) {
  return type != AnnouncementType::kEightTaroks &&
         type != AnnouncementType::kNineTaroks;
}

bool IsBlockedByVolat(AnnouncementType type) {
  return type == AnnouncementType::kFourKings ||
         type == AnnouncementType::kDoubleGame;
}

}  // namespace

void HungarianTarokState::StartAnnouncementsPhase() {
  common_state_.partner_ = std::nullopt;
  announcements_ = AnnouncementsState{};
  announcements_.current_player = common_state_.declarer_;
  announcements_.tarok_counts.fill(0);
  for (Card card = 0; card < kDeckSize; ++card) {
    if (CardSuit(card) == Suit::kTarok) {
      CardLocation location = common_state_.deck_[card];
      if (IsPlayerHand(location)) {
        announcements_.tarok_counts[HandLocationPlayer(location)]++;
      }
    }
  }
}

Player HungarianTarokState::AnnouncementsCurrentPlayer() const {
  return announcements_.current_player;
}

bool HungarianTarokState::IsDeclarerSidePlayer(Player player) const {
  return player == common_state_.declarer_ ||
         (common_state_.partner_.has_value() &&
          player == *common_state_.partner_);
}

CommonState::AnnouncementSide& HungarianTarokState::CurrentAnnouncementSide() {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? common_state_.declarer_side_
             : common_state_.opponents_side_;
}

CommonState::AnnouncementSide& HungarianTarokState::OtherAnnouncementSide() {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? common_state_.opponents_side_
             : common_state_.declarer_side_;
}

const CommonState::AnnouncementSide&
HungarianTarokState::CurrentAnnouncementSide() const {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? common_state_.declarer_side_
             : common_state_.opponents_side_;
}

const CommonState::AnnouncementSide&
HungarianTarokState::OtherAnnouncementSide() const {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? common_state_.opponents_side_
             : common_state_.declarer_side_;
}

bool HungarianTarokState::CanAnnounceTuletroa() const {
  if (CurrentAnnouncementSide()
          .announced[static_cast<int>(AnnouncementType::kTuletroa)]) {
    return false;
  }
  if (CurrentAnnouncementSide()
          .announced[static_cast<int>(AnnouncementType::kVolat)]) {
    return false;
  }

  bool is_declarer = announcements_.current_player == common_state_.declarer_;
  bool is_partner = announcements_.current_player == common_state_.partner_;

  // after a cue bid, tuletroa from declarer means skiz or XXI
  if (announcements_.current_player == common_state_.declarer_ &&
      common_state_.mandatory_called_card_.has_value()) {
    return PlayerHoldsCard(common_state_.declarer_, kXXI) ||
           PlayerHoldsCard(common_state_.declarer_, kSkiz);
  }
  // in a full bid, declarer can only announce tuletroa if they have the skiz
  if (common_state_.full_bid_ && is_declarer && announcements_.first_round) {
    return PlayerHoldsCard(common_state_.declarer_, kSkiz);
  }
  // if not a full bid, tuletroa from declarer means XXI and skiz
  if (is_declarer && announcements_.first_round) {
    return PlayerHoldsCard(common_state_.declarer_, kXXI) &&
           PlayerHoldsCard(common_state_.declarer_, kSkiz);
  }
  // as the partner, tuletroa means XXI or skiz
  if (common_state_.partner_.has_value() && is_partner &&
      announcements_.first_round) {
    return PlayerHoldsCard(*common_state_.partner_, kXXI) ||
           PlayerHoldsCard(*common_state_.partner_, kSkiz);
  }
  return true;
}

bool HungarianTarokState::CanAnnounceType(AnnouncementType type) const {
  const CommonState::AnnouncementSide& current_side = CurrentAnnouncementSide();
  const int type_index = static_cast<int>(type);
  if (current_side.announced[type_index]) return false;

  if (type == AnnouncementType::kTuletroa) {
    return CanAnnounceTuletroa();
  }

  if (type == AnnouncementType::kEightTaroks) {
    return announcements_.tarok_counts[announcements_.current_player] == 8;
  }
  if (type == AnnouncementType::kNineTaroks) {
    return announcements_.tarok_counts[announcements_.current_player] == 9;
  }

  if (IsBlockedByVolat(type)) {
    return !current_side.announced[static_cast<int>(AnnouncementType::kVolat)];
  }

  return true;
}

void HungarianTarokState::AddAnnounceActions(
    std::vector<Action>& actions) const {
  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    const AnnouncementType type = static_cast<AnnouncementType>(i);
    if (CanAnnounceType(type)) {
      actions.push_back(AnnouncementAction::AnnounceAction(type));
    }
  }
}

void HungarianTarokState::AddContraActions(std::vector<Action>& actions) const {
  const CommonState::AnnouncementSide& other_side = OtherAnnouncementSide();
  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    const AnnouncementType type = static_cast<AnnouncementType>(i);
    if (!IsContraAllowedFor(type)) continue;
    if (other_side.announced[i] && other_side.contra_level[i] % 2 == 0 &&
        other_side.contra_level[i] <= kMaxContraLevel) {
      actions.push_back(AnnouncementAction::ContraAction(type));
    }
  }
}

void HungarianTarokState::AddReContraActions(
    std::vector<Action>& actions) const {
  const CommonState::AnnouncementSide& current_side = CurrentAnnouncementSide();
  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    const AnnouncementType type = static_cast<AnnouncementType>(i);
    if (current_side.contra_level[i] % 2 == 1 &&
        current_side.contra_level[i] <= kMaxContraLevel) {
      actions.push_back(AnnouncementAction::ReContraAction(type));
    }
  }
}

std::vector<Action> HungarianTarokState::AnnouncementsLegalActions() const {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  if (!announcements_.partner_called) {
    if (!common_state_.mandatory_called_card_.has_value() &&
        PlayerHoldsCard(announcements_.current_player, MakeTarok(20))) {
      return {kAnnouncementsActionCallPartner, kAnnouncementsActionCallSelf};
    }
    return {kAnnouncementsActionCallPartner};
  }

  std::vector<Action> actions;
  AddAnnounceActions(actions);
  AddContraActions(actions);
  AddReContraActions(actions);

  if (announcements_.mandatory_announcements_.empty()) {
    actions.push_back(AnnouncementAction::PassAction());
  }
  return actions;
}

void HungarianTarokState::AnnouncementsCallPartner(Action action) {
  auto get_called_card = [&]() -> Card {
    if (common_state_.mandatory_called_card_.has_value()) {
      return common_state_.mandatory_called_card_.value();
    }

    Card highest_missing;
    for (int rank = 20;; --rank) {
      Card card = MakeTarok(rank);
      if (!PlayerHoldsCard(announcements_.current_player, card)) {
        highest_missing = card;
        break;
      }
    }
    return highest_missing;
  };

  if (action == kAnnouncementsActionCallPartner) {
    CardLocation location = common_state_.deck_[get_called_card()];
    common_state_.partner_ =
        IsPlayerHand(location)
            ? std::optional<Player>(HandLocationPlayer(location))
            : std::nullopt;
  } else {
    common_state_.partner_ = std::nullopt;
  }

  if (common_state_.partner_ == common_state_.declarer_) {
    throw std::invalid_argument("Partner cannot be the declarer themselves.");
  }
  SPIEL_CHECK_NE(common_state_.partner_, common_state_.declarer_);
  announcements_.partner_called = true;
  announcements_.last_to_speak = common_state_.declarer_;

  for (Player p = 0; p < kNumPlayers; ++p) {
    common_state_.player_sides_[p] =
        (p == common_state_.declarer_ ||
         (common_state_.partner_.has_value() && p == *common_state_.partner_))
            ? Side::kDeclarer
            : Side::kOpponents;
  }
}

void HungarianTarokState::AnnouncementsDoApplyAction(Action action) {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  std::vector<Action> legal_actions = AnnouncementsLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  if (!announcements_.partner_called) {
    AnnouncementsCallPartner(action);
    return;
  }

  if (action == AnnouncementAction::PassAction()) {
    announcements_.current_player =
        (announcements_.current_player + 1) % kNumPlayers;
    if (announcements_.current_player == announcements_.last_to_speak) {
      announcements_.current_player = kTerminalPlayerId;
    }
    if (announcements_.current_player == common_state_.declarer_) {
      announcements_.first_round = false;
    }
    if (common_state_.mandatory_pagatulti_ &&
        announcements_.current_player == common_state_.partner_ &&
        !CurrentAnnouncementSide()
             .announced[static_cast<int>(AnnouncementType::kPagatUltimo)]) {
      announcements_.mandatory_announcements_.push_back(
          AnnouncementType::kPagatUltimo);
    }
    return;
  }

  AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
  CommonState::AnnouncementSide& current_side = CurrentAnnouncementSide();
  CommonState::AnnouncementSide& other_side = OtherAnnouncementSide();
  int type_index = static_cast<int>(ann_action.type);
  switch (ann_action.level) {
    case AnnouncementAction::Level::kAnnounce:
      current_side.announced[type_index] = true;
      break;
    case AnnouncementAction::Level::kContra:
      other_side.contra_level[type_index]++;
      break;
    case AnnouncementAction::Level::kReContra:
      current_side.contra_level[type_index]++;
      break;
  }
  announcements_.last_to_speak = announcements_.current_player;

  auto it =
      absl::c_find(announcements_.mandatory_announcements_, ann_action.type);
  if (it != announcements_.mandatory_announcements_.end()) {
    announcements_.mandatory_announcements_.erase(it);
  }

  if (ann_action.type == AnnouncementType::kPagatUltimo &&
      ann_action.level == AnnouncementAction::Level::kAnnounce) {
    if (announcements_.tarok_counts[announcements_.current_player] == 8 &&
        !CurrentAnnouncementSide()
             .announced[static_cast<int>(AnnouncementType::kEightTaroks)]) {
      announcements_.mandatory_announcements_.push_back(
          AnnouncementType::kEightTaroks);
    } else if (announcements_.tarok_counts[announcements_.current_player] ==
                   9 &&
               !CurrentAnnouncementSide().announced[static_cast<int>(
                   AnnouncementType::kNineTaroks)]) {
      announcements_.mandatory_announcements_.push_back(
          AnnouncementType::kNineTaroks);
    }
  }
}

bool HungarianTarokState::AnnouncementsPhaseOver() const {
  return announcements_.current_player == kTerminalPlayerId;
}

std::string HungarianTarokState::AnnouncementsActionToString(
    Player player, Action action) const {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  std::vector<Action> legal_actions = AnnouncementsLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());
  if (!announcements_.partner_called) {
    if (action == kAnnouncementsActionCallPartner) {
      return "Call partner";
    }
    return "Call self (XX)";
  }

  if (action == AnnouncementAction::PassAction()) {
    return "Pass";
  }

  AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
  const char* level_str = AnnouncementLevelPrefix(ann_action.level);
  const char* type_str =
      kAnnouncementTypeNames[static_cast<int>(ann_action.type)];
  return absl::StrCat(level_str, type_str);
}

std::string HungarianTarokState::AnnouncementsToString() const {
  return absl::StrCat("Announcements Phase\n",
                      "current player: ", announcements_.current_player, "\n",
                      DeckToString(common_state_.deck_));
}

// Play.
void HungarianTarokState::StartPlayPhase() {
  play_ = PlayState{};
  play_.current_player = common_state_.declarer_;
  play_.trick_caller = play_.current_player;
  play_.trick_cards.clear();
  play_.round = 0;
  common_state_.tricks_.clear();
  common_state_.trick_winners_.clear();
}

Player HungarianTarokState::PlayCurrentPlayer() const {
  return play_.current_player;
}

std::vector<Action> HungarianTarokState::PlayLegalActions() const {
  SPIEL_CHECK_FALSE(PlayPhaseOver());

  std::vector<Action> actions;
  auto can_play = [&](Card card) {
    return play_.trick_cards.empty() ||
           CardSuit(play_.trick_cards.front()) == CardSuit(card);
  };
  CardLocation current_player_hand = PlayerHandLocation(play_.current_player);
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] == current_player_hand && can_play(card)) {
      actions.push_back(card);
    }
  }
  if (!actions.empty()) return actions;

  bool has_tarok = false;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] == current_player_hand &&
        CardSuit(card) == Suit::kTarok) {
      has_tarok = true;
      break;
    }
  }
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] != current_player_hand) continue;
    if (!has_tarok || CardSuit(card) == Suit::kTarok) {
      actions.push_back(card);
    }
  }
  return actions;
}

void HungarianTarokState::PlayDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  SPIEL_CHECK_EQ(common_state_.deck_[action],
                 PlayerHandLocation(play_.current_player));
  SPIEL_CHECK_FALSE(PlayPhaseOver());

  common_state_.deck_[action] = CardLocation::kCurrentTrick;
  play_.trick_cards.push_back(action);
  if (play_.trick_cards.size() == kNumPlayers) {
    ResolveTrick();
  } else {
    play_.current_player = (play_.current_player + 1) % kNumPlayers;
  }
}

void HungarianTarokState::ResolveTrick() {
  SPIEL_CHECK_EQ(play_.trick_cards.size(), kNumPlayers);

  Player trick_winner = play_.trick_caller;
  Card winning_card = play_.trick_cards[0];
  for (int i = 1; i < kNumPlayers; ++i) {
    Card card = play_.trick_cards[i];
    if (CardBeats(card, winning_card)) {
      winning_card = card;
      trick_winner = (play_.trick_caller + i) % kNumPlayers;
    }
  }

  play_.trick_caller = trick_winner;
  play_.current_player = trick_winner;
  for (Card card : play_.trick_cards) {
    common_state_.deck_[card] = PlayerWonCardsLocation(trick_winner);
  }

  CommonState::Trick trick;
  std::copy(play_.trick_cards.begin(), play_.trick_cards.end(), trick.begin());
  common_state_.tricks_.push_back(trick);
  common_state_.trick_winners_.push_back(trick_winner);
  play_.trick_cards.clear();

  play_.round++;
  if (play_.round == kNumRounds) {
    play_.current_player = kTerminalPlayerId;
  }
}

bool HungarianTarokState::PlayPhaseOver() const {
  return play_.round >= kNumRounds;
}

bool HungarianTarokState::PlayGameOver() const { return PlayPhaseOver(); }

std::string HungarianTarokState::PlayActionToString(Player player,
                                                    Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  return absl::StrCat("Play card ", CardToString(static_cast<Card>(action)));
}

std::string HungarianTarokState::PlayToString() const {
  return absl::StrCat("Play Phase, round ", play_.round + 1, " ",
                      play_.trick_cards.size(), "/4 cards played\n",
                      DeckToString(common_state_.deck_));
}

std::vector<double> HungarianTarokState::PlayReturns() const {
  if (!PlayPhaseOver()) {
    return std::vector<double>(kNumPlayers, 0.0);
  }
  const std::array<int, kNumPlayers> scores = CalculateScores(common_state_);
  std::vector<double> returns;
  returns.reserve(kNumPlayers);
  for (int p = 0; p < kNumPlayers; ++p) {
    returns.push_back(static_cast<double>(scores[p]));
  }
  return returns;
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
