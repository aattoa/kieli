#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <liblex/lex.hpp>


namespace {

    using Token = kieli::Lexical_token;

    struct Lex_context {
        compiler::Compilation_info compilation_info;
        std::vector<Token>         tokens;
        utl::Wrapper<utl::Source>  source;
        char const*                start;
        char const*                stop;

        struct [[nodiscard]] State {
            char const* pointer = nullptr;
            utl::Usize  line    = 1;
            utl::Usize  column  = 1;
        };

        State token_start;
    private:
        State state;

        auto update_location(char const c) noexcept -> void {
            if (c == '\n')
                ++state.line, state.column = 1;
            else
                ++state.column;
        }

        [[nodiscard]]
        auto source_view_for(std::string_view const view) const
            noexcept -> utl::Source_view
        {
            utl::Source_position start_pos;
            for (char const* ptr = start; ptr != view.data(); ++ptr)
                start_pos.advance_with(*ptr);

            utl::Source_position stop_pos = start_pos;
            for (char const c : view)
                stop_pos.advance_with(c);

            return utl::Source_view { source, view, start_pos, stop_pos };
        }
    public:
        explicit Lex_context(
            utl::Wrapper<utl::Source> const source,
            compiler::Compilation_info&& compilation_info) noexcept
            : compilation_info { std::move(compilation_info) }
            , source           { source }
            , start            { source->string().data() }
            , stop             { start + source->string().size() }
            , token_start      { .pointer = start }
            , state            { .pointer = start }
        {
            tokens.reserve(1024);
        }

        auto current_state() const noexcept -> State {
            return state;
        }
        auto restore(State const old) noexcept -> void {
            state = old;
        }
        [[nodiscard]]
        auto is_finished() const noexcept -> bool {
            return state.pointer == stop;
        }
        auto advance(utl::Usize const distance = 1) noexcept -> void {
            for (utl::Usize i = 0; i != distance; ++i)
                update_location(*state.pointer++);
        }
        [[nodiscard]]
        auto current_pointer() const noexcept -> char const* {
            return state.pointer;
        }
        [[nodiscard]]
        auto current() const noexcept -> char {
            return *state.pointer;
        }
        [[nodiscard]]
        auto extract_current() noexcept -> char {
            update_location(*state.pointer);
            return *state.pointer++;
        }
        auto consume(std::predicate<char> auto const predicate) noexcept -> void {
            for (; (state.pointer != stop) && predicate(*state.pointer); ++state.pointer)
                update_location(*state.pointer);
        }
        [[nodiscard]]
        auto extract(std::predicate<char> auto const predicate) noexcept -> std::string_view {
            char const* const anchor = state.pointer;
            consume(predicate);
            return { anchor, state.pointer };
        }
        [[nodiscard]]
        auto try_consume(char const c) noexcept -> bool {
            assert(c != '\n');
            bool const eq = current() == c;
            if (eq) {
                ++state.column;
                ++state.pointer;
            }
            return eq;
        }
        [[nodiscard]]
        auto try_consume(std::string_view const string) noexcept -> bool {
            char const* ptr = state.pointer;

            for (char const character : string) {
                assert(character != '\n');
                if (*ptr++ != character)
                    return false;
            }

            state.pointer = ptr;
            state.column += string.size();

            return true;
        }
        [[nodiscard]]
        auto success(Token::Type const type, Token::Variant&& value = std::monostate {})
            -> std::true_type
        {
            tokens.push_back(Token {
                .value = value,
                .type  = type,
                .source_view = utl::Source_view {
                    source,
                    std::string_view     { token_start.pointer, state      .pointer },
                    utl::Source_position { token_start.line   , token_start.column  },
                    utl::Source_position { state      .line   ,       state.column  }
                }
            });
            return {};
        }

        auto make_string(std::string_view const string) -> compiler::String {
            return compilation_info.get()->string_literal_pool.make(string);
        }
        auto make_identifier(std::string_view const string) -> compiler::Identifier {
            return compilation_info.get()->identifier_pool.make(string);
        }
        auto make_new_identifier(std::string_view const string) -> compiler::Identifier {
            return compilation_info.get()->identifier_pool.make_guaranteed_new_string(string);
        }

