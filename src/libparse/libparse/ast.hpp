#pragma once

#include <libutl/common/utilities.hpp>
#include <libutl/common/wrapper.hpp>
#include <libutl/source/source.hpp>
#include <libutl/diagnostics/diagnostics.hpp>
#include <libutl/common/flatmap.hpp>
#include <liblex/token.hpp>


/*

    The Abstract Syntax Tree (AST) is a high-level structured representation of
    a program's syntax. It is produced by parsing a sequence of tokens. Any
    syntactically valid program can be represented as an AST, but such a program
    may still be erroneous in other ways, and such errors can only be revealed
    by subsequent compilation steps.

    For example, the following expression is syntactically valid, and can thus
    be represented as an AST node, but it will be rejected upon expression
    resolution due to the obvious type error:

        let x: Int = "hello"

*/


namespace ast {

    struct Mutability {
        struct Concrete {
            bool is_mutable = false;
        };
        struct Parameterized {
            compiler::Identifier identifier;
        };
        using Variant = std::variant<Concrete, Parameterized>;

        Variant          value;
        utl::Source_view source_view;

        [[nodiscard]] auto was_explicitly_specified() const noexcept -> bool;
    };


    struct [[nodiscard]] Expression;
    struct [[nodiscard]] Pattern;
    struct [[nodiscard]] Type;
    struct [[nodiscard]] Definition;


    template <class T>
    concept tree_configuration = requires {
        typename T::Expression;
        typename T::Pattern;
        typename T::Type;
        typename T::Definition;
        requires std::is_empty_v<T>;
    };


    struct [[nodiscard]] Function_argument;


    struct Name {
        compiler::Identifier identifier;
        utl::Strong<bool>    is_upper;
        utl::Source_view     source_view;

        [[nodiscard]] auto operator==(Name const&) const noexcept -> bool;
    };

    template <tree_configuration Configuration>
    struct Basic_template_argument {
        struct Wildcard {
            utl::Source_view source_view;
        };
        using Variant = std::variant<
            utl::Wrapper<typename Configuration::Type>,
            utl::Wrapper<typename Configuration::Expression>,
            Mutability,
            Wildcard>;
        Variant            value;
        tl::optional<Name> name;
    };

    template <tree_configuration Configuration>
    struct Basic_qualifier {
        tl::optional<std::vector<Basic_template_argument<Configuration>>> template_arguments;
        Name                                                              name;
        utl::Source_view                                                  source_view;
    };

    struct Global_root_qualifier {};

    template <tree_configuration Configuration>
    struct Basic_root_qualifier {
        using Variant = std::variant<
            std::monostate,                              // id, id::id
            Global_root_qualifier,                       // global::id
            utl::Wrapper<typename Configuration::Type>>; // Type::id
        Variant value;
    };

    template <tree_configuration Configuration>
    struct Basic_qualified_name {
        std::vector<Basic_qualifier<Configuration>> middle_qualifiers;
        Basic_root_qualifier<Configuration>         root_qualifier;
        Name                                        primary_name;

        [[nodiscard]]
        auto is_unqualified() const noexcept -> bool {
            return middle_qualifiers.empty()
                && std::holds_alternative<std::monostate>(root_qualifier.value);
        }
    };

    template <tree_configuration Configuration>
    struct Basic_class_reference {
        tl::optional<std::vector<Basic_template_argument<Configuration>>> template_arguments;
        Basic_qualified_name<Configuration>                               name;
        utl::Source_view                                                  source_view;
    };

    template <tree_configuration Configuration>
    struct Basic_template_parameter {
        struct Type_parameter {
            std::vector<Basic_class_reference<Configuration>> classes;
        };
        struct Value_parameter {
            tl::optional<utl::Wrapper<typename Configuration::Type>> type;
        };
        struct Mutability_parameter {};

        using Variant = std::variant<
            Type_parameter,
            Value_parameter,
            Mutability_parameter>;

        Variant                                              value;
        Name                                                 name;
        tl::optional<Basic_template_argument<Configuration>> default_argument;
        utl::Source_view                                     source_view;
    };

    template <tree_configuration Configuration>
    struct Basic_function_parameter {
        typename Configuration::Pattern                  pattern;
        tl::optional<typename Configuration::Type>       type;
        tl::optional<typename Configuration::Expression> default_argument;
    };


