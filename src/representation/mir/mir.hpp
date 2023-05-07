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

    using Function_info = Definition_info<hir::definition::Function>;

#define DEFINE_INFO_NAME(name) \
using name##_info          = Definition_info<hir::definition::name>; \
using name##_template_info = Definition_info<hir::definition::name##_template>
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
    template <class T> using From_HIR = typename dtl::From_HIR_impl<T>::type;
    template <class T> using To_HIR = typename dtl::To_HIR_impl<T>::type;

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

    template <template <class> class Definition>
    struct dtl::From_HIR_impl<ast::definition::Template<Definition<hir::HIR_configuration>>>
        : std::type_identity<Template<From_HIR<Definition<hir::HIR_configuration>>>> {};
    template <class Definition>
    struct dtl::To_HIR_impl<Template<Definition>>
        : std::type_identity<ast::definition::Template<To_HIR<Definition>>> {};


    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    class  [[nodiscard]] Type;

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


    class [[nodiscard]] Unification_type_variable_state;
    class [[nodiscard]] Unification_mutability_variable_state;

    class [[nodiscard]] Mutability {
    public:
        struct Concrete {
            utl::Strong<bool> is_mutable;
        };
        struct Variable {
            utl::Wrapper<Unification_mutability_variable_state> state;
        };
        struct Parameterized {
            // The identifier serves no purpose other than debuggability
            compiler::Identifier   identifier;
            Template_parameter_tag tag;
        };
        using Variant = std::variant<Concrete, Variable, Parameterized>;
    private:
        utl::Wrapper<Variant> m_value;
        utl::Source_view      m_source_view;
    public:
        explicit Mutability(utl::Wrapper<Variant>, utl::Source_view) noexcept;

        // Get the wrapped value, but flatten solved unification variables first
        auto flattened_value() const -> utl::Wrapper<Variant>;
        // Get the wrapped value without flattening solved unification variables
        auto pure_value() const noexcept -> utl::Wrapper<Variant>;

        auto source_view() const noexcept -> utl::Source_view;
        auto with(utl::Source_view) const noexcept -> Mutability;
    };

}


#include "representation/mir/nodes/type.hpp"
#include "representation/mir/nodes/expression.hpp"
#include "representation/mir/nodes/definition.hpp"
#include "representation/mir/nodes/pattern.hpp"


namespace mir {

    struct Template_argument {
        using Variant = std::variant<Type, Expression, Mutability>;
        Variant                  value;
        tl::optional<ast::Name> name;
    };

    struct Template_parameter {
        struct Type_parameter {
            std::vector<Class_reference> classes;
        };
        struct Value_parameter {
            Type type;
        };
        struct Mutability_parameter {};

        using Variant = std::variant<Type_parameter, Value_parameter, Mutability_parameter>;

        Variant                              value;
        utl::Strong<tl::optional<ast::Name>> name; // nullopt for implicit template parameters
        tl::optional<Template_argument>      default_argument;
        Template_parameter_tag               reference_tag;
        utl::Source_view                     source_view;

        [[nodiscard]] auto is_implicit() const noexcept -> bool;
    };

    struct Function_parameter {
        Pattern pattern;
        Type    type;
    };


    enum class Unification_type_variable_kind {
        general, integral
    };

    class Unification_type_variable_state {
    public:
        struct Solved {
            Type solution;
        };
        struct Unsolved {
            Unification_variable_tag                    tag;
            utl::Strong<Unification_type_variable_kind> kind;
            std::vector<Class_reference>                classes;
        };
    private:
        std::variant<Solved, Unsolved> m_value;
    public:
        explicit Unification_type_variable_state(Unsolved&&) noexcept;

        auto solve_with(Type solution) -> void;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current())       noexcept -> Unsolved      &;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current()) const noexcept -> Unsolved const&;
        [[nodiscard]] auto as_solved_if()       noexcept -> Solved      *;
        [[nodiscard]] auto as_solved_if() const noexcept -> Solved const*;
    };

    class Unification_mutability_variable_state {
    public:
        struct Solved {
            Mutability solution;
        };
        struct Unsolved {
            Unification_variable_tag tag;
        };
    private:
        std::variant<Solved, Unsolved> m_value;
    public:
        explicit Unification_mutability_variable_state(Unsolved&&) noexcept;

        auto solve_with(Mutability solution) -> void;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current())       noexcept -> Unsolved      &;
        [[nodiscard]] auto as_unsolved(std::source_location = std::source_location::current()) const noexcept -> Unsolved const&;
        [[nodiscard]] auto as_solved_if()       noexcept -> Solved      *;
        [[nodiscard]] auto as_solved_if() const noexcept -> Solved const*;
    };


    using Node_arena = utl::Wrapper_arena<
        Expression,
        Pattern,
        Type::Variant,
        Mutability::Variant,
        Unification_type_variable_state,
        Unification_mutability_variable_state>;

    using Namespace_arena = utl::Wrapper_arena<
        resolution::Function_info,
        resolution::Struct_info,
        resolution::Enum_info,
        resolution::Alias_info,
        resolution::Typeclass_info,
        resolution::Namespace,
        resolution::Implementation_info,
        resolution::Instantiation_info,
        resolution::Struct_template_info,
        resolution::Enum_template_info,
        resolution::Alias_template_info,
        resolution::Typeclass_template_info,
        resolution::Implementation_template_info,
        resolution::Instantiation_template_info>;

    struct Module {
        std::vector<utl::Wrapper<resolution::Function_info>> functions;
    };

}


