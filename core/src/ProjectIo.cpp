#include "ProjectIo.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

#include <nlohmann/json.hpp>

#include "ConnectionInfo.hpp"
#include "Log.hpp"

namespace tg {
namespace {

using json = nlohmann::json;

constexpr const char* kJsonKeyFileVersion = "fileVersion";
constexpr const char* kJsonKeyName = "name";
constexpr const char* kJsonKeyGraphs = "graphs";
constexpr const char* kJsonKeyFile = "file";
constexpr const char* kJsonKeyNodes = "nodes";
constexpr const char* kJsonKeyConnections = "connections";
constexpr const char* kJsonKeyId = "id";
constexpr const char* kJsonKeyType = "type";
constexpr const char* kJsonKeyProperties = "properties";
constexpr const char* kJsonKeyValue = "value";
constexpr const char* kJsonKeyValueType = "valueType";
constexpr const char* kJsonKeyInputs = "inputs";
constexpr const char* kJsonKeyIndex = "index";
constexpr const char* kJsonKeyPresentation = "presentation";
constexpr const char* kJsonKeyFrom = "from";
constexpr const char* kJsonKeyOut = "out";
constexpr const char* kJsonKeyTo = "to";
constexpr const char* kJsonKeyIn = "in";
constexpr const char* kJsonKeyPosition = "position";
constexpr const char* kJsonKeyX = "x";
constexpr const char* kJsonKeyY = "y";
constexpr const char* kValueTypeInt = "int";
constexpr const char* kValueTypeString = "string";
constexpr const char* kValueTypeBool = "bool";
constexpr const char* kPresentationPort = "port";
constexpr const char* kPresentationProperty = "property";
constexpr const char* kTempFileSuffix = ".tmp";
constexpr int kJsonIndentSpaces = 2;

json PropertyValueToJson(const PropertyValue& value) {
  json out;
  std::visit(
      [&out](const auto& held) {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, int>) {
          out[kJsonKeyValueType] = kValueTypeInt;
          out[kJsonKeyValue] = held;
        } else if constexpr (std::is_same_v<Held, std::string>) {
          out[kJsonKeyValueType] = kValueTypeString;
          out[kJsonKeyValue] = held;
        } else if constexpr (std::is_same_v<Held, bool>) {
          out[kJsonKeyValueType] = kValueTypeBool;
          out[kJsonKeyValue] = held;
        }
      },
      value);
  return out;
}

bool PropertyValueFromJson(const json& object, PropertyValue& out_value) {
  if (!object.contains(kJsonKeyValueType) || !object.contains(kJsonKeyValue)) {
    return false;
  }
  const std::string type = object.at(kJsonKeyValueType).get<std::string>();
  if (type == kValueTypeInt) {
    out_value = object.at(kJsonKeyValue).get<int>();
    return true;
  }
  if (type == kValueTypeString) {
    out_value = object.at(kJsonKeyValue).get<std::string>();
    return true;
  }
  if (type == kValueTypeBool) {
    out_value = object.at(kJsonKeyValue).get<bool>();
    return true;
  }
  return false;
}

std::string PresentationToString(InputPresentation presentation) {
  return presentation == InputPresentation::Property ? kPresentationProperty
                                                     : kPresentationPort;
}

InputPresentation PresentationFromString(const std::string& text) {
  if (text == kPresentationProperty) {
    return InputPresentation::Property;
  }
  return InputPresentation::Port;
}

bool WriteTextAtomic(const std::filesystem::path& path,
                     const std::string& text) {
  const std::filesystem::path temp_path =
      path.string() + kTempFileSuffix;
  {
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      return false;
    }
    output << text;
    if (!output) {
      return false;
    }
  }
  std::error_code remove_error;
  std::filesystem::remove(path, remove_error);
  std::error_code rename_error;
  std::filesystem::rename(temp_path, path, rename_error);
  if (!rename_error) {
    return true;
  }
  // Windows 上目标仍占用时 rename 可能失败：覆盖拷贝后再删临时文件。
  std::error_code copy_error;
  std::filesystem::copy_file(temp_path, path,
                             std::filesystem::copy_options::overwrite_existing,
                             copy_error);
  std::filesystem::remove(temp_path, remove_error);
  if (copy_error) {
    logError("atomic write failed for {}: rename={}, copy={}", path.string(),
             rename_error.message(), copy_error.message());
    return false;
  }
  return true;
}

