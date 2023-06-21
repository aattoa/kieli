#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>
#include <libformat/format.hpp>


namespace {
    struct Definition_format_visitor {
        libformat::State& state;
        auto operator()(cst::definition::Function const& function) const -> void {
            state.format("fn {}", function.signature.name);
            if (function.signature.template_parameters.has_value()) {
                state.format("[TODO]");
            }
            state.format("(TODO)");
            if (function.optional_equals_sign_token.has_value()) {
                if (!std::holds_alternative<cst::expression::Block>(function.body->value))
                    state.format(" = ");
            }
            libformat::format_expression(*function.body, state);
        }
        auto operator()(cst::definition::Struct const&) -> void {
            utl::todo();
        }
        auto operator()(cst::definition::Enum const&) -> void {
            utl::todo();
        }
        auto operator()(cst::definition::Typeclass const&) -> void {
            utl::todo();
        }
        auto operator()(cst::definition::Implementation const&) -> void {
            utl::todo();
        }
        auto operator()(cst::definition::Instantiation const&) -> void {
            utl::todo();
        }
        auto operator()(cst::definition::Alias const&) -> void {
            utl::todo();
        }
        auto operator()(cst::definition::Namespace const&) -> void {
            utl::todo();
        }
    };
}


auto kieli::format(
    cst::Module                 const& module,
    kieli::Format_configuration const& configuration) -> std::string
{
    std::string output_string;
    libformat::State state {
        .configuration       = configuration,
        .string              = output_string,
        .current_indentation = 0,
    };
    for (cst::Definition const& definition : module.definitions) {
        utl::match(definition.value, Definition_format_visitor { state });
    }
    return output_string;
}
