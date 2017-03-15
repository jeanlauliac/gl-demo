#include "manifest.h"

namespace upd {

std::string inspect(
  const ManifestGroupType& type,
  const inspect_options& options
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

std::string inspect(
  const ManifestGroup& group,
  const inspect_options& options
) {
  return pretty_print_struct(
    "ManifestGroup",
    options,
    [&group](const inspect_options& options) {
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

std::string inspect(const manifest& manifest, const inspect_options& options) {
  return pretty_print_struct(
    "manifest",
    options,
    [&manifest](const inspect_options& options) {
      return std::map<std::string, std::string>({
        { "groups", inspect(manifest.groups, options) }
      });
    }
  );
}

}
