#pragma once

#include "utl/utilities.hpp"
#include "utl/wrapper.hpp"
#include "utl/source.hpp"
#include "representation/hir/hir.hpp"


/*

    The Mid-level Intermediate Representation (MIR) is the first intermediate
    program representation that is fully typed. It contains abstract
    information concerning generics, type variables, and other details
    relevant to the type-system. It is produced by resolving the HIR.

*/


namespace resolution {
    template <class>
    struct Definition_info;

#define DEFINE_INFO_NAME(name) \
using name##_info          = Definition_info<hir::definition::name>; \
using name##_template_info = Definition_info<hir::definition::name##_template>

    DEFINE_INFO_NAME(Function);
    DEFINE_INFO_NAME(Struct);
    DEFINE_INFO_NAME(Enum);
    DEFINE_INFO_NAME(Alias);
    DEFINE_INFO_NAME(Typeclass);
    DEFINE_INFO_NAME(Implementation);
    DEFINE_INFO_NAME(Instantiation);

#undef DEFINE_INFO_NAME
}


namespace mir {
    struct Function;
    struct Struct;
    struct Enum;
    struct Alias;
    struct Typeclass;
    struct Implementation;
    struct Instantiation;

    template <class>
    struct Template;


    namespace dtl {
        template <class> struct From_HIR_impl;
        template <class> struct To_HIR_impl;
    }
    template <class T> using From_HIR = dtl::From_HIR_impl<T>::type;
    template <class T> using To_HIR = dtl::To_HIR_impl<T>::type;

#define IMPL_TO_FROM_HIR(name) \
template <> struct dtl::From_HIR_impl<hir::definition::name> : std::type_identity<name> {}; \
template <> struct dtl::To_HIR_impl<name> : std::type_identity<hir::definition::name> {}
    IMPL_TO_FROM_HIR(Function);
    IMPL_TO_FROM_HIR(Struct);
    IMPL_TO_FROM_HIR(Enum);
    IMPL_TO_FROM_HIR(Alias);
    IMPL_TO_FROM_HIR(Typeclass);
    IMPL_TO_FROM_HIR(Implementation);
    IMPL_TO_FROM_HIR(Instantiation);
#undef IMPL_TO_FROM_HIR

    template <template <ast::tree_configuration> class Definition>
    struct dtl::From_HIR_impl<ast::definition::Template<Definition<hir::HIR_configuration>>>
        : std::type_identity<Template<From_HIR<Definition<hir::HIR_configuration>>>> {};
    template <class Definition>
    struct dtl::To_HIR_impl<Template<Definition>>
        : std::type_identity<ast::definition::Template<To_HIR<Definition>>> {};


    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Type;

    struct [[nodiscard]] Function_parameter;
    struct [[nodiscard]] Template_parameter;
    struct [[nodiscard]] Template_argument;


    struct [[nodiscard]] Class_reference {
        utl::Wrapper<resolution::Typeclass_info> info;
        utl::Source_view                         source_view;
    };

    struct [[nodiscard]] Unification_variable_tag {
        utl::Usize value;
        constexpr explicit Unification_variable_tag(utl::Usize const value) noexcept
            : value { value } {}
        [[nodiscard]] auto operator==(Unification_variable_tag const&) const noexcept -> bool = default;
    };

    struct [[nodiscard]] Template_parameter_tag {
        utl::Usize value;
        constexpr explicit Template_parameter_tag(utl::Usize const value) noexcept
            : value { value } {}
        [[nodiscard]] auto operator==(Template_parameter_tag const&) const noexcept -> bool = default;
    };

    struct [[nodiscard]] Local_variable_tag {
        utl::Usize value;
        constexpr explicit Local_variable_tag(utl::Usize const value) noexcept
                : value { value } {}
        [[nodiscard]] auto operator==(Local_variable_tag const&) const noexcept -> bool = default;
    };


    struct [[nodiscard]] Mutability {
        struct Concrete {
            bool is_mutable = false;
        };
        struct Variable {
            Unification_variable_tag tag;
        };
        struct Parameterized {
            // The identifier serves no purpose other than debuggability
            compiler::Identifier   identifier;
            Template_parameter_tag tag;
        };

        using Variant = std::variant<Concrete, Variable, Parameterized>;

        utl::Wrapper<Variant> value;
        utl::Source_view      source_view;

        auto with(utl::Source_view const view) const noexcept -> Mutability {
            return { .value = value, .source_view = view };
        }
    };

}


#include "representation/mir/nodes/type.hpp"
#include "representation/mir/nodes/expression.hpp"
#include "representation/mir/nodes/definition.hpp"
#include "representation/mir/nodes/pattern.hpp"


struct mir::Template_argument {
    using Variant = std::variant<Type, Expression, Mutability>;
    Variant                  value;
    tl::optional<ast::Name> name;
};

struct mir::Template_parameter {
    struct Type_parameter {
        std::vector<Class_reference> classes;
    };
    struct Value_parameter {
        Type type;
    };
    struct Mutability_parameter {};

