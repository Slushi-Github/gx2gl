#include "gx2gl_shader_compat.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

struct GlobalLocationRegistry {
  std::unordered_map<std::string, uint32_t> varying_locations;
  uint32_t next_varying_location;
};

static GlobalLocationRegistry &get_location_registry() {
  static GlobalLocationRegistry registry = {
      {{"uv", 0u},
       {"uv_two", 1u},
       {"color", 2u},
       {"normal", 3u},
       {"tangent", 4u},
       {"bitangent", 5u},
       {"world_position", 6u},
       {"world_normal", 7u},
       {"camdist", 8u},
       {"hue_change", 9u},
       {"Position", 10u},
       {"Texcoord", 11u},
       {"SecondTexcoord", 12u},
       {"Color", 13u},
       {"Normal", 14u},
       {"Tangent", 15u},
       {"Bitangent", 16u},
       {"tc", 17u},
       {"pc", 18u},
       {"center", 19u},
       {"energy", 20u},
       {"col", 21u},
       {"radius", 22u},
       {"direction_scale_offset", 23u},
       {"o_color", 24u},
       {"color_lifetime", 25u},
       {"size", 26u},
       {"quadcorner", 27u},
       {"anglespeed", 28u}},
      29u};
  return registry;
}

static size_t skip_whitespace(const std::string &line, size_t index) {
  while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
    ++index;
  }
  return index;
}

static bool is_identifier_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

static bool starts_with_keyword(const std::string &line, size_t index,
                                const char *keyword) {
  size_t keyword_length = strlen(keyword);
  size_t end = index + keyword_length;

  if (end > line.size() || line.compare(index, keyword_length, keyword) != 0) {
    return false;
  }
  if (index > 0 && is_identifier_char(line[index - 1])) {
    return false;
  }
  if (end < line.size() && is_identifier_char(line[end])) {
    return false;
  }
  return true;
}

static bool consume_keyword(const std::string &line, size_t *index,
                            const char *keyword) {
  size_t cursor = skip_whitespace(line, *index);
  if (!starts_with_keyword(line, cursor, keyword)) {
    return false;
  }

  *index = cursor + strlen(keyword);
  return true;
}

static bool consume_identifier(const std::string &line, size_t *index,
                               std::string *identifier) {
  size_t cursor = skip_whitespace(line, *index);
  size_t start = cursor;

  if (cursor >= line.size() ||
      !((line[cursor] >= 'a' && line[cursor] <= 'z') ||
        (line[cursor] >= 'A' && line[cursor] <= 'Z') ||
        line[cursor] == '_')) {
    return false;
  }

  ++cursor;
  while (cursor < line.size() && is_identifier_char(line[cursor])) {
    ++cursor;
  }

  *identifier = line.substr(start, cursor - start);
  *index = cursor;
  return true;
}

static bool has_layout_key(const std::string &line, const char *key) {
  size_t layout_start = line.find("layout");
  size_t paren_start;
  size_t paren_end;

  if (layout_start == std::string::npos) {
    return false;
  }

  paren_start = line.find('(', layout_start);
  if (paren_start == std::string::npos) {
    return false;
  }
  paren_end = line.find(')', paren_start);
  if (paren_end == std::string::npos) {
    return false;
  }

  return line.substr(paren_start + 1, paren_end - paren_start - 1).find(key) !=
         std::string::npos;
}

static int extract_layout_integer(const std::string &line, const char *key) {
  size_t layout_start = line.find("layout");
  size_t paren_start;
  size_t paren_end;
  size_t key_pos;
  size_t value_pos;
  int value = 0;

  if (layout_start == std::string::npos) {
    return -1;
  }

  paren_start = line.find('(', layout_start);
  if (paren_start == std::string::npos) {
    return -1;
  }
  paren_end = line.find(')', paren_start);
  if (paren_end == std::string::npos) {
    return -1;
  }

  key_pos = line.find(key, paren_start + 1);
  if (key_pos == std::string::npos || key_pos >= paren_end) {
    return -1;
  }

  value_pos = line.find('=', key_pos);
  if (value_pos == std::string::npos || value_pos >= paren_end) {
    return -1;
  }

  ++value_pos;
  while (value_pos < paren_end &&
         (line[value_pos] == ' ' || line[value_pos] == '\t')) {
    ++value_pos;
  }

  while (value_pos < paren_end && line[value_pos] >= '0' && line[value_pos] <= '9') {
    value = (value * 10) + (line[value_pos] - '0');
    ++value_pos;
  }

  return value;
}

