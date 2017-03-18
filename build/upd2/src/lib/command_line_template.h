#pragma once

#include <vector>
#include <string>

namespace upd {

/**
 * A command line template contains a number of variables that are replaced when
 * updating a particular file.
 */
enum class command_line_template_variable {
  input_files,
  output_files,
  dependency_file
};

/**
 * Describe a subsequence of a command line's arguments. It starts with a
 * sequence of literals, followed by a sequence of variables.
 */
struct command_line_template_part {
  command_line_template_part(
    std::vector<std::string> literal_args_,
    std::vector<command_line_template_variable> variable_args_
  ): literal_args(literal_args_), variable_args(variable_args_) {}

  std::vector<std::string> literal_args;
  std::vector<command_line_template_variable> variable_args;
};

/**
 * A command line template is composed as an alternance of literal and variable
 * args. These are arranged as subsequences ("parts") that describe a pair of
 * literals, variables. There is no way right now to have a single arg to be a
 * mix of literal and variable characters. Here's an example of command line:
 *
 *     clang++ -Wall -o a.out -L /usr/lib foo.o bar.o
 *
 * This is composed of the following subsequences (or "parts"):
 *
 *     * The "-Wall", "-o" literal arguments, followed by the
 *       output file(s) variable.
 *     * The "-L", "/usr/lib" literals, followed by the input files variable.
 *
 */
struct command_line_template {
  std::string binary_path;
  std::vector<command_line_template_part> parts;
};

/**
 * A pair of binary, arguments, ready to get executed.
 */
struct command_line {
  std::string binary_path;
  std::vector<std::string> args;
};

/**
 * Describe the data necessary to replace variables in a command line template.
 */
struct command_line_parameters {
  std::string dependency_file;
  std::vector<std::string> input_files;
  std::vector<std::string> output_files;
};

/**
 * Given a variable and parameters, pushes the corresponding values on the
 * argument list.
 */
void reify_command_line_arg(
  std::vector<std::string>& args,
  const command_line_template_variable variable_arg,
  const command_line_parameters& parameters
);

/**
 * Specialize a command line template for a particular set of files.
 */
command_line reify_command_line(
  const command_line_template& base,
  const command_line_parameters& parameters
);

}