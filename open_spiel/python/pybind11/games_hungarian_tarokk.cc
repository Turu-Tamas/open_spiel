// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/python/pybind11/games_hungarian_tarokk.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <pybind11/stl.h>

#include "open_spiel/json/include/nlohmann/json.hpp"  // IWYU pragma: keep
#include "open_spiel/games/hungarian_tarokk/announcements.h"
#include "open_spiel/games/hungarian_tarokk/bidding.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/games/hungarian_tarokk/hungarian_tarokk.h"
#include "open_spiel/games/hungarian_tarokk/talon.h"
#include "open_spiel/python/pybind11/pybind11.h"
#include "open_spiel/spiel.h"

namespace py = ::pybind11;

using open_spiel::Game;
using open_spiel::ObservationStruct;
using open_spiel::Player;
using open_spiel::State;
using open_spiel::hungarian_tarokk::AnnouncementState;
using open_spiel::hungarian_tarokk::Bid;
using open_spiel::hungarian_tarokk::BiddingState;
using open_spiel::hungarian_tarokk::Bonus;
using open_spiel::hungarian_tarokk::CalledCard;
using open_spiel::hungarian_tarokk::Card;
using open_spiel::hungarian_tarokk::CardPoints;
using open_spiel::hungarian_tarokk::CardSuit;
using open_spiel::hungarian_tarokk::CardToString;
using open_spiel::hungarian_tarokk::HungarianTarokkBonusAnnouncement;
using open_spiel::hungarian_tarokk::HungarianTarokkCall;
using open_spiel::hungarian_tarokk::HungarianTarokkGame;
using open_spiel::hungarian_tarokk::HungarianTarokkObservationStruct;
using open_spiel::hungarian_tarokk::HungarianTarokkState;
using open_spiel::hungarian_tarokk::HungarianTarokkTrick;
using open_spiel::hungarian_tarokk::IsTarokk;
using open_spiel::hungarian_tarokk::Phase;
using open_spiel::hungarian_tarokk::Side;

namespace {

// Pure tag types, used only so the action-id constants/methods below (mirror-
// ing the disjoint per-phase ranges owned by bidding.h, talon.h and
// announcements.h) can be namespaced under a simple python class per phase,
// instead of dumped flat into the module.
struct BiddingActions {};
struct TalonActions {};
struct AnnouncementActions {};

}  // namespace

