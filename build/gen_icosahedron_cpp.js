#!/usr/bin/env node

'use strict';

const fs = require('fs');

(function main() {
  const outPath = process.argv[2];
  const mesh = getMesh();
  writeCpp(fs.createWriteStream(outPath), mesh);
})();

function getMesh() {
  const t = (1.0 + Math.sqrt(5.0)) / 2.0;
  const vertices = [];
  const add = position => {
    vertices.push({position, normal: normalize(position)});
  }
  forEachRectangleVertex(t, (x, y) => add([x, y, 0]));
  forEachRectangleVertex(t, (x, y) => add([0, x, y]));
  forEachRectangleVertex(t, (x, y) => add([y, 0, x]));
  return {
    vertices,
    triangles: [
      [0, 11, 5],
      [0, 5, 1],
      [0, 1, 7],
      [0, 7, 10],
      [0, 10, 11],

      [1, 5, 9],
      [5, 11, 4],
      [11, 10, 2],
      [10, 7, 6],
      [7, 1, 8],

      [3, 9, 4],
      [3, 4, 2],
      [3, 2, 6],
      [3, 6, 8],
      [3, 8, 9],

      [4, 9, 5],
      [2, 4, 11],
      [6, 2, 10],
      [8, 6, 7],
      [9, 8, 1],
    ],
  };
}

function forEachRectangleVertex(ratio, iter) {
  iter(-1, ratio);
  iter(1, ratio);
  iter(-1, -ratio);
  iter(1, -ratio);
}

function normalize(vector) {
  const frac = 1 / Math.sqrt(vector.reduce((acc, el) => acc + el * el, 0));
  return vector.map(el => el * frac);
}

function writeCpp(stream, mesh) {
  stream.write('#include "../../src/ds/icosahedron.h"\n\n');
  stream.write('namespace ds {\n\n');
  stream.write('mesh icosahedron = {\n');
  stream.write('  .vertices = {\n');
  mesh.vertices.forEach(vertex => {
    stream.write('    { .position = ');
    stream.write(`glm::vec3(${vertex.position.join(', ')})`);
    stream.write(', .normal = ');
    stream.write(`glm::vec3(${vertex.normal.join(', ')})`);
    stream.write(' },\n');
  });
  stream.write('  },\n');
  stream.write('  .triangles = {\n');
  mesh.triangles.forEach(vertex => {
    stream.write(`    glm::uvec3(${vertex.join(', ')}),\n`);
  });
  stream.write('  }\n');
  stream.write('};\n\n}\n');
}
