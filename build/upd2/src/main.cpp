#include "inspect.h"
#include "io.h"
#include "json/Lexer.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>

void compile_cpp_file(const std::string& cpp_path, const std::string& obj_path) {
  std::cout << "compile... " << cpp_path << std::endl;
  auto ret = system((
    std::string("clang++ -c -o ") + obj_path +
    " -Wall -std=c++14 -fcolor-diagnostics " +
    "-I /usr/local/include " + cpp_path
  ).c_str());
  if (ret != 0) {
    throw "compile failed";
  }
}

void compile_itself(const std::string& root_path) {
  auto main_obj_path = root_path + "/dist/main.o";
  auto io_obj_path = root_path + "/dist/io.o";
  compile_cpp_file(root_path + "/src/main.cpp", main_obj_path);
  compile_cpp_file(root_path + "/src/io.cpp", io_obj_path);
  std::cout << "link..." << std::endl;
  auto ret = system((
    std::string("clang++ -o ") + root_path + "/dist/upd " +
    "-Wall -std=c++14 -fcolor-diagnostics -L /usr/local/lib " +
    main_obj_path + " " + io_obj_path
  ).c_str());
  if (ret != 0) {
    throw "link failed";
  }
  std::cout << "done" << std::endl;
}

struct options {
  options(): dev(false), help(false), root(false), version(false) {};
  bool dev;
  bool help;
  bool root;
  bool version;
};

struct option_parse_error {
  option_parse_error(const std::string& arg): arg(arg) {}
  std::string arg;
};

options parse_options(int argc, char *argv[]) {
  options result;
  for (++argv, --argc; argc > 0; ++argv, --argc) {
    const auto arg = std::string(*argv);
    if (arg == "--root") {
      result.root = true;
    } else if (arg == "--version") {
      result.version = true;
    } else if (arg == "--help") {
      result.help = true;
    } else if (arg == "--dev") {
      result.dev = true;
    } else {
      throw option_parse_error(arg);
    }
  }
  return result;
}

void print_help() {
  std::cout
    << "usage: upd [targets] [options]" << std::endl
    << "options:" << std::endl
    << "  --version     print semantic version and exit" << std::endl
    << "  --help        print usage help and exit" << std::endl
    << "  --root        print the root directory path and exit" << std::endl
    ;
}

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
  const InspectOptions& options
) {
  switch (type) {
    case ManifestGroupType::glob:
      return "ManifestGroupType::glob";
    case ManifestGroupType::rule:
      return "ManifestGroupType::rule";
    case ManifestGroupType::alias:
      return "ManifestGroupType::alias";
  }
  throw std::runtime_error("cannot inspect ManifestGroupType");
}

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

struct Manifest {
  ManifestGroups groups;
};

template <size_t BufferSize>
Manifest read_json_manifest(json::Lexer<BufferSize>& lexer) {
  Manifest manifest;
  parse_json_object(lexer, [&manifest, &lexer](const std::string& name) {
    if (name == "groups") {
      manifest.groups = read_json_groups(lexer);
    } else {
      throw std::runtime_error(std::string("unexpected field `") + name + '`');
    }
  });
  return manifest;
}

template <size_t BufferSize>
Manifest read_manifest(const std::string& root_path) {
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
  const InspectOptions& options
) {
  return pretty_print_struct(
    "ManifestGroup",
    options,
    [&group](const InspectOptions& options) {
      return std::map<std::string, std::string>({
        { "type", inspect(group.type, options) },
        { "pattern", inspect(group.pattern, options) },
        { "command", inspect(group.command, options) },
        { "input_group", inspect(group.input_group, options) },
        { "output_pattern", inspect(group.output_pattern, options) },
        { "name", inspect(group.name, options) }
      });
    }
  );
}

std::string inspect(const Manifest& manifest, const InspectOptions& options) {
  return pretty_print_struct(
    "Manifest",
    options,
    [&manifest](const InspectOptions& options) {
      return std::map<std::string, std::string>({
        { "groups", inspect(manifest.groups, options) }
      });
    }
  );
}

int main(int argc, char *argv[]) {
  try {
    auto cli_opts = parse_options(argc, argv);
    if (cli_opts.version) {
      std::cout << "upd v0.1" << std::endl;
      return 0;
    }
    if (cli_opts.help) {
      print_help();
      return 0;
    }
    auto root_path = upd::io::find_root_path();
    if (cli_opts.root) {
      std::cout << root_path << std::endl;
      return 0;
    }
    if (cli_opts.dev) {
      auto manifest = read_manifest<128>(root_path);
      std::cout << inspect(manifest) << std::endl;
      return 0;
    }
    compile_itself(root_path);
  } catch (option_parse_error error) {
    std::cerr << "upd: fatal: invalid argument: `" << error.arg << "`" << std::endl;
    return 1;
  } catch (upd::io::cannot_find_updfile_error) {
    std::cerr << "upd: fatal: cannot find Updfile, in any of the parent directories" << std::endl;
    return 2;
  }
}
