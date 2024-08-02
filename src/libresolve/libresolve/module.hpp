#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libcompiler/hir/hir.hpp>

namespace libresolve {
    namespace ast = kieli::ast;
    namespace hir = kieli::hir;

    struct Function_info;
    struct Enumeration_info;
    struct Concept_info;
    struct Alias_info;
    struct Module_info;
    struct Environment;
    class Scope;

    struct Info_arena {
        utl::Index_vector<hir::Module_id, Module_info>           modules;
        utl::Index_vector<hir::Environment_id, Environment>      environments;
        utl::Index_vector<hir::Function_id, Function_info>       functions;
        utl::Index_vector<hir::Enumeration_id, Enumeration_info> enumerations;
        utl::Index_vector<hir::Concept_id, Concept_info>         concepts;
        utl::Index_vector<hir::Alias_id, Alias_info>             aliases;
    };

    struct Lower_info {
        using Variant = std::variant<hir::Function_id, hir::Module_id>;
        kieli::Lower     name;
        kieli::Source_id source;
        Variant          variant;
    };

    struct Upper_info {
        using Variant = std::variant<hir::Enumeration_id, hir::Concept_id, hir::Alias_id>;
        kieli::Upper     name;
        kieli::Source_id source;
        Variant          variant;
    };

    struct Definition_variant
        : std::variant<
              hir::Function_id,
              hir::Module_id,
              hir::Enumeration_id,
              hir::Concept_id,
              hir::Alias_id> {
        using variant::variant, variant::operator=;
    };

    struct Import {
        std::filesystem::file_time_type last_write_time;
        std::filesystem::path           module_path;
        kieli::Lower                    name;
    };

    struct Variable_bind {
        kieli::Lower            name;
        hir::Type_id            type;
        hir::Mutability         mutability;
        hir::Local_variable_tag tag;
        bool                    unused = true;
    };

    struct Type_bind {
        kieli::Upper name;
        hir::Type_id type;
        bool         unused = true;
    };

    struct Mutability_bind {
        kieli::Lower    name;
        hir::Mutability mutability;
        bool            unused = true;
    };

    struct Function_with_resolved_signature;

    template <class T>
    using Identifier_map = std::vector<std::pair<kieli::Identifier, T>>;
} // namespace libresolve

class libresolve::Scope {
    Identifier_map<Variable_bind>   m_variables;
    Identifier_map<Type_bind>       m_types;
    Identifier_map<Mutability_bind> m_mutabilities;
    kieli::Source_id                m_source;
    Scope*                          m_parent {};
public:
    explicit Scope(kieli::Source_id const source) : m_source { source } {}

    Scope(Scope&&)                    = default;
    auto operator=(Scope&&) -> Scope& = default;

    Scope(Scope const&)                    = delete;
    auto operator=(Scope const&) -> Scope& = delete;

    auto bind_mutability(kieli::Identifier identifier, Mutability_bind binding) -> void;
    auto bind_variable(kieli::Identifier identifier, Variable_bind binding) -> void;
    auto bind_type(kieli::Identifier identifier, Type_bind binding) -> void;

    [[nodiscard]] auto find_mutability(kieli::Identifier identifier) -> Mutability_bind*;
    [[nodiscard]] auto find_variable(kieli::Identifier identifier) -> Variable_bind*;
    [[nodiscard]] auto find_type(kieli::Identifier identifier) -> Type_bind*;

    // Make a child scope. `this` must not be moved or destroyed while the child lives.
    [[nodiscard]] auto child() noexcept -> Scope;

    // Retrieve the parent pointer. Returns `nullptr` if there is no parent.
    [[nodiscard]] auto parent() const noexcept -> Scope*;

    // Retrieve the source id.
    [[nodiscard]] auto source() const noexcept -> kieli::Source_id;

    // Emit warnings for any unused bindings.
    auto report_unused(kieli::Database& db) -> void;
};

struct libresolve::Function_with_resolved_signature {
    ast::Expression         unresolved_body;
    hir::Function_signature signature;
    Scope                   signature_scope;
};

struct libresolve::Function_info {
    using Variant = std::variant<
        ast::definition::Function, //
        Function_with_resolved_signature,
        hir::Function>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Lower        name;
    bool                currently_resolving {};
};

struct libresolve::Enumeration_info {
    using Variant = std::variant<ast::definition::Enumeration, hir::Enumeration>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Upper        name;
    hir::Type           type;
    bool                currently_resolving {};
};

struct libresolve::Concept_info {
    using Variant = std::variant<ast::definition::Concept, hir::Concept>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Upper        name;
    bool                currently_resolving {};
};

struct libresolve::Alias_info {
    using Variant = std::variant<ast::definition::Alias, hir::Alias>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Upper        name;
    bool                currently_resolving {};
};

struct libresolve::Module_info {
    using Variant = std::variant<ast::definition::Submodule, Import, hir::Module>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Lower        name;
};

struct libresolve::Environment {
    Identifier_map<Upper_info>         upper_map;
    Identifier_map<Lower_info>         lower_map;
    std::vector<Definition_variant>    in_order;
    std::optional<hir::Environment_id> parent;
    kieli::Source_id                   source;
};
