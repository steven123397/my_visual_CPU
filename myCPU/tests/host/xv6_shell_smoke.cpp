#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "../../src/debug/debug_session.h"

namespace {

constexpr std::uint64_t kXv6ShellBootMaxSteps = 500000000ULL;
constexpr std::uint64_t kXv6ShellLsOutputMaxSteps = 250000000ULL;
constexpr std::uint64_t kXv6ShellCatOutputMaxSteps = 600000000ULL;
constexpr std::uint64_t kXv6ShellWcOutputMaxSteps = 400000000ULL;
constexpr std::uint64_t kXv6ShellPipeOutputMaxSteps = 20000000ULL;
constexpr std::uint64_t kXv6ShellForktestOutputMaxSteps = 50000000ULL;
constexpr std::uint64_t kXv6ShellForktestPromptMaxSteps = 20000000ULL;
constexpr std::uint64_t kXv6ShellStressfsOutputMaxSteps = 20000000ULL;
constexpr std::uint64_t kXv6ShellStressfsPromptMaxSteps = 200000000ULL;
constexpr std::uint64_t kXv6ShellMkdirPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellEchoPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellPipePromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellDirReadBackPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellDirRmPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellDirMissingReadOutputMaxSteps = 20000000ULL;
constexpr std::uint64_t kXv6ShellDirMissingReadPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellReadBackPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellRmPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellMissingReadOutputMaxSteps = 20000000ULL;
constexpr std::uint64_t kXv6ShellMissingReadPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellLsPromptMaxSteps = 20000000ULL;
constexpr std::uint64_t kXv6ShellCatPromptMaxSteps = 10000000ULL;
constexpr std::uint64_t kXv6ShellWcPromptMaxSteps = 10000000ULL;

struct ExpectedText {
    const char* needle;
    const char* message;
};

bool expect_file_exists(const std::filesystem::path& path, const char* message) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr, "%s: %s\n", message, path.string().c_str());
        return false;
    }
    return true;
}

bool expect_contains(const std::string& text, const char* needle, const char* message) {
    if (text.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", text.c_str());
        return false;
    }
    return true;
}

bool run_until_new_uart_contains(DebugSession& session,
                                 size_t offset,
                                 const char* needle,
                                 std::uint64_t max_steps,
                                 DebugSession::UartOutputChunk& chunk) {
    try {
        chunk = session.run_until_new_uart_contains(offset, needle, max_steps);
        return true;
    } catch (const std::runtime_error& error) {
        chunk = session.uart_output(offset);
        std::fprintf(stderr, "%s\n", error.what());
        std::fprintf(stderr, "xv6 shell smoke failed while waiting for UART text: %s\n", needle);
        std::fprintf(stderr, "recent output was:\n%s\n", chunk.text.c_str());
        return false;
    }
}

bool expect_chunk_contains(const DebugSession::UartOutputChunk& chunk,
                           const ExpectedText* expectations,
                           size_t expectation_count) {
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!expect_contains(chunk.text, expectations[i].needle, expectations[i].message)) {
            return false;
        }
    }
    return true;
}

bool run_shell_command_and_expect_prompt(DebugSession& session,
                                         size_t& offset,
                                         const char* command,
                                         const char* progress_needle,
                                         std::uint64_t progress_max_steps,
                                         std::uint64_t prompt_max_steps,
                                         const ExpectedText* expectations,
                                         size_t expectation_count) {
    session.uart_input(command);
    session.run_until_uart_contains(progress_needle, progress_max_steps);

    DebugSession::UartOutputChunk chunk{};
    if (!run_until_new_uart_contains(session, offset, "$ ", prompt_max_steps, chunk)) {
        return false;
    }

    if (!expect_chunk_contains(chunk, expectations, expectation_count)) {
        return false;
    }

    offset = chunk.next_offset;
    return true;
}

}  // namespace