DECLARE_FORMATTER_FOR(mir::Template_argument);
DECLARE_FORMATTER_FOR(mir::Template_parameter);
DECLARE_FORMATTER_FOR(mir::Class_reference);
DECLARE_FORMATTER_FOR(mir::Mutability);

DECLARE_FORMATTER_FOR(mir::Unification_variable_tag);
DECLARE_FORMATTER_FOR(mir::Unification_type_variable_state::Unsolved);
DECLARE_FORMATTER_FOR(mir::Unification_mutability_variable_state::Unsolved);

DECLARE_FORMATTER_FOR(mir::Expression);
DECLARE_FORMATTER_FOR(mir::Pattern);
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
        mir::Enum_constructor>;

    using Upper_variant = std::variant<
        utl::Wrapper<Struct_info>,
        utl::Wrapper<Enum_info>,
        utl::Wrapper<Alias_info>,
        utl::Wrapper<Typeclass_info>,
        utl::Wrapper<Struct_template_info>,
        utl::Wrapper<Enum_template_info>,
        utl::Wrapper<Alias_template_info>,
        utl::Wrapper<Typeclass_template_info>>;

    using Definition_variant = std::variant<
        utl::Wrapper<Function_info>,
        utl::Wrapper<Struct_info>,
        utl::Wrapper<Enum_info>,
        utl::Wrapper<Alias_info>,
        utl::Wrapper<Typeclass_info>,
        utl::Wrapper<Namespace>,
        utl::Wrapper<Implementation_info>,
        utl::Wrapper<Instantiation_info>,
        utl::Wrapper<Struct_template_info>,
        utl::Wrapper<Enum_template_info>,
        utl::Wrapper<Alias_template_info>,
        utl::Wrapper<Typeclass_template_info>,
        utl::Wrapper<Implementation_template_info>,
        utl::Wrapper<Instantiation_template_info>>;


    struct [[nodiscard]] Namespace {
        std::vector<Definition_variant>                   definitions_in_order;
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
        mir::Function::Signature resolved_signature;
        Scope                    signature_scope;
        hir::Expression          unresolved_body;
        ast::Name                name;
    };


    template <class HIR_representation>
    struct Definition_info {
        using Variant = std::variant<HIR_representation, mir::From_HIR<HIR_representation>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;
    };

    template <template <class> class Definition>
    requires requires { &Definition<hir::HIR_configuration>::name; }
    struct Definition_info<ast::definition::Template<Definition<hir::HIR_configuration>>> {
        using Variant = std::variant<
            ast::definition::Template<Definition<hir::HIR_configuration>>,
            mir::From_HIR<ast::definition::Template<Definition<hir::HIR_configuration>>>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        mir::Type               parameterized_type_of_this; // One of mir::type::{Structure, Enumeration
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
            hir::definition::Function,          // Fully unresolved function
            hir::definition::Function_template, // Fully unresolved function template
            Partially_resolved_function,        // Signature resolved, body unresolved
            mir::Function>;                     // Fully resolved

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
        ast::Name               name;

        tl::optional<Template_instantiation_info<Function_info>> template_instantiation_info;
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

        [[nodiscard]] auto constructor_count() const noexcept -> utl::Usize;
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

    template <template <class> class Definition>
    requires (!requires { &Definition<hir::HIR_configuration>::name; })
    struct Definition_info<ast::definition::Template<Definition<hir::HIR_configuration>>> {
        using Variant = std::variant<
            ast::definition::Template<Definition<hir::HIR_configuration>>,
            mir::From_HIR<ast::definition::Template<Definition<hir::HIR_configuration>>>>;

        Variant                 value;
        utl::Wrapper<Namespace> home_namespace;
        Definition_state        state = Definition_state::unresolved;
    };

}
