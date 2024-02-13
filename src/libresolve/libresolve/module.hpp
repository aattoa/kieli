#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libphase/phase.hpp>
#include <libdesugar/ast.hpp>
#include <libresolve/hir.hpp>
#include <libresolve/fwd.hpp>

namespace libresolve {
    struct Lower_info {
        using Variant = std::variant<
            utl::Mutable_wrapper<Function_info>, //
            utl::Mutable_wrapper<Module_info>>;
        kieli::Name_lower    name;
        utl::Source::Wrapper source;
        Variant              variant;
    };

    struct Upper_info {
        using Variant = std::variant<
            utl::Mutable_wrapper<Enumeration_info>,
            utl::Mutable_wrapper<Typeclass_info>,
            utl::Mutable_wrapper<Alias_info>>;
        kieli::Name_upper    name;
        utl::Source::Wrapper source;
        Variant              variant;
    };

    using Definition_variant = std::variant<
        utl::Mutable_wrapper<Function_info>,
        utl::Mutable_wrapper<Module_info>,
        utl::Mutable_wrapper<Enumeration_info>,
        utl::Mutable_wrapper<Typeclass_info>,
        utl::Mutable_wrapper<Alias_info>>;

    using Info_arena = utl::Wrapper_arena< //
        Enumeration_info,
        Typeclass_info,
        Alias_info,
        Function_info,
        Module_info>;

    using Environment_arena   = utl::Wrapper_arena<Environment>;
    using Environment_wrapper = utl::Mutable_wrapper<Environment>;

    struct Arenas {
        Info_arena        info_arena;
        Environment_arena environment_arena;
        ast::Node_arena   ast_node_arena;
        hir::Node_arena   hir_node_arena;

        static auto defaults() -> Arenas;

        auto type(hir::Type::Variant) -> utl::Mutable_wrapper<hir::Type::Variant>;
        auto mutability(hir::Mutability::Variant) -> utl::Mutable_wrapper<hir::Mutability::Variant>;
    };

    struct Import {
        std::filesystem::file_time_type last_write_time;
        std::filesystem::path           module_path;
        kieli::Name_lower               name;
    };

    struct Function_with_resolved_signature {
        hir::Function::Signature      signature;
        utl::Wrapper<ast::Expression> unresolved_body;
    };
} // namespace libresolve

struct libresolve::Function_info {
    using Variant = std::variant<
        ast::definition::Function, //
        Function_with_resolved_signature,
        hir::Function>;
    Variant             variant;
    Environment_wrapper environment;
    kieli::Name_lower   name;
    bool                currently_resolving {};
};

struct libresolve::Enumeration_info {
    using Variant = std::variant<ast::definition::Enumeration, hir::Enumeration>;
    Variant             variant;
    Environment_wrapper environment;
    kieli::Name_upper   name;
    bool                currently_resolving {};
};

struct libresolve::Typeclass_info {
    using Variant = std::variant<ast::definition::Typeclass, hir::Typeclass>;
    Variant             variant;
    Environment_wrapper environment;
    kieli::Name_upper   name;
    bool                currently_resolving {};
};

struct libresolve::Alias_info {
    using Variant = std::variant<ast::definition::Alias, hir::Alias>;
    Variant             variant;
    Environment_wrapper environment;
    kieli::Name_upper   name;
    bool                currently_resolving {};
};

struct libresolve::Module_info {
    using Variant = std::variant<ast::definition::Submodule, Import, hir::Module>;
    Variant             variant;
    Environment_wrapper environment;
    kieli::Name_lower   name;
};

struct libresolve::Scope {
    struct Variable_bind {
        kieli::Name_lower       name;
        hir::Type               type;
        hir::Mutability         mutability;
        hir::Local_variable_tag tag;
        bool                    unused = true;
    };

    struct Type_bind {
        kieli::Name_upper name;
        hir::Type         type;
        bool              unused = true;
    };

    struct Mutability_bind {
        kieli::Name_lower name;
        hir::Mutability   mutability;
        bool              unused = true;
    };
private:
    utl::Flatmap<kieli::Identifier, Variable_bind>   m_variables;
    utl::Flatmap<kieli::Identifier, Type_bind>       m_types;
    utl::Flatmap<kieli::Identifier, Mutability_bind> m_mutabilities;
    Scope*                                           m_parent {};
public:
    auto bind_mutability(kieli::Identifier, Mutability_bind, kieli::Diagnostics&) -> void;
    auto bind_variable(kieli::Identifier, Variable_bind, kieli::Diagnostics&) -> void;
    auto bind_type(kieli::Identifier, Type_bind, kieli::Diagnostics&) -> void;

    auto find_mutability(kieli::Identifier) -> Mutability_bind*;
    auto find_variable(kieli::Identifier) -> Variable_bind*;
    auto find_type(kieli::Identifier) -> Type_bind*;

    auto child() -> Scope;
};

struct libresolve::Environment {
    utl::Flatmap<kieli::Identifier, Upper_info> upper_map;
    utl::Flatmap<kieli::Identifier, Lower_info> lower_map;
    std::vector<Definition_variant>             in_order;
    std::optional<Environment_wrapper>          parent;

    auto find_lower(kieli::Name_lower) -> std::optional<Lower_info>;
    auto find_upper(kieli::Name_upper) -> std::optional<Upper_info>;
};