        [[noreturn]]
        auto error(
            std::string_view                    const view,
            utl::diagnostics::Message_arguments const arguments) -> void
        {
            compilation_info.get()->diagnostics.emit_error(arguments.add_source_view(source_view_for(view)));
        }
        [[noreturn]]
        auto error(
            char const*                         const location,
            utl::diagnostics::Message_arguments const arguments) -> void
        {
            assert(location != nullptr);
            error({ location, location != stop ? location + 1 : location }, arguments);
        }

        template <utl::trivial T>
        struct Parse_result {
            std::from_chars_result result;
            char const*            start_position;
            T                      private_value;

            [[nodiscard]]
            auto get() const noexcept -> T {
                assert(result.ec == std::errc {});
                return private_value;
            }
            [[nodiscard]]
            auto did_parse() const noexcept -> bool {
                return result.ptr != start_position;
            }
            [[nodiscard]]
            auto is_too_large() const noexcept -> bool {
                return result.ec == std::errc::result_out_of_range;
            }
            [[nodiscard]]
            auto was_non_numeric() const noexcept -> bool {
                return result.ec == std::errc::invalid_argument;
            }
        };

        template <class T> [[nodiscard]]
        auto parse(std::same_as<int> auto const... base) noexcept -> Parse_result<T> {
            static_assert(sizeof...(base) <= 1,
                "The parameter pack is used for the optional base parameter only");
            T value;
            auto const result = std::from_chars(state.pointer, stop, value, base...);
            return { result, state.pointer, value };
        }
    };


    template <char... cs>
    constexpr auto is_one_of(char const c) noexcept -> bool {
        return ((c == cs) || ...);
    }
    template <char... cs>
    constexpr auto is_not_one_of(char const c) noexcept -> bool {
        return ((c != cs) && ...);
    }
    template <char a, char b>
        requires (a < b)
    constexpr auto is_in_range(char const c) noexcept -> bool {
        return a <= c && c <= b;
    }
    template <std::predicate<char> auto... predicates>
    constexpr auto satisfies_one_of(char const c) noexcept -> bool {
        return (predicates(c) || ...);
    }

    constexpr auto is_space = is_one_of<' ', '\t', '\n'>;
    constexpr auto is_digit = is_in_range<'0', '9'>;
    constexpr auto is_lower = is_in_range<'a', 'z'>;
    constexpr auto is_upper = is_in_range<'A', 'Z'>;
    constexpr auto is_alpha = satisfies_one_of<is_lower, is_upper>;
    constexpr auto is_alnum = satisfies_one_of<is_alpha, is_digit>;


    auto skip_comments_and_whitespace(Lex_context& context) -> void {
        context.consume(is_space);

        auto const state = context.current_state();

        if (context.try_consume('/')) {
            switch (context.extract_current()) {
            case '/':
                context.consume(is_not_one_of<'\n'>);
                break;
            case '*':
            {
                for (utl::Usize depth = 1; depth != 0; ) {
                    if (context.try_consume('"')) {
                        char const* const string_start = context.current_pointer() - 1;
                        context.consume(is_not_one_of<'"'>);
                        if (context.is_finished())
                            context.error(string_start, { .message = "Unterminating string within comment block" });
                        context.advance();
                    }
                    
                    if (context.try_consume("*/")) {
                        --depth;
                    }
                    else if (context.try_consume("/*")) {
                        ++depth;
                    }
                    else if (context.is_finished()) {
                        context.error(state.pointer, {
                            .message   = "Unterminating comment block",
                            .help_note = "Comments starting with '/*' can be terminated with '*/'"
                        });
                    }
                    else {
                        context.advance();
                    }
                }
                break;
            }
            default:
                context.restore(state);
                return;
            }

            skip_comments_and_whitespace(context);
        }
    }


