#ifndef KIELI_LIBCOMPILER_HIR
#define KIELI_LIBCOMPILER_HIR

#include <libutl/utilities.hpp>
#include <libutl/string_pool.hpp>
#include <libutl/index_vector.hpp>
#include <libcompiler/compiler.hpp>

namespace ki::hir {

    enum struct Type_variable_kind : std::uint8_t { General, Integral };

    enum struct Expression_category : std::uint8_t { Place, Value };

    struct Template_parameter_tag : utl::Vector_index<Template_parameter_tag> {
        using Vector_index::Vector_index;
    };

    struct Local_variable_tag : utl::Vector_index<Local_variable_tag> {
        using Vector_index::Vector_index;
    };

    struct Wildcard {};

    struct Mutability {
        Mutability_id id;
        Range         range;
    };

    struct Type {
        Type_id id;
        Range   range;
    };

    struct Match_case {
        Pattern_id    pattern;
        Expression_id expression;
    };

    namespace pattern {
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };

        struct Slice {
            std::vector<Pattern> patterns;
        };

        struct Name {
            Mutability         mutability;
            utl::String_id     identifier;
            Local_variable_tag variable_tag;
        };

        struct Guarded {
            Pattern_id    guarded_pattern;
            Expression_id guard_expression;
        };
    } // namespace pattern

    struct Pattern_variant
        : std::variant<
              Integer,
              Floating,
              Boolean,
              String,
              Wildcard,
              pattern::Tuple,
              pattern::Slice,
              pattern::Name,
              pattern::Guarded> {
        using variant::variant;
    };

    struct Pattern {
        Pattern_variant variant;
        Type_id         type;
        Range           range;
    };

    namespace expression {
        struct Array_literal {
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
            std::vector<Expression> side_effects;
            Expression_id           result;
        };

        struct Let {
            Pattern_id    pattern;
            Type          type;
            Expression_id initializer;
        };

        struct Match {
            std::vector<Match_case> cases;
            Expression_id           expression;
        };

        struct Variable_reference {
            Lower              name;
            Local_variable_tag tag;
        };

        struct Function_reference {
            Lower       name;
            Function_id id;
        };

        struct Indirect_call {
            Expression_id              invocable;
            std::vector<Expression_id> arguments;
        };

        struct Direct_call {
            Lower                      function_name;
            Function_id                function_id;
            std::vector<Expression_id> arguments;
        };

        struct Sizeof {
            Type inspected_type;
        };

        struct Addressof {
            Mutability    mutability;
            Expression_id place_expression;
        };

        struct Dereference {
            Expression_id reference_expression;
        };

        struct Defer {
            Expression_id effect_expression;
        };
    } // namespace expression

    struct Expression_variant
        : std::variant<
              Error,
              Wildcard,
              Integer,
              Floating,
              Boolean,
              String,
              expression::Array_literal,
              expression::Tuple,
              expression::Loop,
              expression::Break,
              expression::Continue,
              expression::Block,
              expression::Let,
              expression::Match,
              expression::Variable_reference,
              expression::Function_reference,
              expression::Indirect_call,
              expression::Direct_call,
              expression::Sizeof,
              expression::Addressof,
              expression::Dereference,
              expression::Defer> {
        using variant::variant;
    };

    struct Expression {
        Expression_variant  variant;
        Type_id             type;
        Expression_category category;
        Range               range;
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
            Upper          name;
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
              Error,
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

    namespace mutability {
        struct Parameterized {
            Template_parameter_tag tag;
        };

        struct Variable {
            Mutability_variable_id id;
        };
    } // namespace mutability

    struct Mutability_variant
        : std::variant<Error, ki::Mutability, mutability::Parameterized, mutability::Variable> {
        using variant::variant;
    };

    struct Template_argument : std::variant<Expression, Type, Mutability> {
        using variant::variant;
    };

    struct Template_type_parameter {
        using Default = std::variant<Type, Wildcard>;
        std::vector<Concept_id> concept_ids;
        Upper                   name;
        std::optional<Default>  default_argument;
    };

    struct Template_mutability_parameter {
        using Default = std::variant<Mutability, Wildcard>;
        Lower                  name;
        std::optional<Default> default_argument;
    };

    struct Template_value_parameter {
        Type  type;
        Lower name;
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
        Range                      range;
    };

    struct Function_parameter {
        Pattern                   pattern;
        Type                      type;
        std::optional<Expression> default_argument;
    };

    struct Function_signature {
        std::vector<Template_parameter> template_paramters;
        std::vector<Function_parameter> parameters;
        Type                            return_type;
        Type                            function_type;
        Lower                           name;
        Scope_id                        scope_id;
    };

    struct Enumeration {
        // TODO
    };

    struct Alias {
        Upper name;
        Type  type;
    };

    struct Concept {
        // TODO
    };

    struct Module {
        Environment_id environment;
    };

    struct Arena {
        utl::Index_vector<Expression_id, Expression>         expr;
        utl::Index_vector<Type_id, Type_variant>             type;
        utl::Index_vector<Pattern_id, Pattern>               patt;
        utl::Index_vector<Mutability_id, Mutability_variant> mut;
    };

    // Get the name of a built-in integer type.
    auto integer_name(type::Integer type) -> std::string_view;

    // Get the type of an expression.
    auto expression_type(Expression const& expression) -> hir::Type;

    // Get the type of a pattern.
    auto pattern_type(Pattern const& pattern) -> hir::Type;

    void format(Arena const&, utl::String_pool const&, Pattern const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Expression const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Type const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Type_variant const&, std::string&);
    void format(Arena const&, utl::String_pool const&, Mutability const&, std::string&);
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
