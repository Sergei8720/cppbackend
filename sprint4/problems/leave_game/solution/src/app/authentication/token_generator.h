#pragma once
#include "tagged.h"
#include <random>
#include <memory>
#include <string>

namespace authentication {

struct TokenTag {};

using Token = util::Tagged<std::string, TokenTag>;
using TokenHasher = util::TaggedHasher<Token>;

class TokenGenerator {
public:
    TokenGenerator() = default;
    TokenGenerator(const TokenGenerator& other) = default;
    TokenGenerator(TokenGenerator&& other) = default;
    TokenGenerator& operator = (const TokenGenerator& other) = default;
    TokenGenerator& operator = (TokenGenerator&& other) = default;
    virtual ~TokenGenerator() = default;

    Token GenerateToken();
    
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};

}