#include <libutl/utilities.hpp>
#include <libcompiler/ast/display.hpp>

using namespace kieli::ast;

namespace {
    struct Display_state {
        std::string  output;
        std::string  indent;
        bool const   unicode = true;
        Arena const& arena;
    };

    enum class Last { no, yes };

    template <class Node>
    concept displayable = requires(Display_state& state, Node const& node) {
        { do_display(state, node) } -> std::same_as<void>;
    };

    template <class... Args>
    auto write_line(Display_state& state, std::format_string<Args...> const fmt, Args&&... args)
    {
        std::format_to(std::back_inserter(state.output), fmt, std::forward<Args>(args)...);
        state.output.push_back('\n');
    }

    auto write_node(Display_state& state, Last const last, std::invocable auto const& callback)
        -> void
    {
        std::size_t const previous_indent = state.indent.size();
        state.output.append(state.indent);
        if (last == Last::yes) {
            state.output.append(state.unicode ? "└─ " : "+- ");
            state.indent.append("   ");
        }
        else {
            state.output.append(state.unicode ? "├─ " : "|- ");
            state.indent.append(state.unicode ? "│  " : "|  ");
        }
        std::invoke(callback);
        state.indent.resize(previous_indent);
    }

    auto display_node(
        Display_state&          state,
        Last const              last,
        std::string_view const  description,
        displayable auto const& node) -> void
    {
        write_node(state, last, [&] {
            write_line(state, "{}", description);
            write_node(state, Last::yes, [&] { do_display(state, node); });
        });
    }

    template <displayable T>
    auto display_vector_node(
        Display_state&         state,
        Last const             last,
        std::string_view const description,
        std::vector<T> const&  vector) -> void
    {
        write_node(state, last, [&] {
            write_line(state, "{}", description);
            for (std::size_t i = 0; i != vector.size(); ++i) {
                write_node(state, i == vector.size() - 1 ? Last::yes : Last::no, [&] {
                    do_display(state, vector[i]);
                });
            }
        });
    }

    auto do_display(Display_state& state, Definition const& definition) -> void;
    auto do_display(Display_state& state, Expression const& expression) -> void;
    auto do_display(Display_state& state, Pattern const& pattern) -> void;
    auto do_display(Display_state& state, Type const& type) -> void;

    auto do_display(Display_state& state, Expression_id const id) -> void
    {
        do_display(state, state.arena.expressions[id]);
    }

    auto do_display(Display_state& state, Pattern_id const id) -> void
    {
        do_display(state, state.arena.patterns[id]);
    }

    auto do_display(Display_state& state, Type_id const id) -> void
    {
        do_display(state, state.arena.types[id]);
    }

    auto do_display(Display_state& state, kieli::Name const& name) -> void
    {
        write_line(state, "\"{}\"", name);
    }

    auto do_display(Display_state& state, Mutability const& mutability) -> void
    {
        auto const visitor = utl::Overload {
            [&](kieli::Mutability const& concrete) { //
                write_line(state, "concrete {}", concrete);
            },
            [&](Parameterized_mutability const& parameterized) {
                write_line(state, "parameterized {}", parameterized.name);
            },
        };
        std::visit(visitor, mutability.variant);
    }

    auto do_display(Display_state& state, Template_parameter const& parameter) -> void
    {
        auto const visitor = utl::Overload {
            [&](Template_type_parameter const& parameter) {
                write_line(state, "type parameter");
                display_node(state, Last::yes, "name", parameter.name);
            },
            [&](Template_value_parameter const& parameter) {
                write_line(state, "value parameter");
                display_node(state, Last::yes, "name", parameter.name);
            },
            [&](Template_mutability_parameter const& parameter) {
                write_line(state, "mutability parameter");
                display_node(state, Last::yes, "name", parameter.name);
            },
        };
        std::visit(visitor, parameter.variant); // TODO
    }

    auto do_display(Display_state& state, Template_argument const& argument) -> void
    {
        std::visit([&](auto const& alternative) { do_display(state, alternative); }, argument);
    }

