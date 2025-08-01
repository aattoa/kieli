#ifndef KIELI_LIBCOMPILER_CST
#define KIELI_LIBCOMPILER_CST

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

/*

    The Concrete Syntax Tree (CST) is the highest level structured
    representation of a program's syntax. It is produced by parsing a sequence of
    tokens. Any syntactically valid program can be represented as a CST, but such
    a program may still be erroneous in other ways, and such errors can only be
    revealed by subsequent compilation steps.

    For example, the following expression is syntactically valid, and can thus
    be represented by a CST node, but it will be rejected upon expression
    resolution due to the obvious type error:

        let x: Int = "hello"

*/

namespace ki::cst {

    template <typename T>
    struct Surrounded {
        T          value;
        lsp::Range open_token;
        lsp::Range close_token;
    };

    template <typename T>
    struct Separated {
        std::vector<T>          elements;
        std::vector<lsp::Range> separator_tokens;
    };

    struct Type_annotation {
        Type_id    type;
        lsp::Range colon_token;
    };

    struct Wildcard {
        lsp::Range underscore_token;
    };

    struct Parameterized_mutability {
        db::Lower  name;
        lsp::Range question_mark_token;
    };

    struct Mutability_variant : std::variant<db::Mutability, Parameterized_mutability> {
        using variant::variant;
    };

    struct Mutability {
        Mutability_variant variant;
        lsp::Range         range;
        lsp::Range         keyword_token;
    };

    struct Template_argument : std::variant<Type_id, Expression_id, Mutability, Wildcard> {
        using variant::variant;
    };

    using Template_arguments = Surrounded<Separated<Template_argument>>;

    struct Path_segment {
        std::optional<Template_arguments> template_arguments;
        db::Name                          name;
        std::optional<lsp::Range>         leading_double_colon_token;
    };

    struct Path_root_global {
        lsp::Range double_colon_token;
    };

    using Path_root = std::variant<std::monostate, Path_root_global, Type_id>;

    struct Path {
        Path_root                 root;
        std::vector<Path_segment> segments;
        lsp::Range                range;

        [[nodiscard]] auto head() const -> Path_segment const&;
        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    template <typename T>
    struct Default_argument {
        std::variant<T, Wildcard> variant;
        lsp::Range                equals_sign_token;
    };

    using Type_parameter_default_argument       = Default_argument<Type_id>;
    using Value_parameter_default_argument      = Default_argument<Expression_id>;
    using Mutability_parameter_default_argument = Default_argument<Mutability>;

    struct Function_parameter {
        Pattern_id                                      pattern;
        std::optional<Type_annotation>                  type;
        std::optional<Value_parameter_default_argument> default_argument;
    };

    using Function_parameters = Surrounded<Separated<Function_parameter>>;
    using Function_arguments  = Surrounded<Separated<Expression_id>>;

    struct Template_type_parameter {
        db::Upper                                      name;
        std::optional<lsp::Range>                      colon_token;
        Separated<Path>                                concepts;
        std::optional<Type_parameter_default_argument> default_argument;
    };

    struct Template_value_parameter {
        db::Lower                                       name;
        Type_annotation                                 type_annotation;
        std::optional<Value_parameter_default_argument> default_argument;
    };

    struct Template_mutability_parameter {
        db::Lower                                            name;
        lsp::Range                                           colon_token;
        lsp::Range                                           mut_token;
        std::optional<Mutability_parameter_default_argument> default_argument;
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

    using Template_parameters = Surrounded<Separated<Template_parameter>>;

    struct Struct_field_equals {
        lsp::Range    equals_sign_token;
        Expression_id expression;
    };

    struct Field_init {
        db::Lower                          name;
        std::optional<Struct_field_equals> equals;
    };

    struct Match_arm {
        Pattern_id                pattern;
        Expression_id             handler;
        lsp::Range                arrow_token;
        std::optional<lsp::Range> semicolon_token;
    };

    namespace expr {
        struct Paren {
            Surrounded<Expression_id> expression;
        };

        struct Array {
            Surrounded<Separated<Expression_id>> elements;
        };

        struct Tuple {
            Surrounded<Separated<Expression_id>> fields;
        };

        struct Block {
            struct Side_effect {
                Expression_id expression;
                lsp::Range    trailing_semicolon_token;
            };

            std::vector<Side_effect>     effects;
            std::optional<Expression_id> result;
            lsp::Range                   open_brace_token;
            lsp::Range                   close_brace_token;
        };

        struct Function_call {
            Function_arguments arguments;
            Expression_id      invocable;
        };

        struct Struct_init {
            Path                              path;
            Surrounded<Separated<Field_init>> fields;
        };

