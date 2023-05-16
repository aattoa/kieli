#pragma once

#include <libutl/common/utilities.hpp>
#include <libparse/ast.hpp>


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
    static_assert(ast::tree_configuration<HIR_configuration>);

    using Template_argument  = ast::Basic_template_argument  <HIR_configuration>;
    using Root_qualifier     = ast::Basic_root_qualifier     <HIR_configuration>;
    using Qualifier          = ast::Basic_qualifier          <HIR_configuration>;
    using Qualified_name     = ast::Basic_qualified_name     <HIR_configuration>;
    using Class_reference    = ast::Basic_class_reference    <HIR_configuration>;
    using Template_parameter = ast::Basic_template_parameter <HIR_configuration>;
    using Function_parameter = ast::Basic_function_parameter <HIR_configuration>;

    struct [[nodiscard]] Function_argument;



    namespace expression {
        template <class T>
        struct Literal {
            T value;
        };
        struct Array_literal {
            std::vector<Expression> elements;
        };
        struct Self {};
        struct Variable {
            Qualified_name name;
        };
        struct Tuple {
            std::vector<Expression> fields;
        };
        struct Loop {
            enum class Kind { plain_loop, while_loop, for_loop };
            utl::Wrapper<Expression> body;
            utl::Strong<Kind>        kind;
        };
        struct Continue {};
        struct Break {
            tl::optional<ast::Name>  label;
            utl::Wrapper<Expression> result;
        };
        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
        };
        struct Invocation {
            std::vector<Function_argument> arguments;
            utl::Wrapper<Expression>       invocable;
        };
        struct Struct_initializer {
            utl::Flatmap<ast::Name, utl::Wrapper<Expression>> member_initializers;
            utl::Wrapper<Type>                                struct_type;
        };
        struct Binary_operator_invocation {
            utl::Wrapper<Expression> left;
            utl::Wrapper<Expression> right;
            compiler::Identifier     op;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            ast::Name                field_name;
        };
        struct Tuple_field_access {
            utl::Wrapper<Expression> base_expression;
            utl::Strong<utl::Usize>  field_index;
            utl::Source_view         field_index_source_view;
        };
        struct Array_index_access {
            utl::Wrapper<Expression> base_expression;
            utl::Wrapper<Expression> index_expression;
        };
        struct Method_invocation {
            std::vector<Function_argument>               arguments;
            tl::optional<std::vector<Template_argument>> template_arguments;
            utl::Wrapper<Expression>                     base_expression;
            ast::Name                                    method_name;
        };
        struct Conditional {
            enum class Kind { normal_conditional, while_loop_body };
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> true_branch;
            utl::Wrapper<Expression> false_branch;
            utl::Strong<Kind>        kind;
            utl::Strong<bool>        has_explicit_false_branch;
        };
        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
            };
            std::vector<Case>        cases;
            utl::Wrapper<Expression> matched_expression;
        };
        struct Template_application {
            std::vector<Template_argument> template_arguments;
            Qualified_name                 name;
        };
        struct Type_cast {
            utl::Wrapper<Expression>         expression;
            utl::Wrapper<Type>               target_type;
            ast::expression::Type_cast::Kind cast_kind;
        };
        struct Let_binding {
            utl::Wrapper<Pattern>            pattern;
            utl::Wrapper<Expression>         initializer;
            tl::optional<utl::Wrapper<Type>> type;
        };
        struct Local_type_alias {
            compiler::Identifier identifier;
            utl::Wrapper<Type>   aliased_type;
        };
        struct Ret {
            tl::optional<utl::Wrapper<Expression>> returned_expression;
        };
        struct Sizeof {
            utl::Wrapper<Type> inspected_type;
        };
        struct Reference {
            ast::Mutability          mutability;
            utl::Wrapper<Expression> referenced_expression;
        };
        struct Dereference {
            utl::Wrapper<Expression> dereferenced_expression;
        };
        struct Addressof {
            utl::Wrapper<Expression> lvalue;
        };
        struct Unsafe_dereference {
            utl::Wrapper<Expression> pointer;
        };
        struct Placement_init {
            utl::Wrapper<Expression> lvalue;
            utl::Wrapper<Expression> initializer;
        };
        struct Move {
            utl::Wrapper<Expression> lvalue;
        };
        struct Meta {
            utl::Wrapper<Expression> expression;
        };
        struct Hole {};
    }

    struct Expression {
        using Variant = std::variant<
            expression::Literal<kieli::Signed_integer>,
            expression::Literal<kieli::Unsigned_integer>,
            expression::Literal<kieli::Integer_of_unknown_sign>,
            expression::Literal<kieli::Floating>,
            expression::Literal<kieli::Character>,
            expression::Literal<kieli::Boolean>,
            expression::Literal<compiler::String>,
            expression::Array_literal,
            expression::Self,
            expression::Variable,
            expression::Tuple,
            expression::Loop,
            expression::Break,
            expression::Continue,
            expression::Block,
            expression::Invocation,
            expression::Struct_initializer,
            expression::Binary_operator_invocation,
            expression::Struct_field_access,
            expression::Tuple_field_access,
            expression::Array_index_access,
            expression::Method_invocation,
            expression::Conditional,
            expression::Match,
            expression::Template_application,
            expression::Type_cast,
            expression::Let_binding,
            expression::Local_type_alias,
            expression::Ret,
            expression::Sizeof,
            expression::Reference,
            expression::Dereference,
            expression::Addressof,
            expression::Unsafe_dereference,
            expression::Placement_init,
            expression::Move,
            expression::Meta,
            expression::Hole>;

        Variant          value;
        utl::Source_view source_view;
    };



    namespace pattern {
        using ast::pattern::Literal;
        using ast::pattern::Wildcard;
        using ast::pattern::Name;
        struct Constructor {
            Qualified_name                      constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };
        struct Abbreviated_constructor {
            ast::Name                           constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Slice {
            std::vector<Pattern> element_patterns;
        };
        struct As {
            Name                  alias;
            utl::Wrapper<Pattern> aliased_pattern;
        };
        struct Guarded {
            utl::Wrapper<Pattern> guarded_pattern;
            Expression            guard;
        };
    }

    struct Pattern {
        using Variant = std::variant<
            pattern::Literal<kieli::Signed_integer>,
            pattern::Literal<kieli::Unsigned_integer>,
            pattern::Literal<kieli::Integer_of_unknown_sign>,
            pattern::Literal<kieli::Floating>,
            pattern::Literal<kieli::Character>,
            pattern::Literal<kieli::Boolean>,
            pattern::Literal<compiler::String>,
            pattern::Wildcard,
            pattern::Name,
            pattern::Constructor,
            pattern::Abbreviated_constructor,
            pattern::Tuple,
            pattern::Slice,
            pattern::As,
            pattern::Guarded>;

        Variant         value;
        utl::Source_view source_view;
    };



    namespace type {
        using ast::type::Primitive;
        using ast::type::Integer;
        using ast::type::Floating;
        using ast::type::Character;
        using ast::type::Boolean;
        using ast::type::String;
        using ast::type::Wildcard;
        using ast::type::Self;
        struct Typename {
            Qualified_name name;
        };
        struct Tuple {
            std::vector<Type> field_types;
        };
        struct Array {
            utl::Wrapper<Type>       element_type;
            utl::Wrapper<Expression> array_length;
        };
        struct Slice {
            utl::Wrapper<Type> element_type;
        };
        struct Function {
            std::vector<Type> argument_types;
            utl::Wrapper<Type> return_type;
        };
        struct Typeof {
            utl::Wrapper<Expression> inspected_expression;
        };
        struct Reference {
            utl::Wrapper<Type> referenced_type;
            ast::Mutability   mutability;
        };
        struct Pointer {
            utl::Wrapper<Type> pointed_to_type;
            ast::Mutability   mutability;
        };
        struct Instance_of {
            std::vector<Class_reference> classes;
        };
        struct Template_application {
            std::vector<Template_argument> arguments;
            Qualified_name                 name;
        };
    }

    struct Type {
        using Variant = std::variant<
            type::Integer,
            type::Floating,
            type::Character,
            type::Boolean,
            type::String,
            type::Wildcard,
            type::Self,
            type::Typename,
            type::Tuple,
            type::Array,
            type::Slice,
            type::Function,
            type::Typeof,
            type::Reference,
            type::Pointer,
            type::Instance_of,
            type::Template_application>;

        Variant          value;
        utl::Source_view source_view;
    };



    using Function_signature          = ast::Basic_function_signature          <HIR_configuration>;
    using Function_template_signature = ast::Basic_function_template_signature <HIR_configuration>;
    using Type_signature              = ast::Basic_type_signature              <HIR_configuration>;
    using Type_template_signature     = ast::Basic_type_template_signature     <HIR_configuration>;

    namespace definition {
        using Function       = ast::definition::Basic_function       <HIR_configuration>;
        using Struct         = ast::definition::Basic_struct         <HIR_configuration>;
        using Enum           = ast::definition::Basic_enum           <HIR_configuration>;
        using Alias          = ast::definition::Basic_alias          <HIR_configuration>;
        using Typeclass      = ast::definition::Basic_typeclass      <HIR_configuration>;
        using Implementation = ast::definition::Basic_implementation <HIR_configuration>;
        using Instantiation  = ast::definition::Basic_instantiation  <HIR_configuration>;
        using Namespace      = ast::definition::Basic_namespace      <HIR_configuration>;

        using Function_template       = ast::definition::Template<Function>;
        using Struct_template         = ast::definition::Template<Struct>;
        using Enum_template           = ast::definition::Template<Enum>;
        using Alias_template          = ast::definition::Template<Alias>;
        using Typeclass_template      = ast::definition::Template<Typeclass>;
        using Implementation_template = ast::definition::Template<Implementation>;
        using Instantiation_template  = ast::definition::Template<Instantiation>;
        using Namespace_template      = ast::definition::Template<Namespace>;
    }

    struct Definition : ast::Basic_definition<HIR_configuration> {
        using Basic_definition::Basic_definition;
        using Basic_definition::operator=;
    };


    struct Function_argument {
        Expression               expression;
        tl::optional<ast::Name> name;
    };


    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    using Node_arena = utl::Wrapper_arena<Expression, Type, Pattern>;

    struct Module {
        std::vector<Definition> definitions;
    };

}


DECLARE_FORMATTER_FOR(hir::Expression);
DECLARE_FORMATTER_FOR(hir::Type);
DECLARE_FORMATTER_FOR(hir::Pattern);
DECLARE_FORMATTER_FOR(hir::Function_parameter);


template <>
struct fmt::formatter<hir::Definition> : fmt::formatter<ast::Basic_definition<hir::HIR_configuration>> {
    auto format(hir::Definition const& definition, fmt::format_context& context) {
        return fmt::formatter<ast::Basic_definition<hir::HIR_configuration>>::format(definition, context);
    }
};
