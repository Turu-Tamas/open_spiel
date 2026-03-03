#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

namespace open_spiel {
namespace hungarian_tarok {

std::ostream& operator<<(std::ostream& os, const PhaseType& phase) {
  switch (phase) {
    case PhaseType::kSetup:
      os << "Setup";
      break;
    case PhaseType::kAnnulments:
      os << "Annulments";
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

std::ostream& operator<<(std::ostream& os,
                         const AnnouncementType& announcement) {
  switch (announcement) {
    case AnnouncementType::kTuletroa:
      os << "Tuletroa";
      break;
    case AnnouncementType::kFourKings:
      os << "Four Kings";
      break;
    case AnnouncementType::kXXICapture:
      os << "XXI Capture";
      break;
    case AnnouncementType::kDoubleGame:
      os << "Double Game";
      break;
    case AnnouncementType::kVolat:
      os << "Volat";
      break;
    case AnnouncementType::kPagatUltimo:
      os << "Pagat Ultimo";
      break;
    case AnnouncementType::kEightTaroks:
      os << "Eight Taroks";
      break;
    case AnnouncementType::kNineTaroks:
      os << "Nine Taroks";
      break;
    case AnnouncementType::kGame:
      os << "Game";
      break;
    default:
      SpielFatalError("Unknown announcement type");
  }
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
    case PhaseType::kAnnulments:
      return AnnulmentsCurrentPlayer();
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
    case PhaseType::kAnnulments:
      return AnnulmentsLegalActions();
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
    case PhaseType::kAnnulments:
      return AnnulmentsDoApplyAction(action);
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
    case PhaseType::kAnnulments:
      return AnnulmentsPhaseOver();
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
    case PhaseType::kAnnulments:
      return AnnulmentsGameOver();
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
    case PhaseType::kAnnulments:
      return AnnulmentsActionToString(player, action);
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
    case PhaseType::kAnnulments:
      return AnnulmentsToString();
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
      int bidder_count = absl::c_count(bidding_.has_bid_, true);
      common_state_.full_bid_ = (bidder_count == 3);
      common_state_.winning_bid_ = bidding_.winning_bid_.number;
      common_state_.declarer_ = bidding_.last_bidder_.value();
      common_state_.trial_three_ =
          common_state_.declarer_ == 3 && !bidding_.can_bid_[3];
      current_phase_ = PhaseType::kTalon;
      StartTalonPhase();
      return;
    }
    case PhaseType::kTalon:
      current_phase_ = PhaseType::kAnnulments;
      StartAnnulmentsPhase();
      return;
    case PhaseType::kAnnulments:
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

void HungarianTarokState::StartAnnulmentsPhase() {
  annulments_ = AnnulmentsState{};
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
