#ifndef KIELI_MIR_NODES_EXPRESSION
#define KIELI_MIR_NODES_EXPRESSION
#else
#error "This isn't supposed to be included by anything other than mir/mir.hpp"
#endif


namespace mir {

    struct Enum_constructor {
        ast::Name          name;
        tl::optional<Type> payload_type;
        tl::optional<Type> function_type;
        Type               enum_type;
    };

    namespace expression {
        template <class T>
        struct Literal {
            T value;
        };
        struct Array_literal {
            std::vector<Expression> elements;
        };
        struct Tuple {
            std::vector<Expression> fields;
        };
        struct Loop {
            utl::Wrapper<Expression> body;
        };
        struct Break {
            utl::Wrapper<Expression> result;
        };
        struct Continue {};
        struct Block {
            std::vector<Expression>  side_effect_expressions;
            utl::Wrapper<Expression> result_expression;
        };
        struct Let_binding {
            utl::Wrapper<Pattern>    pattern;
            Type                     type; // TODO: is this field necessary?
            utl::Wrapper<Expression> initializer;
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
            std::vector<Case>        cases;
            utl::Wrapper<Expression> matched_expression;
        };
        struct Local_variable_reference {
            Local_variable_tag   tag;
            compiler::Identifier identifier;
        };
        struct Struct_initializer {
            std::vector<Expression> initializers;
            Type                    struct_type;
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
        struct Function_reference {
            utl::Wrapper<resolution::Function_info> info;
            bool                                   is_application = false;
        };
        struct Direct_invocation {
            Function_reference      function;
            std::vector<Expression> arguments;
        };
        struct Indirect_invocation {
            std::vector<Expression> arguments;
            utl::Wrapper<Expression> invocable;
        };
        struct Enum_constructor_reference {
            Enum_constructor constructor;
        };
        struct Direct_enum_constructor_invocation {
            Enum_constructor        constructor;
            std::vector<Expression> arguments;
        };
        struct Sizeof {
            Type inspected_type;
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
        struct Move {
            utl::Wrapper<Expression> lvalue;
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
            expression::Tuple,
            expression::Loop,
            expression::Break,
            expression::Continue,
            expression::Block,
            expression::Let_binding,
            expression::Conditional,
            expression::Match,
            expression::Local_variable_reference,
            expression::Struct_initializer,
            expression::Struct_field_access,
            expression::Tuple_field_access,
            expression::Function_reference,
            expression::Direct_invocation,
            expression::Indirect_invocation,
            expression::Enum_constructor_reference,
            expression::Direct_enum_constructor_invocation,
            expression::Sizeof,
            expression::Reference,
            expression::Dereference,
            expression::Addressof,
            expression::Unsafe_dereference,
            expression::Move,
            expression::Hole
        >;

        Variant          value;
        Type             type;
        utl::Source_view source_view;
        Mutability       mutability;
        bool             is_addressable = false;
    };

}
