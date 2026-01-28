#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_ANNOUNCEMENTS_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_ANNOUNCEMENTS_H_

#include "spiel.h"
#include "game_phase.h"
#include "card.h"
#include "play.h"

#include <array>
#include <optional>
#include <vector>
#include <memory>

namespace open_spiel {
namespace hungarian_tarok {
    const Action kActionCallPartner = 0;
    const Action kActionCallSelf = 1;

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
    const int kNumAnnouncementTypes = 8;

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
            AnnouncementType type = static_cast<AnnouncementType>(action % kNumAnnouncementTypes);
            return AnnouncementAction{type, level};
        }
        constexpr Action ToAction() const {
            return static_cast<Action>(static_cast<int>(level) * kNumAnnouncementTypes + static_cast<int>(type));
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

    class AnnouncementsPhase : public GamePhase {
    public:
        AnnouncementsPhase(Player declarer, Deck deck)
            : current_player_(declarer),
              deck_(deck),
              declarer_(declarer) {
        }
        ~AnnouncementsPhase() override = default;
        Player CurrentPlayer() const override {
            return current_player_;
        }
        std::vector<Action> LegalActions() const override;
        void DoApplyAction(Action action) override;
        bool PhaseOver() const override {
            return current_player_ == kTerminalPlayerId;
        }
        std::unique_ptr<GamePhase> NextPhase() const override {
            return std::make_unique<PlayPhase>(deck_, declarer_);
        }
        std::unique_ptr<GamePhase> Clone() const override {
            return std::make_unique<AnnouncementsPhase>(*this);
        }
    private:
        Player current_player_ = 0;
        Deck deck_;

        struct Side {
            std::array<bool, kNumAnnouncementTypes> announced = {false};
            std::array<int, kNumAnnouncementTypes> contra_level = {0};
        };
        Side declarer_side_;
        Side opponents_side_;
        bool partner_called_ = false;
        std::optional<Player> partner_;
        Player declarer_;
        Player last_to_speak_;

        Side &CurrentSide() {
            return (current_player_ == declarer_ || (partner_ && current_player_ == *partner_))
                       ? declarer_side_
                       : opponents_side_;
        }
        Side &/*take it on the*/OtherSide() {
            return (current_player_ == declarer_ || (partner_ && current_player_ == *partner_))
                       ? opponents_side_
                       : declarer_side_;
        }
        const Side &CurrentSide() const {
            return (current_player_ == declarer_ || (partner_ && current_player_ == *partner_))
                       ? declarer_side_
                       : opponents_side_;
        }
        const Side &OtherSide() const {
            return (current_player_ == declarer_ || (partner_ && current_player_ == *partner_))
                       ? opponents_side_
                       : declarer_side_;
        }
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_ANNOUNCEMENTS_H_