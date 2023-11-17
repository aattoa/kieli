#include <libutl/common/utilities.hpp>

auto utl::filename_without_path(std::string_view path) noexcept -> std::string_view
{
    auto const trim_if = [&](char const c) {
        if (auto const pos = path.find_last_of(c); pos != std::string_view::npos) {
            path.remove_prefix(pos + 1);
        }
    };
    trim_if('\\');
    trim_if('/');
    assert(!path.empty());
    return path;
}

utl::Safe_cast_argument_out_of_range::Safe_cast_argument_out_of_range()
    : invalid_argument { "utl::safe_cast argument out of target range" }
{}

utl::Exception::Exception(std::string&& message) noexcept : m_message { std::move(message) }
{
    disable_short_string_optimization(m_message);
}

auto utl::Exception::what() const noexcept -> char const*
{
    return m_message.c_str();
}

auto utl::abort(std::string_view const message, std::source_location const caller) -> void
{
    print(
        "[{}:{}:{}] {}, in function '{}'\n",
        filename_without_path(caller.file_name()),
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
    print(
        "utl::trace: Reached line {} in {}, in function '{}'\n",
        caller.line(),
        filename_without_path(caller.file_name()),
        caller.function_name());
}

auto utl::disable_short_string_optimization(std::string& string) -> void
{
    if (string.capacity() <= sizeof(std::string)) {
        string.reserve(sizeof(std::string) + 1);
    }
}

auto utl::string_with_capacity(Usize const capacity) -> std::string
{
    std::string string;
    string.reserve(capacity);
    return string;
}

auto utl::Relative_string::view_in(std::string_view const string) const -> std::string_view
{
    always_assert(string.size() >= (offset + length));
    return string.substr(offset, length);
}
