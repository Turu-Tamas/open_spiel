#include "card.h"

namespace open_spiel {
namespace hungarian_tarok {
std::ostream &operator<<(std::ostream &os, const CardLocation &location) {
  switch (location) {
  case CardLocation::kPlayer0Hand:
    os << "Player0Hand";
    break;
  case CardLocation::kPlayer1Hand:
    os << "Player1Hand";
    break;
  case CardLocation::kPlayer2Hand:
    os << "Player2Hand";
    break;
  case CardLocation::kPlayer3Hand:
    os << "Player3Hand";
    break;
  case CardLocation::kPlayer0WonCards:
    os << "Player0WonCards";
    break;
  case CardLocation::kPlayer1WonCards:
    os << "Player1WonCards";
    break;
  case CardLocation::kPlayer2WonCards:
    os << "Player2WonCards";
    break;
  case CardLocation::kPlayer3WonCards:
    os << "Player3WonCards";
    break;
  case CardLocation::kTalon:
    os << "Talon";
    break;
  case CardLocation::kDeclarerSkart:
    os << "DeclarerSkart";
    break;
  case CardLocation::kOpponentsSkart:
    os << "OpponentsSkart";
    break;
  case CardLocation::kCurrentTrick:
    os << "CurrentTrick";
    break;
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const Suit &suit) {
  switch (suit) {
  case Suit::kHearts:
    os << "Hearts";
    break;
  case Suit::kDiamonds:
    os << "Diamonds";
    break;
  case Suit::kClubs:
    os << "Clubs";
    break;
  case Suit::kSpades:
    os << "Spades";
    break;
  case Suit::kTarok:
    os << "Tarok";
    break;
  }
  return os;
}
std::ostream &operator<<(std::ostream &os, const SuitRank &rank) {
  switch (rank) {
  case SuitRank::kAceTen:
    os << "Ace/Ten";
    break;
  case SuitRank::kJack:
    os << "Jack";
    break;
  case SuitRank::kRider:
    os << "Rider";
    break;
  case SuitRank::kQueen:
    os << "Queen";
    break;
  case SuitRank::kKing:
    os << "King";
    break;
  }
  return os;
}
bool CardBeats(Card a, Card b) {
  SPIEL_CHECK_LT(a, kDeckSize);
  SPIEL_CHECK_LT(b, kDeckSize);

  Suit suit_a = CardSuit(a);
  Suit suit_b = CardSuit(b);
  if (suit_a == Suit::kTarok && suit_b != Suit::kTarok) {
    return true;
  } else if (suit_a != Suit::kTarok && suit_b == Suit::kTarok) {
    return false;
  } else if (suit_a == Suit::kTarok && suit_b == Suit::kTarok) {
    return a > b; // higher tarok wins
  } else {
    // both are suit cards
    if (suit_a != suit_b) {
      return false; // different suits, first does not win
    } else {
      return a > b; // higher rank wins
    }
  }
}

std::string ToRomanNumeral(int number) {
  std::vector<std::pair<int, std::string>> roman_numerals{
      {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}};
  std::string result;
  int sum = 0;
  while (sum < number) {
    for (const auto &[value, symbol] : roman_numerals) {
      if (sum + value <= number) {
        result += symbol;
        sum += value;
        break;
      }
    }
  }
  return result;
}

std::string CardToString(Card card) {
  SPIEL_CHECK_GE(card, 0);
  SPIEL_CHECK_LT(card, kDeckSize);
  if (card < kNumTaroks) {
    if (card == kSkiz) {
      return "Skiz";
    }
    return ToRomanNumeral(card + 1);
  } else {
    Suit suit = CardSuit(card);
    SuitRank rank =
        static_cast<SuitRank>((card - kNumTaroks) % kNumRanksPerSuit);
    std::string suit_str;
    switch (suit) {
    case Suit::kHearts:
      suit_str = "H";
      break;
    case Suit::kDiamonds:
      suit_str = "D";
      break;
    case Suit::kClubs:
      suit_str = "C";
      break;
    case Suit::kSpades:
      suit_str = "S";
      break;
    }
    std::string rank_str;
    switch (rank) {
    case SuitRank::kAceTen:
      rank_str = "A";
      break;
    case SuitRank::kJack:
      rank_str = "J";
      break;
    case SuitRank::kRider:
      rank_str = "R";
      break;
    case SuitRank::kQueen:
      rank_str = "Q";
      break;
    case SuitRank::kKing:
      rank_str = "K";
      break;
    }
    return absl::StrCat(rank_str, "/", suit_str);
  }
}
std::string DeckToString(const Deck &deck) {
  std::stringstream ss;
  for (Player p = 0; p < kNumPlayers; ++p) {
    int count = 0;
    ss << "Player " << p << " hand: ";
    for (Card card = 0; card < kDeckSize; ++card) {
      if (deck[card] == PlayerHandLocation(p)) {
        count++;
        ss << CardToString(card) << "; ";
      }
    }
    ss << " (total " << count << ")";
    ss << std::endl;
  }
  return ss.str();
}
} // namespace hungarian_tarok
} // namespace open_spiel