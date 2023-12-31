#pragma once

#include <stdexcept>
#include <string_view>

class LuaParseException : public std::runtime_error {
public:
    explicit LuaParseException(std::string_view message);
};
