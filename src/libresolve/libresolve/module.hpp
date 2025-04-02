#ifndef KIELI_LIBRESOLVE_MODULE
#define KIELI_LIBRESOLVE_MODULE

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libcompiler/hir/hir.hpp>

namespace ki::resolve {

    namespace cst = ki::cst;
    namespace ast = ki::ast;
    namespace hir = ki::hir;

    struct Lower_info {
        using Variant = std::variant<hir::Function_id, hir::Module_id>;
        Lower       name;
        Document_id doc_id;
        Variant     variant;
    };

    struct Upper_info {
        using Variant = std::variant<hir::Enumeration_id, hir::Concept_id, hir::Alias_id>;
        Upper       name;
        Document_id doc_id;
        Variant     variant;
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
        Lower                   name;
        hir::Type_id            type;
        hir::Mutability         mutability;
        hir::Local_variable_tag tag;
        bool                    unused = true;
    };

    struct Type_bind {
        Upper        name;
        hir::Type_id type;
        bool         unused = true;
    };

    struct Mutability_bind {
        Lower           name;
        hir::Mutability mutability;
        bool            unused = true;
    };

    struct Function_info {
        cst::Function                          cst;
        ast::Function                          ast;
        std::optional<hir::Function_signature> signature;
        std::optional<hir::Expression>         body;
        hir::Environment_id                    env_id;
        Document_id                            doc_id;
        Lower                                  name;
    };

    struct Enumeration_info {
        std::variant<cst::Struct, cst::Enum> cst; // TODO: improve
        ast::Enumeration                     ast;
        std::optional<hir::Enumeration>      hir;
        hir::Environment_id                  env_id;
        Document_id                          doc_id;
        Upper                                name;
    };

    struct Concept_info {
        cst::Concept                cst;
        ast::Concept                ast;
        std::optional<hir::Concept> hir;
        hir::Environment_id         env_id;
        Document_id                 doc_id;
        Upper                       name;
    };

    struct Alias_info {
        cst::Alias                cst;
        ast::Alias                ast;
        std::optional<hir::Alias> hir;
        hir::Environment_id       env_id;
        Document_id               doc_id;
        Upper                     name;
    };

    struct Module_info {
        cst::Submodule      cst;
        ast::Submodule      ast;
        hir::Module         hir;
        hir::Environment_id env_id;
        Document_id         doc_id;
        Lower               name;
    };

    template <typename T>
    using Identifier_map = std::vector<std::pair<utl::String_id, T>>;

    struct Environment {
        Identifier_map<Upper_info>         upper_map;
        Identifier_map<Lower_info>         lower_map;
        std::vector<Definition_variant>    in_order;
        std::optional<hir::Environment_id> parent_id;
        Document_id                        doc_id;
    };

    struct Scope {
        Identifier_map<Variable_bind>   variables;
        Identifier_map<Type_bind>       types;
        Identifier_map<Mutability_bind> mutabilities;
        Document_id                     doc_id;
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

    auto make_scope(Document_id doc_id) -> Scope;

} // namespace ki::resolve

#endif // KIELI_LIBRESOLVE_MODULE
