#pragma once

#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <utility>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io) 
        : io_{io}, strand_{net::make_strand(io)} {
    }

    void OrderHotDog(HotDogHandler handler) {
        auto order_context = std::make_shared<OrderContext>(
            std::move(handler), store_, ++hotdog_counter_);
        
        net::post(strand_, [this, order_context] {
            PrepareHotDog(order_context);
        });
    }

private:
    struct OrderContext {
        HotDogHandler handler;
        std::shared_ptr<Sausage> sausage;
        std::shared_ptr<Bread> bread;
        bool sausage_cooking = false;
        bool bread_baking = false;
        bool hotdog_assembled = false;
        int hotdog_id;
        std::shared_ptr<GasCooker> gas_cooker;

        OrderContext(HotDogHandler h, Store& store, int id)
            : handler(std::move(h)), hotdog_id(id) {
            sausage = store.GetSausage();
            bread = store.GetBread();
        }
    };

    void PrepareHotDog(std::shared_ptr<OrderContext> context) {
        context->gas_cooker = gas_cooker_;
        StartSausageCooking(context);
        StartBreadBaking(context);
    }

    void StartSausageCooking(std::shared_ptr<OrderContext> context) {
        context->sausage_cooking = true;
        context->sausage->StartFry(
            *context->gas_cooker,
            [this, context]() mutable {
                net::post(strand_, [this, context] {
                    OnSausageCookingStarted(context);
                });
            });
    }

    void StartBreadBaking(std::shared_ptr<OrderContext> context) {
        context->bread_baking = true;
        context->bread->StartBake(
            *context->gas_cooker,
            [this, context]() mutable {
                net::post(strand_, [this, context] {
                    OnBreadBakingStarted(context);
                });
            });
    }

    void OnSausageCookingStarted(std::shared_ptr<OrderContext> context) {
        StartSausageTimer(context);
    }

    void OnBreadBakingStarted(std::shared_ptr<OrderContext> context) {
        StartBreadTimer(context);
    }

    void StartSausageTimer(std::shared_ptr<OrderContext> context) {
        auto timer = std::make_shared<net::steady_timer>(io_, Milliseconds{1500});
        timer->async_wait([this, context, timer](const boost::system::error_code& ec) {
            if (!ec) {
                net::post(strand_, [this, context] {
                    FinishSausageCooking(context);
                });
            }
        });
    }

    void StartBreadTimer(std::shared_ptr<OrderContext> context) {
        auto timer = std::make_shared<net::steady_timer>(io_, Milliseconds{1000});
        timer->async_wait([this, context, timer](const boost::system::error_code& ec) {
            if (!ec) {
                net::post(strand_, [this, context] {
                    FinishBreadBaking(context);
                });
            }
        });
    }

    void FinishSausageCooking(std::shared_ptr<OrderContext> context) {
        context->sausage->StopFry();
        context->sausage_cooking = false;
        CheckHotDogReadiness(context);
    }

    void FinishBreadBaking(std::shared_ptr<OrderContext> context) {
        context->bread->StopBaking();
        context->bread_baking = false;
        CheckHotDogReadiness(context);
    }

    void CheckHotDogReadiness(std::shared_ptr<OrderContext> context) {
        if (!context->sausage_cooking && !context->bread_baking && !context->hotdog_assembled) {
            context->hotdog_assembled = true;
            AssembleHotDog(context);
        }
    }

    void AssembleHotDog(std::shared_ptr<OrderContext> context) {
        try {
            auto hot_dog = HotDog(context->hotdog_id, context->sausage, context->bread);
            net::post(io_, [handler = std::move(context->handler), 
                           hot_dog = std::move(hot_dog)]() mutable {
                handler(Result<HotDog>(std::move(hot_dog)));
            });
        } catch (...) {
            net::post(io_, [handler = std::move(context->handler)] {
                handler(Result<HotDog>::FromCurrentException());
            });
        }
    }

    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    int hotdog_counter_ = 0;
};