#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PHASES_H_

#include <array>
#include <optional>
#include <vector>

#include "card.h"
#include "spiel.h"

namespace open_spiel {
namespace hungarian_tarok {

enum class BidType {
  kStandard,
  kInvitXIX,
  kInvitXVIII,
  kStraightSolo,  // solo as first bidder
};

constexpr std::optional<Card> IndicatedCard(BidType bid_type) {
  switch (bid_type) {
    case BidType::kStandard:
      return std::nullopt;
    case BidType::kInvitXIX:
      return MakeTarok(19);
    case BidType::kInvitXVIII:
      return MakeTarok(18);
    case BidType::kStraightSolo:
      return std::nullopt;
  }
  SpielFatalError("Unknown BidType");
}

struct Bid {
  int number;
  bool is_hold;

  constexpr static Bid FromAction(Action action) {
    SPIEL_CHECK_GE(action, MinAction());
    SPIEL_CHECK_LE(action, MaxAction());
    Bid result{};
    result.number = action / 2;
    result.is_hold = (action % 2 == 1);
    return result;
  }
  constexpr static Action MinAction() { return 0; }
  constexpr static Action MaxAction() { return 6; }
  constexpr Action ToAction() const {
    return static_cast<Action>(number * 2 + (is_hold ? 1 : 0));
  }
  constexpr static Bid NewInitialBid() { return Bid{4, true}; }
  constexpr static Action PassAction() { return MaxAction() + 1; }
  std::optional<Bid> NextBid(BidType bid_type, bool first_bid) const;
  constexpr bool operator==(const Bid& other) const {
    return number == other.number && is_hold == other.is_hold;
  }

  BidType GetBidTypeOf(Action action, bool first_bid) const;
  bool NextBidCanBe(Action action) const;
};

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

std::ostream& operator<<(std::ostream& os, const PhaseType& phase);

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
  bool full_bid_;             // wether all three honours bid
  bool trial_three_ = false;  // when declarer draws three cards as last player
                              // without an honour
  std::optional<Card> mandatory_called_card_;
  std::optional<Player> cue_bidder_ = std::nullopt;

  // announcements results
  std::optional<Player> partner_;
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