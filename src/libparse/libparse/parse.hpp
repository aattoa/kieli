#pragma once

#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace kieli {
    auto parse(Database& db, Document_id document_id) -> CST;
}
