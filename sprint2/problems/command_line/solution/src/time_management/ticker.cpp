#include "ticker.h"
#include "error_report.h"

namespace time_m {

using namespace std::literals;

Ticker::Ticker(std::shared_ptr<Strand> strand, std::chrono::milliseconds period, Handler handler)
    : strand_{std::move(strand)}
    , timer_{*strand_}
    , period_{period}
    , handler_{std::move(handler)} {
}

void Ticker::Start() {
    last_tick_ = std::chrono::steady_clock::now();
    ScheduleTick();
}

void Ticker::ScheduleTick() {
    timer_.expires_after(period_);
    timer_.async_wait(net::bind_executor(*strand_,
        [self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        }));
}

void Ticker::OnTick(sys::error_code ec) {
    if (ec) {
        if (ec != net::error::operation_aborted) {
            error_report::ReportError(ec, "ticker"sv);
        }
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_);
    
    handler_(duration);
    
    last_tick_ = now;
    ScheduleTick();
}

}