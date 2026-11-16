#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Controller.hpp"
#include "ProjectFormat.hpp"

namespace tg {

/** \brief 工程目录 Facade：读写 project.json 与 graphs/，路径相对工程根。 */
class ProjectIo {
 public:
  /** \brief 在 root 下建空工程目录（manifest、graphs/、assets/）。 */
  static bool createEmpty(const std::filesystem::path& root,
                          const std::string& project_name);

  /** \brief 把 controller 中的图写入工程目录（原子写临时文件再替换）。 */
  static bool save(const Controller& controller,
                   const std::filesystem::path& root,
                   const std::string& project_name,
                   const NodePoseGetter& pose_getter = {});

  /** \brief 从工程目录加载图到 controller（先清空已有图）。
   *
   *  兼容 v1（节点 name 等于 type、允许重名）与当前 v2。
   *
   *  \param[out] out_migrated 本次载入做了 v1→v2 迁移、名称去重，或丢弃了非法/重复边时写 true；可为空。
   */
  static bool load(Controller& controller, const std::filesystem::path& root,
                   std::string& out_project_name,
                   const NodePoseSetter& pose_setter = {},
                   bool* out_migrated = nullptr);

  /** \brief 绝对路径若在工程根下则改为相对正斜杠路径，否则原样返回。 */
  static std::string toProjectRelative(const std::filesystem::path& root,
                                       const std::string& value);

  /** \brief 相对路径接到工程根；已是绝对路径则原样返回。 */
  static std::string fromProjectRelative(const std::filesystem::path& root,
                                         const std::string& value);

  /** \brief 生成 graphs/graph-NNN.json 相对路径。 */
  static std::filesystem::path graphFileRelativePath(std::size_t index);
};

}  // namespace tg
