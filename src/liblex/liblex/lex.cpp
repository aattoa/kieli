#include <libutl/common/utilities.hpp>
#include <liblex/context.hpp>
#include <liblex/numeric.hpp>
#include <liblex/lex.hpp>


namespace {

    using Token = kieli::Lexical_token;

    constexpr auto punctuation_token_map = std::to_array<utl::Pair<std::string_view, Token::Type>>({
        { "."      , Token::Type::dot          },
        { ":"      , Token::Type::colon        },
        { "::"     , Token::Type::double_colon },
        { "|"      , Token::Type::pipe         },
        { "="      , Token::Type::equals       },
        { "&"      , Token::Type::ampersand    },
        { "*"      , Token::Type::asterisk     },
        { "+"      , Token::Type::plus         },
        { "?"      , Token::Type::question     },
        { "\\"     , Token::Type::lambda       },
        { "<-"     , Token::Type::left_arrow   },
        { "->"     , Token::Type::right_arrow  },
        { "\?\?\?" , Token::Type::hole         },
    });

    constexpr auto keyword_token_map = std::to_array<utl::Pair<std::string_view, Token::Type>>({
        { "let"         , Token::Type::let            },
        { "mut"         , Token::Type::mut            },
        { "if"          , Token::Type::if_            },
        { "else"        , Token::Type::else_          },
        { "elif"        , Token::Type::elif           },
        { "for"         , Token::Type::for_           },
        { "in"          , Token::Type::in             },
        { "while"       , Token::Type::while_         },
        { "loop"        , Token::Type::loop           },
        { "continue"    , Token::Type::continue_      },
        { "break"       , Token::Type::break_         },
        { "match"       , Token::Type::match          },
        { "ret"         , Token::Type::ret            },
        { "discard"     , Token::Type::discard        },
        { "fn"          , Token::Type::fn             },
        { "as"          , Token::Type::as             },
        { "I8"          , Token::Type::i8_type        },
        { "I16"         , Token::Type::i16_type       },
        { "I32"         , Token::Type::i32_type       },
        { "I64"         , Token::Type::i64_type       },
        { "U8"          , Token::Type::u8_type        },
        { "U16"         , Token::Type::u16_type       },
        { "U32"         , Token::Type::u32_type       },
        { "U64"         , Token::Type::u64_type       },
        { "Float"       , Token::Type::floating_type  },
        { "Char"        , Token::Type::character_type },
        { "Bool"        , Token::Type::boolean_type   },
        { "String"      , Token::Type::string_type    },
        { "self"        , Token::Type::lower_self     },
        { "Self"        , Token::Type::upper_self     },
        { "enum"        , Token::Type::enum_          },
        { "struct"      , Token::Type::struct_        },
        { "class"       , Token::Type::class_         },
        { "inst"        , Token::Type::inst           },
        { "impl"        , Token::Type::impl           },
        { "alias"       , Token::Type::alias          },
        { "namespace"   , Token::Type::namespace_     },
        { "import"      , Token::Type::import_        },
        { "export"      , Token::Type::export_        },
        { "module"      , Token::Type::module_        },
        { "sizeof"      , Token::Type::sizeof_        },
        { "typeof"      , Token::Type::typeof_        },
        { "addressof"   , Token::Type::addressof      },
        { "dereference" , Token::Type::dereference    },
        { "unsafe"      , Token::Type::unsafe         },
        { "mov"         , Token::Type::mov            },
        { "meta"        , Token::Type::meta           },
        { "where"       , Token::Type::where          },
        { "immut"       , Token::Type::immut          },
        { "dyn"         , Token::Type::dyn            },
        { "pub"         , Token::Type::pub            },
        { "macro"       , Token::Type::macro          },
        { "global"      , Token::Type::global         },
    });


