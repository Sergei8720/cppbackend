#include "player_tokens.h"

#include <iomanip>
#include <sstream>

namespace authentication {

const size_t NUMBER_OF_DIGITS_IN_HALF_TOKEN = 16;

Token PlayerTokens::AddPlayer(std::shared_ptr<app::Player> player) {
    std::stringstream ss;
    ss << std::setw(NUMBER_OF_DIGITS_IN_HALF_TOKEN) << std::setfill('0') << std::hex << generator1_();
    ss << std::setw(NUMBER_OF_DIGITS_IN_HALF_TOKEN) << std::setfill('0') << std::hex << generator2_();
    Token token{ss.str()};
    tokenToPalyer_[token] = player;
    BOOST_LOG_TRIVIAL(debug) << "PlayerTokens::AddPlayer: token=" << *token 
                             << " for player id=" << *player->GetId();
    return token;
};

void PlayerTokens::AddTokenPlayerPair(Token token, std::shared_ptr<app::Player> player) {
    BOOST_LOG_TRIVIAL(debug) << "PlayerTokens::AddTokenPlayerPair: token=" << *token 
                             << " for player id=" << *player->GetId();
    tokenToPalyer_[token] = player;
};

std::shared_ptr<app::Player> PlayerTokens::FindPlayerBy(Token token) {
    auto it = tokenToPalyer_.find(token);
    if (it == tokenToPalyer_.end()) {
        BOOST_LOG_TRIVIAL(debug) << "PlayerTokens::FindPlayerBy: token=" << *token << " NOT found";
        return std::shared_ptr<app::Player>();
    }
    BOOST_LOG_TRIVIAL(debug) << "PlayerTokens::FindPlayerBy: token=" << *token 
                             << " found player id=" << *it->second->GetId();
    return it->second;
};

}