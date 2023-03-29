#pragma once

#include "phase/reify/reify.hpp"
#include "representation/cir/cir.hpp"
#include "representation/lir/lir.hpp"


namespace compiler {

    struct Lower_result {};

    auto lower(Reify_result&&) -> Lower_result;

}