#include "phases.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "scoring.h"

namespace open_spiel {
namespace hungarian_tarok {

namespace {

constexpr int kTalonSize = 6;
constexpr int kNumRounds = 9;

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
    return kNumAnnouncementTypes * 3;  // special action for passing
  }
};

class BiddingPhase final : public GamePhase {
 public:
  explicit BiddingPhase(std::unique_ptr<GameData> game_data)
      : GamePhase(std::move(game_data)) {
    has_honour_[this->deck()[kSkiz]] = true;
    has_honour_[this->deck()[kPagat]] = true;
    has_honour_[this->deck()[kXXI]] = true;
  }

  PhaseType phase_type() const override { return PhaseType::kBidding; }

  Player CurrentPlayer() const override { return current_player_; }

  std::vector<Action> LegalActions() const override {
    SPIEL_CHECK_FALSE(PhaseOver());
    if (!has_honour_[current_player_]) {
      return {kBiddingActionPass};
    }
    return {kBiddingActionPass, kBiddingActionStandardBid};
  }

  void DoApplyAction(Action action) override {
    SPIEL_CHECK_FALSE(PhaseOver());
    std::vector<Action> legal_actions = LegalActions();
    SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) !=
                     legal_actions.end());

    if (action == kBiddingActionStandardBid) {
      // No holding as first bid.
      if (!was_held_ && !has_bid_[current_player_]) {
        was_held_ = true;
      } else {
        lowest_bid_ -= 1;
        was_held_ = false;
      }
      declarer() = current_player_;
      has_bid_[current_player_] = true;
    } else if (action == kBiddingActionPass) {
      has_passed_[current_player_] = true;
    }
    NextPlayer();
  }

  bool PhaseOver() const override {
    return current_player_ == kTerminalPlayerId;
  }
  bool GameOver() const override { return all_passed_; }

  std::unique_ptr<GamePhase> NextPhase() override;

  std::unique_ptr<GamePhase> Clone() const override {
    return std::make_unique<BiddingPhase>(*this);
  }

  std::string ActionToString(Player player, Action action) const override {
    SPIEL_CHECK_FALSE(PhaseOver());
    std::vector<Action> legal_actions = LegalActions();
    SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) !=
                     legal_actions.end());
    if (action == kBiddingActionPass) {
      return "Pass";
    } else if (action == kBiddingActionStandardBid) {
      return was_held_ || !has_bid_[current_player_]
                 ? absl::StrCat("Bid ", lowest_bid_ - 1)
                 : absl::StrCat("Bid hold ", lowest_bid_);
    }
    return "Unknown action";
  }

  std::string ToString() const override { return "Bidding Phase"; }

 private:
  void NextPlayer() {
    Player next_player = (current_player_ + 1) % kNumPlayers;
    while (has_passed_[next_player] && next_player != current_player_) {
      next_player = (next_player + 1) % kNumPlayers;
    }
    if (next_player == declarer()) {
      current_player_ = kTerminalPlayerId;
      return;
    }
    if (next_player == current_player_) {
      // Everyone passed.
      current_player_ = kTerminalPlayerId;
      all_passed_ = true;
      declarer() = -1;
      winning_bid() = -1;
      return;
    }
    current_player_ = next_player;
  }

  Player current_player_ = 0;
  int lowest_bid_ = 4;
  bool was_held_ = false;
  bool all_passed_ = false;
  std::array<bool, kNumPlayers> has_passed_ = {false, false, false, false};
  std::array<bool, kNumPlayers> has_honour_ = {false, false, false, false};
  std::array<bool, kNumPlayers> has_bid_ = {false, false, false, false};
};

