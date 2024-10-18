#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libcompiler/hir/hir.hpp>

namespace libresolve {
    namespace cst = kieli::cst;
    namespace ast = kieli::ast;
    namespace hir = kieli::hir;

    struct Lower_info {
        using Variant = std::variant<hir::Function_id, hir::Module_id>;
        kieli::Lower       name;
        kieli::Document_id document_id;
        Variant            variant;
    };

    struct Upper_info {
        using Variant = std::variant<hir::Enumeration_id, hir::Concept_id, hir::Alias_id>;
        kieli::Upper       name;
        kieli::Document_id document_id;
        Variant            variant;
    };

    struct Definition_variant
        : std::variant<
              hir::Function_id,
              hir::Module_id,
              hir::Enumeration_id,
              hir::Concept_id,
              hir::Alias_id> {
        using variant::variant;
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

    struct Function_info {
        cst::definition::Function              cst;
        ast::definition::Function              ast;
        std::optional<hir::Function_signature> signature;
        std::optional<hir::Expression>         body;
        hir::Environment_id                    environment_id;
        kieli::Document_id                     document_id;
        kieli::Lower                           name;
    };

    struct Enumeration_info {
        cst::definition::Enum           cst;
        ast::definition::Enumeration    ast;
        std::optional<hir::Enumeration> hir;
        hir::Type                       type;
        hir::Environment_id             environment_id;
        kieli::Document_id              document_id;
        kieli::Upper                    name;
    };

    struct Concept_info {
        cst::definition::Concept    cst;
        ast::definition::Concept    ast;
        std::optional<hir::Concept> hir;
        hir::Environment_id         environment_id;
        kieli::Document_id          document_id;
        kieli::Upper                name;
    };

    struct Alias_info {
        cst::definition::Alias    cst;
        ast::definition::Alias    ast;
        std::optional<hir::Alias> hir;
        hir::Environment_id       environment_id;
        kieli::Document_id        document_id;
        kieli::Upper              name;
    };

    struct Module_info {
        cst::definition::Submodule cst;
        ast::definition::Submodule ast;
        hir::Module                hir;
        hir::Environment_id        environment_id;
        kieli::Document_id         document_id;
        kieli::Lower               name;
    };

    template <typename T>
    using Identifier_map = std::vector<std::pair<kieli::Identifier, T>>;

    struct Environment {
        Identifier_map<Upper_info>         upper_map;
        Identifier_map<Lower_info>         lower_map;
        std::vector<Definition_variant>    in_order;
        std::optional<hir::Environment_id> parent;
        kieli::Document_id                 document_id;
    };

    struct Scope {
        Identifier_map<Variable_bind>   variables;
        Identifier_map<Type_bind>       types;
        Identifier_map<Mutability_bind> mutabilities;
        kieli::Document_id              document_id;
        std::optional<hir::Scope_id>    parent_id;
    };

    struct Info_arena {
        utl::Index_vector<hir::Module_id, Module_info>           modules;
        utl::Index_vector<hir::Function_id, Function_info>       functions;
        utl::Index_vector<hir::Enumeration_id, Enumeration_info> enumerations;
        utl::Index_vector<hir::Concept_id, Concept_info>         concepts;
        utl::Index_vector<hir::Alias_id, Alias_info>             aliases;
        utl::Index_vector<hir::Environment_id, Environment>      environments;
        utl::Index_arena<hir::Scope_id, Scope>                   scopes;
    };
} // namespace libresolve
