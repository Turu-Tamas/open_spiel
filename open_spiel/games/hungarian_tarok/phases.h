#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "card.h"
#include "spiel.h"

namespace open_spiel {
namespace hungarian_tarok {
const int kNumAnnouncementTypes = 8;
const int kPlayerHandSize = 9;

class HungarianTarokState;

enum class Side {
  kDeclarer,
  kOpponents,
};

enum class AnnouncementType {
  kFourKings = 0,
  kTuletroa = 1,
  kDoubleGame = 2,
  kVolat = 3,
  kPagatUltimo = 4,
  kXXICapture = 5,
  kEightTaroks = 6,
  kNineTaroks = 7,
};

enum class PhaseType {
  kSetup,
  kBidding,
  kTalon,
  kSkart,
  kAnnouncements,
  kPlay
};

// Shared game state across different phases.
struct GameData {
  Deck deck_;
  Player pagat_holder_;

  // bidding results
  Player declarer_;
  int winning_bid_;
  bool full_bid_;  // wether all three honours bid
  std::optional<Player> partner_;

  // announcements results
  struct AnnouncementSide {
    std::array<bool, kNumAnnouncementTypes> announced = {false};
    std::array<int, kNumAnnouncementTypes> contra_level = {0};
  };
  AnnouncementSide declarer_side_;
  AnnouncementSide opponents_side_;
  std::array<Side, kNumPlayers> player_sides_;

  // play results
  typedef std::array<Card, 4> Trick;
  std::vector<Trick> tricks_;
  std::vector<Player> trick_winners_;
};

// Abstract interface for a single phase of Hungarian Tarok.
//
// Intended usage pattern:
// - `HungarianTarokState` owns the current phase (e.g. via
//   `std::unique_ptr<GamePhase> phase_`).
// - `HungarianTarokState` implements the OpenSpiel `State` virtuals by
//   delegating to `*phase_`.
// - When a phase finishes, the state can replace `phase_` with the successor.
class GamePhase {
 public:
  virtual ~GamePhase() = default;

  virtual PhaseType phase_type() const = 0;

  // Delegation points corresponding to the OpenSpiel `State` API.
  virtual Player CurrentPlayer() const = 0;
  virtual std::vector<Action> LegalActions() const = 0;

  virtual void DoApplyAction(Action action) = 0;

  virtual bool GameOver() const { return false; }
  virtual bool PhaseOver() const = 0;
  virtual std::string ActionToString(Player player, Action action) const = 0;
  virtual std::string ToString() const = 0;

  // virtual std::string ObservationString(Player player) const = 0;
  // virtual void ObservationTensor(Player player, absl::Span<float> values)
  // const = 0;
  virtual std::unique_ptr<GamePhase> NextPhase() = 0;
  virtual std::unique_ptr<GamePhase> Clone() const = 0;

  virtual std::vector<double> Returns() const {
    return std::vector<double>(kNumPlayers, 0.0);
  }

 protected:
  explicit GamePhase(std::unique_ptr<GameData> game_data)
      : game_data_(std::move(game_data)) {}

  GamePhase(const GamePhase& other)
      : game_data_(other.game_data_
                       ? std::make_unique<GameData>(*other.game_data_)
                       : nullptr) {}
  GamePhase& operator=(const GamePhase& other) {
    if (this != &other) {
      game_data_ = other.game_data_
                       ? std::make_unique<GameData>(*other.game_data_)
                       : nullptr;
    }
    return *this;
  }
  GamePhase(GamePhase&&) = default;
  GamePhase& operator=(GamePhase&&) = default;

  GameData& game_data() { return *game_data_; }
  const GameData& game_data() const { return *game_data_; }

  Deck& deck() {
    SPIEL_CHECK_TRUE(game_data_ != nullptr);
    return game_data_->deck_;
  }
  const Deck& deck() const {
    SPIEL_CHECK_TRUE(game_data_ != nullptr);
    return game_data_->deck_;
  }

  Player& declarer() {
    CheckPhaseAtLeast(PhaseType::kBidding, "declarer");
    return game_data_->declarer_;
  }
  Player declarer() const {
    CheckPhaseAtLeast(PhaseType::kBidding, "declarer");
    return game_data_->declarer_;
  }

