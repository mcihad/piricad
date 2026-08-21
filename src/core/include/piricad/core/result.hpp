// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: fallible return type. Every fallible core function returns Result<T>.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace piricad::core {

enum class ErrorCode : std::uint16_t {
    None = 0,
    InvalidArgument,
    NotFound,
    ParseError,
    ValidationFailed,
    Unsupported,
    IoFailure,
    Cancelled,
    Internal,
};

struct Error
{
    ErrorCode code{ErrorCode::Internal};
    std::string message;

    /// Actionable messages only. "Invalid point" is a defect;
    /// "Expected 2 numbers or an object snap, got 'abc'" is correct. (piricad.md §3)
    Error() = default;

    Error(ErrorCode c, std::string m) : code(c), message(std::move(m)) {}
};

inline Error err(ErrorCode c, std::string m)
{
    return Error{c, std::move(m)};
}

template<class T> class [[nodiscard]] Result
{
public:
    Result(T value) : slot_(std::move(value)) {}

    Result(Error e) : slot_(std::move(e)) {}

    bool ok() const noexcept { return slot_.index() == 0; }

    explicit operator bool() const noexcept { return ok(); }

    T& value() { return std::get<0>(slot_); }

    const T& value() const { return std::get<0>(slot_); }

    const Error& error() const { return std::get<1>(slot_); }

    T value_or(T fallback) const { return ok() ? std::get<0>(slot_) : std::move(fallback); }

private:
    std::variant<T, Error> slot_;
};

template<> class [[nodiscard]] Result<void>
{
public:
    Result() = default;

    Result(Error e) : error_(std::move(e)), failed_(true) {}

    bool ok() const noexcept { return !failed_; }

    explicit operator bool() const noexcept { return ok(); }

    const Error& error() const { return error_; }

private:
    Error error_{};
    bool failed_{false};
};

using Status = Result<void>;

inline Status ok()
{
    return Status{};
}

} // namespace piricad::core
