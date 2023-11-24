#include <libutl/common/utilities.hpp>
#include <liblex/context.hpp>
#include <liblex/numeric.hpp>
#include <liblex/lex.hpp>

namespace {

    using Token = kieli::Lexical_token;

    constexpr auto punctuation_token_map = std::to_array<utl::Pair<std::string_view, Token::Type>>({
        { ".", Token::Type::dot },
        { ":", Token::Type::colon },
        { "::", Token::Type::double_colon },
        { "|", Token::Type::pipe },
        { "=", Token::Type::equals },
        { "&", Token::Type::ampersand },
        { "*", Token::Type::asterisk },
        { "+", Token::Type::plus },
        { "?", Token::Type::question },
        { "\\", Token::Type::lambda },
        { "<-", Token::Type::left_arrow },
        { "->", Token::Type::right_arrow },
        { R"(???)", Token::Type::hole },
    });

    constexpr auto keyword_token_map = std::to_array<utl::Pair<std::string_view, Token::Type>>({
        { "let", Token::Type::let },
        { "mut", Token::Type::mut },
        { "if", Token::Type::if_ },
        { "else", Token::Type::else_ },
        { "elif", Token::Type::elif },
        { "for", Token::Type::for_ },
        { "in", Token::Type::in },
        { "while", Token::Type::while_ },
        { "loop", Token::Type::loop },
        { "continue", Token::Type::continue_ },
        { "break", Token::Type::break_ },
        { "match", Token::Type::match },
        { "ret", Token::Type::ret },
        { "discard", Token::Type::discard },
        { "fn", Token::Type::fn },
        { "as", Token::Type::as },
        { "I8", Token::Type::i8_type },
        { "I16", Token::Type::i16_type },
        { "I32", Token::Type::i32_type },
        { "I64", Token::Type::i64_type },
        { "U8", Token::Type::u8_type },
        { "U16", Token::Type::u16_type },
        { "U32", Token::Type::u32_type },
        { "U64", Token::Type::u64_type },
        { "Float", Token::Type::floating_type },
        { "Char", Token::Type::character_type },
        { "Bool", Token::Type::boolean_type },
        { "String", Token::Type::string_type },
        { "self", Token::Type::lower_self },
        { "Self", Token::Type::upper_self },
        { "enum", Token::Type::enum_ },
        { "struct", Token::Type::struct_ },
        { "class", Token::Type::class_ },
        { "inst", Token::Type::inst },
        { "impl", Token::Type::impl },
        { "alias", Token::Type::alias },
        { "namespace", Token::Type::namespace_ },
        { "import", Token::Type::import_ },
        { "export", Token::Type::export_ },
        { "module", Token::Type::module_ },
        { "sizeof", Token::Type::sizeof_ },
        { "typeof", Token::Type::typeof_ },
        { "addressof", Token::Type::addressof },
        { "dereference", Token::Type::dereference },
        { "unsafe", Token::Type::unsafe },
        { "mov", Token::Type::mov },
        { "meta", Token::Type::meta },
        { "where", Token::Type::where },
        { "immut", Token::Type::immut },
        { "dyn", Token::Type::dyn },
        { "pub", Token::Type::pub },
        { "macro", Token::Type::macro },
        { "global", Token::Type::global },
    });

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
    constexpr auto is_digit16 = satisfies_one_of<is_digit12, is_one_of<"cdefCDEF">>;

    constexpr auto is_alpha = satisfies_one_of<is_lower, is_upper>;
    constexpr auto is_alnum = satisfies_one_of<is_alpha, is_digit10>;

    constexpr auto is_identifier_head = satisfies_one_of<is_alpha, is_one_of<"_">>;
    constexpr auto is_identifier_tail = satisfies_one_of<is_alnum, is_one_of<"_'">>;
    constexpr auto is_operator        = is_one_of<"+-*/.|<=>:!?#%&^~$@\\">;

