#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "Node.hpp"

namespace tg {

/** \brief 工程目录布局与 JSON schema 常量（对照 Houdini $HIP / VS Code 工作区：目录为根）。 */
inline constexpr std::uint32_t kProjectFileVersion = 2;
inline constexpr const char* kProjectManifestFileName = "project.json";
inline constexpr const char* kProjectGraphsDirectoryName = "graphs";
inline constexpr const char* kProjectAssetsDirectoryName = "assets";
inline constexpr const char* kProjectGraphFileNamePrefix = "graph-";
inline constexpr const char* kProjectGraphFileNameSuffix = ".json";
inline constexpr int kProjectGraphFileIndexWidth = 3;

/** \brief 画布节点位置；core 不依赖 ImGui，由 UI 读写。 */
struct NodePose {
  float x = 0.0f;
  float y = 0.0f;
};

/** \brief 保存时按节点取画布坐标；未提供则不写 position。 */
using NodePoseGetter =
    std::function<bool(const Node& node, NodePose& out_pose)>;

/** \brief 加载后把坐标交还 UI。 */
using NodePoseSetter =
    std::function<void(Node& node, const NodePose& pose)>;

}  // namespace tg