    using Variant = std::variant<Type_parameter, Value_parameter, Mutability_parameter>;

    Variant                          value;
    ast::Name                        name;
    tl::optional<Template_argument> default_argument;
    Template_parameter_tag           reference_tag;
    utl::Source_view                 source_view;
};

struct mir::Function_parameter {
    Pattern pattern;
    Type    type;
};


namespace mir {

    using Node_context = utl::Wrapper_context<
        Expression,
        Pattern,
        Type::Variant,
        Mutability::Variant
    >;

    using Namespace_context = utl::Wrapper_context<
        resolution::Function_info,
        resolution::Struct_info,
        resolution::Enum_info,
        resolution::Alias_info,
        resolution::Typeclass_info,
        resolution::Namespace,
        resolution::Implementation_info,
        resolution::Instantiation_info,

        resolution::Function_template_info,
        resolution::Struct_template_info,
        resolution::Enum_template_info,
        resolution::Alias_template_info,
        resolution::Typeclass_template_info,
        resolution::Implementation_template_info,
        resolution::Instantiation_template_info
    >;

    struct Module {
        std::vector<utl::Wrapper<resolution::Function_info>>          functions;
        std::vector<utl::Wrapper<resolution::Function_template_info>> function_templates;
    };

    inline auto is_unification_variable(Type::Variant const& variant) -> bool {
        return std::holds_alternative<type::General_unification_variable>(variant)
            || std::holds_alternative<type::Integral_unification_variable>(variant);
    }
    inline auto is_unification_variable(Mutability::Variant const& variant) -> bool {
        return std::holds_alternative<Mutability::Variable>(variant);
    }

    template <class T>
    concept unification_variable = std::same_as<T, mir::type::General_unification_variable>
                                || std::same_as<T, mir::type::Integral_unification_variable>;

}


DECLARE_FORMATTER_FOR(mir::Template_argument);
DECLARE_FORMATTER_FOR(mir::Template_parameter);
DECLARE_FORMATTER_FOR(mir::Class_reference);
DECLARE_FORMATTER_FOR(mir::Mutability::Variant);
DECLARE_FORMATTER_FOR(mir::Mutability);
DECLARE_FORMATTER_FOR(mir::Unification_variable_tag);

DECLARE_FORMATTER_FOR(mir::Expression);
DECLARE_FORMATTER_FOR(mir::Pattern);
DECLARE_FORMATTER_FOR(mir::Type::Variant);
DECLARE_FORMATTER_FOR(mir::Type);

DECLARE_FORMATTER_FOR(mir::Function);
DECLARE_FORMATTER_FOR(mir::Struct);
DECLARE_FORMATTER_FOR(mir::Enum);
DECLARE_FORMATTER_FOR(mir::Alias);
DECLARE_FORMATTER_FOR(mir::Typeclass);
DECLARE_FORMATTER_FOR(mir::Implementation);
DECLARE_FORMATTER_FOR(mir::Instantiation);


namespace resolution {

    class [[nodiscard]] Context;


    class [[nodiscard]] Scope {
    public:
        struct Variable_binding {
            mir::Type                      type;
            mir::Mutability                mutability;
            mir::Local_variable_tag        variable_tag;
            bool                           has_been_mentioned = false;
            tl::optional<utl::Source_view> moved_at;
            utl::Source_view               source_view;
        };
        struct Type_binding {
            mir::Type        type;
            bool             has_been_mentioned = false;
            utl::Source_view source_view;
        };
        struct Mutability_binding {
            mir::Mutability  mutability;
            bool             has_been_mentioned = false;
            utl::Source_view source_view;
        };
    private:
        utl::Flatmap<compiler::Identifier, Variable_binding>   variable_bindings;
        utl::Flatmap<compiler::Identifier, Type_binding>       type_bindings;
        utl::Flatmap<compiler::Identifier, Mutability_binding> mutability_bindings;
        Context*                                               context;
        Scope*                                                 parent = nullptr;
    public:
        explicit Scope(Context&) noexcept;

        auto bind_variable  (compiler::Identifier, Variable_binding  &&) -> void;
        auto bind_type      (compiler::Identifier, Type_binding      &&) -> void;
        auto bind_mutability(compiler::Identifier, Mutability_binding&&) -> void;

        auto find_variable  (compiler::Identifier) noexcept -> Variable_binding*;
        auto find_type      (compiler::Identifier) noexcept -> Type_binding*;
        auto find_mutability(compiler::Identifier) noexcept -> Mutability_binding*;

        auto make_child() noexcept -> Scope;

        auto warn_about_unused_bindings() -> void;
    };


    using Lower_variant = std::variant<
        utl::Wrapper<Namespace>,
        utl::Wrapper<Function_info>,
        utl::Wrapper<Function_template_info>,
        mir::Enum_constructor
    >;

