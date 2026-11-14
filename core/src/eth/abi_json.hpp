#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "eth/abi.hpp"

namespace hft::eth::abi {

struct Param {
    std::string name;
    std::string type;
    bool indexed = false;
};

struct Function {
    std::string name;
    std::string signature;
    Selector selector{};
    std::vector<Param> inputs;
    std::vector<Param> outputs;
    std::string state_mutability;
};

struct Event {
    std::string name;
    std::string signature;
    Hash topic{};
    std::vector<Param> inputs;
    bool anonymous = false;
};

struct CustomError {
    std::string name;
    std::string signature;
    Selector selector{};
};

class Contract {
public:
    static Contract parse(std::string_view json);
    static Contract load_file(const std::string& path);

    const Function& function(std::string_view name) const;
    const Event& event(std::string_view name) const;

    const std::vector<Function>& functions() const { return functions_; }
    const std::vector<Event>& events() const { return events_; }
    const std::vector<CustomError>& errors() const { return errors_; }

    std::string error_name(std::span<const std::uint8_t> revert_data) const;

private:
    std::vector<Function> functions_;
    std::vector<Event> events_;
    std::vector<CustomError> errors_;
};

}
