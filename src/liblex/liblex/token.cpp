#include <libutl/common/utilities.hpp>
#include <liblex/token.hpp>

static_assert(std::is_trivially_copyable_v<kieli::Token>);

namespace {
    constexpr auto type_strings_array = std::to_array<std::string_view>({
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
        "class",
        "inst",
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
}

auto kieli::Token::description(Type const type) -> std::string_view
{
    // clang-format off
    switch (type) {
    case Type::error:         return "a lexical error";
    case Type::dot:           return "a '.'";
    case Type::comma:         return "a ','";
    case Type::colon:         return "a ':'";
    case Type::semicolon:     return "a ';'";
    case Type::double_colon:  return "a '::'";
    case Type::ampersand:     return "a '&'";
    case Type::asterisk:      return "a '*'";
    case Type::plus:          return "a '+'";
    case Type::question:      return "a '?'";
    case Type::equals:        return "a '='";
    case Type::pipe:          return "a '|'";
    case Type::lambda:        return "a '\\'";
    case Type::left_arrow:    return "a '<-'";
    case Type::right_arrow:   return "a '->'";
    case Type::hole:          return "a hole";
    case Type::paren_open:    return "a '('";
    case Type::paren_close:   return "a ')'";
    case Type::brace_open:    return "a '{'";
    case Type::brace_close:   return "a '}'";
    case Type::bracket_open:  return "a '['";
    case Type::bracket_close: return "a ']'";
    case Type::let:
    case Type::mut:
    case Type::immut:
    case Type::if_:
    case Type::else_:
    case Type::elif:
    case Type::for_:
    case Type::in:
    case Type::while_:
    case Type::loop:
    case Type::continue_:
    case Type::break_:
    case Type::match:
    case Type::ret:
    case Type::discard:
    case Type::fn:
    case Type::as:
    case Type::enum_:
    case Type::struct_:
    case Type::class_:
    case Type::inst:
    case Type::impl:
    case Type::alias:
    case Type::import_:
    case Type::export_:
    case Type::module_:
    case Type::sizeof_:
    case Type::typeof_:
    case Type::unsafe:
    case Type::mov:
    case Type::meta:
    case Type::where:
    case Type::dyn:
    case Type::macro:
    case Type::global:
    case Type::lower_self:
    case Type::upper_self:        return "a keyword";
    case Type::underscore:        return "a wildcard pattern";
    case Type::lower_name:        return "an uncapitalized identifier";
    case Type::upper_name:        return "a capitalized identifier";
    case Type::operator_name:     return "an operator";
    case Type::integer_literal:   return "an integer literal";
    case Type::floating_literal:  return "a floating-point literal";
    case Type::string_literal:    return "a string literal";
    case Type::character_literal: return "a character literal";
    case Type::boolean_literal:   return "a boolean literal";
    case Type::string_type:
    case Type::floating_type:
    case Type::character_type:
    case Type::boolean_type:
    case Type::i8_type:
    case Type::i16_type:
    case Type::i32_type:
    case Type::i64_type:
    case Type::u8_type:
    case Type::u16_type:
    case Type::u32_type:
    case Type::u64_type:     return "a primitive typename";
    case Type::end_of_input: return "the end of input";
    default:
        cpputil::unreachable();
    }
    // clang-format on
}

auto kieli::Token::type_string(kieli::Token::Type const type) -> std::string_view
{
    static_assert(type_strings_array.size() == utl::enumerator_count<kieli::Token::Type>);
    return type_strings_array[utl::as_index(type)];
}
