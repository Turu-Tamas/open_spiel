#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_

#include <array>
#include <optional>
#include <vector>

#include "card.h"
#include "spiel.h"

namespace open_spiel {
namespace hungarian_tarok {

constexpr Action kBiddingActionPass = 0;
constexpr Action kBiddingActionStandardBid = 1;
constexpr Action kBiddingActionInvitXIX = 2;
constexpr Action kBiddingActionInvitXVIII = 3;

constexpr Action kAnnouncementsActionCallPartner = 0;
constexpr Action kAnnouncementsActionCallSelf = 1;

constexpr int kMaxContraLevel = 5;

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

// Shared game state across different phases.
struct CommonState {
  Deck deck_;
  Player pagat_holder_;

  // bidding results
  Player declarer_;
  int winning_bid_;
  bool full_bid_;  // wether all three honours bid
  std::optional<Player> partner_;
  bool trial_three_ = false;  // when declarer draws three cards as last player
                              // without an honour

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
}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_