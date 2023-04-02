#ifndef KIELI_HIR_NODES_EXPRESSION
#define KIELI_HIR_NODES_EXPRESSION
#else
#error "This isn't supposed to be included by anything other than hir/hir.hpp"
#endif


namespace hir {

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
            Kind                     kind {};
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
            utl::Usize               field_index {};
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
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> true_branch;
            utl::Wrapper<Expression> false_branch;
            bool                     has_explicit_false_branch {};
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
            expression::Literal<compiler::Signed_integer>,
            expression::Literal<compiler::Unsigned_integer>,
            expression::Literal<compiler::Integer_of_unknown_sign>,
            expression::Literal<compiler::Floating>,
            expression::Literal<compiler::Character>,
            expression::Literal<compiler::Boolean>,
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
            expression::Hole
        >;

        Variant          value;
        utl::Source_view source_view;
    };

}