json SerializeNode(const Node& node, std::size_t node_id,
                   const std::filesystem::path& root,
                   const NodePoseGetter& pose_getter) {
  json node_json;
  node_json[kJsonKeyId] = static_cast<int>(node_id);
  // kind 与 identifier 分开存：type 是类型 id，name 是图内唯一显示名。
  node_json[kJsonKeyType] =
      node.typeId().empty() ? node.getName() : node.typeId();
  node_json[kJsonKeyName] = node.getName();

  json properties = json::array();
  for (const auto& property : node.getProperties()) {
    if (property == nullptr) {
      continue;
    }
    json property_json = PropertyValueToJson(property->value);
    property_json[kJsonKeyName] = property->name;
    if (std::holds_alternative<std::string>(property->value)) {
      property_json[kJsonKeyValue] = ProjectIo::toProjectRelative(
          root, std::get<std::string>(property->value));
    }
    properties.push_back(std::move(property_json));
  }
  node_json[kJsonKeyProperties] = std::move(properties);

  json inputs = json::array();
  const unsigned int input_count = node.inputPortCount();
  for (unsigned int index = 0; index < input_count; ++index) {
    json input_json;
    input_json[kJsonKeyIndex] = static_cast<int>(index);
    input_json[kJsonKeyPresentation] =
        PresentationToString(node.inputPresentation(index));
    if (auto property = node.inputProperty(index)) {
      json value_json = PropertyValueToJson(property->value);
      if (std::holds_alternative<std::string>(property->value)) {
        value_json[kJsonKeyValue] = ProjectIo::toProjectRelative(
            root, std::get<std::string>(property->value));
      }
      input_json[kJsonKeyValueType] = value_json[kJsonKeyValueType];
      input_json[kJsonKeyValue] = value_json[kJsonKeyValue];
    }
    inputs.push_back(std::move(input_json));
  }
  node_json[kJsonKeyInputs] = std::move(inputs);

  if (pose_getter) {
    NodePose pose;
    if (pose_getter(node, pose)) {
      node_json[kJsonKeyPosition] = json{{kJsonKeyX, pose.x}, {kJsonKeyY, pose.y}};
    }
  }
  return node_json;
}

json SerializeGraph(const Graph& graph, const std::filesystem::path& root,
                    const NodePoseGetter& pose_getter) {
  json graph_json;
  graph_json[kJsonKeyName] = graph.getName();

  const auto& nodes = graph.getNodes();
  std::unordered_map<const Node*, int> node_ids;
  json nodes_json = json::array();
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    const auto& node = nodes[index];
    if (node == nullptr) {
      continue;
    }
    node_ids[node.get()] = static_cast<int>(index);
    nodes_json.push_back(SerializeNode(*node, index, root, pose_getter));
  }
  graph_json[kJsonKeyNodes] = std::move(nodes_json);

  json connections = json::array();
  for (const auto& node : nodes) {
    if (node == nullptr) {
      continue;
    }
    const auto from_it = node_ids.find(node.get());
    if (from_it == node_ids.end()) {
      continue;
    }
    for (const ConnectionInfo& edge : node->outgoing()) {
      if (edge.adj == nullptr) {
        continue;
      }
      const auto to_it = node_ids.find(edge.adj.get());
      if (to_it == node_ids.end()) {
        continue;
      }
      connections.push_back({{kJsonKeyFrom, from_it->second},
                             {kJsonKeyOut, static_cast<int>(edge.port_out)},
                             {kJsonKeyTo, to_it->second},
                             {kJsonKeyIn, static_cast<int>(edge.port_in)}});
    }
  }
  graph_json[kJsonKeyConnections] = std::move(connections);
  return graph_json;
}

void ApplyPropertyValue(Node::Property& property, const PropertyValue& value,
                        const std::filesystem::path& root) {
  if (std::holds_alternative<std::string>(value)) {
    property.value = ProjectIo::fromProjectRelative(
        root, std::get<std::string>(value));
    return;
  }
  property.value = value;
}

