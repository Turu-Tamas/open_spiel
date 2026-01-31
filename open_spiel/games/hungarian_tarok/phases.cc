#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/scoring.h"

namespace open_spiel {
namespace hungarian_tarok {

namespace {

constexpr Action kBiddingActionPass = 0;
constexpr Action kBiddingActionStandardBid = 1;

constexpr Action kAnnouncementsActionCallPartner = 0;
constexpr Action kAnnouncementsActionCallSelf = 1;

constexpr int kMaxContraLevel = 5;

struct AnnouncementAction {
  AnnouncementType type;
  enum class Level {
    kAnnounce = 0,
    kContra = 1,
    kReContra = 2,
  } level;

  static constexpr AnnouncementAction FromAction(Action action) {
    SPIEL_CHECK_GE(action, 0);
    SPIEL_CHECK_LT(action, kNumAnnouncementTypes * 3);
    Level level = static_cast<Level>(action / kNumAnnouncementTypes);
    AnnouncementType type =
        static_cast<AnnouncementType>(action % kNumAnnouncementTypes);
    return AnnouncementAction{type, level};
  }

  constexpr Action ToAction() const {
    return static_cast<Action>(static_cast<int>(level) * kNumAnnouncementTypes +
                               static_cast<int>(type));
  }

  static constexpr Action AnnounceAction(AnnouncementType type) {
    return AnnouncementAction{type, Level::kAnnounce}.ToAction();
  }
  static constexpr Action ContraAction(AnnouncementType type) {
    return AnnouncementAction{type, Level::kContra}.ToAction();
  }
  static constexpr Action ReContraAction(AnnouncementType type) {
    return AnnouncementAction{type, Level::kReContra}.ToAction();
  }
  static constexpr Action PassAction() {
    return kNumAnnouncementTypes * 3; // special action for passing
  }
};

} // namespace

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
  default:
    return false;
  }
}

std::vector<double> HungarianTarokState::PhaseReturns() const {
  switch (current_phase_) {
  case PhaseType::kPlay:
    return PlayReturns();
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
    game_data_.full_bid_ = (bidder_count == 3);
    game_data_.winning_bid_ = bidding_.lowest_bid;
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
    game_data_.pagat_holder_ = action;
  }

  // Action is the player ID who receives the next card.
  game_data_.deck_[setup_.current_card] = action;
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

  game_data_.declarer_ = 0;
  game_data_.winning_bid_ = -1;
  game_data_.full_bid_ = false;
  game_data_.partner_ = std::nullopt;
  game_data_.declarer_side_ = GameData::AnnouncementSide{};
  game_data_.opponents_side_ = GameData::AnnouncementSide{};
  game_data_.player_sides_.fill(Side::kOpponents);
  game_data_.tricks_.clear();
  game_data_.trick_winners_.clear();

  bidding_.has_honour[game_data_.deck_[kSkiz]] = true;
  bidding_.has_honour[game_data_.deck_[kPagat]] = true;
  bidding_.has_honour[game_data_.deck_[kXXI]] = true;
}

Player HungarianTarokState::BiddingCurrentPlayer() const {
  return bidding_.current_player;
}

std::vector<Action> HungarianTarokState::BiddingLegalActions() const {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());
  if (!bidding_.has_honour[bidding_.current_player]) {
    return {kBiddingActionPass};
  }
  return {kBiddingActionPass, kBiddingActionStandardBid};
}

