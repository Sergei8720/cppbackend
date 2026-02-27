#pragma once
#include "map.h"
#include "tagged.h"
#include <memory>
#include <boost/asio/strand.hpp>

namespace app {

namespace net = boost::asio;

class IGameSession {
public:
    using SessionStrand = net::strand<net::io_context::executor_type>;
    using Id = util::Tagged<std::string, IGameSession>;
    
    virtual ~IGameSession() = default;
    
    virtual const Id& GetId() const noexcept = 0;
    virtual const std::shared_ptr<model::Map> GetMap() = 0;
    virtual std::shared_ptr<SessionStrand> GetStrand() = 0;
};

}