#pragma once

#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>

namespace kieli {

    struct [[nodiscard]] Token {
        using Variant = std::
            variant<std::monostate, Integer, Floating, Character, Boolean, String, Identifier>;

        enum class Type {
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
            class_,
            inst,
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

            lower_self, // self
            upper_self, // Self

            end_of_input,

            _enumerator_count
        };

        Variant           variant;
        Type              type;
        std::string_view  preceding_trivia;
        utl::Source_range source_range;

        template <class T>
        [[nodiscard]] auto value_as() const -> T
        {
            T const* const pointer = std::get_if<T>(&variant);
            cpputil::always_assert(pointer != nullptr);
            return *pointer;
        }

        [[nodiscard]] static auto description(Type) -> std::string_view;
        [[nodiscard]] static auto type_string(Type) -> std::string_view;
    };
} // namespace kieli

template <>
struct std::formatter<kieli::Token::Type> : std::formatter<std::string_view> {
    auto format(kieli::Token::Type const type, auto& context) const
    {
        return std::formatter<std::string_view>::format(kieli::Token::type_string(type), context);
    }
};

template <>
struct std::formatter<kieli::Token> : std::formatter<std::string_view> {
    auto format(kieli::Token const& token, auto& context) const
    {
        if (std::holds_alternative<std::monostate>(token.variant)) {
            return std::formatter<std::string_view>::format(
                kieli::Token::type_string(token.type), context);
        }
        return std::format_to(context.out(), "({}: {})", token.type, token.variant);
    }
};
