#include <libutl/utilities.hpp>
#include <libutl/safe_integer.hpp>
#include <liblex/numeric.hpp>
#include <liblex/lex.hpp>
#include <liblex/state.hpp>

using kieli::Token;
using kieli::Token_type;
using kieli::Token_variant;

namespace {

    constexpr auto punctuation_token_map = std::to_array<std::pair<std::string_view, Token_type>>({
        { ".", Token_type::dot },
        { ":", Token_type::colon },
        { "::", Token_type::double_colon },
        { "|", Token_type::pipe },
        { "=", Token_type::equals },
        { "&", Token_type::ampersand },
        { "*", Token_type::asterisk },
        { "+", Token_type::plus },
        { "?", Token_type::question },
        { "\\", Token_type::lambda },
        { "<-", Token_type::left_arrow },
        { "->", Token_type::right_arrow },
        { R"(???)", Token_type::hole },
    });

    constexpr auto keyword_token_map = std::to_array<std::pair<std::string_view, Token_type>>({
        { "let", Token_type::let },
        { "mut", Token_type::mut },
        { "if", Token_type::if_ },
        { "else", Token_type::else_ },
        { "elif", Token_type::elif },
        { "for", Token_type::for_ },
        { "in", Token_type::in },
        { "while", Token_type::while_ },
        { "loop", Token_type::loop },
        { "continue", Token_type::continue_ },
        { "break", Token_type::break_ },
        { "match", Token_type::match },
        { "ret", Token_type::ret },
        { "discard", Token_type::discard },
        { "fn", Token_type::fn },
        { "as", Token_type::as },
        { "I8", Token_type::i8_type },
        { "I16", Token_type::i16_type },
        { "I32", Token_type::i32_type },
        { "I64", Token_type::i64_type },
        { "U8", Token_type::u8_type },
        { "U16", Token_type::u16_type },
        { "U32", Token_type::u32_type },
        { "U64", Token_type::u64_type },
        { "Float", Token_type::floating_type },
        { "Char", Token_type::character_type },
        { "Bool", Token_type::boolean_type },
        { "String", Token_type::string_type },
        { "self", Token_type::lower_self },
        { "Self", Token_type::upper_self },
        { "enum", Token_type::enum_ },
        { "struct", Token_type::struct_ },
        { "concept", Token_type::concept_ },
        { "impl", Token_type::impl },
        { "alias", Token_type::alias },
        { "import", Token_type::import_ },
        { "export", Token_type::export_ },
        { "module", Token_type::module_ },
        { "sizeof", Token_type::sizeof_ },
        { "typeof", Token_type::typeof_ },
        { "unsafe", Token_type::unsafe },
        { "mov", Token_type::mov },
        { "meta", Token_type::meta },
        { "where", Token_type::where },
        { "immut", Token_type::immut },
        { "dyn", Token_type::dyn },
        { "macro", Token_type::macro },
        { "global", Token_type::global },
        { "defer", Token_type::defer },
    });

    auto find_token(auto const& map, std::string_view const string) -> std::optional<Token_type>
    {
        auto const it = std::ranges::find(map, string, utl::first);
        return it != map.end() ? std::optional(it->second) : std::nullopt;
    }

    template <utl::Metastring string>
    constexpr auto is_one_of(char const c) noexcept -> bool
    {
        return string.view().contains(c);
    }

    template <char a, char b>
    constexpr auto is_in_range(char const c) noexcept -> bool
        requires(a < b)
    {
        return a <= c && c <= b;
    }

    template <std::predicate<char> auto... predicates>
    constexpr auto satisfies_one_of(char const c) noexcept -> bool
    {
        return (predicates(c) || ...);
    }

    constexpr auto is_space = is_one_of<" \t\n">;
    constexpr auto is_lower = is_in_range<'a', 'z'>;
    constexpr auto is_upper = is_in_range<'A', 'Z'>;

    constexpr auto is_digit2  = is_in_range<'0', '1'>;
    constexpr auto is_digit4  = is_in_range<'0', '3'>;
    constexpr auto is_digit8  = is_in_range<'0', '7'>;
    constexpr auto is_digit10 = is_in_range<'0', '9'>;
    constexpr auto is_digit12 = satisfies_one_of<is_digit10, is_one_of<"abAB">>;
    constexpr auto is_digit16 = satisfies_one_of<is_digit10, is_one_of<"abcdefABCDEF">>;

