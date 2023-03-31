#ifndef KIELI_MIR_NODES_PATTERN
#define KIELI_MIR_NODES_PATTERN
#else
#error "This isn't supposed to be included by anything other than mir/mir.hpp"
#endif


namespace mir {

    namespace pattern {
        struct Wildcard {};
        template <class T>
        struct Literal {
            T value;
        };
        struct Name {
            Local_variable_tag   variable_tag;
            compiler::Identifier identifier;
            Mutability           mutability;
        };
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Slice {
            std::vector<Pattern> element_patterns;
        };
        struct Enum_constructor {
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
            ::mir::Enum_constructor             constructor;
        };
        struct As {
            Name                  alias;
            utl::Wrapper<Pattern> aliased_pattern;
        };
        struct Guarded {
            utl::Wrapper<Pattern> guarded_pattern;
            Expression           guard;
        };
    }


    struct Pattern {
        using Variant = std::variant<
            pattern::Literal<compiler::Signed_integer>,
            pattern::Literal<compiler::Unsigned_integer>,
            pattern::Literal<compiler::Integer_of_unknown_sign>,
            pattern::Literal<compiler::Floating>,
            pattern::Literal<compiler::Character>,
            pattern::Literal<compiler::Boolean>,
            pattern::Literal<compiler::String>,
            pattern::Wildcard,
            pattern::Name,
            pattern::Tuple,
            pattern::Slice,
            pattern::Enum_constructor,
            pattern::As,
            pattern::Guarded
        >;
        Variant          value;
        Type             type;
        bool             is_exhaustive_by_itself = false;
        utl::Source_view source_view;
    };

}
