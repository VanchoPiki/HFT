#include "eth/abi_json.hpp"

#include <simdjson.h>

#include <algorithm>

namespace hft::eth::abi {
namespace {

std::string canonical_type(simdjson::dom::element param) {
    const std::string_view type = param["type"];
    if (!type.starts_with("tuple")) {
        return std::string(type);
    }
    std::string out = "(";
    bool first = true;
    for (simdjson::dom::element component : param["components"].get_array()) {
        if (!first) {
            out += ',';
        }
        first = false;
        out += canonical_type(component);
    }
    out += ')';
    out += type.substr(5);
    return out;
}

std::vector<Param> parse_params(simdjson::dom::element entry, std::string_view key) {
    std::vector<Param> params;
    simdjson::dom::array array;
    if (entry[key].get(array) != simdjson::SUCCESS) {
        return params;
    }
    for (simdjson::dom::element p : array) {
        Param param;
        std::string_view name;
        if (p["name"].get(name) == simdjson::SUCCESS) {
            param.name = name;
        }
        param.type = canonical_type(p);
        bool indexed = false;
        if (p["indexed"].get(indexed) == simdjson::SUCCESS) {
            param.indexed = indexed;
        }
        params.push_back(std::move(param));
    }
    return params;
}

std::string make_signature(std::string_view name, const std::vector<Param>& inputs) {
    std::string sig(name);
    sig += '(';
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (i > 0) {
            sig += ',';
        }
        sig += inputs[i].type;
    }
    sig += ')';
    return sig;
}

template <typename T>
const T& find_unique(const std::vector<T>& items, std::string_view name, const char* what) {
    const T* found = nullptr;
    for (const auto& item : items) {
        if (item.name == name) {
            if (found != nullptr) {
                throw Error(std::string("abi: ") + what + " '" + std::string(name) +
                            "' is overloaded");
            }
            found = &item;
        }
    }
    if (found == nullptr) {
        throw Error(std::string("abi: ") + what + " '" + std::string(name) + "' not found");
    }
    return *found;
}

}

Contract Contract::parse(std::string_view json) {
    simdjson::dom::parser parser;
    const simdjson::padded_string padded(json);
    simdjson::dom::element doc = parser.parse(padded);
    simdjson::dom::array entries;
    if (doc.get(entries) != simdjson::SUCCESS) {
        entries = doc["abi"].get_array();
    }

    Contract contract;
    for (simdjson::dom::element entry : entries) {
        const std::string_view kind = entry["type"];
        if (kind == "function") {
            Function f;
            f.name = std::string_view(entry["name"]);
            f.inputs = parse_params(entry, "inputs");
            f.outputs = parse_params(entry, "outputs");
            std::string_view mutability;
            if (entry["stateMutability"].get(mutability) == simdjson::SUCCESS) {
                f.state_mutability = mutability;
            }
            f.signature = make_signature(f.name, f.inputs);
            f.selector = selector(f.signature);
            contract.functions_.push_back(std::move(f));
        } else if (kind == "event") {
            Event e;
            e.name = std::string_view(entry["name"]);
            e.inputs = parse_params(entry, "inputs");
            bool anonymous = false;
            if (entry["anonymous"].get(anonymous) == simdjson::SUCCESS) {
                e.anonymous = anonymous;
            }
            e.signature = make_signature(e.name, e.inputs);
            e.topic = event_topic(e.signature);
            contract.events_.push_back(std::move(e));
        } else if (kind == "error") {
            CustomError err;
            err.name = std::string_view(entry["name"]);
            err.signature = make_signature(err.name, parse_params(entry, "inputs"));
            err.selector = selector(err.signature);
            contract.errors_.push_back(std::move(err));
        }
    }
    return contract;
}

std::string Contract::error_name(std::span<const std::uint8_t> revert_data) const {
    if (revert_data.size() < 4) {
        return {};
    }
    for (const auto& err : errors_) {
        if (std::equal(err.selector.begin(), err.selector.end(), revert_data.begin())) {
            return err.name;
        }
    }
    return {};
}

Contract Contract::load_file(const std::string& path) {
    const simdjson::padded_string json = simdjson::padded_string::load(path);
    return parse(std::string_view(json.data(), json.size()));
}

const Function& Contract::function(std::string_view name) const {
    return find_unique(functions_, name, "function");
}

const Event& Contract::event(std::string_view name) const {
    return find_unique(events_, name, "event");
}

}
