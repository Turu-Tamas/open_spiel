#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SETUP_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SETUP_H_

#include "game_phase.h"
#include "card.h"
#include "spiel.h"
#include "spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarok {
    class SetupPhase : public GamePhase {
    public:
        SetupPhase() = default;
        ~SetupPhase() override = default;

        Player CurrentPlayer() const override {
            return kChancePlayerId;
        }
        std::vector<Action> LegalActions() const override {
            std::vector<Action> actions;
            for (int player = 0; player < kNumPlayers; ++player) {
                if (player_hands_sizes_[player] < 9) 
                    actions.push_back(player);
            }
            return actions;
        }
        void DoApplyAction(Action action) override {
            SPIEL_CHECK_LT(current_card_, kDeckSize);
            // Action is the player ID who receives the next card.
            deck_[current_card_] = action;
            player_hands_sizes_[action]++;
            current_card_++;
        }
        bool PhaseOver() const override {
            return current_card_ >= kDeckSize;
        }
        bool GameOver() const override {
            return false;
        }
        const Deck &GetDeck() const {
            return deck_;
        }
        Deck &GetDeck() {
            return deck_;
        }
        std::unique_ptr<GamePhase> NextPhase() const override {
            SPIEL_CHECK_TRUE(PhaseOver());
            return std::make_unique<BiddingPhase>(*this);
        }

    private:
        Deck deck_;
        std::array<int, kNumPlayers> player_hands_sizes_;
        Card current_card_ = 0;
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SETUP_H_