int main() {
    const std::filesystem::path kernel_image = "external/xv6-riscv/kernel/kernel";
    const std::filesystem::path fs_image = "external/xv6-riscv/fs.img";

    if (!expect_file_exists(kernel_image, "xv6 shell smoke expects a built kernel image") ||
        !expect_file_exists(fs_image, "xv6 shell smoke expects a built filesystem image")) {
        return 1;
    }

    const std::string kernel_image_text = kernel_image.string();
    const std::string fs_image_text = fs_image.string();

    DebugSession session;
    session.load_elf(kernel_image_text,
                     BackendKind::Functional,
                     BlockTransport::VirtioBlk,
                     fs_image_text.c_str());

    session.run_until_uart_contains("$ ", kXv6ShellBootMaxSteps);
    const DebugSession::UartOutputChunk boot_chunk = session.uart_output(0);
    if (!expect_contains(boot_chunk.text,
                         "xv6 kernel is booting",
                         "xv6 shell smoke should print the boot banner before reaching the shell") ||
        !expect_contains(boot_chunk.text,
                         "init: starting sh",
                         "xv6 shell smoke should reach init before the shell prompt") ||
        !expect_contains(boot_chunk.text,
                         "$ ",
                         "xv6 shell smoke should reach the shell prompt")) {
        return 1;
    }

    const size_t shell_offset = boot_chunk.next_offset;
    size_t command_offset = shell_offset;

    constexpr ExpectedText kLsExpectations[] = {
        {"ls", "xv6 shell smoke should echo the requested ls command"},
        {"README", "xv6 shell smoke should list the root README entry"},
        {"dorphan", "xv6 shell smoke should reach the tail of the root directory listing"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after ls"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "ls\r",
                                             "dorphan",
                                             kXv6ShellLsOutputMaxSteps,
                                             kXv6ShellLsPromptMaxSteps,
                                             kLsExpectations,
                                             sizeof(kLsExpectations) / sizeof(kLsExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kCatExpectations[] = {
        {"cat README", "xv6 shell smoke should echo the requested cat command"},
        {"xv6 is a re-implementation",
         "xv6 shell smoke should print the start of the README contents"},
        {"make qemu\".", "xv6 shell smoke should print the tail of the README contents"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after cat README"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "cat README\r",
                                             "make qemu\".",
                                             kXv6ShellCatOutputMaxSteps,
                                             kXv6ShellCatPromptMaxSteps,
                                             kCatExpectations,
                                             sizeof(kCatExpectations) / sizeof(kCatExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kWcExpectations[] = {
        {"wc README", "xv6 shell smoke should echo the requested wc command"},
        {"48 334 2425 README",
         "xv6 shell smoke should print the current README line/word/byte counts"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after wc README"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "wc README\r",
                                             "48 334 2425 README",
                                             kXv6ShellWcOutputMaxSteps,
                                             kXv6ShellWcPromptMaxSteps,
                                             kWcExpectations,
                                             sizeof(kWcExpectations) / sizeof(kWcExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kPipeExpectations[] = {
        {"grep qemu README | wc",
         "xv6 shell smoke should echo the requested pipeline command"},
        {"2 12 106", "xv6 shell smoke should complete the grep-to-wc pipeline over README"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after the pipeline command"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "grep qemu README | wc\r",
                                             "2 12 106",
                                             kXv6ShellPipeOutputMaxSteps,
                                             kXv6ShellPipePromptMaxSteps,
                                             kPipeExpectations,
                                             sizeof(kPipeExpectations) / sizeof(kPipeExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kEchoExpectations[] = {
        {"echo SHELL_OK > xv6smoke.txt",
         "xv6 shell smoke should echo the requested redirected write command"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after creating the smoke file"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "echo SHELL_OK > xv6smoke.txt\r",
                                             "echo SHELL_OK > xv6smoke.txt",
                                             kXv6ShellEchoPromptMaxSteps,
                                             kXv6ShellEchoPromptMaxSteps,
                                             kEchoExpectations,
                                             sizeof(kEchoExpectations) / sizeof(kEchoExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kReadBackExpectations[] = {
        {"cat xv6smoke.txt", "xv6 shell smoke should echo the requested read-back command"},
        {"SHELL_OK", "xv6 shell smoke should read back the freshly written smoke file"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after cat xv6smoke.txt"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "cat xv6smoke.txt\r",
                                             "SHELL_OK",
                                             kXv6ShellCatOutputMaxSteps,
                                             kXv6ShellReadBackPromptMaxSteps,
                                             kReadBackExpectations,
                                             sizeof(kReadBackExpectations) /
                                                 sizeof(kReadBackExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kRmExpectations[] = {
        {"rm xv6smoke.txt", "xv6 shell smoke should echo the requested remove command"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after removing the smoke file"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "rm xv6smoke.txt\r",
                                             "rm xv6smoke.txt",
                                             kXv6ShellRmPromptMaxSteps,
                                             kXv6ShellRmPromptMaxSteps,
                                             kRmExpectations,
                                             sizeof(kRmExpectations) / sizeof(kRmExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kMissingReadExpectations[] = {
        {"cat xv6smoke.txt", "xv6 shell smoke should echo the requested missing-file read command"},
        {"cat: cannot open xv6smoke.txt",
         "xv6 shell smoke should report the missing smoke file after unlink"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after the missing-file read"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "cat xv6smoke.txt\r",
                                             "cat: cannot open xv6smoke.txt",
                                             kXv6ShellMissingReadOutputMaxSteps,
                                             kXv6ShellMissingReadPromptMaxSteps,
                                             kMissingReadExpectations,
                                             sizeof(kMissingReadExpectations) /
                                                 sizeof(kMissingReadExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kMkdirExpectations[] = {
        {"mkdir smdir", "xv6 shell smoke should echo the requested mkdir command"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after mkdir smdir"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "mkdir smdir\r",
                                             "mkdir smdir",
                                             kXv6ShellMkdirPromptMaxSteps,
                                             kXv6ShellMkdirPromptMaxSteps,
                                             kMkdirExpectations,
                                             sizeof(kMkdirExpectations) /
                                                 sizeof(kMkdirExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kDirWriteExpectations[] = {
        {"echo DIR_OK > smdir/note",
         "xv6 shell smoke should echo the requested nested-path write command"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after creating smdir/note"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "echo DIR_OK > smdir/note\r",
                                             "echo DIR_OK > smdir/note",
                                             kXv6ShellEchoPromptMaxSteps,
                                             kXv6ShellEchoPromptMaxSteps,
                                             kDirWriteExpectations,
                                             sizeof(kDirWriteExpectations) /
                                                 sizeof(kDirWriteExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kDirReadBackExpectations[] = {
        {"cat smdir/note", "xv6 shell smoke should echo the requested nested-path read command"},
        {"DIR_OK", "xv6 shell smoke should read back the nested-path file contents"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after cat smdir/note"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "cat smdir/note\r",
                                             "DIR_OK",
                                             kXv6ShellCatOutputMaxSteps,
                                             kXv6ShellDirReadBackPromptMaxSteps,
                                             kDirReadBackExpectations,
                                             sizeof(kDirReadBackExpectations) /
                                                 sizeof(kDirReadBackExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kDirFileRmExpectations[] = {
        {"rm smdir/note", "xv6 shell smoke should echo the requested nested file remove command"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after removing smdir/note"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "rm smdir/note\r",
                                             "rm smdir/note",
                                             kXv6ShellDirRmPromptMaxSteps,
                                             kXv6ShellDirRmPromptMaxSteps,
                                             kDirFileRmExpectations,
                                             sizeof(kDirFileRmExpectations) /
                                                 sizeof(kDirFileRmExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kDirRmExpectations[] = {
        {"rm smdir", "xv6 shell smoke should echo the requested directory remove command"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after removing smdir"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "rm smdir\r",
                                             "rm smdir",
                                             kXv6ShellDirRmPromptMaxSteps,
                                             kXv6ShellDirRmPromptMaxSteps,
                                             kDirRmExpectations,
                                             sizeof(kDirRmExpectations) /
                                                 sizeof(kDirRmExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kDirMissingReadExpectations[] = {
        {"cat smdir/note", "xv6 shell smoke should echo the requested missing nested-path read command"},
        {"cat: cannot open smdir/note",
         "xv6 shell smoke should report the missing nested-path file after directory cleanup"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after the missing nested-path read"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "cat smdir/note\r",
                                             "cat: cannot open smdir/note",
                                             kXv6ShellDirMissingReadOutputMaxSteps,
                                             kXv6ShellDirMissingReadPromptMaxSteps,
                                             kDirMissingReadExpectations,
                                             sizeof(kDirMissingReadExpectations) /
                                                 sizeof(kDirMissingReadExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kForktestExpectations[] = {
        {"forktest", "xv6 shell smoke should echo the requested forktest command"},
        {"fork test", "xv6 shell smoke should print the forktest progress banner"},
        {"fork test OK", "xv6 shell smoke should complete forktest successfully"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after forktest"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "forktest\r",
                                             "fork test OK",
                                             kXv6ShellForktestOutputMaxSteps,
                                             kXv6ShellForktestPromptMaxSteps,
                                             kForktestExpectations,
                                             sizeof(kForktestExpectations) /
                                                 sizeof(kForktestExpectations[0]))) {
        return 1;
    }

    constexpr ExpectedText kStressfsExpectations[] = {
        {"stressfs", "xv6 shell smoke should echo the requested stressfs command"},
        {"stressfs starting", "xv6 shell smoke should start the stressfs workload"},
        {"write 0", "xv6 shell smoke should exercise the first stressfs writer"},
        {"write 4", "xv6 shell smoke should exercise the last stressfs writer"},
        {"read", "xv6 shell smoke should reach the stressfs read-back phase"},
        {"$ ", "xv6 shell smoke should return to the shell prompt after stressfs"},
    };
    if (!run_shell_command_and_expect_prompt(session,
                                             command_offset,
                                             "stressfs\r",
                                             "stressfs starting",
                                             kXv6ShellStressfsOutputMaxSteps,
                                             kXv6ShellStressfsPromptMaxSteps,
                                             kStressfsExpectations,
                                             sizeof(kStressfsExpectations) /
                                                 sizeof(kStressfsExpectations[0]))) {
        return 1;
    }

    return 0;
}
