#pragma once

#include <iostream>
#include <map>
#include <sstream>

namespace upd {

struct global_inspect_options {
  unsigned int indent;
  unsigned int width;
};

struct inspect_options {
  global_inspect_options& global;
  unsigned int depth;
};

template <typename FieldMapper>
std::string pretty_print_struct(
  const std::string& name,
  const inspect_options& options,
  FieldMapper field_mapper
) {
  inspect_options inner_options =
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

std::string inspect(unsigned int value, const inspect_options& options);
std::string inspect(std::string value, const inspect_options& options);

template <typename TKey, typename TValue>
std::string inspect(
  const std::map<TKey, TValue>& map,
  const inspect_options& options
) {
  inspect_options inner_options =
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
  global_inspect_options global = { .indent = 2, .width = 60 };
  return inspect(value, { .global = global, .depth = 0 });
}

}
