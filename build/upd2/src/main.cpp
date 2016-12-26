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

void compile_itself(const std::string& root_path) {
  auto ret = system((
    std::string("clang++ -c -o ") + root_path + "/dist/main.o " +
    "-Wall -std=c++14 -fcolor-diagnostics " +
    "-I /usr/local/include " + root_path + "/src/main.cpp"
  ).c_str());
  if (ret != 0) {
    throw "compile failed";
  }
  ret = system((
    std::string("clang++ -o ") + root_path + "/dist/upd " +
    "-Wall -std=c++14 -fcolor-diagnostics -L /usr/local/lib " +
    root_path + "/dist/main.o"
  ).c_str());
  if (ret != 0) {
    throw "link failed";
  }
}

std::string getcwd_string() {
  char temp[MAXPATHLEN];
  if (getcwd(temp, MAXPATHLEN) == nullptr) {
    throw std::runtime_error("unable to get current working directory");
  }
  return temp;
}

const char* UPDFILE_SUFFIX = "/Updfile";

std::string dirname_string(const std::string& path) {
  if (path.size() >= MAXPATHLEN) {
    throw std::runtime_error("string too long");
  }
  char temp[MAXPATHLEN];
  strcpy(temp, path.c_str());
  return dirname(temp);
}

std::string find_root_path() {
  std::string path = getcwd_string();
  std::ifstream updfile(path + UPDFILE_SUFFIX);
  while (!updfile.is_open() && path != "/") {
    path = dirname_string(path);
    updfile.open(path + UPDFILE_SUFFIX);
  }
  if (!updfile.is_open()) {
    throw std::runtime_error("unable to find Updfile directory");
  }
  return path;
}

struct Options {
  Options(): root(false), help(false), version(false) {};
  bool root;
  bool help;
  bool version;
};

Options parse_options(int argc, char *argv[]) {
  Options options;
  for (++argv, --argc; argc > 0; ++argv, --argc) {
    const auto arg = std::string(*argv);
    if (arg == "--root") {
      options.root = true;
    } else if (arg == "--version") {
      options.version = true;
    } else if (arg == "--help") {
      options.help = true;
    } else {
      throw std::runtime_error(
        std::string("invalid argument: ") + arg
      );
    }
  }
  return options;
}

void print_help() {
  std::cout
    << "usage: upd [targets] [options]" << std::endl
    << "options:" << std::endl
    << "  --version   Print semantic version and exit" << std::endl
    << "  --help      Print usage help and exit" << std::endl
    << "  --root      Print the root directory path and exit" << std::endl
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

struct ManifestGroup {
  std::string type;
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
      group.type = read_json_string(lexer);
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
  auto manifest_path = root_path + UPDFILE_SUFFIX;
  std::unique_ptr<FILE, decltype(&pclose)>
    pipe(popen(manifest_path.c_str(), "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("could not execute manifest");
  }
  json::Lexer<BufferSize> lexer(std::move(pipe));
  return read_json_manifest<BufferSize>(lexer);
}

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

template <typename T>
std::string inspect(
  T value,
  const InspectOptions& options
) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

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
      << '{' << iter->first << ", " << iter->second << '}';
  }
  stream << " })";
  return stream.str();
}

std::string inspect(
  const ManifestGroup& manifest,
  const InspectOptions& options
) {
  return pretty_print_struct(
    "ManifestGroup",
    options,
    [&manifest](const InspectOptions& options) {
      return std::map<std::string, std::string>();
    }
  );
}

std::string inspect(const Manifest& manifest, const InspectOptions& options) {
  return pretty_print_struct(
    "Manifest",
    options,
    [&manifest](const InspectOptions& options) {
      return std::map<std::string, std::string>(
        {{"groups", inspect(manifest.groups, options)}}
      );
    }
  );
}

template <typename T>
std::string inspect(const T& value) {
  GlobalInspectOptions global = {.indent = 2, .width = 60};
  return inspect(value, {.global = global, .depth = 0});
}

int main(int argc, char *argv[]) {
  auto options = parse_options(argc, argv);
  auto root_path = find_root_path();
  if (options.version) {
    std::cout << "Upd version 0.1" << std::endl;
    return 0;
  }
  if (options.help) {
    print_help();
    return 0;
  }
  if (options.root) {
    std::cout << root_path << std::endl;
    return 0;
  }
  auto manifest = read_manifest<128>(root_path);
  std::cout << inspect(manifest) << std::endl;
  //compile_itself(root_path);
}