    constexpr auto is_invalid_character = std::not_fn(
        satisfies_one_of<is_identifier_tail, is_operator, is_space, is_one_of<"(){}[],\"'">>);

    template <std::predicate<char> auto is_digit>
    constexpr auto is_digit_or_separator
        = satisfies_one_of<is_digit, [](char const c) { return c == '\''; }>;

    static_assert(is_one_of<"ab">('a'));
    static_assert(is_one_of<"ab">('b'));
    static_assert(!is_one_of<"ab">('c'));
    static_assert(!is_one_of<"ab">('\0'));

    constexpr auto digit_predicate_for(int const base) noexcept -> bool (*)(char)
    {
        // clang-format off
        switch (base) {
        case 2:  return is_digit_or_separator<is_digit2>;
        case 4:  return is_digit_or_separator<is_digit4>;
        case 8:  return is_digit_or_separator<is_digit8>;
        case 10: return is_digit_or_separator<is_digit10>;
        case 12: return is_digit_or_separator<is_digit12>;
        case 16: return is_digit_or_separator<is_digit16>;
        default:
            utl::unreachable();
        }
        // clang-format on
    }

    auto make_token(
        utl::Source_position const start_position,
        utl::Source_position const stop_position,
        std::string_view const     string_view,
        utl::Source::Wrapper const source,
        Token::Variant const&      value,
        Token::Type const          type,
        std::string_view const     preceding_trivia) -> Token
    {
        return Token {
            .value            = value,
            .type             = type,
            .preceding_trivia = preceding_trivia,
            .source_view = utl::Source_view { source, string_view, start_position, stop_position },
        };
    }

    class [[nodiscard]] Token_maker {
        char const*          m_old_pointer;
        utl::Source_position m_old_position;
        std::string_view     m_trivia;
        liblex::Context&     m_context;
    public:
        explicit Token_maker(std::string_view const trivia, liblex::Context& context) noexcept
            : m_old_pointer { context.pointer() }
            , m_old_position { context.position() }
            , m_trivia { trivia }
            , m_context { context }
        {}

        auto operator()(Token::Variant&& value, Token::Type const type) const -> Token
        {
            return make_token(
                m_old_position,
                m_context.position(),
                { m_old_pointer, m_context.pointer() },
                m_context.source(),
                std::move(value),
                type,
                m_trivia);
        }
    };

    [[nodiscard]] auto error_if_trailing_separator(
        std::string_view const string, liblex::Context& context)
        -> std::optional<tl::unexpected<liblex::Token_extraction_failure>>
    {
        if (!string.empty() && string.back() == '\'') {
            return context.error("Expected one or more digits after the digit separator");
        }
        return std::nullopt;
    }

    [[nodiscard]] auto handle_escape_sequence(liblex::Context& context) -> liblex::Expected<char>
    {
        char const* const anchor = context.pointer();
        if (context.is_finished()) {
            return context.error(anchor, "Expected an escape sequence, but found the end of input");
        }
        // clang-format off
        switch (context.extract_current()) {
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
        default:
            return context.error(anchor, "Unrecognized escape sequence");
        }
        // clang-format on
    }

    auto skip_string_literal_within_comment(liblex::Context& context) -> liblex::Expected<void>
    {
        char const* const anchor = context.pointer();
        if (!context.try_consume('"')) {
            return {};
        }
        for (;;) {
            if (context.is_finished()) {
                return context.error(anchor, "Unterminating string literal within comment block");
            }
            switch (context.extract_current()) {
            case '"':
                return {};
            case '\\':
                (void)handle_escape_sequence(context);
                [[fallthrough]];
            default:
                continue;
            }
        }
    }

