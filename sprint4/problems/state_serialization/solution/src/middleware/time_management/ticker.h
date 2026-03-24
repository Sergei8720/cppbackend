#pragma once

#include "sdk.h"
#include <chrono>
#include <functional>
#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

namespace time_m {

using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

template<typename Handler>
class Ticker : public std::enable_shared_from_this<Ticker<Handler>> {
public:
    // Конструктор для использования со strand
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler) 
        : period_(period), handler_(handler), strand_(std::make_shared<Strand>(strand)), timer_(*this->strand_) {
    }
    
    // Конструктор без strand
    Ticker(boost::asio::io_context& ioc, std::chrono::milliseconds period, Handler handler)
        : period_(period), handler_(handler), strand_(nullptr), timer_(ioc) {
    }
    
    void Start() {
        if (strand_) {
            boost::asio::dispatch(*strand_, [self = this->shared_from_this()] {
                self->ScheduleTick();
            });
        } else {
            ScheduleTick();
        }
    }
    
    void SingleShot() {
        auto self = this->shared_from_this();
        timer_.expires_after(period_);
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) {
                self->handler_();
            }
        });
    }

private:
    void ScheduleTick() {
        last_tick_ = std::chrono::steady_clock::now();
        auto self = this->shared_from_this();
        timer_.expires_after(period_);
        timer_.async_wait([self](boost::system::error_code ec) {
            self->OnTick(ec);
        });
    }
    
    void OnTick(boost::system::error_code ec) {
        if (ec) return;
        auto current_tick = std::chrono::steady_clock::now();
        handler_(std::chrono::duration_cast<std::chrono::milliseconds>(current_tick - last_tick_));
        last_tick_ = current_tick;
        ScheduleTick();
    }

    std::shared_ptr<Strand> strand_;
    boost::asio::steady_timer timer_;
    std::chrono::milliseconds period_;
    Handler handler_;
    std::chrono::time_point<std::chrono::steady_clock> last_tick_;
};

} // namespace time_m