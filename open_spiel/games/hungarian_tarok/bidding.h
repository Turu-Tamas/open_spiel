#include "game_phase.h"
#include "setup.h"

namespace open_spiel {
namespace hungarian_tarok {
    const Action kActionPass = 0;
    const Action kActionStandardBid = 1;
    class BiddingPhase : public GamePhase {
    public:
        BiddingPhase(const SetupPhase &setup_phase);
        ~BiddingPhase() override = default;
        Player CurrentPlayer() const override {
            return current_player_;
        }
        std::vector<Action> LegalActions() const override;
        void DoApplyAction(Action action) override;
        bool PhaseOver() const override {
            return current_player_ == kTerminalPlayerId;
        }
        Player GetDeclarer() const {
            SPIEL_CHECK_TRUE(PhaseOver());
            return winning_bidder_.value();
        }
    private:
        Player current_player_ = 0;
        int lowest_bid_ = 4;
        bool was_held_ = false;
        std::array<bool, 4> has_passed_ = {false, false, false, false};
        std::array<bool, 4> has_honour_ = {false, false, false, false};
        std::optional<Player> winning_bidder_ = std::nullopt;

        void NextPlayer();
    };
} // namespace hungarian_tarok
} // namespace open_spiel