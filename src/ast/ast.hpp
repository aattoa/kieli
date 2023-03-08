#pragma once

#include "utl/utilities.hpp"
#include "utl/wrapper.hpp"
#include "utl/source.hpp"
#include "utl/diagnostics.hpp"
#include "utl/flatmap.hpp"
#include "lexer/token.hpp"


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

        Variant         value;
        utl::Source_view source_view;

        constexpr auto was_explicitly_specified() const noexcept -> bool {
            if (auto const* const concrete = std::get_if<Concrete>(&value)) {
                // Immutability can not be specified explicitly
                return concrete->is_mutable;
            }
            else {
                // Parameterized mutability is always explicit
                return true;
            }
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
    struct [[nodiscard]] Function_parameter;


    struct Name {
        compiler::Identifier identifier;
        bool                 is_upper = false;
        utl::Source_view      source_view;

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
            Wildcard
        >;
        Variant             value;
        std::optional<Name> name;
    };

    template <tree_configuration Configuration>
    struct Basic_qualifier {
        std::optional<std::vector<Basic_template_argument<Configuration>>> template_arguments;
        Name                                                               name;
        utl::Source_view                                                    source_view;
    };

    template <tree_configuration Configuration>
    struct Basic_root_qualifier {
        struct Global {};
        std::variant<
            std::monostate,                           // id, id::id
            Global,                                   // ::id
            utl::Wrapper<typename Configuration::Type> // Type::id
        > value;
    };

    template <tree_configuration Configuration>
    struct Basic_qualified_name {
        std::vector<Basic_qualifier<Configuration>> middle_qualifiers;
        Basic_root_qualifier<Configuration>         root_qualifier;
        Name                                        primary_name;

        inline auto is_unqualified() const noexcept -> bool;
    };

    template <tree_configuration Configuration>
    struct Basic_class_reference {
        std::optional<std::vector<Basic_template_argument<Configuration>>> template_arguments;
        Basic_qualified_name<Configuration>                                name;
        utl::Source_view                                                    source_view;
    };

    template <tree_configuration Configuration>
    struct Basic_template_parameter {
        struct Type_parameter {
            std::vector<Basic_class_reference<Configuration>> classes;
        };
        struct Value_parameter {
            std::optional<utl::Wrapper<typename Configuration::Type>> type;
        };
        struct Mutability_parameter {};

        using Variant = std::variant<
            Type_parameter,
            Value_parameter,
            Mutability_parameter
        >;

        Variant                                               value;
        Name                                                  name;
        std::optional<Basic_template_argument<Configuration>> default_argument;
        utl::Source_view                                       source_view;
    };


    struct AST_configuration {
        using Expression  = ::ast::Expression;
        using Pattern     = ::ast::Pattern;
        using Type        = ::ast::Type;
        using Definition  = ::ast::Definition;
    };

    using Template_argument  = Basic_template_argument   <AST_configuration>;
    using Qualifier          = Basic_qualifier           <AST_configuration>;
    using Root_qualifier     = Basic_root_qualifier      <AST_configuration>;
    using Qualified_name     = Basic_qualified_name      <AST_configuration>;
    using Class_reference    = Basic_class_reference     <AST_configuration>;
    using Template_parameter = Basic_template_parameter  <AST_configuration>;

}


#include "nodes/expression.hpp"
#include "nodes/pattern.hpp"
#include "nodes/type.hpp"
#include "nodes/definition.hpp"


struct ast::Function_argument {
    Expression          expression;
    std::optional<Name> name;
};

struct ast::Function_parameter {
    Pattern                   pattern;
    std::optional<Type>       type;
    std::optional<Expression> default_value;
};

template <ast::tree_configuration Configuration>
auto ast::Basic_qualified_name<Configuration>::is_unqualified() const noexcept -> bool {
    return middle_qualifiers.empty()
        && std::holds_alternative<std::monostate>(root_qualifier.value);
}


namespace ast {

    using Node_context = utl::Wrapper_context<
        ast::Expression,
        ast::Type,
        ast::Pattern,
        ast::Definition
    >;


    struct Module_path {
        std::vector<compiler::Identifier> components;
        compiler::Identifier              module_name;
    };

    struct [[nodiscard]] Import {
        Module_path                         path;
        std::optional<compiler::Identifier> alias;
    };

    struct [[nodiscard]] Module {
        std::vector<Definition>             definitions;
        std::optional<compiler::Identifier> name;
        std::vector<Import>                 imports;
        std::vector<Module_path>            imported_by;
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
struct std::formatter<ast::Definition> : std::formatter<ast::Basic_definition<ast::AST_configuration>> {
    auto format(ast::Definition const& definition, std::format_context& context) {
        return std::formatter<ast::Basic_definition<ast::AST_configuration>>::format(definition, context);
    }
};