    auto skip_block_comment(char const* const anchor, liblex::Context& context)
        -> liblex::Expected<void>
    {
        for (utl::Usize depth = 1; depth != 0;) {
            skip_string_literal_within_comment(context);

            if (context.try_consume("*/")) {
                assert(depth != 0);
                --depth;
            }
            else if (context.try_consume("/*")) {
                if (depth == 50) {
                    return context.error(
                        { context.pointer() - 2, 2 }, "This block comment is too deeply nested");
                }
                ++depth;
            }
            else if (context.is_finished()) {
                // TODO: help note: "Comments starting with '/*' must be terminated with '*/'"
                return context.error(anchor, "Unterminating comment block");
            }
            else {
                context.advance();
            }
        }
        return {};
    }

    auto extract_comments_and_whitespace(liblex::Context& context) -> std::string_view
    {
        char const* const anchor = context.pointer();
        for (;;) {
            context.consume(is_space);
            if (context.try_consume("//")) {
                context.consume([](char const c) { return c != '\n'; });
            }
            else if (context.try_consume("/*")) {
                skip_block_comment(anchor, context);
            }
            else {
                break;
            }
        }
        return std::string_view { anchor, context.pointer() };
    }

    auto extract_identifier(Token_maker const& make_token, liblex::Context& context) -> Token
    {
        std::string_view const view = context.extract(is_identifier_tail);
        assert(!view.empty());

        if (auto const it = ranges::find(keyword_token_map, view, utl::first);
            it != keyword_token_map.end())
        {
            return make_token({}, it->second);
        }

        if (view == "true") {
            return make_token(kieli::Boolean { true }, Token::Type::boolean_literal);
        }
        if (view == "false") {
            return make_token(kieli::Boolean { false }, Token::Type::boolean_literal);
        }
        if (ranges::all_of(view, is_one_of<"_">)) {
            return make_token({}, Token::Type::underscore);
        }
        if (is_upper(view[view.find_first_not_of('_')])) {
            return make_token(context.make_identifier(view), Token::Type::upper_name);
        }
        return make_token(context.make_identifier(view), Token::Type::lower_name);
    }

    auto extract_operator(Token_maker const& make_token, liblex::Context& context) -> Token
    {
        std::string_view const view = context.extract(is_operator);
        if (auto const it = ranges::find(punctuation_token_map, view, utl::first);
            it != punctuation_token_map.end())
        {
            return make_token({}, it->second);
        }
        return make_token(context.make_operator(view), Token::Type::operator_name);
    }

    auto extract_character_literal(Token_maker const& make_token, liblex::Context& context)
        -> liblex::Expected<Token>
    {
        char const* const anchor = context.pointer();
        assert(context.current() == '\'');
        context.advance();

        if (context.is_finished()) {
            return context.error(anchor, "Unterminating character literal");
        }

        char c = context.extract_current();
        if (c == '\\') {
            if (auto const character = handle_escape_sequence(context)) {
                c = character.value();
            }
            else {
                return tl::unexpected { character.error() };
            }
        }

        if (context.try_consume('\'')) {
            return make_token(kieli::Character { c }, Token::Type::character_literal);
        }
        return context.error("Expected a closing single-quote");
    }

    auto extract_string_literal(Token_maker const& make_token, liblex::Context& context)
        -> liblex::Expected<Token>
    {
        char const* const anchor = context.pointer();
        assert(context.current() == '"');
        context.advance();

        static thread_local std::string string = utl::string_with_capacity(50);
        string.clear();

        for (;;) {
            if (context.is_finished()) {
                return context.error(anchor, "Unterminating string literal");
            }
            switch (char c = context.extract_current()) {
            case '"':
                return make_token(context.make_string_literal(string), Token::Type::string_literal);
            case '\\':
                if (auto const character = handle_escape_sequence(context)) {
                    c = character.value();
                }
                else {
                    return tl::unexpected { character.error() };
                }
                [[fallthrough]];
            default:
                string.push_back(c);
            }
        }
    }

    auto try_extract_numeric_base(liblex::Context& context) -> std::optional<int>
    {
        // clang-format off
        if (context.try_consume("0b")) return 2;  // binary
        if (context.try_consume("0q")) return 4;  // quaternary
        if (context.try_consume("0o")) return 8;  // octal
        if (context.try_consume("0d")) return 12; // duodecimal
        if (context.try_consume("0x")) return 16; // hexadecimal
        return std::nullopt;
        // clang-format on
    }

