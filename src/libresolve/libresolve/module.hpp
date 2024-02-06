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
            utl::Mutable_wrapper<Namespace_info>>;
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

    using Info_arena = utl::Wrapper_arena< //
        Enumeration_info,
        Typeclass_info,
        Alias_info,
        Function_info,
        Namespace_info>;

    struct Context {
        Info_arena                      info_arena;
        ast::Node_arena                 ast_node_arena;
        utl::Wrapper_arena<Environment> environment_arena;
        kieli::Compile_info&            compile_info;
    };

    struct Module {
        utl::Mutable_wrapper<Environment> root_environment;
    };

    using Module_map = utl::Flatmap<std::filesystem::path, Module>;

    auto read_module_map(Context& context, std::filesystem::path const& project_root) -> Module_map;
} // namespace libresolve

struct libresolve::Function_info {
    std::variant<ast::definition::Function, hir::Function> variant;
    utl::Mutable_wrapper<Environment>                      environment;
    kieli::Name_lower                                      name;
    bool                                                   currently_resolving {};
};

struct libresolve::Enumeration_info {
    std::variant<ast::definition::Enum, hir::Enumeration> variant;
    utl::Mutable_wrapper<Environment>                     environment;
    kieli::Name_upper                                     name;
    bool                                                  currently_resolving {};
};

struct libresolve::Typeclass_info {
    std::variant<ast::definition::Typeclass, hir::Typeclass> variant;
    utl::Mutable_wrapper<Environment>                        environment;
    kieli::Name_upper                                        name;
    bool                                                     currently_resolving {};
};

struct libresolve::Alias_info {
    std::variant<ast::definition::Alias, hir::Alias> variant;
    utl::Mutable_wrapper<Environment>                environment;
    kieli::Name_upper                                name;
    bool                                             currently_resolving {};
};

struct libresolve::Namespace_info {
    std::variant<ast::definition::Namespace, hir::Namespace> variant;
    utl::Mutable_wrapper<Environment>                        environment;
    kieli::Name_lower                                        name;
};

struct libresolve::Scope {
    // TODO
};

struct libresolve::Environment {
    utl::Flatmap<kieli::Identifier, Upper_info>      upper_map;
    utl::Flatmap<kieli::Identifier, Lower_info>      lower_map;
    std::optional<utl::Mutable_wrapper<Environment>> parent;

    auto find_lower(kieli::Name_lower) -> std::optional<Lower_info>;
    auto find_upper(kieli::Name_upper) -> std::optional<Upper_info>;
};
