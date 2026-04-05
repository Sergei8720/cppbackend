#include "player_tokens.h"
#include "player.h"

#include <iomanip>
#include <sstream>

namespace authentication {

const size_t TOKEN_LENGTH = 32;

Token PlayerTokens::AddPlayer(std::shared_ptr<app::Player> player) {
    std::stringstream ss;
    // Генерируем 32-символьный hex токен
    for (size_t i = 0; i < TOKEN_LENGTH; ++i) {
        ss << std::hex << (generator1_() & 0xF);
    }
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

void PlayerTokens::RemoveToken(const Token& token) {
    auto it = tokenToPalyer_.find(token);
    if (it != tokenToPalyer_.end()) {
        BOOST_LOG_TRIVIAL(debug) << "PlayerTokens::RemoveToken: removing token=" << *token;
        tokenToPalyer_.erase(it);
    } else {
        BOOST_LOG_TRIVIAL(debug) << "PlayerTokens::RemoveToken: token=" << *token << " not found";
    }
};

}