#include <libutl/utilities.hpp>
#include <libparse/parse.hpp>
#include <libparse/internals.hpp>

#include <charconv>

using namespace ki;
using namespace ki::par;

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

    void escape_string_literal(Context& ctx, std::string& out, lex::Token token)
    {
        auto view = utl::View { .offset = token.view.offset + 1, .length = token.view.length - 2 };
        auto string = view.string(ctx.lex_state.text);

        for (std::size_t i = 0; i != view.length; ++i) {
            if (char const ch = string.at(i); ch != '\\') {
                out.push_back(ch);
            }
            else if (auto const escaped = escape_character(string.at(++i))) {
                out.push_back(escaped.value());
            }
            else {
                auto const range = lsp::to_range(lsp::column_offset(token.range.start, i + 1));
                db::add_error(ctx.db, ctx.doc_id, range, "Unrecognized escape sequence");
            }
        }
    }

    template <typename Default, std::invocable<Context&> auto parse_argument>
    auto parse_default_argument(Context& ctx) -> std::optional<Default>
    {
        return try_extract(ctx, lex::Type::Equals).transform([&](lex::Token const& equals) {
            add_semantic_token(ctx, equals.range, Semantic::Operator_name);
            if (auto const underscore = try_extract(ctx, lex::Type::Underscore)) {
                return Default {
                    .variant           = cst::Wildcard { underscore.value().range },
                    .equals_sign_token = equals.range,
                };
            }
            return Default {
                .variant           = require<parse_argument>(ctx, "a default argument"),
                .equals_sign_token = equals.range,
            };
        });
    }

    constexpr auto parse_type_parameter_default_argument
        = parse_default_argument<cst::Type_parameter_default_argument, parse_type>;
    constexpr auto parse_value_parameter_default_argument
        = parse_default_argument<cst::Value_parameter_default_argument, parse_expression>;
    constexpr auto parse_mutability_parameter_default_argument
        = parse_default_argument<cst::Mutability_parameter_default_argument, parse_mutability>;

    auto extract_template_value_or_mutability_parameter(Context& ctx, db::Lower const name)
        -> cst::Template_parameter_variant
    {
        add_semantic_token(ctx, name.range, Semantic::Parameter);
        auto const colon = require_extract(ctx, lex::Type::Colon);
        add_punctuation(ctx, colon.range);
        if (auto const mut_keyword = try_extract(ctx, lex::Type::Mut)) {
            add_keyword(ctx, mut_keyword.value().range);
            return cst::Template_mutability_parameter {
                .name             = name,
                .colon_token      = colon.range,
                .mut_token        = mut_keyword.value().range,
                .default_argument = parse_mutability_parameter_default_argument(ctx),
            };
        }
        if (auto const type = parse_type(ctx)) {
            return cst::Template_value_parameter {
                .name = name,
                .type_annotation { cst::Type_annotation {
                    .type        = type.value(),
                    .colon_token = colon.range,
                } },
                .default_argument = parse_value_parameter_default_argument(ctx),
            };
        }
        error_expected(ctx, "'mut' or a type");
    }

    auto extract_template_type_parameter(Context& ctx, db::Upper const name)
        -> cst::Template_parameter_variant
    {
        add_semantic_token(ctx, name.range, Semantic::Type_parameter);
        if (auto const colon = try_extract(ctx, lex::Type::Colon)) {
            add_punctuation(ctx, colon.value().range);
            return cst::Template_type_parameter {
                .name             = name,
                .colon_token      = colon.value().range,
                .concepts         = extract_concept_references(ctx),
                .default_argument = parse_type_parameter_default_argument(ctx),
            };
        }
        return cst::Template_type_parameter {
            .name             = name,
            .colon_token      = {},
            .concepts         = {},
            .default_argument = {},
        };
    }

    auto dispatch_parse_template_parameter(Context& ctx)
        -> std::optional<cst::Template_parameter_variant>
    {
        auto const extract_lower
            = std::bind_front(extract_template_value_or_mutability_parameter, std::ref(ctx));
        auto const extract_upper = std::bind_front(extract_template_type_parameter, std::ref(ctx));
        return parse_lower_name(ctx).transform(extract_lower).or_else([&] {
            return parse_upper_name(ctx).transform(extract_upper);
        });
    }

    auto parse_concept_path(Context& ctx) -> std::optional<cst::Path>
    {
        auto path = parse_simple_path(ctx);
        if (path.has_value()) {
            set_previous_path_head_semantic_type(ctx, Semantic::Interface);
        }
        return path;
    }

    auto root_range(cst::Arena const& arena, cst::Path_root const& root)
        -> std::optional<lsp::Range>
    {
        if (auto const* const global = std::get_if<cst::Path_root_global>(&root)) {
            return global->double_colon_token;
        }
        if (auto const* const type_id = std::get_if<cst::Type_id>(&root)) {
            return arena.types[*type_id].range;
        }
        return std::nullopt;
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

void ki::par::error_expected(Context& ctx, lsp::Range range, std::string_view description)
{
    auto found   = token_description(peek(ctx).type);
    auto message = std::format("Expected {}, but found {}", description, found);
    db::add_error(ctx.db, ctx.doc_id, range, std::move(message));
    throw Failure {};
}

void ki::par::error_expected(Context& ctx, std::string_view description)
{
    error_expected(ctx, peek(ctx).range, description);
}

auto ki::par::up_to_current(Context& ctx, lsp::Range range) -> lsp::Range
{
    cpputil::always_assert(ctx.previous_token_end.has_value());
    return lsp::Range(range.start, ctx.previous_token_end.value());
}

void ki::par::add_semantic_token(Context& ctx, lsp::Range range, Semantic type)
{
    if (ctx.db.config.semantic_tokens == db::Semantic_token_mode::Full) {
        cpputil::always_assert(not lsp::is_multiline(range));
        cpputil::always_assert(range.start.column < range.stop.column);
        ctx.semantic_tokens.push_back(
            lsp::Semantic_token {
                .position = range.start,
                .length   = range.stop.column - range.start.column,
                .type     = type,
            });
    }
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
    if (ctx.db.config.semantic_tokens == db::Semantic_token_mode::Full) {
        ctx.semantic_tokens.at(ctx.previous_path_semantic_offset).type = type;
    }
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

    auto const string = literal.view.string(ctx.lex_state.text);
    if (auto const integer = parse_impl<decltype(db::Integer::value)>(string)) {
        return db::Integer { integer.value() };
    }
    db::add_error(ctx.db, ctx.doc_id, literal.range, "Invalid integer literal");
    return std::nullopt;
}

auto ki::par::parse_floating(Context& ctx, lex::Token const& literal) -> std::optional<db::Floating>
{
    add_semantic_token(ctx, literal.range, Semantic::Number);

    auto const string = literal.view.string(ctx.lex_state.text);
    if (auto const floating = parse_impl<double>(string)) {
        return db::Floating { floating.value() };
    }
    db::add_error(ctx.db, ctx.doc_id, literal.range, "Invalid floating point literal");
    return std::nullopt;
}

auto ki::par::parse_boolean(Context& ctx, lex::Token const& literal) -> std::optional<db::Boolean>
{
    add_semantic_token(ctx, literal.range, Semantic::Number);

    // The value of the boolean literal can be deduced from the token width.
    // This looks brittle but is perfectly fine.

    assert(literal.view.length == 4 or literal.view.length == 5);
    return db::Boolean { literal.view.length == 4 };
}

auto ki::par::context(db::Database& db, db::Document_id doc_id) -> Context
{
    return Context {
        .db                            = db,
        .doc_id                        = doc_id,
        .arena                         = cst::Arena {},
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
    return ctx.db.string_pool.make(token.view.string(ctx.lex_state.text));
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
        auto token = extract(ctx);
        if (auto semantic = lex::recovery_semantic_token(token.type)) {
            add_semantic_token(ctx, token.range, semantic.value());
        }
    }
}

auto ki::par::parse_simple_path_root(Context& ctx) -> std::optional<cst::Path_root>
{
    switch (auto token = peek(ctx); token.type) {
    case lex::Type::Lower_name:
    case lex::Type::Upper_name:   return cst::Path_root {};
    case lex::Type::Double_colon: return cst::Path_root_global { token.range };
    default:                      return std::nullopt;
    }
}

auto ki::par::parse_simple_path(Context& ctx) -> std::optional<cst::Path>
{
    return parse_simple_path_root(ctx).transform(
        [&](cst::Path_root const& root) { return extract_path(ctx, root); });
}

auto ki::par::parse_complex_path(Context& ctx) -> std::optional<cst::Path>
{
    return parse_simple_path_root(ctx)
        .or_else([&] { return std::optional<cst::Path_root>(parse_type_root(ctx)); })
        .transform([&](cst::Path_root const root) { return extract_path(ctx, root); });
}

auto ki::par::parse_mutability(Context& ctx) -> std::optional<cst::Mutability>
{
    if (auto mut_keyword = try_extract(ctx, lex::Type::Mut)) {
        add_keyword(ctx, mut_keyword.value().range);
        if (auto question_mark = try_extract(ctx, lex::Type::Question)) {
            add_semantic_token(ctx, question_mark.value().range, Semantic::Operator_name);
            return cst::Mutability {
                .variant = cst::Parameterized_mutability {
                    .name = extract_lower_name(ctx, "a mutability parameter name"),
                    .question_mark_token = question_mark.value().range,
                },
                .range         = up_to_current(ctx, mut_keyword.value().range),
                .keyword_token = mut_keyword.value().range,
            };
        }
        return cst::Mutability {
            .variant       = db::Mutability::Mut,
            .range         = mut_keyword.value().range,
            .keyword_token = mut_keyword.value().range,
        };
    }
    if (auto immut_keyword = try_extract(ctx, lex::Type::Immut)) {
        add_keyword(ctx, immut_keyword.value().range);
        auto const range = immut_keyword.value().range;
        return cst::Mutability {
            .variant       = db::Mutability::Immut,
            .range         = range,
            .keyword_token = range,
        };
    }
    return std::nullopt;
}

auto ki::par::parse_type_annotation(Context& ctx) -> std::optional<cst::Type_annotation>
{
    return try_extract(ctx, lex::Type::Colon).transform([&](lex::Token const& colon) {
        add_punctuation(ctx, colon.range);
        return cst::Type_annotation {
            .type        = require<parse_type>(ctx, "a type"),
            .colon_token = colon.range,
        };
    });
}

auto ki::par::parse_template_parameters(Context& ctx) -> std::optional<cst::Template_parameters>
{
    return parse_bracketed<
        parse_comma_separated_one_or_more<parse_template_parameter, "a template parameter">,
        "a bracketed list of template parameters">(ctx);
}

auto ki::par::parse_template_parameter(Context& ctx) -> std::optional<cst::Template_parameter>
{
    auto const anchor_range = peek(ctx).range;
    return dispatch_parse_template_parameter(ctx).transform(
        [&](cst::Template_parameter_variant&& variant) {
            return cst::Template_parameter {
                .variant = std::move(variant),
                .range   = up_to_current(ctx, anchor_range),
            };
        });
}

auto ki::par::parse_template_argument(Context& ctx) -> std::optional<cst::Template_argument>
{
    if (auto const underscore = try_extract(ctx, lex::Type::Underscore)) {
        add_semantic_token(ctx, underscore.value().range, Semantic::Variable);
        return cst::Template_argument { cst::Wildcard { underscore.value().range } };
    }
    if (auto const type = parse_type(ctx)) {
        return cst::Template_argument { type.value() };
    }
    if (auto const expression = parse_expression(ctx)) {
        return cst::Template_argument { expression.value() };
    }
    if (auto const immut_keyword = try_extract(ctx, lex::Type::Immut)) {
        add_keyword(ctx, immut_keyword.value().range);
        auto const range = immut_keyword.value().range;
        return cst::Template_argument { cst::Mutability {
            .variant       = db::Mutability::Immut,
            .range         = range,
            .keyword_token = range,
        } };
    }
    return parse_mutability(ctx).transform(utl::make<cst::Template_argument>);
}

auto ki::par::parse_template_arguments(Context& ctx) -> std::optional<cst::Template_arguments>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_template_argument, "a template argument">;
    return parse_bracketed<pretend_parse<extract>, "">(ctx);
}