    using Upper_variant = std::variant<
        utl::Wrapper<Struct_info>,
        utl::Wrapper<Enum_info>,
        utl::Wrapper<Alias_info>,
        utl::Wrapper<Typeclass_info>,

        utl::Wrapper<Struct_template_info>,
        utl::Wrapper<Enum_template_info>,
        utl::Wrapper<Alias_template_info>,
        utl::Wrapper<Typeclass_template_info>
    >;

    using Definition_variant = std::variant<
        utl::Wrapper<Function_info>,
        utl::Wrapper<Struct_info>,
        utl::Wrapper<Enum_info>,
        utl::Wrapper<Alias_info>,
        utl::Wrapper<Typeclass_info>,
        utl::Wrapper<Namespace>,
        utl::Wrapper<Implementation_info>,
        utl::Wrapper<Instantiation_info>,

        utl::Wrapper<Function_template_info>,
        utl::Wrapper<Struct_template_info>,
        utl::Wrapper<Enum_template_info>,
        utl::Wrapper<Alias_template_info>,
        utl::Wrapper<Typeclass_template_info>,
        // namespace template

        utl::Wrapper<Implementation_template_info>,
        utl::Wrapper<Instantiation_template_info>
    >;


    struct [[nodiscard]] Namespace {
        std::vector<Definition_variant>                  definitions_in_order;
        utl::Flatmap<compiler::Identifier, Lower_variant> lower_table;
        utl::Flatmap<compiler::Identifier, Upper_variant> upper_table;
        tl::optional<utl::Wrapper<Namespace>>            parent;
        tl::optional<ast::Name>                          name;
    };


    enum class Definition_state {
        unresolved,
        resolved,
        currently_on_resolution_stack,
    };


    struct Partially_resolved_function {
        mir::Function::Signature           resolved_signature;
        Scope                              signature_scope;
        hir::Expression                    unresolved_body;
        ast::Name                          name;
        tl::optional<mir::Self_parameter> self_parameter;
    };

    struct Partially_resolved_function_template {
        Partially_resolved_function          function;
        std::vector<mir::Template_parameter> template_parameters;
    };


    template <class HIR_representation>
    struct Definition_info {
        using Variant = std::variant<HIR_representation, mir::From_HIR<HIR_representation>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;
    };

    template <template <ast::tree_configuration> class Definition>
        requires requires { &Definition<hir::HIR_configuration>::name; }
    struct Definition_info<ast::definition::Template<Definition<hir::HIR_configuration>>> {
        using Variant = std::variant<
            ast::definition::Template<Definition<hir::HIR_configuration>>,
            mir::From_HIR<ast::definition::Template<Definition<hir::HIR_configuration>>>
        >;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        mir::Type               parameterized_type_of_this; // One of mir::type::{Structure, Enumeration
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;
    };

    template <>
    struct Definition_info<hir::definition::Function_template> {
        using Variant = std::variant<
            hir::definition::Function_template,   // Fully unresolved
            Partially_resolved_function_template, // Signature resolved, body unresolved
            mir::Function_template                // Fully resolved
        >;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;
    };

    template <class Info>
    struct Template_instantiation_info {
        utl::Wrapper<Info>                   template_instantiated_from;
        std::vector<mir::Template_parameter> template_parameters;
        std::vector<mir::Template_argument>  template_arguments;
    };

    template <>
    struct Definition_info<hir::definition::Function> {
        using Variant = std::variant<
            hir::definition::Function,   // Fully unresolved
            Partially_resolved_function, // Signature resolved, body unresolved
            mir::Function                // Fully resolved
        >;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;

        tl::optional<Template_instantiation_info<Function_template_info>> template_instantiation_info;
    };

    template <>
    struct Definition_info<hir::definition::Struct> {
        using Variant = std::variant<hir::definition::Struct, mir::Struct>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        mir::Type               structure_type;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;

        tl::optional<Template_instantiation_info<Struct_template_info>> template_instantiation_info;
    };

    template <>
    struct Definition_info<hir::definition::Enum> {
        using Variant = std::variant<hir::definition::Enum, mir::Enum>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        mir::Type               enumeration_type;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;

        tl::optional<Template_instantiation_info<Enum_template_info>> template_instantiation_info;

        [[nodiscard]]
        auto constructor_count() const noexcept -> utl::Usize;
    };

    template <>
    struct Definition_info<hir::definition::Implementation> {
        using Variant = std::variant<hir::definition::Implementation, mir::Implementation>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

    template <>
    struct Definition_info<hir::definition::Instantiation> {
        using Variant = std::variant<hir::definition::Instantiation, mir::Instantiation>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

    template <template <ast::tree_configuration> class Definition>
    requires (!requires { &Definition<hir::HIR_configuration>::name; })
    struct Definition_info<ast::definition::Template<Definition<hir::HIR_configuration>>> {
        using Variant = std::variant<
            ast::definition::Template<Definition<hir::HIR_configuration>>,
            mir::From_HIR<ast::definition::Template<Definition<hir::HIR_configuration>>>
        >;

        Variant                value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

}
