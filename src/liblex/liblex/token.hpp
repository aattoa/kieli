#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


namespace compiler {

    struct Signed_integer          { utl::Isize value {}; };
    struct Unsigned_integer        { utl::Usize value {}; };
    struct Integer_of_unknown_sign { utl::Isize value {}; };
    struct Floating                { utl::Float value {}; };
    struct Boolean                 { bool       value {}; };
    struct Character               { char       value {}; };


    struct [[nodiscard]] Lexical_token {
        using Variant = std::variant<
            std::monostate,
            Signed_integer,
            Unsigned_integer,
            Integer_of_unknown_sign,
            Floating,
            Character,
            Boolean,
            String,
            Identifier>;

        enum class Type {
            dot,
            comma,
            colon,
            semicolon,
            double_colon,

            ampersand,
            asterisk,
            plus,
            question,
            equals,
            pipe,
            lambda,
            left_arrow,
            right_arrow,
            hole,

            paren_open,
            paren_close,
            brace_open,
            brace_close,
            bracket_open,
            bracket_close,

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
            unsafe_dereference,
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

            string,
            floating,
            character,
            boolean,

            signed_integer,
            unsigned_integer,
            integer_of_unknown_sign,

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

            lower_self,
            upper_self,

            end_of_input,

            _enumerator_count
        };

        Variant          value;
        Type             type;
        utl::Source_view source_view;

        template <class T> [[nodiscard]]
        auto value_as() const noexcept -> T {
            if (T const* const pointer = std::get_if<T>(&value))
                return *pointer;
            else
                utl::abort();
        }

        [[nodiscard]] auto as_floating        () const noexcept -> decltype(Floating::value);
        [[nodiscard]] auto as_character       () const noexcept -> decltype(Character::value);
        [[nodiscard]] auto as_boolean         () const noexcept -> decltype(Boolean::value);
        [[nodiscard]] auto as_string          () const noexcept -> String;
        [[nodiscard]] auto as_identifier      () const noexcept -> Identifier;
        [[nodiscard]] auto as_signed_integer  () const noexcept -> utl::Isize;
        [[nodiscard]] auto as_unsigned_integer() const noexcept -> utl::Usize;
    };

    [[nodiscard]]
    auto token_description(Lexical_token::Type) noexcept -> std::string_view;

}


DECLARE_FORMATTER_FOR(compiler::Lexical_token::Type);
DECLARE_FORMATTER_FOR(compiler::Lexical_token);


template <utl::one_of<
    compiler::Signed_integer,
    compiler::Unsigned_integer,
    compiler::Integer_of_unknown_sign,
    compiler::Floating,
    compiler::Boolean,
    compiler::Character> T>
struct fmt::formatter<T> : fmt::formatter<decltype(T::value)> {
    auto format(T const value_wrapper, auto& context) {
        return fmt::formatter<decltype(T::value)>::format(value_wrapper.value, context);
    }
};
