#pragma once

#include "json/parser.h"
#include "path_glob.h"
#include "substitution.h"

namespace upd {
namespace manifest {

enum class update_rule_input_type { source, rule };

struct update_rule_input {
  static update_rule_input from_source(size_t ix) {
    return {
      .type = update_rule_input_type::source,
      .input_ix = ix,
    };
  }

  static update_rule_input from_rule(size_t ix) {
    return {
      .type = update_rule_input_type::rule,
      .input_ix = ix,
    };
  }

  update_rule_input_type type;
  size_t input_ix;
};

inline bool operator==(const update_rule_input& left, const update_rule_input& right) {
  return
    left.type == right.type &&
    left.input_ix == right.input_ix;
}

struct update_rule {
  size_t command_line_ix;
  std::vector<update_rule_input> inputs;
  substitution::pattern output;
};

inline bool operator==(const update_rule& left, const update_rule& right) {
  return
    left.command_line_ix == right.command_line_ix &&
    left.inputs == right.inputs &&
    left.output == right.output;
}

struct manifest {
  std::vector<path_glob::pattern> source_patterns;
  std::vector<update_rule> rules;
};

inline bool operator==(const manifest& left, const manifest& right) {
  return
    left.source_patterns == right.source_patterns &&
    left.rules == right.rules;
}

struct unexpected_element_error {};

template <typename Lexer>
struct source_pattern_array_handler {
  source_pattern_array_handler(std::vector<path_glob::pattern>& source_patterns):
    source_patterns_(source_patterns) {}
  void object(json::object_reader<Lexer>& read_object) const { throw unexpected_element_error(); }
  void array(json::array_reader<Lexer>& read_array) const { throw unexpected_element_error(); }
  void string_literal(const std::string& pattern_string) const {
    source_patterns_.push_back(path_glob::parse(pattern_string));
  }
  void number_literal(float number) { throw unexpected_element_error(); }

private:
  std::vector<path_glob::pattern>& source_patterns_;
};

template <typename Lexer>
struct source_patterns_handler {
  source_patterns_handler(std::vector<path_glob::pattern>& source_patterns):
    source_patterns_(source_patterns) {}
  void object(json::object_reader<Lexer>& read_object) const {
    throw unexpected_element_error();
  }
  void array(json::array_reader<Lexer>& read_array) const {
    source_pattern_array_handler<Lexer> handler(source_patterns_);
    read_array(handler);
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

struct expected_integer_number_error {};

template <typename Lexer>
struct read_size_t_handler {
  size_t object(json::object_reader<Lexer>& read_object) const {
    throw unexpected_element_error();
  }
  size_t array(json::array_reader<Lexer>& read_array) const {
    throw unexpected_element_error();
  }
  size_t string_literal(const std::string&) const { throw unexpected_element_error(); }
  size_t number_literal(float number) const {
    size_t result = static_cast<size_t>(number);
    if (static_cast<float>(result) != number) {
      throw expected_integer_number_error();
    }
    return result;
  }
};

template <typename Lexer>
struct read_rule_output_handler {
  substitution::pattern object(json::object_reader<Lexer>& read_object) const {
    throw unexpected_element_error();
  }
  substitution::pattern array(json::array_reader<Lexer>& read_array) const {
    throw unexpected_element_error();
  }
  substitution::pattern string_literal(const std::string& substitution_pattern_string) const {
    return substitution::parse(substitution_pattern_string);
  }
  substitution::pattern number_literal(float number) const { throw unexpected_element_error(); }
};

template <typename Lexer>
struct update_rule_array_handler {
  update_rule_array_handler(std::vector<update_rule>& rules):
    rules_(rules) {}
  void object(json::object_reader<Lexer>& read_object) const {
    update_rule rule;
    read_object([&rule](
      const std::string& field_name,
      json::field_value_reader<Lexer>& read_field_value
    ) {
      if (field_name == "command_line_ix") {
        rule.command_line_ix = read_field_value.template
          read<const read_size_t_handler<Lexer>, size_t>(read_size_t_handler<Lexer>());
        return;
      }
      if (field_name == "output") {
        rule.output = read_field_value.template
          read<const read_rule_output_handler<Lexer>, substitution::pattern>(read_rule_output_handler<Lexer>());
        return;
      }
      throw std::runtime_error("doesn't know field `" + field_name + "`");
    });
    rules_.push_back(rule);
  }
  void array(json::array_reader<Lexer>& read_array) const { throw unexpected_element_error(); }
  void string_literal(const std::string& pattern_string) const {  throw unexpected_element_error(); }
  void number_literal(float number) { throw unexpected_element_error(); }

private:
  std::vector<update_rule>& rules_;
};

template <typename Lexer>
struct update_rules_handler {
  update_rules_handler(std::vector<update_rule>& rules):
    rules_(rules) {}
  void object(json::object_reader<Lexer>& read_object) const {
    throw unexpected_element_error();
  }
  void array(json::array_reader<Lexer>& read_array) const {
    update_rule_array_handler<Lexer> handler(rules_);
    read_array(handler);
  }
  void string_literal(const std::string&) const { throw unexpected_element_error(); }
  void number_literal(float) const { throw unexpected_element_error(); }

private:
  std::vector<update_rule>& rules_;
};

template <typename Lexer>
void parse_update_rules(
  json::field_value_reader<Lexer>& read_field_value,
  std::vector<update_rule>& rules
) {
  update_rules_handler<Lexer> handler(rules);
  read_field_value.template read<update_rules_handler<Lexer>, void>(handler);
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
        return;
      }
      if (field_name == "rules") {
        parse_update_rules(read_field_value, result.rules);
        return;
      }
      throw std::runtime_error("doesn't know field `" + field_name + "`");
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
