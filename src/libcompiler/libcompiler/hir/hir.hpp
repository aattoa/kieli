#ifndef KIELI_LIBCOMPILER_HIR
#define KIELI_LIBCOMPILER_HIR

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

/*

   The High-level Intermediate Representation (HIR) is a fully typed representation of a program's
   syntax. It contains abstract information concerning generics, type variables, and other details
   relevant to the type-system. It is produced by resolving the AST.

*/

namespace ki::hir {

    enum struct Type_variable_kind : std::uint8_t { General, Integral };

    enum struct Expression_category : std::uint8_t { Place, Value };

    struct Template_parameter_tag {
        std::uint32_t value {};
    };

    struct Wildcard {};

    struct Mutability {
        Mutability_id id;
        lsp::Range    range;
    };

    struct Type {
        Type_id    id;
        lsp::Range range;
    };

    struct Match_arm {
        Pattern_id    pattern;
        Expression_id expression;
    };

    namespace patt {
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> patterns;
        };

        struct Name {
            utl::String_id    name_id;
            Mutability_id     mut_id;
            Local_variable_id var_id;
        };

        struct Guarded {
            Pattern_id    pattern;
            Expression_id guard;
        };
    } // namespace patt

    struct Pattern_variant
        : std::variant<
              db::Integer,
              db::Floating,
              db::Boolean,
              db::String,
              Wildcard,
              patt::Tuple,
              patt::Slice,
              patt::Name,
              patt::Guarded> {
        using variant::variant;
    };

    struct Pattern {
        Pattern_variant variant;
        Type_id         type_id;
        lsp::Range      range;
    };

    namespace expr {
        struct Array {
            std::vector<Expression> elements;
        };

        struct Tuple {
            std::vector<Expression> fields;
        };

        struct Loop {
            Expression_id body;
        };

        struct Break {
            Expression_id result;
        };

        struct Continue {};

        struct Block {
            std::vector<Expression> effects;
            Expression_id           result;
        };

        struct Let {
            Pattern_id    pattern;
            Type_id       type;
            Expression_id initializer;
        };

        struct Match {
            std::vector<Match_arm> arms;
            Expression_id          scrutinee;
        };

        struct Variable_reference {
            Local_variable_id id;
        };

        struct Function_reference {
            Function_id id;
        };

        struct Function_call {
            Expression_id              invocable;
            std::vector<Expression_id> arguments;
        };

        struct Sizeof {
            Type inspected_type;
        };

        struct Addressof {
            Mutability    mutability;
            Expression_id expression;
        };

        struct Deref {
            Expression_id expression;
        };

        struct Defer {
            Expression_id expression;
        };
    } // namespace expr

    struct Expression_variant
        : std::variant<
              db::Error,
              db::Integer,
              db::Floating,
              db::Boolean,
              db::String,
              Wildcard,
              expr::Array,
              expr::Tuple,
              expr::Loop,
              expr::Break,
              expr::Continue,
              expr::Block,
              expr::Let,
              expr::Match,
              expr::Variable_reference,
              expr::Function_reference,
              expr::Function_call,
              expr::Sizeof,
              expr::Addressof,
              expr::Deref,
              expr::Defer> {
        using variant::variant;
    };

    struct Expression {
        Expression_variant  variant;
        Type_id             type_id;
        Expression_category category;
        lsp::Range          range;
    };

    namespace type {
        enum struct Integer : std::uint8_t { I8, I16, I32, I64, U8, U16, U32, U64 };

        struct Floating {};

        struct Boolean {};

        struct Character {};

        struct String {};

        struct Array {
            Type          element_type;
            Expression_id length;
        };

        struct Slice {
            Type element_type;
        };

        struct Tuple {
            std::vector<Type> types;
        };

        struct Function {
            std::vector<Type> parameter_types;
            Type              return_type;
        };

        struct Enumeration {
            db::Upper      name;
            Enumeration_id id;
        };

        struct Reference {
            Type       referenced_type;
            Mutability mutability;
        };

        struct Pointer {
            Type       pointee_type;
            Mutability mutability;
        };

        struct Parameterized {
            Template_parameter_tag tag;
            utl::String_id         id;
        };

        struct Variable {
            Type_variable_id id;
        };
    } // namespace type

    struct Type_variant
        : std::variant<
              db::Error,
              type::Integer,
              type::Floating,
              type::Character,
              type::Boolean,
              type::String,
              type::Array,
              type::Slice,
              type::Reference,
              type::Pointer,
              type::Function,
              type::Enumeration,
              type::Tuple,
              type::Parameterized,
              type::Variable> {
        using variant::variant;
    };

    namespace mut {
        struct Parameterized {
            Template_parameter_tag tag;
        };

        struct Variable {
            Mutability_variable_id id;
        };
    } // namespace mut

    struct Mutability_variant
        : std::variant<db::Error, db::Mutability, mut::Parameterized, mut::Variable> {
        using variant::variant;
    };

    struct Template_argument : std::variant<Expression, Type, Mutability> {
        using variant::variant;
    };

    struct Template_type_parameter {
        using Default = std::variant<Type, Wildcard>;
        std::vector<Concept_id> concept_ids;
        db::Upper               name;
        std::optional<Default>  default_argument;
    };

    struct Template_mutability_parameter {
        using Default = std::variant<Mutability, Wildcard>;
        db::Lower              name;
        std::optional<Default> default_argument;
    };

    struct Template_value_parameter {
        Type      type;
        db::Lower name;
    };

    struct Template_parameter_variant
        : std::variant<
              Template_type_parameter,
              Template_mutability_parameter,
              Template_value_parameter> {
        using variant::variant;
    };

    struct Template_parameter {
        Template_parameter_variant variant;
        Template_parameter_tag     tag;
        lsp::Range                 range;
    };

    struct Function_parameter {
        Pattern_id                   pattern_id;
        Type                         type;
        std::optional<Expression_id> default_argument;
    };

    struct Function_signature {
        std::vector<Template_parameter> template_paramters;
        std::vector<Function_parameter> parameters;
        Type                            return_type;
        Type                            function_type;
        db::Lower                       name;
    };

    struct Enumeration {
        // TODO
    };

    struct Alias {
        db::Upper name;
        Type      type;
    };

    struct Concept {
        // TODO
    };

    struct Local_variable {
        db::Lower     name;
        Mutability_id mut_id;
        Type_id       type_id;
        bool          unused = true;
    };

    struct Local_mutability {
        db::Lower     name;
        Mutability_id mut_id;
        bool          unused = true;
    };

    struct Local_type {
        db::Upper name;
        Type_id   type_id;
        bool      unused = true;
    };

    struct Function_info {
        cst::Function                     cst;
        ast::Function                     ast;
        std::optional<Function_signature> signature;
        std::optional<Expression>         body;
        Environment_id                    env_id;
        db::Document_id                   doc_id;
        db::Lower                         name;
    };

    struct Enumeration_info {
        std::variant<cst::Struct, cst::Enum> cst; // TODO: improve
        ast::Enumeration                     ast;
        std::optional<Enumeration>           hir;
        Type_id                              type_id;
        Environment_id                       env_id;
        db::Document_id                      doc_id;
        db::Upper                            name;
    };

    struct Concept_info {
        cst::Concept           cst;
        ast::Concept           ast;
        std::optional<Concept> hir;
        Environment_id         env_id;
        db::Document_id        doc_id;
        db::Upper              name;
    };

    struct Alias_info {
        cst::Alias           cst;
        ast::Alias           ast;
        std::optional<Alias> hir;
        Environment_id       env_id;
        db::Document_id      doc_id;
        db::Upper            name;
    };

    struct Module_info {
        Environment_id  mod_env_id;
        Environment_id  env_id;
        db::Document_id doc_id;
        db::Lower       name;
    };

    template <typename T>
    using Identifier_map = std::unordered_map<utl::String_id, T, utl::Hash_vector_index>;

    struct Symbol
        : std::variant<
              db::Error,
              Function_id,
              Enumeration_id,
              Concept_id,
              Alias_id,
              Module_id,
              Local_variable_id,
              Local_mutability_id,
              Local_type_id> {
        using variant::variant;

        auto operator==(Symbol const&) const -> bool = default;
    };

    struct Environment {
        Identifier_map<Symbol>        map;
        std::vector<Symbol>           in_order;
        std::optional<Environment_id> parent_id;
        std::optional<utl::String_id> name_id;
        db::Document_id               doc_id;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression>             expressions;
        utl::Index_vector<Pattern_id, Pattern>                   patterns;
        utl::Index_vector<Type_id, Type_variant>                 types;
        utl::Index_vector<Mutability_id, Mutability_variant>     mutabilities;
        utl::Index_vector<Module_id, Module_info>                modules;
        utl::Index_vector<Function_id, Function_info>            functions;
        utl::Index_vector<Enumeration_id, Enumeration_info>      enumerations;
        utl::Index_vector<Concept_id, Concept_info>              concepts;
        utl::Index_vector<Alias_id, Alias_info>                  aliases;
        utl::Index_vector<Environment_id, Environment>           environments;
        utl::Index_vector<Local_variable_id, Local_variable>     local_variables;
        utl::Index_vector<Local_mutability_id, Local_mutability> local_mutabilities;
        utl::Index_vector<Local_type_id, Local_type>             local_types;
    };

    // Get a human readable description of the symbol kind.
    auto describe_symbol_kind(Symbol symbol) -> std::string_view;

    // Get the name of a built-in integer type.
    auto integer_name(type::Integer type) -> std::string_view;

    // Get the type of an expression.
    auto expression_type(Expression const& expression) -> Type;

    // Get the type of a pattern.
    auto pattern_type(Pattern const& pattern) -> Type;

    void format(Arena const&, utl::String_pool const&, Pattern const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Expression const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Type const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Type_id const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Type_variant const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Mutability const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Mutability_id const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Mutability_variant const&, std::string&);

    auto to_string(Arena const& arena, utl::String_pool const& pool, auto const& x) -> std::string
        requires requires(std::string output) { hir::format(arena, pool, x, output); }
    {
        std::string output;
        hir::format(arena, pool, x, output);
        return output;
    };

} // namespace ki::hir

#endif // KIELI_LIBCOMPILER_HIR