        struct Infix_call {
            Expression_id left;
            Expression_id right;
            db::Name      op;
        };

        struct Struct_field {
            Expression_id base;
            db::Lower     name;
            lsp::Range    dot_token;
        };

        struct Tuple_field {
            Expression_id base;
            std::uint16_t index {};
            lsp::Range    index_token;
            lsp::Range    dot_token;
        };

        struct Array_index {
            Expression_id             base;
            Surrounded<Expression_id> index;
            lsp::Range                dot_token;
        };

        struct Method_call {
            Function_arguments                function_arguments;
            std::optional<Template_arguments> template_arguments;
            Expression_id                     expression;
            db::Lower                         name;
        };

        struct False_branch {
            Expression_id body;
            lsp::Range    keyword_token;
        };

        struct Conditional {
            Expression_id               condition;
            Expression_id               true_branch;
            std::optional<False_branch> false_branch;
            lsp::Range                  keyword_token;
        };

        struct Match {
            Surrounded<std::vector<Match_arm>> arms;
            Expression_id                      scrutinee;
            lsp::Range                         match_token;
        };

        struct Ascription {
            Expression_id expression;
            lsp::Range    colon_token;
            Type_id       type;
        };

        struct Let {
            Pattern_id                     pattern;
            std::optional<Type_annotation> type;
            Expression_id                  initializer;
            lsp::Range                     let_token;
            lsp::Range                     equals_sign_token;
        };

        struct Type_alias {
            db::Upper  name;
            Type_id    type;
            lsp::Range alias_token;
            lsp::Range equals_sign_token;
        };

        struct Loop {
            Expression_id body;
            lsp::Range    loop_token;
        };

        struct While_loop {
            Expression_id condition;
            Expression_id body;
            lsp::Range    while_token;
        };

        struct For_loop {
            Pattern_id    iterator;
            Expression_id iterable;
            Expression_id body;
            lsp::Range    for_token;
            lsp::Range    in_token;
        };

        struct Continue {
            lsp::Range continue_token;
        };

        struct Break {
            std::optional<Expression_id> result;
            lsp::Range                   break_token;
        };

        struct Return {
            std::optional<Expression_id> expression;
            lsp::Range                   ret_token;
        };

        struct Sizeof {
            Surrounded<Type_id> type;
            lsp::Range          sizeof_token;
        };

        struct Addressof {
            std::optional<Mutability> mutability;
            Expression_id             expression;
            lsp::Range                ampersand_token;
        };

        struct Deref {
            Expression_id expression;
            lsp::Range    asterisk_token;
        };

        struct Defer {
            Expression_id expression;
            lsp::Range    defer_token;
        };
    } // namespace expr

    struct Expression_variant
        : std::variant<
              Wildcard,
              db::Error,
              db::Integer,
              db::Floating,
              db::Boolean,
              db::String,
              Path,
              expr::Paren,
              expr::Array,
              expr::Tuple,
              expr::Block,
              expr::Function_call,
              expr::Struct_init,
              expr::Infix_call,
              expr::Struct_field,
              expr::Tuple_field,
              expr::Array_index,
              expr::Method_call,
              expr::Conditional,
              expr::Match,
              expr::Ascription,
              expr::Let,
              expr::Type_alias,
              expr::Loop,
              expr::While_loop,
              expr::For_loop,
              expr::Continue,
              expr::Break,
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
        struct Paren {
            Surrounded<Pattern_id> pattern;
        };

        struct Name {
            db::Lower                 name;
            std::optional<Mutability> mutability;
        };

        struct Equals {
            lsp::Range equals_sign_token;
            Pattern_id pattern;
        };

        struct Field {
            db::Lower             name;
            std::optional<Equals> equals;
        };

        struct Unit_constructor {};

        struct Struct_constructor {
            Surrounded<Separated<Field>> fields;
        };

        struct Tuple_constructor {
            Surrounded<Separated<Pattern_id>> fields;
        };

        struct Constructor_body
            : std::variant<Unit_constructor, Struct_constructor, Tuple_constructor> {
            using variant::variant;
        };

        struct Constructor {
            Path             path;
            Constructor_body body;
        };

        struct Tuple {
            Surrounded<Separated<Pattern_id>> fields;
        };

        struct Top_level_tuple {
            Separated<Pattern_id> fields;
        };

        struct Slice {
            Surrounded<Separated<Pattern_id>> elements;
        };

        struct Guarded {
            Pattern_id    pattern;
            Expression_id guard;
            lsp::Range    if_token;
        };
    } // namespace patt

