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
        utl::Wrapper<Environment>                             environment;
        kieli::Name_upper                                     name;
        bool                                                  currently_resolving {};
    };

    struct Enumeration_info {
        std::variant<ast::definition::Enum, hir::Enumeration> variant;
        utl::Wrapper<Environment>                             environment;
        bool                                                  currently_resolving {};
    };

    struct Typeclass_info {
        std::variant<ast::definition::Typeclass, hir::Typeclass> variant;
        utl::Wrapper<Environment>                                environment;
        bool                                                     currently_resolving {};
    };

    struct Alias_info {
        std::variant<ast::definition::Alias, hir::Alias> variant;
        utl::Wrapper<Environment>                        environment;
        bool                                             currently_resolving {};
    };

    struct Function_info {
        std::variant<ast::definition::Function, hir::Function> variant;
        utl::Wrapper<Environment>                              environment;
        bool                                                   currently_resolving {};
    };

    struct Namespace_info {
        std::variant<ast::definition::Namespace, hir::Namespace> variant;
        utl::Wrapper<Environment>                                environment;
        bool                                                     currently_resolving {};
    };

    using Lower_info_variant
        = std::variant<utl::Mutable_wrapper<Function_info>, utl::Mutable_wrapper<Namespace_info>>;

    using Upper_info_variant = std::variant<
        utl::Mutable_wrapper<Structure_info>,
        utl::Mutable_wrapper<Enumeration_info>,
        utl::Mutable_wrapper<Typeclass_info>,
        utl::Mutable_wrapper<Alias_info>>;

    struct Module_interface {};

} // namespace libresolve

struct libresolve::Scope {};

struct libresolve::Environment {
    utl::Flatmap<kieli::Identifier, Upper_info_variant> upper_map;
    utl::Flatmap<kieli::Identifier, Lower_info_variant> lower_map;
    std::optional<utl::Wrapper<Environment>>            parent;

    auto find_lower(kieli::Name_lower) -> std::optional<Lower_info_variant>;
    auto find_upper(kieli::Name_upper) -> std::optional<Upper_info_variant>;
};
