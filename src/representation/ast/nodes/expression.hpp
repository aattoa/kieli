#ifndef KIELI_AST_NODES_EXPRESSION
#define KIELI_AST_NODES_EXPRESSION
#else
#error "This isn't supposed to be included by anything other than ast/ast.hpp"
#endif


namespace ast {

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
        struct Template_application {
            std::vector<Template_argument> template_arguments;
            Qualified_name                 name;
        };
        struct Tuple {
            std::vector<Expression> fields;
        };
        struct Block {
            std::vector<Expression>                side_effect_expressions;
            tl::optional<utl::Wrapper<Expression>> result_expression;
        };
        struct Invocation {
            std::vector<Function_argument> arguments;
            utl::Wrapper<Expression>       invocable;
        };
        struct Struct_initializer {
            utl::Flatmap<Name, utl::Wrapper<Expression>> member_initializers;
            utl::Wrapper<Type>                           struct_type;
        };
        struct Binary_operator_invocation {
            utl::Wrapper<Expression> left;
            utl::Wrapper<Expression> right;
            compiler::Identifier    op;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            Name                     field_name;
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
            Name                                         method_name;
        };
        struct Conditional {
            utl::Wrapper<Expression>               condition;
            utl::Wrapper<Expression>               true_branch;
            tl::optional<utl::Wrapper<Expression>> false_branch;
        };
        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
            };
            std::vector<Case>        cases;
            utl::Wrapper<Expression> matched_expression;
        };
        struct Type_cast {
            enum class Kind {
                conversion,
                ascription,
            };
            utl::Wrapper<Expression> expression;
            utl::Wrapper<Type>       target_type;
            Kind                     cast_kind = Kind::conversion;
        };
        struct Let_binding {
            utl::Wrapper<Pattern>            pattern;
            utl::Wrapper<Expression>         initializer;
            tl::optional<utl::Wrapper<Type>> type;
        };
        struct Conditional_let {
            utl::Wrapper<Pattern>    pattern;
            utl::Wrapper<Expression> initializer;
        };
        struct Local_type_alias {
            compiler::Identifier identifier;
            utl::Wrapper<Type>   aliased_type;
        };
        struct Lambda {
            struct Capture {
                struct By_pattern {
                    utl::Wrapper<Pattern>    pattern;
                    utl::Wrapper<Expression> expression;
                };
                struct By_reference {
                    compiler::Identifier variable;
                };
                using Variant = std::variant<By_pattern, By_reference>;

                Variant          value;
                utl::Source_view source_view;
            };
            utl::Wrapper<Expression>        body;
            std::vector<Function_parameter> parameters;
            std::vector<Capture>            explicit_captures;
        };
        struct Infinite_loop {
            tl::optional<Name>       label;
            utl::Wrapper<Expression> body;
        };
        struct While_loop {
            tl::optional<Name>       label;
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> body;
        };
        struct For_loop {
            tl::optional<Name>       label;
            utl::Wrapper<Pattern>    iterator;
            utl::Wrapper<Expression> iterable;
            utl::Wrapper<Expression> body;
        };
        struct Continue {};
        struct Break {
            tl::optional<Name>                     label;
            tl::optional<utl::Wrapper<Expression>> result;
        };
        struct Discard {
            utl::Wrapper<Expression> discarded_expression;
        };
        struct Ret {
            tl::optional<utl::Wrapper<Expression>> returned_expression;
        };
        struct Sizeof {
            utl::Wrapper<Type> inspected_type;
        };
        struct Reference {
            Mutability               mutability;
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
            expression::Template_application,
            expression::Tuple,
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
            expression::Type_cast,
            expression::Let_binding,
            expression::Conditional_let,
            expression::Local_type_alias,
            expression::Lambda,
            expression::Infinite_loop,
            expression::While_loop,
            expression::For_loop,
            expression::Continue,
            expression::Break,
            expression::Discard,
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