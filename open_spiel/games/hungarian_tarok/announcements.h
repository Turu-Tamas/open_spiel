#include "spiel.h"
#include "game_phase.h"
#include "card.h"
#include "bidding.h"
#include "setup.h"

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_ANNOUNCEMENTS_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_ANNOUNCEMENTS_H_

namespace open_spiel {
namespace hungarian_tarok {

    class AnnouncementsPhase : public GamePhase {
    public:
        AnnouncementsPhase(const BiddingPhase &bidding_phase, SetupPhase &setup_phase);
        ~AnnouncementsPhase() override = default;
        Player CurrentPlayer() const override {
            return current_player_;
        }
        std::vector<Action> LegalActions() const override;
        void DoApplyAction(Action action) override;
        bool PhaseOver() const override {
            return current_player_ == kTerminalPlayerId;
        }
    private:
        Player current_player_ = 0;
        const Deck &deck_;
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_ANNOUNCEMENTS_H_