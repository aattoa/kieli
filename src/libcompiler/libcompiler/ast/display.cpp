#include <libutl/utilities.hpp>
#include <libcompiler/ast/display.hpp>

using namespace kieli::ast;

namespace {
    struct Display_state {
        std::string  output;
        std::string  indent;
        bool         unicode = true;
        Arena const& arena;
    };

    enum struct Last { no, yes };

    template <typename Node>
    concept displayable = requires(Display_state& state, Node const& node) {
        { do_display(state, node) } -> std::same_as<void>;
    };

    template <typename... Args>
    auto write_line(Display_state& state, std::format_string<Args...> const fmt, Args&&... args)
    {
        std::format_to(std::back_inserter(state.output), fmt, std::forward<Args>(args)...);
        state.output.push_back('\n');
    }

    void write_node(Display_state& state, Last const last, std::invocable auto const& callback)
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

    void display_node(
        Display_state&          state,
        Last const              last,
        std::string_view const  description,
        displayable auto const& node)
    {
        write_node(state, last, [&] {
            write_line(state, "{}", description);
            write_node(state, Last::yes, [&] { do_display(state, node); });
        });
    }

    template <displayable T>
    void display_vector_node(
        Display_state&         state,
        Last const             last,
        std::string_view const description,
        std::vector<T> const&  vector)
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

    void do_display(Display_state& state, Definition const& definition);
    void do_display(Display_state& state, Expression const& expression);
    void do_display(Display_state& state, Pattern const& pattern);
    void do_display(Display_state& state, Type const& type);

    void do_display(Display_state& state, Expression_id const id)
    {
        do_display(state, state.arena.expressions[id]);
    }

    void do_display(Display_state& state, Pattern_id const id)
    {
        do_display(state, state.arena.patterns[id]);
    }

    void do_display(Display_state& state, Type_id const id)
    {
        do_display(state, state.arena.types[id]);
    }

    void do_display(Display_state& state, Wildcard const&)
    {
        write_line(state, "built-in wildcard");
    }

    void do_display(Display_state& state, kieli::Name const& name)
    {
        write_line(state, "\"{}\"", name);
    }

    void do_display(Display_state& state, Mutability const& mutability)
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

    void do_display(Display_state& state, Template_argument const& argument)
    {
        std::visit([&](auto const& alternative) { do_display(state, alternative); }, argument);
    }

    void do_display(Display_state& state, Path_segment const& segment)
    {
        write_line(state, "path segment");
        if (segment.template_arguments.has_value()) {
            display_vector_node(
                state, Last::no, "template arguments", segment.template_arguments.value());
        }
        display_node(state, Last::yes, "name", segment.name);
    }

    void do_display(Display_state& state, Path_root const& root)
    {
        auto const visitor = utl::Overload {
            [&](std::monostate) { write_line(state, "none"); },
            [&](Path_root_global const&) { write_line(state, "global"); },
            [&](Type_id const type) { do_display(state, type); },
        };
        std::visit(visitor, root);
    }

    void do_display(Display_state& state, Path const& path)
    {
        write_line(state, "path");
        display_node(state, Last::no, "root", path.root);
        display_vector_node(state, Last::yes, "segments", path.segments);
    }

