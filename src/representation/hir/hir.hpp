#pragma once

#include "utl/utilities.hpp"
#include "representation/ast/ast.hpp"


/*

    The High-level Intermediate Representation (HIR) is a high level structured
    representation of a program's syntax, much like the AST. The HIR is
    essentially a simplified AST, with slightly lower level representations for
    certain nodes. It is produced by desugaring the AST.

    For example, the following AST node:
        while a { b }

    would be desugared to the following HIR node:
        loop { if a { b } else { break } }

*/


namespace hir {

    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Definition;


    struct HIR_configuration {
        using Expression = hir::Expression;
        using Pattern    = hir::Pattern;
        using Type       = hir::Type;
        using Definition = hir::Definition;
    };

    using Template_argument  = ast::Basic_template_argument   <HIR_configuration>;
    using Root_qualifier     = ast::Basic_root_qualifier      <HIR_configuration>;
    using Qualifier          = ast::Basic_qualifier           <HIR_configuration>;
    using Qualified_name     = ast::Basic_qualified_name      <HIR_configuration>;
    using Class_reference    = ast::Basic_class_reference     <HIR_configuration>;
    using Template_parameter = ast::Basic_template_parameter  <HIR_configuration>;


    struct [[nodiscard]] Function_argument;
    struct [[nodiscard]] Function_parameter;

    struct Implicit_template_parameter {
        struct Tag {
            utl::Usize value;
            explicit constexpr Tag(utl::Usize const value) noexcept
                : value { value } {}
        };

        std::vector<Class_reference> classes;
        Tag                          tag;
    };

}


#include "representation/hir/nodes/expression.hpp"
#include "representation/hir/nodes/pattern.hpp"
#include "representation/hir/nodes/type.hpp"
#include "representation/hir/nodes/definition.hpp"


struct hir::Function_argument {
    Expression               expression;
    tl::optional<ast::Name> name;
};

struct hir::Function_parameter {
    Pattern                   pattern;
    Type                      type;
    tl::optional<Expression> default_value;
};


namespace hir {

    using Node_context = utl::Wrapper_context<Expression, Type, Pattern>;

    struct Module {
        std::vector<Definition> definitions;
    };

}


DECLARE_FORMATTER_FOR(hir::Expression);
DECLARE_FORMATTER_FOR(hir::Type);
DECLARE_FORMATTER_FOR(hir::Pattern);

DECLARE_FORMATTER_FOR(hir::Function_parameter);
DECLARE_FORMATTER_FOR(hir::Implicit_template_parameter);
DECLARE_FORMATTER_FOR(hir::Implicit_template_parameter::Tag);


template <>
struct fmt::formatter<hir::Definition> : fmt::formatter<ast::Basic_definition<hir::HIR_configuration>> {
    auto format(hir::Definition const& definition, fmt::format_context& context) {
        return fmt::formatter<ast::Basic_definition<hir::HIR_configuration>>::format(definition, context);
    }
};