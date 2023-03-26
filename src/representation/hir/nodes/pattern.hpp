#ifndef KIELI_HIR_NODES_PATTERN
#define KIELI_HIR_NODES_PATTERN
#else
#error "This isn't supposed to be included by anything other than hir/hir.hpp"
#endif


namespace hir {

    namespace pattern {

        using ast::pattern::Literal;
        using ast::pattern::Wildcard;
        using ast::pattern::Name;

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
            pattern::Literal<utl::Isize>,
            pattern::Literal<utl::Float>,
            pattern::Literal<utl::Char>,
            pattern::Literal<bool>,
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