    void do_display(Display_state& state, Template_parameter const& parameter)
    {
        auto display_default = [&](auto const& argument) {
            if (argument.has_value()) {
                write_node(state, Last::no, [&] {
                    write_line(state, "default argument");
                    write_node(state, Last::yes, [&] {
                        auto const visitor
                            = [&](auto const& alternative) { do_display(state, alternative); };
                        std::visit(visitor, argument.value());
                    });
                });
            }
        };
        auto const visitor = utl::Overload {
            [&](Template_type_parameter const& parameter) {
                write_line(state, "type parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::no, "name", parameter.name);
                display_vector_node(state, Last::yes, "concepts", parameter.concepts);
            },
            [&](Template_value_parameter const& parameter) {
                write_line(state, "value parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::no, "name", parameter.name);
                display_node(state, Last::yes, "type", parameter.type);
            },
            [&](Template_mutability_parameter const& parameter) {
                write_line(state, "mutability parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::yes, "name", parameter.name);
            },
        };
        std::visit(visitor, parameter.variant);
    }

    void display_template_parameters_node(
        Display_state& state, Last const last, Template_parameters const& parameters)
    {
        if (parameters.has_value()) {
            display_vector_node(state, last, "template parameters", parameters.value());
        }
    }

    void do_display(Display_state& state, Loop_source const source)
    {
        write_line(state, "loop source: {}", describe_loop_source(source));
    }

    void do_display(Display_state& state, Conditional_source const source)
    {
        write_line(state, "conditional source: {}", describe_conditional_source(source));
    }

    void do_display(Display_state& state, definition::Field const& field)
    {
        write_line(state, "field");
        display_node(state, Last::no, "name", field.name);
        display_node(state, Last::yes, "type", field.type);
    }

    void do_display(Display_state& state, Struct_field_initializer const& field)
    {
        write_line(state, "struct field initializer");
        display_node(state, Last::no, "name", field.name);
        display_node(state, Last::yes, "expression", field.expression);
    }

    void do_display(Display_state& state, pattern::Field const& field)
    {
        write_line(state, "field");
        if (field.pattern.has_value()) {
            display_node(state, Last::no, "pattern", field.pattern.value());
        }
        display_node(state, Last::yes, "name", field.name);
    }

    void do_display(Display_state& state, definition::Constructor_body const& body)
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

    void do_display(Display_state& state, pattern::Constructor_body const& body)
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

    void do_display(Display_state& state, definition::Constructor const& constructor)
    {
        write_line(state, "constructor");
        display_node(state, Last::no, "name", constructor.name);
        display_node(state, Last::yes, "body", constructor.body);
    }

    void do_display(Display_state& state, Function_parameter const& parameter)
    {
        write_line(state, "function parameter");
        display_node(state, Last::no, "type", parameter.type);
        if (parameter.default_argument.has_value()) {
            display_node(state, Last::no, "default argument", parameter.default_argument.value());
        }
        display_node(state, Last::yes, "pattern", parameter.pattern);
    }

    void do_display(Display_state& state, Function_signature const& signature)
    {
        write_line(state, "function signature");
        display_node(state, Last::no, "name", signature.name);
        display_template_parameters_node(state, Last::no, signature.template_parameters);
        display_node(state, Last::no, "return type", signature.return_type);
        display_vector_node(state, Last::yes, "function parameters", signature.function_parameters);
    }

    void do_display(Display_state& state, Type_signature const& signature)
    {
        write_line(state, "type signature");
        display_node(state, Last::no, "name", signature.name);
        display_vector_node(state, Last::yes, "concepts", signature.concepts);
    }

    void do_display(Display_state& state, Match_case const& match_case)
    {
        write_line(state, "match case");
        display_node(state, Last::no, "pattern", match_case.pattern);
        display_node(state, Last::yes, "handler", match_case.expression);
    }

    void do_display(Display_state& state, definition::Function const& function)
    {
        write_line(state, "function");
        display_node(state, Last::no, "signature", function.signature);
        display_node(state, Last::yes, "body", function.body);
    }

    void do_display(Display_state& state, definition::Enumeration const& enumeration)
    {
        write_line(state, "enumeration");
        display_node(state, Last::no, "name", enumeration.name);
        display_template_parameters_node(state, Last::no, enumeration.template_parameters);
        display_vector_node(state, Last::yes, "constructors", enumeration.constructors);
    }

    void do_display(Display_state& state, definition::Alias const& alias)
    {
        write_line(state, "type alias");
        display_node(state, Last::no, "name", alias.name);
        display_template_parameters_node(state, Last::no, alias.template_parameters);
        display_node(state, Last::yes, "aliased type", alias.type);
    }

    void do_display(Display_state& state, definition::Concept const& concept_)
    {
        write_line(state, "concept");
        display_node(state, Last::no, "name", concept_.name);
        display_template_parameters_node(state, Last::no, concept_.template_parameters);
        display_vector_node(state, Last::no, "functions", concept_.function_signatures);
        display_vector_node(state, Last::yes, "types", concept_.type_signatures);
    }

    void do_display(Display_state& state, definition::Impl const& implementation)
    {
        write_line(state, "implementation");
        display_template_parameters_node(state, Last::no, implementation.template_parameters);
        display_node(state, Last::no, "type", implementation.type);
        display_vector_node(state, Last::yes, "definitions", implementation.definitions);
    }

    void do_display(Display_state& state, definition::Submodule const& submodule)
    {
        write_line(state, "submodule");
        display_node(state, Last::no, "name", submodule.name);
        display_template_parameters_node(state, Last::no, submodule.template_parameters);
        display_vector_node(state, Last::yes, "definitions", submodule.definitions);
    }

    void do_display(Display_state& state, kieli::Identifier const identifier)
    {
        write_line(state, "identifier {}", identifier);
    }

    void display_literal(Display_state& state, kieli::Integer const& integer)
    {
        write_line(state, "integer literal {}", integer.value);
    }

    void display_literal(Display_state& state, kieli::Floating const& floating)
    {
        write_line(state, "floating point literal {}", floating.value);
    }

    void display_literal(Display_state& state, kieli::Character const& character)
    {
        write_line(state, "character literal {:?}", character.value);
    }

    void display_literal(Display_state& state, kieli::Boolean const& boolean)
    {
        write_line(state, "boolean literal {}", boolean.value);
    }

    void display_literal(Display_state& state, kieli::String const& string)
    {
        write_line(state, "string literal {:?}", string.value);
    }

    struct Expression_display_visitor {
        Display_state& state;

        void operator()(kieli::literal auto const& literal)
        {
            display_literal(state, literal);
        }

        void operator()(Wildcard const& wildcard)
        {
            do_display(state, wildcard);
        }

        void operator()(Path const& path)
        {
            do_display(state, path);
        }

        void operator()(expression::Array_literal const& array)
        {
            write_line(state, "array literal");
            display_vector_node(state, Last::yes, "elements", array.elements);
        }

        void operator()(expression::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::yes, "fields", tuple.fields);
        }

        void operator()(expression::Loop const& loop)
        {
            write_line(state, "loop");
            display_node(state, Last::no, "body", loop.body);
            display_node(state, Last::yes, "source", loop.source.get());
        }

        void operator()(expression::Break const& break_)
        {
            write_line(state, "break");
            display_node(state, Last::yes, "result", break_.result);
        }

        void operator()(expression::Continue const&)
        {
            write_line(state, "continue");
        }

        void operator()(expression::Block const& block)
        {
            write_line(state, "block");
            display_vector_node(state, Last::no, "side effects", block.side_effects);
            display_node(state, Last::yes, "result", block.result);
        }

        void operator()(expression::Function_call const& call)
        {
            write_line(state, "function call");
            display_node(state, Last::no, "invocable", call.invocable);
            display_vector_node(state, Last::yes, "arguments", call.arguments);
        }

        void operator()(expression::Tuple_initializer const& initializer)
        {
            write_line(state, "tuple initializer");
            display_node(state, Last::no, "constructor path", initializer.constructor_path);
            display_vector_node(state, Last::yes, "field initializers", initializer.initializers);
        }

        void operator()(expression::Struct_initializer const& initializer)
        {
            write_line(state, "struct initializer");
            display_node(state, Last::no, "constructor path", initializer.constructor_path);
            display_vector_node(state, Last::yes, "field initializers", initializer.initializers);
        }

        void operator()(expression::Infix_call const& application)
        {
            write_line(state, "infix call");
            display_node(state, Last::no, "left operand", application.left);
            display_node(state, Last::no, "right operand", application.right);
            display_node(state, Last::yes, "operator", application.op);
        }

        void operator()(expression::Struct_field_access const& access)
        {
            write_line(state, "struct index access");
            display_node(state, Last::no, "base expression", access.base_expression);
            display_node(state, Last::yes, "field name", access.field_name);
        }

        void operator()(expression::Tuple_field_access const& access)
        {
            write_line(state, "tuple index access");
            display_node(state, Last::no, "base expression", access.base_expression);
            write_node(state, Last::yes, [&] {
                write_line(state, "field index {}", access.field_index.get());
            });
        }

        void operator()(expression::Array_index_access const& access)
        {
            write_line(state, "array index access");
            display_node(state, Last::no, "base expression", access.base_expression);
            display_node(state, Last::yes, "index expression", access.index_expression);
        }

        void operator()(expression::Method_call const& call)
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

        void operator()(expression::Conditional const& conditional)
        {
            write_line(state, "conditional");
            display_node(state, Last::no, "condition", conditional.condition);
            display_node(state, Last::no, "true branch", conditional.true_branch);
            display_node(state, Last::no, "false branch", conditional.false_branch);
            display_node(state, Last::yes, "source", conditional.source.get());
        }

        void operator()(expression::Match const& match)
        {
            write_line(state, "match");
            display_node(state, Last::no, "expression", match.expression);
            display_vector_node(state, Last::yes, "cases", match.cases);
        }

        void operator()(expression::Type_cast const& cast)
        {
            write_line(state, "type cast");
            display_node(state, Last::no, "expression", cast.expression);
            display_node(state, Last::yes, "target type", cast.target_type);
        }

        void operator()(expression::Type_ascription const& ascription)
        {
            write_line(state, "type ascription");
            display_node(state, Last::no, "expression", ascription.expression);
            display_node(state, Last::yes, "ascribed type", ascription.ascribed_type);
        }

        void operator()(expression::Let_binding const& let)
        {
            write_line(state, "let binding");
            display_node(state, Last::no, "type", let.type);
            display_node(state, Last::no, "pattern", let.pattern);
            display_node(state, Last::yes, "initializer", let.initializer);
        }

        void operator()(expression::Local_type_alias const& alias)
        {
            write_line(state, "local type alias");
            display_node(state, Last::no, "name", alias.name);
            display_node(state, Last::yes, "aliased type", alias.type);
        }

        void operator()(expression::Ret const& ret)
        {
            write_line(state, "ret");
            display_node(state, Last::yes, "returned expression", ret.returned_expression);
        }

        void operator()(expression::Sizeof const& sizeof_)
        {
            write_line(state, "sizeof");
            display_node(state, Last::yes, "inspected type", sizeof_.inspected_type);
        }

        void operator()(expression::Addressof const& addressof)
        {
            write_line(state, "addressof");
            display_node(state, Last::no, "reference mutability", addressof.mutability);
            display_node(state, Last::yes, "place expression", addressof.place_expression);
        }

        void operator()(expression::Dereference const& dereference)
        {
            write_line(state, "dereference");
            display_node(
                state, Last::yes, "reference expression", dereference.reference_expression);
        }

        void operator()(expression::Move const& move)
        {
            write_line(state, "mv");
            display_node(state, Last::yes, "place expression", move.place_expression);
        }

        void operator()(expression::Defer const& defer)
        {
            write_line(state, "defer");
            display_node(state, Last::yes, "effect", defer.effect_expression);
        }

        void operator()(Error const&)
        {
            write_line(state, "error");
        }
    };

    struct Pattern_display_visitor {
        Display_state& state;

        void operator()(kieli::literal auto const& literal)
        {
            display_literal(state, literal);
        }

        void operator()(Wildcard const& wildcard)
        {
            do_display(state, wildcard);
        }

        void operator()(pattern::Name const& name)
        {
            write_line(state, "name");
            display_node(state, Last::no, "name", name.name);
            display_node(state, Last::yes, "mutability", name.mutability);
        }

        void operator()(pattern::Constructor const& constructor)
        {
            write_line(state, "constructor");
            display_node(state, Last::no, "constructor path", constructor.path);
            display_node(state, Last::yes, "body", constructor.body);
        }

        void operator()(pattern::Abbreviated_constructor const& constructor)
        {
            write_line(state, "abbreviated constructor");
            display_node(state, Last::no, "name", constructor.name);
            display_node(state, Last::yes, "body", constructor.body);
        }

        void operator()(pattern::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::yes, "field patterns", tuple.field_patterns);
        }

        void operator()(pattern::Slice const& slice)
        {
            write_line(state, "slice");
            display_vector_node(state, Last::yes, "element patterns", slice.element_patterns);
        }

        void operator()(pattern::Alias const& alias)
        {
            write_line(state, "alias");
            display_node(state, Last::no, "name", alias.name);
            display_node(state, Last::no, "pattern", alias.pattern);
            display_node(state, Last::yes, "mutability", alias.mutability);
        }

        void operator()(pattern::Guarded const& guarded)
        {
            write_line(state, "guarded");
            display_node(state, Last::no, "guarded pattern", guarded.guarded_pattern);
            display_node(state, Last::yes, "guard expression", guarded.guard_expression);
        }
    };

    struct Type_display_visitor {
        Display_state& state;

        void operator()(Path const& path)
        {
            do_display(state, path);
        }

        void operator()(type::Never const&)
        {
            write_line(state, "built-in never");
        }

        void operator()(Wildcard const& wildcard)
        {
            do_display(state, wildcard);
        }

        void operator()(type::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::yes, "field types", tuple.field_types);
        }

        void operator()(type::Array const& array)
        {
            write_line(state, "tuple");
            display_node(state, Last::no, "length", array.length);
            display_node(state, Last::yes, "element type", array.element_type);
        }

        void operator()(type::Slice const& slice)
        {
            write_line(state, "slice");
            display_node(state, Last::yes, "element type", slice.element_type);
        }

        void operator()(type::Function const& function)
        {
            write_line(state, "function");
            display_vector_node(state, Last::no, "parameter types", function.parameter_types);
            display_node(state, Last::yes, "return type", function.return_type);
        }

        void operator()(type::Typeof const& typeof_)
        {
            write_line(state, "typeof");
            display_node(state, Last::yes, "inspected expression", typeof_.inspected_expression);
        }

        void operator()(type::Reference const& reference)
        {
            write_line(state, "reference");
            display_node(state, Last::no, "reference mutability", reference.mutability);
            display_node(state, Last::yes, "referenced type", reference.referenced_type);
        }

        void operator()(type::Pointer const& pointer)
        {
            write_line(state, "pointer");
            display_node(state, Last::no, "pointer mutability", pointer.mutability);
            display_node(state, Last::yes, "pointee type", pointer.pointee_type);
        }

        void operator()(type::Impl const& implementation)
        {
            write_line(state, "implementation");
            display_vector_node(state, Last::yes, "concepts", implementation.concepts);
        }

        void operator()(Error const&)
        {
            write_line(state, "error");
        }
    };