class DealTalonPhase final : public GamePhase {
 public:
  explicit DealTalonPhase(std::unique_ptr<GameData> game_data)
      : GamePhase(std::move(game_data)) {
  Player declarer = this->declarer();
  int declarer_cards_to_take = this->winning_bid();

    cards_to_take_ = {0, 0, 0, 0};
    cards_to_take_[declarer] = declarer_cards_to_take;
    current_player_ = declarer;

    // Distribute remaining talon cards to other players evenly, starting from
    // the player after declarer.
    int remaining_cards = kTalonSize - declarer_cards_to_take;
    Player player = (declarer + 1) % kNumPlayers;
    while (remaining_cards > 0) {
      if (player != declarer) {
        cards_to_take_[player]++;
        remaining_cards--;
      }
      player = (player + 1) % kNumPlayers;
    }

    int index = 0;
    for (Card card = 0; card < kDeckSize; ++card) {
      if (this->deck()[card] == kTalon) {
        talon_cards_[index++] = card;
      }
    }
    SPIEL_CHECK_EQ(index, kTalonSize);
  }

  PhaseType phase_type() const override { return PhaseType::kTalon; }

  Player CurrentPlayer() const override {
    return PhaseOver() ? kTerminalPlayerId : kChancePlayerId;
  }

  std::vector<Action> LegalActions() const override {
    SPIEL_CHECK_FALSE(PhaseOver());
    std::vector<Action> actions;
    for (int i = 0; i < kTalonSize; ++i) {
      if (!talon_taken_[i]) {
        actions.push_back(i);
      }
    }
    return actions;
  }

  void DoApplyAction(Action action) override {
    SPIEL_CHECK_GE(action, 0);
    SPIEL_CHECK_LT(action, kTalonSize);
    SPIEL_CHECK_FALSE(talon_taken_[action]);
    SPIEL_CHECK_FALSE(PhaseOver());

    talon_taken_[action] = true;
    deck()[talon_cards_[action]] = current_player_;
    talon_taken_count_++;
    cards_to_take_[current_player_]--;

    if (talon_taken_count_ == kTalonSize) {
      current_player_ = kTerminalPlayerId;
    } else if (cards_to_take_[current_player_] == 0) {
      current_player_ = (current_player_ + 1) % kNumPlayers;
    }
  }

  bool PhaseOver() const override {
    return current_player_ == kTerminalPlayerId;
  }

  std::unique_ptr<GamePhase> NextPhase() override;

  std::unique_ptr<GamePhase> Clone() const override {
    return std::make_unique<DealTalonPhase>(*this);
  }

  std::string ActionToString(Player player, Action action) const override {
    SPIEL_CHECK_GE(action, 0);
    SPIEL_CHECK_LT(action, kTalonSize);
    return absl::StrCat("Take talon card ", action);
  }

  std::string ToString() const override { return "Dealing Talon Phase"; }

 private:
  Player current_player_;
  std::array<Card, kTalonSize> talon_cards_{};
  std::array<bool, kTalonSize> talon_taken_ = {false};
  std::array<int, kNumPlayers> cards_to_take_{};
  int talon_taken_count_ = 0;
};

class SkartPhase final : public GamePhase {
 public:
  explicit SkartPhase(std::unique_ptr<GameData> game_data)
      : GamePhase(std::move(game_data)) {
    hand_sizes_.fill(0);
    for (Card card = 0; card < kDeckSize; ++card) {
      Player owner = this->deck()[card];
      if (owner >= 0 && owner < kNumPlayers) {
        hand_sizes_[owner]++;
      }
    }
    current_player_ = this->declarer();
  }

  PhaseType phase_type() const override { return PhaseType::kSkart; }

  Player CurrentPlayer() const override { return current_player_; }

  std::vector<Action> LegalActions() const override {
    SPIEL_CHECK_FALSE(PhaseOver());
    std::vector<Action> actions;
    for (Card card = 0; card < kDeckSize; ++card) {
      if (deck()[card] == current_player_) {
        actions.push_back(card);
      }
    }
    return actions;
  }

