#include "utl/utilities.hpp"
#include "lexer.hpp"


namespace {

    using Token = compiler::Lexical_token;

    struct Lex_context {
        std::vector<Token>             tokens;
        utl::Source                    source;
        utl::diagnostics::Builder      diagnostics;
        compiler::Program_string_pool& string_pool;
        char const                   * start;
        char const                   * stop;

        struct [[nodiscard]] State {
            char const* pointer = nullptr;
            utl::Usize  line    = 1;
            utl::Usize  column  = 1;
        };

        State token_start;
    private:
        State state;

        auto update_location(char const c) noexcept -> void {
            if (c == '\n') {
                ++state.line;
                state.column = 1;
            }
            else {
                ++state.column;
            }
        }

        [[nodiscard]]
        auto source_view_for(std::string_view const view) const
            noexcept -> utl::Source_view
        {
            utl::Source_position start_pos;
            for (char const* ptr = start; ptr != view.data(); ++ptr) {
                start_pos.increment_with(*ptr);
            }

            utl::Source_position stop_pos = start_pos;
            for (char const c : view) {
                stop_pos.increment_with(c);
            }

            return utl::Source_view { view, start_pos, stop_pos };
        }
    public:
        explicit Lex_context(
            utl::Source                  && source,
            utl::diagnostics::Builder    && diagnostics,
            compiler::Program_string_pool&  string_pool
        ) noexcept
            : source      { std::move(source) }
            , diagnostics { std::move(diagnostics) }
            , string_pool { string_pool }
            , start       { this->source.string().data() }
            , stop        { start + this->source.string().size() }
            , token_start { .pointer = start }
            , state       { .pointer = start }
        {
            tokens.reserve(1024);
        }

        auto current_state() const noexcept -> State {
            return state;
        }
        auto restore(State const old) noexcept -> void {
            state = old;
        }
        auto is_finished() const noexcept -> bool {
            return state.pointer == stop;
        }
        auto advance(utl::Usize const distance = 1) noexcept -> void {
            for (utl::Usize i = 0; i != distance; ++i) {
                update_location(*state.pointer++);
            }
        }
        auto current_pointer() const noexcept -> char const* {
            return state.pointer;
        }
        auto current() const noexcept -> char {
            return *state.pointer;
        }
        auto extract_current() noexcept -> char {
            update_location(*state.pointer);
            return *state.pointer++;
        }
        auto consume(std::predicate<char> auto const predicate) noexcept -> void {
            for (; (state.pointer != stop) && predicate(*state.pointer); ++state.pointer) {
                update_location(*state.pointer);
            }
        }
        auto extract(std::predicate<char> auto const predicate) noexcept -> std::string_view {
            auto* const anchor = state.pointer;
            consume(predicate);
            return { anchor, state.pointer };
        }
        auto try_consume(char const c) noexcept -> bool {
            assert(c != '\n');

            if (*state.pointer == c) {
                ++state.column;
                ++state.pointer;
                return true;
            }
            else {
                return false;
            }
        }
        auto try_consume(std::string_view const string) noexcept -> bool {
            char const* ptr = state.pointer;

            for (char const character : string) {
                assert(character != '\n');
                if (*ptr++ != character) {
                    return false;
                }
            }

            state.pointer = ptr;
            state.column += string.size();

            return true;
        }
        auto success(Token::Type const type, Token::Variant&& value = std::monostate {})
            noexcept -> std::true_type
        {
            tokens.emplace_back(
                std::move(value),
                type,
                utl::Source_view {
                    std::string_view    { token_start.pointer, state      .pointer },
                    utl::Source_position { token_start.line   , token_start.column  },
                    utl::Source_position { state      .line   ,       state.column  }
                }
            );
            return {};
        }

        auto make_string(std::string_view const string) -> compiler::String {
            return string_pool.literals.make(string);
        }
        auto make_identifier(std::string_view const string) -> compiler::Identifier {
            return string_pool.identifiers.make(string);
        }
        auto make_new_identifier(std::string_view const string) -> compiler::Identifier {
            return string_pool.identifiers.make_guaranteed_new_string(string);
        }

        [[noreturn]]
        auto error(
            std::string_view                   const view,
            utl::diagnostics::Message_arguments const arguments) -> void
        {
            diagnostics.emit_simple_error(arguments.add_source_info(source, source_view_for(view)));
        }
        [[noreturn]]
        auto error(
            char const*                        const location,
            utl::diagnostics::Message_arguments const arguments) -> void
        {
            error({ location, location + 1 }, arguments);
        }

        template <utl::trivial T>
        struct Parse_result {
            std::from_chars_result result;
            char const* start_position;
            T private_value;

            auto get() const noexcept -> T {
                assert(result.ec == std::errc {});
                return private_value;
            }
            auto did_parse() const noexcept -> bool {
                return result.ptr != start_position;
            }
            auto is_too_large() const noexcept -> bool {
                return result.ec == std::errc::result_out_of_range;
            }
            auto was_non_numeric() const noexcept -> bool {
                return result.ec == std::errc::invalid_argument;
            }
        };

