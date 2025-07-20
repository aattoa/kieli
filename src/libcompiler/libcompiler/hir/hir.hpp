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
            std::vector<Pattern> fields;
        };

        struct Slice {
            std::vector<Pattern> elements;
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
              Wildcard,
              db::Integer,
              db::Floating,
              db::Boolean,
              db::String,
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
            std::vector<Expression_id> elements;
        };

        struct Tuple {
            std::vector<Expression_id> fields;
        };

        struct Loop {
            Expression_id body;
        };

        struct Break {
            Expression_id result;
        };

        struct Continue {};

        struct Block {
            std::vector<Expression_id> effects;
            Expression_id              result;
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

        struct Constructor_reference {
            Constructor_id id;
        };

        struct Function_call {
            Expression_id              invocable;
            std::vector<Expression_id> arguments;
        };

        struct Initializer {
            Constructor_id             constructor;
            std::vector<Expression_id> arguments;
        };

        struct Tuple_field {
            Expression_id base;
            std::size_t   index {};
        };

        struct Struct_field {
            Expression_id base;
            Field_id      id;
        };

        struct Return {
            Expression_id result;
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
              expr::Constructor_reference,
              expr::Function_call,
              expr::Initializer,
              expr::Tuple_field,
              expr::Struct_field,
              expr::Return,
              expr::Sizeof,
              expr::Addressof,
              expr::Deref,
              expr::Defer> {
        using variant::variant;
    };

    struct Expression {
        Expression_variant  variant;
        Type_id             type_id;
        Mutability_id       mut_id;
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

        struct Structure {
            db::Upper    name;
            Structure_id id;
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
              type::Structure,
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

    struct Template_argument : std::variant<db::Error, Expression, Type, Mutability> {
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
        std::vector<Template_parameter> template_parameters;
        std::vector<Function_parameter> parameters;
        Type                            return_type;
        Type                            function_type;
        db::Lower                       name;
    };

    struct Struct_constructor {
        std::unordered_map<utl::String_id, Field_id, utl::Hash_vector_index> fields;
    };

    struct Tuple_constructor {
        std::vector<Type> types;
        Type_id           function_type_id;
    };

    struct Unit_constructor {};

    struct Constructor_body
        : std::variant<Struct_constructor, Tuple_constructor, Unit_constructor> {
        using variant::variant;
    };

    struct Structure {
        Constructor_id     constructor_id;
        db::Environment_id associated_env_id;
    };

    struct Enumeration {
        std::vector<db::Symbol_id> constructor_ids;
        db::Environment_id         associated_env_id;
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
    };

    struct Local_mutability {
        db::Lower     name;
        Mutability_id mut_id;
    };

    struct Local_type {
        db::Upper name;
        Type_id   type_id;
    };

    struct Function_info {
        ast::Function                     ast;
        std::optional<Function_signature> signature;
        std::optional<Expression_id>      body_id;
        db::Environment_id                env_id;
        db::Document_id                   doc_id;
        db::Lower                         name;
    };

    struct Constructor_info {
        Constructor_body body;
        db::Upper        name;
        Type_id          owner_type_id;
        std::size_t      discriminant {};
    };

    struct Field_info {
        db::Lower     name;
        Type          type;
        db::Symbol_id symbol_id;
        std::size_t   field_index {};
    };

    struct Structure_info {
        ast::Struct              ast;
        std::optional<Structure> hir;
        Type_id                  type_id;
        db::Environment_id       env_id;
        db::Document_id          doc_id;
        db::Upper                name;
    };

    struct Enumeration_info {
        ast::Enum                  ast;
        std::optional<Enumeration> hir;
        Type_id                    type_id;
        db::Environment_id         env_id;
        db::Document_id            doc_id;
        db::Upper                  name;
    };

    struct Concept_info {
        ast::Concept           ast;
        std::optional<Concept> hir;
        db::Environment_id     env_id;
        db::Document_id        doc_id;
        db::Upper              name;
    };

    struct Alias_info {
        ast::Alias           ast;
        std::optional<Alias> hir;
        db::Environment_id   env_id;
        db::Document_id      doc_id;
        db::Upper            name;
    };

    struct Module_info {
        db::Environment_id mod_env_id;
        db::Environment_id env_id;
        db::Document_id    doc_id;
        db::Lower          name;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression>             expressions;
        utl::Index_vector<Pattern_id, Pattern>                   patterns;
        utl::Index_vector<Type_id, Type_variant>                 types;
        utl::Index_vector<Mutability_id, Mutability_variant>     mutabilities;
        utl::Index_vector<Module_id, Module_info>                modules;
        utl::Index_vector<Function_id, Function_info>            functions;
        utl::Index_vector<Structure_id, Structure_info>          structures;
        utl::Index_vector<Enumeration_id, Enumeration_info>      enumerations;
        utl::Index_vector<Constructor_id, Constructor_info>      constructors;
        utl::Index_vector<Field_id, Field_info>                  fields;
        utl::Index_vector<Concept_id, Concept_info>              concepts;
        utl::Index_vector<Alias_id, Alias_info>                  aliases;
        utl::Index_vector<Local_variable_id, Local_variable>     local_variables;
        utl::Index_vector<Local_mutability_id, Local_mutability> local_mutabilities;
        utl::Index_vector<Local_type_id, Local_type>             local_types;
    };

    // Get the name of a built-in integer type.
    auto integer_name(type::Integer type) -> std::string_view;

    // Get the type of an expression.
    auto expression_type(Expression const& expression) -> Type;

    // Get the type of a pattern.
    auto pattern_type(Pattern const& pattern) -> Type;

    // Get a one-word description of the constructor kind.
    auto describe_constructor(Constructor_body const& body) -> std::string_view;

    void format_to(std::string&, Arena const&, utl::String_pool const&, Expression const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Expression_id const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Pattern const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Pattern_id const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Type const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Type_id const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Type_variant const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Mutability const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Mutability_id const&);
    void format_to(std::string&, Arena const&, utl::String_pool const&, Mutability_variant const&);

    auto to_string(Arena const& arena, utl::String_pool const& pool, auto const& x) -> std::string
        requires requires(std::string output) { hir::format_to(output, arena, pool, x); }
    {
        std::string output;
        hir::format_to(output, arena, pool, x);
        return output;
    };

} // namespace ki::hir

template <>
struct std::hash<ki::hir::Template_parameter_tag> {
    static constexpr auto operator()(ki::hir::Template_parameter_tag tag) noexcept -> std::size_t
    {
        return static_cast<std::size_t>(tag.value);
    }
};

#endif // KIELI_LIBCOMPILER_HIR