  void DoApplyAction(Action action) override {
    SPIEL_CHECK_GE(action, 0);
    SPIEL_CHECK_LT(action, kDeckSize);
    SPIEL_CHECK_EQ(deck()[action], current_player_);
    SPIEL_CHECK_FALSE(PhaseOver());

    if (current_player_ == declarer()) {
      deck()[action] = kDeclarerSkart;
    } else {
      deck()[action] = kOpponentsSkart;
    }
    cards_discarded_++;

    hand_sizes_[current_player_]--;
    if (hand_sizes_[current_player_] == kPlayerHandSize) {
      current_player_ = (current_player_ + 1) % kNumPlayers;
      if (hand_sizes_[current_player_] == 0) {
        current_player_ = kTerminalPlayerId;
      }
    }
  }

  bool PhaseOver() const override { return cards_discarded_ == kTalonSize; }

  std::unique_ptr<GamePhase> NextPhase() override;

  std::unique_ptr<GamePhase> Clone() const override {
    return std::make_unique<SkartPhase>(*this);
  }

  std::string ActionToString(Player player, Action action) const override {
    SPIEL_CHECK_GE(action, 0);
    SPIEL_CHECK_LT(action, kDeckSize);
    return absl::StrCat("Discard card ",
                        CardToString(static_cast<Card>(action)));
  }

  std::string ToString() const override {
    return absl::StrCat("Skart Phase, ", cards_discarded_, "/6 cards discarded",
                        "\n", DeckToString(deck()));
  }

 private:
  Player current_player_ = 0;
  std::array<int, kNumPlayers> hand_sizes_{};
  int cards_discarded_ = 0;
};

class AnnouncementsPhase final : public GamePhase {
 public:
  explicit AnnouncementsPhase(std::unique_ptr<GameData> game_data)
      : GamePhase(std::move(game_data)) {
    current_player_ = this->declarer();
    tarok_counts_.fill(0);
    for (Card card = 0; card < kDeckSize; ++card) {
      if (CardSuit(card) == Suit::kTarok) {
        Player owner = this->deck()[card];
        if (owner >= 0 && owner < kNumPlayers) {
          tarok_counts_[owner]++;
        }
      }
    }
  }

  PhaseType phase_type() const override { return PhaseType::kAnnouncements; }

  Player CurrentPlayer() const override { return current_player_; }

  std::vector<Action> LegalActions() const override {
    SPIEL_CHECK_FALSE(PhaseOver());
    if (!partner_called_) {
      if (deck()[MakeTarok(20)] == declarer()) {
        // Declarer can call self with XX.
        return {kAnnouncementsActionCallPartner, kAnnouncementsActionCallSelf};
      }
      return {kAnnouncementsActionCallPartner};
    }

    std::vector<Action> actions;
    const GameData::AnnouncementSide& current_side = CurrentSide();
    const GameData::AnnouncementSide& other_side = OtherSide();

    // Separate loops so actions are sorted.
    for (int i = 0; i < kNumAnnouncementTypes; ++i) {
      AnnouncementType type = static_cast<AnnouncementType>(i);
      if (current_side.announced[i]) continue;

      switch (type) {
        case AnnouncementType::kTuletroa:
          if (CanAnnounceTuletroa())
            actions.push_back(AnnouncementAction::AnnounceAction(type));
          break;
        case AnnouncementType::kEightTaroks:
          if (tarok_counts_[current_player_] == 8)
            actions.push_back(AnnouncementAction::AnnounceAction(type));
          break;
        case AnnouncementType::kNineTaroks:
          if (tarok_counts_[current_player_] == 9)
            actions.push_back(AnnouncementAction::AnnounceAction(type));
          break;
        default:
          actions.push_back(AnnouncementAction::AnnounceAction(type));
      }
    }

    for (int i = 0; i < kNumAnnouncementTypes; ++i) {
      AnnouncementType type = static_cast<AnnouncementType>(i);
      // Contra only if other side announced and not contra'd yet (or
      // re-contra'd).
      if (other_side.announced[i] && other_side.contra_level[i] % 2 == 0 &&
          other_side.contra_level[i] <= kMaxContraLevel) {
        actions.push_back(AnnouncementAction::ContraAction(type));
      }
    }

    for (int i = 0; i < kNumAnnouncementTypes; ++i) {
      AnnouncementType type = static_cast<AnnouncementType>(i);
      // Re-contra only if contra'd already (or re-contra/subcontra'd).
      if (current_side.contra_level[i] % 2 == 1 &&
          current_side.contra_level[i] <= kMaxContraLevel) {
        actions.push_back(AnnouncementAction::ReContraAction(type));
      }
    }

    actions.push_back(AnnouncementAction::PassAction());
    return actions;
  }

