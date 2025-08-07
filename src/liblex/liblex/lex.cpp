#include <libutl/utilities.hpp>
#include <libcompiler/lsp.hpp>
#include <liblex/lex.hpp>

using namespace ki;
using namespace ki::lex;

namespace {

    constexpr auto punctuation_token_map = std::to_array<std::pair<std::string_view, Type>>({
        { ".", Type::Dot },
        { ":", Type::Colon },
        { "::", Type::Double_colon },
        { "@", Type::At },
        { "|", Type::Pipe },
        { "=", Type::Equals },
        { "&", Type::Ampersand },
        { "*", Type::Asterisk },
        { "+", Type::Plus },
        { "?", Type::Question },
        { "!", Type::Exclamation },
        { "\\", Type::Lambda },
        { "<-", Type::Left_arrow },
        { "->", Type::Right_arrow },
    });

    constexpr auto keyword_token_map = std::to_array<std::pair<std::string_view, Type>>({
        { "let", Type::Let },         { "mut", Type::Mut },       { "if", Type::If },
        { "else", Type::Else },       { "for", Type::For },       { "in", Type::In },
        { "while", Type::While },     { "loop", Type::Loop },     { "continue", Type::Continue },
        { "break", Type::Break },     { "match", Type::Match },   { "ret", Type::Ret },
        { "fn", Type::Fn },           { "enum", Type::Enum },     { "struct", Type::Struct },
        { "concept", Type::Concept }, { "impl", Type::Impl },     { "alias", Type::Alias },
        { "import", Type::Import },   { "export", Type::Export }, { "module", Type::Module },
        { "sizeof", Type::Sizeof },   { "typeof", Type::Typeof }, { "where", Type::Where },
        { "immut", Type::Immut },     { "dyn", Type::Dyn },       { "macro", Type::Macro },
        { "defer", Type::Defer },
    });