    struct AST_configuration {
        using Expression  = ast::Expression;
        using Pattern     = ast::Pattern;
        using Type        = ast::Type;
        using Definition  = ast::Definition;
    };
    static_assert(tree_configuration<AST_configuration>);

    using Template_argument  = Basic_template_argument  <AST_configuration>;
    using Qualifier          = Basic_qualifier          <AST_configuration>;
    using Root_qualifier     = Basic_root_qualifier     <AST_configuration>;
    using Qualified_name     = Basic_qualified_name     <AST_configuration>;
    using Class_reference    = Basic_class_reference    <AST_configuration>;
    using Template_parameter = Basic_template_parameter <AST_configuration>;
    using Function_parameter = Basic_function_parameter <AST_configuration>;



    namespace expression {
        template <class T>
        struct Literal {
            T value;
        };
        struct Array_literal {
            std::vector<Expression> elements;
        };
        struct Self {};
        struct Variable {
            Qualified_name name;
        };
        struct Template_application {
            std::vector<Template_argument> template_arguments;
            Qualified_name                 name;
        };
        struct Tuple {
            std::vector<Expression> fields;
        };
        struct Block {
            std::vector<Expression>                side_effect_expressions;
            tl::optional<utl::Wrapper<Expression>> result_expression;
        };
        struct Invocation {
            std::vector<Function_argument> arguments;
            utl::Wrapper<Expression>       invocable;
        };
        struct Struct_initializer {
            utl::Flatmap<Name, utl::Wrapper<Expression>> member_initializers;
            utl::Wrapper<Type>                           struct_type;
        };
        struct Binary_operator_invocation {
            utl::Wrapper<Expression> left;
            utl::Wrapper<Expression> right;
            compiler::Operator       op;
        };
        struct Struct_field_access {
            utl::Wrapper<Expression> base_expression;
            Name                     field_name;
        };
        struct Tuple_field_access {
            utl::Wrapper<Expression> base_expression;
            utl::Usize               field_index {};
            utl::Source_view         field_index_source_view;
        };
        struct Array_index_access {
            utl::Wrapper<Expression> base_expression;
            utl::Wrapper<Expression> index_expression;
        };
        struct Method_invocation {
            std::vector<Function_argument>               arguments;
            tl::optional<std::vector<Template_argument>> template_arguments;
            utl::Wrapper<Expression>                     base_expression;
            Name                                         method_name;
        };
        struct Conditional {
            utl::Wrapper<Expression>               condition;
            utl::Wrapper<Expression>               true_branch;
            tl::optional<utl::Wrapper<Expression>> false_branch;
        };
        struct Match {
            struct Case {
                utl::Wrapper<Pattern>    pattern;
                utl::Wrapper<Expression> handler;
            };
            std::vector<Case>        cases;
            utl::Wrapper<Expression> matched_expression;
        };
        struct Type_cast {
            enum class Kind {
                conversion,
                ascription,
            };
            utl::Wrapper<Expression> expression;
            utl::Wrapper<Type>       target_type;
            Kind                     cast_kind = Kind::conversion;
        };
        struct Let_binding {
            utl::Wrapper<Pattern>            pattern;
            utl::Wrapper<Expression>         initializer;
            tl::optional<utl::Wrapper<Type>> type;
        };
        struct Conditional_let {
            utl::Wrapper<Pattern>    pattern;
            utl::Wrapper<Expression> initializer;
        };
        struct Local_type_alias {
            compiler::Identifier identifier;
            utl::Wrapper<Type>   aliased_type;
        };
        struct Lambda {
            struct Capture {
                struct By_pattern {
                    utl::Wrapper<Pattern>    pattern;
                    utl::Wrapper<Expression> expression;
                };
                struct By_reference {
                    compiler::Identifier variable;
                };
                using Variant = std::variant<By_pattern, By_reference>;