    constexpr auto is_alpha = satisfies_one_of<is_lower, is_upper>;
    constexpr auto is_alnum = satisfies_one_of<is_alpha, is_digit10>;

    constexpr auto is_identifier_head = satisfies_one_of<is_alpha, is_one_of<"_">>;
    constexpr auto is_identifier_tail = satisfies_one_of<is_alnum, is_one_of<"_'">>;
    constexpr auto is_operator        = is_one_of<"+-*/.|<=>:!?#%&^~$@\\">;

    constexpr auto is_invalid_character = std::not_fn(
        satisfies_one_of<is_identifier_tail, is_operator, is_space, is_one_of<"(){}[],\"'">>);

    static_assert(is_one_of<"ab">('a'));
    static_assert(is_one_of<"ab">('b'));
    static_assert(!is_one_of<"ab">('c'));

    template <std::predicate<char> auto predicate>
    constexpr auto or_digit_separator(char const c)
    {
        return predicate(c) || c == '\'';
    }

    constexpr auto digit_predicate_for(int const base) noexcept -> auto (*)(char) -> bool
    {
        switch (base) {
        case 2:  return or_digit_separator<is_digit2>;
        case 4:  return or_digit_separator<is_digit4>;
        case 8:  return or_digit_separator<is_digit8>;
        case 10: return or_digit_separator<is_digit10>;
        case 12: return or_digit_separator<is_digit12>;
        case 16: return or_digit_separator<is_digit16>;
        default: cpputil::unreachable();
        }
    }

    class Token_maker {
        std::string_view  m_old_string;
        std::string_view  m_trivia;
        kieli::Position   m_old_position;
        kieli::Lex_state& m_state;
    public:
        explicit Token_maker(std::string_view const trivia, kieli::Lex_state& state) noexcept
            : m_old_string { state.text }
            , m_trivia { trivia }
            , m_old_position { state.position }
            , m_state { state }
        {}

        auto operator()(Token_variant&& value, Token_type const type) const noexcept -> Token
        {
            return Token {
                .variant          = std::move(value),
                .type             = type,
                .preceding_trivia = m_trivia,
                .range            = kieli::Range { m_old_position, m_state.position },
            };
        }
    };

    // If `longer` is `"abcdef"` and `shorter` is `"def"`, return `"abc"`.
    constexpr auto view_difference(std::string_view const longer, std::string_view const shorter)
        -> std::string_view
    {
        assert(longer.size() >= shorter.size());
        return longer.substr(0, longer.size() - shorter.size());
    }

    static_assert(view_difference("abcdef", "def") == "abc");

    auto ensure_no_trailing_separator(std::string_view const string, kieli::Lex_state& state)
        -> liblex::Expected<void>
    {
        cpputil::always_assert(!string.empty());
        if (string.back() == '\'') {
            return liblex::error(state, "Expected one or more digits after the digit separator");
        }
        return {};
    }

    auto handle_escape_sequence(kieli::Lex_state& state) -> liblex::Expected<char>
    {
        char const* const anchor = state.text.data();
        if (state.text.empty()) {
            return liblex::error(state, "Expected an escape sequence, but found the end of input");
        }
        switch (liblex::extract_current(state)) {
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
        default:   return liblex::error(state, { anchor, 1 }, "Unrecognized escape sequence");
        }
    }

    auto skip_string_literal_within_comment(kieli::Lex_state& state) -> liblex::Expected<void>
    {
        char const* const anchor = state.text.data();
        if (!liblex::try_consume(state, '"')) {
            return {};
        }
        for (;;) {
            if (state.text.empty()) {
                return liblex::error(
                    state, { anchor, 1 }, "Unterminating string literal within comment block");
            }
            switch (liblex::extract_current(state)) {
            case '"':  return {};
            case '\\': (void)handle_escape_sequence(state); [[fallthrough]];
            default:   continue;
            }
        }
    }

    auto skip_block_comment(kieli::Lex_state& state, std::string_view const comment_begin)
        -> liblex::Expected<void>
    {
        for (std::size_t depth = 1; depth != 0;) {
            if (auto const result = skip_string_literal_within_comment(state); !result) {
                return result;
            }
            if (liblex::try_consume(state, "*/")) {
                --depth;
            }
            else if (liblex::try_consume(state, "/*")) {
                cpputil::always_assert(!utl::would_increment_overflow(depth));
                ++depth;
            }
            else if (state.text.empty()) {
                return liblex::error(
                    state,
                    comment_begin.substr(0, 2),
                    "Unterminating comment block; missing `*/` terminator");
            }
            else {
                liblex::advance(state);
            }
        }
        return {};
    }