    auto extract_identifier(Lex_context& context) -> bool {
        static constexpr auto is_valid_head = satisfies_one_of<is_alpha, is_one_of<'_'>>;
        static constexpr auto is_identifier = satisfies_one_of<is_alnum, is_one_of<'_', '\''>>;

        if (!is_valid_head(context.current()))
            return false;

        static constexpr auto options = std::to_array<utl::Pair<std::string_view, Token::Type>>({
            { "let"                , Token::Type::let                },
            { "mut"                , Token::Type::mut                },
            { "if"                 , Token::Type::if_                },
            { "else"               , Token::Type::else_              },
            { "elif"               , Token::Type::elif               },
            { "for"                , Token::Type::for_               },
            { "in"                 , Token::Type::in                 },
            { "while"              , Token::Type::while_             },
            { "loop"               , Token::Type::loop               },
            { "continue"           , Token::Type::continue_          },
            { "break"              , Token::Type::break_             },
            { "match"              , Token::Type::match              },
            { "ret"                , Token::Type::ret                },
            { "discard"            , Token::Type::discard            },
            { "fn"                 , Token::Type::fn                 },
            { "as"                 , Token::Type::as                 },
            { "I8"                 , Token::Type::i8_type            },
            { "I16"                , Token::Type::i16_type           },
            { "I32"                , Token::Type::i32_type           },
            { "I64"                , Token::Type::i64_type           },
            { "U8"                 , Token::Type::u8_type            },
            { "U16"                , Token::Type::u16_type           },
            { "U32"                , Token::Type::u32_type           },
            { "U64"                , Token::Type::u64_type           },
            { "Float"              , Token::Type::floating_type      },
            { "Char"               , Token::Type::character_type     },
            { "Bool"               , Token::Type::boolean_type       },
            { "String"             , Token::Type::string_type        },
            { "self"               , Token::Type::lower_self         },
            { "Self"               , Token::Type::upper_self         },
            { "enum"               , Token::Type::enum_              },
            { "struct"             , Token::Type::struct_            },
            { "class"              , Token::Type::class_             },
            { "inst"               , Token::Type::inst               },
            { "impl"               , Token::Type::impl               },
            { "alias"              , Token::Type::alias              },
            { "namespace"          , Token::Type::namespace_         },
            { "import"             , Token::Type::import_            },
            { "export"             , Token::Type::export_            },
            { "module"             , Token::Type::module_            },
            { "sizeof"             , Token::Type::sizeof_            },
            { "typeof"             , Token::Type::typeof_            },
            { "addressof"          , Token::Type::addressof          },
            { "unsafe_dereference" , Token::Type::unsafe_dereference },
            { "mov"                , Token::Type::mov                },
            { "meta"               , Token::Type::meta               },
            { "where"              , Token::Type::where              },
            { "immut"              , Token::Type::immut              },
            { "dyn"                , Token::Type::dyn                },
            { "pub"                , Token::Type::pub                },
            { "macro"              , Token::Type::macro              },
            { "global"             , Token::Type::global             },
        });

        std::string_view const view = context.extract(is_identifier);

        if (ranges::all_of(view, is_one_of<'_'>))
            return context.success(Token::Type::underscore);

        if (view == "true")
            return context.success(Token::Type::boolean, kieli::Boolean { true });
        else if (view == "false")
            return context.success(Token::Type::boolean, kieli::Boolean { false });

        for (auto const [keyword, keyword_type] : options) { // NOLINT
            if (view == keyword)
                return context.success(keyword_type);
        }

        return context.success(
            is_upper(view[view.find_first_not_of('_')])
                ? Token::Type::upper_name
                : Token::Type::lower_name,
            context.compilation_info.get()->identifier_pool.make(view));
    }

    auto extract_operator(Lex_context& context) -> bool {
        static constexpr auto is_operator = is_one_of<
            '+', '-', '*', '/', '.', '|', '<', '=', '>', ':',
            '!', '?', '#', '%', '&', '^', '~', '$', '@', '\\'>;
        
        std::string_view const view = context.extract(is_operator);
        if (view.empty())
            return false;

        static constexpr auto clashing = std::to_array<utl::Pair<std::string_view, Token::Type>>({
            { "."     , Token::Type::dot          },
            { ":"     , Token::Type::colon        },
            { "::"    , Token::Type::double_colon },
            { "|"     , Token::Type::pipe         },
            { "="     , Token::Type::equals       },
            { "&"     , Token::Type::ampersand    },
            { "*"     , Token::Type::asterisk     },
            { "+"     , Token::Type::plus         },
            { "?"     , Token::Type::question     },
            { "\\"    , Token::Type::lambda       },
            { "<-"    , Token::Type::left_arrow   },
            { "->"    , Token::Type::right_arrow  },
            { "\?\?\?", Token::Type::hole         },
        });

        for (auto [punctuation, punctuation_type] : clashing) {
            if (view == punctuation)
                return context.success(punctuation_type);
        }

        return context.success(Token::Type::operator_name, context.compilation_info.get()->identifier_pool.make(view));
    }

