// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: fallible return type. Every fallible core function returns Result<T>.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace kentos::core {

/// Why something failed, in the small closed set a caller can actually branch on.
///
/// The CODE is for code — a retry, a fallback, a different message at a higher
/// layer — and the `message` beside it is for the person. Adding a code is an
/// amendment: every existing `switch` over this must still be exhaustive, and a
/// caller that silently fell into a default would stop distinguishing the case
/// that was just added.
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

/// One failure: a machine-readable code and a sentence for the user.
struct Error
{
    /// Defaults to `Internal` so a half-built Error reads as a bug rather than as
    /// a user mistake.
    ErrorCode code{ErrorCode::Internal};

    /// TURKISH, and actionable. This string reaches a surveyor in the transcript,
    /// so it names what was expected and what arrived: "Geçersiz nokta" is a
    /// defect, "2 sayı ya da bir nesne yakalama bekleniyordu, gelen: 'abc'" is
    /// correct (kentoscad.md §3, §13).
    std::string message;

    /// An `Internal` error with no message. Exists so `Error` can sit inside a
    /// variant and be default-constructed; a real failure always sets both.
    Error() = default;

    /// The form every failure site uses, through `err()` below.
    Error(ErrorCode c, std::string m) : code(c), message(std::move(m)) {}
};

/// Builds an `Error`. A free function because it reads as a verb at the call
/// site — `return err(ErrorCode::NotFound, "...")` — and that is where a reader
/// scanning for failure paths looks.
inline Error err(ErrorCode c, std::string m)
{
    return Error{c, std::move(m)};
}

/// A value or the reason there is none.
///
/// `[[nodiscard]]` is load-bearing: core does not throw, so an ignored `Result` is
/// an ignored failure and the compiler is the only thing that will notice. Article
/// 1.6 requires a validation failure to roll the whole transaction back, and it
/// cannot roll back a failure nobody read.
template<class T> class [[nodiscard]] Result
{
public:
    /// Success. Implicit on purpose: `return value;` inside a fallible function
    /// should not need ceremony, or the failure path stops being the noticeable
    /// one.
    Result(T value) : slot_(std::move(value)) {}

    /// Failure. Also implicit, so `return err(...)` works unchanged.
    Result(Error e) : slot_(std::move(e)) {}

    /// Whether this holds a value.
    bool ok() const noexcept { return slot_.index() == 0; }

    /// `if (auto r = f())`. Explicit so a `Result<bool>` cannot be mistaken for
    /// the bool it carries.
    explicit operator bool() const noexcept { return ok(); }

    /// The value. Undefined — a `std::variant` throw — when this holds an error,
    /// so it is called after `ok()` and not instead of it.
    T& value() { return std::get<0>(slot_); }

    /// The value, for a caller holding a const Result.
    const T& value() const { return std::get<0>(slot_); }

    /// The failure. Same contract as `value()` in reverse.
    const Error& error() const { return std::get<1>(slot_); }

    /// The value, or `fallback` when this failed. For the callers where a missing
    /// answer has a sensible substitute — never for one where it does not.
    T value_or(T fallback) const { return ok() ? std::get<0>(slot_) : std::move(fallback); }

private:
    std::variant<T, Error> slot_;
};

/// Succeeded, or failed for a reason. The `Status` alias below is the name this
/// is used under.
///
/// Specialised rather than `Result<std::monostate>` so the success case costs a
/// bool and no variant discriminator, and so `return ok();` reads as a sentence.
template<> class [[nodiscard]] Result<void>
{
public:
    /// Success. Default-constructible so `return {};` works.
    Result() = default;

    /// Failure, implicit so `return err(...)` works unchanged.
    Result(Error e) : error_(std::move(e)), failed_(true) {}

    /// Whether the operation succeeded.
    bool ok() const noexcept { return !failed_; }

    /// `if (auto st = f())`. Explicit for the same reason as above.
    explicit operator bool() const noexcept { return ok(); }

    /// The failure. Holds a default-constructed `Error` when this succeeded, so
    /// reading it out of order is meaningless rather than undefined.
    const Error& error() const { return error_; }

private:
    Error error_{};
    bool failed_{false};
};

/// The return type of every fallible function that produces no value. Named
/// separately because `Result<void>` reads as a contradiction at a call site.
using Status = Result<void>;

/// Success. A free function so the success path is as short to write as the
/// failure path is: `return ok();` against `return err(...)`.
inline Status ok()
{
    return Status{};
}

} // namespace kentos::core