  void DoApplyAction(Action action) override {
    SPIEL_CHECK_FALSE(PhaseOver());
    std::vector<Action> legal_actions = LegalActions();
    SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) !=
                     legal_actions.end());

    if (!partner_called_) {
      if (action == kAnnouncementsActionCallPartner) {
        // Call highest tarok not in declarer's hand.
        for (int rank = 20; rank >= 1; --rank) {
          Card card = MakeTarok(rank);
          if (deck()[card] != declarer()) {
            // the card my have been discarded to skart
            Player location = deck()[card];
            partner() = IsPlayerHandLocation(location)
                           ? std::optional<Player>(location)
                           : std::nullopt;
            break;
          }
        }
      }  else {
        partner() = std::nullopt;
      }

      partner_called_ = true;
      // Next player is the player after declarer.
      current_player_ = (declarer() + 1) % kNumPlayers;
      last_to_speak_ = declarer();

      for (Player p = 0; p < kNumPlayers; ++p) {
        player_sides()[p] =
            (p == declarer() || p == partner())
                ? Side::kDeclarer
                : Side::kOpponents;
      }
      return;
    }

    if (action == AnnouncementAction::PassAction()) {
      current_player_ = (current_player_ + 1) % kNumPlayers;
      if (current_player_ == last_to_speak_) {
        current_player_ = kTerminalPlayerId;  // end of phase
      }
      if (current_player_ == declarer()) {
        first_round_ = false;
      }
      return;
    }

    AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
    GameData::AnnouncementSide& current_side = CurrentSide();
    GameData::AnnouncementSide& other_side = OtherSide();
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
    last_to_speak_ = current_player_;
  }

  bool PhaseOver() const override {
    return current_player_ == kTerminalPlayerId;
  }

  std::unique_ptr<GamePhase> NextPhase() override;

  std::unique_ptr<GamePhase> Clone() const override {
    return std::make_unique<AnnouncementsPhase>(*this);
  }

  std::string ActionToString(Player player, Action action) const override {
    SPIEL_CHECK_FALSE(PhaseOver());
    std::vector<Action> legal_actions = LegalActions();
    SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) !=
                     legal_actions.end());
    if (!partner_called_) {
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

  std::string ToString() const override { return "Announcements Phase"; }

 private:
  bool IsDeclarerSidePlayer(Player player) const {
    return player == partner() || player == declarer();
  }

  GameData::AnnouncementSide& CurrentSide() {
    return IsDeclarerSidePlayer(current_player_) ? declarer_side()
                                                 : opponents_side();
  }
  GameData::AnnouncementSide& OtherSide() {
    return IsDeclarerSidePlayer(current_player_) ? opponents_side()
                                                 : declarer_side();
  }
  const GameData::AnnouncementSide& CurrentSide() const {
    return IsDeclarerSidePlayer(current_player_) ? declarer_side()
                                                 : opponents_side();
  }
  const GameData::AnnouncementSide& OtherSide() const {
    return IsDeclarerSidePlayer(current_player_) ? opponents_side()
                                                 : declarer_side();
  }

  bool CanAnnounceTuletroa() const {
    if (CurrentSide()
            .announced[static_cast<int>(AnnouncementType::kTuletroa)]) {
      return false;  // already announced
    }
    if (full_bid() && current_player_ == declarer() &&
        first_round_) {
      // In full bid in the first round tuletroa from declarer means skiz in
      // hand.
      return deck()[kSkiz] == declarer();
    }
    if (current_player_ == declarer() && first_round_) {
      // In the first round, tuletroa from declarer means XXI and Skiz in hand.
      return deck()[kXXI] == declarer() && deck()[kSkiz] == declarer();
    }
    if (partner().has_value() && current_player_ == *partner() && first_round_) {
      // In the first round, tuletroa from partner means XXI or Skiz in hand.
      return deck()[kXXI] == *partner() || deck()[kSkiz] == *partner();
    }
    return true;
  }

  Player current_player_ = 0;
  bool partner_called_ = false;
  Player last_to_speak_ = 0;
  bool first_round_ = true;
  std::array<int, kNumPlayers> tarok_counts_{};
};

