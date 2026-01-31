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
      int bidder_count = static_cast<int>(
          std::count(bidding_.has_bid.begin(), bidding_.has_bid.end(), true));
      common_state_.full_bid_ = (bidder_count == 3);
      common_state_.winning_bid_ = bidding_.lowest_bid;
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
  common_state_.partner_ = std::nullopt;
  common_state_.declarer_side_ = CommonState::AnnouncementSide{};
  common_state_.opponents_side_ = CommonState::AnnouncementSide{};
  common_state_.player_sides_.fill(Side::kOpponents);
  common_state_.tricks_.clear();
  common_state_.trick_winners_.clear();

  bidding_.has_honour[HandLocationPlayer(common_state_.deck_[kSkiz])] = true;
  bidding_.has_honour[HandLocationPlayer(common_state_.deck_[kPagat])] = true;
  bidding_.has_honour[HandLocationPlayer(common_state_.deck_[kXXI])] = true;
}

Player HungarianTarokState::BiddingCurrentPlayer() const {
  return bidding_.current_player;
}

std::vector<Action> HungarianTarokState::BiddingLegalActions() const {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());
  if (!bidding_.has_honour[bidding_.current_player]) {
    if (bidding_.lowest_bid == 4 && bidding_.current_player == 3) {
      return {kBiddingActionPass, kBiddingActionStandardBid};
    }
    return {kBiddingActionPass};
  }
  return {kBiddingActionPass, kBiddingActionStandardBid};
}

void HungarianTarokState::BiddingDoApplyAction(Action action) {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());
  std::vector<Action> legal_actions = BiddingLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  if (action == kBiddingActionStandardBid) {
    if (!bidding_.has_honour[bidding_.current_player] &&
        bidding_.lowest_bid == 4 && bidding_.current_player == 3) {
      // Special case: last player can bid 3 even without an honour.
      common_state_.trial_three_ = true;
    }
    // No holding as first bid.
    if (!bidding_.was_held && bidding_.has_bid[bidding_.current_player]) {
      bidding_.was_held = true;
    } else {
      bidding_.lowest_bid -= 1;
      bidding_.was_held = false;
    }
    common_state_.declarer_ = bidding_.current_player;
    bidding_.has_bid[bidding_.current_player] = true;
  } else if (action == kBiddingActionPass) {
    bidding_.has_passed[bidding_.current_player] = true;
  }
  BiddingNextPlayer();
}

