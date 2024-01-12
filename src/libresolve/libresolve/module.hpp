#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libphase/phase.hpp>
#include <libdesugar/ast.hpp>
#include <libresolve/hir.hpp>

namespace libresolve {

    struct Scope;
    struct Environment;

    struct Structure_info {
        std::variant<ast::definition::Struct, hir::Structure> variant;
        utl::Mutable_wrapper<Environment>                     environment;
        kieli::Name_upper                                     name;
        bool                                                  currently_resolving {};
    };

    struct Enumeration_info {
        std::variant<ast::definition::Enum, hir::Enumeration> variant;
        utl::Mutable_wrapper<Environment>                     environment;
        kieli::Name_upper                                     name;
        bool                                                  currently_resolving {};
    };

    struct Typeclass_info {
        std::variant<ast::definition::Typeclass, hir::Typeclass> variant;
        utl::Mutable_wrapper<Environment>                        environment;
        kieli::Name_upper                                        name;
        bool                                                     currently_resolving {};
    };

    struct Alias_info {
        std::variant<ast::definition::Alias, hir::Alias> variant;
        utl::Mutable_wrapper<Environment>                environment;
        kieli::Name_upper                                name;
        bool                                             currently_resolving {};
    };

    struct Function_info {
        std::variant<ast::definition::Function, hir::Function> variant;
        utl::Mutable_wrapper<Environment>                      environment;
        kieli::Name_lower                                      name;
        bool                                                   currently_resolving {};
    };

    struct Namespace_info {
        std::variant<ast::definition::Namespace, hir::Namespace> variant;
        utl::Mutable_wrapper<Environment>                        environment;
        kieli::Name_lower                                        name;
    };

    struct Lower_info {
        using Variant = std::variant<
            utl::Mutable_wrapper<Function_info>, //
            utl::Mutable_wrapper<Namespace_info>>;
        kieli::Name_lower name;
        Variant           variant;
    };

    struct Upper_info {
        using Variant = std::variant<
            utl::Mutable_wrapper<Structure_info>,
            utl::Mutable_wrapper<Enumeration_info>,
            utl::Mutable_wrapper<Typeclass_info>,
            utl::Mutable_wrapper<Alias_info>>;
        kieli::Name_upper name;
        Variant           variant;
    };

    using Info_arena = utl::Wrapper_arena<
        Structure_info,
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