    auto extract_punctuation(Lex_context& context) -> bool {
        static constexpr auto options = std::to_array<utl::Pair<char, Token::Type>>({
            { ',', Token::Type::comma         },
            { ';', Token::Type::semicolon     },
            { '(', Token::Type::paren_open    },
            { ')', Token::Type::paren_close   },
            { '{', Token::Type::brace_open    },
            { '}', Token::Type::brace_close   },
            { '[', Token::Type::bracket_open  },
            { ']', Token::Type::bracket_close },
        });

        char const current = context.current();

        for (auto const [character, punctuation_type] : options) {
            if (character == current) {
                context.advance();
                return context.success(punctuation_type);
            }
        }
        return false;
    }


    auto extract_numeric_base(Lex_context& context) -> int {
        int base = 10; // NOLINT
        
        auto const state = context.current_state();

        if (context.try_consume('0')) {
            switch (context.extract_current()) {
            case 'b': base = 2; break;  // NOLINT
            case 'q': base = 4; break;  // NOLINT
            case 'o': base = 8; break;  // NOLINT
            case 'd': base = 12; break; // NOLINT
            case 'x': base = 16; break; // NOLINT
            default:
                context.restore(state);
                return base;
            }

            if (context.try_consume('-'))
                context.error(state.pointer - 1, { "'-' must be applied before the base specifier" });
        }

        return base;
    }

    [[nodiscard]]
    auto apply_scientific_coefficient(
        char const* const anchor,
        utl::Usize  const integer,
        Lex_context&      context) -> utl::Usize
    {
        if (!context.try_consume('e') && !context.try_consume('E'))
            return integer;

        auto const exponent = context.parse<utl::Isize>();

        if (exponent.did_parse()) {
            if (exponent.is_too_large()) {
                context.error({ exponent.start_position, exponent.result.ptr }, { "Exponent is too large" });
            }
            else if (exponent.get() < 0) {
                context.error(context.current_pointer(), {
                    .message   = "Negative exponent",
                    .help_note = "use a floating point literal if this was intended" });
            }

            try {
                utl::Safe_usize checked_value    = integer;
                utl::Safe_isize checked_exponent = exponent.get();

                while (checked_exponent--)
                    checked_value *= 10; // NOLINT

                context.advance(utl::unsigned_distance(context.current_pointer(), exponent.result.ptr));
                return checked_value.get();
            }
            catch (utl::Safe_integer_error const&) {
                context.error(
                    { anchor, exponent.result.ptr },
                    { "Integer literal is too large after applying scientific coefficient" });
            }
        }
        else if (exponent.was_non_numeric()) {
            context.error(exponent.start_position, { "Expected an exponent" });
        }
        else {
            // Should be unreachable because there either is an exponent or there isn't
            utl::unreachable();
        }
    }

    auto extract_numeric(Lex_context& context) -> bool {
        auto const state    = context.current_state();
        auto const negative = context.try_consume('-');
        auto const base     = extract_numeric_base(context);
        auto const integer  = context.parse<utl::Usize>(base);

        if (integer.was_non_numeric()) {
            if (base == 10) { // NOLINT
                context.restore(state);
                return false;
            }
            else {
                context.error(
                    std::string_view { state.pointer, 2 }, // view of the base specifier
                    { "Expected an integer literal after the base-{} specifier"_format(base) });
            }
        }
        else if (integer.is_too_large()) {
            context.error(
                { state.pointer, integer.result.ptr },
                { "Integer literal is too large" });
        }
        else if (!integer.did_parse()) {
            // Should be unreachable because `integer.was_non_numeric`
            // returned false, which means at one digit was encountered
            utl::unreachable();
        }

        auto const is_tuple_member_index = [&] {
            // If the numeric literal is preceded by '.', then don't attempt to
            // parse a float. This allows nested tuple member-access: tuple.0.0
            return state.pointer != context.start && state.pointer[-1] == '.';
        };

        if (*integer.result.ptr == '.' && !is_tuple_member_index()) {
            if (base != 10) // NOLINT
                context.error({ state.pointer, 2 }, { "Float literals must be base-10" });

            // Go back to the beginning of the digit sequence
            context.restore(state);

            // Parse a floating point number instead
            auto const floating = context.parse<utl::Float>();

            // The parse should always succeed because an integer followed by a dot
            // is already a valid textual representation of floating point number
            utl::always_assert(floating.did_parse());

            if (floating.is_too_large()) {
                context.error(
                    { state.pointer, floating.result.ptr },
                    { "Floating-point literal is too large" });
            }
            else {
                context.advance(utl::unsigned_distance(context.current_pointer(), floating.result.ptr));
                return context.success(Token::Type::floating, kieli::Floating { floating.get() });
            }
        }

        context.advance(utl::unsigned_distance(context.current_pointer(), integer.result.ptr));
        utl::Usize const result = apply_scientific_coefficient(state.pointer, integer.private_value, context);

        if (std::in_range<utl::Isize>(result)) {
            auto const signed_representation = // NOLINT
                utl::safe_cast<utl::Isize>(result);

            if (negative)
                return context.success(
                    Token::Type::signed_integer,
                    kieli::Signed_integer { -1 * signed_representation });
            else
                return context.success(
                    Token::Type::integer_of_unknown_sign,
                    kieli::Integer_of_unknown_sign { signed_representation });
        }
        else {
            if (!negative)
                return context.success(
                    Token::Type::unsigned_integer,
                    kieli::Unsigned_integer { result });
            else
                context.error(
                    { state.pointer, integer.result.ptr },
                    { "Integer literal is too large to be a signed integer" });
        }
    }


