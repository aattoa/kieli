#ifndef KIELI_MIR_NODES_TYPE
#define KIELI_MIR_NODES_TYPE
#else
#error "This isn't supposed to be included by anything other than mir/mir.hpp"
#endif


namespace mir {

    class [[nodiscard]] Type {
    public:
        struct Variant;
    private:
        utl::Wrapper<Variant> m_value;
        utl::Source_view      m_source_view;
    public:
        explicit Type(utl::Wrapper<Variant>, utl::Source_view) noexcept;

        // Get the wrapped value, but flatten solved unification variables first
        auto flattened_value() const -> utl::Wrapper<Variant>;
        // Get the wrapped value without flattening solved unification variables
        auto pure_value() const noexcept -> utl::Wrapper<Variant>;

        auto source_view() const noexcept -> utl::Source_view;
        auto with(utl::Source_view) const noexcept -> Type;
    };


    namespace type {
        using hir::type::Primitive;
        using hir::type::Integer;
        using hir::type::Floating;
        using hir::type::Character;
        using hir::type::Boolean;
        using hir::type::String;
        // Self within a class
        struct Self_placeholder {};
        struct Tuple {
            std::vector<Type> field_types;
        };
        struct Array {
            Type                     element_type;
            utl::Wrapper<Expression> array_length;
        };
        struct Slice {
            Type element_type;
        };
        struct Function {
            std::vector<Type> parameter_types;
            Type              return_type;
        };
        struct Reference {
            Mutability mutability;
            Type       referenced_type;
        };
        struct Pointer {
            Mutability mutability;
            Type       pointed_to_type;
        };
        struct Structure {
            utl::Wrapper<resolution::Struct_info> info;
            bool                                  is_application = false;
        };
        struct Enumeration {
            utl::Wrapper<resolution::Enum_info> info;
            bool                                is_application = false;
        };
        struct Unification_variable {
            utl::Wrapper<Unification_type_variable_state> state;
        };
        struct Template_parameter_reference {
            // The identifier serves no purpose other than debuggability
            utl::Strong<tl::optional<compiler::Identifier>> identifier;
            Template_parameter_tag                          tag;
        };
    }


    struct Type::Variant : std::variant<
        type::Tuple,
        type::Integer,
        type::Floating,
        type::Character,
        type::Boolean,
        type::String,
        type::Self_placeholder,
        type::Array,
        type::Slice,
        type::Function,
        type::Reference,
        type::Pointer,
        type::Structure,
        type::Enumeration,
        type::Unification_variable,
        type::Template_parameter_reference>
    {
        using variant::variant;
        using variant::operator=;
    };

}
