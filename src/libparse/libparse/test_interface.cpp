#include <libutl/utilities.hpp>
#include <libparse/test_interface.hpp>
#include <libparse/parser_internals.hpp>
#include <libformat/format.hpp>

namespace {
    template <libparse::parser auto parser, auto format>
    auto test_parse(std::string&& string, std::string_view const expectation) -> std::string
    {
        kieli::Compile_info    info;
        kieli::Source_id const source     = info.source_vector.push(std::move(string), "[test]");
        cst::Node_arena        node_arena = cst::Node_arena::with_page_size(64);
        try {
            libparse::Context context { node_arena, kieli::Lex_state::make(source, info) };
            auto const        result = libparse::require<parser>(context, expectation);
            if (context.is_finished()) {
                return format(*result, kieli::Format_configuration {});
            }
            context.error_expected(expectation);
        }
        catch (kieli::Compilation_failure const&) {
            return kieli::format_diagnostics(info.diagnostics, cppdiag::Colors::none());
        }
    }
} // namespace

auto libparse::test_parse_expression(std::string&& string) -> std::string
{
    return test_parse<parse_expression, kieli::format_expression>(
        std::move(string), "an expression");
}

auto libparse::test_parse_pattern(std::string&& string) -> std::string
{
    return test_parse<parse_pattern, kieli::format_pattern>(std::move(string), "a pattern");
}

auto libparse::test_parse_type(std::string&& string) -> std::string
{
    return test_parse<parse_type, kieli::format_type>(std::move(string), "a type");
}
