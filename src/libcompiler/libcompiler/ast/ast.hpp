#ifndef KIELI_LIBCOMPILER_AST
#define KIELI_LIBCOMPILER_AST

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

/*

    The Abstract Syntax Tree (AST) is a high level structured representation
    of a program's syntax, much like the CST, just without the exact source
    information. It is produced by desugaring the CST.

    For example, the following CST node:
        while a { b }

    would be desugared to the following AST node:
        loop { if a { b } else { break () } }

*/

namespace ki::ast {

    struct Wildcard {
        lsp::Range range;
    };

    struct Parameterized_mutability {
        db::Lower name;
    };

    struct Mutability_variant : std::variant<db::Mutability, Parameterized_mutability> {
        using variant::variant;
    };

    struct Mutability {
        Mutability_variant variant;
        lsp::Range         range;
    };

    struct Template_argument : std::variant<Type_id, Expression_id, Mutability, Wildcard> {
        using variant::variant;
    };

    struct Path_segment {
        std::optional<std::vector<Template_argument>> template_arguments;
        db::Name                                      name;
    };

    struct Path_root_global {};

    using Path_root = std::variant<std::monostate, Path_root_global, Type_id>;

    struct Path {
        Path_root                 root;
        std::vector<Path_segment> segments;

        [[nodiscard]] auto head() const -> Path_segment const&;
        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    struct Template_type_parameter {
        using Default = std::variant<Type_id, Wildcard>;
        db::Upper              name;
        std::vector<Path>      concepts;
        std::optional<Default> default_argument;
    };

    struct Template_value_parameter {
        using Default = std::variant<Expression_id, Wildcard>;
        db::Lower              name;
        Type_id                type;
        std::optional<Default> default_argument;
    };

    struct Template_mutability_parameter {
        using Default = std::variant<Mutability, Wildcard>;
        db::Lower              name;
        std::optional<Default> default_argument;
    };

    struct Template_parameter_variant
        : std::variant<
              Template_type_parameter,
              Template_value_parameter,
              Template_mutability_parameter> {
        using variant::variant;
    };

    struct Template_parameter {
        Template_parameter_variant variant;
        lsp::Range                 range;
    };

    using Template_parameters = std::optional<std::vector<Template_parameter>>;

    struct Function_parameter {
        Pattern_id                   pattern;
        Type_id                      type;
        std::optional<Expression_id> default_argument;
    };

    struct Struct_field_initializer {
        db::Lower     name;
        Expression_id expression;
    };

    struct Match_arm {
        Pattern_id    pattern;
        Expression_id expression;
    };

    enum struct Loop_source : std::uint8_t { Plain_loop, While_loop, For_loop };
    enum struct Conditional_source : std::uint8_t { If, Elif, While };

    auto describe_loop_source(Loop_source source) -> std::string_view;
    auto describe_conditional_source(Conditional_source source) -> std::string_view;

    namespace expr {
        struct Array {
            std::vector<Expression> elements;
        };

        struct Tuple {
            std::vector<Expression> fields;
        };

        struct Loop {
            Expression_id body;
            Loop_source   source {};
        };

        struct Continue {};

        struct Break {
            Expression_id result;
        };

        struct Block {
            std::vector<Expression> side_effects;
            Expression_id           result;
        };

        struct Function_call {
            std::vector<Expression_id> arguments;
            Expression_id              invocable;
        };

        struct Infix_call {
            Expression_id  left;
            Expression_id  right;
            utl::String_id op;
            lsp::Range     op_range;
        };

        struct Tuple_initializer {
            Path                       constructor_path;
            std::vector<Expression_id> initializers;
        };

        struct Struct_initializer {
            Path                                  constructor_path;
            std::vector<Struct_field_initializer> initializers;
        };

        struct Struct_field {
            Expression_id base_expression;
            db::Lower     field_name;
        };

        struct Tuple_field {
            Expression_id base_expression;
            std::size_t   field_index {};
            lsp::Range    field_index_range;
        };

        struct Array_index {
            Expression_id base_expression;
            Expression_id index_expression;
        };

        struct Method_call {
            std::vector<Expression_id>                    function_arguments;
            std::optional<std::vector<Template_argument>> template_arguments;
            Expression_id                                 base_expression;
            db::Lower                                     method_name;
        };

        struct Conditional {
            Expression_id      condition;
            Expression_id      true_branch;
            Expression_id      false_branch;
            Conditional_source source {};
            bool               has_explicit_false_branch {};
        };

        struct Match {
            std::vector<Match_arm> arms;
            Expression_id          scrutinee;
        };

        struct Type_ascription {
            Expression_id expression;
            Type_id       type;
        };

        struct Let {
            Pattern_id    pattern;
            Expression_id initializer;
            Type_id       type;
        };

        struct Type_alias {
            db::Upper name;
            Type_id   type;
        };

        struct Return {
            Expression_id expression;
        };