void LoadNodeFields(Node& node, const json& node_json,
                    const std::filesystem::path& root,
                    const NodePoseSetter& pose_setter) {
  // identifier 由载入流程在 addNode 后应用，这里不再直接写 name。

  if (node_json.contains(kJsonKeyProperties) &&
      node_json.at(kJsonKeyProperties).is_array()) {
    auto native = node.getProperties();
    for (const json& property_json : node_json.at(kJsonKeyProperties)) {
      if (!property_json.contains(kJsonKeyName)) {
        continue;
      }
      const std::string name = property_json.at(kJsonKeyName).get<std::string>();
      PropertyValue value;
      if (!PropertyValueFromJson(property_json, value)) {
        continue;
      }
      for (auto& property : native) {
        if (property != nullptr && property->name == name) {
          ApplyPropertyValue(*property, value, root);
          break;
        }
      }
    }
  }

  if (node_json.contains(kJsonKeyInputs) &&
      node_json.at(kJsonKeyInputs).is_array()) {
    for (const json& input_json : node_json.at(kJsonKeyInputs)) {
      if (!input_json.contains(kJsonKeyIndex)) {
        continue;
      }
      const unsigned int index =
          static_cast<unsigned int>(input_json.at(kJsonKeyIndex).get<int>());
      if (input_json.contains(kJsonKeyPresentation)) {
        node.setInputPresentation(
            index, PresentationFromString(
                       input_json.at(kJsonKeyPresentation).get<std::string>()));
      }
      PropertyValue value;
      if (PropertyValueFromJson(input_json, value)) {
        if (auto property = node.inputProperty(index)) {
          ApplyPropertyValue(*property, value, root);
        }
      }
    }
  }

  if (pose_setter && node_json.contains(kJsonKeyPosition)) {
    const json& position = node_json.at(kJsonKeyPosition);
    NodePose pose;
    pose.x = position.value(kJsonKeyX, 0.0f);
    pose.y = position.value(kJsonKeyY, 0.0f);
    pose_setter(node, pose);
  }
}

}  // namespace

std::filesystem::path ProjectIo::graphFileRelativePath(std::size_t index) {
  std::ostringstream name;
  name << kProjectGraphFileNamePrefix << std::setw(kProjectGraphFileIndexWidth)
       << std::setfill('0') << index << kProjectGraphFileNameSuffix;
  return std::filesystem::path(kProjectGraphsDirectoryName) / name.str();
}

namespace {

bool IsManagedGraphFileName(const std::string& file_name) {
  const std::string prefix = kProjectGraphFileNamePrefix;
  const std::string suffix = kProjectGraphFileNameSuffix;
  if (file_name.size() <= prefix.size() + suffix.size()) {
    return false;
  }
  if (file_name.compare(0, prefix.size(), prefix) != 0) {
    return false;
  }
  if (file_name.compare(file_name.size() - suffix.size(), suffix.size(),
                        suffix) != 0) {
    return false;
  }
  return true;
}

/** \brief 删除 graphs/ 下未列入本次保存集合的 graph-*.json；失败只记日志。 */
void RemoveOrphanGraphFiles(
    const std::filesystem::path& root,
    const std::unordered_set<std::string>& kept_relative_paths) {
  const std::filesystem::path graphs_dir = root / kProjectGraphsDirectoryName;
  std::error_code error;
  if (!std::filesystem::is_directory(graphs_dir, error)) {
    return;
  }
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(graphs_dir, error)) {
    if (error) {
      logError("save project: cannot iterate graphs dir: {}", error.message());
      return;
    }
    if (!entry.is_regular_file(error)) {
      continue;
    }
    const std::string file_name = entry.path().filename().generic_string();
    if (!IsManagedGraphFileName(file_name)) {
      continue;
    }
    const std::string relative =
        (std::filesystem::path(kProjectGraphsDirectoryName) / file_name)
            .generic_string();
    if (kept_relative_paths.find(relative) != kept_relative_paths.end()) {
      continue;
    }
    std::filesystem::remove(entry.path(), error);
    if (error) {
      logError("save project: failed removing orphan {}: {}",
               entry.path().string(), error.message());
      error.clear();
    }
  }
}

}  // namespace

bool ProjectIo::createEmpty(const std::filesystem::path& root,
                            const std::string& project_name) {
  std::error_code error;
  std::filesystem::create_directories(root / kProjectGraphsDirectoryName, error);
  if (error) {
    logError("create project graphs dir failed: {}", error.message());
    return false;
  }
  std::filesystem::create_directories(root / kProjectAssetsDirectoryName, error);
  if (error) {
    logError("create project assets dir failed: {}", error.message());
    return false;
  }
  json manifest;
  manifest[kJsonKeyFileVersion] = kProjectFileVersion;
  manifest[kJsonKeyName] = project_name;
  manifest[kJsonKeyGraphs] = json::array();
  return WriteTextAtomic(root / kProjectManifestFileName,
                         manifest.dump(kJsonIndentSpaces));
}

