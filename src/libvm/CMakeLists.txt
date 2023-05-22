add_library(libvm STATIC)

target_sources(libvm
    PRIVATE libvm/bytecode.hpp
    PRIVATE libvm/opcode.cpp
    PRIVATE libvm/opcode.hpp
    PRIVATE libvm/virtual_machine.cpp
    PRIVATE libvm/virtual_machine.hpp
    PRIVATE libvm/vm_formatting.cpp)

kieli_consume_libutl_common(libvm)
kieli_add_tests(libvm TESTS libvm)
