#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_

#include <array>
#include <optional>
#include <vector>

#include "card.h"
#include "spiel.h"

namespace open_spiel {
namespace hungarian_tarok {
const int kNumAnnouncementTypes = 8;
const int kPlayerHandSize = 9;
constexpr int kTalonSize = 6;
constexpr int kNumRounds = 9;

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
} // namespace hungarian_tarok
} // namespace open_spiel

#endif // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_