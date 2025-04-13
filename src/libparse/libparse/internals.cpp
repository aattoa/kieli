#include <libutl/utilities.hpp>
#include <libparse/internals.hpp>
#include <charconv>

using namespace ki;

namespace {
    template <typename T>
    auto parse_impl(std::string_view const string, std::same_as<int> auto const... base)
        -> std::optional<T>
    {
        char const* const begin = string.data();
        char const* const end   = begin + string.size();
        cpputil::always_assert(begin != end);

        T value {};
        auto const [ptr, ec] = std::from_chars(begin, end, value, base...);

        if (ptr != end) {
            return std::nullopt;
        }
        if (ec != std::errc {}) {
            assert("Not sure if other errors could occur" and ec == std::errc::result_out_of_range);
            return std::nullopt;
        }
        return value;
    }

    auto escape_character(char const ch) -> std::optional<char>
    {
        switch (ch) {
        case '0':  return '\0';
        case 'a':  return '\a';
        case 'b':  return '\b';
        case 'f':  return '\f';
        case 'n':  return '\n';
        case 'r':  return '\r';
        case 't':  return '\t';
        case 'v':  return '\v';
        case '\'': return '\'';
        case '\"': return '\"';
        case '\\': return '\\';
        default:   return std::nullopt;
        }
    }

    void escape_string_literal(par::Context& ctx, std::string& out, lex::Token token)
    {
        auto view = utl::View { .offset = token.view.offset + 1, .length = token.view.length - 2 };
        auto string = view.string(ctx.db.documents[ctx.doc_id].text);

        for (std::size_t i = 0; i != view.length; ++i) {
            if (char const ch = string.at(i); ch != '\\') {
                out.push_back(ch);
            }
            else if (auto const escaped = escape_character(string.at(++i))) {
                out.push_back(escaped.value());
            }
            else {
                auto range = lsp::to_range(lsp::column_offset(token.range.start, i + 1));
                db::add_error(ctx.db, ctx.doc_id, range, "Unrecognized escape sequence");
            }
        }
    }
} // namespace

auto ki::par::Failure::what() const noexcept -> char const*
{
    return "ki::par::Failure";
}

auto ki::par::is_finished(Context& ctx) -> bool
{
    return peek(ctx).type == lex::Type::End_of_input;
}

auto ki::par::peek(Context& ctx) -> lex::Token
{
    if (not ctx.next_token.has_value()) {
        ctx.next_token = lex::next(ctx.lex_state);
    }
    return ctx.next_token.value();
}

auto ki::par::extract(Context& ctx) -> lex::Token
{
    auto token = peek(ctx);
    ctx.next_token.reset();
    ctx.previous_token_end = token.range.stop;
    return token;
}

auto ki::par::try_extract(Context& ctx, lex::Type type) -> std::optional<lex::Token>
{
    return peek(ctx).type == type ? std::optional(extract(ctx)) : std::nullopt;
}

auto ki::par::require_extract(Context& ctx, lex::Type type) -> lex::Token
{
    if (auto token = try_extract(ctx, type)) {
        return token.value();
    }
    error_expected(ctx, token_description(type));
}

void ki::par::error_expected(Context& ctx, lsp::Range range, std::string_view desc)
{
    auto found = token_description(peek(ctx).type);
    db::add_error(ctx.db, ctx.doc_id, range, std::format("Expected {}, but found {}", desc, found));
    throw Failure {};
}

void ki::par::error_expected(Context& ctx, std::string_view description)
{
    error_expected(ctx, peek(ctx).range, description);
}

auto ki::par::up_to_current(Context& ctx, lsp::Range range) -> cst::Range_id
{
    cpputil::always_assert(ctx.previous_token_end.has_value());
    return ctx.arena.ranges.push(lsp::Range(range.start, ctx.previous_token_end.value()));
}

auto ki::par::token(Context& ctx, lex::Token const& token) -> cst::Range_id
{
    return ctx.arena.ranges.push(token.range);
}

void ki::par::add_semantic_token(Context& ctx, lsp::Range range, Semantic type)
{
    if (lsp::is_multiline(range)) {
        cpputil::always_assert(type == Semantic::String);
        return; // TODO
    }
    cpputil::always_assert(range.start.column < range.stop.column);
    ctx.semantic_tokens.push_back(lsp::Semantic_token {
        .position = range.start,
        .length   = range.stop.column - range.start.column,
        .type     = type,
    });
}

