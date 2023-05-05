cmake_minimum_required(VERSION 3.24)
project(kieli)

# Set the global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Set global warning settings
if (MSVC)
    add_compile_options("/W4")
else ()
    add_compile_options("-Wall" "-Wextra" "-Wpedantic")
    add_compile_options("-Wno-missing-field-initializers"
                        "-Wno-switch")
endif ()


# Enable sized deallocation functions on Clang
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fsized-deallocation)
endif ()


set(KIELI_CLI_SOURCE_FILES
	src/cli/cli.hpp
	src/cli/cli.cpp)
set(KIELI_COMPILER_SOURCE_FILES
	src/compiler/compiler.cpp
	src/compiler/compiler.hpp
	src/compiler/predefinitions.cpp)
set(KIELI_PROJECT_SOURCE_FILES
	src/project/project.cpp
	src/project/project.hpp)
set(KIELI_LANGUAGE_SOURCE_FILES
	src/language/configuration.hpp)
set(KIELI_UTL_SOURCE_FILES
	src/utl/bytestack.hpp
	src/utl/color.cpp
	src/utl/color.hpp
	src/utl/diagnostics.cpp
	src/utl/diagnostics.hpp
	src/utl/disable_unnecessary_warnings.hpp
	src/utl/flatmap.hpp
	src/utl/noisy.hpp
	src/utl/pooled_string.hpp
	src/utl/safe_integer.hpp
	src/utl/source.cpp
	src/utl/source.hpp
	src/utl/timer.hpp
	src/utl/utilities.hpp
	src/utl/wrapper.hpp)
set(KIELI_UTL_READLINE_SOURCE_FILES
	src/utl/readline.cpp
	src/utl/readline.hpp)
set(KIELI_VM_SOURCE_FILES
	src/vm/bytecode.hpp
	src/vm/opcode.cpp
	src/vm/opcode.hpp
	src/vm/serializing.cpp
	src/vm/virtual_machine.cpp
	src/vm/virtual_machine.hpp
	src/vm/vm_formatting.cpp
	src/vm/vm_formatting.hpp)
set(KIELI_REPRESENTATION_SOURCE_FILES
	src/representation/ast/ast.hpp
	src/representation/ast/ast_formatting.cpp
	src/representation/ast/nodes/definition.hpp
	src/representation/ast/nodes/expression.hpp
	src/representation/ast/nodes/pattern.hpp
	src/representation/ast/nodes/type.hpp

	src/representation/cir/cir.hpp
	src/representation/cir/cir_formatting.hpp
	src/representation/cir/cir_formatting.cpp

	src/representation/hir/hir.hpp
	src/representation/hir/hir_formatting.cpp
	src/representation/hir/shared_formatting.cpp
	src/representation/hir/nodes/definition.hpp
	src/representation/hir/nodes/expression.hpp
	src/representation/hir/nodes/pattern.hpp
	src/representation/hir/nodes/type.hpp

	src/representation/lir/lir.hpp
	src/representation/lir/lir_formatting.cpp
	src/representation/lir/lir_formatting.hpp

	src/representation/mir/mir.cpp
	src/representation/mir/mir.hpp
	src/representation/mir/mir_formatting.cpp
	src/representation/mir/nodes/definition.hpp
	src/representation/mir/nodes/expression.hpp
	src/representation/mir/nodes/pattern.hpp
	src/representation/mir/nodes/type.hpp

	src/representation/token/token.hpp
	src/representation/token/token_formatting.cpp)
set(KIELI_PHASE_SOURCE_FILES
	src/phase/codegen/codegen.cpp
	src/phase/codegen/codegen.hpp

	src/phase/desugar/desugar.hpp
	src/phase/desugar/desugar.cpp
	src/phase/desugar/desugaring_internals.hpp
	src/phase/desugar/desugar_definition.cpp
	src/phase/desugar/desugar_expression.cpp
	src/phase/desugar/desugar_pattern.cpp
	src/phase/desugar/desugar_type.cpp

	src/phase/lex/lex.cpp
	src/phase/lex/lex.hpp

	src/phase/lower/lower.hpp
	src/phase/lower/lower.cpp

	src/phase/parse/parse.cpp
	src/phase/parse/parse.hpp
	src/phase/parse/parser_internals.cpp
	src/phase/parse/parser_internals.hpp
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
	src/phase/resolve/generalization.cpp
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
	src/phase/resolve/unification.cpp)


set(DEPENDENCY_INCLUDE_DIRECTORIES
	PRIVATE dependencies/range-v3/include
	PRIVATE dependencies/optional/include
	PRIVATE dependencies/expected/include)


# Libraries that require linking
add_subdirectory(dependencies/Catch2)
add_subdirectory(dependencies/fmt)


# Configure the main executable
add_executable(${PROJECT_NAME}
	src/main.cpp
	${KIELI_CLI_SOURCE_FILES}
	${KIELI_COMPILER_SOURCE_FILES}
	${KIELI_PROJECT_SOURCE_FILES}
	${KIELI_LANGUAGE_SOURCE_FILES}
	${KIELI_UTL_SOURCE_FILES}
	${KIELI_UTL_READLINE_SOURCE_FILES}
	${KIELI_VM_SOURCE_FILES}
	${KIELI_REPRESENTATION_SOURCE_FILES}
	${KIELI_PHASE_SOURCE_FILES})
target_precompile_headers(${PROJECT_NAME}
    PRIVATE src/utl/utilities.hpp)
target_include_directories(${PROJECT_NAME}
    PRIVATE src
	${DEPENDENCY_INCLUDE_DIRECTORIES})
target_link_libraries(${PROJECT_NAME}
	PRIVATE fmt::fmt)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(${PROJECT_NAME}
		PRIVATE readline)
endif ()


# Configure the testing executable
set(TEST_NAME tests)
add_executable(${TEST_NAME}
	tests/utl_test.cpp
	tests/lexer_test.cpp
	tests/parser_test.cpp
	tests/vm_test.cpp
	tests/resolution_test.cpp
	${KIELI_COMPILER_SOURCE_FILES}
	${KIELI_UTL_SOURCE_FILES}
	${KIELI_VM_SOURCE_FILES}
	${KIELI_REPRESENTATION_SOURCE_FILES}
	${KIELI_PHASE_SOURCE_FILES})
target_precompile_headers(${TEST_NAME}
    PRIVATE src/utl/utilities.hpp)
target_include_directories(${TEST_NAME}
	PRIVATE src
	${DEPENDENCY_INCLUDE_DIRECTORIES})
target_link_libraries(${TEST_NAME}
	PRIVATE fmt::fmt
    PRIVATE Catch2::Catch2WithMain)


# Enable automatic CTest registration
list(APPEND CMAKE_MODULE_PATH dependencies/Catch2/extras)
include(CTest)
include(Catch)
catch_discover_tests(${TEST_NAME})
