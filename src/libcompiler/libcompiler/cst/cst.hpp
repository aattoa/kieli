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

    // Since tokens are used all over the CST, this saves a significant amount of memory.
    struct Range_id : utl::Vector_index<Range_id, std::uint32_t> {
        using Vector_index::Vector_index;
    };

    template <typename T>
    struct Surrounded {
        T        value;
        Range_id open_token;
        Range_id close_token;
    };

    template <typename T>
    struct Separated {
        std::vector<T>        elements;
        std::vector<Range_id> separator_tokens;
    };

    struct Type_annotation {
        Type_id  type;
        Range_id colon_token;
    };

    struct Wildcard {
        Range_id underscore_token;
    };

    struct Parameterized_mutability {
        db::Lower name;
        Range_id  question_mark_token;
    };

    struct Mutability_variant : std::variant<db::Mutability, Parameterized_mutability> {
        using variant::variant;
    };

    struct Mutability {
        Mutability_variant variant;
        Range_id           range;
        Range_id           keyword_token;
    };

    struct Template_argument : std::variant<Type_id, Expression_id, Mutability, Wildcard> {
        using variant::variant;
    };

    using Template_arguments = Surrounded<Separated<Template_argument>>;

    struct Path_segment {
        std::optional<Template_arguments> template_arguments;
        db::Name                          name;
        std::optional<Range_id>           leading_double_colon_token;
    };

    struct Path_root_global {
        Range_id global_token;
    };

    using Path_root = std::variant<std::monostate, Path_root_global, Type_id>;

    struct Path {
        Path_root                 root;
        std::vector<Path_segment> segments;
        Range_id                  range;

        [[nodiscard]] auto head() const -> Path_segment const&;
        [[nodiscard]] auto is_unqualified() const noexcept -> bool;
    };

    template <typename T>
    struct Default_argument {
        std::variant<T, Wildcard> variant;
        Range_id                  equals_sign_token;
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
        std::optional<Range_id>                        colon_token;
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
        Range_id                                             colon_token;
        Range_id                                             mut_token;
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
        Range_id                   range;
    };

    using Template_parameters = Surrounded<Separated<Template_parameter>>;

    struct Struct_field_equals {
        Range_id      equals_sign_token;
        Expression_id expression;
    };

    struct Struct_field_init {
        db::Lower                          name;
        std::optional<Struct_field_equals> equals;
    };

    struct Match_arm {
        Pattern_id              pattern;
        Expression_id           handler;
        Range_id                arrow_token;
        std::optional<Range_id> semicolon_token;
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
                Range_id      trailing_semicolon_token;
            };

            std::vector<Side_effect>     side_effects;
            std::optional<Expression_id> result_expression;
            Range_id                     open_brace_token;
            Range_id                     close_brace_token;
        };

        struct Function_call {
            Function_arguments arguments;
            Expression_id      invocable;
        };

        struct Tuple_init {
            Path                                 path;
            Surrounded<Separated<Expression_id>> fields;
        };

        struct Struct_init {
            Path                                     path;
            Surrounded<Separated<Struct_field_init>> fields;
        };

        struct Infix_call {
            Expression_id  left;
            Expression_id  right;
            utl::String_id op;
            Range_id       op_token;
        };

        struct Struct_field {
            Expression_id base_expression;
            db::Lower     name;
            Range_id      dot_token;
        };

        struct Tuple_field {
            Expression_id base_expression;
            std::uint16_t field_index {};
            Range_id      field_index_token;
            Range_id      dot_token;
        };

        struct Array_index {
            Expression_id             base_expression;
            Surrounded<Expression_id> index_expression;
            Range_id                  dot_token;
        };

        struct Method_call {
            Function_arguments                function_arguments;
            std::optional<Template_arguments> template_arguments;
            Expression_id                     base_expression;
            db::Lower                         method_name;
        };

        struct False_branch {
            Expression_id body;
            Range_id      keyword_token;
        };

        struct Conditional {
            Expression_id               condition;
            Expression_id               true_branch;
            std::optional<False_branch> false_branch;
            Range_id                    keyword_token;
            bool                        is_elif {};
        };

        struct Match {
            Surrounded<std::vector<Match_arm>> arms;
            Expression_id                      scrutinee;
            Range_id                           match_token;
        };

        struct Ascription {
            Expression_id base_expression;
            Range_id      colon_token;
            Type_id       type;
        };

        struct Let {
            Pattern_id                     pattern;
            std::optional<Type_annotation> type;
            Expression_id                  initializer;
            Range_id                       let_token;
            Range_id                       equals_sign_token;
        };

        struct Type_alias {
            db::Upper name;
            Type_id   type;
            Range_id  alias_token;
            Range_id  equals_sign_token;
        };

        struct Loop {
            Expression_id body;
            Range_id      loop_token;
        };

        struct While_loop {
            Expression_id condition;
            Expression_id body;
            Range_id      while_token;
        };

        struct For_loop {
            Pattern_id    iterator;
            Expression_id iterable;
            Expression_id body;
            Range_id      for_token;
            Range_id      in_token;
        };

        struct Continue {
            Range_id continue_token;
        };

        struct Break {
            std::optional<Expression_id> result;
            Range_id                     break_token;
        };

        struct Return {
            std::optional<Expression_id> expression;
            Range_id                     ret_token;
        };

        struct Sizeof {
            Surrounded<Type_id> type;
            Range_id            sizeof_token;
        };

        struct Addressof {
            std::optional<Mutability> mutability;
            Expression_id             expression;
            Range_id                  ampersand_token;
        };

        struct Deref {
            Expression_id expression;
            Range_id      asterisk_token;
        };

        struct Defer {
            Expression_id expression;
            Range_id      defer_token;
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
              expr::Paren,
              expr::Array,
              expr::Tuple,
              expr::Block,
              expr::Function_call,
              expr::Tuple_init,
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
        Range_id           range;
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
            Range_id   equals_sign_token;
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
            Surrounded<Pattern_id> pattern;
        };

        struct Constructor_body
            : std::variant<Unit_constructor, Struct_constructor, Tuple_constructor> {
            using variant::variant;
        };

        struct Constructor {
            Path             path;
            Constructor_body body;
        };

        struct Abbreviated_constructor {
            db::Upper        name;
            Constructor_body body;
            Range_id         double_colon_token;
        };

        struct Tuple {
            Surrounded<Separated<Pattern_id>> patterns;
        };

        struct Top_level_tuple {
            Separated<Pattern_id> patterns;
        };

        struct Slice {
            Surrounded<Separated<Pattern_id>> patterns;
        };

        struct Guarded {
            Pattern_id    pattern;
            Expression_id guard;
            Range_id      if_token;
        };
    } // namespace patt

    struct Pattern_variant
        : std::variant<
              db::Integer,
              db::Floating,
              db::Boolean,
              db::String,
              Wildcard,
              patt::Paren,
              patt::Name,
              patt::Constructor,
              patt::Abbreviated_constructor,
              patt::Tuple,
              patt::Top_level_tuple,
              patt::Slice,
              patt::Guarded> {
        using variant::variant;
    };

    struct Pattern {
        Pattern_variant variant;
        Range_id        range;
    };

    namespace type {
        struct Paren {
            Surrounded<Type_id> type;
        };

        struct Never {
            Range_id exclamation_token;
        };

        struct Tuple {
            Surrounded<Separated<Type_id>> field_types;
        };

        struct Array {
            Type_id       element_type;
            Expression_id length;
            Range_id      open_bracket_token;
            Range_id      close_bracket_token;
            Range_id      semicolon_token;
        };

        struct Slice {
            Surrounded<Type_id> element_type;
        };

        struct Function {
            Surrounded<Separated<Type_id>> parameter_types;
            Type_annotation                return_type;
            Range_id                       fn_token;
        };

        struct Typeof {
            Surrounded<Expression_id> expression;
            Range_id                  typeof_token;
        };

        struct Reference {
            std::optional<Mutability> mutability;
            Type_id                   referenced_type;
            Range_id                  ampersand_token;
        };

        struct Pointer {
            std::optional<Mutability> mutability;
            Type_id                   pointee_type;
            Range_id                  asterisk_token;
        };

        struct Impl {
            Separated<Path> concepts;
            Range_id        impl_token;
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
        Range_id     range;
    };

    struct Function_signature {
        std::optional<Template_parameters> template_parameters;
        Function_parameters                function_parameters;
        std::optional<Type_annotation>     return_type;
        db::Lower                          name;
        Range_id                           fn_token;
    };

    struct Type_signature {
        std::optional<Template_parameters> template_parameters;
        Separated<Path>                    concepts;
        db::Upper                          name;
        std::optional<Range_id>            concepts_colon_token;
        Range_id                           alias_token;
    };

    using Concept_requirement = std::variant<Function_signature, Type_signature>;

    struct Function {
        Function_signature      signature;
        Expression_id           body;
        std::optional<Range_id> equals_sign_token;
        Range_id                fn_token;
    };

    struct Field {
        db::Lower       name;
        Type_annotation type;
        Range_id        range;
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
        Constructor_body                   body;
        db::Upper                          name;
        Range_id                           struct_token;
    };

    struct Enum {
        std::optional<Template_parameters> template_parameters;
        Separated<Constructor>             constructors;
        db::Upper                          name;
        Range_id                           enum_token;
        Range_id                           equals_sign_token;
    };

    struct Alias {
        std::optional<Template_parameters> template_parameters;
        db::Upper                          name;
        Type_id                            type;
        Range_id                           alias_token;
        Range_id                           equals_sign_token;
    };

    struct Concept {
        std::optional<Template_parameters> template_parameters;
        std::vector<Concept_requirement>   requirements;
        db::Upper                          name;
        Range_id                           concept_token;
        Range_id                           open_brace_token;
        Range_id                           close_brace_token;
    };

    struct Impl {
        std::optional<Template_parameters>  template_parameters;
        Surrounded<std::vector<Definition>> definitions;
        Type_id                             self_type;
        Range_id                            impl_token;
    };

    struct Submodule {
        std::optional<Template_parameters>  template_parameters;
        Surrounded<std::vector<Definition>> definitions;
        db::Lower                           name;
        Range_id                            module_token;
    };

    struct Definition_variant
        : std::variant<Function, Struct, Enum, Alias, Concept, Impl, Submodule> {
        using variant::variant;
    };

    struct Definition {
        Definition_variant variant;
        Range_id           range;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression> expressions;
        utl::Index_vector<Type_id, Type>             types;
        utl::Index_vector<Pattern_id, Pattern>       patterns;
        utl::Index_vector<Range_id, lsp::Range>      ranges;
    };

    struct Import {
        Separated<db::Lower> segments;
        Range_id             import_token;
    };

    struct Module {
        std::vector<cst::Import>     imports;
        std::vector<cst::Definition> definitions;
    };

} // namespace ki::cst

#endif // KIELI_LIBCOMPILER_CST
