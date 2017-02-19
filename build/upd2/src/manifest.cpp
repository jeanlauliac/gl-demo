#include "manifest.h"

namespace upd {

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

}
