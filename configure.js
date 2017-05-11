#!/usr/bin/env node

const updfile = require('./build/upd2/tools/lib/updfile');

const manifest = new updfile.Manifest();

const resource_sources = [
  manifest.source("(resources/**/*.vs)"),
  manifest.source("(resources/**/*.fs)"),
];

const resource_cpp_files = manifest.rule(
  manifest.cli_template("build/embed-resource.js", [
    {variables: ["output_file", "input_files"]},
  ]),
  resource_sources,
  ".build_files/($1).cpp"
);

const resource_index_file = manifest.rule(
  manifest.cli_template("build/build-resource-index.js", [
    {variables: ["output_file", "input_files"]},
  ]),
  resource_sources,
  ".build_files/resources.h"
);

const compile_cpp_cli = manifest.cli_template('clang++', [
  {literals: ["-c", "-o"], variables: ["output_file"]},
  {
    literals: ["-std=c++14", "-Wall", "-fcolor-diagnostics", "-MMD", "-MF"],
    variables: ["dependency_file"]
  },
  {literals: ["-I", "/usr/local/include"], variables: ["input_files"]},
]);

// FIXME: this should actually depend on `.build_files/resources.h`
const compiled_cpp_files = manifest.rule(
  compile_cpp_cli,
  [
    manifest.source("(ds/**/*).cpp"),
    manifest.source("(glfwpp/**/*).cpp"),
    manifest.source("(glpp/**/*).cpp"),
    manifest.source("(main).cpp"),
    resource_cpp_files,
  ],
  ".build_files/($1).o",
  [resource_index_file]
);

manifest.rule(
  manifest.cli_template('clang++', [
    {literals: ["-o"], variables: ["output_file"]},
    {
      literals: ['-framework', 'OpenGL', '-Wall', '-std=c++14',
        '-lglew', '-lglfw3', '-fcolor-diagnostics', '-L', '/usr/local/lib'],
      variables: ["input_files"]
    },
  ]),
  [compiled_cpp_files],
  "dist/gl-demo"
);

manifest.export(__dirname);
