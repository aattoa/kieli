#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>
#include <libformat/format.hpp>
#include "test_interface.hpp"

namespace {
    template <std::invocable<ki::parse::Context&> auto parser>
    auto test_parse(std::string&& text, std::string_view const expectation) -> std::string
    {
        auto db     = ki::Database {};
        auto doc_id = ki::test_document(db, std::move(text));
        auto arena  = ki::cst::Arena {};
        auto ctx    = ki::parse::context(db, arena, doc_id);

        auto const result = ki::parse::require<parser>(ctx, expectation);
        if (is_finished(ctx)) {
            return ki::format::to_string(db.string_pool, arena, ki::format::Options {}, result);
        }
        error_expected(ctx, expectation);
    }
} // namespace

auto ki::parse::test_parse_expression(std::string text) -> std::string
{
    return test_parse<parse_expression>(std::move(text), "an expression");
}

auto ki::parse::test_parse_pattern(std::string text) -> std::string
{
    return test_parse<parse_pattern>(std::move(text), "a pattern");
}

auto ki::parse::test_parse_type(std::string text) -> std::string
{
    return test_parse<parse_type>(std::move(text), "a type");
}