                Variant          value;
                utl::Source_view source_view;
            };
            utl::Wrapper<Expression>        body;
            std::vector<Function_parameter> parameters;
            std::vector<Capture>            explicit_captures;
        };
        struct Infinite_loop {
            tl::optional<Name>       label;
            utl::Wrapper<Expression> body;
        };
        struct While_loop {
            tl::optional<Name>       label;
            utl::Wrapper<Expression> condition;
            utl::Wrapper<Expression> body;
        };
        struct For_loop {
            tl::optional<Name>       label;
            utl::Wrapper<Pattern>    iterator;
            utl::Wrapper<Expression> iterable;
            utl::Wrapper<Expression> body;
        };
        struct Continue {};
        struct Break {
            tl::optional<Name>                     label;
            tl::optional<utl::Wrapper<Expression>> result;
        };
        struct Discard {
            utl::Wrapper<Expression> discarded_expression;
        };
        struct Ret {
            tl::optional<utl::Wrapper<Expression>> returned_expression;
        };
        struct Sizeof {
            utl::Wrapper<Type> inspected_type;
        };
        struct Reference {
            Mutability               mutability;
            utl::Wrapper<Expression> referenced_expression;
        };
        struct Dereference {
            utl::Wrapper<Expression> dereferenced_expression;
        };
        struct Addressof {
            utl::Wrapper<Expression> lvalue;
        };
        struct Unsafe_dereference {
            utl::Wrapper<Expression> pointer;
        };
        struct Placement_init {
            utl::Wrapper<Expression> lvalue;
            utl::Wrapper<Expression> initializer;
        };
        struct Move {
            utl::Wrapper<Expression> lvalue;
        };
        struct Meta {
            utl::Wrapper<Expression> expression;
        };
        struct Hole {};
    }


    struct Expression {
        using Variant = std::variant<
            expression::Literal<kieli::Integer>,
            expression::Literal<kieli::Floating>,
            expression::Literal<kieli::Character>,
            expression::Literal<kieli::Boolean>,
            expression::Literal<compiler::String>,
            expression::Array_literal,
            expression::Self,
            expression::Variable,
            expression::Template_application,
            expression::Tuple,
            expression::Block,
            expression::Invocation,
            expression::Struct_initializer,
            expression::Binary_operator_invocation,
            expression::Struct_field_access,
            expression::Tuple_field_access,
            expression::Array_index_access,
            expression::Method_invocation,
            expression::Conditional,
            expression::Match,
            expression::Type_cast,
            expression::Let_binding,
            expression::Conditional_let,
            expression::Local_type_alias,
            expression::Lambda,
            expression::Infinite_loop,
            expression::While_loop,
            expression::For_loop,
            expression::Continue,
            expression::Break,
            expression::Discard,
            expression::Ret,
            expression::Sizeof,
            expression::Reference,
            expression::Dereference,
            expression::Addressof,
            expression::Unsafe_dereference,
            expression::Placement_init,
            expression::Move,
            expression::Meta,
            expression::Hole>;

        Variant          value;
        utl::Source_view source_view;
    };



    namespace pattern {
        template <class T>
        using Literal = expression::Literal<T>;
        struct Wildcard {};
        struct Name {
            compiler::Identifier identifier;
            Mutability           mutability;
        };
        struct Constructor {
            Qualified_name                      constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };
        struct Abbreviated_constructor {
            ast::Name                           constructor_name;
            tl::optional<utl::Wrapper<Pattern>> payload_pattern;
        };
        struct Tuple {
            std::vector<Pattern> field_patterns;
        };
        struct Slice {
            std::vector<Pattern> element_patterns;
        };
        struct As {
            Name                  alias;
            utl::Wrapper<Pattern> aliased_pattern;  
        };
        struct Guarded {
            utl::Wrapper<Pattern> guarded_pattern;
            Expression            guard;
        };
    }


    struct Pattern {
        using Variant = std::variant<
            pattern::Literal<kieli::Integer>,
            pattern::Literal<kieli::Floating>,
            pattern::Literal<kieli::Character>,
            pattern::Literal<kieli::Boolean>,
            pattern::Literal<compiler::String>,
            pattern::Wildcard,
            pattern::Name,
            pattern::Constructor,
            pattern::Abbreviated_constructor,
            pattern::Tuple,
            pattern::Slice,
            pattern::As,
            pattern::Guarded>;

        Variant          value;
        utl::Source_view source_view;
    };



