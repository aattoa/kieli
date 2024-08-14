#include <libutl/utilities.hpp>
#include <libcompiler/token/token.hpp>

static_assert(std::is_trivially_copyable_v<kieli::Token>);

static constexpr auto type_strings_array = std::to_array<std::string_view>({
    "lexical error",

    ".",
    ",",
    ":",
    ";",
    "::",

    "&",
    "*",
    "+",
    "?",
    "!",
    "=",
    "|",
    "\\",
    "<-",
    "->",
    R"(???)",

    "(",
    ")",
    "{",
    "}",
    "[",
    "]",

    "let",
    "mut",
    "immut",
    "if",
    "else",
    "elif",
    "for",
    "in",
    "while",
    "loop",
    "continue",
    "break",
    "match",
    "ret",
    "discard",
    "fn",
    "as",
    "enum",
    "struct",
    "concept",
    "impl",
    "alias",
    "import",
    "export",
    "module",
    "sizeof",
    "typeof",
    "unsafe",
    "mov",
    "meta",
    "where",
    "dyn",
    "macro",
    "global",
    "defer",

    "_",
    "lower",
    "upper",
    "op",

    "int",
    "float",
    "str",
    "char",
    "bool",

    "String",
    "Float",
    "Char",
    "Bool",
    "I8",
    "I16",
    "I32",
    "I64",
    "U8",
    "U16",
    "U32",
    "U64",

    "self",
    "Self",

    "end of input",
});

auto kieli::token_description(Token_type const type) -> std::string_view
{
    switch (type) {
    case Token_type::error:             return "a lexical error";
    case Token_type::dot:               return "a '.'";
    case Token_type::comma:             return "a ','";
    case Token_type::colon:             return "a ':'";
    case Token_type::semicolon:         return "a ';'";
    case Token_type::double_colon:      return "a '::'";
    case Token_type::ampersand:         return "a '&'";
    case Token_type::asterisk:          return "a '*'";
    case Token_type::plus:              return "a '+'";
    case Token_type::question:          return "a '?'";
    case Token_type::equals:            return "a '='";
    case Token_type::pipe:              return "a '|'";
    case Token_type::lambda:            return "a '\\'";
    case Token_type::left_arrow:        return "a '<-'";
    case Token_type::right_arrow:       return "a '->'";
    case Token_type::hole:              return "a hole";
    case Token_type::paren_open:        return "a '('";
    case Token_type::paren_close:       return "a ')'";
    case Token_type::brace_open:        return "a '{'";
    case Token_type::brace_close:       return "a '}'";
    case Token_type::bracket_open:      return "a '['";
    case Token_type::bracket_close:     return "a ']'";
    case Token_type::let:
    case Token_type::mut:
    case Token_type::immut:
    case Token_type::if_:
    case Token_type::else_:
    case Token_type::elif:
    case Token_type::for_:
    case Token_type::in:
    case Token_type::while_:
    case Token_type::loop:
    case Token_type::continue_:
    case Token_type::break_:
    case Token_type::match:
    case Token_type::ret:
    case Token_type::discard:
    case Token_type::fn:
    case Token_type::as:
    case Token_type::enum_:
    case Token_type::struct_:
    case Token_type::concept_:
    case Token_type::impl:
    case Token_type::alias:
    case Token_type::import_:
    case Token_type::export_:
    case Token_type::module_:
    case Token_type::sizeof_:
    case Token_type::typeof_:
    case Token_type::unsafe:
    case Token_type::mov:
    case Token_type::meta:
    case Token_type::where:
    case Token_type::dyn:
    case Token_type::macro:
    case Token_type::global:
    case Token_type::defer:
    case Token_type::lower_self:
    case Token_type::upper_self:        return "a keyword";
    case Token_type::underscore:        return "a wildcard pattern";
    case Token_type::lower_name:        return "an uncapitalized identifier";
    case Token_type::upper_name:        return "a capitalized identifier";
    case Token_type::operator_name:     return "an operator";
    case Token_type::integer_literal:   return "an integer literal";
    case Token_type::floating_literal:  return "a floating-point literal";
    case Token_type::string_literal:    return "a string literal";
    case Token_type::character_literal: return "a character literal";
    case Token_type::boolean_literal:   return "a boolean literal";
    case Token_type::string_type:
    case Token_type::floating_type:
    case Token_type::character_type:
    case Token_type::boolean_type:
    case Token_type::i8_type:
    case Token_type::i16_type:
    case Token_type::i32_type:
    case Token_type::i64_type:
    case Token_type::u8_type:
    case Token_type::u16_type:
    case Token_type::u32_type:
    case Token_type::u64_type:          return "a primitive typename";
    case Token_type::end_of_input:      return "the end of input";
    default:                            cpputil::unreachable();
    }
}

auto kieli::token_type_string(kieli::Token_type const type) -> std::string_view
{
    static_assert(type_strings_array.size() == utl::enumerator_count<kieli::Token_type>);
    return type_strings_array[utl::as_index(type)];
}
