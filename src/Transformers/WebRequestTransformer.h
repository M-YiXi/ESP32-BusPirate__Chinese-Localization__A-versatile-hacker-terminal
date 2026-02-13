#pragma once
#include <string>
#include "Models/TerminalCommand.h"
#include <ArduinoJson.h>

class WebRequestTransformer {
public:
    TerminalCommand toTerminalCommand(const std::string& body);
    std::string toTerminalCommandRaw(const std::string& body);
    std::string toJsonResponse(const std::string& cliOutput);
};
