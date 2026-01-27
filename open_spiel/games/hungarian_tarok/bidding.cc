#include "bidding.h"

namespace open_spiel {
namespace hungarian_tarok {
    BiddingPhase::BiddingPhase(const SetupPhase &setup_phase) {
        const Deck &deck = setup_phase.GetDeck();
        has_honour_[deck[kSkiz]] = true;
        has_honour_[deck[kPagat]] = true;
        has_honour_[deck[kXXI]] = true;
    }
    std::vector<Action> BiddingPhase::LegalActions() const {
        SPIEL_CHECK_FALSE(PhaseOver());
        if (!has_honour_[current_player_]) {
            return {kActionPass};
        }
        return {kActionPass, kActionStandardBid};
    }
    void BiddingPhase::NextPlayer() {
        Player next_player = (current_player_ + 1) % 4;
        while (has_passed_[next_player]) {
            next_player = (next_player + 1) % 4;
        }
        if (next_player == winning_bidder_) {
            current_player_ = kTerminalPlayerId;
            return;
        }
        current_player_ = next_player;
    }
    void BiddingPhase::DoApplyAction(Action action) {
        SPIEL_CHECK_FALSE(PhaseOver());
        std::vector<Action> legal_actions = LegalActions();
        SPIEL_CHECK_TRUE(std::find(legal_actions.begin(), legal_actions.end(), action) != legal_actions.end());
        if (action == kActionStandardBid) {
            if (!was_held_) {
                was_held_ = true;
            } else {
                lowest_bid_ -= 1;
                was_held_ = false;
            }
            winning_bidder_ = current_player_;
        } else if (action == kActionPass) {
            has_passed_[current_player_] = true;
        }
        NextPlayer();
    }
} // namespace hungarian_tarok
} // namespace open_spiel