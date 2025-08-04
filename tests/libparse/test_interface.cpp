#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>
#include <libformat/format.hpp>
#include "test_interface.hpp"

using namespace ki;
using namespace ki::par;

namespace {
    template <std::invocable<Context&> auto parser>
    auto test_parse(std::string&& text, std::string_view const expectation) -> std::string
    {
        auto db      = db::Database {};
        auto doc_id  = db::test_document(db, std::move(text));
        auto par_ctx = context(db, doc_id, db::ignore_sink);
        auto result  = require<parser>(par_ctx, expectation);

        if (is_finished(par_ctx)) {
            return fmt::to_string(db, par_ctx.arena, result);
        }
        error_expected(par_ctx, expectation);
    }
} // namespace

auto ki::par::test_parse_expression(std::string text) -> std::string
{
    return test_parse<parse_expression>(std::move(text), "an expression");
}

auto ki::par::test_parse_pattern(std::string text) -> std::string
{
    return test_parse<parse_pattern>(std::move(text), "a pattern");
}

auto ki::par::test_parse_type(std::string text) -> std::string
{
    return test_parse<parse_type>(std::move(text), "a type");
}
