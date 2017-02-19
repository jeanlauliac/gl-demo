#pragma once

#include "inspect.h"
#include "io.h"
#include "json/Lexer.h"
#include <map>

namespace upd {

template <size_t BufferSize, typename ReadFieldValue>
void parse_json_object(
  json::Lexer<BufferSize>& lexer,
  ReadFieldValue read_field_value
) {
  if (lexer.token.type != json::TokenType::brace_open) {
    throw std::runtime_error("expected object");
  }
  lexer.forward();
  while (lexer.token.type != json::TokenType::brace_close) {
    if (lexer.token.type != json::TokenType::string_literal) {
      throw std::runtime_error("expected string as field name");
    }
    auto name = lexer.token.string_value;
    lexer.forward();
    if (lexer.token.type != json::TokenType::colon) {
      throw std::runtime_error("expected colon");
    }
    lexer.forward();
    read_field_value(name);
    if (lexer.token.type == json::TokenType::comma) {
      lexer.forward();
      if (lexer.token.type == json::TokenType::brace_close) {
        throw std::runtime_error("unexpected closing brace after comma");
      }
    } else if (lexer.token.type != json::TokenType::brace_close) {
      std::cout << ">>>>" << (int)lexer.token.type << std::endl;
      throw std::runtime_error("expected closing brace");
    }
  }
  lexer.forward();
}

template <size_t BufferSize>
float read_json_number(json::Lexer<BufferSize>& lexer) {
  if (lexer.token.type != json::TokenType::number_literal) {
    throw std::runtime_error("cannot read integer");
  }
  auto number = lexer.token.number_value;
  lexer.forward();
  return number;
}

template <size_t BufferSize>
std::string read_json_string(json::Lexer<BufferSize>& lexer) {
  if (lexer.token.type != json::TokenType::string_literal) {
    throw std::runtime_error("cannot read string");
  }
  auto str = lexer.token.string_value;
  lexer.forward();
  return str;
}

enum class ManifestGroupType {
  glob,
  rule,
  alias,
};

std::string inspect(
  const ManifestGroupType& type,
  const inspect_options& options
);

struct ManifestGroup {
  ManifestGroupType type;
  std::string pattern;
  std::string command;
  int input_group;
  std::string output_pattern;
  std::string name;
};

template <size_t BufferSize>
ManifestGroup read_json_group(json::Lexer<BufferSize>& lexer) {
  ManifestGroup group;
  parse_json_object(lexer, [&group, &lexer](const std::string& name) {
    if (name == "type") {
      auto type_str = read_json_string(lexer);
      if (type_str == "glob") {
        group.type = ManifestGroupType::glob;
      } else if (type_str == "rule") {
        group.type = ManifestGroupType::rule;
      } else if (type_str == "alias") {
        group.type = ManifestGroupType::alias;
      } else {
        throw std::runtime_error(
          std::string("invalid group type `") + type_str + '`'
        );
      }
    } else if (name == "pattern") {
      group.pattern = read_json_string(lexer);
    } else if (name == "command") {
      group.command = read_json_string(lexer);
    } else if (name == "inputGroup") {
      group.input_group = static_cast<int>(read_json_number(lexer));
    } else if (name == "outputPattern") {
      group.output_pattern = read_json_string(lexer);
    } else if (name == "name") {
      group.name = read_json_string(lexer);
    } else {
      throw std::runtime_error(std::string("unexpected field `") + name + '`');
    }
  });
  return group;
}

typedef std::map<unsigned int, ManifestGroup> ManifestGroups;

template <size_t BufferSize>
ManifestGroups read_json_groups(json::Lexer<BufferSize>& lexer) {
  ManifestGroups groups;
  parse_json_object(lexer, [&groups, &lexer](const std::string& name) {
    auto group_id = atoi(name.c_str());
    groups[group_id] = read_json_group(lexer);
  });
  return groups;
}

struct manifest {
  ManifestGroups groups;
};

template <size_t BufferSize>
manifest read_json_manifest(json::Lexer<BufferSize>& lexer) {
  manifest result;
  parse_json_object(lexer, [&result, &lexer](const std::string& name) {
    if (name == "groups") {
      result.groups = read_json_groups(lexer);
    } else {
      throw std::runtime_error(std::string("unexpected field `") + name + '`');
    }
  });
  return result;
}

template <size_t BufferSize>
manifest read_manifest(const std::string& root_path) {
  auto manifest_path = root_path + upd::io::UPDFILE_SUFFIX;
  std::unique_ptr<FILE, decltype(&pclose)>
    pipe(popen(manifest_path.c_str(), "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("could not execute manifest");
  }
  json::Lexer<BufferSize> lexer(std::move(pipe));
  return read_json_manifest<BufferSize>(lexer);
}

std::string inspect(
  const ManifestGroup& group,
  const inspect_options& options
);

std::string inspect(const manifest& manifest, const inspect_options& options);

}