    auto handle_escape_sequence(Lex_context& context) -> char {
        char const* const anchor = context.current_pointer();
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
        case '\0':
            context.error(anchor, { "Expected an escape sequence, but found the end of input" });
        default:
            context.error(anchor, { "Unrecognized escape sequence" });
        }
    }


    auto extract_character(Lex_context& context) -> bool {
        char const* const anchor = context.current_pointer();

        if (context.try_consume('\'')) {
            char c = context.extract_current();

            if (c == '\0')
                context.error(anchor, { "Unterminating character literal" });
            else if (c == '\\')
                c = handle_escape_sequence(context);

            if (context.try_consume('\''))
                return context.success(Token::Type::character, kieli::Character { c });
            else
                context.error(context.current_pointer(), { "Expected a closing single-quote" });
        }
        return false;
    }

    auto extract_string(Lex_context& context) -> bool {
        if (!context.try_consume('"'))
            return false;

        char const* const anchor = context.current_pointer();

        static thread_local std::string string = utl::string_with_capacity(50);
        string.clear();

        for (;;) {
            if (context.is_finished())
                context.error(anchor, { "Unterminating string literal" });

            switch (char c = context.extract_current()) {
            case '\0':
                utl::abort();
            case '"':
                if (!context.tokens.empty() && context.tokens.back().type == Token::Type::string) {
                    // Concatenate adjacent string literals
                    string.insert(0, context.tokens.back().as_string().view());
                    context.tokens.pop_back(); // Pop the previous string
                }
                return context.success(Token::Type::string, context.compilation_info.get()->string_literal_pool.make(string));
            case '\\':
                c = handle_escape_sequence(context);
                [[fallthrough]];
            default:
                string.push_back(c);
            }
        }
    }

}


auto kieli::lex(Lex_arguments&& lex_arguments) -> Lex_result {
    if (lex_arguments.source->string().empty()) {
        Token const end_of_input {
            .type        = Token::Type::end_of_input,
            .source_view = utl::Source_view { lex_arguments.source, lex_arguments.source->string(), {}, {} }
        };
        return {
            .compilation_info = std::move(lex_arguments.compilation_info),
            .tokens           = std::vector { end_of_input },
        };
    }

    Lex_context context { lex_arguments.source, std::move(lex_arguments.compilation_info) };

    static constexpr auto extractors = std::to_array({
        extract_identifier,
        extract_numeric,
        extract_operator,
        extract_punctuation,
        extract_string,
        extract_character,
    });

    for (;;) {
        skip_comments_and_whitespace(context);
        context.token_start = context.current_state();

        bool did_extract = false;

        for (auto extractor : extractors) {
            if (extractor(context)) {
                did_extract = true;
                break;
            }
        }

        if (did_extract)
            continue;
        else if (!context.is_finished())
            context.error(context.current_pointer(), { "Syntax error; unable to extract lexical token" });

        auto const state = context.current_state();

        context.tokens.push_back(Token {
            .type        = Token::Type::end_of_input,
            .source_view = utl::Source_view {
                lex_arguments.source,
                std::string_view { context.stop, context.stop },
                { state.line, state.column },
                { state.line, state.column },
            }
        });

        return Lex_result {
            .compilation_info = std::move(context.compilation_info),
            .tokens           = std::move(context.tokens),
        };
    }
}
