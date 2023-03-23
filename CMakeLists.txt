cmake_minimum_required(VERSION 3.25)
project(kieli)
set(CMAKE_CXX_STANDARD 20)

set(SOURCE_FILES
	src/main.cpp

	src/ast/ast.hpp
	src/ast/ast_formatting.cpp
	src/ast/lower/lower.hpp
	src/ast/lower/lower.cpp
	src/ast/lower/lowering_internals.hpp
	src/ast/lower/lower_definition.cpp
	src/ast/lower/lower_expression.cpp
	src/ast/lower/lower_pattern.cpp
	src/ast/lower/lower_type.cpp
	src/ast/nodes/definition.hpp
	src/ast/nodes/expression.hpp
	src/ast/nodes/pattern.hpp
	src/ast/nodes/type.hpp

	src/cir/cir.hpp
	src/cli/cli.hpp
	src/cli/cli.cpp

	src/hir/hir.hpp
	src/hir/hir_formatting.cpp
	src/hir/shared_formatting.cpp
	src/hir/nodes/definition.hpp
	src/hir/nodes/expression.hpp
	src/hir/nodes/pattern.hpp
	src/hir/nodes/type.hpp

	src/language/configuration.hpp

	src/lexer/lexer.cpp
	src/lexer/lexer.hpp
	src/lexer/lexer_test.cpp
	src/lexer/token.hpp
	src/lexer/token_formatting.cpp

	src/lir/lir.hpp

	src/mir/mir.hpp
	src/mir/mir_formatting.cpp
	src/mir/nodes/definition.hpp
	src/mir/nodes/expression.hpp
	src/mir/nodes/pattern.hpp
	src/mir/nodes/type.hpp

	src/parser/parser.cpp
	src/parser/parser.hpp
	src/parser/parser_internals.cpp
	src/parser/parser_internals.hpp
	src/parser/parser_test.cpp
	src/parser/parse_expression.cpp
	src/parser/parse_pattern.cpp
	src/parser/parse_type.cpp

	src/project/project.cpp
	src/project/project.hpp

	src/reification/expression_reification.cpp
	src/reification/pattern_reification.cpp
	src/reification/reification.cpp
	src/reification/reification.hpp
	src/reification/reification_internals.cpp
	src/reification/reification_internals.hpp
	src/reification/type_reification.cpp

	src/resolution/constraint.cpp
	src/resolution/definition_resolution.cpp
	src/resolution/expression_resolution.cpp
	src/resolution/method.cpp
	src/resolution/namespace_lookup.cpp
	src/resolution/pattern_resolution.cpp
	src/resolution/resolution.cpp
	src/resolution/resolution.hpp
	src/resolution/resolution_internals.cpp
	src/resolution/resolution_internals.hpp
	src/resolution/scope.cpp
	src/resolution/template_instantiation.cpp
	src/resolution/type_resolution.cpp
	src/resolution/unification.cpp

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

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${PROJECT_SOURCE_DIR}/dependencies/src/fmt/format.cc)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/utl/utilities.hpp)

target_include_directories(${PROJECT_NAME}
    PRIVATE src
    PRIVATE dependencies/include
    PRIVATE dependencies/include/rangev3)
