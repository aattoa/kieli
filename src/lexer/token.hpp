#pragma once

#include "utl/utilities.hpp"
#include "utl/pooled_string.hpp"
#include "utl/source.hpp"


namespace compiler {

    using String     = utl::Pooled_string<struct String_tag>;
    using Identifier = utl::Pooled_string<struct Identifier_tag>;


    struct [[nodiscard]] Lexical_token {
        using Variant = std::variant<
            std::monostate,
            utl::Isize,
            utl::Float,
            char,
            bool,
            String,
            Identifier
        >;

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

            underscore,
            lower_name,
            upper_name,
            operator_name,

            string,
            integer,
            floating,
            character,
            boolean,

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

            _token_type_count
        };

        Variant          value;
        Type             type;
        utl::Source_view source_view;

        template <class T> [[nodiscard]]
        inline auto value_as() const noexcept -> T const& {
            assert(std::holds_alternative<T>(value));
            return *std::get_if<T>(&value);
        }

        [[nodiscard]] inline auto& as_integer    () const noexcept { return value_as<utl::Isize >(); }
        [[nodiscard]] inline auto& as_floating   () const noexcept { return value_as<utl::Float >(); }
        [[nodiscard]] inline auto& as_character  () const noexcept { return value_as<utl::Char  >(); }
        [[nodiscard]] inline auto& as_boolean    () const noexcept { return value_as<bool      >(); }
        [[nodiscard]] inline auto& as_string     () const noexcept { return value_as<String    >(); }
        [[nodiscard]] inline auto& as_identifier () const noexcept { return value_as<Identifier>(); }
    };

    static_assert(std::is_trivially_copyable_v<Lexical_token>);


    auto token_description(Lexical_token::Type) noexcept -> std::string_view;

}


DECLARE_FORMATTER_FOR(compiler::Lexical_token::Type);
DECLARE_FORMATTER_FOR(compiler::Lexical_token);
