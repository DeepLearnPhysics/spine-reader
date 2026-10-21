#pragma once

#include <string>
#include <vector>

namespace spine::core {

/// Parses the YAML SPINE writes for a list of identifiers (`yaml.safe_dump(["a", "b"])`):
/// block style, `- a` per line, or `[]`. Plain, single-quoted and double-quoted scalars.
/// Throws spine::Error on anything else.
std::vector<std::string> parse_yaml_list(const std::string& text);

}  // namespace spine::core