    auto find_token(auto const& map, std::string_view const string) -> std::optional<Type>
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
        return a <= c and c <= b;
    }

    template <std::predicate<char> auto... predicates>
    constexpr auto satisfies_one_of(char const c) noexcept -> bool
    {
        return (predicates(c) or ...);
    }

    constexpr auto is_whitespace = is_one_of<" \n\r\t">;
    constexpr auto is_lowercase  = is_in_range<'a', 'z'>;
    constexpr auto is_uppercase  = is_in_range<'A', 'Z'>;
    constexpr auto is_digit      = is_in_range<'0', '9'>;
    constexpr auto is_alphabetic = satisfies_one_of<is_lowercase, is_uppercase>;
    constexpr auto is_name       = satisfies_one_of<is_alphabetic, is_digit, is_one_of<"_'">>;
    constexpr auto is_name_head  = satisfies_one_of<is_alphabetic, is_one_of<"_">>;
    constexpr auto is_operator   = is_one_of<"+-*/.|<=>:!?#%&^~$@\\'">;

    constexpr auto is_valid_character
        = satisfies_one_of<is_name, is_operator, is_whitespace, is_one_of<"(){}[],'\"">>;

    auto current(State const& state) -> char
    {
        return state.text.at(state.offset);
    }

    auto is_finished(State const& state) -> bool
    {
        return state.offset == state.text.size();
    }

    void advance(State& state, std::size_t const distance = 1)
    {
        for (std::size_t i = 0; i != distance; ++i) {
            state.position = lsp::advance(state.position, state.text.at(state.offset++));
        }
    }

    auto extract_current(State& state) -> char
    {
        cpputil::always_assert(not is_finished(state));
        advance(state);
        return state.text.at(state.offset - 1);
    }

    auto try_consume(State& state, char const character) -> bool
    {
        if (is_finished(state)) {
            return false;
        }
        if (current(state) == character) {
            advance(state);
            return true;
        }
        return false;
    }

    auto try_consume(State& state, std::string_view const string) -> bool
    {
        if (state.text.substr(state.offset).starts_with(string)) {
            advance(state, string.size());
            return true;
        }
        return false;
    }

    void consume(State& state, std::predicate<char> auto const& predicate)
    {
        while (not is_finished(state) and predicate(current(state))) {
            advance(state);
        }
    }

    auto extract(State& state, std::predicate<char> auto const& predicate) -> std::string_view
    {
        auto const offset = state.offset;
        consume(state, predicate);
        return state.text.substr(offset, state.offset - offset);
    }

    auto skip_block_comment(State& state) -> std::optional<Type>
    {
        for (std::size_t depth = 1; depth != 0;) {
            if (try_consume(state, "*/")) {
                --depth;
            }
            else if (try_consume(state, "/*")) {
                ++depth;
            }
            else if (is_finished(state)) [[unlikely]] {
                return Type::Unterminated_comment;
            }
            else {
                advance(state);
            }
        }
        return std::nullopt;
    }

    auto skip_comments_and_whitespace(State& state) -> std::optional<Type>
    {
        for (;;) {
            consume(state, is_whitespace);
            if (try_consume(state, "//")) {
                consume(state, std::not_fn(is_one_of<"\n">));
            }
            else if (try_consume(state, "/*")) {
                if (auto error = skip_block_comment(state)) {
                    return error;
                }
            }
            else {
                return std::nullopt;
            }
        }
    }

    auto extract_name(State& state) -> Type
    {
        auto const string = extract(state, is_name);
        cpputil::always_assert(not string.empty());
        if (auto const type = find_token(keyword_token_map, string)) {
            return type.value();
        }
        if (string == "true" or string == "false") {
            return Type::Boolean;
        }
        auto const head = string.find_first_not_of('_');
        return head == std::string_view::npos ? Type::Underscore
             : is_uppercase(string[head])     ? Type::Upper_name
                                              : Type::Lower_name;
    }

    auto extract_operator(State& state) -> Type
    {
        auto const string = extract(state, is_operator);
        return find_token(punctuation_token_map, string).value_or(Type::Operator);
    }

    auto extract_string_literal(State& state) -> Type
    {
        cpputil::always_assert(extract_current(state) == '"');
        while (not is_finished(state)) {
            if (current(state) == '\n') [[unlikely]] {
                return Type::Unterminated_string;
            }
            char const character = extract_current(state);
            if (character == '"') {
                return Type::String;
            }
            if (character == '\\') {
                if (is_finished(state)) [[unlikely]] {
                    return Type::Unterminated_string;
                }
                advance(state);
            }
        }
        return Type::Unterminated_string;
    }

    auto extract_numeric(State& state) -> Type
    {
        bool const has_preceding_dot = state.offset != 0 and state.text.at(state.offset - 1) == '.';
        consume(state, is_name);
        if (not has_preceding_dot and try_consume(state, '.')) {
            consume(state, is_name);
            return Type::Floating;
        }
        return Type::Integer;
    }

    auto extract_token(State& state) -> Type
    {
        auto const simple = [&](Type type) {
            advance(state);
            return type;
        };
        switch (char const ch = current(state)) {
        case '(':  return simple(Type::Paren_open);
        case ')':  return simple(Type::Paren_close);
        case '{':  return simple(Type::Brace_open);
        case '}':  return simple(Type::Brace_close);
        case '[':  return simple(Type::Bracket_open);
        case ']':  return simple(Type::Bracket_close);
        case ';':  return simple(Type::Semicolon);
        case ',':  return simple(Type::Comma);
        case '\"': return extract_string_literal(state);
        default:
            if (is_name_head(ch)) {
                return extract_name(state);
            }
            if (is_operator(ch)) {
                return extract_operator(state);
            }
            if (is_digit(ch)) {
                return extract_numeric(state);
            }
            consume(state, std::not_fn(is_valid_character));
            return Type::Invalid_character;
        }
    }

} // namespace

auto ki::lex::state(std::string_view text) -> State
{
    return State { .position = {}, .offset = 0, .text = text };
}

auto ki::lex::next(State& state) -> Token
{
    skip_comments_and_whitespace(state);

    if (is_finished(state)) {
        return Token {
            .type  = Type::End_of_input,
            .range = lsp::to_range(state.position),
            .view  = utl::View { .offset = state.offset, .length = 0 },
        };
    }

    auto const position = state.position;
    auto const offset   = state.offset;
    auto const type     = extract_token(state);

    return Token {
        .type  = type,
        .range = lsp::Range(position, state.position),
        .view  = utl::View { .offset = offset, .length = state.offset - offset },
    };
}