    auto extract_numeric_floating(
        Token_maker const& make_token, char const* const anchor, liblex::Context& context)
        -> liblex::Expected<Token>
    {
        static constexpr auto digit_predicate = is_digit_or_separator<is_digit10>;
        if (context.is_finished() || !digit_predicate(context.current())) {
            return context.error("Expected one or more digits after the decimal separator");
        }

        std::string_view const second_digit_sequence = context.extract(digit_predicate);
        if (auto const error = error_if_trailing_separator(second_digit_sequence, context)) {
            return error.value();
        }

        std::string_view const suffix = context.extract(is_alpha);
        if (!suffix.empty()) {
            if (suffix != "e" && suffix != "E") {
                return context.error(suffix, "Erroneous floating point literal alphabetic suffix");
            }
            (void)context.try_consume('-');
            if (context.is_finished() || !is_digit10(context.current())) {
                return context.error("Expected an exponent");
            }
            std::string_view const exponent_digits = context.extract(digit_predicate);
            if (auto const error = error_if_trailing_separator(exponent_digits, context)) {
                return error.value();
            }
        }

        std::string_view const floating_string { anchor, context.pointer() };
        auto const             floating = liblex::parse_floating(floating_string);
        if (floating == tl::unexpected { liblex::Numeric_error::out_of_range }) {
            return context.error(floating_string, "Floating point literal is too large");
        }
        utl::always_assert(floating.has_value());

        std::string_view const extraneous_suffix = context.extract(is_alpha);
        if (!extraneous_suffix.empty()) {
            return context.error(
                extraneous_suffix, "Erroneous floating point literal alphabetic suffix");
        }

        return make_token(kieli::Floating { floating.value() }, Token::Type::floating_literal);
    }

    auto extract_numeric_integer(
        Token_maker const&     make_token,
        char const* const      anchor,
        std::string_view const digit_sequence,
        int const              base,
        liblex::Context&       context) -> liblex::Expected<Token>
    {
        auto integer = liblex::parse_integer(digit_sequence, base);
        if (integer == tl::unexpected { liblex::Numeric_error::out_of_range }) {
            return context.error(digit_sequence, "Integer literal is too large");
        }
        utl::always_assert(integer.has_value());

        std::string_view const suffix = context.extract(is_alpha);
        if (suffix.empty()) {
            return make_token(kieli::Integer { integer.value() }, Token::Type::integer_literal);
        }
        if (suffix != "e" && suffix != "E") {
            return context.error(suffix, "Erroneous integer literal alphabetic suffix");
        }
        if (context.try_consume('-')) {
            context.consume(is_digit10);
            return context.error("An integer literal may not have a negative exponent");
        }
        if (context.is_finished() || !is_digit10(context.current())) {
            return context.error("Expected an exponent");
        }

        std::string_view const exponent_digit_sequence = context.extract(is_digit10);
        if (auto const error = error_if_trailing_separator(exponent_digit_sequence, context)) {
            return error.value();
        }
        std::string_view const extraneous_suffix = context.extract(is_alpha);
        if (!extraneous_suffix.empty()) {
            return context.error(extraneous_suffix, "Erroneous integer literal alphabetic suffix");
        }
        auto const exponent = liblex::parse_integer(exponent_digit_sequence);
        if (exponent == tl::unexpected { liblex::Numeric_error::out_of_range }) {
            return context.error(exponent_digit_sequence, "Exponent is too large");
        }
        utl::always_assert(exponent.has_value());

        auto const result = liblex::apply_scientific_exponent(*integer, *exponent);
        if (result == tl::unexpected { liblex::Numeric_error::out_of_range }) {
            return context.error(
                { anchor, context.pointer() },
                "Integer literal is too large after applying scientific exponent");
        }
        utl::always_assert(result.has_value());

        return make_token(kieli::Integer { result.value() }, Token::Type::integer_literal);
    }

