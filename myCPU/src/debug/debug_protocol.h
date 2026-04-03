#pragma once

#include <string>
#include <istream>
#include <ostream>

#include "debug_snapshot.h"

std::string debug_snapshot_json(const DebugSnapshot& snapshot);
int run_debug_cli(std::istream& in, std::ostream& out, std::ostream& err);
