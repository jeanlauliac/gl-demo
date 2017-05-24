#!/usr/bin/env node

const {Manifest} = require('@jeanlauliac/upd-configure');

const manifest = new Manifest();

const resource_sources = [
  manifest.source("(src/resources/**/*.vs)"),
  manifest.source("(src/resources/**/*.fs)"),
];

const BUILD_DIR = '.build_files';

const resource_cpp_files = manifest.rule(
  manifest.cli_template("build/embed-resource.js", [
    {variables: ["output_file", "input_files"]},
  ]),
  resource_sources,
  `${BUILD_DIR}/($1).cpp`
);

const resource_index_file = manifest.rule(
  manifest.cli_template("build/build-resource-index.js", [
    {variables: ["output_file", "input_files"]},
  ]),
  resource_sources,
  `${BUILD_DIR}/headers/resources.h`
);

const generated_cpps = manifest.rule(
  manifest.cli_template('node', [
    {variables: ["input_files", "output_file"]},
  ]),
  [manifest.source('build/gen_(icosahedron)_cpp.js')],
  `${BUILD_DIR}/(generated_cpps/$1).cpp`
);

const compile_cpp_cli = manifest.cli_template('clang++', [
  {literals: ["-c", "-o"], variables: ["output_file"]},
  {
    literals: ["-std=c++14", "-Wall", "-fcolor-diagnostics", "-MMD", "-MF"],
    variables: ["dependency_file"]
  },
  {
    literals: ["-I", "/usr/local/include", "-I", `${BUILD_DIR}/headers`],
    variables: ["input_files"],
  },
]);

const compiled_cpp_files = manifest.rule(
  compile_cpp_cli,
  [
    manifest.source("(src/**/*).cpp"),
    generated_cpps,
    resource_cpp_files,
  ],
  `${BUILD_DIR}/($1).o`,
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