    namespace type {
        enum class Integer {
            i8, i16, i32, i64,
            u8, u16, u32, u64,
            _enumerator_count,
        };
        template <class>
        struct Primitive {};
        using Floating  = Primitive<utl::Float>;
        using Character = Primitive<utl::Char>;
        using Boolean   = Primitive<bool>;
        using String    = Primitive<compiler::String>;
        struct Wildcard {};
        struct Self {};
        struct Typename {
            Qualified_name name;
        };
        struct Tuple {
            std::vector<Type> field_types;
        };
        struct Array {
            utl::Wrapper<Type>       element_type;
            utl::Wrapper<Expression> array_length;
        };
        struct Slice {
            utl::Wrapper<Type> element_type;
        };
        struct Function {
            std::vector<Type>  argument_types;
            utl::Wrapper<Type> return_type;
        };
        struct Typeof {
            utl::Wrapper<Expression> inspected_expression;
        };
        struct Reference {
            utl::Wrapper<Type> referenced_type;
            Mutability         mutability;
        };
        struct Pointer {
            utl::Wrapper<Type> pointed_to_type;
            Mutability         mutability;
        };
        struct Instance_of {
            std::vector<Class_reference> classes;
        };
        struct Template_application {
            std::vector<Template_argument> arguments;
            Qualified_name                 name;
        };
    }


    struct Type {
        using Variant = std::variant<
            type::Integer,
            type::Floating,
            type::Character,
            type::Boolean,
            type::String,
            type::Wildcard,
            type::Self,
            type::Typename,
            type::Tuple,
            type::Array,
            type::Slice,
            type::Function,
            type::Typeof,
            type::Instance_of,
            type::Reference,
            type::Pointer,
            type::Template_application>;

        Variant          value;
        utl::Source_view source_view;
    };



    struct [[nodiscard]] Self_parameter {
        Mutability        mutability;
        utl::Strong<bool> is_reference;
        utl::Source_view  source_view;
    };


    template <tree_configuration Configuration>
    struct Basic_function_signature {
        std::vector<Basic_function_parameter<Configuration>> parameters;
        tl::optional<Self_parameter>                         self_parameter;
        tl::optional<typename Configuration::Type>           return_type;
        Name                                                 name;
    };

    template <tree_configuration Configuration>
    struct Basic_function_template_signature {
        Basic_function_signature<Configuration>              function_signature;
        std::vector<Basic_template_parameter<Configuration>> template_parameters;
    };

    template <tree_configuration Configuration>
    struct Basic_type_signature {
        std::vector<Basic_class_reference<Configuration>> classes;
        Name                                              name;
    };

    template <tree_configuration Configuration>
    struct Basic_type_template_signature {
        Basic_type_signature<Configuration>                  type_signature;
        std::vector<Basic_template_parameter<Configuration>> template_parameters;
    };

    using Function_signature          = Basic_function_signature          <AST_configuration>;
    using Function_template_signature = Basic_function_template_signature <AST_configuration>;
    using Type_signature              = Basic_type_signature              <AST_configuration>;
    using Type_template_signature     = Basic_type_template_signature     <AST_configuration>;


    namespace definition {
        template <tree_configuration Configuration>
        struct Basic_function {
            Basic_function_signature<Configuration> signature;
            typename Configuration::Expression      body;
        };
        template <tree_configuration Configuration>
        struct Basic_struct_member {
            Name                         name;
            typename Configuration::Type type;
            utl::Strong<bool>            is_public;
            utl::Source_view             source_view;
        };
        template <tree_configuration Configuration>
        struct Basic_struct {
            using Member = Basic_struct_member<Configuration>;
            std::vector<Member> members;
            Name                name;
        };
        template <tree_configuration Configuration>
        struct Basic_enum_constructor {
            Name                                       name;
            tl::optional<typename Configuration::Type> payload_type;
            utl::Source_view                           source_view;
        };
        template <tree_configuration Configuration>
        struct Basic_enum {
            using Constructor = Basic_enum_constructor<Configuration>;
            std::vector<Constructor> constructors;
            Name                     name;
        };
        template <tree_configuration Configuration>
        struct Basic_alias {
            Name                         name;
            typename Configuration::Type type;
        };
        template <tree_configuration Configuration>
        struct Basic_typeclass {
            std::vector<Basic_function_signature         <Configuration>> function_signatures;
            std::vector<Basic_function_template_signature<Configuration>> function_template_signatures;
            std::vector<Basic_type_signature             <Configuration>> type_signatures;
            std::vector<Basic_type_template_signature    <Configuration>> type_template_signatures;
            Name                                                          name;
        };
        template <tree_configuration Configuration>
        struct Basic_implementation {
            typename Configuration::Type                    type;
            std::vector<typename Configuration::Definition> definitions;
        };
        template <tree_configuration Configuration>
        struct Basic_instantiation {
            Basic_class_reference<Configuration>            typeclass;
            typename Configuration::Type                    self_type;
            std::vector<typename Configuration::Definition> definitions;
        };
        template <tree_configuration Configuration>
        struct Basic_namespace {
            std::vector<typename Configuration::Definition> definitions;
            Name                                            name;
        };

