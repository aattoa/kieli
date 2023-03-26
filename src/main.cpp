#include "utl/utilities.hpp"
#include "utl/color.hpp"
#include "utl/timer.hpp"

#include "phase/lex/lex.hpp"

#include "phase/parse/parse.hpp"
#include "phase/parse/parser_internals.hpp"

#include "representation/ast/ast.hpp"
#include "phase/desugar/desugar.hpp"

#include "phase/resolve/resolve.hpp"
#include "phase/reify/reify.hpp"

#include "vm/virtual_machine.hpp"
#include "vm/vm_formatting.hpp"

#include "tests/tests.hpp"

#include "cli/cli.hpp"

#include "language/configuration.hpp"
#include "project/project.hpp"


namespace {

    [[nodiscard]]
    auto error_stream() -> std::ostream& {
        return std::cerr << utl::Color::red << "Error: " << utl::Color::white;
    }


    [[nodiscard]]
    consteval auto generic_repl(void(*f)(compiler::Lex_result&&)) {
        return [=] {
            for (;;) {
                std::string string;

                fmt::print(" >>> ");
                std::getline(std::cin, string);

                if (string.empty())
                    continue;
                else if (string == "q")
                    break;

                try {
                    utl::Source source { utl::Source::Mock_tag { "repl" }, std::move(string) };
                    compiler::Program_string_pool string_pool;
                    f(compiler::lex({ .source = std::move(source), .string_pool = string_pool }));
                }
                catch (utl::diagnostics::Error const& error) {
                    std::cerr << error.what() << "\n\n";
                }
                catch (std::exception const& exception) {
                    error_stream() << exception.what() << "\n\n";
                }
            }
        };
    }

    [[maybe_unused]]
    constexpr auto lexer_repl = generic_repl([](compiler::Lex_result&& lex_result) {
        fmt::print("Tokens: {}\n", lex_result.tokens);
    });

    [[maybe_unused]]
    constexpr auto expression_parser_repl = generic_repl([](compiler::Lex_result&& lex_result) {
        Parse_context context { std::move(lex_result) };

        if (auto result = parse_expression(context)) {
            fmt::println("Result: {}", result);
            if (!context.pointer->source_view.string.empty())
                fmt::println("Remaining input: '{}'", context.pointer->source_view.string.data());
        }
        else {
            fmt::println("No parse");
        }
    });

    [[maybe_unused]]
    constexpr auto program_parser_repl = generic_repl([](compiler::Lex_result&& lex_result) {
        auto parse_result = compiler::parse(std::move(lex_result));
        fmt::println("{}", parse_result.module);
    });

    [[maybe_unused]]
    constexpr auto lowering_repl = generic_repl([](compiler::Lex_result&& lex_result) {
        auto lower_result = compiler::desugar(compiler::parse(std::move(lex_result)));
        fmt::print("{}\n\n", utl::formatting::delimited_range(lower_result.module.definitions, "\n\n"));
    });

}


// Tokens -> AST -> HIR -> MIR -> CIR -> LIR -> Bytecode


auto main(int argc, char const** argv) -> int try {
    cli::Options_description description;
    description.add_options()
        ({ "help"   , 'h' },                                   "Show this text"            )
        ({ "version", 'v' },                                   "Show kieli version"        )
        ("new"             , cli::string("project name"),      "Create a new kieli project")
        ("repl"            , cli::string("lex|expr|prog|low"), "Run the given repl"        )
        ("machine"                                                                         )
        ("debug"                                                                           )
        ("nocolor"         ,                                   "Disable colored output"    )
        ("time"            ,                                   "Print the execution time"  )
        ("test"            ,                                   "Run all tests"             );

    cli::Options options = utl::expect(cli::parse_command_line(argc, argv, description));


    utl::Logging_timer execution_timer {
        [&options](utl::Logging_timer::Duration const elapsed) {
            if (options["time"]) {
                fmt::print("Total execution time: {}\n", elapsed);
            }
        }
    };

    if (options["nocolor"]) {
        utl::disable_color_formatting();
    }

    if (options["help"]) {
        fmt::print("Valid options:\n\n{}", description);
        return 0;
    }

    if (options["version"]) {
        fmt::print("kieli version {}, compiled on " __DATE__ ", " __TIME__ ".\n", language::version);
    }

    if (std::string_view const* const name = options["new"]) {
        project::initialize(*name);
    }

    if (options["test"]) {
        tests::run_all_tests();
    }

    if (options["machine"]) {
        vm::Virtual_machine machine { .stack = utl::Bytestack { 32 } };

        auto const string = machine.program.constants.add_to_string_pool("Hello, world!\n");

        using enum vm::Opcode;
        machine.program.bytecode.write(
            ipush, 0_iz,
            iinc_top,
            idup,
            spush, string,
            sprint,
            local_jump_ineq_i, vm::Local_offset_type(-23), 10_iz,
            halt
        );

        return machine.run();
    }

    if (options["debug"]) {
        using namespace compiler;

        utl::Source debug_source { (std::filesystem::current_path().parent_path() / "sample-project" / "src" / "main.kieli").string() };

        Program_string_pool debug_string_pool;
        compiler::Lex_arguments lex_arguments {
            .source      = std::move(debug_source),
            .string_pool = debug_string_pool
        };

        constexpr auto pipeline = utl::compose(&reify, &resolve, &desugar, &parse, &lex);
        (void)pipeline(std::move(lex_arguments));
    }

    if (cli::types::Str const* const name = options["repl"]) {
        if (*name == "lex") {
            lexer_repl();
        }
        else if (*name == "expr") {
            ast::Node_context repl_context;
            expression_parser_repl();
        }
        else if (*name == "prog") {
            program_parser_repl();
        }
        else if (*name == "low") {
            lowering_repl();
        }
        else {
            utl::abort("Unrecognized repl name");
        }
    }
}

catch (cli::Unrecognized_option const& exception) {
    std::cerr << exception.what() << "; use --help to see a list of valid options\n";
}
catch (utl::diagnostics::Error const& error) {
    std::cerr << error.what() << '\n';
}
catch (std::exception const& exception) {
    error_stream() << exception.what() << '\n';
}
catch (...) {
    error_stream() << "Caught unrecognized non-standard exception\n";
    throw;
}
