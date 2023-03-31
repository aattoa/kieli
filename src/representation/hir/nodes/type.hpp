#ifndef KIELI_HIR_NODES_TYPE
#define KIELI_HIR_NODES_TYPE
#else
#error "This isn't supposed to be included by anything other than hir/hir.hpp"
#endif


namespace hir {

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
        struct Implicit_parameter_reference {
            Implicit_template_parameter::Tag tag;
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
            type::Implicit_parameter_reference,
            type::Tuple,
            type::Array,
            type::Slice,
            type::Function,
            type::Typeof,
            type::Reference,
            type::Pointer,
            type::Instance_of,
            type::Template_application
        >;
        Variant         value;
        utl::Source_view source_view;
    };

}