auto ki::par::parse_function_parameters(Context& ctx) -> std::optional<cst::Function_parameters>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_function_parameter, "a function parameter">;
    return parse_parenthesized<pretend_parse<extract>, "">(ctx);
}

auto ki::par::parse_function_parameter(Context& ctx) -> std::optional<cst::Function_parameter>
{
    return parse_pattern(ctx).transform([&](cst::Pattern_id const pattern) {
        return cst::Function_parameter {
            .pattern          = pattern,
            .type             = parse_type_annotation(ctx),
            .default_argument = parse_value_parameter_default_argument(ctx),
        };
    });
}

auto ki::par::parse_function_arguments(Context& ctx) -> std::optional<cst::Function_arguments>
{
    static constexpr auto extract
        = extract_comma_separated_zero_or_more<parse_expression, "a function argument">;
    return parse_parenthesized<pretend_parse<extract>, "">(ctx);
}

auto ki::par::extract_path(Context& ctx, cst::Path_root const root) -> cst::Path
{
    auto anchor_range               = peek(ctx).range;
    auto segments                   = std::vector<cst::Path_segment> {};
    auto head_semantic_token_offset = 0UZ;

    auto extract_segment = [&](std::optional<lsp::Range> double_colon_token_id) {
        auto const token           = extract(ctx);
        head_semantic_token_offset = ctx.semantic_tokens.size();
        if (token.type == lex::Type::Upper_name) {
            add_semantic_token(ctx, token.range, Semantic::Type);
        }
        else if (token.type == lex::Type::Lower_name) {
            add_semantic_token(ctx, token.range, Semantic::Variable);
        }
        else {
            error_expected(ctx, token.range, "an identifier");
        }
        segments.push_back(
            cst::Path_segment {
                .template_arguments         = parse_template_arguments(ctx),
                .name                       = name(ctx, token),
                .leading_double_colon_token = double_colon_token_id,
            });
    };

    if (std::holds_alternative<std::monostate>(root)) {
        extract_segment(std::nullopt);
    }
    while (auto const double_colon = try_extract(ctx, lex::Type::Double_colon)) {
        add_semantic_token(ctx, double_colon.value().range, Semantic::Operator_name);
        extract_segment(double_colon.value().range);
    }
    if (segments.empty()) {
        error_expected(ctx, "at least one path segment");
    }

    ctx.previous_path_semantic_offset = head_semantic_token_offset;
    if (peek(ctx).type == lex::Type::Paren_open) {
        set_previous_path_head_semantic_type(ctx, Semantic::Function);
    }

    return cst::Path {
        .root     = root,
        .segments = std::move(segments),
        .range    = up_to_current(ctx, root_range(ctx.arena, root).value_or(anchor_range)),
    };
}

auto ki::par::extract_concept_references(Context& ctx) -> cst::Separated<cst::Path>
{
    return require<
        parse_separated_one_or_more<parse_concept_path, "a concept path", lex::Type::Plus>>(
        ctx, "one or more concept paths separated by '+'");
}