        template <utl::trivial T>
        auto parse(std::same_as<int> auto const... args) noexcept -> Parse_result<T> {
            static_assert(
                sizeof...(args) <= 1,
                "The parameter pack is used for the optional base parameter only"
            );
            T value;
            auto const result = std::from_chars(state.pointer, stop, value, args...);
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
                        if (context.is_finished()) {
                            context.error(string_start, { .message = "Unterminating string within comment block" });
                        }
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

        if (!is_valid_head(context.current())) {
            return false;
        }

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
            { "typeof"             , Token::Type::typeof             },
            { "addressof"          , Token::Type::addressof          },
            { "unsafe_dereference" , Token::Type::unsafe_dereference },
            { "mov"                , Token::Type::mov                },
            { "meta"               , Token::Type::meta               },
            { "where"              , Token::Type::where              },
            { "immut"              , Token::Type::immut              },
            { "dyn"                , Token::Type::dyn                },
            { "pub"                , Token::Type::pub                },
            { "macro"              , Token::Type::macro              },
        });

        std::string_view const view = context.extract(is_identifier);

        if (std::ranges::all_of(view, is_one_of<'_'>)) {
            return context.success(Token::Type::underscore);
        }

        if (view == "true") {
            return context.success(Token::Type::boolean, true);
        }
        else if (view == "false") {
            return context.success(Token::Type::boolean, false);
        }

        for (auto const [keyword, keyword_type] : options) {
            if (view == keyword) {
                return context.success(keyword_type);
            }
        }

