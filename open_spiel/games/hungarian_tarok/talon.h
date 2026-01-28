#include "spiel.h"
#include "game_phase.h"
#include "setup.h"
#include "bidding.h"

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_TALON_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_TALON_H_

namespace open_spiel {
namespace hungarian_tarok {
    const int kTalonSize = 6;
    class DealTalonPhase : public GamePhase {
    public:
        DealTalonPhase(SetupPhase &setup_phase, const BiddingPhase &bidding_phase) : deck_(setup_phase.GetDeck()) {
            Player declarer = bidding_phase.GetDeclarer();
            int declarer_cards_to_take = bidding_phase.GetWinningBid();
            cards_to_take_ = {0, 0, 0, 0};
            cards_to_take_[declarer] = declarer_cards_to_take;
            current_player_ = declarer;

            // distribute remaining talon cards to other players evenly, starting from the player after declarer
            int remaining_cards = kTalonSize - declarer_cards_to_take;
            Player player = (declarer + 1) % 4;
            while (remaining_cards > 0) {
                if (player != declarer) {
                    cards_to_take_[player]++;
                    remaining_cards--;
                }
                player = (player + 1) % 4;
            }

            int index = 0;
            for (Card card = 0; card < kDeckSize; ++card) {
                if (deck_[card] == kTalon)
                    talon_cards_[index++] = card;
            }
        }

        Player CurrentPlayer() const override {
            return PhaseOver() ? kTerminalPlayerId : kChancePlayerId;
        };
        std::vector<Action> LegalActions() const override {
            SPIEL_CHECK_FALSE(PhaseOver());
            std::vector<Action> actions;
            for (int i = 0; i < kTalonSize; ++i) {
                if (!talon_taken_[i]) {
                    actions.push_back(i);
                }
            }
            return actions;
        }

        void DoApplyAction(Action action) override {
            SPIEL_CHECK_GE(action, 0);
            SPIEL_CHECK_LT(action, kTalonSize);
            SPIEL_CHECK_FALSE(talon_taken_[action]);
            SPIEL_CHECK_FALSE(PhaseOver());

            talon_taken_[action] = true;
            deck_[talon_cards_[action]] = current_player_;
            talon_taken_count_++;
            cards_to_take_[current_player_]--;

            if (talon_taken_count_ == kTalonSize) {
                current_player_ = kTerminalPlayerId;
            } else if (cards_to_take_[current_player_] == 0) {
                current_player_ = (current_player_ + 1) % 4;
            }
        }

        bool PhaseOver() const override {
            return current_player_ == kTerminalPlayerId;
        }

    private:
        Deck &deck_;
        Player current_player_;
        std::array<Card, kTalonSize> talon_cards_;
        std::array<bool, kTalonSize> talon_taken_ = {false};
        std::array<int, 4> cards_to_take_;
        int talon_taken_count_ = 0;
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_TALON_H_