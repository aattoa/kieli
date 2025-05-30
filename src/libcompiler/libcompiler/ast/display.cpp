#include <libutl/utilities.hpp>
#include <libcompiler/ast/display.hpp>

using namespace ki;
using namespace ki::ast;

namespace {
    struct Display_state {
        std::string             output;
        std::string             indent;
        bool                    unicode {};
        Arena const&            arena;
        utl::String_pool const& pool;
    };

    enum struct Last : std::uint8_t { No, Yes };

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
        if (last == Last::Yes) {
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
            write_node(state, Last::Yes, [&] { do_display(state, node); });
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
                write_node(state, i == vector.size() - 1 ? Last::Yes : Last::No, [&] {
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

    void do_display(Display_state& state, db::Name const& name)
    {
        write_line(state, "{:?}", state.pool.get(name.id));
    }

    void do_display(Display_state& state, Mutability const& mutability)
    {
        auto const visitor = utl::Overload {
            [&](db::Mutability concrete) { //
                write_line(state, "concrete {}", db::mutability_string(concrete));
            },
            [&](Parameterized_mutability const& parameterized) {
                write_line(state, "parameterized {}", state.pool.get(parameterized.name.id));
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
                state, Last::No, "template arguments", segment.template_arguments.value());
        }
        display_node(state, Last::Yes, "name", segment.name);
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
        display_node(state, Last::No, "root", path.root);
        display_vector_node(state, Last::Yes, "segments", path.segments);
    }

    void do_display(Display_state& state, Template_parameter const& parameter)
    {
        auto display_default = [&](auto const& argument) {
            if (argument.has_value()) {
                write_node(state, Last::No, [&] {
                    write_line(state, "default argument");
                    write_node(state, Last::Yes, [&] {
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
                display_node(state, Last::No, "name", parameter.name);
                display_vector_node(state, Last::Yes, "concepts", parameter.concepts);
            },
            [&](Template_value_parameter const& parameter) {
                write_line(state, "value parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::No, "name", parameter.name);
                display_node(state, Last::Yes, "type", parameter.type);
            },
            [&](Template_mutability_parameter const& parameter) {
                write_line(state, "mutability parameter");
                display_default(parameter.default_argument);
                display_node(state, Last::Yes, "name", parameter.name);
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

    void do_display(Display_state& state, Field const& field)
    {
        write_line(state, "field");
        display_node(state, Last::No, "name", field.name);
        display_node(state, Last::Yes, "type", field.type);
    }

    void do_display(Display_state& state, Field_init const& field)
    {
        write_line(state, "struct field initializer");
        display_node(state, Last::No, "name", field.name);
        display_node(state, Last::Yes, "expression", field.expression);
    }

    void do_display(Display_state& state, patt::Field const& field)
    {
        write_line(state, "field");
        if (field.pattern.has_value()) {
            display_node(state, Last::No, "pattern", field.pattern.value());
        }
        display_node(state, Last::Yes, "name", field.name);
    }

    void do_display(Display_state& state, Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](Struct_constructor const& constructor) {
                write_line(state, "struct constructor");
                display_vector_node(state, Last::Yes, "fields", constructor.fields);
            },
            [&](Tuple_constructor const& constructor) {
                write_line(state, "tuple constructor");
                display_vector_node(state, Last::Yes, "types", constructor.types);
            },
            [&](Unit_constructor const&) { write_line(state, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    void do_display(Display_state& state, patt::Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](patt::Struct_constructor const& constructor) {
                write_line(state, "struct constructor");
                display_vector_node(state, Last::Yes, "fields", constructor.fields);
            },
            [&](patt::Tuple_constructor const& constructor) {
                write_line(state, "tuple constructor");
                display_node(state, Last::Yes, "pattern", constructor.pattern);
            },
            [&](patt::Unit_constructor const&) { write_line(state, "unit constructor"); },
        };
        std::visit(visitor, body);
    }

    void do_display(Display_state& state, Constructor const& constructor)
    {
        write_line(state, "constructor");
        display_node(state, Last::No, "name", constructor.name);
        display_node(state, Last::Yes, "body", constructor.body);
    }

    void do_display(Display_state& state, Function_parameter const& parameter)
    {
        write_line(state, "function parameter");
        display_node(state, Last::No, "type", parameter.type);
        if (parameter.default_argument.has_value()) {
            display_node(state, Last::No, "default argument", parameter.default_argument.value());
        }
        display_node(state, Last::Yes, "pattern", parameter.pattern);
    }

    void do_display(Display_state& state, Function_signature const& signature)
    {
        write_line(state, "function signature");
        display_node(state, Last::No, "name", signature.name);
        display_template_parameters_node(state, Last::No, signature.template_parameters);
        display_node(state, Last::No, "return type", signature.return_type);
        display_vector_node(state, Last::Yes, "function parameters", signature.function_parameters);
    }

    void do_display(Display_state& state, Type_signature const& signature)
    {
        write_line(state, "type signature");
        display_node(state, Last::No, "name", signature.name);
        display_vector_node(state, Last::Yes, "concepts", signature.concepts);
    }

    void do_display(Display_state& state, Match_arm const& arm)
    {
        write_line(state, "arm");
        display_node(state, Last::No, "pattern", arm.pattern);
        display_node(state, Last::Yes, "handler", arm.expression);
    }

    void do_display(Display_state& state, Function const& function)
    {
        write_line(state, "function");
        display_node(state, Last::No, "signature", function.signature);
        display_node(state, Last::Yes, "body", function.body);
    }

    void do_display(Display_state& state, Struct const& structure)
    {
        write_line(state, "structure");
        display_template_parameters_node(state, Last::No, structure.template_parameters);
        display_node(state, Last::Yes, "constructor", structure.constructor);
    }

    void do_display(Display_state& state, Enum const& enumeration)
    {
        write_line(state, "enumeration");
        display_node(state, Last::No, "name", enumeration.name);
        display_template_parameters_node(state, Last::No, enumeration.template_parameters);
        display_vector_node(state, Last::Yes, "constructors", enumeration.constructors);
    }

    void do_display(Display_state& state, Alias const& alias)
    {
        write_line(state, "type alias");
        display_node(state, Last::No, "name", alias.name);
        display_template_parameters_node(state, Last::No, alias.template_parameters);
        display_node(state, Last::Yes, "aliased type", alias.type);
    }

    void do_display(Display_state& state, Concept const& concept_)
    {
        write_line(state, "concept");
        display_node(state, Last::No, "name", concept_.name);
        display_template_parameters_node(state, Last::No, concept_.template_parameters);
        display_vector_node(state, Last::No, "functions", concept_.function_signatures);
        display_vector_node(state, Last::Yes, "types", concept_.type_signatures);
    }

    void do_display(Display_state& state, Impl const& implementation)
    {
        write_line(state, "implementation");
        display_template_parameters_node(state, Last::No, implementation.template_parameters);
        display_node(state, Last::No, "type", implementation.type);
        display_vector_node(state, Last::Yes, "definitions", implementation.definitions);
    }

    void do_display(Display_state& state, Submodule const& submodule)
    {
        write_line(state, "submodule");
        display_node(state, Last::No, "name", submodule.name);
        display_template_parameters_node(state, Last::No, submodule.template_parameters);
        display_vector_node(state, Last::Yes, "definitions", submodule.definitions);
    }

    void display_literal(Display_state& state, db::Integer const& integer)
    {
        write_line(state, "integer literal {}", integer.value);
    }

    void display_literal(Display_state& state, db::Floating const& floating)
    {
        write_line(state, "floating point literal {}", floating.value);
    }

    void display_literal(Display_state& state, db::Boolean const& boolean)
    {
        write_line(state, "boolean literal {}", boolean.value);
    }

    void display_literal(Display_state& state, db::String const& string)
    {
        write_line(state, "string literal {:?}", state.pool.get(string.id));
    }

    struct Expression_display_visitor {
        Display_state& state;

        void operator()(
            utl::one_of<db::Integer, db::Floating, db::Boolean, db::String> auto const& literal)
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

        void operator()(expr::Array const& array)
        {
            write_line(state, "array literal");
            display_vector_node(state, Last::Yes, "elements", array.elements);
        }

        void operator()(expr::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::Yes, "fields", tuple.fields);
        }

        void operator()(expr::Loop const& loop)
        {
            write_line(state, "loop");
            display_node(state, Last::No, "body", loop.body);
            display_node(state, Last::Yes, "source", loop.source);
        }

        void operator()(expr::Break const& break_)
        {
            write_line(state, "break");
            display_node(state, Last::Yes, "result", break_.result);
        }

        void operator()(expr::Continue const&)
        {
            write_line(state, "continue");
        }

        void operator()(expr::Block const& block)
        {
            write_line(state, "block");
            display_vector_node(state, Last::No, "side effects", block.effects);
            display_node(state, Last::Yes, "result", block.result);
        }

        void operator()(expr::Function_call const& call)
        {
            write_line(state, "function call");
            display_node(state, Last::No, "invocable", call.invocable);
            display_vector_node(state, Last::Yes, "arguments", call.arguments);
        }

        void operator()(expr::Struct_init const& initializer)
        {
            write_line(state, "struct initializer");
            display_node(state, Last::No, "constructor path", initializer.path);
            display_vector_node(state, Last::Yes, "field initializers", initializer.fields);
        }

        void operator()(expr::Infix_call const& application)
        {
            write_line(state, "infix call");
            display_node(state, Last::No, "left operand", application.left);
            display_node(state, Last::No, "right operand", application.right);
            display_node(state, Last::Yes, "operator", application.op);
        }

        void operator()(expr::Struct_field const& field)
        {
            write_line(state, "struct index");
            display_node(state, Last::No, "base expression", field.base);
            display_node(state, Last::Yes, "field name", field.name);
        }

        void operator()(expr::Tuple_field const& field)
        {
            write_line(state, "tuple index");
            display_node(state, Last::No, "base expression", field.base);
            write_node(state, Last::Yes, [&] { write_line(state, "field index {}", field.index); });
        }

        void operator()(expr::Array_index const& index)
        {
            write_line(state, "array index");
            display_node(state, Last::No, "base expression", index.base);
            display_node(state, Last::Yes, "index expression", index.index);
        }

        void operator()(expr::Method_call const& call)
        {
            write_line(state, "method call");
            display_node(state, Last::No, "method name", call.name);
            display_node(state, Last::No, "base expression", call.expression);
            if (call.template_arguments.has_value()) {
                display_vector_node(
                    state, Last::No, "template arguments", call.template_arguments.value());
            }
            display_vector_node(state, Last::Yes, "method arguments", call.function_arguments);
        }

        void operator()(expr::Conditional const& conditional)
        {
            write_line(state, "conditional");
            display_node(state, Last::No, "condition", conditional.condition);
            display_node(state, Last::No, "true branch", conditional.true_branch);
            display_node(state, Last::No, "false branch", conditional.false_branch);
            display_node(state, Last::Yes, "source", conditional.source);
        }

        void operator()(expr::Match const& match)
        {
            write_line(state, "match");
            display_node(state, Last::No, "scrutinee", match.scrutinee);
            display_vector_node(state, Last::Yes, "arms", match.arms);
        }

        void operator()(expr::Ascription const& ascription)
        {
            write_line(state, "type ascription");
            display_node(state, Last::No, "expression", ascription.expression);
            display_node(state, Last::Yes, "ascribed type", ascription.type);
        }

        void operator()(expr::Let const& let)
        {
            write_line(state, "let binding");
            if (let.type.has_value()) {
                display_node(state, Last::No, "type", let.type.value());
            }
            display_node(state, Last::No, "pattern", let.pattern);
            display_node(state, Last::Yes, "initializer", let.initializer);
        }

        void operator()(expr::Type_alias const& alias)
        {
            write_line(state, "local type alias");
            display_node(state, Last::No, "name", alias.name);
            display_node(state, Last::Yes, "aliased type", alias.type);
        }

        void operator()(expr::Return const& ret)
        {
            write_line(state, "ret");
            display_node(state, Last::Yes, "returned expression", ret.expression);
        }

        void operator()(expr::Sizeof const& sizeof_)
        {
            write_line(state, "sizeof");
            display_node(state, Last::Yes, "inspected type", sizeof_.type);
        }

        void operator()(expr::Addressof const& addressof)
        {
            write_line(state, "addressof");
            display_node(state, Last::No, "reference mutability", addressof.mutability);
            display_node(state, Last::Yes, "place expression", addressof.expression);
        }

        void operator()(expr::Deref const& dereference)
        {
            write_line(state, "dereference");
            display_node(state, Last::Yes, "reference expression", dereference.expression);
        }

        void operator()(expr::Defer const& defer)
        {
            write_line(state, "defer");
            display_node(state, Last::Yes, "effect", defer.expression);
        }

        void operator()(db::Error)
        {
            write_line(state, "error");
        }
    };

    struct Pattern_display_visitor {
        Display_state& state;

        void operator()(
            utl::one_of<db::Integer, db::Floating, db::Boolean, db::String> auto const& literal)
        {
            display_literal(state, literal);
        }

        void operator()(Wildcard const& wildcard)
        {
            do_display(state, wildcard);
        }

        void operator()(patt::Name const& name)
        {
            write_line(state, "name");
            display_node(state, Last::No, "name", name.name);
            display_node(state, Last::Yes, "mutability", name.mutability);
        }

        void operator()(patt::Constructor const& constructor)
        {
            write_line(state, "constructor");
            display_node(state, Last::No, "constructor path", constructor.path);
            display_node(state, Last::Yes, "body", constructor.body);
        }

        void operator()(patt::Abbreviated_constructor const& constructor)
        {
            write_line(state, "abbreviated constructor");
            display_node(state, Last::No, "name", constructor.name);
            display_node(state, Last::Yes, "body", constructor.body);
        }

        void operator()(patt::Tuple const& tuple)
        {
            write_line(state, "tuple");
            display_vector_node(state, Last::Yes, "field patterns", tuple.field_patterns);
        }

        void operator()(patt::Slice const& slice)
        {
            write_line(state, "slice");
            display_vector_node(state, Last::Yes, "element patterns", slice.element_patterns);
        }

        void operator()(patt::Guarded const& guarded)
        {
            write_line(state, "guarded");
            display_node(state, Last::No, "guarded pattern", guarded.pattern);
            display_node(state, Last::Yes, "guard expression", guarded.guard);
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
            display_vector_node(state, Last::Yes, "field types", tuple.field_types);
        }

        void operator()(type::Array const& array)
        {
            write_line(state, "tuple");
            display_node(state, Last::No, "length", array.length);
            display_node(state, Last::Yes, "element type", array.element_type);
        }

        void operator()(type::Slice const& slice)
        {
            write_line(state, "slice");
            display_node(state, Last::Yes, "element type", slice.element_type);
        }

        void operator()(type::Function const& function)
        {
            write_line(state, "function");
            display_vector_node(state, Last::No, "parameter types", function.parameter_types);
            display_node(state, Last::Yes, "return type", function.return_type);
        }

        void operator()(type::Typeof const& typeof_)
        {
            write_line(state, "typeof");
            display_node(state, Last::Yes, "inspected expression", typeof_.expression);
        }

        void operator()(type::Reference const& reference)
        {
            write_line(state, "reference");
            display_node(state, Last::No, "reference mutability", reference.mutability);
            display_node(state, Last::Yes, "referenced type", reference.referenced_type);
        }

        void operator()(type::Pointer const& pointer)
        {
            write_line(state, "pointer");
            display_node(state, Last::No, "pointer mutability", pointer.mutability);
            display_node(state, Last::Yes, "pointee type", pointer.pointee_type);
        }

        void operator()(type::Impl const& implementation)
        {
            write_line(state, "implementation");
            display_vector_node(state, Last::Yes, "concepts", implementation.concepts);
        }

        void operator()(db::Error const&)
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

    auto display_string(Arena const& arena, utl::String_pool const& pool, auto const& object)
        -> std::string
    {
        Display_state state {
            .output  = {},
            .indent  = {},
            .unicode = true,
            .arena   = arena,
            .pool    = pool,
        };
        do_display(state, object);
        return std::move(state.output);
    }
} // namespace

auto ki::ast::display(Arena const& arena, utl::String_pool const& pool, Function const& function)
    -> std::string
{
    return display_string(arena, pool, function);
}

auto ki::ast::display(Arena const& arena, utl::String_pool const& pool, Enum const& enumeration)
    -> std::string
{
    return display_string(arena, pool, enumeration);
}

auto ki::ast::display(Arena const& arena, utl::String_pool const& pool, Alias const& alias)
    -> std::string
{
    return display_string(arena, pool, alias);
}

auto ki::ast::display(Arena const& arena, utl::String_pool const& pool, Concept const& concept_)
    -> std::string
{
    return display_string(arena, pool, concept_);
}

auto ki::ast::display(Arena const& arena, utl::String_pool const& pool, Submodule const& submodule)
    -> std::string
{
    return display_string(arena, pool, submodule);
}

auto ki::ast::display(
    Arena const& arena, utl::String_pool const& pool, Definition const& definition) -> std::string
{
    return display_string(arena, pool, definition);
}

auto ki::ast::display(
    Arena const& arena, utl::String_pool const& pool, Expression const& expression) -> std::string
{
    return display_string(arena, pool, expression);
}

auto ki::ast::display(Arena const& arena, utl::String_pool const& pool, Pattern const& pattern)
    -> std::string
{
    return display_string(arena, pool, pattern);
}

auto ki::ast::display(Arena const& arena, utl::String_pool const& pool, Type const& type)
    -> std::string
{
    return display_string(arena, pool, type);
}