static std::string add_layout_qualifier(const std::string &line,
                                        const std::string &qualifier) {
  size_t indent_end = skip_whitespace(line, 0);
  size_t layout_start = line.find("layout", indent_end);

  if (layout_start == indent_end) {
    size_t paren_end = line.find(')', layout_start);
    if (paren_end != std::string::npos) {
      return line.substr(0, paren_end) + ", " + qualifier + line.substr(paren_end);
    }
  }

  return line.substr(0, indent_end) + "layout(" + qualifier + ") " +
         line.substr(indent_end);
}

static uint32_t next_unused_location(const std::unordered_map<std::string, uint32_t> &locations,
                                     uint32_t start) {
  std::unordered_set<uint32_t> used;
  for (std::unordered_map<std::string, uint32_t>::const_iterator it = locations.begin();
       it != locations.end(); ++it) {
    used.insert(it->second);
  }

  while (used.find(start) != used.end()) {
    ++start;
  }
  return start;
}

static uint32_t assign_global_varying_location(const std::string &name,
                                               int explicit_location) {
  GlobalLocationRegistry &registry = get_location_registry();
  std::unordered_map<std::string, uint32_t>::iterator existing =
      registry.varying_locations.find(name);

  if (explicit_location >= 0) {
    registry.varying_locations[name] = (uint32_t)explicit_location;
    if ((uint32_t)explicit_location >= registry.next_varying_location) {
      registry.next_varying_location = (uint32_t)explicit_location + 1u;
    }
    return (uint32_t)explicit_location;
  }

  if (existing != registry.varying_locations.end()) {
    return existing->second;
  }

  registry.next_varying_location =
      next_unused_location(registry.varying_locations, registry.next_varying_location);
  registry.varying_locations[name] = registry.next_varying_location;
  return registry.next_varying_location++;
}

static uint32_t assign_local_binding(std::unordered_map<std::string, uint32_t> *bindings,
                                     uint32_t *next_binding, const std::string &name) {
  std::unordered_map<std::string, uint32_t>::iterator existing = bindings->find(name);
  if (existing != bindings->end()) {
    return existing->second;
  }

  (*bindings)[name] = *next_binding;
  return (*next_binding)++;
}

static uint32_t assign_local_fragment_output(
    std::unordered_map<std::string, uint32_t> *outputs,
    std::unordered_set<uint32_t> *used_locations,
    uint32_t *next_location,
    const std::string &name) {
  std::unordered_map<std::string, uint32_t>::iterator existing = outputs->find(name);
  if (existing != outputs->end()) {
    return existing->second;
  }

  if (name == "FragColor" || name == "o_diffuse_color" || name == "Diff" ||
      name == "o_frag_color" || name == "o_final_color" ||
      name == "o_displace_mask" || name == "AO") {
    if (used_locations->find(0u) == used_locations->end()) {
      (*outputs)[name] = 0u;
      used_locations->insert(0u);
      if (*next_location == 0u) {
        *next_location = 1u;
      }
      return 0u;
    }
  } else if (name == "Spec" || name == "o_normal_color" ||
             name == "o_displace_ssr") {
    if (used_locations->find(1u) == used_locations->end()) {
      (*outputs)[name] = 1u;
      used_locations->insert(1u);
      if (*next_location <= 1u) {
        *next_location = 2u;
      }
      return 1u;
    }
  }

  while (used_locations->find(*next_location) != used_locations->end()) {
    ++(*next_location);
  }

  (*outputs)[name] = *next_location;
  used_locations->insert(*next_location);
  return (*next_location)++;
}