    template <utl::Metastring string>
    constexpr auto is_one_of(char const c) noexcept -> bool {
        return ranges::contains(string.view(), c);
    }
    template <char a, char b> requires (a < b)
    constexpr auto is_in_range(char const c) noexcept -> bool {
        return a <= c && c <= b;
    }
    template <std::predicate<char> auto... predicates>
    constexpr auto satisfies_one_of(char const c) noexcept -> bool {
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

    constexpr auto is_invalid_character =
        std::not_fn(satisfies_one_of<is_identifier_tail, is_operator, is_space, is_one_of<"(){}[],\"'">>);

    template <std::predicate<char> auto is_digit>
    constexpr auto is_digit_or_separator =
        satisfies_one_of<is_digit, [](char const c) { return c == '\''; }>;

    static_assert(is_one_of<"ab">('a'));
    static_assert(is_one_of<"ab">('b'));
    static_assert(!is_one_of<"ab">('c'));
    static_assert(!is_one_of<"ab">('\0'));

    constexpr auto digit_predicate_for(int const base) noexcept -> bool(*)(char) {
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
    }

    auto make_token(
        utl::Source_position const start_position,
        utl::Source_position const stop_position,
        std::string_view const     string_view,
        Token::Variant const&      value,
        Token::Type const          type,
        utl::Source::Wrapper const source) -> Token
    {
        return Token {
            .value       = value,
            .type        = type,
            .source_view = utl::Source_view {
                source,
                string_view,
                start_position,
                stop_position,
            },
        };
    }

    class [[nodiscard]] Token_maker {
        char const*          m_old_pointer;
        utl::Source_position m_old_position;
        liblex::Context&     m_context;
    public:
        explicit Token_maker(liblex::Context& context) noexcept
            : m_old_pointer  { context.pointer() }
            , m_old_position { context.position() }
            , m_context      { context } {}
        auto operator()(Token::Variant&& value, Token::Type const type) const -> Token {
            return make_token(
                m_old_position,
                m_context.position(),
                { m_old_pointer, m_context.pointer() },
                std::move(value),
                type,
                m_context.source());
        }
    };

    auto handle_escape_sequence(liblex::Context& context) -> char {
        char const* const anchor = context.pointer();
        if (context.is_finished()) {
            context.error(anchor, { "Expected an escape sequence, but found the end of input" });
        }
        switch (context.extract_current()) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\'': return '\'';
        case '\"': return '\"';
        case '\\': return '\\';
        default:
            context.error(anchor, { "Unrecognized escape sequence" });
        }
    }

    auto skip_string_literal_within_comment(liblex::Context& context) -> void {
        char const* const anchor = context.pointer();
        if (!context.try_consume('"')) return;
        for (;;) {
            if (context.is_finished()) {
                context.error(anchor, { "Unterminating string literal within comment block" });
            }
            switch (context.extract_current()) {
            case '"':
                return;
            case '\\':
                (void)handle_escape_sequence(context);
                [[fallthrough]];
            default:
                continue;
            }
        }
    }

    auto skip_comment_block(char const* const anchor, liblex::Context& context) -> void {
        for (utl::Usize depth = 1; depth != 0; ) {
            skip_string_literal_within_comment(context);

            if (context.try_consume("*/")) {
                assert(depth != 0);
                --depth;
            }
            else if (context.try_consume("/*")) {
                assert(depth != std::numeric_limits<utl::Usize>::max());
                ++depth;
            }
            else if (context.is_finished()) {
                context.error(anchor, {
                    .message   = "Unterminating comment block",
                    .help_note = "Comments starting with '/*' must be terminated with '*/'",
                });
            }
            else {
                context.advance();
            }
        }
    }

    auto extract_comments_and_whitespace(liblex::Context& context) -> std::string_view {
        char const* const anchor = context.pointer();
        for (;;) {
            context.consume(is_space);
            if (context.try_consume("//"))
                context.consume([](char const c) { return c != '\n'; });
            else if (context.try_consume("/*"))
                skip_comment_block(anchor, context);
            else
                break;
        }
        return std::string_view { anchor, context.pointer() };
    }

    auto extract_identifier(Token_maker const& token_maker, liblex::Context& context) -> Token {
        std::string_view const view = context.extract(is_identifier_tail);
        assert(!view.empty());
        for (auto const& [keyword, token_type] : keyword_token_map) { // NOLINT
            if (view == keyword)
                return token_maker({}, token_type);
        }
        if (view == "true")
            return token_maker(compiler::Boolean { true }, Token::Type::boolean_literal);
        else if (view == "false")
            return token_maker(compiler::Boolean { false }, Token::Type::boolean_literal);
        else if (ranges::all_of(view, is_one_of<"_">))
            return token_maker({}, Token::Type::underscore);
        else if (is_upper(view[view.find_first_not_of('_')]))
            return token_maker(context.make_identifier(view), Token::Type::upper_name);
        else
            return token_maker(context.make_identifier(view), Token::Type::lower_name);
    }