void ki::par::add_keyword(Context& ctx, lsp::Range range)
{
    add_semantic_token(ctx, range, Semantic::Keyword);
}

void ki::par::add_punctuation(Context& ctx, lsp::Range range)
{
    add_semantic_token(ctx, range, Semantic::Operator_name);
}

void ki::par::set_previous_path_head_semantic_type(Context& ctx, Semantic const type)
{
    ctx.semantic_tokens.at(ctx.previous_path_semantic_offset).type = type;
}

auto ki::par::parse_string(Context& ctx, lex::Token const& literal) -> std::optional<db::String>
{
    std::string buffer;
    add_semantic_token(ctx, literal.range, Semantic::String);
    escape_string_literal(ctx, buffer, literal);
    while (auto const token = try_extract(ctx, lex::Type::String)) {
        add_semantic_token(ctx, token.value().range, Semantic::String);
        escape_string_literal(ctx, buffer, token.value());
    }
    return db::String { .id = ctx.db.string_pool.make(std::move(buffer)) };
}

auto ki::par::parse_integer(Context& ctx, lex::Token const& literal) -> std::optional<db::Integer>
{
    add_semantic_token(ctx, literal.range, Semantic::Number);

    auto const string = literal.view.string(ctx.db.documents[ctx.doc_id].text);
    if (auto const integer = parse_impl<decltype(db::Integer::value)>(string)) {
        return db::Integer { integer.value() };
    }
    db::add_error(ctx.db, ctx.doc_id, literal.range, "Invalid integer literal");
    return std::nullopt;
}

auto ki::par::parse_floating(Context& ctx, lex::Token const& literal) -> std::optional<db::Floating>
{
    add_semantic_token(ctx, literal.range, Semantic::Number);

    // TODO: when from_chars for floating points is supported:

    // if (auto const floating = parse_impl<double>(string)) {
    //     return Floating { floating.value() };
    // }
    // db::add_error(ctx.db, ctx.doc_id, literal.range, "Invalid floating point literal");
    // return std::nullopt;

    try {
        auto const string = literal.view.string(ctx.db.documents[ctx.doc_id].text);
        return db::Floating { std::stod(std::string(string)) };
    }
    catch (std::out_of_range const&) {
        db::add_error(ctx.db, ctx.doc_id, literal.range, "Floating point literal is too large");
        return std::nullopt;
    }
    catch (std::invalid_argument const&) {
        db::add_error(ctx.db, ctx.doc_id, literal.range, "Invalid floating point literal");
        return std::nullopt;
    }
}

auto ki::par::parse_boolean(Context& ctx, lex::Token const& literal) -> std::optional<db::Boolean>
{
    add_keyword(ctx, literal.range);

    // The value of the boolean literal can be deduced from the token width.
    // This looks brittle but is perfectly fine.

    assert(literal.view.length == 4 or literal.view.length == 5);
    return db::Boolean { literal.view.length == 4 };
}

auto ki::par::context(db::Database& db, db::Document_id doc_id) -> Context
{
    return Context {
        .db                            = db,
        .arena                         = cst::Arena {},
        .doc_id                        = doc_id,
        .lex_state                     = lex::state(db.documents[doc_id].text),
        .next_token                    = std::nullopt,
        .previous_token_end            = std::nullopt,
        .semantic_tokens               = {},
        .previous_path_semantic_offset = {},
        .plus_id                       = db.string_pool.make("+"sv),
        .asterisk_id                   = db.string_pool.make("*"sv),
    };
}

auto ki::par::identifier(Context& ctx, lex::Token const& token) -> utl::String_id
{
    return ctx.db.string_pool.make(token.view.string(ctx.db.documents[ctx.doc_id].text));
}

auto ki::par::name(Context& ctx, lex::Token const& token) -> db::Name
{
    return db::Name { .id = identifier(ctx, token), .range = token.range };
}

auto ki::par::is_recovery_point(lex::Type type) -> bool
{
    return type == lex::Type::Fn      //
        or type == lex::Type::Struct  //
        or type == lex::Type::Enum    //
        or type == lex::Type::Concept //
        or type == lex::Type::Alias   //
        or type == lex::Type::Impl    //
        or type == lex::Type::Module  //
        or type == lex::Type::End_of_input;
}

void ki::par::skip_to_next_recovery_point(Context& ctx)
{
    while (not is_recovery_point(peek(ctx).type)) {
        (void)extract(ctx);
    }
}
