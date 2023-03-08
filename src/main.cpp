#include "utl/utilities.hpp"
#include "utl/color.hpp"
#include "utl/timer.hpp"

#include "lexer/lexer.hpp"

#include "parser/parser.hpp"
#include "parser/parser_internals.hpp"

#include "ast/ast.hpp"
#include "ast/lower/lower.hpp"

#include "resolution/resolution.hpp"
#include "reification/reification.hpp"

#include "vm/bytecode.hpp"
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
    consteval auto generic_repl(void (*f)(utl::Source)) {
        return [=] {
            for (;;) {
                std::string string;

                utl::print(" >>> ");
                std::getline(std::cin, string);

                if (string.empty()) {
                    continue;
                }
                else if (string == "q") {
                    break;
                }

                try {
                    utl::Source source { utl::Source::Mock_tag { "repl" }, std::move(string) };
                    f(std::move(source));
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
    constexpr auto lexer_repl = generic_repl([](utl::Source source) {
        compiler::Program_string_pool string_pool;
        utl::print("Tokens: {}\n", compiler::lex(std::move(source), string_pool).tokens);
    });

    [[maybe_unused]]
    constexpr auto expression_parser_repl = generic_repl([](utl::Source source) {
        compiler::Program_string_pool string_pool;
        Parse_context context { compiler::lex(std::move(source), string_pool) };

        if (auto result = parse_expression(context)) {
            utl::print("Result: {}\n", result);

            if (!context.pointer->source_view.string.empty()) {
                utl::print("Remaining input: '{}'\n", context.pointer->source_view.string.data());
            }
        }
        else {
            utl::print("No parse\n");
        }
    });

    [[maybe_unused]]
    constexpr auto program_parser_repl = generic_repl([](utl::Source source) {
        compiler::Program_string_pool string_pool;
        auto parse_result = compiler::parse(compiler::lex(std::move(source), string_pool));
        utl::print("{}\n", parse_result.module);
    });

    [[maybe_unused]]
    constexpr auto lowering_repl = generic_repl([](utl::Source source) {
        compiler::Program_string_pool string_pool;
        auto lower_result = compiler::lower(compiler::parse(compiler::lex(std::move(source), string_pool)));
        utl::print("{}\n\n", utl::fmt::delimited_range(lower_result.module.definitions, "\n\n"));
    });


    [[maybe_unused]]
    auto initialize_project(std::string_view const project_name) -> void {
        auto parent_path  = std::filesystem::current_path();
        auto project_path = parent_path / project_name;
        auto source_dir   = project_path / "src";

        if (project_path.has_extension()) {
            throw utl::exception("A directory name can not have a file extension");
        }

        if (is_directory(project_path)) {
            throw utl::exception(
                "A directory with the path '{}' already exists. Please use a new name",
                project_path.string()
            );
        }

        if (!create_directory(project_path)) {
            throw utl::exception(
                "Could not create a directory with the path '{}'",
                project_path.string()
            );
        }

        {
            std::ofstream configuration_file { project_path / "kieli_config" };
            if (!configuration_file) {
                throw utl::exception("Could not create the configuration file");
            }
            configuration_file << project::default_configuration().string();
        }

        if (!create_directory(source_dir)) {
            throw utl::exception("Could not create the source directory");
        }

        {
            std::ofstream main_file { source_dir / "main.kieli" };
            if (!main_file) {
                throw utl::exception("Could not create the main file");
            }
            main_file << "import std\n\nfn main() {\n    print(\"Hello, world!\\n\")\n}";
        }

        utl::print("Successfully created a new project at '{}'\n", project_path.string());
    }

}


// Tokens -> AST -> HIR -> MIR -> CIR -> LIR -> Bytecode


auto main(int argc, char const** argv) -> int try {
    cli::Options_description description;
    description.add_options()
        ({ "help"   , 'h' },                                   "Show this text"          )
        ({ "version", 'v' },                                   "Show kieli version"        )
        ("new"             , cli::string("project name"),      "Create a new kieli project")
        ("repl"            , cli::string("lex|expr|prog|low"), "Run the given repl"      )
        ("machine"                                                                       )
        ("debug"                                                                         )
        ("nocolor"         ,                                   "Disable colored output"  )
        ("time"            ,                                   "Print the execution time")
        ("test"            ,                                   "Run all tests"           );

    cli::Options options = utl::expect(cli::parse_command_line(argc, argv, description));


    auto configuration = project::read_configuration();
    (void)configuration;

    utl::Logging_timer execution_timer {
        [&options](utl::Logging_timer::Duration const elapsed) {
            if (options["time"]) {
                utl::print("Total execution time: {}\n", elapsed);
            }
        }
    };

    if (options["nocolor"]) {
        utl::disable_color_formatting();
    }

    if (options["help"]) {
        utl::print("Valid options:\n\n{}", description);
        return 0;
    }

    if (options["version"]) {
        utl::print("kieli version {}, compiled on " __DATE__ ", " __TIME__ ".\n", language::version);
    }

    if (std::string_view const* const name = options["new"]) {
        initialize_project(*name);
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

        utl::Source debug_source { (std::filesystem::current_path() / "sample-project" / "src" / "main.kieli").string() };
        Program_string_pool debug_string_pool;

        constexpr auto pipeline = utl::compose(&reify, &resolve, &lower, &parse, &lex);
        (void)pipeline(std::move(debug_source), debug_string_pool, utl::diagnostics::Builder::Configuration {});
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