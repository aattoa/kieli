#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


namespace kieli {
    struct Integer   { utl::Usize value {}; };
    struct Floating  { utl::Float value {}; };
    struct Boolean   { bool       value {}; };
    struct Character { char       value {}; };

    struct [[nodiscard]] Lexical_token {
        using Variant = std::variant<
            std::monostate,
            Integer,
            Floating,
            Character,
            Boolean,
            compiler::String,
            compiler::Operator,
            compiler::Identifier>;

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
            namespace_,
            import_,
            export_,
            module_,
            sizeof_,
            typeof_,
            addressof,
            dereference,
            unsafe,
            mov,
            meta,
            where,
            dyn,
            pub,
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

        Variant          value;
        Type             type;
        std::string_view preceding_trivia;
        utl::Source_view source_view;

        template <class T> [[nodiscard]]
        auto value_as() const noexcept -> T {
            if (T const* const pointer = std::get_if<T>(&value))
                return *pointer;
            else
                utl::abort();
        }

        [[nodiscard]] auto as_integer    () const noexcept -> decltype(Integer::value);
        [[nodiscard]] auto as_floating   () const noexcept -> decltype(Floating::value);
        [[nodiscard]] auto as_character  () const noexcept -> decltype(Character::value);
        [[nodiscard]] auto as_boolean    () const noexcept -> decltype(Boolean::value);
        [[nodiscard]] auto as_string     () const noexcept -> compiler::String;
        [[nodiscard]] auto as_operator   () const noexcept -> compiler::Operator;
        [[nodiscard]] auto as_identifier () const noexcept -> compiler::Identifier;

        [[nodiscard]] static auto description(Type) noexcept -> std::string_view;
        [[nodiscard]] static auto type_string(Type) noexcept -> std::string_view;
    };
}


template <>
struct fmt::formatter<kieli::Lexical_token::Type> : fmt::formatter<std::string_view> {
    auto format(kieli::Lexical_token::Type const type, auto& context) { // NOLINT
        return fmt::formatter<std::string_view>::format(kieli::Lexical_token::type_string(type), context);
    }
};

template <>
struct fmt::formatter<kieli::Lexical_token> : fmt::formatter<std::string_view> {
    auto format(kieli::Lexical_token const& token, auto& context) { // NOLINT
        if (std::holds_alternative<std::monostate>(token.value))
            return fmt::formatter<std::string_view>::format(kieli::Lexical_token::type_string(token.type), context);
        else
            return fmt::format_to(context.out(), "({}: '{}')", token.type, token.value);
    }
};

template <utl::one_of<kieli::Integer, kieli::Floating, kieli::Boolean, kieli::Character> T>
struct fmt::formatter<T> : fmt::formatter<decltype(T::value)> {
    auto format(T const value_wrapper, auto& context) {
        return fmt::formatter<decltype(T::value)>::format(value_wrapper.value, context);
    }
};
