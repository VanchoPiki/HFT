#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "eth/abi.hpp"

namespace hft::eth::abi {

struct Param {
    std::string name;
    std::string type;  // canonical: tuples are expanded to "(t1,t2)", e.g. "(address,uint24)[]"
    bool indexed = false;
};

struct Function {
    std::string name;
    std::string signature;  // "quoteExactInputSingle((address,address,uint256,uint24,uint160))"
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

// Solidity custom error, e.g. InsufficientProfit(int256,uint256): revert data starts with
// its selector.
struct CustomError {
    std::string name;
    std::string signature;
    Selector selector{};
};

// Contract ABI loaded from JSON: either a plain ABI array or a Foundry/Hardhat artifact
// object with an "abi" field.
class Contract {
public:
    static Contract parse(std::string_view json);
    static Contract load_file(const std::string& path);

    // Throws abi::Error if the name is missing or overloaded.
    const Function& function(std::string_view name) const;
    const Event& event(std::string_view name) const;

    const std::vector<Function>& functions() const { return functions_; }
    const std::vector<Event>& events() const { return events_; }
    const std::vector<CustomError>& errors() const { return errors_; }

    // Name of the custom error whose selector starts `revert_data`, if any.
    std::string error_name(std::span<const std::uint8_t> revert_data) const;

private:
    std::vector<Function> functions_;
    std::vector<Event> events_;
    std::vector<CustomError> errors_;
};

}  // namespace hft::eth::abi