static void pre_scan_shader_source(
    const std::vector<std::string> &lines,
    GLenum shader_type,
    std::unordered_map<std::string, uint32_t> *sampler_bindings,
    uint32_t *next_sampler_binding,
    std::unordered_map<std::string, uint32_t> *fragment_outputs,
    std::unordered_set<uint32_t> *used_fragment_locations,
    uint32_t *next_fragment_location) {
  for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
    const std::string &line = lines[line_index];
    size_t cursor = 0;
    bool is_fragment_output = false;
    std::string storage;
    std::string type;
    std::string name;

    if (has_layout_key(line, "binding")) {
      int binding = extract_layout_integer(line, "binding");
      if (binding >= 0) {
        cursor = 0;
        if (consume_keyword(line, &cursor, "layout")) {
          size_t paren_end = line.find(')', cursor);
          if (paren_end != std::string::npos) {
            cursor = paren_end + 1;
          }
        }
        if (consume_keyword(line, &cursor, "uniform") &&
            consume_identifier(line, &cursor, &type) &&
            type.compare(0, 7, "sampler") == 0 &&
            consume_identifier(line, &cursor, &name)) {
          (*sampler_bindings)[name] = (uint32_t)binding;
          if ((uint32_t)binding >= *next_sampler_binding) {
            *next_sampler_binding = (uint32_t)binding + 1u;
          }
        }
      }
    }

    cursor = 0;
    if (consume_keyword(line, &cursor, "layout")) {
      size_t paren_end = line.find(')', cursor);
      if (paren_end != std::string::npos) {
        cursor = paren_end + 1;
      }
    }
    while (consume_keyword(line, &cursor, "flat") ||
           consume_keyword(line, &cursor, "smooth") ||
           consume_keyword(line, &cursor, "noperspective") ||
           consume_keyword(line, &cursor, "centroid") ||
           consume_keyword(line, &cursor, "invariant")) {
    }
    if ((shader_type == GL_VERTEX_SHADER || shader_type == GL_FRAGMENT_SHADER) &&
        (consume_keyword(line, &cursor, "in") ||
         consume_keyword(line, &cursor, "out"))) {
      size_t storage_start = skip_whitespace(line, 0);
      if (starts_with_keyword(line, storage_start, "layout")) {
        size_t paren_end = line.find(')', storage_start);
        storage_start = paren_end == std::string::npos ? storage_start : paren_end + 1;
        storage_start = skip_whitespace(line, storage_start);
      }
      while (starts_with_keyword(line, storage_start, "flat") ||
             starts_with_keyword(line, storage_start, "smooth") ||
             starts_with_keyword(line, storage_start, "noperspective") ||
             starts_with_keyword(line, storage_start, "centroid") ||
             starts_with_keyword(line, storage_start, "invariant")) {
        size_t qualifier_end = line.find_first_of(" \t", storage_start);
        storage_start = qualifier_end == std::string::npos ? line.size() : qualifier_end;
        storage_start = skip_whitespace(line, storage_start);
      }

      if (starts_with_keyword(line, storage_start, "in")) {
        storage = "in";
        storage_start += 2;
      } else if (starts_with_keyword(line, storage_start, "out")) {
        storage = "out";
        storage_start += 3;
      }

      cursor = storage_start;
      if (consume_identifier(line, &cursor, &type) &&
          consume_identifier(line, &cursor, &name)) {
        is_fragment_output = (shader_type == GL_FRAGMENT_SHADER && storage == "out");
        if (is_fragment_output && has_layout_key(line, "location")) {
          int location = extract_layout_integer(line, "location");
          if (location >= 0) {
            (*fragment_outputs)[name] = (uint32_t)location;
            used_fragment_locations->insert((uint32_t)location);
            if ((uint32_t)location >= *next_fragment_location) {
              *next_fragment_location = (uint32_t)location + 1u;
            }
          }
        }

        if ((shader_type == GL_VERTEX_SHADER && storage == "out") ||
            (shader_type == GL_FRAGMENT_SHADER && storage == "in")) {
          int location = has_layout_key(line, "location")
                             ? extract_layout_integer(line, "location")
                             : -1;
          assign_global_varying_location(name, location);
        }
      }
    }
  }
}