void HungarianTarokState::BiddingDoApplyAction(Action action) {
  SPIEL_CHECK_FALSE(BiddingPhaseOver());
  std::vector<Action> legal_actions = BiddingLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  if (action == kBiddingActionStandardBid) {
    // No holding as first bid.
    if (!bidding_.was_held && bidding_.has_bid[bidding_.current_player]) {
      bidding_.was_held = true;
    } else {
      bidding_.lowest_bid -= 1;
      bidding_.was_held = false;
    }
    game_data_.declarer_ = bidding_.current_player;
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
  if (next_player == game_data_.declarer_) {
    bidding_.current_player = kTerminalPlayerId;
    return;
  }
  if (next_player == bidding_.current_player) {
    // Everyone passed.
    bidding_.current_player = kTerminalPlayerId;
    bidding_.all_passed = true;
    game_data_.declarer_ = -1;
    game_data_.winning_bid_ = -1;
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

  const Player declarer = game_data_.declarer_;
  const int declarer_cards_to_take = game_data_.winning_bid_;

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
    if (game_data_.deck_[card] == kTalon) {
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

void HungarianTarokState::TalonDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kTalonSize);
  SPIEL_CHECK_FALSE(talon_.talon_taken[action]);
  SPIEL_CHECK_FALSE(TalonPhaseOver());

  talon_.talon_taken[action] = true;
  game_data_.deck_[talon_.talon_cards[action]] = talon_.current_player;
  talon_.talon_taken_count++;
  talon_.cards_to_take[talon_.current_player]--;

  if (talon_.talon_taken_count == kTalonSize) {
    talon_.current_player = kTerminalPlayerId;
  } else if (talon_.cards_to_take[talon_.current_player] == 0) {
    talon_.current_player = (talon_.current_player + 1) % kNumPlayers;
  }
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
    Player owner = game_data_.deck_[card];
    if (owner >= 0 && owner < kNumPlayers) {
      skart_.hand_sizes[owner]++;
    }
  }
  skart_.current_player = game_data_.declarer_;
}

Player HungarianTarokState::SkartCurrentPlayer() const {
  return skart_.current_player;
}

std::vector<Action> HungarianTarokState::SkartLegalActions() const {
  SPIEL_CHECK_FALSE(SkartPhaseOver());
  std::vector<Action> actions;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (game_data_.deck_[card] == skart_.current_player) {
      actions.push_back(card);
    }
  }
  return actions;
}

void HungarianTarokState::SkartDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  SPIEL_CHECK_EQ(game_data_.deck_[action], skart_.current_player);
  SPIEL_CHECK_FALSE(SkartPhaseOver());

  if (skart_.current_player == game_data_.declarer_) {
    game_data_.deck_[action] = kDeclarerSkart;
  } else {
    game_data_.deck_[action] = kOpponentsSkart;
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
                      "/6 cards discarded\n", DeckToString(game_data_.deck_));
}

// Announcements.
void HungarianTarokState::StartAnnouncementsPhase() {
  announcements_ = AnnouncementsState{};
  announcements_.current_player = game_data_.declarer_;
  announcements_.tarok_counts.fill(0);
  for (Card card = 0; card < kDeckSize; ++card) {
    if (CardSuit(card) == Suit::kTarok) {
      Player owner = game_data_.deck_[card];
      if (owner >= 0 && owner < kNumPlayers) {
        announcements_.tarok_counts[owner]++;
      }
    }
  }
}

Player HungarianTarokState::AnnouncementsCurrentPlayer() const {
  return announcements_.current_player;
}

bool HungarianTarokState::IsDeclarerSidePlayer(Player player) const {
  return player == game_data_.declarer_ ||
         (game_data_.partner_.has_value() && player == *game_data_.partner_);
}

GameData::AnnouncementSide &HungarianTarokState::CurrentAnnouncementSide() {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? game_data_.declarer_side_
             : game_data_.opponents_side_;
}

GameData::AnnouncementSide &HungarianTarokState::OtherAnnouncementSide() {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? game_data_.opponents_side_
             : game_data_.declarer_side_;
}

const GameData::AnnouncementSide &
HungarianTarokState::CurrentAnnouncementSide() const {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? game_data_.declarer_side_
             : game_data_.opponents_side_;
}

const GameData::AnnouncementSide &
HungarianTarokState::OtherAnnouncementSide() const {
  return IsDeclarerSidePlayer(announcements_.current_player)
             ? game_data_.opponents_side_
             : game_data_.declarer_side_;
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
  if (game_data_.full_bid_ &&
      announcements_.current_player == game_data_.declarer_ &&
      announcements_.first_round) {
    return game_data_.deck_[kSkiz] == game_data_.declarer_;
  }
  if (announcements_.current_player == game_data_.declarer_ &&
      announcements_.first_round) {
    return game_data_.deck_[kXXI] == game_data_.declarer_ &&
           game_data_.deck_[kSkiz] == game_data_.declarer_;
  }
  if (game_data_.partner_.has_value() &&
      announcements_.current_player == *game_data_.partner_ &&
      announcements_.first_round) {
    return game_data_.deck_[kXXI] == *game_data_.partner_ ||
           game_data_.deck_[kSkiz] == *game_data_.partner_;
  }
  return true;
}