bool ProjectIo::save(const Controller& controller,
                     const std::filesystem::path& root,
                     const std::string& project_name,
                     const NodePoseGetter& pose_getter) {
  std::error_code error;
  std::filesystem::create_directories(root / kProjectGraphsDirectoryName, error);
  if (error) {
    logError("save project: cannot create graphs dir: {}", error.message());
    return false;
  }
  std::filesystem::create_directories(root / kProjectAssetsDirectoryName, error);
  if (error) {
    logError("save project: cannot create assets dir: {}", error.message());
    return false;
  }

  json manifest;
  manifest[kJsonKeyFileVersion] = kProjectFileVersion;
  manifest[kJsonKeyName] = project_name;
  json graph_entries = json::array();
  std::unordered_set<std::string> kept_relative_paths;

  const auto& graphs = controller.getGraphs();
  for (std::size_t index = 0; index < graphs.size(); ++index) {
    const auto& graph = graphs[index];
    if (graph == nullptr) {
      continue;
    }
    const std::filesystem::path relative = graphFileRelativePath(index);
    const std::filesystem::path absolute = root / relative;
    const json graph_json = SerializeGraph(*graph, root, pose_getter);
    if (!WriteTextAtomic(absolute, graph_json.dump(kJsonIndentSpaces))) {
      logError("save project: failed writing {}", absolute.string());
      return false;
    }
    const std::string relative_key = relative.generic_string();
    kept_relative_paths.insert(relative_key);
    graph_entries.push_back({{kJsonKeyFile, relative_key}});
  }
  manifest[kJsonKeyGraphs] = std::move(graph_entries);
  if (!WriteTextAtomic(root / kProjectManifestFileName,
                       manifest.dump(kJsonIndentSpaces))) {
    logError("save project: failed writing manifest");
    return false;
  }
  RemoveOrphanGraphFiles(root, kept_relative_paths);
  return true;
}

