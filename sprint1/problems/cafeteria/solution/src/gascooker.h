#pragma once

#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <cassert>
#include <deque>
#include <memory>

namespace net = boost::asio;
namespace sys = boost::system;

class GasCooker : public std::enable_shared_from_this<GasCooker> {
public:
    using Handler = std::function<void()>;

    GasCooker(net::io_context& io, int num_burners = 8)
        : io_{io}
        , number_of_burners_{num_burners} {
    }

    GasCooker(const GasCooker&) = delete;
    GasCooker& operator=(const GasCooker&) = delete;

    ~GasCooker() {
        assert(burners_in_use_ == 0);
    }

    void UseBurner(Handler handler) {
        net::dispatch(strand_,
                      [handler = std::move(handler), self = shared_from_this(), this]() mutable {
                          assert(strand_.running_in_this_thread());
                          assert(burners_in_use_ >= 0 && burners_in_use_ <= number_of_burners_);

                          if (burners_in_use_ < number_of_burners_) {
                              ++burners_in_use_;
                              net::post(io_, std::move(handler));
                          } else {
                              pending_handlers_.emplace_back(std::move(handler));
                          }

                          assert(burners_in_use_ > 0 && burners_in_use_ <= number_of_burners_);
                      });
    }

    void ReleaseBurner() {
        net::dispatch(strand_, [this, self = shared_from_this()] {
            assert(strand_.running_in_this_thread());
            assert(burners_in_use_ > 0 && burners_in_use_ <= number_of_burners_);

            if (!pending_handlers_.empty()) {
                net::post(io_, std::move(pending_handlers_.front()));
                pending_handlers_.pop_front();
            } else {
                --burners_in_use_;
            }
        });
    }

private:
    using Strand = net::strand<net::io_context::executor_type>;
    net::io_context& io_;
    Strand strand_{net::make_strand(io_)};
    int number_of_burners_;
    int burners_in_use_ = 0;
    std::deque<Handler> pending_handlers_;
};

class GasCookerLock {
public:
    GasCookerLock() = default;

    explicit GasCookerLock(std::shared_ptr<GasCooker> cooker) noexcept
        : cooker_{std::move(cooker)} {
    }

    GasCookerLock(GasCookerLock&& other) = default;
    GasCookerLock& operator=(GasCookerLock&& rhs) = default;

    GasCookerLock(const GasCookerLock&) = delete;
    GasCookerLock& operator=(const GasCookerLock&) = delete;

    ~GasCookerLock() {
        try {
            Unlock();
        } catch (...) {
        }
    }

    void Unlock() {
        if (cooker_) {
            cooker_->ReleaseBurner();
            cooker_.reset();
        }
    }

private:
    std::shared_ptr<GasCooker> cooker_;
};