std::vector<Action> HungarianTarokState::AnnouncementsLegalActions() const {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  if (!announcements_.partner_called) {
    if (game_data_.deck_[MakeTarok(20)] == game_data_.declarer_) {
      return {kAnnouncementsActionCallPartner, kAnnouncementsActionCallSelf};
    }
    return {kAnnouncementsActionCallPartner};
  }

  std::vector<Action> actions;
  const GameData::AnnouncementSide &current_side = CurrentAnnouncementSide();
  const GameData::AnnouncementSide &other_side = OtherAnnouncementSide();

  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    AnnouncementType type = static_cast<AnnouncementType>(i);
    if (current_side.announced[i])
      continue;

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
      if (!current_side.announced[static_cast<int>(AnnouncementType::kVolat)]) {
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
      if (game_data_.deck_[card] != game_data_.declarer_) {
        Player location = game_data_.deck_[card];
        game_data_.partner_ = IsPlayerHandLocation(location)
                                  ? std::optional<Player>(location)
                                  : std::nullopt;
        break;
      }
    }
  } else {
    game_data_.partner_ = std::nullopt;
  }

  announcements_.partner_called = true;
  announcements_.last_to_speak = game_data_.declarer_;

  for (Player p = 0; p < kNumPlayers; ++p) {
    game_data_.player_sides_[p] =
        (p == game_data_.declarer_ ||
         (game_data_.partner_.has_value() && p == *game_data_.partner_))
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
    if (announcements_.current_player == game_data_.declarer_) {
      announcements_.first_round = false;
    }
    return;
  }

  AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
  GameData::AnnouncementSide &current_side = CurrentAnnouncementSide();
  GameData::AnnouncementSide &other_side = OtherAnnouncementSide();
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

std::string
HungarianTarokState::AnnouncementsActionToString(Player player,
                                                 Action action) const {
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
                      DeckToString(game_data_.deck_));
}

// Play.
void HungarianTarokState::StartPlayPhase() {
  play_ = PlayState{};
  play_.current_player = game_data_.declarer_;
  play_.trick_caller = play_.current_player;
  play_.trick_cards.clear();
  play_.round = 0;
  game_data_.tricks_.clear();
  game_data_.trick_winners_.clear();
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
  for (Card card = 0; card < kDeckSize; ++card) {
    if (game_data_.deck_[card] == play_.current_player && can_play(card)) {
      actions.push_back(card);
    }
  }
  if (!actions.empty())
    return actions;

  bool has_tarok = false;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (game_data_.deck_[card] == play_.current_player &&
        CardSuit(card) == Suit::kTarok) {
      has_tarok = true;
      break;
    }
  }
  for (Card card = 0; card < kDeckSize; ++card) {
    if (game_data_.deck_[card] != play_.current_player)
      continue;
    if (!has_tarok || CardSuit(card) == Suit::kTarok) {
      actions.push_back(card);
    }
  }
  return actions;
}

void HungarianTarokState::PlayDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  SPIEL_CHECK_EQ(game_data_.deck_[action], play_.current_player);
  SPIEL_CHECK_FALSE(PlayPhaseOver());

  game_data_.deck_[action] = kCurrentTrick;
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
    game_data_.deck_[card] = WonCardsLocation(trick_winner);
  }

  GameData::Trick trick;
  std::copy(play_.trick_cards.begin(), play_.trick_cards.end(), trick.begin());
  game_data_.tricks_.push_back(trick);
  game_data_.trick_winners_.push_back(trick_winner);
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
                      DeckToString(game_data_.deck_));
}

std::vector<double> HungarianTarokState::PlayReturns() const {
  if (!PlayPhaseOver()) {
    return std::vector<double>(kNumPlayers, 0.0);
  }
  const std::array<int, kNumPlayers> scores = CalculateScores(game_data_);
  std::vector<double> returns;
  returns.reserve(kNumPlayers);
  for (int p = 0; p < kNumPlayers; ++p) {
    returns.push_back(static_cast<double>(scores[p]));
  }
  return returns;
}

} // namespace hungarian_tarok
} // namespace open_spiel
