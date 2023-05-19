add_library(libvm STATIC
    libvm/bytecode.hpp
    libvm/opcode.cpp
    libvm/opcode.hpp
    libvm/virtual_machine.cpp
    libvm/virtual_machine.hpp
    libvm/vm_formatting.cpp)
target_precompile_headers(libvm
    REUSE_FROM libutl-common)
target_link_libraries(libvm
    PRIVATE fmt::fmt)

add_kieli_test(libvm)
