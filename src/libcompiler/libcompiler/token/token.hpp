#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

namespace kieli {

    using Token_variant = std::variant<
        std::monostate, //
        Integer,
        Floating,
        Character,
        Boolean,
        String,
        Identifier>;

    enum class Token_type {
        error,

        dot,          // .
        comma,        // ,
        colon,        // :
        semicolon,    // ;
        double_colon, // ::

        ampersand,   // &
        asterisk,    // *
        plus,        // +
        question,    // ?
        exclamation, // !
        equals,      // =
        pipe,        // |
        lambda,      // backslash
        left_arrow,  // <-
        right_arrow, // ->
        hole,        // ???

        paren_open,    // (
        paren_close,   // )
        brace_open,    // {
        brace_close,   // }
        bracket_open,  // [
        bracket_close, // ]

        let,
        mut,
        immut,
        if_,
        else_,
        elif,
        for_,
        in,
        while_,
        loop,
        continue_,
        break_,
        match,
        ret,
        discard,
        fn,
        as,
        enum_,
        struct_,
        concept_,
        impl,
        alias,
        import_,
        export_,
        module_,
        sizeof_,
        typeof_,
        unsafe,
        mov,
        meta,
        where,
        dyn,
        macro,
        global,
        defer,

        underscore,
        lower_name,
        upper_name,
        operator_name,

        integer_literal,
        floating_literal,
        string_literal,
        character_literal,
        boolean_literal,

        string_type,
        floating_type,
        character_type,
        boolean_type,
        i8_type,
        i16_type,
        i32_type,
        i64_type,
        u8_type,
        u16_type,
        u32_type,
        u64_type,
        self,

        end_of_input,

        _enumerator_count
    };

    struct [[nodiscard]] Token {
        Token_variant    variant;
        Token_type       type;
        std::string_view preceding_trivia;
        Range            range;

        template <class T>
        [[nodiscard]] auto value_as() const -> T
        {
            T const* const pointer = std::get_if<T>(&variant);
            cpputil::always_assert(pointer != nullptr);
            return *pointer;
        }
    };

    [[nodiscard]] auto token_description(Token_type) -> std::string_view;
    [[nodiscard]] auto token_type_string(Token_type) -> std::string_view;
} // namespace kieli

template <>
struct std::formatter<kieli::Token_type> : std::formatter<std::string_view> {
    auto format(kieli::Token_type const type, auto& context) const
    {
        return std::formatter<std::string_view>::format(kieli::token_type_string(type), context);
    }
};

template <>
struct std::formatter<kieli::Token> : std::formatter<std::string_view> {
    auto format(kieli::Token const& token, auto& context) const
    {
        if (std::holds_alternative<std::monostate>(token.variant)) {
            return std::formatter<std::string_view>::format(
                kieli::token_type_string(token.type), context);
        }
        return std::format_to(context.out(), "({}: {})", token.type, token.variant);
    }
};