    auto extract_comments_and_whitespace(kieli::Lex_state& state) -> std::string_view
    {
        std::string_view const old_string = state.text;
        for (;;) {
            liblex::consume(state, is_space);
            std::string_view const comment_begin = state.text;
            if (liblex::try_consume(state, "//")) {
                liblex::consume(state, [](char const c) { return c != '\n'; });
            }
            else if (liblex::try_consume(state, "/*")) {
                (void)skip_block_comment(state, comment_begin);
            }
            else {
                break;
            }
        }
        return view_difference(old_string, state.text);
    }

    auto extract_identifier(Token_maker const& token, kieli::Lex_state& state) -> Token
    {
        std::string_view const view = liblex::extract(state, is_identifier_tail);
        cpputil::always_assert(!view.empty());
        if (auto const type = find_token(keyword_token_map, view)) {
            return token({}, type.value());
        }
        if (view == "true") {
            return token(kieli::Boolean { true }, Token_type::boolean_literal);
        }
        if (view == "false") {
            return token(kieli::Boolean { false }, Token_type::boolean_literal);
        }
        if (std::ranges::all_of(view, is_one_of<"_">)) {
            return token({}, Token_type::underscore);
        }
        if (is_upper(view[view.find_first_not_of('_')])) {
            return token(liblex::make_identifier(state, view), Token_type::upper_name);
        }
        return token(liblex::make_identifier(state, view), Token_type::lower_name);
    }

    auto extract_operator(Token_maker const& token, kieli::Lex_state& state) -> Token
    {
        std::string_view const view = liblex::extract(state, is_operator);
        if (auto const type = find_token(punctuation_token_map, view)) {
            return token({}, type.value());
        }
        return token(liblex::make_operator_identifier(state, view), Token_type::operator_name);
    }

    auto extract_potentially_escaped_character(kieli::Lex_state& state) -> liblex::Expected<char>
    {
        char const character = liblex::extract_current(state);
        if (character == '\\') {
            return handle_escape_sequence(state);
        }
        return character;
    }

    auto extract_character_literal(Token_maker const& token, kieli::Lex_state& state)
        -> liblex::Expected<Token>
    {
        cpputil::always_assert(liblex::current(state) == '\'');
        liblex::advance(state);
        if (state.text.empty()) {
            return liblex::error(state, "Unterminating character literal");
        }
        return extract_potentially_escaped_character(state).and_then(
            [&](char const character) -> liblex::Expected<Token> {
                if (liblex::try_consume(state, '\'')) {
                    return token(kieli::Character { character }, Token_type::character_literal);
                }
                return liblex::error(state, "Expected a closing single-quote");
            });
    }

    auto extract_string_literal(Token_maker const& token, kieli::Lex_state& state)
        -> liblex::Expected<Token>
    {
        char const* const anchor = state.text.data();
        cpputil::always_assert(liblex::current(state) == '"');
        liblex::advance(state);

        std::string string;
        for (;;) {
            if (state.text.empty()) {
                return liblex::error(state, { anchor, 1 }, "Unterminating string literal");
            }
            switch (char const character = liblex::extract_current(state)) {
            case '"':
                return token(
                    liblex::make_string_literal(state, string), Token_type::string_literal);
            case '\\':
                if (auto const escaped = handle_escape_sequence(state)) {
                    string.push_back(escaped.value());
                    continue;
                }
                else {
                    return std::unexpected { escaped.error() };
                }
            default: string.push_back(character);
            }
        }
    }

    auto try_extract_numeric_base(kieli::Lex_state& state) -> std::optional<int>
    {
        // clang-format off
        if (liblex::try_consume(state, "0b")) return 2;  // binary
        if (liblex::try_consume(state, "0q")) return 4;  // quaternary
        if (liblex::try_consume(state, "0o")) return 8;  // octal
        if (liblex::try_consume(state, "0d")) return 12; // duodecimal
        if (liblex::try_consume(state, "0x")) return 16; // hexadecimal
        return std::nullopt;
        // clang-format on
    }