class PlayPhase final : public GamePhase {
 public:
  explicit PlayPhase(std::unique_ptr<GameData> game_data)
      : GamePhase(std::move(game_data)) {
    current_player_ = this->declarer();
    trick_caller_ = current_player_;
  }

  PhaseType phase_type() const override { return PhaseType::kPlay; }

  Player CurrentPlayer() const override { return current_player_; }

  std::vector<Action> LegalActions() const override {
    SPIEL_CHECK_FALSE(PhaseOver());

    std::vector<Action> actions;
    auto can_play = [&](Card card) {
      return trick_cards_.empty() ||
             CardSuit(trick_cards_.front()) == CardSuit(card);
    };
    for (Card card = 0; card < kDeckSize; ++card) {
      if (deck()[card] == current_player_ && can_play(card)) {
        actions.push_back(card);
      }
    }
    if (!actions.empty()) return actions;

    // No cards of the leading suit.
    bool has_tarok = false;
    for (Card card = 0; card < kDeckSize; ++card) {
      if (deck()[card] == current_player_ &&
          CardSuit(card) == Suit::kTarok) {
        has_tarok = true;
        break;
      }
    }
    for (Card card = 0; card < kDeckSize; ++card) {
      if (deck()[card] != current_player_) continue;
      // Must play tarok if has one.
      if (!has_tarok || CardSuit(card) == Suit::kTarok) {
        actions.push_back(card);
      }
    }
    return actions;
  }

  void DoApplyAction(Action action) override {
    SPIEL_CHECK_GE(action, 0);
    SPIEL_CHECK_LT(action, kDeckSize);
    SPIEL_CHECK_EQ(deck()[action], current_player_);
    SPIEL_CHECK_FALSE(PhaseOver());

    // Play the card.
    deck()[action] = kCurrentTrick;  // mark as played in round
    trick_cards_.push_back(action);
    if (trick_cards_.size() == kNumPlayers) {
      ResolveTrick();
    } else {
      current_player_ = (current_player_ + 1) % kNumPlayers;
    }
  }

  bool PhaseOver() const override { return round_ >= kNumRounds; }
  bool GameOver() const override { return PhaseOver(); }

  std::unique_ptr<GamePhase> NextPhase() override {
    SPIEL_CHECK_TRUE(PhaseOver());
    return nullptr;  // no next phase, game over
  }

  std::unique_ptr<GamePhase> Clone() const override {
    return std::make_unique<PlayPhase>(*this);
  }

  std::string ActionToString(Player player, Action action) const override {
    SPIEL_CHECK_GE(action, 0);
    SPIEL_CHECK_LT(action, kDeckSize);
    return absl::StrCat("Play card ", CardToString(static_cast<Card>(action)));
  }

  std::string ToString() const override {
    return absl::StrCat("Play Phase, round ", round_ + 1, " ",
                        trick_cards_.size(), "/4 cards played", "\n",
                        DeckToString(deck()));
  }

  std::vector<double> Returns() const override {
    if (!PhaseOver()) {
      return std::vector<double>(kNumPlayers, 0.0);
    }
    const std::array<int, kNumPlayers> scores =
        CalculateScores(game_data_for_scoring());
    std::vector<double> returns;
    returns.reserve(kNumPlayers);
    for (int p = 0; p < kNumPlayers; ++p) {
      returns.push_back(static_cast<double>(scores[p]));
    }
    return returns;
  }