    auto extract_operator(Token_maker const& token_maker, liblex::Context& context) -> Token {
        std::string_view const view = context.extract(is_operator);
        assert(!view.empty());
        for (auto const [operator_name, token_type] : punctuation_token_map) {
            if (view == operator_name)
                return token_maker({}, token_type);
        }
        return token_maker(context.make_operator(view), Token::Type::operator_name);
    }

    auto extract_character_literal(Token_maker const& token_maker, liblex::Context& context) -> Token {
        char const* const anchor = context.pointer();
        assert(context.current() == '\'');
        context.advance();

        if (context.is_finished())
            context.error(anchor, { "Unterminating character literal" });

        char c = context.extract_current();
        if (c == '\\') c = handle_escape_sequence(context);

        if (context.try_consume('\''))
            return token_maker(compiler::Character { c }, Token::Type::character_literal);
        else
            context.error(context.pointer(), { "Expected a closing single-quote" });
    }

    auto extract_string_literal(Token_maker const& token_maker, liblex::Context& context) -> Token {
        char const* const anchor = context.pointer();
        assert(context.current() == '"');
        context.advance();

        static thread_local std::string string = utl::string_with_capacity(50);
        string.clear();

        for (;;) {
            if (context.is_finished())
                context.error(anchor, { "Unterminating string literal" });

            switch (char c = context.extract_current()) {
            case '"':
                return token_maker(context.make_string_literal(string), Token::Type::string_literal);
            case '\\':
                c = handle_escape_sequence(context);
                [[fallthrough]];
            default:
                string.push_back(c);
            }
        }
    }

    auto try_extract_numeric_base(liblex::Context& context) -> tl::optional<int> {
        if (context.try_consume("0b")) return 2;  // binary
        if (context.try_consume("0q")) return 4;  // quaternary
        if (context.try_consume("0o")) return 8;  // octal
        if (context.try_consume("0d")) return 12; // duodecimal
        if (context.try_consume("0x")) return 16; // hexadecimal
        return tl::nullopt;
    }

    auto extract_numeric(Token_maker const& token_maker, liblex::Context& context) -> Token {
        char const* const anchor = context.pointer();

        auto const error_if_trailing_separator = [&](std::string_view const view) {
            if (view.empty() || view.back() != '\'') return;
            context.error({ "Expected one or more digits after the digit separator" });
        };

        tl::optional     const base                 = try_extract_numeric_base(context);
        auto             const digit_predicate      = digit_predicate_for(base.value_or(10));
        std::string_view const first_digit_sequence = context.extract(digit_predicate);

        if (base.has_value()) {
            if (first_digit_sequence.find_first_not_of('\'') == std::string_view::npos) {
                context.error(context.pointer(),
                    { "Expected one or more digits after the base-{} specifier"_format(*base) });
            }
        }
        else {
            assert(!first_digit_sequence.empty());
            error_if_trailing_separator(first_digit_sequence);
        }

        auto const has_preceding_dot = [&] {
            return anchor != context.source_begin() && anchor[-1] == '.';
        };

        if (!has_preceding_dot() && context.try_consume('.')) {
            if (base.has_value()) {
                context.consume(is_digit10);
                context.error({ anchor, 2 },
                    { "A floating point literal may not have a base specifier" });
            }
            else if (context.is_finished() || !digit_predicate(context.current())) {
                context.error({ "Expected one or more digits after the decimal separator" });
            }
            std::string_view const second_digit_sequence = context.extract(digit_predicate);
            error_if_trailing_separator(second_digit_sequence);

            std::string_view const suffix = context.extract(is_alpha);
            if (!suffix.empty()) {
                if (suffix != "e" && suffix != "E") {
                    context.error(suffix, { "Erroneous floating point literal alphabetic suffix" });
                }
                (void)context.try_consume('-');
                if (context.is_finished() || !is_digit10(context.current())) {
                    context.error({ "Expected an exponent" });
                }
                else {
                    std::string_view const exponent_digits = context.extract(digit_predicate);
                    assert(!exponent_digits.empty());
                    error_if_trailing_separator(exponent_digits);
                }
            }

            std::string_view const floating_string { anchor, context.pointer() };
            auto const floating = liblex::parse_floating(floating_string);
            if (floating == tl::unexpected { liblex::Numeric_error::out_of_range })
                context.error(floating_string, { "Floating point literal is too large" });
            else
                utl::always_assert(floating.has_value());

            std::string_view const extraneous_suffix = context.extract(is_alpha);
            if (!extraneous_suffix.empty()) {
                context.error(extraneous_suffix, { "Erroneous floating point literal alphabetic suffix" });
            }

            return token_maker(compiler::Floating { floating.value() }, Token::Type::floating_literal);
        }
        else {
            auto integer = liblex::parse_integer(first_digit_sequence, base.value_or(10));
            if (integer == tl::unexpected { liblex::Numeric_error::out_of_range })
                context.error(first_digit_sequence, { "Integer literal is too large" });
            else
                utl::always_assert(integer.has_value());

            std::string_view const suffix = context.extract(is_alpha);
            if (suffix.empty()) {
                return token_maker(compiler::Integer { integer.value() }, Token::Type::integer_literal);
            }
            else if (suffix != "e" && suffix != "E") {
                context.error(suffix, { "Erroneous integer literal alphabetic suffix" });
            }
            else if (context.try_consume('-')) {
                context.consume(is_digit10);
                context.error({ "An integer literal may not have a negative exponent" });
            }
            else if (context.is_finished() || !is_digit10(context.current())) {
                context.error({ "Expected an exponent" });
            }
            else {
                std::string_view const exponent_digit_sequence = context.extract(is_digit10);
                assert(!exponent_digit_sequence.empty());
                error_if_trailing_separator(exponent_digit_sequence);

                std::string_view const extraneous_suffix = context.extract(is_alpha);
                if (!extraneous_suffix.empty()) {
                    context.error(extraneous_suffix, { "Erroneous integer literal alphabetic suffix" });
                }

                auto const exponent = liblex::parse_integer(exponent_digit_sequence);
                if (exponent == tl::unexpected { liblex::Numeric_error::out_of_range })
                    context.error(exponent_digit_sequence, { "Exponent is too large" });
                else
                    utl::always_assert(exponent.has_value());

                auto const result = liblex::apply_scientific_exponent(*integer, *exponent);
                if (result == tl::unexpected { liblex::Numeric_error::out_of_range })
                    context.error({ anchor, context.pointer() },
                        { "Integer literal is too large after applying scientific exponent" });
                else
                    utl::always_assert(result.has_value());

                return token_maker(compiler::Integer { result.value() }, Token::Type::integer_literal);
            }
        }
    }