    auto do_display(Display_state& state, Path_segment const& segment) -> void
    {
        write_line(state, "path segment");
        if (segment.template_arguments.has_value()) {
            display_vector_node(
                state, Last::no, "template arguments", segment.template_arguments.value());
        }
        display_node(state, Last::yes, "name", segment.name);
    }

    auto do_display(Display_state& state, Path_root const& root) -> void
    {
        auto const visitor = utl::Overload {
            [&](Path_root_global const&) { write_line(state, "global"); },
            [&](Type_id const type) { do_display(state, type); },
        };
        std::visit(visitor, root);
    }

    auto do_display(Display_state& state, Path const& path) -> void
    {
        write_line(state, "path");
        if (path.root.has_value()) {
            display_node(state, Last::no, "root", path.root.value());
        }
        display_vector_node(state, Last::no, "segments", path.segments);
        display_node(state, Last::yes, "head", path.head);
    }

    auto display_template_parameters_node(
        Display_state& state, Last const last, Template_parameters const& parameters) -> void
    {
        if (parameters.has_value()) {
            display_vector_node(state, last, "template parameters", parameters.value());
        }
    }

    auto do_display(Display_state& state, Loop_source const source) -> void
    {
        write_line(state, "loop source: {}", describe_loop_source(source));
    }

    auto do_display(Display_state& state, Conditional_source const source) -> void
    {
        write_line(state, "conditional source: {}", describe_conditional_source(source));
    }

    auto do_display(Display_state& state, definition::Field const& field) -> void
    {
        write_line(state, "field");
        display_node(state, Last::no, "name", field.name);
        display_node(state, Last::yes, "type", field.type);
    }

    auto do_display(Display_state& state, Struct_field_initializer const& field) -> void
    {
        write_line(state, "struct field initializer");
        display_node(state, Last::no, "name", field.name);
        display_node(state, Last::yes, "expression", field.expression);
    }

    auto do_display(Display_state& state, pattern::Field const& field) -> void
    {
        write_line(state, "field");
        if (field.pattern.has_value()) {
            display_node(state, Last::no, "pattern", field.pattern.value());
        }
        display_node(state, Last::yes, "name", field.name);
    }

