#include "utl/utilities.hpp"
#include "utl/readline.hpp"
#include "utl/color.hpp"
#include "utl/timer.hpp"

#include "language/configuration.hpp"
#include "project/project.hpp"
#include "cli/cli.hpp"

#include "phase/lex/lex.hpp"
#include "phase/parse/parse.hpp"
#include "phase/parse/parser_internals.hpp"
#include "phase/desugar/desugar.hpp"
#include "phase/resolve/resolve.hpp"
#include "phase/reify/reify.hpp"
#include "phase/lower/lower.hpp"
#include "phase/codegen/codegen.hpp"

#include "representation/lir/lir_formatting.hpp"

#include "vm/virtual_machine.hpp"
#include "vm/vm_formatting.hpp"

#include "compiler/compiler.hpp"


namespace {

    [[nodiscard]]
    auto error_stream() -> std::ostream& {
        return std::cerr << utl::Color::red << "Error: " << utl::Color::white;
    }


    auto print_mir_module(mir::Module const& module) -> void {
        for (utl::wrapper auto const function : module.functions) {
            fmt::print("{}\n\n", utl::get<mir::Function>(function->value));
        }
        for (utl::wrapper auto const function_template : module.function_templates) {
            auto& f = utl::get<mir::Function_template>(function_template->value);
            fmt::print("function template {}", f.definition.name);
            for (utl::wrapper auto const instantiation : f.instantiations) {
                fmt::print("\ninstantiation {}", utl::get<mir::Function>(instantiation->value));
            }
            fmt::print("\n\n");
        }
    }


    template <void(*f)(compiler::Lex_result&&)>
    auto generic_repl() {
        for (;;) {
            std::string string = utl::readline(">>> ");

            if (string.empty())
                continue;
            else if (string == "q")
                break;

            try {
                compiler::Compilation_info repl_info = compiler::mock_compilation_info();
                utl::wrapper auto const repl_source = repl_info.get()->source_arena.wrap("[repl]", std::move(string));
                f(compiler::lex({ .compilation_info = std::move(repl_info), .source = repl_source }));
            }
            catch (utl::diagnostics::Error const& error) {
                std::cerr << error.what() << "\n\n";
            }
            catch (std::exception const& exception) {
                error_stream() << exception.what() << "\n\n";
            }
        }
    }

    constexpr auto lexer_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        fmt::print("Tokens: {}\n", lex_result.tokens);
    }>;

    constexpr auto expression_parser_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        Parse_context context { std::move(lex_result), ast::Node_arena {} };

        if (auto result = parse_expression(context)) {
            fmt::println("Result: {}", result);
            if (!context.pointer->source_view.string.empty())
                fmt::println("Remaining input: '{}'", context.pointer->source_view.string.data());
        }
        else {
            fmt::println("No parse");
        }
    }>;

    constexpr auto program_parser_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        auto parse_result = compiler::parse(std::move(lex_result));
        fmt::println("{}", parse_result.module);
    }>;

    constexpr auto desugaring_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        auto desugar_result = compiler::desugar(compiler::parse(std::move(lex_result)));
        fmt::print("{}\n\n", utl::formatting::delimited_range(desugar_result.module.definitions, "\n\n"));
    }>;

    constexpr auto resolution_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        auto resolve_result = compiler::resolve(compiler::desugar(compiler::parse(std::move(lex_result))));
        print_mir_module(resolve_result.module);
    }>;

    constexpr auto reification_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        (void)compiler::reify(compiler::resolve(compiler::desugar(compiler::parse(std::move(lex_result)))));
    }>;

    constexpr auto lowering_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        using namespace compiler;
        auto lowering_result = lower(reify(resolve(desugar(parse(std::move(lex_result))))));
        for (lir::Function const& function : lowering_result.functions)
            fmt::println("{}: {}", function.symbol, function.body);
    }>;

    constexpr auto codegen_repl = generic_repl<[](compiler::Lex_result&& lex_result) {
        using namespace compiler;
        (void)codegen(lower(reify(resolve(desugar(parse(std::move(lex_result)))))));
    }>;

}


