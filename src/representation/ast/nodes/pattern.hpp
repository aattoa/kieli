#ifndef KIELI_AST_NODES_PATTERN
#define KIELI_AST_NODES_PATTERN
#else
#error "This isn't supposed to be included by anything other than ast/ast.hpp"
#endif


namespace ast {

    namespace pattern {

        template <class T>
        using Literal = expression::Literal<T>;

        struct Wildcard {};

        struct Name {
            compiler::Identifier identifier;
            Mutability           mutability;
        };

        struct Constructor {
            Qualified_name                      constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };

        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> element_patterns;
        };

        struct As {
            Name                 alias;
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
            pattern::Constructor,
            pattern::Tuple,
            pattern::Slice,
            pattern::As,
            pattern::Guarded
        >;

        Variant         value;
        utl::Source_view source_view;
    };

}
