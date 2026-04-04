#pragma once

#include <string>

#include "debug_session.h"

std::string debug_protocol_ok_json(const char* command);
std::string debug_protocol_error_json(const std::string& message);
std::string debug_protocol_uart_output_json(const DebugSession::UartOutputChunk& chunk);
