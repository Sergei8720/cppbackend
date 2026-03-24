#pragma once

#include "sdk.h"
#include <chrono>
#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

namespace time_m {

using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

template<typename Handler>
class Ticker : public std::enable_shared_from_this<Ticker<Handler>> {
public:
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler) 
        : period_(period), handler_(handler), strand_(strand) {
    }
    
    Ticker(boost::asio::io_context& ioc, std::chrono::milliseconds period, Handler handler)
        : period_(period), handler_(handler), timer_(ioc) {
    }
    
    void Start() {
        if (strand_) {
            strand_->execute([this] { ScheduleTick(); });
        } else {
            ScheduleTick();
        }
    }
    
    void SingleShot() {
        timer_.expires_after(period_);
        timer_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                handler_();
            }
        });
    }

private:
    void ScheduleTick() {
        last_tick_ = std::chrono::steady_clock::now();
        timer_.expires_after(period_);
        timer_.async_wait([this](boost::system::error_code ec) { OnTick(ec); });
    }
    
    void OnTick(boost::system::error_code ec) {
        if (ec) return;
        auto current_tick = std::chrono::steady_clock::now();
        handler_(std::chrono::duration_cast<std::chrono::milliseconds>(current_tick - last_tick_));
        last_tick_ = current_tick;
        ScheduleTick();
    }

    std::shared_ptr<Strand> strand_;
    boost::asio::steady_timer timer_{strand_ ? *strand_ : boost::asio::make_strand(timer_.get_executor())};
    std::chrono::milliseconds period_;
    Handler handler_;
    std::chrono::time_point<std::chrono::steady_clock> last_tick_;
};

} // namespace time_m