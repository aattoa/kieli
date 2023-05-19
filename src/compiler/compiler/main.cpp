#include <libutl/common/utilities.hpp>
#include <libutl/common/flatmap.hpp>
#include <libutl/common/timer.hpp>
#include <libutl/readline/readline.hpp>
#include <libutl/color/color.hpp>
#include <libutl/cli/cli.hpp>

#include <liblex/lex.hpp>
#include <libparse/parse.hpp>
#include <libparse/parser_internals.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libreify/reify.hpp>
#include <liblower/lower.hpp>

#include <compiler/compiler.hpp>
#include <compiler/project.hpp>


namespace {

    [[nodiscard]]
    auto error_stream() -> std::ostream& {
        return std::cerr << utl::Color::red << "Error: " << utl::Color::white;
    }


    template <void(*f)(kieli::Lex_result&&)>
    auto generic_repl() {
        for (;;) {
            std::string string = utl::readline(">>> ");

            if (string.empty())
                continue;
            else if (string == "q")
                break;

            utl::add_to_readline_history(string);

            try {
                compiler::Compilation_info repl_info = std::make_shared<compiler::Shared_compilation_info>(compiler::Shared_compilation_info {
                    .source_arena = utl::Wrapper_arena<utl::Source>::with_page_size(1)
                });
                utl::wrapper auto const repl_source = repl_info.get()->source_arena.wrap("[repl]", std::move(string));
                f(kieli::lex({ .compilation_info = std::move(repl_info), .source = repl_source }));
            }
            catch (utl::diagnostics::Error const& error) {
                std::cerr << error.what() << "\n\n";
            }
            catch (std::exception const& exception) {
                error_stream() << exception.what() << "\n\n";
            }
        }
    }

    constexpr auto lexer_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        fmt::print("Tokens: {}\n", lex_result.tokens);
    }>;

    constexpr auto expression_parser_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        libparse::Parse_context context { std::move(lex_result), ast::Node_arena::with_default_page_size() };

        if (auto result = parse_expression(context)) {
            fmt::println("Result: {}", result);
            if (!context.pointer->source_view.string.empty())
                fmt::println("Remaining input: '{}'", context.pointer->source_view.string.data());
        }
        else {
            fmt::println("No parse");
        }
    }>;

    constexpr auto program_parser_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        auto parse_result = parse(std::move(lex_result));
        fmt::println("{}", parse_result.module);
    }>;

    constexpr auto desugaring_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        auto desugar_result = desugar(parse(std::move(lex_result)));
        fmt::print("{}\n\n", utl::formatting::delimited_range(desugar_result.module.definitions, "\n\n"));
    }>;

    constexpr auto resolution_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        auto resolve_result = resolve(desugar(parse(std::move(lex_result))));
        fmt::println("{}", utl::formatting::delimited_range(resolve_result.functions, "\n\n"));
    }>;

    constexpr auto reification_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        (void)reify(resolve(desugar(parse(std::move(lex_result)))));
    }>;

    constexpr auto lowering_repl = generic_repl<[](kieli::Lex_result&& lex_result) {
        auto lowering_result = lower(reify(resolve(desugar(kieli::parse(std::move(lex_result))))));
        for (lir::Function const& function : lowering_result.functions)
            fmt::println("{}: {}", function.symbol, function.body);
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
        fmt::println("kieli version 0, compiled on " __DATE__ ", " __TIME__ ".");
    }

    if (std::string_view const* const name = options["new"]) {
        project::initialize(*name);
    }

    if (std::string_view const* const phase = options["debug"]) {
        using namespace compiler;

        auto source_directory_path = std::filesystem::current_path().parent_path() / "sample-project" / "src";

        auto const do_resolve = [&] {
            compiler::Compilation_info repl_info = compiler::mock_compilation_info();
            utl::wrapper auto const repl_source = repl_info.get()->source_arena.wrap(utl::Source::read(source_directory_path / "main.kieli"));
            return kieli::resolve(kieli::desugar(kieli::parse(kieli::lex({ .compilation_info = std::move(repl_info), .source = repl_source }))));
        };

        if (*phase == "low")
            (void)lower(reify(do_resolve()));
        else if (*phase == "rei")
            (void)reify(do_resolve());
        else if (*phase == "res")
            fmt::println("{}", utl::formatting::delimited_range(do_resolve().functions, "\n\n"));
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