    void do_display(Display_state& state, Definition const& definition)
    {
        std::visit([&](auto const& def) { do_display(state, def); }, definition.variant);
    }

    void do_display(Display_state& state, Expression const& expression)
    {
        std::visit(Expression_display_visitor { state }, expression.variant);
    }

    void do_display(Display_state& state, Pattern const& pattern)
    {
        std::visit(Pattern_display_visitor { state }, pattern.variant);
    }

    void do_display(Display_state& state, Type const& type)
    {
        std::visit(Type_display_visitor { state }, type.variant);
    }

    auto display_string(Arena const& arena, auto const& object) -> std::string
    {
        Display_state state { .arena = arena };
        do_display(state, object);
        return std::move(state.output);
    }
} // namespace

auto kieli::ast::display(Arena const& arena, definition::Function const& function) -> std::string
{
    return display_string(arena, function);
}

auto kieli::ast::display(Arena const& arena, definition::Enumeration const& enumeration)
    -> std::string
{
    return display_string(arena, enumeration);
}

auto kieli::ast::display(Arena const& arena, definition::Alias const& alias) -> std::string
{
    return display_string(arena, alias);
}

auto kieli::ast::display(Arena const& arena, definition::Concept const& concept_) -> std::string
{
    return display_string(arena, concept_);
}

auto kieli::ast::display(Arena const& arena, definition::Submodule const& submodule) -> std::string
{
    return display_string(arena, submodule);
}

auto kieli::ast::display(Arena const& arena, Definition const& definition) -> std::string
{
    return display_string(arena, definition);
}

auto kieli::ast::display(Arena const& arena, Expression const& expression) -> std::string
{
    return display_string(arena, expression);
}

auto kieli::ast::display(Arena const& arena, Pattern const& pattern) -> std::string
{
    return display_string(arena, pattern);
}

auto kieli::ast::display(Arena const& arena, Type const& type) -> std::string
{
    return display_string(arena, type);
}