    auto consume_and_validate_floating_point_alphabetic_suffix(kieli::Lex_state& state)
        -> liblex::Expected<void>
    {
        std::string_view const suffix = liblex::extract(state, is_alpha);
        if (suffix.empty()) {
            return {};
        }
        if (suffix != "e" && suffix != "E") {
            return liblex::error(
                state, suffix, "Erroneous floating point literal alphabetic suffix");
        }
        (void)liblex::try_consume(state, '-');
        if (state.text.empty() || !is_digit10(liblex::current(state))) {
            return liblex::error(state, "Expected an exponent");
        }
        return ensure_no_trailing_separator(
            liblex::extract(state, or_digit_separator<is_digit10>), state);
    }

    auto parse_floating_value(kieli::Lex_state& state, std::string_view const string)
        -> liblex::Expected<double>
    {
        return liblex::parse_floating(string).transform_error([&](liblex::Numeric_error error) {
            cpputil::always_assert(error == liblex::Numeric_error::out_of_range);
            return liblex::error(state, string, "Floating point literal is too large").error();
        });
    }

    auto extract_numeric_floating(
        Token_maker const&     token,
        std::string_view const old_string,
        kieli::Lex_state&      state) -> liblex::Expected<Token>
    {
        if (state.text.empty() || !or_digit_separator<is_digit10>(liblex::current(state))) {
            return liblex::error(state, "Expected one or more digits after the decimal separator");
        }
        return ensure_no_trailing_separator(
                   liblex::extract(state, or_digit_separator<is_digit10>), state)
            .and_then([&] {
                // Consume the suffix here so the next block can parse from old string to current
                return consume_and_validate_floating_point_alphabetic_suffix(state);
            })
            .and_then([&] {
                return parse_floating_value(state, view_difference(old_string, state.text));
            })
            .and_then([&](double const floating) -> liblex::Expected<Token> {
                std::string_view const extraneous_suffix = liblex::extract(state, is_alpha);
                if (!extraneous_suffix.empty()) {
                    return liblex::error(
                        state,
                        extraneous_suffix,
                        "Erroneous floating point literal alphabetic suffix");
                }
                return token(kieli::Floating { floating }, Token_type::floating_literal);
            });
    }

    auto extract_integer_exponent(kieli::Lex_state& state) -> liblex::Expected<std::size_t>
    {
        std::string_view const digits = liblex::extract(state, is_digit10);
        return ensure_no_trailing_separator(digits, state)
            .and_then([&]() -> liblex::Expected<std::size_t> {
                std::string_view const extraneous_suffix = liblex::extract(state, is_alpha);
                if (!extraneous_suffix.empty()) {
                    return liblex::error(
                        state, extraneous_suffix, "Erroneous integer literal alphabetic suffix");
                }
                return liblex::parse_integer(digits).transform_error(
                    [&](liblex::Numeric_error const error) {
                        cpputil::always_assert(error == liblex::Numeric_error::out_of_range);
                        return liblex::error(state, digits, "Exponent is too large").error();
                    });
            });
    }

    auto apply_integer_exponent(
        std::size_t const      integer,
        std::size_t const      exponent,
        std::string_view const old_string,
        kieli::Lex_state&      state) -> liblex::Expected<std::size_t>
    {
        return liblex::apply_scientific_exponent(integer, exponent)
            .transform_error([&](liblex::Numeric_error const error) {
                cpputil::always_assert(error == liblex::Numeric_error::out_of_range);
                return liblex::error(
                           state,
                           view_difference(old_string, state.text),
                           "Integer literal is too large after applying scientific exponent")
                    .error();
            });
    }

    auto extract_and_apply_potential_integer_exponent(
        kieli::Lex_state&      state,
        std::string_view const old_string,
        std::size_t const      integer) -> liblex::Expected<std::size_t>
    {
        std::string_view const suffix = liblex::extract(state, is_alpha);
        if (suffix.empty()) {
            return integer;
        }
        if (suffix != "e" && suffix != "E") {
            return liblex::error(state, suffix, "Erroneous integer literal alphabetic suffix");
        }
        if (liblex::try_consume(state, '-')) {
            liblex::consume(state, is_digit10);
            return liblex::error(state, "An integer literal may not have a negative exponent");
        }
        if (state.text.empty() || !is_digit10(liblex::current(state))) {
            return liblex::error(state, "Expected an exponent");
        }
        return extract_integer_exponent(state).and_then([&](std::size_t const exponent) {
            return apply_integer_exponent(integer, exponent, old_string, state);
        });
    }