        struct Sizeof {
            Type_id type;
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
              Path,
              Wildcard,
              expr::Array,
              expr::Tuple,
              expr::Loop,
              expr::Break,
              expr::Continue,
              expr::Block,
              expr::Function_call,
              expr::Tuple_initializer,
              expr::Struct_initializer,
              expr::Infix_call,
              expr::Struct_field,
              expr::Tuple_field,
              expr::Array_index,
              expr::Method_call,
              expr::Conditional,
              expr::Match,
              expr::Type_ascription,
              expr::Let,
              expr::Type_alias,
              expr::Return,
              expr::Sizeof,
              expr::Addressof,
              expr::Deref,
              expr::Defer> {
        using variant::variant;
    };

    struct Expression {
        Expression_variant variant;
        lsp::Range         range;
    };

    namespace patt {
        struct Name {
            db::Lower  name;
            Mutability mutability;
        };

        struct Field {
            db::Lower                 name;
            std::optional<Pattern_id> pattern;
        };

        struct Struct_constructor {
            std::vector<Field> fields;
        };

        struct Tuple_constructor {
            Pattern_id pattern;
        };

        struct Unit_constructor {};

        struct Constructor_body
            : std::variant<Struct_constructor, Tuple_constructor, Unit_constructor> {
            using variant::variant;
        };

        struct Constructor {
            Path             path;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            db::Upper        name;
            Constructor_body body;
        };

        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> element_patterns;
        };

        struct Guarded {
            Pattern_id pattern;
            Expression guard;
        };
    } // namespace patt

    struct Pattern_variant
        : std::variant<
              db::Integer,
              db::Floating,
              db::Boolean,
              db::String,
              Wildcard,
              patt::Name,
              patt::Constructor,
              patt::Abbreviated_constructor,
              patt::Tuple,
              patt::Slice,
              patt::Guarded> {
        using variant::variant;
    };

    struct Pattern {
        Pattern_variant variant;
        lsp::Range      range;
    };

    namespace type {
        struct Never {};

        struct Tuple {
            std::vector<Type> field_types;
        };

        struct Array {
            Type_id       element_type;
            Expression_id length;
        };

        struct Slice {
            Type_id element_type;
        };

        struct Function {
            std::vector<Type> parameter_types;
            Type_id           return_type;
        };

        struct Typeof {
            Expression_id expression;
        };

        struct Reference {
            Type_id    referenced_type;
            Mutability mutability;
        };

        struct Pointer {
            Type_id    pointee_type;
            Mutability mutability;
        };

        struct Impl {
            std::vector<Path> concepts;
        };
    } // namespace type

    struct Type_variant
        : std::variant<
              db::Error,
              Wildcard,
              Path,
              type::Never,
              type::Tuple,
              type::Array,
              type::Slice,
              type::Function,
              type::Typeof,
              type::Reference,
              type::Pointer,
              type::Impl> {
        using variant::variant;
    };

    struct Type {
        Type_variant variant;
        lsp::Range   range;
    };

    struct Function_signature {
        Template_parameters             template_parameters;
        std::vector<Function_parameter> function_parameters;
        Type                            return_type;
        db::Lower                       name;
    };

    struct Type_signature {
        std::vector<Path> concepts;
        db::Upper         name;
    };

    struct Function {
        Function_signature signature;
        Expression         body;
    };

    struct Field {
        db::Lower  name;
        Type       type;
        lsp::Range range;
    };

    struct Struct_constructor {
        std::vector<Field> fields;
    };

    struct Tuple_constructor {
        std::vector<Type_id> types;
    };

    struct Unit_constructor {};

    struct Constructor_body
        : std::variant<Struct_constructor, Tuple_constructor, Unit_constructor> {
        using variant::variant;
    };

    struct Constructor {
        db::Upper        name;
        Constructor_body body;
    };

    struct Enumeration {
        std::vector<Constructor> constructors;
        db::Upper                name;
        Template_parameters      template_parameters;
    };

    struct Alias {
        db::Upper           name;
        Type                type;
        Template_parameters template_parameters;
    };

    struct Concept {
        std::vector<Function_signature> function_signatures;
        std::vector<Type_signature>     type_signatures;
        db::Upper                       name;
        Template_parameters             template_parameters;
    };

    struct Impl {
        Type                    type;
        std::vector<Definition> definitions;
        Template_parameters     template_parameters;
    };

    struct Submodule {
        std::vector<Definition> definitions;
        db::Lower               name;
        Template_parameters     template_parameters;
    };

    struct Definition_variant
        : std::variant<Function, Enumeration, Alias, Concept, Impl, Submodule> {
        using variant::variant;
    };

    struct Definition {
        Definition_variant variant;
        lsp::Range         range;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression> expressions;
        utl::Index_vector<Pattern_id, Pattern>       patterns;
        utl::Index_vector<Type_id, Type>             types;
    };

} // namespace ki::ast

#endif // KIELI_LIBCOMPILER_AST
