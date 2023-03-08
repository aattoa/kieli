#ifndef KIELI_HIR_NODES_EXPRESSION
#define KIELI_HIR_NODES_EXPRESSION
#else
#error This isn't supposed to be included by anything other than hir/hir.hpp
#endif


namespace hir {

    namespace expression {

        template <class T>
        using Literal = ast::expression::Literal<T>;

        struct Array_literal {
            std::vector<Expression> elements;
        };

        using ast::expression::Self;

        struct Variable {
            Qualified_name name;
        };

        struct Tuple {
            std::vector<Expression> fields;
        };

        struct Loop {
            utl::Wrapper<Expression> body;
        };

        struct Continue {};

        struct Break {
            std::optional<ast::Name>               label;
            std::optional<utl::Wrapper<Expression>> result;
        };

        struct Block {
            std::vector<Expression>                side_effects;
            std::optional<utl::Wrapper<Expression>> result;
        };

        struct Invocation {
            std::vector<Function_argument> arguments;
            utl::Wrapper<Expression>        invocable;
        };

        struct Struct_initializer {
            utl::Flatmap<ast::Name, Expression> member_initializers;
            utl::Wrapper<Type>                  struct_type;
        };

        struct Binary_operator_invocation {
            utl::Wrapper<Expression> left;
            utl::Wrapper<Expression> right;
            compiler::Identifier    op;
        };

        struct Member_access_chain {
            struct Tuple_field {
                utl::Usize index;
            };
            struct Struct_field {
                compiler::Identifier identifier;
            };
            struct Array_index {
                utl::Wrapper<Expression> expression;
            };

            struct Accessor {
                using Variant = std::variant<Tuple_field, Struct_field, Array_index>;
                Variant         value;
                utl::Source_view source_view;
            };

            std::vector<Accessor>   accessors;
            utl::Wrapper<Expression> base_expression;
        };

        struct Method_invocation {
            std::vector<Function_argument>                arguments;
            std::optional<std::vector<Template_argument>> template_arguments;
            utl::Wrapper<Expression>                       base_expression;
            ast::Name                                     method_name;
        };

        struct Conditional {
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> true_branch;
            utl::Wrapper<Expression> false_branch;
        };

        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
            };
            std::vector<Case>       cases;
            utl::Wrapper<Expression> matched_expression;
        };

        struct Template_application {
            std::vector<Template_argument> template_arguments;
            Qualified_name                 name;
        };

        struct Type_cast {
            utl::Wrapper<Expression>          expression;
            utl::Wrapper<Type>                target_type;
            ast::expression::Type_cast::Kind cast_kind;
        };

        struct Let_binding {
            utl::Wrapper<Pattern>             pattern;
            utl::Wrapper<Expression>          initializer;
            std::optional<utl::Wrapper<Type>> type;
        };

        struct Local_type_alias {
            compiler::Identifier identifier;
            utl::Wrapper<Type>    aliased_type;
        };

        struct Ret {
            std::optional<utl::Wrapper<Expression>> returned_expression;
        };

        struct Sizeof {
            utl::Wrapper<Type> inspected_type;
        };

        struct Reference {
            ast::Mutability         mutability;
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
            expression::Literal<utl::Isize>,
            expression::Literal<utl::Float>,
            expression::Literal<utl::Char>,
            expression::Literal<bool>,
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
            expression::Member_access_chain,
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