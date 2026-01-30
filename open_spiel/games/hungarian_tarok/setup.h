#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SETUP_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SETUP_H_

#include "game_phase.h"
#include "card.h"
#include "spiel.h"
#include "spiel_utils.h"
#include "bidding.h"

#include <memory>
#include <array>

namespace open_spiel {
namespace hungarian_tarok {

    const int kPlayerHandSize = 9;

    class SetupPhase : public GamePhase {
    public:
        SetupPhase() : GamePhase(std::make_unique<GameData>()) {
            game_data().deck_.fill(kTalon); // all cards not dealt stay in the talon
            player_hands_sizes_.fill(0);
        }
        ~SetupPhase() override = default;

        Player CurrentPlayer() const override {
            return kChancePlayerId;
        }
        std::vector<Action> LegalActions() const override {
            std::vector<Action> actions;
            for (int player = 0; player < kNumPlayers; ++player) {
                if (player_hands_sizes_[player] < kPlayerHandSize)
                    actions.push_back(player);
            }
            return actions;
        }
        void DoApplyAction(Action action) override {
            SPIEL_CHECK_GE(action, 0);
            SPIEL_CHECK_LT(action, kNumPlayers);
            SPIEL_CHECK_FALSE(PhaseOver());
            if (current_card_ == kPagat) {
                game_data().pagat_holder_ = action;
            }
            // Action is the player ID who receives the next card.
            game_data().deck_[current_card_] = action;
            player_hands_sizes_[action]++;
            current_card_++;
        }
        bool PhaseOver() const override {
            return current_card_ >= kPlayerHandSize * kNumPlayers;
        }
        std::unique_ptr<GamePhase> NextPhase() override {
            SPIEL_CHECK_TRUE(PhaseOver());
            return std::make_unique<BiddingPhase>(std::move(game_data_));
        }
        std::unique_ptr<GamePhase> Clone() const override {
            return std::make_unique<SetupPhase>(*this);
        }
        std::string ActionToString(Player player, Action action) const override {
            SPIEL_CHECK_GE(action, 0);
            SPIEL_CHECK_LT(action, kNumPlayers);
            return absl::StrCat("Deal card ", CardToString(current_card_) ,
                                " to player ", action);
        }
        std::string ToString() const override {
            return "Setup Phase";
        }
    private:
        std::array<int, kNumPlayers> player_hands_sizes_;
        Card current_card_ = 0;
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SETUP_H_