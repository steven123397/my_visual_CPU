#pragma once

#include "dbt_block_plan.h"
#include "dbt_ir.h"

DbtTranslationUnit translate_dbt_block(const DbtBlockPlan& plan);
