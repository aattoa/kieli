#ifndef KIELI_AST_NODES_TYPE
#define KIELI_AST_NODES_TYPE
#else
#error This isn't supposed to be included by anything other than ast/ast.hpp
#endif


namespace ast {

    namespace type {

        enum class Integer {
            i8, i16, i32, i64,
            u8, u16, u32, u64,
            _integer_count
        };

        template <class>
        struct Primitive {};

        using Floating  = Primitive<utl::Float>;
        using Character = Primitive<utl::Char>;
        using Boolean   = Primitive<bool>;
        using String    = Primitive<compiler::String>;

        struct Wildcard {};

        struct Self {};

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
            Mutability        mutability;
        };

        struct Pointer {
            utl::Wrapper<Type> pointed_to_type;
            Mutability        mutability;
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
            type::Instance_of,
            type::Reference,
            type::Pointer,
            type::Template_application
        >;

        Variant         value;
        utl::Source_view source_view;
    };

}