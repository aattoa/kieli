#include <libformat/format.hpp>
#include <libparse/parser_internals.hpp>
#include <libparse/test_interface.hpp>
#include <libutl/common/utilities.hpp>

namespace {
    template <auto parse, auto format>
    auto test_parse(std::string&& string) -> libparse::Test_parse_result
    {
        auto [info, source] = kieli::test_info_and_source(std::move(string));
        libparse::Context context {
            kieli::lex({ info, source }),
            cst::Node_arena::with_page_size(64),
        };
        if (auto node = parse(context)) {
            if (context.is_finished()) {
                return format(**node, kieli::Format_configuration {});
            }
            return tl::unexpected(libparse::Test_parse_failure::unconsumed_input);
        }
        return tl::unexpected(libparse::Test_parse_failure::no_parse);
    }

    auto failure_string(libparse::Test_parse_failure const failure) -> std::string_view
    {
        switch (failure) {
        case libparse::Test_parse_failure::unconsumed_input:
            return "libparse::Test_parse_failure::unconsumed_input";
        case libparse::Test_parse_failure::no_parse:
            return "libparse::Test_parse_failure::no_parse";
        default:
            return "libparse::Test_parse_failure::{?}";
        }
    }
} // namespace

auto libparse::test_parse_expression(std::string&& string) -> Test_parse_result
{
    return test_parse<parse_expression, kieli::format_expression>(std::move(string));
}

auto libparse::test_parse_pattern(std::string&& string) -> Test_parse_result
{
    return test_parse<parse_pattern, kieli::format_pattern>(std::move(string));
}

auto libparse::test_parse_type(std::string&& string) -> Test_parse_result
{
    return test_parse<parse_type, kieli::format_type>(std::move(string));
}

auto libparse::operator<<(std::ostream& os, Test_parse_result const& result) -> std::ostream&
{
    if (result.has_value()) {
        return os << '"' << result.value() << '"';
    }
    return os << failure_string(result.error());
}
