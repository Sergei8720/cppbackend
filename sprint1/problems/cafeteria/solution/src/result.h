#pragma once

#include <stdexcept>
#include <type_traits>
#include <variant>

template <typename ValueType>
class Result {
public:
    Result(const ValueType& value) noexcept(std::is_nothrow_copy_constructible_v<ValueType>)
        : state_{value} {
    }

    Result(ValueType&& value) noexcept(std::is_nothrow_move_constructible_v<ValueType>)
        : state_{std::move(value)} {
    }

    Result(std::nullptr_t) = delete;

    Result(std::exception_ptr error) : state_{error} {
        if (!error) {
            throw std::invalid_argument("Exception must not be null");
        }
    }

    static Result FromCurrentException() {
        return std::current_exception();
    }

    bool HasValue() const noexcept {
        return std::holds_alternative<ValueType>(state_);
    }

    std::exception_ptr GetError() const {
        return std::get<std::exception_ptr>(state_);
    }

    void ThrowIfHoldsError() const {
        if (auto e = std::get_if<std::exception_ptr>(&state_)) {
            std::rethrow_exception(*e);
        }
    }

    const ValueType& GetValue() const& {
        return std::get<ValueType>(state_);
    }

    ValueType&& GetValue() && {
        return std::get<ValueType>(std::move(state_));
    }

private:
    std::variant<ValueType, std::exception_ptr> state_;
};