        return context.success(
            is_upper(view[view.find_first_not_of('_')])
                ? Token::Type::upper_name
                : Token::Type::lower_name,
            context.string_pool.identifiers.make(view)
        );
    }

    auto extract_operator(Lex_context& context) -> bool {
        static constexpr auto is_operator = is_one_of<
            '+', '-', '*', '/', '.', '|', '<', '=', '>', ':',
            '!', '?', '#', '%', '&', '^', '~', '$', '@', '\\'
        >;
        
        std::string_view const view = context.extract(is_operator);
        if (view.empty()) {
            return false;
        }

        static constexpr auto clashing = std::to_array<utl::Pair<std::string_view, Token::Type>>({
            { "."  , Token::Type::dot          },
            { ":"  , Token::Type::colon        },
            { "::" , Token::Type::double_colon },
            { "|"  , Token::Type::pipe         },
            { "="  , Token::Type::equals       },
            { "&"  , Token::Type::ampersand    },
            { "*"  , Token::Type::asterisk     },
            { "+"  , Token::Type::plus         },
            { "?"  , Token::Type::question     },
            { "\\" , Token::Type::lambda       },
            { "<-" , Token::Type::left_arrow   },
            { "->" , Token::Type::right_arrow  },
            { "???", Token::Type::hole         },
        });

        for (auto [punctuation, punctuation_type] : clashing) {
            if (view == punctuation) {
                return context.success(punctuation_type);
            }
        }

        return context.success(Token::Type::operator_name, context.string_pool.identifiers.make(view));
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
        int base = 10;
        
        auto const state = context.current_state();

        if (context.try_consume('0')) {
            switch (context.extract_current()) {
            case 'b': base = 2; break;
            case 'q': base = 4; break;
            case 'o': base = 8; break;
            case 'd': base = 12; break;
            case 'x': base = 16; break;
            default:
                context.restore(state);
                return base;
            }

            if (context.try_consume('-')) {
                context.error(state.pointer - 1, { "'-' must be applied before the base specifier" });
            }
        }

        return base;
    }

    auto apply_scientific_coefficient(utl::Isize& integer, char const* anchor, Lex_context& context) -> void {
        if (context.try_consume('e') || context.try_consume('E')) {
            auto const exponent = context.parse<utl::Isize>();

            if (exponent.did_parse()) {
                if (exponent.is_too_large()) {
                    context.error({ exponent.start_position, exponent.result.ptr }, { "Exponent is too large" });
                }
                if (exponent.get() < 0) [[unlikely]] {
                    context.error(context.current_pointer(), {
                        .message   = "Negative exponent",
                        .help_note = "use a floating point literal if this was intended"
                    });
                }
                else if (utl::digit_count(integer) + exponent.get() >= std::numeric_limits<utl::Isize>::digits10) {
                    context.error(
                        { anchor, exponent.result.ptr },
                        { "Integer literal is too large after applying scientific coefficient" }
                    );
                }

                auto value = exponent.get();
                utl::Isize coefficient = 1;
                while (value--) {
                    coefficient *= 10;
                }

                integer *= coefficient;
                context.advance(utl::unsigned_distance(context.current_pointer(), exponent.result.ptr));
            }
            else if (exponent.was_non_numeric()) {
                context.error(exponent.start_position, { "Expected an exponent" });
            }
            else {
                // Should be unreachable because there either is an exponent or there isn't
                utl::abort();
            }
        }
    }

    auto extract_numeric(Lex_context& context) -> bool {
        auto const state    = context.current_state();
        auto const negative = context.try_consume('-');
        auto const base     = extract_numeric_base(context);
        auto const integer  = context.parse<utl::Isize>(base);

        if (integer.was_non_numeric()) {
            if (base == 10) {
                context.restore(state);
                return false;
            }
            else {
                context.error(
                    { state.pointer, 2 }, // view of the base specifier
                    {
                        .message = "Expected an integer literal after the base-{} specifier",
                        .message_arguments = std::make_format_args(base)
                    }
                );
            }
        }
        else if (integer.is_too_large()) {
            context.error(
                { state.pointer, integer.result.ptr },
                { "Integer literal is too large" }
            );
        }
        else if (!integer.did_parse()) {
            // Should be unreachable because `integer.was_non_numeric`
            // returned false, which means at one digiti was encountered
            utl::abort();
        }

        if (negative && integer.get() < 0) {
            context.error(state.pointer + 1, { "Only one '-' may be applied" });
        }

        auto const is_tuple_member_index = [&] {
            // If the numeric literal is preceded by '.', then don't attempt to
            // parse a float. This allows nested tuple member-access: tuple.0.0
            return state.pointer != context.start
                ? state.pointer[-1] == '.'
                : false;
        };

        if (*integer.result.ptr == '.' && !is_tuple_member_index()) {
            if (base != 10) {
                context.error({ state.pointer, 2 }, { "Float literals must be base-10" });
            }

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
                    { "Floating-point literal is too large" }
                );
            }
            else {
                context.advance(utl::unsigned_distance(context.current_pointer(), floating.result.ptr));
                return context.success(Token::Type::floating, floating.get());
            }
        }

        auto value = integer.get();
        value *= (negative ? -1 : 1);

        context.advance(utl::unsigned_distance(context.current_pointer(), integer.result.ptr));
        apply_scientific_coefficient(value, state.pointer, context);
        return context.success(Token::Type::integer, value);
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

            if (c == '\0') {
                context.error(anchor, { "Unterminating character literal" });
            }
            else if (c == '\\') {
                c = handle_escape_sequence(context);
            }

            if (context.try_consume('\'')) {
                return context.success(Token::Type::character, c);
            }
            else {
                context.error(context.current_pointer(), { "Expected a closing single-quote" });
            }
        }
        else {
            return false;
        }
    }

    auto extract_string(Lex_context& context) -> bool {
        char const* const anchor = context.current_pointer();

        if (context.try_consume('"')) {
            thread_local std::string string = utl::string_with_capacity(50);
            string.clear();

            for (;;) {
                switch (char c = context.extract_current()) {
                case '\0':
                    context.error(anchor, { "Unterminating string literal" });
                case '"':
                    if (!context.tokens.empty() && context.tokens.back().type == Token::Type::string) {
                        // Concatenate adjacent string literals

                        string.insert(0, context.tokens.back().as_string().view());
                        context.tokens.pop_back(); // Pop the previous string
                    }
                    return context.success(Token::Type::string, context.string_pool.literals.make(string));
                case '\\':
                    c = handle_escape_sequence(context);
                    [[fallthrough]];
                default:
                    string.push_back(c);
                }
            }
        }
        else {
            return false;
        }
    }

}


auto compiler::lex(
    utl::Source                                 && source,
    Program_string_pool                         & string_pool,
    utl::diagnostics::Builder::Configuration const diagnostics_configuration) -> Lex_result
{
    utl::diagnostics::Builder diagnostics { diagnostics_configuration };

    if (source.string().empty()) {
        Token const end_of_input {
            .type = Token::Type::end_of_input,
            .source_view { source.string(), { 0, 0 }, { 0, 0 } }
        };
        return {
            .tokens      = std::vector { end_of_input },
            .source      = std::move(source),
            .diagnostics = std::move(diagnostics),
            .string_pool = string_pool
        };
    }

    Lex_context context { std::move(source), std::move(diagnostics), string_pool };

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

        if (!did_extract) {
            if (context.is_finished()) {
                auto const state = context.current_state();

                context.tokens.push_back(
                    Token {
                        .type        = Token::Type::end_of_input,
                        .source_view = utl::Source_view {
                            std::string_view { context.stop, context.stop },
                            { state.line, state.column },
                            { state.line, state.column }
                        }
                    }
                );

                return Lex_result {
                    .tokens      = std::move(context.tokens),
                    .source      = std::move(context.source),
                    .diagnostics = std::move(context.diagnostics),
                    .string_pool = string_pool
                };
            }
            else {
                context.error(context.current_pointer(), { "Syntax error; unable to extract lexical token" });
            }
        }
    }
}