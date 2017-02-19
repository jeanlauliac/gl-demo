#pragma once

#include <iostream>
#include <map>
#include <sstream>

namespace upd {

struct GlobalInspectOptions {
  unsigned int indent;
  unsigned int width;
};

struct InspectOptions {
  GlobalInspectOptions& global;
  unsigned int depth;
};

template <typename FieldMapper>
std::string pretty_print_struct(
  const std::string& name,
  const InspectOptions& options,
  FieldMapper field_mapper
) {
  InspectOptions inner_options =
    {.depth = options.depth + 1, .global = options.global};
  std::map<std::string, std::string> string_map = field_mapper(inner_options);
  std::ostringstream stream;
  std::string indent_spaces =
    std::string((options.depth + 1) * options.global.indent, ' ');
  stream << name << '(';
  if (string_map.size() == 0) {
    stream << ')';
    return stream.str();
  }
  stream << '{';
  for (auto iter = string_map.begin(); iter != string_map.end(); ++iter) {
    if (iter != string_map.begin()) {
      stream << ',';
    }
    stream << std::endl << indent_spaces
      << '.' << iter->first << " = " << iter->second;
  }
  stream << " })";
  return stream.str();
}

std::string inspect(
  unsigned int value,
  const InspectOptions& options
);

std::string inspect(
  std::string value,
  const InspectOptions& options
);

template <typename TKey, typename TValue>
std::string inspect(
  const std::map<TKey, TValue>& map,
  const InspectOptions& options
) {
  InspectOptions inner_options =
    {.depth = options.depth + 1, .global = options.global};
  std::map<std::string, std::string> string_map;
  for (auto iter = map.begin(); iter != map.end(); ++iter) {
    string_map[inspect(iter->first, inner_options)] =
      inspect(iter->second, inner_options);
  }
  std::ostringstream stream;
  std::string indent_spaces =
    std::string((options.depth + 1) * options.global.indent, ' ');
  stream << "std::map(";
  if (string_map.size() == 0) {
    stream << ')';
    return stream.str();
  }
  stream << '{';
  for (auto iter = string_map.begin(); iter != string_map.end(); ++iter) {
    if (iter != string_map.begin()) {
      stream << ',';
    }
    stream << std::endl << indent_spaces
      << "{ " << iter->first << ", " << iter->second << " }";
  }
  stream << " })";
  return stream.str();
}

template <typename T>
std::string inspect(const T& value) {
  GlobalInspectOptions global = { .indent = 2, .width = 60 };
  return inspect(value, { .global = global, .depth = 0 });
}

}
