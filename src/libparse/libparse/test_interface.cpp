#include <libutl/utilities.hpp>
#include <libparse/test_interface.hpp>
#include <libparse/parser_internals.hpp>
#include <libformat/format_internals.hpp> // TODO: fix this

namespace {
    template <libparse::parser auto parser>
    auto test_parse(std::string&& string, std::string_view const expectation) -> std::string
    {
        auto       db     = kieli::Database { .current_revision = 0 };
        auto const source = db.sources.push(std::move(string), "[test]");
        try {
            auto       arena   = kieli::cst::Arena {};
            auto       context = libparse::Context { arena, kieli::Lex_state::make(source, db) };
            auto const result  = libparse::require<parser>(context, expectation);
            if (context.is_finished()) {
                auto config = kieli::Format_configuration {};
                auto state  = libformat::State { .config = config, .arena = arena };
                libformat::format(state, result);
                return std::move(state.output);
            }
            context.error_expected(expectation);
        }
        catch (kieli::Compilation_failure const&) {
            return kieli::format_diagnostics(db.diagnostics, cppdiag::Colors::none());
        }
    }
} // namespace

auto libparse::test_parse_expression(std::string&& string) -> std::string
{
    return test_parse<parse_expression>(std::move(string), "an expression");
}

auto libparse::test_parse_pattern(std::string&& string) -> std::string
{
    return test_parse<parse_pattern>(std::move(string), "a pattern");
}

auto libparse::test_parse_type(std::string&& string) -> std::string
{
    return test_parse<parse_type>(std::move(string), "a type");
}
