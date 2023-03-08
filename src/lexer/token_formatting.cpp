#include "utl/utilities.hpp"
#include "token.hpp"


auto compiler::token_description(Lexical_token::Type const type)
    noexcept -> std::string_view
{
    using enum Lexical_token::Type;
    switch (type) {
    case dot:           return "a '.'";
    case comma:         return "a ','";
    case colon:         return "a ':'";
    case semicolon:     return "a ';'";
    case double_colon:  return "a '::'";
    case ampersand:     return "a '&'";
    case asterisk:      return "a '*'";
    case plus:          return "a '+'";
    case question:      return "a '?'";
    case equals:        return "a '='";
    case pipe:          return "a '|'";
    case lambda:        return "a '\\'";
    case left_arrow:    return "a '<-'";
    case right_arrow:   return "a '->'";
    case hole:          return "a hole";
    case paren_open:    return "a '('";
    case paren_close:   return "a ')'";
    case brace_open:    return "a '{'";
    case brace_close:   return "a '}'";
    case bracket_open:  return "a '['";
    case bracket_close: return "a ']'";

    case let:
    case mut:
    case immut:
    case if_:
    case else_:
    case elif:
    case for_:
    case in:
    case while_:
    case loop:
    case continue_:
    case break_:
    case match:
    case ret:
    case discard:
    case fn:
    case as:
    case enum_:
    case struct_:
    case class_:
    case namespace_:
    case inst:
    case impl:
    case alias:
    case import_:
    case export_:
    case module_:
    case sizeof_:
    case typeof:
    case addressof:
    case unsafe_dereference:
    case meta:
    case where:
    case dyn:
    case pub:
    case macro:
    case lower_self:
    case upper_self:
        return "a keyword";

    case underscore:    return "a wildcard pattern";
    case lower_name:    return "an uncapitalized identifier";
    case upper_name:    return "a capitalized identifier";
    case operator_name: return "an operator";
    case string:        return "a string literal";
    case integer:       return "an integer literal";
    case floating:      return "a floating-point literal";
    case character:     return "a character literal";
    case boolean:       return "a boolean literal";
    case end_of_input:  return "the end of input";

    case string_type:
    case floating_type:
    case character_type:
    case boolean_type:
    case i8_type:
    case i16_type:
    case i32_type:
    case i64_type:
    case u8_type:
    case u16_type:
    case u32_type:
    case u64_type:
        return "a primitive typename";

    default:
        utl::abort("Unimplemented for {}"_format(std::to_underlying(type)));
    }
}


DEFINE_FORMATTER_FOR(compiler::Lexical_token::Type) {
    constexpr auto strings = std::to_array<std::string_view>({
        ".", ",", ":", ";", "::", "&", "*", "+", "?", "=", "|", "\\", "<-", "->", "???", "(", ")", "{", "}", "[", "]",

        "let", "mut", "immut", "if", "else", "elif", "for", "in", "while", "loop", "continue", "break",
        "match", "ret", "discard", "fn", "as", "enum", "struct", "class", "inst", "impl", "alias",
        "namespace", "import", "export", "module", "sizeof", "typeof", "addressof", "unsafe_dereference", "meta", "where", "dyn", "pub", "macro",

        "underscore", "lower", "upper", "op",

        "str", "int", "float", "char", "bool",

        "String", "Float", "Char", "Bool", "I8", "I16", "I32", "I64", "U8", "U16", "U32", "U64",
        "self", "Self",

        "end of input"
    });
    static_assert(strings.size() == static_cast<utl::Usize>(compiler::Lexical_token::Type::_token_type_count));
    return std::format_to(context.out(), "{}", strings[static_cast<utl::Usize>(value)]);
}


DEFINE_FORMATTER_FOR(compiler::Lexical_token) {
    if (std::holds_alternative<std::monostate>(value.value)) {
        return std::format_to(context.out(), "'{}'", value.type);
    }
    else {
        return std::format_to(context.out(), "({}: '{}')", value.type, value.value);
    }
}