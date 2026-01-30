#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_GAME_PHASE_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_GAME_PHASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/types/span.h"
#include "open_spiel/spiel.h"

#include "card.h"

namespace open_spiel {
namespace hungarian_tarok {

class HungarianTarokState;

enum class Side {
  kDeclarer,
  kOpponents,
};

// Shared game state across different phases.
struct GameData {
  Deck deck_;
  Player pagat_holder_;

  // bidding results
  Player declarer_;
  int winning_bid_;
  bool full_bid_; // wether all three honours bid
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

  // Delegation points corresponding to the OpenSpiel `State` API.
  virtual Player CurrentPlayer() const = 0;
  virtual std::vector<Action> LegalActions() const = 0;

  virtual void DoApplyAction(Action action) = 0;

  virtual bool GameOver() const { return false; }
  virtual bool PhaseOver() const = 0;
  virtual std::string ActionToString(Player player, Action action) const = 0;
  virtual std::string ToString() const = 0;

  // virtual std::string ObservationString(Player player) const = 0;
  // virtual void ObservationTensor(Player player, absl::Span<float> values) const = 0;
  virtual std::unique_ptr<GamePhase> NextPhase() = 0;
  virtual std::unique_ptr<GamePhase> Clone() const = 0;

 protected:
  explicit GamePhase(std::unique_ptr<GameData> game_data)
      : game_data_(std::move(game_data)) {}

  GamePhase(const GamePhase& other)
      : game_data_(other.game_data_ ? std::make_unique<GameData>(*other.game_data_)
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

  std::unique_ptr<GameData> game_data_;
};

}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_GAME_PHASE_H_