// Tokens -> AST -> HIR -> MIR -> CIR -> LIR -> Bytecode


auto main(int argc, char const** argv) -> int try {
    cli::Options_description description;
    description.add_options()
        ({ "help"   , 'h' },                               "Show this text"            )
        ({ "version", 'v' },                               "Show kieli version"        )
        ("new"             , cli::string("project name"),  "Create a new kieli project")
        ("repl"            , cli::string("repl to run"),   "Run the given repl"        )
        ("machine"                                                                     )
        ("debug"           , cli::string("phase to debug")                             )
        ("nocolor"         ,                               "Disable colored output"    )
        ("time"            ,                               "Print the execution time"  );

    cli::Options options = utl::expect(cli::parse_command_line(argc, argv, description));

    utl::Logging_timer const execution_timer {
        [&options](auto const elapsed) {
            if (options["time"])
                fmt::println("Total execution time: {}", elapsed);
        }
    };

    if (options["nocolor"]) {
        utl::disable_color_formatting();
    }

    if (options["help"]) {
        fmt::print("Valid options:\n\n{}", description);
        return EXIT_SUCCESS;
    }

    if (options["version"]) {
        fmt::print("kieli version {}, compiled on " __DATE__ ", " __TIME__ ".\n", language::version);
    }

    if (std::string_view const* const name = options["new"]) {
        project::initialize(*name);
    }

    if (options["machine"]) {
        vm::Virtual_machine machine { .stack = utl::Bytestack { 32 } };

        [[maybe_unused]]
        auto const string = machine.program.constants.add_to_string_pool("Hello, world!\n");

        utl::Logging_timer timer;

        using enum vm::Opcode;
        machine.program.bytecode.write(
            const_8, 0_iz,
            iinc_top,
            dup_8,

            dup_8,
            dup_8,
            imul,
            iprint,

            local_jump_ineq_i, vm::Local_offset_type(-13 - 4), 10_iz,
            halt);

        return machine.run();
    }


    if (std::string_view const* const phase = options["debug"]) {
        using namespace compiler;

        auto source_directory_path = std::filesystem::current_path().parent_path() / "sample-project" / "src";

        auto const do_resolve = [&] {
            compiler::Compilation_info repl_info = compiler::mock_compilation_info();
            utl::wrapper auto const repl_source = repl_info.get()->source_arena.wrap(utl::Source::read(source_directory_path / "types.kieli"));
            return resolve(desugar(parse(lex({ .compilation_info = std::move(repl_info), .source = repl_source }))));
        };

        if (*phase == "low")
            (void)lower(reify(do_resolve()));
        else if (*phase == "rei")
            (void)reify(do_resolve());
        else if (*phase == "res")
            print_mir_module(do_resolve().module);
        else if (*phase == "comp")
            (void)compiler::compile({ .source_directory_path = std::move(source_directory_path), .main_file_name = "main.kieli" });
        else
            throw utl::exception("The phase must be one of low|rei|res|comp, not '{}'", *phase);

        fmt::println("Finished debugging phase {}", *phase);
    }

    if (std::string_view const* const name = options["repl"]) {
        utl::Flatmap<std::string_view, void(*)()> const repls { {
            { "lex" , lexer_repl             },
            { "expr", expression_parser_repl },
            { "prog", program_parser_repl    },
            { "des" , desugaring_repl        },
            { "res" , resolution_repl        },
            { "rei" , reification_repl       },
            { "low" , lowering_repl          },
            { "gen" , codegen_repl           },
        } };
        if (auto const* const repl = repls.find(*name))
            (*repl)();
        else
            throw utl::exception("The repl must be one of lex|expr|prog|des|res|rei|low|gen, not '{}'", *name);
    }

    return EXIT_SUCCESS;
}

catch (cli::Unrecognized_option const& exception) {
    std::cerr << exception.what() << "; use --help to see a list of valid options\n";
    return EXIT_FAILURE;
}
catch (utl::diagnostics::Error const& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
}
catch (std::exception const& exception) {
    error_stream() << exception.what() << '\n';
    return EXIT_FAILURE;
}
catch (...) {
    error_stream() << "Caught unrecognized exception\n";
    throw;
}