        using Function       = Basic_function       <AST_configuration>;
        using Struct         = Basic_struct         <AST_configuration>;
        using Enum           = Basic_enum           <AST_configuration>;
        using Alias          = Basic_alias          <AST_configuration>;
        using Typeclass      = Basic_typeclass      <AST_configuration>;
        using Implementation = Basic_implementation <AST_configuration>;
        using Instantiation  = Basic_instantiation  <AST_configuration>;
        using Namespace      = Basic_namespace      <AST_configuration>;

        template <class>
        struct Template;

        template <tree_configuration Configuration, template <tree_configuration> class Definition>
        struct Template<Definition<Configuration>> {
            Definition<Configuration>                            definition;
            std::vector<Basic_template_parameter<Configuration>> parameters;
        };

        using Function_template       = Template<Function>;
        using Struct_template         = Template<Struct>;
        using Enum_template           = Template<Enum>;
        using Alias_template          = Template<Alias>;
        using Typeclass_template      = Template<Typeclass>;
        using Implementation_template = Template<Implementation>;
        using Instantiation_template  = Template<Instantiation>;
        using Namespace_template      = Template<Namespace>;
    }


    template <tree_configuration Configuration>
    struct Basic_definition {
        using Variant = std::variant<
            definition::Basic_function       <Configuration>,
            definition::Basic_struct         <Configuration>,
            definition::Basic_enum           <Configuration>,
            definition::Basic_alias          <Configuration>,
            definition::Basic_typeclass      <Configuration>,
            definition::Basic_implementation <Configuration>,
            definition::Basic_instantiation  <Configuration>,
            definition::Basic_namespace      <Configuration>,

            definition::Template<definition::Basic_function       <Configuration>>,
            definition::Template<definition::Basic_struct         <Configuration>>,
            definition::Template<definition::Basic_enum           <Configuration>>,
            definition::Template<definition::Basic_alias          <Configuration>>,
            definition::Template<definition::Basic_typeclass      <Configuration>>,
            definition::Template<definition::Basic_implementation <Configuration>>,
            definition::Template<definition::Basic_instantiation  <Configuration>>,
            definition::Template<definition::Basic_namespace      <Configuration>>>;

        Variant          value;
        utl::Source_view source_view;

        Basic_definition(Variant&& value, utl::Source_view const view) noexcept
            : value { std::move(value) }
            , source_view { view } {}
    };

    struct Definition : Basic_definition<AST_configuration> {
        using Basic_definition::Basic_definition;
        using Basic_definition::operator=;
    };


    struct Function_argument {
        Expression         expression;
        tl::optional<Name> name;
    };


    template <class T>
    concept node = utl::one_of<T, Expression, Type, Pattern>;

    using Node_arena = utl::Wrapper_arena<Expression, Type, Pattern>;

    struct [[nodiscard]] Module {
        std::vector<Definition>        definitions;
        tl::optional<compiler::String> name;
        std::vector<compiler::String>  imports;
    };

}


DECLARE_FORMATTER_FOR(ast::Expression);
DECLARE_FORMATTER_FOR(ast::Pattern);
DECLARE_FORMATTER_FOR(ast::Type);

DECLARE_FORMATTER_FOR(ast::Function_parameter);
DECLARE_FORMATTER_FOR(ast::Mutability);
DECLARE_FORMATTER_FOR(ast::Name);
DECLARE_FORMATTER_FOR(ast::Module);
DECLARE_FORMATTER_FOR(ast::expression::Type_cast::Kind);


// vvv These are defined and explicitly instantiated in hir/shared_formatting.cpp to avoid repetition vvv

template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_template_argument<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_qualified_name<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_class_reference<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_template_parameter<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::definition::Basic_struct_member<Configuration>);
template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::definition::Basic_enum_constructor<Configuration>);

template <ast::tree_configuration Configuration>
DECLARE_FORMATTER_FOR_TEMPLATE(ast::Basic_definition<Configuration>);

template <>
struct fmt::formatter<ast::Definition> : fmt::formatter<ast::Basic_definition<ast::AST_configuration>> {
    auto format(ast::Definition const& definition, fmt::format_context& context) {
        return fmt::formatter<ast::Basic_definition<ast::AST_configuration>>::format(definition, context);
    }
};