void open_spiel::init_pyspiel_games_hungarian_tarokk(py::module& m) {
  py::module_ ht = m.def_submodule("hungarian_tarokk");

  // ---- Enums --------------------------------------------------------------
  py::enum_<Phase>(ht, "HungarianTarokkPhase")
      .value("DEALING", Phase::kDealing)
      .value("BIDDING", Phase::kBidding)
      .value("TALON_EXCHANGE", Phase::kTalonExchange)
      .value("ANNOUNCEMENTS", Phase::kAnnouncements)
      .value("PLAYING", Phase::kPlaying)
      .value("FINISHED", Phase::kFinished);
  ht.def("phase_actions", &open_spiel::hungarian_tarokk::PhaseActions,
         py::arg("phase"));

  py::enum_<Bid>(ht, "HungarianTarokkBid")
      .value("THREE", Bid::kThree)
      .value("TWO", Bid::kTwo)
      .value("ONE", Bid::kOne)
      .value("SOLO", Bid::kSolo);

  py::enum_<CalledCard>(ht, "HungarianTarokkCalledCard")
      .value("NONE", CalledCard::kNone)
      .value("XIX", CalledCard::kXIX)
      .value("XVIII", CalledCard::kXVIII)
      .value("XX", CalledCard::kXX);

  py::enum_<Side>(ht, "HungarianTarokkSide")
      .value("DECLARERS", Side::kDeclarers)
      .value("DEFENDERS", Side::kDefenders);

  py::enum_<Bonus>(ht, "HungarianTarokkBonus")
      .value("TRULL", Bonus::kTrull)
      .value("FOUR_KINGS", Bonus::kFourKings)
      .value("PAGAT_ULTI", Bonus::kPagatUlti)
      .value("XXI_CATCH", Bonus::kXxiCatch)
      .value("DOUBLE_GAME", Bonus::kDoubleGame)
      .value("VOLAT", Bonus::kVolat);

  // ---- Card ---------------------------------------------------------------
  py::class_<Card>(ht, "Card")
      .def(py::init<>())
      .def(py::init([](int index) { return Card{index}; }), py::arg("index"))
      .def_readwrite("index", &Card::index)
      .def("is_tarokk", [](const Card& c) { return IsTarokk(c); })
      .def("points", [](const Card& c) { return CardPoints(c); })
      .def("suit", [](const Card& c) { return CardSuit(c); })
      .def("__str__", [](const Card& c) { return CardToString(c); })
      .def("__repr__",
           [](const Card& c) { return "Card(" + CardToString(c) + ")"; })
      .def("__eq__", [](const Card& a, const Card& b) { return a == b; },
           py::is_operator())
      .def("__hash__", [](const Card& c) { return std::hash<int>()(c.index); });

  // ---- Action ids -----------------------------------------------------------
  // A card action is simply the card's 0..41 index (Card.index above; tarokks
  // are 0..21). The auction, talon exchange and announcement phases each own a
  // disjoint range of action ids above that (see the respective headers'
  // comments); the constants/methods below expose those ranges to python
  // without needing to parse or produce string representations.
  ht.attr("NUM_DISTINCT_ACTIONS") =
      open_spiel::hungarian_tarokk::kNumDistinctActions;
  ht.attr("NUM_CARDS") = open_spiel::hungarian_tarokk::kNumCards;
  ht.attr("NUM_TAROKKS") = open_spiel::hungarian_tarokk::kNumTarokks;

  py::class_<BiddingActions> bidding_actions(ht, "BiddingActions");
  bidding_actions.attr("ACTION_BASE") =
      open_spiel::hungarian_tarokk::kBiddingActionBase;
  bidding_actions.attr("NUM_ACTIONS") =
      open_spiel::hungarian_tarokk::kNumBiddingActions;
  bidding_actions.attr("PASS") = open_spiel::hungarian_tarokk::kActionPass;
  bidding_actions.attr("BID_THREE") =
      open_spiel::hungarian_tarokk::kActionBidThree;
  bidding_actions.attr("BID_TWO") =
      open_spiel::hungarian_tarokk::kActionBidTwo;
  bidding_actions.attr("BID_ONE") =
      open_spiel::hungarian_tarokk::kActionBidOne;
  bidding_actions.attr("BID_SOLO") =
      open_spiel::hungarian_tarokk::kActionBidSolo;
  bidding_actions.attr("NUM_ACTIONS") = 
    open_spiel::hungarian_tarokk::kNumBiddingActions;
  bidding_actions.attr("HOLD") = open_spiel::hungarian_tarokk::kActionHold;
  bidding_actions
      .def_static("bid_to_action", &open_spiel::hungarian_tarokk::BidToAction,
                  py::arg("bid"))
      .def_static("action_to_bid", &open_spiel::hungarian_tarokk::ActionToBid,
                  py::arg("action"))
      .def_static("is_bid_action", &open_spiel::hungarian_tarokk::IsBidAction,
                  py::arg("action"))
      .def_static("is_bidding_action",
                  &open_spiel::hungarian_tarokk::IsBiddingAction,
                  py::arg("action"));

  py::class_<TalonActions> talon_actions(ht, "TalonActions");
  talon_actions.attr("DISCARD_ACTION_BASE") =
      open_spiel::hungarian_tarokk::kDiscardActionBase;
  talon_actions.attr("ANNUL") = open_spiel::hungarian_tarokk::kActionAnnul;
  talon_actions.attr("DECLINE_ANNUL") =
      open_spiel::hungarian_tarokk::kActionDeclineAnnul;
  talon_actions
      .def_static("discard_action_for_card",
                  &open_spiel::hungarian_tarokk::DiscardActionForCard,
                  py::arg("card"))
      .def_static("card_for_discard_action",
                  &open_spiel::hungarian_tarokk::CardForDiscardAction,
                  py::arg("action"))
      .def_static("is_discard_action",
                  &open_spiel::hungarian_tarokk::IsDiscardAction,
                  py::arg("action"));

  py::class_<AnnouncementActions> announcement_actions(ht,
                                                        "AnnouncementActions");
  announcement_actions.attr("CALL_ACTION_BASE") =
      open_spiel::hungarian_tarokk::kCallActionBase;
  announcement_actions.attr("ANNOUNCE_BONUS_BASE") =
      open_spiel::hungarian_tarokk::kAnnounceBonusBase;
  announcement_actions.attr("KONTRA_ACTION_BASE") =
      open_spiel::hungarian_tarokk::kKontraActionBase;
  announcement_actions.attr("GAME_KONTRA_ITEM") =
      open_spiel::hungarian_tarokk::kGameKontraItem;
  announcement_actions.attr("NUM_KONTRA_ITEMS") =
      open_spiel::hungarian_tarokk::kNumKontraItems;
  // A kontra action names its level (kontra/rekontra/szub-/hirskontra are
  // distinct calls, C §5.3) but never a side; NUM_KONTRA_GROUPS is the game
  // plus one per bonus, NUM_KONTRA_LEVELS is those four levels, and their
  // product is the total kontra action count.
  announcement_actions.attr("NUM_KONTRA_GROUPS") =
      open_spiel::hungarian_tarokk::kNumKontraGroups;
  announcement_actions.attr("NUM_KONTRA_LEVELS") =
      open_spiel::hungarian_tarokk::kNumKontraLevels;
  announcement_actions.attr("NUM_KONTRA_ACTIONS") =
      open_spiel::hungarian_tarokk::kNumKontraActions;
  announcement_actions.attr("KONTRA_GAME") =
      open_spiel::hungarian_tarokk::kActionKontraGame;
  announcement_actions.attr("REKONTRA_GAME") =
      open_spiel::hungarian_tarokk::kActionRekontraGame;
  announcement_actions.attr("SZUBKONTRA_GAME") =
      open_spiel::hungarian_tarokk::kActionSzubkontraGame;
  announcement_actions.attr("HIRSKONTRA_GAME") =
      open_spiel::hungarian_tarokk::kActionHirskontraGame;
  announcement_actions.attr("DECLARE_EIGHT") =
      open_spiel::hungarian_tarokk::kActionDeclareEight;
  announcement_actions.attr("DECLARE_NINE") =
      open_spiel::hungarian_tarokk::kActionDeclareNine;
  announcement_actions.attr("PASS") =
      open_spiel::hungarian_tarokk::kActionAnnouncePass;
  announcement_actions.attr("LAST_ACTION") =
      open_spiel::hungarian_tarokk::kLastAnnounceAction;
  announcement_actions.attr("NUM_ACTIONS") =
      open_spiel::hungarian_tarokk::kNumAnnouncementActions;
  announcement_actions
      .def_static("call_action_for_tarokk",
                  &open_spiel::hungarian_tarokk::CallActionForTarokk,
                  py::arg("tarokk"))
      .def_static("tarokk_for_call_action",
                  &open_spiel::hungarian_tarokk::TarokkForCallAction,
                  py::arg("action"))
      .def_static("announce_bonus_action",
                  &open_spiel::hungarian_tarokk::AnnounceBonusAction,
                  py::arg("bonus"))
      .def_static("kontra_game_action",
                  &open_spiel::hungarian_tarokk::KontraGameAction,
                  py::arg("level"))
      .def_static("kontra_bonus_action",
                  &open_spiel::hungarian_tarokk::KontraBonusAction,
                  py::arg("bonus"), py::arg("level"))
      .def_static("kontra_item_for_bonus",
                  &open_spiel::hungarian_tarokk::KontraItemForBonus,
                  py::arg("bonus"), py::arg("side"))
      .def_static("bonus_for_kontra_item",
                  &open_spiel::hungarian_tarokk::BonusForKontraItem,
                  py::arg("item"))
      .def_static("side_for_kontra_item",
                  &open_spiel::hungarian_tarokk::SideForKontraItem,
                  py::arg("item"))
      .def_static("is_call_action",
                  &open_spiel::hungarian_tarokk::IsCallAction,
                  py::arg("action"))
      .def_static("is_announce_bonus_action",
                  &open_spiel::hungarian_tarokk::IsAnnounceBonusAction,
                  py::arg("action"))
      .def_static("is_kontra_action",
                  &open_spiel::hungarian_tarokk::IsKontraAction,
                  py::arg("action"))
      .def_static("is_tarokk_declare_action",
                  &open_spiel::hungarian_tarokk::IsTarokkDeclareAction,
                  py::arg("action"));

  // ---- Bidding sub-state --------------------------------------------------
  py::class_<BiddingState>(ht, "HungarianTarokkBiddingState")
      .def("current_player", &BiddingState::CurrentPlayer)
      .def("legal_actions", &BiddingState::LegalActions)
      .def("is_finished", &BiddingState::IsFinished)
      .def("passed_out", &BiddingState::PassedOut)
      .def("declarer", &BiddingState::Declarer)
      .def("winning_bid", &BiddingState::WinningBid)
      .def("num_bidders", &BiddingState::NumBidders)
      .def("cue_bidder", &BiddingState::CueBidder)
      .def("cued_card", &BiddingState::CuedCard)
      .def("yielded", &BiddingState::Yielded)
      .def("obligatory_called_card", &BiddingState::ObligatoryCalledCard)
      .def("pagat_ulti_obligation", &BiddingState::PagatUltiObligation)
      .def("standing_bid",
           [](const BiddingState& b) -> py::object {
             auto v = b.StandingBid();
             return v.has_value() ? py::cast(*v) : py::none();
           })
      .def("player_bid",
           [](const BiddingState& b, Player p) -> py::object {
             auto v = b.PlayerBid(p);
             return v.has_value() ? py::cast(*v) : py::none();
           },
           py::arg("player"))
      .def("__str__", &BiddingState::ToString)
      .def("to_string", &BiddingState::ToString);

  // ---- Announcement sub-state ---------------------------------------------
  py::class_<AnnouncementState>(ht, "HungarianTarokkAnnouncementState")
      .def("current_player", &AnnouncementState::CurrentPlayer)
      .def("legal_actions", &AnnouncementState::LegalActions)
      .def("is_finished", &AnnouncementState::IsFinished)
      .def("called_card_tarokk", &AnnouncementState::CalledCardTarokk)
      .def("partner", &AnnouncementState::Partner)
      .def("declared_tarokks", &AnnouncementState::DeclaredTarokks,
           py::arg("player"))
      .def("public_side",
           [](const AnnouncementState& a, Player p) -> py::object {
             auto v = a.PublicSide(p);
             return v.has_value() ? py::cast(*v) : py::none();
           },
           py::arg("player"))
      .def("hivatalbol_kontra_player",
           &AnnouncementState::HivatalbolKontraPlayer)
      .def("bonus_announced", &AnnouncementState::BonusAnnounced,
           py::arg("bonus"), py::arg("side"))
      .def("bonus_kontra_level", &AnnouncementState::BonusKontraLevel,
           py::arg("bonus"), py::arg("side"))
      .def("game_kontra_level", &AnnouncementState::GameKontraLevel)
      .def("__str__", &AnnouncementState::ToString)
      .def("to_string", &AnnouncementState::ToString);

  // ---- Observation struct (+ nested record types) --------------------------
  py::class_<HungarianTarokkBonusAnnouncement>(
      ht, "HungarianTarokkBonusAnnouncement")
      .def(py::init<>())
      .def_readwrite("bonus", &HungarianTarokkBonusAnnouncement::bonus)
      .def_readwrite("side", &HungarianTarokkBonusAnnouncement::side)
      .def_readwrite("kontra_level",
                     &HungarianTarokkBonusAnnouncement::kontra_level);

  py::class_<HungarianTarokkCall>(ht, "HungarianTarokkCall")
      .def(py::init<>())
      .def_readwrite("player", &HungarianTarokkCall::player)
      .def_readwrite("action", &HungarianTarokkCall::action);

  py::class_<HungarianTarokkTrick>(ht, "HungarianTarokkTrick")
      .def(py::init<>())
      .def_readwrite("leader", &HungarianTarokkTrick::leader)
      .def_readwrite("cards", &HungarianTarokkTrick::cards)
      .def_readwrite("winner", &HungarianTarokkTrick::winner);

  auto obs_struct_class =
      bind_spiel_struct<HungarianTarokkObservationStruct, ObservationStruct>(
          ht, "HungarianTarokkObservationStruct");
  obs_struct_class
      .def_readwrite("phase", &HungarianTarokkObservationStruct::phase)
      .def_readwrite("current_player",
                     &HungarianTarokkObservationStruct::current_player)
      .def_readwrite("hand", &HungarianTarokkObservationStruct::hand)
      .def_readwrite("declarer", &HungarianTarokkObservationStruct::declarer)
      .def_readwrite("bid", &HungarianTarokkObservationStruct::bid)
      .def_readwrite("obligatory_call",
                     &HungarianTarokkObservationStruct::obligatory_call)
      .def_readwrite("bid_slots", &HungarianTarokkObservationStruct::bid_slots)
      .def_readwrite("bidding_history",
                     &HungarianTarokkObservationStruct::bidding_history)
      .def_readwrite("called_tarokk",
                     &HungarianTarokkObservationStruct::called_tarokk)
      .def_readwrite("sides", &HungarianTarokkObservationStruct::sides)
      .def_readwrite("declared_tarokks",
                     &HungarianTarokkObservationStruct::declared_tarokks)
      .def_readwrite("hivatalbol_kontra",
                     &HungarianTarokkObservationStruct::hivatalbol_kontra)
      .def_readwrite("bonus_announcements",
                     &HungarianTarokkObservationStruct::bonus_announcements)
      .def_readwrite("game_kontra",
                     &HungarianTarokkObservationStruct::game_kontra)
      .def_readwrite("announcement_history",
                     &HungarianTarokkObservationStruct::announcement_history)
      .def_readwrite("discard_tarokk_counts",
                     &HungarianTarokkObservationStruct::discard_tarokk_counts)
      .def_readwrite("declarer_shown_tarokks",
                     &HungarianTarokkObservationStruct::declarer_shown_tarokks)
      .def_readwrite("current_trick",
                     &HungarianTarokkObservationStruct::current_trick)
      .def_readwrite("current_trick_leader",
                     &HungarianTarokkObservationStruct::current_trick_leader)
      .def_readwrite("last_trick",
                     &HungarianTarokkObservationStruct::last_trick)
      .def_readwrite("trick_history",
                     &HungarianTarokkObservationStruct::trick_history)
      .def_readwrite("observing_player",
                     &HungarianTarokkObservationStruct::observing_player);

  // ---- State --------------------------------------------------------------
  py::classh<HungarianTarokkState, State>(ht, "HungarianTarokkState")
      .def("current_phase", &HungarianTarokkState::CurrentPhase)
      .def("player_cards", &HungarianTarokkState::PlayerCards, py::arg("player"))
      .def("talon", &HungarianTarokkState::Talon)
      .def("bidding", &HungarianTarokkState::Bidding,
           py::return_value_policy::reference_internal)
      .def("declarer", &HungarianTarokkState::Declarer)
      .def("winning_bid", &HungarianTarokkState::WinningBid)
      .def("is_annulled", &HungarianTarokkState::IsAnnulled)
      .def("announcements", &HungarianTarokkState::Announcements,
           py::return_value_policy::reference_internal)
      .def("partner", &HungarianTarokkState::Partner)
      // Pickle support
      .def(py::pickle(
          [](const HungarianTarokkState& state) {  // __getstate__
            return SerializeGameAndState(*state.GetGame(), state);
          },
          [](const std::string& data) {  // __setstate__
            std::pair<std::shared_ptr<const Game>, std::unique_ptr<State>>
                game_and_state = DeserializeGameAndState(data);
            return dynamic_cast<HungarianTarokkState*>(
                game_and_state.second.release());
          }));

  // ---- Game ---------------------------------------------------------------
  auto game_class =
      py::classh<HungarianTarokkGame, Game>(ht, "HungarianTarokkGame")
          // Pickle support
          .def(py::pickle(
              [](std::shared_ptr<const HungarianTarokkGame> game) {  // __get__
                return game->ToString();
              },
              [](const std::string& data) {  // __setstate__
                return std::dynamic_pointer_cast<HungarianTarokkGame>(
                    std::const_pointer_cast<Game>(LoadGame(data)));
              }));
  game_class.attr("ObservationStruct") = obs_struct_class;
}
