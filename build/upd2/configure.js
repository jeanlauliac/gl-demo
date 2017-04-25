#!/usr/bin/env node

const updfile = require('./tools/lib/updfile');

const manifest = new updfile.Manifest();

const compile_cpp_cli = manifest.cli_template('clang++', [
  {literals: ["-c", "-o"], variables: ["output_file"]},
  {
    literals: ["-std=c++14", "-Wall", "-fcolor-diagnostics", "-MMD", "-MF"],
    variables: ["dependency_file"]
  },
  {literals: ["-I", "/usr/local/include"], variables: ["input_files"]},
]);

const compile_c_cli = manifest.cli_template('clang++', [
  {literals: ["-c", "-o"], variables: ["output_file"]},
  {
    literals: ["-x", "c", "-Wall", "-fcolor-diagnostics", "-MMD", "-MF"],
    variables: ["dependency_file"]
  },
  {literals: ["-I", "/usr/local/include"], variables: ["input_files"]},
]);

const cppt_sources = manifest.source("(src/lib/**/*).cppt");

const test_cpp_files = manifest.rule(
  manifest.cli_template('tools/compile_test.js', [
    {variables: ["input_files", "output_file", "dependency_file"]},
  ]),
  [cppt_sources],
  "dist/($1).cpp"
);

const compiled_cpp_files = manifest.rule(
  compile_cpp_cli,
  [manifest.source("(src/lib/**/*).cpp")],
  "dist/($1).o"
);

const compiled_c_files = manifest.rule(
  compile_c_cli,
  [manifest.source("(src/lib/**/*).c")],
  "dist/($1).o"
);

const tests_cpp_file = manifest.rule(
  manifest.cli_template('tools/index_tests.js', [
    {variables: ["output_file", "dependency_file", "input_files"]}
  ]),
  [cppt_sources],
  "dist/(tests).cpp"
);

const package_cpp_file = manifest.rule(
  manifest.cli_template('tools/gen_package_info.js', [
    {variables: ["output_file", "dependency_file", "input_files"]}
  ]),
  [manifest.source("package.json")],
  "dist/(package).cpp"
);

const compiled_main_files = manifest.rule(
  compile_cpp_cli,
  [manifest.source("(src/main).cpp"), package_cpp_file],
  "dist/($1).o"
);

const compiled_test_files = manifest.rule(
  compile_cpp_cli,
  [manifest.source("(tools/lib/testing).cpp"), test_cpp_files, tests_cpp_file],
  "dist/($1).o"
);

const link_cpp_cli = manifest.cli_template('clang++', [
  {literals: ["-o"], variables: ["output_file"]},
  {
    literals: ["-Wall", "-fcolor-diagnostics", "-std=c++14", "-L", "/usr/local/lib"],
    variables: ["input_files"]
  }
]);

manifest.rule(
  link_cpp_cli,
  [compiled_cpp_files, compiled_c_files, compiled_main_files],
  "dist/upd"
);

manifest.rule(
  link_cpp_cli,
  [compiled_cpp_files, compiled_c_files, compiled_test_files],
  "dist/unit_tests"
);

manifest.export(__dirname);
