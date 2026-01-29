#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_BIDDING_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_BIDDING_H_

#include <array>
#include <optional>

#include "card.h"
#include "game_phase.h"

namespace open_spiel {
namespace hungarian_tarok {
    class SetupPhase;

    const Action kActionPass = 0;
    const Action kActionStandardBid = 1;

    class BiddingPhase : public GamePhase {
    public:
		explicit BiddingPhase(std::unique_ptr<GameData> game_data);
        ~BiddingPhase() override = default;
        Player CurrentPlayer() const override {
            return current_player_;
        }
        std::vector<Action> LegalActions() const override;
        void DoApplyAction(Action action) override;
        bool PhaseOver() const override {
            return current_player_ == kTerminalPlayerId;
        }
        bool GameOver() const override {
            return all_passed_;
        }
        std::unique_ptr<GamePhase> NextPhase() override;
        std::unique_ptr<GamePhase> Clone() const override {
            return std::make_unique<BiddingPhase>(*this);
        }
        std::string ActionToString(Player player, Action action) const override;
        std::string ToString() const override {
            return "Bidding Phase";
        }
    private:
        Player current_player_ = 0;
        int lowest_bid_ = 4;
        bool was_held_ = false;
        bool all_passed_ = false;
        std::array<bool, kNumPlayers> has_passed_ = {false, false, false, false};
        std::array<bool, kNumPlayers> has_honour_ = {false, false, false, false};
        std::array<bool, kNumPlayers> has_bid_ = {false, false, false, false};

        void NextPlayer();
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_BIDDING_H_