    auto do_display(Display_state& state, definition::Constructor_body const& body) -> void
    {
        auto const visitor = utl::Overload {
            [&](definition::Struct_constructor const& constructor) {
                write_line(state, "struct constructor");
                display_vector_node(state, Last::yes, "fields", constructor.fields);
            },
            [&](definition::Tuple_constructor const& constructor) {
                write_line(state, "tuple constructor");
                display_vector_node(state, Last::yes, "types", constructor.types);
            },
            [&](definition::Unit_constructor const&) { write_line(state, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    auto do_display(Display_state& state, pattern::Constructor_body const& body) -> void
    {
        auto const visitor = utl::Overload {
            [&](pattern::Struct_constructor const& constructor) {
                write_line(state, "struct constructor");
                display_vector_node(state, Last::yes, "fields", constructor.fields);
            },
            [&](pattern::Tuple_constructor const& constructor) {
                write_line(state, "tuple constructor");
                display_node(state, Last::yes, "pattern", constructor.pattern);
            },
            [&](pattern::Unit_constructor const&) { write_line(state, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    auto do_display(Display_state& state, definition::Constructor const& constructor) -> void
    {
        write_line(state, "constructor");
        display_node(state, Last::no, "name", constructor.name);
        display_node(state, Last::yes, "body", constructor.body);
    }

    auto do_display(Display_state& state, Concept_reference const& reference) -> void
    {
        write_line(state, "concept reference");
        if (reference.template_arguments.has_value()) {
            display_vector_node(
                state, Last::no, "template arguments", reference.template_arguments.value());
        }
        display_node(state, Last::yes, "reference path", reference.path);
    }

    auto do_display(Display_state& state, Function_argument const& argument) -> void
    {
        write_line(state, "function argument");
        if (argument.name.has_value()) {
            display_node(state, Last::no, "name", argument.name.value());
        }
        display_node(state, Last::yes, "expression", argument.expression);
    }

    auto do_display(Display_state& state, Function_parameter const& parameter) -> void
    {
        write_line(state, "function parameter");
        if (parameter.type.has_value()) {
            display_node(state, Last::no, "type", parameter.type.value());
        }
        if (parameter.default_argument.has_value()) {
            display_node(state, Last::no, "default argument", parameter.default_argument.value());
        }
        display_node(state, Last::yes, "pattern", parameter.pattern);
    }

    auto do_display(Display_state& state, Function_signature const& signature) -> void
    {
        write_line(state, "function signature");
        display_node(state, Last::no, "name", signature.name);
        display_template_parameters_node(state, Last::no, signature.template_parameters);
        display_node(state, Last::no, "return type", signature.return_type);
        display_vector_node(state, Last::yes, "function parameters", signature.function_parameters);
    }

    auto do_display(Display_state& state, Type_signature const& signature) -> void
    {
        write_line(state, "type signature");
        display_node(state, Last::no, "name", signature.name);
        display_vector_node(state, Last::yes, "concepts", signature.concepts);
    }

    auto do_display(Display_state& state, Match_case const& match_case) -> void
    {
        write_line(state, "match case");
        display_node(state, Last::no, "pattern", match_case.pattern);
        display_node(state, Last::yes, "handler", match_case.expression);
    }

    auto do_display(Display_state& state, kieli::Identifier const identifier) -> void
    {
        write_line(state, "identifier {}", identifier);
    }

    struct Definition_display_visitor {
        Display_state& state;

        auto operator()(definition::Function const& function)
        {
            write_line(state, "function");
            display_node(state, Last::no, "signature", function.signature);
            display_node(state, Last::yes, "body", function.body);
        }

        auto operator()(definition::Enumeration const& enumeration)
        {
            write_line(state, "enumeration");
            display_node(state, Last::no, "name", enumeration.name);
            display_template_parameters_node(state, Last::no, enumeration.template_parameters);
            display_vector_node(state, Last::yes, "constructors", enumeration.constructors);
        }

        auto operator()(definition::Alias const& alias)
        {
            write_line(state, "type alias");
            display_node(state, Last::no, "name", alias.name);
            display_template_parameters_node(state, Last::no, alias.template_parameters);
            display_node(state, Last::yes, "aliased type", alias.type);
        }

        auto operator()(definition::Concept const& concept_)
        {
            write_line(state, "concept");
            display_node(state, Last::no, "name", concept_.name);
            display_template_parameters_node(state, Last::no, concept_.template_parameters);
            display_vector_node(state, Last::no, "functions", concept_.function_signatures);
            display_vector_node(state, Last::yes, "types", concept_.type_signatures);
        }

        auto operator()(definition::Implementation const& implementation)
        {
            write_line(state, "implementation");
            display_template_parameters_node(state, Last::no, implementation.template_parameters);
            display_node(state, Last::no, "type", implementation.type);
            display_vector_node(state, Last::yes, "definitions", implementation.definitions);
        }

        auto operator()(definition::Submodule const& submodule)
        {
            write_line(state, "submodule");
            display_node(state, Last::no, "name", submodule.name);
            display_template_parameters_node(state, Last::no, submodule.template_parameters);
            display_vector_node(state, Last::yes, "definitions", submodule.definitions);
        }
    };

    auto display_literal(Display_state& state, kieli::Integer const& integer) -> void
    {
        write_line(state, "integer literal {}", integer.value);
    }

    auto display_literal(Display_state& state, kieli::Floating const& floating) -> void
    {
        write_line(state, "floating point literal {}", floating.value);
    }

    auto display_literal(Display_state& state, kieli::Character const& character) -> void
    {
        if (std::isprint(static_cast<unsigned char>(character.value))) {
            write_line(state, "character literal '{}'", character.value);
        }
        else {
            write_line(state, "character literal {:#x}", static_cast<int>(character.value));
        }
    }

    auto display_literal(Display_state& state, kieli::Boolean const& boolean) -> void
    {
        write_line(state, "boolean literal {}", boolean.value);
    }

    auto display_literal(Display_state& state, kieli::String const& string) -> void
    {
        // todo: nonprintables
        write_line(state, "string literal \"{}\"", string.value);
    }

    struct Expression_display_visitor {
        Display_state& state;

        auto operator()(kieli::literal auto const& literal)
        {
            display_literal(state, literal);
        }

        auto operator()(expression::Array_literal const& array)
        {
            write_line(state, "array literal");
            display_vector_node(state, Last::yes, "elements", array.elements);
        }

        auto operator()(expression::Self const&)
        {
            write_line(state, "self reference");
        }

        auto operator()(expression::Variable const& variable)
        {
            write_line(state, "variable");
            display_node(state, Last::yes, "path", variable.path);
        }

        auto operator()(expression::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::yes, "fields", tuple.fields);
        }

        auto operator()(expression::Loop const& loop)
        {
            write_line(state, "loop");
            display_node(state, Last::no, "body", loop.body);
            display_node(state, Last::yes, "source", loop.source.get());
        }

        auto operator()(expression::Break const& break_)
        {
            write_line(state, "break");
            display_node(state, Last::yes, "result", break_.result);
        }

        auto operator()(expression::Continue const&)
        {
            write_line(state, "continue");
        }

        auto operator()(expression::Block const& block)
        {
            write_line(state, "block");
            display_vector_node(state, Last::no, "side effects", block.side_effects);
            display_node(state, Last::yes, "result", block.result);
        }

        auto operator()(expression::Function_call const& call)
        {
            write_line(state, "function call");
            display_node(state, Last::no, "invocable", call.invocable);
            display_vector_node(state, Last::yes, "arguments", call.arguments);
        }

        auto operator()(expression::Unit_initializer const& initializer)
        {
            write_line(state, "unit initializer");
            display_node(state, Last::yes, "constructor path", initializer.constructor_path);
        }

        auto operator()(expression::Tuple_initializer const& initializer)
        {
            write_line(state, "tuple initializer");
            display_node(state, Last::no, "constructor path", initializer.constructor_path);
            display_vector_node(state, Last::yes, "field initializers", initializer.initializers);
        }

        auto operator()(expression::Struct_initializer const& initializer)
        {
            write_line(state, "struct initializer");
            display_node(state, Last::no, "constructor path", initializer.constructor_path);
            display_vector_node(state, Last::yes, "field initializers", initializer.initializers);
        }

        auto operator()(expression::Binary_operator_application const& application)
        {
            write_line(state, "binary operator application");
            display_node(state, Last::no, "left operand", application.left);
            display_node(state, Last::no, "right operand", application.right);
            display_node(state, Last::yes, "operator", application.op);
        }

        auto operator()(expression::Struct_field_access const& access)
        {
            write_line(state, "struct index access");
            display_node(state, Last::no, "base expression", access.base_expression);
            display_node(state, Last::yes, "field name", access.field_name);
        }

        auto operator()(expression::Tuple_field_access const& access)
        {
            write_line(state, "tuple index access");
            display_node(state, Last::no, "base expression", access.base_expression);
            write_node(state, Last::yes, [&] {
                write_line(state, "field index {}", access.field_index.get());
            });
        }

        auto operator()(expression::Array_index_access const& access)
        {
            write_line(state, "array index access");
            display_node(state, Last::no, "base expression", access.base_expression);
            display_node(state, Last::yes, "index expression", access.index_expression);
        }

        auto operator()(expression::Method_call const& call)
        {
            write_line(state, "method call");
            display_node(state, Last::no, "method name", call.method_name);
            display_node(state, Last::no, "base expression", call.base_expression);
            if (call.template_arguments.has_value()) {
                display_vector_node(
                    state, Last::no, "template arguments", call.template_arguments.value());
            }
            display_vector_node(state, Last::yes, "method arguments", call.function_arguments);
        }

        auto operator()(expression::Conditional const& conditional)
        {
            write_line(state, "conditional");
            display_node(state, Last::no, "condition", conditional.condition);
            display_node(state, Last::no, "true branch", conditional.true_branch);
            display_node(state, Last::no, "false branch", conditional.false_branch);
            display_node(state, Last::yes, "source", conditional.source.get());
        }

        auto operator()(expression::Match const& match)
        {
            write_line(state, "match");
            display_node(state, Last::no, "expression", match.expression);
            display_vector_node(state, Last::yes, "cases", match.cases);
        }

        auto operator()(expression::Template_application const& application)
        {
            write_line(state, "template application");
            display_node(state, Last::no, "path", application.path);
            display_vector_node(
                state, Last::yes, "template arguments", application.template_arguments);
        }

        auto operator()(expression::Type_cast const& cast)
        {
            write_line(state, "type cast");
            display_node(state, Last::no, "expression", cast.expression);
            display_node(state, Last::yes, "target type", cast.target_type);
        }

        auto operator()(expression::Type_ascription const& ascription)
        {
            write_line(state, "type ascription");
            display_node(state, Last::no, "expression", ascription.expression);
            display_node(state, Last::yes, "ascribed type", ascription.ascribed_type);
        }

        auto operator()(expression::Let_binding const& let)
        {
            write_line(state, "let binding");
            display_node(state, Last::no, "type", let.type);
            display_node(state, Last::no, "pattern", let.pattern);
            display_node(state, Last::yes, "initializer", let.initializer);
        }

        auto operator()(expression::Local_type_alias const& alias)
        {
            write_line(state, "local type alias");
            display_node(state, Last::no, "name", alias.name);
            display_node(state, Last::yes, "aliased type", alias.type);
        }

        auto operator()(expression::Ret const& ret)
        {
            write_line(state, "ret");
            display_node(state, Last::yes, "returned expression", ret.returned_expression);
        }

        auto operator()(expression::Sizeof const& sizeof_)
        {
            write_line(state, "sizeof");
            display_node(state, Last::yes, "inspected type", sizeof_.inspected_type);
        }

        auto operator()(expression::Addressof const& addressof)
        {
            write_line(state, "addressof");
            display_node(state, Last::no, "reference mutability", addressof.mutability);
            display_node(state, Last::yes, "place expression", addressof.place_expression);
        }

        auto operator()(expression::Dereference const& dereference)
        {
            write_line(state, "dereference");
            display_node(
                state, Last::yes, "reference expression", dereference.reference_expression);
        }

        auto operator()(expression::Unsafe const& unsafe)
        {
            write_line(state, "unsafe");
            display_node(state, Last::yes, "expression", unsafe.expression);
        }

        auto operator()(expression::Move const& move)
        {
            write_line(state, "move");
            display_node(state, Last::yes, "place expression", move.place_expression);
        }

        auto operator()(expression::Defer const& defer)
        {
            write_line(state, "defer");
            display_node(state, Last::yes, "effect", defer.effect_expression);
        }

        auto operator()(expression::Meta const& meta)
        {
            write_line(state, "meta");
            display_node(state, Last::yes, "expression", meta.expression);
        }

        auto operator()(expression::Hole const&)
        {
            write_line(state, "hole");
        }

        auto operator()(expression::Error const&)
        {
            write_line(state, "error");
        }
    };

    struct Pattern_display_visitor {
        Display_state& state;

        auto operator()(kieli::literal auto const& literal)
        {
            display_literal(state, literal);
        }

        auto operator()(Wildcard const&)
        {
            write_line(state, "wildcard");
        }

        auto operator()(pattern::Name const& name)
        {
            write_line(state, "name");
            display_node(state, Last::no, "name", name.name);
            display_node(state, Last::yes, "mutability", name.mutability);
        }

        auto operator()(pattern::Constructor const& constructor)
        {
            write_line(state, "constructor");
            display_node(state, Last::no, "constructor path", constructor.path);
            display_node(state, Last::yes, "body", constructor.body);
        }

        auto operator()(pattern::Abbreviated_constructor const& constructor)
        {
            write_line(state, "abbreviated constructor");
            display_node(state, Last::no, "name", constructor.name);
            display_node(state, Last::yes, "body", constructor.body);
        }

        auto operator()(pattern::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::yes, "field patterns", tuple.field_patterns);
        }

        auto operator()(pattern::Slice const& slice)
        {
            write_line(state, "slice");
            display_vector_node(state, Last::yes, "element patterns", slice.element_patterns);
        }

        auto operator()(pattern::Alias const& alias)
        {
            write_line(state, "alias");
            display_node(state, Last::no, "name", alias.name);
            display_node(state, Last::no, "pattern", alias.pattern);
            display_node(state, Last::yes, "mutability", alias.mutability);
        }

        auto operator()(pattern::Guarded const& guarded)
        {
            write_line(state, "guarded");
            display_node(state, Last::no, "guarded pattern", guarded.guarded_pattern);
            display_node(state, Last::yes, "guard expression", guarded.guard_expression);
        }
    };

    struct Type_display_visitor {
        Display_state& state;

        auto operator()(kieli::type::Integer const& integer)
        {
            write_line(state, "built-in {}", kieli::type::integer_name(integer));
        }

        auto operator()(kieli::type::Floating const&)
        {
            write_line(state, "built-in floating point");
        }

        auto operator()(kieli::type::Character const&)
        {
            write_line(state, "built-in character");
        }

        auto operator()(kieli::type::Boolean const&)
        {
            write_line(state, "built-in boolean");
        }

        auto operator()(kieli::type::String const&)
        {
            write_line(state, "built-in string");
        }

        auto operator()(type::Never const&)
        {
            write_line(state, "built-in never");
        }

        auto operator()(Wildcard const&)
        {
            write_line(state, "built-in wildcard");
        }

        auto operator()(type::Self const&)
        {
            write_line(state, "implementation self");
        }

        auto operator()(type::Typename const& typename_)
        {
            write_line(state, "type name");
            display_node(state, Last::yes, "type path", typename_.path);
        }

        auto operator()(type::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::yes, "field types", tuple.field_types);
        }

        auto operator()(type::Array const& array)
        {
            write_line(state, "tuple");
            display_node(state, Last::no, "length", array.length);
            display_node(state, Last::yes, "element type", array.element_type);
        }

        auto operator()(type::Slice const& slice)
        {
            write_line(state, "slice");
            display_node(state, Last::yes, "element type", slice.element_type);
        }

        auto operator()(type::Function const& function)
        {
            write_line(state, "function");
            display_vector_node(state, Last::no, "parameter types", function.parameter_types);
            display_node(state, Last::yes, "return type", function.return_type);
        }

        auto operator()(type::Typeof const& typeof_)
        {
            write_line(state, "typeof");
            display_node(state, Last::yes, "inspected expression", typeof_.inspected_expression);
        }

        auto operator()(type::Reference const& reference)
        {
            write_line(state, "reference");
            display_node(state, Last::no, "reference mutability", reference.mutability);
            display_node(state, Last::yes, "referenced type", reference.referenced_type);
        }

        auto operator()(type::Pointer const& pointer)
        {
            write_line(state, "pointer");
            display_node(state, Last::no, "pointer mutability", pointer.mutability);
            display_node(state, Last::yes, "pointee type", pointer.pointee_type);
        }

        auto operator()(type::Implementation const& implementation)
        {
            write_line(state, "implementation");
            display_vector_node(state, Last::yes, "concepts", implementation.concepts);
        }

        auto operator()(type::Template_application const& application)
        {
            write_line(state, "template application");
            display_node(state, Last::no, "template path", application.path);
            display_vector_node(state, Last::yes, "template arguments", application.arguments);
        }
    };

    auto do_display(Display_state& state, Definition const& definition) -> void
    {
        std::visit(Definition_display_visitor { state }, definition.variant);
    }

    auto do_display(Display_state& state, Expression const& expression) -> void
    {
        std::visit(Expression_display_visitor { state }, expression.variant);
    }

    auto do_display(Display_state& state, Pattern const& pattern) -> void
    {
        std::visit(Pattern_display_visitor { state }, pattern.variant);
    }

    auto do_display(Display_state& state, Type const& type) -> void
    {
        std::visit(Type_display_visitor { state }, type.variant);
    }
} // namespace

auto kieli::ast::display(Arena const& arena, Definition const& definition) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, definition);
    return std::move(state.output);
}

auto kieli::ast::display(Arena const& arena, Expression const& expression) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, expression);
    return std::move(state.output);
}

auto kieli::ast::display(Arena const& arena, Pattern const& pattern) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, pattern);
    return std::move(state.output);
}

auto kieli::ast::display(Arena const& arena, Type const& type) -> std::string
{
    Display_state state { .arena = arena };
    do_display(state, type);
    return std::move(state.output);
}
