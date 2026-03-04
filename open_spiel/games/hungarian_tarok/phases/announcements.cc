#include <array>
#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

namespace open_spiel {
namespace hungarian_tarok {

namespace {

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
  announcements_.current_player_ = common_state_.declarer_;
  announcements_.tarok_counts_.fill(0);
  for (Card card = 0; card < kDeckSize; ++card) {
    if (CardSuit(card) == Suit::kTarok) {
      CardLocation location = common_state_.deck_[card];
      if (IsPlayerHand(location)) {
        announcements_.tarok_counts_[HandLocationPlayer(location)]++;
      }
    }
  }

  common_state_.declarer_side_.announced_for(AnnouncementType::kGame) = true;
}

Player HungarianTarokState::AnnouncementsCurrentPlayer() const {
  return announcements_.current_player_;
}

bool HungarianTarokState::IsDeclarerSidePlayer(Player player) const {
  return player == common_state_.declarer_ ||
         (common_state_.partner_.has_value() &&
          player == *common_state_.partner_);
}

CommonState::AnnouncementSide& HungarianTarokState::CurrentAnnouncementSide() {
  return IsDeclarerSidePlayer(announcements_.current_player_)
             ? common_state_.declarer_side_
             : common_state_.opponents_side_;
}

CommonState::AnnouncementSide& HungarianTarokState::OtherAnnouncementSide() {
  return IsDeclarerSidePlayer(announcements_.current_player_)
             ? common_state_.opponents_side_
             : common_state_.declarer_side_;
}

const CommonState::AnnouncementSide&
HungarianTarokState::CurrentAnnouncementSide() const {
  return IsDeclarerSidePlayer(announcements_.current_player_)
             ? common_state_.declarer_side_
             : common_state_.opponents_side_;
}

const CommonState::AnnouncementSide&
HungarianTarokState::OtherAnnouncementSide() const {
  return IsDeclarerSidePlayer(announcements_.current_player_)
             ? common_state_.opponents_side_
             : common_state_.declarer_side_;
}

bool HungarianTarokState::CanAnnounceTuletroa() const {
  if (CurrentAnnouncementSide().announced_for(AnnouncementType::kTuletroa)) {
    return false;
  }
  if (CurrentAnnouncementSide().announced_for(AnnouncementType::kVolat)) {
    return false;
  }

  bool is_declarer = announcements_.current_player_ == common_state_.declarer_;
  bool is_partner = announcements_.current_player_ == common_state_.partner_;

  // after a cue bid, tuletroa from declarer means skiz or XXI
  if (announcements_.current_player_ == common_state_.declarer_ &&
      common_state_.mandatory_called_card_.has_value()) {
    return PlayerHoldsCard(common_state_.declarer_, kXXI) ||
           PlayerHoldsCard(common_state_.declarer_, kSkiz);
  }
  // as cue bidder, if declarer did not announce tuletroa, you may do so if you
  // have two honours (you know the declarer holds the third)
  if (announcements_.current_player_ == common_state_.cue_bidder_ &&
      !CurrentAnnouncementSide().announced_for(AnnouncementType::kTuletroa)) {
    const std::vector<Card> honours{kPagat, kSkiz, kXXI};
    return absl::c_count_if(honours, [&](Card card) {
             return PlayerHoldsCard(announcements_.current_player_, card);
           }) == 2;
  }
  // in a full bid, declarer can only announce tuletroa if they have the skiz
  if (common_state_.full_bid_ && is_declarer && announcements_.first_round_) {
    return PlayerHoldsCard(common_state_.declarer_, kSkiz);
  }
  // if not a full bid, tuletroa from declarer means XXI and skiz
  if (is_declarer && announcements_.first_round_) {
    return PlayerHoldsCard(common_state_.declarer_, kXXI) &&
           PlayerHoldsCard(common_state_.declarer_, kSkiz);
  }
  // as the partner, tuletroa means XXI or skiz
  if (common_state_.partner_.has_value() && is_partner &&
      announcements_.first_round_) {
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
    return announcements_.tarok_counts_[announcements_.current_player_] == 8;
  }
  if (type == AnnouncementType::kNineTaroks) {
    return announcements_.tarok_counts_[announcements_.current_player_] == 9;
  }

  if (IsBlockedByVolat(type)) {
    return !current_side.announced_for(AnnouncementType::kVolat);
  }

  return true;
}

void HungarianTarokState::AddAnnounceActions(
    std::vector<Action>& actions) const {
  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    const AnnouncementType type = static_cast<AnnouncementType>(i);
    if (type == AnnouncementType::kGame) continue;  // game is implicit
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
    if (other_side.announced_for(type) &&
        other_side.contra_level_for(type) % 2 == 0 &&
        other_side.contra_level_for(type) < kMaxContraLevel) {
      actions.push_back(AnnouncementAction::ContraAction(type));
    }
  }
}

void HungarianTarokState::AddReContraActions(
    std::vector<Action>& actions) const {
  const CommonState::AnnouncementSide& current_side = CurrentAnnouncementSide();
  for (int i = 0; i < kNumAnnouncementTypes; ++i) {
    const AnnouncementType type = static_cast<AnnouncementType>(i);
    if (current_side.contra_level_for(type) % 2 == 1 &&
        current_side.contra_level_for(type) < kMaxContraLevel) {
      actions.push_back(AnnouncementAction::ReContraAction(type));
    }
  }
}

Card HungarianTarokState::HighestMissingTarok() const {
  for (int rank = 20; rank >= 1; --rank) {
    Card card = MakeTarok(rank);
    if (!PlayerHoldsCard(announcements_.current_player_, card)) {
      return card;
    }
  }
  SpielFatalError("Player holds all taroks?!");
}

std::vector<Card> HungarianTarokState::CallableCards() const {
  SPIEL_CHECK_FALSE(announcements_.partner_called_);
  SPIEL_CHECK_EQ(announcements_.current_player_, common_state_.declarer_);

  if (common_state_.mandatory_called_card_.has_value()) {
    return {common_state_.mandatory_called_card_.value()};
  }
  bool tarok_discarded = false;
  for (int rank = 1; rank <= 20; ++rank) {
    CardLocation location = common_state_.deck_[MakeTarok(rank)];
    if (location == CardLocation::kOpponentsSkart) {
      tarok_discarded = true;
      break;
    }
  }

  if (!tarok_discarded) {
    Card highest_missing = HighestMissingTarok();
    if (highest_missing == kXX) {
      return {kXX};
    }
    return {highest_missing, kXX};
  }

  // other taroks can be called if a tarok was discarded by another player
  std::vector<Card> callable_cards;
  // 2..20 because honours (pagat=1, XXI and skiz=22) cannot be called
  for (int rank = 2; rank <= 20; ++rank) {
    Card card = MakeTarok(rank);
    CardLocation location = common_state_.deck_[card];
    if (card == kXX ||
        location != PlayerHandLocation(announcements_.current_player_)) {
      callable_cards.push_back(card);
    }
  }
  return callable_cards;
}

std::vector<Action> HungarianTarokState::AnnouncementsLegalActions() const {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  if (!announcements_.partner_called_) {
    std::vector<Card> callable_cards = CallableCards();
    std::vector<Action> actions;
    for (Card card : callable_cards) {
      actions.push_back(static_cast<Action>(card));
    }
    return actions;
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
  Card called_card = static_cast<Card>(action);

  CardLocation location = common_state_.deck_[called_card];
  if (PlayerHoldsCard(common_state_.declarer_, called_card)) {
    // called self with XX
    SPIEL_CHECK_EQ(called_card, kXX);
    common_state_.partner_ = std::nullopt;
  } else {
    // TODO mandatory contra game
    common_state_.partner_ =
        IsPlayerHand(location)
            ? std::optional<Player>(HandLocationPlayer(location))
            : std::nullopt;
  }

  SPIEL_CHECK_NE(common_state_.partner_, common_state_.declarer_);
  announcements_.partner_called_ = true;
  announcements_.last_to_speak_ = common_state_.declarer_;

  for (Player p = 0; p < kNumPlayers;
       ++p) {  // partner must announce pagat ultimo if they can
    common_state_.player_sides_[p] =
        (p == common_state_.declarer_ || (p == common_state_.partner_))
            ? Side::kDeclarer
            : Side::kOpponents;
  }
}

void HungarianTarokState::AnnouncementsDoApplyCallDefaultPartner() {
  SPIEL_CHECK_EQ(current_phase_, PhaseType::kAnnouncements);
  SPIEL_CHECK_FALSE(announcements_.partner_called_);

  if (common_state_.mandatory_called_card_.has_value()) {
    AnnouncementsDoApplyAction(static_cast<Action>(common_state_.mandatory_called_card_.value()));
  } else {
    AnnouncementsDoApplyAction(static_cast<Action>(HighestMissingTarok()));
  }
}

void HungarianTarokState::AnnouncementsDoApplyAction(Action action) {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  std::vector<Action> legal_actions = AnnouncementsLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());
  Player& current_player = announcements_.current_player_;

  if (!announcements_.partner_called_) {
    AnnouncementsCallPartner(action);
    return;
  }

  if (action == AnnouncementAction::PassAction()) {
    current_player = (current_player + 1) % kNumPlayers;
    if (current_player == announcements_.last_to_speak_) {
      current_player = kTerminalPlayerId;
    }
    if (current_player == common_state_.declarer_) {
      announcements_.first_round_ = false;
    }
    if (common_state_.mandatory_pagatulti_ &&
        current_player == common_state_.partner_ &&
        !CurrentAnnouncementSide().announced_for(
            AnnouncementType::kPagatUltimo)) {
      // partner must announce pagat ultimo if they can
      announcements_.mandatory_announcements_.push_back(
          AnnouncementAction::AnnounceAction(AnnouncementType::kPagatUltimo));
    }
    bool contrad = common_state_.declarer_side_.contra_level_for(
                       AnnouncementType::kGame) > 0;
    if (announcements_.mandatory_contra_player_ == current_player && !contrad) {
      // mandatory contra game if you discarded the called tarok
      announcements_.mandatory_announcements_.push_back(
          AnnouncementAction::ContraAction(AnnouncementType::kGame));
    }
    return;
  }

  AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
  CommonState::AnnouncementSide& current_side = CurrentAnnouncementSide();
  CommonState::AnnouncementSide& other_side = OtherAnnouncementSide();
  switch (ann_action.level) {
    case AnnouncementAction::Level::kAnnounce:
      current_side.announced_for(ann_action.type) = true;
      break;
    case AnnouncementAction::Level::kContra:
      other_side.contra_level_for(ann_action.type)++;
      break;
    case AnnouncementAction::Level::kReContra:
      current_side.contra_level_for(ann_action.type)++;
      break;
  }
  announcements_.last_to_speak_ = current_player;

  auto it = absl::c_find(announcements_.mandatory_announcements_,
                         ann_action.ToAction());
  if (it != announcements_.mandatory_announcements_.end()) {
    announcements_.mandatory_announcements_.erase(it);
  }

  if (ann_action.type == AnnouncementType::kPagatUltimo &&
      ann_action.level == AnnouncementAction::Level::kAnnounce) {
    int tarok_count = announcements_.tarok_counts_[current_player];
    if (tarok_count == 8) {
      bool announced = CurrentAnnouncementSide().announced_for(
          AnnouncementType::kEightTaroks);
      if (!announced) {
        announcements_.mandatory_announcements_.push_back(
            AnnouncementAction::AnnounceAction(AnnouncementType::kEightTaroks));
      }
    } else if (tarok_count == 9) {
      bool announced = CurrentAnnouncementSide().announced_for(
          AnnouncementType::kNineTaroks);
      if (!announced) {
        announcements_.mandatory_announcements_.push_back(
            AnnouncementAction::AnnounceAction(AnnouncementType::kNineTaroks));
      }
    }
  }
}

bool HungarianTarokState::AnnouncementsPhaseOver() const {
  return announcements_.current_player_ == kTerminalPlayerId;
}

std::string HungarianTarokState::AnnouncementsActionToString(
    Player player, Action action) const {
  SPIEL_CHECK_FALSE(AnnouncementsPhaseOver());
  std::vector<Action> legal_actions = AnnouncementsLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());
  if (!announcements_.partner_called_) {
    return absl::StrCat("Call ", CardToString(static_cast<Card>(action)));
  }

  if (action == AnnouncementAction::PassAction()) {
    return "Pass";
  }

  AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
  const char* level_str = AnnouncementLevelPrefix(ann_action.level);
  const char* type_str;
  switch (ann_action.type) {
    case AnnouncementType::kGame:
      type_str = "Game";
      break;
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
    default:
      SpielFatalError("Unknown announcement type");
  }
  return absl::StrCat(level_str, type_str);
}

std::string HungarianTarokState::AnnouncementsToString() const {
  return absl::StrCat("Announcements Phase\n",
                      "current player: ", announcements_.current_player_, "\n",
                      DeckToString(common_state_.deck_));
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