bool ProjectIo::load(Controller& controller, const std::filesystem::path& root,
                     std::string& out_project_name,
                     const NodePoseSetter& pose_setter, bool* out_migrated) {
  const std::filesystem::path manifest_path = root / kProjectManifestFileName;
  std::ifstream input(manifest_path, std::ios::binary);
  if (!input) {
    logError("open project: missing {}", manifest_path.string());
    return false;
  }
  json manifest;
  try {
    input >> manifest;
  } catch (const json::exception& error) {
    logError("open project: invalid manifest: {}", error.what());
    return false;
  }

  const std::uint32_t version =
      manifest.value(kJsonKeyFileVersion, static_cast<std::uint32_t>(0));
  // 兼容 v1（name 等于 type、允许重名）与当前 v2。
  if (version != 1 && version != kProjectFileVersion) {
    logError("open project: unsupported fileVersion {}", version);
    return false;
  }
  // v1 的 name 全等于 kind，须在载入时重新生成唯一 identifier。
  bool migrated = (version == 1);
  out_project_name = manifest.value(kJsonKeyName, root.filename().string());

  std::vector<std::shared_ptr<Graph>> loaded_graphs;
  try {
    if (manifest.contains(kJsonKeyGraphs) &&
        manifest.at(kJsonKeyGraphs).is_array()) {
      for (const json& entry : manifest.at(kJsonKeyGraphs)) {
        const std::string relative = entry.at(kJsonKeyFile).get<std::string>();
        const std::filesystem::path graph_path = root / relative;
        std::ifstream graph_input(graph_path, std::ios::binary);
        if (!graph_input) {
          throw std::runtime_error("missing graph file: " + graph_path.string());
        }
        json graph_json;
        graph_input >> graph_json;
        auto graph = std::make_shared<Graph>(
            graph_json.value(kJsonKeyName, std::string("Graph")));

        std::vector<std::shared_ptr<Node>> loaded_nodes;
        if (graph_json.contains(kJsonKeyNodes) &&
            graph_json.at(kJsonKeyNodes).is_array()) {
          for (const json& node_json : graph_json.at(kJsonKeyNodes)) {
            const std::string type_id =
                node_json.at(kJsonKeyType).get<std::string>();
            auto node = controller.createNode(type_id);
            if (node == nullptr) {
              throw std::runtime_error("unknown node type: " + type_id);
            }
            // 先入图再填属性，保证端口与绑定属性已生成。
            graph->addNode(node);
            LoadNodeFields(*node, node_json, root, pose_setter);
            // v2 保留存档 identifier；v1 的名称等于 kind，交给 addNode 去重。
            if (version != 1) {
              const std::string saved_name =
                  node_json.value(kJsonKeyName, std::string());
              // 冲突/非法名保留自动名并记日志，不阻断打开。
              if (saved_name.empty() ||
                  !graph->renameNode(node, saved_name)) {
                migrated = true;
                logError(
                    "open project: duplicate or empty node name '{}' in graph "
                    "'{}'; kept '{}'",
                    saved_name, graph->getName(), node->getName());
              }
            }
            loaded_nodes.push_back(node);
          }
        }

        if (graph_json.contains(kJsonKeyConnections) &&
            graph_json.at(kJsonKeyConnections).is_array()) {
          for (const json& connection : graph_json.at(kJsonKeyConnections)) {
            const int from = connection.at(kJsonKeyFrom).get<int>();
            const int to = connection.at(kJsonKeyTo).get<int>();
            const unsigned int port_out = static_cast<unsigned int>(
                connection.at(kJsonKeyOut).get<int>());
            const unsigned int port_in = static_cast<unsigned int>(
                connection.at(kJsonKeyIn).get<int>());
            if (from < 0 || to < 0 ||
                static_cast<std::size_t>(from) >= loaded_nodes.size() ||
                static_cast<std::size_t>(to) >= loaded_nodes.size()) {
              throw std::runtime_error("connection node id out of range");
            }
            const std::shared_ptr<Node>& upstream =
                loaded_nodes[static_cast<std::size_t>(from)];
            const std::shared_ptr<Node>& downstream =
                loaded_nodes[static_cast<std::size_t>(to)];
            // 类型、露面方式或重复边被 connect 拒绝时丢弃该边并标脏。
            if (!graph->connect(upstream, port_out, downstream, port_in)) {
              migrated = true;
              const Node::Port* output = upstream->outputPort(port_out);
              const Node::Port* input = downstream->inputPort(port_in);
              logError(
                  "open project: dropped connection '{}':{}('{}') -> "
                  "'{}':{}('{}') in graph '{}'",
                  upstream->getName(), port_out,
                  output == nullptr ? std::string() : output->data_type.id,
                  downstream->getName(), port_in,
                  input == nullptr ? std::string() : input->data_type.id,
                  graph->getName());
            }
          }
        }
        loaded_graphs.push_back(std::move(graph));
      }
    }
  } catch (const std::exception& error) {
    logError("open project failed: {}", error.what());
    return false;
  }

  // 全部解析成功后再替换当前工程，避免失败时丢图。
  controller.clearGraphs();
  for (auto& graph : loaded_graphs) {
    controller.addGraph(std::move(graph));
  }
  if (out_migrated != nullptr) {
    *out_migrated = migrated;
  }
  return true;
}

std::string ProjectIo::toProjectRelative(const std::filesystem::path& root,
                                         const std::string& value) {
  if (value.empty() || root.empty()) {
    return value;
  }
  std::error_code error;
  const std::filesystem::path absolute(value);
  if (!absolute.is_absolute()) {
    return std::filesystem::path(value).generic_string();
  }
  const std::filesystem::path canonical_root =
      std::filesystem::weakly_canonical(root, error);
  if (error) {
    return value;
  }
  const std::filesystem::path canonical_value =
      std::filesystem::weakly_canonical(absolute, error);
  if (error) {
    return value;
  }
  const std::filesystem::path relative =
      std::filesystem::relative(canonical_value, canonical_root, error);
  if (error || relative.empty()) {
    return absolute.generic_string();
  }
  for (const std::filesystem::path& part : relative) {
    // 不在工程根下（含 ..）时保持绝对路径。
    if (part == "..") {
      return absolute.generic_string();
    }
  }
  return relative.generic_string();
}

std::string ProjectIo::fromProjectRelative(const std::filesystem::path& root,
                                           const std::string& value) {
  if (value.empty()) {
    return value;
  }
  const std::filesystem::path path(value);
  if (path.is_absolute() || root.empty()) {
    return path.generic_string();
  }
  return (root / path).lexically_normal().generic_string();
}

}  // namespace tg
