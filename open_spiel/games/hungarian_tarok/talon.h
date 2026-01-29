#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_TALON_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_TALON_H_

#include "spiel.h"
#include "game_phase.h"
#include "bidding.h"
#include "skart.h"

namespace open_spiel {
namespace hungarian_tarok {
    class DealTalonPhase : public GamePhase {
    public:
        DealTalonPhase(GameData &game_data) : GamePhase(game_data) {
            Player declarer = game_data.declarer_;
            int declarer_cards_to_take = game_data.winning_bid_;
            cards_to_take_ = {0, 0, 0, 0};
            cards_to_take_[declarer] = declarer_cards_to_take;
            current_player_ = declarer;

            // distribute remaining talon cards to other players evenly, starting from the player after declarer
            int remaining_cards = kTalonSize - declarer_cards_to_take;
            Player player = (declarer + 1) % kNumPlayers;
            while (remaining_cards > 0) {
                if (player != declarer) {
                    cards_to_take_[player]++;
                    remaining_cards--;
                }
                player = (player + 1) % kNumPlayers;
            }

            int index = 0;
            for (Card card = 0; card < kDeckSize; ++card) {
                if (game_data_.deck_[card] == kTalon)
                    talon_cards_[index++] = card;
            }
            SPIEL_CHECK_EQ(index, kTalonSize);
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
            std::cout << action << " " << cards_to_take_[current_player_] << " " << talon_cards_[action] << std::endl;
            talon_taken_[action] = true;
            game_data_.deck_[talon_cards_[action]] = current_player_;
            talon_taken_count_++;
            cards_to_take_[current_player_]--;

            if (talon_taken_count_ == kTalonSize) {
                current_player_ = kTerminalPlayerId;
            } else if (cards_to_take_[current_player_] == 0) {
                current_player_ = (current_player_ + 1) % kNumPlayers;
            }
        }

        bool PhaseOver() const override {
            return current_player_ == kTerminalPlayerId;
        }
        std::unique_ptr<GamePhase> NextPhase() const override {
            SPIEL_CHECK_TRUE(PhaseOver());
            return std::make_unique<SkartPhase>(game_data_);
        }
        std::unique_ptr<GamePhase> Clone() const override {
            return std::make_unique<DealTalonPhase>(*this);
        }
        std::string ActionToString(Player player, Action action) const override {
            SPIEL_CHECK_GE(action, 0);
            SPIEL_CHECK_LT(action, kTalonSize);
            return absl::StrCat("Take talon card ", action);
        }
        std::string ToString() const override {
            return "Dealing Talon Phase";
        }
    private:
        Player current_player_;
        std::array<Card, kTalonSize> talon_cards_;
        std::array<bool, kTalonSize> talon_taken_ = {false};
        std::array<int, kNumPlayers> cards_to_take_;
        int talon_taken_count_ = 0;
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_TALON_H_