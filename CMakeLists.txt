cmake_minimum_required(VERSION 3.24)
project(kieli)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options("/W4")
else ()
    # Turn up warning levels
    add_compile_options("-Wall" "-Wextra" "-Wpedantic")
    # Turn off unnecessary warnings
    add_compile_options("-Wno-missing-field-initializers"
                        "-Wno-switch")
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Clang doesn't provide sized deallocation functions by default
  add_compile_options(-fsized-deallocation)
endif ()

set(SOURCE_FILES
	src/main.cpp

	src/representation/ast/ast.hpp
	src/representation/ast/ast_formatting.cpp
	src/representation/ast/nodes/definition.hpp
	src/representation/ast/nodes/expression.hpp
	src/representation/ast/nodes/pattern.hpp
	src/representation/ast/nodes/type.hpp

	src/representation/cir/cir.hpp

	src/representation/hir/hir.hpp
	src/representation/hir/hir_formatting.cpp
	src/representation/hir/shared_formatting.cpp
	src/representation/hir/nodes/definition.hpp
	src/representation/hir/nodes/expression.hpp
	src/representation/hir/nodes/pattern.hpp
	src/representation/hir/nodes/type.hpp

	src/representation/lir/lir.hpp

	src/representation/token/token.hpp
	src/representation/token/token_formatting.cpp

	src/representation/mir/mir.hpp
	src/representation/mir/mir_formatting.cpp
	src/representation/mir/nodes/definition.hpp
	src/representation/mir/nodes/expression.hpp
	src/representation/mir/nodes/pattern.hpp
	src/representation/mir/nodes/type.hpp

	src/phase/desugar/desugar.hpp
	src/phase/desugar/desugar.cpp
	src/phase/desugar/desugaring_internals.hpp
	src/phase/desugar/desugar_definition.cpp
	src/phase/desugar/desugar_expression.cpp
	src/phase/desugar/desugar_pattern.cpp
	src/phase/desugar/desugar_type.cpp

	src/phase/lex/lex.cpp
	src/phase/lex/lex.hpp
	src/phase/lex/lexer_test.cpp

	src/phase/parse/parse.cpp
	src/phase/parse/parse.hpp
	src/phase/parse/parser_internals.cpp
	src/phase/parse/parser_internals.hpp
	src/phase/parse/parser_test.cpp
	src/phase/parse/parse_expression.cpp
	src/phase/parse/parse_pattern.cpp
	src/phase/parse/parse_type.cpp

	src/phase/reify/expression_reification.cpp
	src/phase/reify/pattern_reification.cpp
	src/phase/reify/reify.cpp
	src/phase/reify/reify.hpp
	src/phase/reify/reification_internals.cpp
	src/phase/reify/reification_internals.hpp
	src/phase/reify/type_reification.cpp

	src/phase/resolve/constraint.cpp
	src/phase/resolve/definition_resolution.cpp
	src/phase/resolve/expression_resolution.cpp
	src/phase/resolve/method.cpp
	src/phase/resolve/namespace_lookup.cpp
	src/phase/resolve/pattern_resolution.cpp
	src/phase/resolve/resolve.cpp
	src/phase/resolve/resolve.hpp
	src/phase/resolve/resolution_internals.cpp
	src/phase/resolve/resolution_internals.hpp
	src/phase/resolve/scope.cpp
	src/phase/resolve/template_instantiation.cpp
	src/phase/resolve/type_resolution.cpp
	src/phase/resolve/unification.cpp

	src/project/project.cpp
	src/project/project.hpp

	src/cli/cli.hpp
	src/cli/cli.cpp

	src/language/configuration.hpp

	src/tests/tests.cpp
	src/tests/tests.hpp

	src/utl/utl_test.cpp
	src/utl/bytestack.hpp
	src/utl/color.cpp
	src/utl/color.hpp
	src/utl/diagnostics.cpp
	src/utl/diagnostics.hpp
	src/utl/disable_unnecessary_warnings.hpp
	src/utl/flatmap.hpp
	src/utl/noisy.hpp
	src/utl/pooled_string.hpp
	src/utl/readline.hpp
	src/utl/readline.cpp
	src/utl/safe_integer.hpp
	src/utl/source.cpp
	src/utl/source.hpp
	src/utl/timer.hpp
	src/utl/utilities.hpp
	src/utl/wrapper.hpp

	src/vm/bytecode.hpp
	src/vm/opcode.hpp
	src/vm/serializing.cpp
	src/vm/virtual_machine.cpp
	src/vm/virtual_machine.hpp
	src/vm/vm_formatting.cpp
	src/vm/vm_formatting.hpp
	src/vm/vm_test.cpp)

add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
    ${PROJECT_SOURCE_DIR}/dependencies/src/fmt/format.cc)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE src/utl/utilities.hpp)

target_include_directories(${PROJECT_NAME}
    PRIVATE src
    PRIVATE dependencies/include)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(${PROJECT_NAME}
		PRIVATE readline)
endif ()