    auto extract_numeric(Token_maker const& make_token, liblex::Context& context)
        -> liblex::Expected<Token>
    {
        char const* const      anchor               = context.pointer();
        std::optional const    base                 = try_extract_numeric_base(context);
        auto const             digit_predicate      = digit_predicate_for(base.value_or(10));
        std::string_view const first_digit_sequence = context.extract(digit_predicate);

        if (base.has_value()) {
            if (first_digit_sequence.find_first_not_of('\'') == std::string_view::npos) {
                // FIXME: Specify base value in message
                return context.error("Expected one or more digits after the base specifier");
            }
        }
        else {
            assert(!first_digit_sequence.empty());
            if (auto const error = error_if_trailing_separator(first_digit_sequence, context)) {
                return error.value();
            }
        }

        // If the literal is preceded by a dot, don't attempt to extract a float.
        // This enables nested tuple field access: x.0.0

        bool const has_preceding_dot
            = std::invoke([&] { return anchor != context.source_begin() && anchor[-1] == '.'; });

        if (!has_preceding_dot && context.try_consume('.')) {
            if (base.has_value()) {
                context.consume(is_digit10);
                return context.error(
                    { anchor, 2 }, "A floating point literal may not have a base specifier");
            }
            return extract_numeric_floating(make_token, anchor, context);
        }
        return extract_numeric_integer(
            make_token, anchor, first_digit_sequence, base.value_or(10), context);
    }

    auto extract_token(Token_maker const& token, liblex::Context& context)
        -> liblex::Expected<Token>
    {
        assert(!context.is_finished());
        switch (char const current = context.current()) {
        case '(':
            context.advance();
            return token({}, Token::Type::paren_open);
        case ')':
            context.advance();
            return token({}, Token::Type::paren_close);
        case '{':
            context.advance();
            return token({}, Token::Type::brace_open);
        case '}':
            context.advance();
            return token({}, Token::Type::brace_close);
        case '[':
            context.advance();
            return token({}, Token::Type::bracket_open);
        case ']':
            context.advance();
            return token({}, Token::Type::bracket_close);
        case ';':
            context.advance();
            return token({}, Token::Type::semicolon);
        case ',':
            context.advance();
            return token({}, Token::Type::comma);
        case '\'':
            return extract_character_literal(token, context);
        case '\"':
            return extract_string_literal(token, context);
        default:
            if (is_identifier_head(current)) {
                return extract_identifier(token, context);
            }
            else if (is_operator(current)) {
                return extract_operator(token, context);
            }
            else if (is_digit10(current)) {
                return extract_numeric(token, context);
            }
            else {
                return context.error(
                    context.extract(is_invalid_character), "Unable to extract lexical token");
            }
        }
    }

} // namespace

auto kieli::lex(utl::Source::Wrapper const source, Compile_info& compile_info)
    -> std::vector<Lexical_token>
{
    std::string_view const source_string = source->string();
    utl::always_assert(source_string.data() != nullptr);

    std::string_view   current_trivia;
    std::vector<Token> tokens;
    tokens.reserve(1024);

    liblex::Context context { source, compile_info };
    for (;;) {
        current_trivia = extract_comments_and_whitespace(context);
        if (context.is_finished()) {
            break;
        }
        Token_maker const make_token { current_trivia, context };
        if (auto token = extract_token(make_token, context)) {
            tokens.push_back(std::move(*token));
        }
        else {
            tokens.push_back(make_token({}, Token::Type::error));
        }
    }

    Token end_of_input_token = make_token(
        context.position(),
        context.position(),
        { source_string.data() + source_string.size(), 0 },
        source,
        Token::Variant {},
        Token::Type::end_of_input,
        current_trivia);
    tokens.push_back(std::move(end_of_input_token));

    return tokens;
}
