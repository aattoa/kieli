#pragma once

#include <libutl/utilities.hpp>
#include <libutl/flatmap.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libcompiler/hir/hir.hpp>

namespace libresolve {
    struct Function_info;
    struct Enumeration_info;
    struct Typeclass_info;
    struct Alias_info;
    struct Module_info;
    struct Environment;
    class Scope;

    struct Info_arena {
        utl::Index_vector<hir::Module_id, Module_info>           modules;
        utl::Index_vector<hir::Environment_id, Environment>      environments;
        utl::Index_vector<hir::Function_id, Function_info>       functions;
        utl::Index_vector<hir::Enumeration_id, Enumeration_info> enumerations;
        utl::Index_vector<hir::Typeclass_id, Typeclass_info>     typeclasses;
        utl::Index_vector<hir::Alias_id, Alias_info>             aliases;
    };

    struct Lower_info {
        using Variant = std::variant<hir::Function_id, hir::Module_id>;
        kieli::Name_lower name;
        kieli::Source_id  source;
        Variant           variant;
    };

    struct Upper_info {
        using Variant = std::variant<hir::Enumeration_id, hir::Typeclass_id, hir::Alias_id>;
        kieli::Name_upper name;
        kieli::Source_id  source;
        Variant           variant;
    };

    using Definition_variant = std::variant<
        hir::Function_id,
        hir::Module_id,
        hir::Enumeration_id,
        hir::Typeclass_id,
        hir::Alias_id>;

    struct Import {
        std::filesystem::file_time_type last_write_time;
        std::filesystem::path           module_path;
        kieli::Name_lower               name;
    };

    struct Variable_bind {
        kieli::Name_lower       name;
        hir::Type_id            type;
        hir::Mutability         mutability;
        hir::Local_variable_tag tag;
        bool                    unused = true;
    };

    struct Type_bind {
        kieli::Name_upper name;
        hir::Type_id      type;
        bool              unused = true;
    };

    struct Mutability_bind {
        kieli::Name_lower name;
        hir::Mutability   mutability;
        bool              unused = true;
    };

    struct Function_with_resolved_signature;
} // namespace libresolve

class libresolve::Scope {
    utl::Flatmap<kieli::Identifier, Variable_bind>   m_variables;
    utl::Flatmap<kieli::Identifier, Type_bind>       m_types;
    utl::Flatmap<kieli::Identifier, Mutability_bind> m_mutabilities;
    kieli::Source_id                                 m_source;
    Scope*                                           m_parent {};
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
    auto report_unused(kieli::Compile_info& info) -> void;
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
    kieli::Name_lower   name;
    bool                currently_resolving {};
};

struct libresolve::Enumeration_info {
    using Variant = std::variant<ast::definition::Enumeration, hir::Enumeration>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Name_upper   name;
    hir::Type           type;
    bool                currently_resolving {};
};

struct libresolve::Typeclass_info {
    using Variant = std::variant<ast::definition::Typeclass, hir::Typeclass>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Name_upper   name;
    bool                currently_resolving {};
};

struct libresolve::Alias_info {
    using Variant = std::variant<ast::definition::Alias, hir::Alias>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Name_upper   name;
    bool                currently_resolving {};
};

struct libresolve::Module_info {
    using Variant = std::variant<ast::definition::Submodule, Import, hir::Module>;
    Variant             variant;
    hir::Environment_id environment;
    kieli::Source_id    source;
    kieli::Name_lower   name;
};

struct libresolve::Environment {
    utl::Flatmap<kieli::Identifier, Upper_info> upper_map;
    utl::Flatmap<kieli::Identifier, Lower_info> lower_map;
    std::vector<Definition_variant>             in_order;
    std::optional<hir::Environment_id>          parent;
    kieli::Source_id                            source;
};
