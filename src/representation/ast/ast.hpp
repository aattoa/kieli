#pragma once

#include "utl/utilities.hpp"
#include "utl/wrapper.hpp"
#include "utl/source.hpp"
#include "utl/diagnostics.hpp"
#include "utl/flatmap.hpp"
#include "representation/token/token.hpp"


/*

    The Abstract Syntax Tree (AST) is a high-level structured representation of
    a program's syntax. It is produced by parsing a sequence of tokens. Any
    syntactically valid program can be represented as an AST, but such a program
    may still be erroneous in other ways, and such errors can only be revealed
    by subsequent compilation steps.

    For example, the following expression is syntactically valid, and can thus
    be represented as an AST node, but it will be rejected upon expression
    resolution due to the obvious type error:

        let x: Int = "hello"

*/


namespace ast {

    struct Mutability {
        struct Concrete {
            bool is_mutable = false;
        };
        struct Parameterized {
            compiler::Identifier identifier;
        };

        using Variant = std::variant<Concrete, Parameterized>;

        Variant          value;
        utl::Source_view source_view;

        [[nodiscard]]
        constexpr auto was_explicitly_specified() const noexcept -> bool {
            auto const* const concrete = std::get_if<Concrete>(&value);
            return !concrete || concrete->is_mutable;
        }
    };


    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Definition;


    template <class T>
    concept tree_configuration = requires {
        typename T::Expression;
        typename T::Pattern;
        typename T::Type;
        typename T::Definition;
    };


    struct [[nodiscard]] Function_argument;


    struct Name {
        compiler::Identifier identifier;
        utl::Strong<bool>    is_upper;
        utl::Source_view     source_view;

        constexpr auto operator==(Name const& other) const noexcept -> bool {
            return identifier == other.identifier;
        }
    };

    template <tree_configuration Configuration>
    struct Basic_template_argument {
        struct Wildcard {
            utl::Source_view source_view;
        };
        using Variant = std::variant<
            utl::Wrapper<typename Configuration::Type>,
            utl::Wrapper<typename Configuration::Expression>,
            Mutability,
            Wildcard>;
        Variant            value;
        tl::optional<Name> name;
    };

    template <tree_configuration Configuration>
    struct Basic_qualifier {
        tl::optional<std::vector<Basic_template_argument<Configuration>>> template_arguments;
        Name                                                              name;
        utl::Source_view                                                  source_view;
    };

    template <tree_configuration Configuration>
    struct Basic_root_qualifier {
        struct Global {};
        std::variant<
            std::monostate,                            // id, id::id
            Global,                                    // ::id
            utl::Wrapper<typename Configuration::Type> // Type::id
        > value;
    };

    template <tree_configuration Configuration>
    struct Basic_qualified_name {
        std::vector<Basic_qualifier<Configuration>> middle_qualifiers;
        Basic_root_qualifier<Configuration>         root_qualifier;
        Name                                        primary_name;

        [[nodiscard]]
        auto is_unqualified() const noexcept -> bool {
            return middle_qualifiers.empty()
                && std::holds_alternative<std::monostate>(root_qualifier.value);
        }
    };

    template <tree_configuration Configuration>
    struct Basic_class_reference {
        tl::optional<std::vector<Basic_template_argument<Configuration>>> template_arguments;
        Basic_qualified_name<Configuration>                               name;
        utl::Source_view                                                  source_view;
    };

    template <tree_configuration Configuration>
    struct Basic_template_parameter {
        struct Type_parameter {
            std::vector<Basic_class_reference<Configuration>> classes;
        };
        struct Value_parameter {
            tl::optional<utl::Wrapper<typename Configuration::Type>> type;
        };
        struct Mutability_parameter {};

        using Variant = std::variant<
            Type_parameter,
            Value_parameter,
            Mutability_parameter
        >;

        Variant                                              value;
        Name                                                 name;
        tl::optional<Basic_template_argument<Configuration>> default_argument;
        utl::Source_view                                     source_view;
    };

    template <tree_configuration Configuration>
    struct Basic_function_parameter {
        typename Configuration::Pattern                  pattern;
        tl::optional<typename Configuration::Type>       type;
        tl::optional<typename Configuration::Expression> default_argument;
    };


    struct AST_configuration {
        using Expression  = ::ast::Expression;
        using Pattern     = ::ast::Pattern;
        using Type        = ::ast::Type;
        using Definition  = ::ast::Definition;
    };

    using Template_argument  = Basic_template_argument  <AST_configuration>;
    using Qualifier          = Basic_qualifier          <AST_configuration>;
    using Root_qualifier     = Basic_root_qualifier     <AST_configuration>;
    using Qualified_name     = Basic_qualified_name     <AST_configuration>;
    using Class_reference    = Basic_class_reference    <AST_configuration>;
    using Template_parameter = Basic_template_parameter <AST_configuration>;
    using Function_parameter = Basic_function_parameter <AST_configuration>;

}


#include "nodes/expression.hpp"
#include "nodes/pattern.hpp"
#include "nodes/type.hpp"
#include "nodes/definition.hpp"


struct ast::Function_argument {
    Expression         expression;
    tl::optional<Name> name;
};


namespace ast {
    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    using Node_arena = utl::Wrapper_arena<Expression, Type, Pattern>;

    struct [[nodiscard]] Module {
        std::vector<Definition>        definitions;
        tl::optional<compiler::String> name;
        std::vector<compiler::String>  imports;
    };
}


DECLARE_FORMATTER_FOR(ast::Expression);
DECLARE_FORMATTER_FOR(ast::Pattern);
DECLARE_FORMATTER_FOR(ast::Type);

DECLARE_FORMATTER_FOR(ast::Function_parameter);
DECLARE_FORMATTER_FOR(ast::Mutability);
DECLARE_FORMATTER_FOR(ast::Name);
DECLARE_FORMATTER_FOR(ast::Module);
DECLARE_FORMATTER_FOR(ast::expression::Type_cast::Kind);


// vvv These are defined and explicitly instantiated in hir/shared_formatting.cpp to avoid repetition vvv

template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_template_argument<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_qualified_name<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_class_reference<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_template_parameter<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::definition::Basic_struct_member<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::definition::Basic_enum_constructor<Configuration>);

template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_definition<Configuration>);

template <>
struct fmt::formatter<ast::Definition> : fmt::formatter<ast::Basic_definition<ast::AST_configuration>> {
    auto format(ast::Definition const& definition, fmt::format_context& context) {
        return fmt::formatter<ast::Basic_definition<ast::AST_configuration>>::format(definition, context);
    }
};
