static bool prepare_args_valid(const user_program_smoke_prepare_t* prepare);
static bool round_args_valid(const user_program_smoke_round_t* round);
static bool user_program_smoke_prepare_stage_valid(
    const user_program_smoke_prepare_stage_t* stage);
static bool user_program_smoke_create_staged_program(
    const user_program_smoke_prepare_stage_t* stage);
static bool user_program_smoke_prepare_stage_address_space(
    const user_program_smoke_prepare_stage_t* stage);
static bool user_program_smoke_prepare_stage_runtime(
    const user_program_smoke_prepare_stage_t* stage);
static bool user_program_smoke_round_stage_valid(
    const user_program_smoke_round_stage_t* stage);
static bool user_program_smoke_round_activate_if_needed(
    const user_program_smoke_round_stage_t* stage);
static bool user_program_smoke_round_enter_active(
    const user_program_smoke_round_stage_t* stage);