void HungarianTarokState::BiddingNextPlayer() {
  if (bidding_.lowest_bid == 0 && bidding_.was_held) {
    // Maximum bid reached.
    bidding_.current_player = kTerminalPlayerId;
    return;
  }
  Player next_player = (bidding_.current_player + 1) % kNumPlayers;
  while (bidding_.has_passed[next_player] &&
         next_player != bidding_.current_player) {
    next_player = (next_player + 1) % kNumPlayers;
  }
  if (next_player == common_state_.declarer_) {
    bidding_.current_player = kTerminalPlayerId;
    return;
  }
  if (next_player == bidding_.current_player) {
    // Everyone passed.
    bidding_.current_player = kTerminalPlayerId;
    bidding_.all_passed = true;
    common_state_.declarer_ = -1;
    common_state_.winning_bid_ = -1;
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
  if (action == kBiddingActionPass) {
    return "Pass";
  }
  if (!bidding_.was_held && !bidding_.has_bid[bidding_.current_player]) {
    return absl::StrCat("Bid ", bidding_.lowest_bid - 1);
  } else if (bidding_.was_held) {
    return absl::StrCat("Bid ", bidding_.lowest_bid - 1);
  } else if (!bidding_.was_held && bidding_.has_bid[bidding_.current_player]) {
    return absl::StrCat("Hold ", bidding_.lowest_bid);
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
    // trial three
    if (common_state_.deck_[kPagat] ==
            PlayerHandLocation(talon_.current_player) ||
        common_state_.deck_[kSkiz] ==
            PlayerHandLocation(talon_.current_player) ||
        common_state_.deck_[kXXI] ==
            PlayerHandLocation(talon_.current_player)) {
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
void HungarianTarokState::StartAnnouncementsPhase() {
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
  if (common_state_.full_bid_ &&
      announcements_.current_player == common_state_.declarer_ &&
      announcements_.first_round) {
    return common_state_.deck_[kSkiz] ==
           PlayerHandLocation(common_state_.declarer_);
  }
  if (announcements_.current_player == common_state_.declarer_ &&
      announcements_.first_round) {
    return common_state_.deck_[kXXI] ==
               PlayerHandLocation(common_state_.declarer_) &&
           common_state_.deck_[kSkiz] ==
               PlayerHandLocation(common_state_.declarer_);
  }
  if (common_state_.partner_.has_value() &&
      announcements_.current_player == *common_state_.partner_ &&
      announcements_.first_round) {
    return common_state_.deck_[kXXI] ==
               PlayerHandLocation(*common_state_.partner_) ||
           common_state_.deck_[kSkiz] ==
               PlayerHandLocation(*common_state_.partner_);
  }
  return true;
}

std::vector<Action> HungarianTarokState::AnnouncementsLegalActions() const {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  if (!announcements_.partner_called) {
    if (common_state_.deck_[MakeTarok(20)] ==
        PlayerHandLocation(common_state_.declarer_)) {
      return {kAnnouncementsActionCallPartner, kAnnouncementsActionCallSelf};
    }
    return {kAnnouncementsActionCallPartner};
  }

  std::vector<Action> actions;
  const CommonState::AnnouncementSide& current_side = CurrentAnnouncementSide();
  const CommonState::AnnouncementSide& other_side = OtherAnnouncementSide();

  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    AnnouncementType type = static_cast<AnnouncementType>(i);
    if (current_side.announced[i]) continue;

    switch (type) {
      case AnnouncementType::kTuletroa:
        if (CanAnnounceTuletroa()) {
          actions.push_back(AnnouncementAction::AnnounceAction(type));
        }
        break;
      case AnnouncementType::kEightTaroks:
        if (announcements_.tarok_counts[announcements_.current_player] == 8) {
          actions.push_back(AnnouncementAction::AnnounceAction(type));
        }
        break;
      case AnnouncementType::kNineTaroks:
        if (announcements_.tarok_counts[announcements_.current_player] == 9) {
          actions.push_back(AnnouncementAction::AnnounceAction(type));
        }
        break;
      case AnnouncementType::kFourKings:
      case AnnouncementType::kDoubleGame:
        if (!current_side
                 .announced[static_cast<int>(AnnouncementType::kVolat)]) {
          actions.push_back(AnnouncementAction::AnnounceAction(type));
        }
        break;
      default:
        actions.push_back(AnnouncementAction::AnnounceAction(type));
    }
  }

  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    AnnouncementType type = static_cast<AnnouncementType>(i);
    if (type == AnnouncementType::kEightTaroks ||
        type == AnnouncementType::kNineTaroks) {
      continue;
    }
    if (other_side.announced[i] && other_side.contra_level[i] % 2 == 0 &&
        other_side.contra_level[i] <= kMaxContraLevel) {
      actions.push_back(AnnouncementAction::ContraAction(type));
    }
  }

  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    AnnouncementType type = static_cast<AnnouncementType>(i);
    if (current_side.contra_level[i] % 2 == 1 &&
        current_side.contra_level[i] <= kMaxContraLevel) {
      actions.push_back(AnnouncementAction::ReContraAction(type));
    }
  }

  actions.push_back(AnnouncementAction::PassAction());
  return actions;
}

void HungarianTarokState::AnnouncementsCallPartner(Action action) {
  if (action == kAnnouncementsActionCallPartner) {
    for (int rank = 20; rank >= 1; --rank) {
      Card card = MakeTarok(rank);
      if (common_state_.deck_[card] !=
          PlayerHandLocation(common_state_.declarer_)) {
        CardLocation location = common_state_.deck_[card];
        common_state_.partner_ =
            IsPlayerHand(location)
                ? std::optional<Player>(HandLocationPlayer(location))
                : std::nullopt;
        break;
      }
    }
  } else {
    common_state_.partner_ = std::nullopt;
  }

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
  std::string level_str;
  switch (ann_action.level) {
    case AnnouncementAction::Level::kAnnounce:
      level_str = "Announce ";
      break;
    case AnnouncementAction::Level::kContra:
      level_str = "Contra ";
      break;
    case AnnouncementAction::Level::kReContra:
      level_str = "Re-Contra ";
      break;
  }

  std::string type_str;
  switch (ann_action.type) {
    case AnnouncementType::kFourKings:
      type_str = "Four Kings";
      break;
    case AnnouncementType::kTuletroa:
      type_str = "Tuletroa";
      break;
    case AnnouncementType::kDoubleGame:
      type_str = "Double Game";
      break;
    case AnnouncementType::kVolat:
      type_str = "Volat";
      break;
    case AnnouncementType::kPagatUltimo:
      type_str = "Pagat Ultimo";
      break;
    case AnnouncementType::kXXICapture:
      type_str = "XXI Capture";
      break;
    case AnnouncementType::kEightTaroks:
      type_str = "Eight Taroks";
      break;
    case AnnouncementType::kNineTaroks:
      type_str = "Nine Taroks";
      break;
  }
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