    auto extract_token(liblex::Context& context) -> Token {
        assert(!context.is_finished());
        Token_maker const token { context };
        try {
            switch (char const current = context.current()) {
            case '(': context.advance(); return token({}, Token::Type::paren_open);
            case ')': context.advance(); return token({}, Token::Type::paren_close);
            case '{': context.advance(); return token({}, Token::Type::brace_open);
            case '}': context.advance(); return token({}, Token::Type::brace_close);
            case '[': context.advance(); return token({}, Token::Type::bracket_open);
            case ']': context.advance(); return token({}, Token::Type::bracket_close);
            case ';': context.advance(); return token({}, Token::Type::semicolon);
            case ',': context.advance(); return token({}, Token::Type::comma);
            case '\'': return extract_character_literal(token, context);
            case '\"': return extract_string_literal(token, context);
            default:
                if (is_identifier_head(current))
                    return extract_identifier(token, context);
                else if (is_operator(current))
                    return extract_operator(token, context);
                else if (is_digit10(current))
                    return extract_numeric(token, context);
                else
                    context.error(context.extract(is_invalid_character), { "Unable to extract lexical token" });
            }
        }
        catch (liblex::Token_extraction_failure const&) {
            return token({}, Token::Type::error);
        }
    }

}


auto kieli::lex(Lex_arguments&& lex_arguments) -> Lex_result {
    utl::always_assert(lex_arguments.source->string().data() != nullptr);
    auto tokens = utl::vector_with_capacity<Token>(1024);
    std::string_view current_trivia;

    liblex::Context context { lex_arguments.source, lex_arguments.compilation_info };
    for (;;) {
        current_trivia = extract_comments_and_whitespace(context);
        if (context.is_finished()) break;
        Token token = extract_token(context);
        token.preceding_trivia = current_trivia;
        tokens.push_back(token);
    }

    Token end_of_input_token = make_token(
        context.position(),
        context.position(),
        lex_arguments.source->string(),
        Token::Variant {},
        Token::Type::end_of_input,
        lex_arguments.source);
    end_of_input_token.preceding_trivia = current_trivia;
    tokens.push_back(std::move(end_of_input_token));

    return Lex_result {
        .compilation_info = std::move(lex_arguments.compilation_info),
        .tokens           = std::move(tokens),
    };
}
