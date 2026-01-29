#include "bidding.h"
#include "setup.h"
#include "talon.h"

namespace open_spiel {
namespace hungarian_tarok {
    BiddingPhase::BiddingPhase(std::unique_ptr<GameData> game_data)
        : GamePhase(std::move(game_data)) {
		has_honour_[this->game_data().deck_[kSkiz]] = true;
		has_honour_[this->game_data().deck_[kPagat]] = true;
		has_honour_[this->game_data().deck_[kXXI]] = true;
    }
    std::vector<Action> BiddingPhase::LegalActions() const {
        SPIEL_CHECK_FALSE(PhaseOver());
        if (!has_honour_[current_player_]) {
            return {kActionPass};
        }
        return {kActionPass, kActionStandardBid};
    }
    void BiddingPhase::NextPlayer() {
        Player next_player = (current_player_ + 1) % kNumPlayers;
        while (has_passed_[next_player] && next_player != current_player_) {
            next_player = (next_player + 1) % kNumPlayers;
        }
        if (next_player == game_data().declarer_) {
            current_player_ = kTerminalPlayerId;
            return;
        }
        if (next_player == current_player_) {
            // everyone passed
            current_player_ = kTerminalPlayerId;
            all_passed_ = true;
            game_data().declarer_ = -1;
            game_data().winning_bid_ = -1;
            return;
        }
        current_player_ = next_player;
    }
    void BiddingPhase::DoApplyAction(Action action) {
        SPIEL_CHECK_FALSE(PhaseOver());
        std::vector<Action> legal_actions = LegalActions();
        SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

        if (action == kActionStandardBid) {
            // no holding as first bid
            if (!was_held_ && !has_bid_[current_player_]) {
                was_held_ = true;
            } else {
                lowest_bid_ -= 1;
                was_held_ = false;
            }
            game_data().declarer_ = current_player_;
            has_bid_[current_player_] = true;
        } else if (action == kActionPass) {
            has_passed_[current_player_] = true;
        }
        NextPlayer();
    }

    std::unique_ptr<GamePhase> BiddingPhase::NextPhase() {
        SPIEL_CHECK_TRUE(PhaseOver());

        int bidder_count = std::count(has_bid_.begin(), has_bid_.end(), true);
        game_data().full_bid_ = (bidder_count == 3); // all three honours bid
        game_data().winning_bid_ = lowest_bid_;
        return std::make_unique<DealTalonPhase>(std::move(game_data_));
    }
    std::string BiddingPhase::ActionToString(Player player, Action action) const {
        SPIEL_CHECK_FALSE(PhaseOver());
        std::vector<Action> legal_actions = LegalActions();
        SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());
        if (action == kActionPass) {
            return "Pass";
        } else if (action == kActionStandardBid) {
            return was_held_ || !has_bid_[current_player_] ? absl::StrCat("Bid ", lowest_bid_ - 1) :
                              absl::StrCat("Bid hold ", lowest_bid_);
        } else {
            return "Unknown action";
        }
    }
} // namespace hungarian_tarok
} // namespace open_spiel