    auto extract_numeric_integer(
        Token_maker const&     token,
        std::string_view const old_string,
        std::string_view const digits,
        int const              base,
        kieli::Lex_state&      state) -> liblex::Expected<Token>
    {
        return liblex::parse_integer(digits, base)
            .transform_error([&](liblex::Numeric_error const error) {
                cpputil::always_assert(error == liblex::Numeric_error::out_of_range);
                return liblex::error(state, digits, "Integer literal is too large").error();
            })
            .and_then([&](std::size_t const integer) {
                return extract_and_apply_potential_integer_exponent(state, old_string, integer);
            })
            .transform([&](std::size_t const integer) {
                return token(kieli::Integer { integer }, Token_type::integer_literal);
            });
    }

    auto extract_numeric(Token_maker const& token, kieli::Lex_state& state)
        -> liblex::Expected<Token>
    {
        std::string_view const old_string = state.text;
        std::optional const    base       = try_extract_numeric_base(state);
        auto const             predicate  = digit_predicate_for(base.value_or(10));
        std::string_view const digits     = liblex::extract(state, predicate);

        if (base.has_value()) {
            if (digits.find_first_not_of('\'') == std::string_view::npos) {
                std::string message = std::format(
                    "Expected one or more digits after the base-{} specifier", base.value());
                return liblex::error(state, std::move(message));
            }
        }

        return ensure_no_trailing_separator(digits, state)
            .and_then([&]() -> liblex::Expected<Token> {
                // If the literal is preceded by a dot, don't attempt to extract a float.
                // This enables nested tuple field access: x.0.0

                std::string_view const preceding_string
                    = view_difference(liblex::source_begin(state), old_string);
                bool const has_preceding_dot
                    = !preceding_string.empty() && preceding_string.back() == '.';

                if (!has_preceding_dot && liblex::try_consume(state, '.')) {
                    if (base.has_value()) {
                        liblex::consume(state, is_digit10);
                        return liblex::error(
                            state,
                            old_string.substr(0, 2),
                            "A floating point literal may not have a base specifier");
                    }
                    return extract_numeric_floating(token, old_string, state);
                }
                return extract_numeric_integer(token, old_string, digits, base.value_or(10), state);
            });
    }

    auto extract_token(Token_maker const& token, kieli::Lex_state& state) -> liblex::Expected<Token>
    {
        auto const simple_token = [&](Token_type const type) noexcept {
            liblex::advance(state);
            return token(Token_variant {}, type);
        };
        switch (char const current = liblex::current(state)) {
        case '(':  return simple_token(Token_type::paren_open);
        case ')':  return simple_token(Token_type::paren_close);
        case '{':  return simple_token(Token_type::brace_open);
        case '}':  return simple_token(Token_type::brace_close);
        case '[':  return simple_token(Token_type::bracket_open);
        case ']':  return simple_token(Token_type::bracket_close);
        case ';':  return simple_token(Token_type::semicolon);
        case ',':  return simple_token(Token_type::comma);
        case '\'': return extract_character_literal(token, state);
        case '\"': return extract_string_literal(token, state);
        default:
            if (is_identifier_head(current)) {
                return extract_identifier(token, state);
            }
            if (is_operator(current)) {
                return extract_operator(token, state);
            }
            if (is_digit10(current)) {
                return extract_numeric(token, state);
            }
            return liblex::error(
                state,
                liblex::extract(state, is_invalid_character),
                "Unable to extract lexical token");
        }
    }

} // namespace

auto kieli::lex(Lex_state& state) -> Token
{
    Token_maker const token_maker { extract_comments_and_whitespace(state), state };
    if (state.text.empty()) {
        ++state.position.column; // According to the LSP spec, the stop position is exclusive.
        return token_maker({}, Token_type::end_of_input);
    }
    if (auto token = extract_token(token_maker, state)) {
        return token.value();
    }
    return token_maker({}, Token_type::error);
}

auto kieli::lex_state(Database& db, Document_id const document_id) -> Lex_state
{
    return Lex_state {
        .db          = db,
        .document_id = document_id,
        .text        = kieli::document(db, document_id).text,
    };
}