static std::vector<std::string> split_lines(const char *source) {
  std::vector<std::string> lines;
  size_t start = 0;
  size_t length = strlen(source);

  for (size_t i = 0; i < length; ++i) {
    if (source[i] == '\n') {
      lines.push_back(std::string(source + start, i - start));
      start = i + 1;
    }
  }

  if (start < length) {
    lines.push_back(std::string(source + start, length - start));
  } else if (length > 0 && source[length - 1] == '\n') {
    lines.push_back(std::string());
  }

  return lines;
}

static std::string join_lines(const std::vector<std::string> &lines) {
  std::string rebuilt;

  for (size_t i = 0; i < lines.size(); ++i) {
    if (i != 0) {
      rebuilt.push_back('\n');
    }
    rebuilt += lines[i];
  }

  return rebuilt;
}

static std::string normalize_version_directive(const std::string &line) {
  size_t cursor = skip_whitespace(line, 0);
  size_t comment_pos;
  std::string prefix;
  std::string directive;
  std::string suffix;
  size_t token_pos;

  if (cursor >= line.size() || line.compare(cursor, 8, "#version") != 0) {
    return line;
  }

  comment_pos = line.find("//", cursor);
  prefix = line.substr(0, cursor);
  if (comment_pos == std::string::npos) {
    directive = line.substr(cursor);
  } else {
    directive = line.substr(cursor, comment_pos - cursor);
    suffix = line.substr(comment_pos);
  }

  token_pos = directive.find(" core");
  if (token_pos != std::string::npos) {
    directive.erase(token_pos, 5);
  }
  token_pos = directive.find(" compatibility");
  if (token_pos != std::string::npos) {
    directive.erase(token_pos, 14);
  }

  return prefix + directive + suffix;
}

static bool is_fragment_color_type(const std::string &type) {
  return type == "vec4";
}

static void replace_identifier(std::string *line,
                               const std::string &from,
                               const std::string &to) {
  std::string replaced;
  size_t cursor = 0;

  if (!line || from.empty()) {
    return;
  }

  while (cursor < line->size()) {
    size_t found = line->find(from, cursor);
    if (found == std::string::npos) {
      replaced.append(*line, cursor, std::string::npos);
      break;
    }

    bool left_boundary = found == 0 || !is_identifier_char((*line)[found - 1]);
    size_t right = found + from.size();
    bool right_boundary = right >= line->size() || !is_identifier_char((*line)[right]);
    if (left_boundary && right_boundary) {
      replaced.append(*line, cursor, found - cursor);
      replaced += to;
      cursor = right;
    } else {
      replaced.append(*line, cursor, (found + 1u) - cursor);
      cursor = found + 1u;
    }
  }

  *line = replaced;
}

static bool parse_interface_declaration(const std::string &line,
                                        std::string *storage,
                                        std::string *type,
                                        std::string *name) {
  size_t cursor = 0;

  if (consume_keyword(line, &cursor, "layout")) {
    size_t paren_end = line.find(')', cursor);
    if (paren_end == std::string::npos) {
      return false;
    }
    cursor = paren_end + 1u;
  }

  while (consume_keyword(line, &cursor, "flat") ||
         consume_keyword(line, &cursor, "smooth") ||
         consume_keyword(line, &cursor, "noperspective") ||
         consume_keyword(line, &cursor, "centroid") ||
         consume_keyword(line, &cursor, "invariant")) {
  }

  if (consume_keyword(line, &cursor, "in")) {
    if (storage) {
      *storage = "in";
    }
  } else if (consume_keyword(line, &cursor, "out")) {
    if (storage) {
      *storage = "out";
    }
  } else {
    return false;
  }

  if (!consume_identifier(line, &cursor, type) ||
      !consume_identifier(line, &cursor, name)) {
    return false;
  }

  cursor = skip_whitespace(line, cursor);
  if (cursor < line.size() && line[cursor] == '[') {
    size_t bracket_end = line.find(']', cursor);
    if (bracket_end == std::string::npos) {
      return false;
    }
    cursor = skip_whitespace(line, bracket_end + 1u);
  }

  if (cursor >= line.size() || line[cursor] != ';') {
    return false;
  }

  return true;
}

} // namespace