    struct Pattern_variant
        : std::variant<
              Wildcard,
              db::Integer,
              db::Floating,
              db::Boolean,
              db::String,
              patt::Paren,
              patt::Name,
              patt::Constructor,
              patt::Tuple,
              patt::Top_level_tuple,
              patt::Slice,
              patt::Guarded> {
        using variant::variant;
    };

    struct Pattern {
        Pattern_variant variant;
        lsp::Range      range;
    };

    namespace type {
        struct Paren {
            Surrounded<Type_id> type;
        };

        struct Never {
            lsp::Range exclamation_token;
        };

        struct Tuple {
            Surrounded<Separated<Type_id>> field_types;
        };

        struct Array {
            Type_id       element_type;
            Expression_id length;
            lsp::Range    open_bracket_token;
            lsp::Range    close_bracket_token;
            lsp::Range    semicolon_token;
        };

        struct Slice {
            Surrounded<Type_id> element_type;
        };

        struct Function {
            Surrounded<Separated<Type_id>> parameter_types;
            Type_annotation                return_type;
            lsp::Range                     fn_token;
        };

        struct Typeof {
            Surrounded<Expression_id> expression;
            lsp::Range                typeof_token;
        };

        struct Reference {
            std::optional<Mutability> mutability;
            Type_id                   referenced_type;
            lsp::Range                ampersand_token;
        };

        struct Pointer {
            std::optional<Mutability> mutability;
            Type_id                   pointee_type;
            lsp::Range                asterisk_token;
        };

        struct Impl {
            Separated<Path> concepts;
            lsp::Range      impl_token;
        };
    } // namespace type

    struct Type_variant
        : std::variant<
              Wildcard,
              Path,
              type::Paren,
              type::Never,
              type::Tuple,
              type::Array,
              type::Slice,
              type::Function,
              type::Typeof,
              type::Impl,
              type::Reference,
              type::Pointer> {
        using variant::variant;
    };

    struct Type {
        Type_variant variant;
        lsp::Range   range;
    };

    struct Function_signature {
        std::optional<Template_parameters> template_parameters;
        Function_parameters                function_parameters;
        std::optional<Type_annotation>     return_type;
        db::Lower                          name;
        lsp::Range                         fn_token;
    };

    struct Type_signature {
        std::optional<Template_parameters> template_parameters;
        Separated<Path>                    concepts;
        db::Upper                          name;
        std::optional<lsp::Range>          concepts_colon_token;
        lsp::Range                         alias_token;
    };

    using Concept_requirement = std::variant<Function_signature, Type_signature>;

    struct Function {
        Function_signature        signature;
        Expression_id             body;
        std::optional<lsp::Range> equals_sign_token;
        lsp::Range                fn_token;
        lsp::Range                range;
    };

    struct Field {
        db::Lower       name;
        Type_annotation type;
        lsp::Range      range;
    };

    struct Struct_constructor {
        Surrounded<Separated<Field>> fields;
    };

    struct Tuple_constructor {
        Surrounded<Separated<Type_id>> types;
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

    struct Struct {
        std::optional<Template_parameters> template_parameters;
        Constructor                        constructor;
        lsp::Range                         struct_token;
        lsp::Range                         range;
    };

    struct Enum {
        std::optional<Template_parameters> template_parameters;
        Separated<Constructor>             constructors;
        db::Upper                          name;
        lsp::Range                         enum_token;
        lsp::Range                         equals_sign_token;
        lsp::Range                         range;
    };

    struct Alias {
        std::optional<Template_parameters> template_parameters;
        db::Upper                          name;
        Type_id                            type;
        lsp::Range                         alias_token;
        lsp::Range                         equals_sign_token;
        lsp::Range                         range;
    };

    struct Concept {
        std::optional<Template_parameters> template_parameters;
        std::vector<Concept_requirement>   requirements;
        db::Upper                          name;
        lsp::Range                         concept_token;
        lsp::Range                         open_brace_token;
        lsp::Range                         close_brace_token;
        lsp::Range                         range;
    };

    struct Impl_begin {
        std::optional<Template_parameters> template_parameters;
        Type_id                            self_type;
        lsp::Range                         impl_token;
        lsp::Range                         open_brace_token;
        lsp::Range                         range;
    };

    struct Submodule_begin {
        db::Lower  name;
        lsp::Range module_token;
        lsp::Range open_brace_token;
        lsp::Range range;
    };

    struct Block_end {
        lsp::Range range;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression> expressions;
        utl::Index_vector<Type_id, Type>             types;
        utl::Index_vector<Pattern_id, Pattern>       patterns;
    };

} // namespace ki::cst

#endif // KIELI_LIBCOMPILER_CST
