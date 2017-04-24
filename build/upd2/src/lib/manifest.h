#pragma once

#include "json/parser.h"
#include "path_glob.h"

namespace upd {
namespace manifest {

struct manifest {
  std::vector<path_glob::pattern> source_patterns;
};

inline bool operator==(const manifest& left, const manifest& right) {
  return left.source_patterns == right.source_patterns;
}

struct unexpected_element_error {};

template <typename Lexer>
struct source_patterns_handler {
  source_patterns_handler(std::vector<path_glob::pattern>& source_patterns):
    source_patterns_(source_patterns) {}
  void object(json::object_reader<Lexer>& read_object) const {
    throw unexpected_element_error();
  }
  void array(json::array_reader<Lexer>& read_object) const {
    throw std::runtime_error("not implemented");
  }
  void string_literal(const std::string&) const { throw unexpected_element_error(); }
  void number_literal(float) const { throw unexpected_element_error(); }

private:
  std::vector<path_glob::pattern>& source_patterns_;
};

template <typename Lexer>
void parse_source_patterns(
  json::field_value_reader<Lexer>& read_field_value,
  std::vector<path_glob::pattern>& source_patterns
) {
  source_patterns_handler<Lexer> handler(source_patterns);
  read_field_value.template read<source_patterns_handler<Lexer>, void>(handler);
}

template <typename Lexer>
struct manifest_expression_handler {
  manifest object(json::object_reader<Lexer>& read_object) const {
    manifest result;
    read_object([&result](
      const std::string& field_name,
      json::field_value_reader<Lexer>& read_field_value
    ) {
      if (field_name == "source_patterns") {
        parse_source_patterns(read_field_value, result.source_patterns);
      }
    });
    return result;
  }
  manifest array(json::array_reader<Lexer>& read_object) const { throw unexpected_element_error(); }
  manifest string_literal(const std::string&) const { throw unexpected_element_error(); }
  manifest number_literal(float) const { throw unexpected_element_error(); }
};

template <typename Lexer>
manifest parse(Lexer& lexer) {
  manifest result;
  manifest_expression_handler<Lexer> handler;
  return json::parse_expression<Lexer, manifest_expression_handler<Lexer>, manifest>(lexer, handler);
}

}
}