char *gx2gl_prepare_shader_source_for_cafeglsl(const char *source,
                                               GLenum shader_type) {
  std::vector<std::string> lines;
  std::unordered_map<std::string, uint32_t> sampler_bindings;
  std::unordered_map<std::string, uint32_t> fragment_outputs;
  std::unordered_set<uint32_t> used_fragment_locations;
  uint32_t next_sampler_binding = 0u;
  uint32_t next_fragment_location = 0u;
  std::string primary_fragment_output_name;
  std::string rebuilt_source;
  char *prepared_source;

  if (!source) {
    return NULL;
  }

  lines = split_lines(source);
  for (size_t i = 0; i < lines.size(); ++i) {
    lines[i] = normalize_version_directive(lines[i]);
  }
  pre_scan_shader_source(lines, shader_type, &sampler_bindings, &next_sampler_binding,
                         &fragment_outputs, &used_fragment_locations,
                         &next_fragment_location);

  for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
    std::string &line = lines[line_index];
    size_t cursor = 0;
    std::string storage;
    std::string type;
    std::string name;

    if (consume_keyword(line, &cursor, "layout")) {
      size_t paren_end = line.find(')', cursor);
      if (paren_end != std::string::npos) {
        cursor = paren_end + 1;
      } else {
        continue;
      }
    }

    if (consume_keyword(line, &cursor, "uniform")) {
      size_t after_uniform = cursor;
      if (!consume_identifier(line, &after_uniform, &type)) {
        continue;
      }
      after_uniform = skip_whitespace(line, after_uniform);
      if (after_uniform >= line.size() || line[after_uniform] == '{') {
        /* CafeGLSL rejects modern layout(binding=...) syntax. gx2gl rebuilds
         * block binding state from shader reflection after compilation, so we
         * can keep the source block declarations untouched here. */
        continue;
      }

      /* CafeGLSL also rejects sampler layout(binding=...). Runtime sampler
       * bindings are tracked separately from reflection, so leave sampler
       * uniforms untouched. */
      continue;
    }

    cursor = 0;
    if (consume_keyword(line, &cursor, "layout")) {
      size_t paren_end = line.find(')', cursor);
      if (paren_end != std::string::npos) {
        cursor = paren_end + 1;
      } else {
        continue;
      }
    }
    while (consume_keyword(line, &cursor, "flat") ||
           consume_keyword(line, &cursor, "smooth") ||
           consume_keyword(line, &cursor, "noperspective") ||
           consume_keyword(line, &cursor, "centroid") ||
           consume_keyword(line, &cursor, "invariant")) {
    }

    if (!(consume_keyword(line, &cursor, "in") || consume_keyword(line, &cursor, "out"))) {
      continue;
    }

    if (!parse_interface_declaration(line, &storage, &type, &name)) {
      continue;
    }

    if (shader_type == GL_FRAGMENT_SHADER && storage == "out" &&
        is_fragment_color_type(type)) {
      uint32_t location;
      if (has_layout_key(line, "location")) {
        int explicit_location = extract_layout_integer(line, "location");
        if (explicit_location < 0) {
          continue;
        }
        location = (uint32_t)explicit_location;
      } else {
        location = assign_local_fragment_output(&fragment_outputs,
                                                &used_fragment_locations,
                                                &next_fragment_location,
                                                name);
      }

      if (location == 0u && primary_fragment_output_name.empty()) {
        primary_fragment_output_name = name;
        line.clear();
      }
      continue;
    }

    if (has_layout_key(line, "location")) {
      continue;
    }

    /* CafeGLSL's GLSL parser is stricter than desktop 3.3 and rejects the
     * explicit varying/output location decorations we used to synthesize here.
     * Stage matching still works by name, so keep these declarations plain. */
  }

  if (!primary_fragment_output_name.empty()) {
    for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
      replace_identifier(&lines[line_index], primary_fragment_output_name, "gl_FragColor");
    }
  }

  rebuilt_source = join_lines(lines);
  prepared_source = (char *)malloc(rebuilt_source.size() + 1u);
  if (!prepared_source) {
    return NULL;
  }

  memcpy(prepared_source, rebuilt_source.c_str(), rebuilt_source.size() + 1u);
  return prepared_source;
}
