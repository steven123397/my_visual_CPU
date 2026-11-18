#pragma once

#include <string>

#include "debug_session.h"

std::string debug_protocol_ok_json(const char* command);
std::string debug_inspect_memory_json(uint64_t address, const std::vector<uint8_t>& bytes);
std::string debug_inspect_page_json(const PageInspection& page);
std::string debug_capture_metadata_json(const CaptureMetadata& metadata);
std::string debug_memory_record_json(const MemoryRecord& record);
std::string debug_capture_page_json(const CapturePage& page);
std::string debug_protocol_error_json(const std::string& message);
std::string debug_protocol_uart_output_json(const DebugSession::UartOutputChunk& chunk);
std::string debug_protocol_translation_plan_json(const DebugSession::TranslationPlanSnapshot& plan);
std::string debug_protocol_jit_dispatch_json(const DbtJitDryRunSummary& summary);