 private:
  void ResolveTrick() {
    SPIEL_CHECK_EQ(trick_cards_.size(), kNumPlayers);

    Player trick_winner = trick_caller_;
    Card winning_card = trick_cards_[0];
    for (int i = 1; i < kNumPlayers; ++i) {
      Card card = trick_cards_[i];
      if (CardBeats(card, winning_card)) {
        winning_card = card;
        trick_winner = (trick_caller_ + i) % kNumPlayers;
      }
    }

    trick_caller_ = trick_winner;
    current_player_ = trick_winner;
    for (Card card : trick_cards_) {
      deck()[card] = WonCardsLocation(trick_winner);
    }
    GameData::Trick trick;
    std::copy(trick_cards_.begin(), trick_cards_.end(), trick.begin());
    tricks().push_back(trick);
    trick_winners().push_back(trick_winner);
    trick_cards_.clear();

    round_++;
    if (round_ == kNumRounds) {
      current_player_ = kTerminalPlayerId;
    }
  }

  Player current_player_ = 0;
  Player trick_caller_ = 0;
  std::vector<Card> trick_cards_;
  int round_ = 0;
};

std::unique_ptr<GamePhase> AnnouncementsPhase::NextPhase() {
  SPIEL_CHECK_TRUE(PhaseOver());
  return std::make_unique<PlayPhase>(std::move(game_data_));
}

std::unique_ptr<GamePhase> SkartPhase::NextPhase() {
  SPIEL_CHECK_TRUE(PhaseOver());
  return std::make_unique<AnnouncementsPhase>(std::move(game_data_));
}

std::unique_ptr<GamePhase> DealTalonPhase::NextPhase() {
  SPIEL_CHECK_TRUE(PhaseOver());
  return std::make_unique<SkartPhase>(std::move(game_data_));
}

std::unique_ptr<GamePhase> BiddingPhase::NextPhase() {
  SPIEL_CHECK_TRUE(PhaseOver());

  int bidder_count = std::count(has_bid_.begin(), has_bid_.end(), true);
  full_bid() = (bidder_count == 3);  // all three honours bid
  winning_bid() = lowest_bid_;
  return std::make_unique<DealTalonPhase>(std::move(game_data_));
}

}  // namespace

SetupPhase::SetupPhase() : GamePhase(std::make_unique<GameData>()) {
  deck().fill(kTalon);  // all cards not dealt stay in the talon
  player_hands_sizes_.fill(0);
}

SetupPhase::~SetupPhase() = default;

Player SetupPhase::CurrentPlayer() const { return kChancePlayerId; }

std::vector<Action> SetupPhase::LegalActions() const {
  std::vector<Action> actions;
  for (int player = 0; player < kNumPlayers; ++player) {
    if (player_hands_sizes_[player] < kPlayerHandSize) {
      actions.push_back(player);
    }
  }
  return actions;
}

void SetupPhase::DoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kNumPlayers);
  SPIEL_CHECK_FALSE(PhaseOver());
  if (current_card_ == kPagat) {
    SetPagatHolder(action);
  }
  // Action is the player ID who receives the next card.
  deck()[current_card_] = action;
  player_hands_sizes_[action]++;
  current_card_++;
}

bool SetupPhase::PhaseOver() const {
  return current_card_ >= kPlayerHandSize * kNumPlayers;
}

std::unique_ptr<GamePhase> SetupPhase::NextPhase() {
  SPIEL_CHECK_TRUE(PhaseOver());
  return std::make_unique<BiddingPhase>(std::move(game_data_));
}

std::unique_ptr<GamePhase> SetupPhase::Clone() const {
  return std::make_unique<SetupPhase>(*this);
}

std::string SetupPhase::ActionToString(Player player, Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kNumPlayers);
  return absl::StrCat("Deal card ", CardToString(current_card_), " to player ",
                      action);
}

std::string SetupPhase::ToString() const { return "Setup Phase"; }

}  // namespace hungarian_tarok
}  // namespace open_spiel
