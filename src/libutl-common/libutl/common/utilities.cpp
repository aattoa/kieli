#include <libutl/common/utilities.hpp>

utl::Safe_cast_argument_out_of_range::Safe_cast_argument_out_of_range()
    : invalid_argument { "utl::safe_cast argument out of target range" }
{}

auto utl::abort(std::string_view const message, std::source_location const caller) -> void
{
    std::print(
        stderr,
        "[{}:{}:{}] {}, in function '{}'\n",
        std::filesystem::path(caller.file_name()).filename().c_str(),
        caller.line(),
        caller.column(),
        message,
        caller.function_name());
    std::quick_exit(EXIT_FAILURE);
}

auto utl::todo(std::source_location const caller) -> void
{
    abort("Unimplemented branch reached", caller);
}

auto utl::unreachable(std::source_location const caller) -> void
{
    abort("Unreachable branch reached", caller);
}

auto utl::always_assert(bool const condition, std::source_location const caller) -> void
{
    if (!condition) [[unlikely]] {
        abort("Assertion failed", caller);
    }
}

auto utl::trace(std::source_location const caller) -> void
{
    std::print(
        stderr,
        "utl::trace: Reached line {} in {}, in function '{}'\n",
        caller.line(),
        std::filesystem::path(caller.file_name()).filename().c_str(),
        caller.function_name());
}

auto utl::disable_short_string_optimization(std::string& string) -> void
{
    if (string.capacity() <= sizeof(std::string)) {
        string.reserve(sizeof(std::string) + 1);
    }
}

auto utl::Relative_string::view_in(std::string_view const string) const -> std::string_view
{
    always_assert(string.size() >= (offset + length));
    return string.substr(offset, length);
}
