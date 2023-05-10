add_library(libvm STATIC)

target_sources(libvm
    PRIVATE libvm/bytecode.hpp
    PRIVATE libvm/opcode.cpp
    PRIVATE libvm/opcode.hpp
    PRIVATE libvm/virtual_machine.cpp
    PRIVATE libvm/virtual_machine.hpp
    PRIVATE libvm/vm_formatting.cpp)

target_precompile_headers(libvm
    REUSE_FROM libutl-common)

target_link_libraries(libvm
    PRIVATE fmt::fmt)