  int& winning_bid() {
    CheckPhaseAtLeast(PhaseType::kBidding, "winning_bid");
    return game_data_->winning_bid_;
  }
  int winning_bid() const {
    CheckPhaseAtLeast(PhaseType::kBidding, "winning_bid");
    return game_data_->winning_bid_;
  }

  bool& full_bid() {
    CheckPhaseAtLeast(PhaseType::kBidding, "full_bid");
    return game_data_->full_bid_;
  }
  bool full_bid() const {
    CheckPhaseAtLeast(PhaseType::kBidding, "full_bid");
    return game_data_->full_bid_;
  }

  std::optional<Player>& partner() {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "partner");
    return game_data_->partner_;
  }
  const std::optional<Player>& partner() const {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "partner");
    return game_data_->partner_;
  }

  void SetPagatHolder(Player player) {
    SPIEL_CHECK_TRUE(game_data_ != nullptr);
    game_data_->pagat_holder_ = player;
  }
  Player pagat_holder() const {
    CheckPhaseAtLeast(PhaseType::kBidding, "pagat_holder");
    return game_data_->pagat_holder_;
  }

  GameData::AnnouncementSide& declarer_side() {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "declarer_side");
    return game_data_->declarer_side_;
  }
  const GameData::AnnouncementSide& declarer_side() const {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "declarer_side");
    return game_data_->declarer_side_;
  }
  GameData::AnnouncementSide& opponents_side() {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "opponents_side");
    return game_data_->opponents_side_;
  }
  const GameData::AnnouncementSide& opponents_side() const {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "opponents_side");
    return game_data_->opponents_side_;
  }
  std::array<Side, kNumPlayers>& player_sides() {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "player_sides");
    return game_data_->player_sides_;
  }
  const std::array<Side, kNumPlayers>& player_sides() const {
    CheckPhaseAtLeast(PhaseType::kAnnouncements, "player_sides");
    return game_data_->player_sides_;
  }

  std::vector<GameData::Trick>& tricks() {
    CheckPhaseAtLeast(PhaseType::kPlay, "tricks");
    return game_data_->tricks_;
  }
  const std::vector<GameData::Trick>& tricks() const {
    CheckPhaseAtLeast(PhaseType::kPlay, "tricks");
    return game_data_->tricks_;
  }
  std::vector<Player>& trick_winners() {
    CheckPhaseAtLeast(PhaseType::kPlay, "trick_winners");
    return game_data_->trick_winners_;
  }
  const std::vector<Player>& trick_winners() const {
    CheckPhaseAtLeast(PhaseType::kPlay, "trick_winners");
    return game_data_->trick_winners_;
  }

  const GameData& game_data_for_scoring() const {
    CheckPhaseAtLeast(PhaseType::kPlay, "game_data_for_scoring");
    return *game_data_;
  }

  void CheckPhaseAtLeast(PhaseType min_phase, const char* field_name) const {
    SPIEL_CHECK_TRUE(game_data_ != nullptr);
    const int current = static_cast<int>(phase_type());
    const int required = static_cast<int>(min_phase);
    if (current < required) {
      open_spiel::SpielFatalError(open_spiel::internal::SpielStrCat(
          "Tried to access GameData field '", field_name,
          "' before it is ready (current phase: ", current,
          ", required >= ", required, ")."));
    }
  }

  std::unique_ptr<GameData> game_data_;
};

class SetupPhase : public GamePhase {
 public:
  SetupPhase();
  ~SetupPhase() override;
  PhaseType phase_type() const override { return PhaseType::kSetup; }
  Player CurrentPlayer() const override;
  std::vector<Action> LegalActions() const override;
  void DoApplyAction(Action action) override;
  bool PhaseOver() const override;
  std::unique_ptr<GamePhase> NextPhase() override;
  std::unique_ptr<GamePhase> Clone() const override;
  std::string ActionToString(Player player, Action action) const override;
  std::string ToString() const override;

 private:
  std::array<int, kNumPlayers> player_hands_sizes_;
  Card current_card_ = 0;
};
}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_