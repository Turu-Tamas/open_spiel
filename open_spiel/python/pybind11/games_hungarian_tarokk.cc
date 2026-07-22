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
using open_spiel::hungarian_tarokk::HungarianTarokkGame;
using open_spiel::hungarian_tarokk::HungarianTarokkObservationStruct;
using open_spiel::hungarian_tarokk::HungarianTarokkState;
using open_spiel::hungarian_tarokk::IsTarokk;
using open_spiel::hungarian_tarokk::Phase;
using open_spiel::hungarian_tarokk::Side;

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

  // ---- Observation struct (+ nested bonus announcement) -------------------
  py::class_<HungarianTarokkBonusAnnouncement>(
      ht, "HungarianTarokkBonusAnnouncement")
      .def(py::init<>())
      .def_readwrite("bonus", &HungarianTarokkBonusAnnouncement::bonus)
      .def_readwrite("side", &HungarianTarokkBonusAnnouncement::side)
      .def_readwrite("kontra_level",
                     &HungarianTarokkBonusAnnouncement::kontra_level);

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
