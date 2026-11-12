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

  /** \brief 从工程目录加载图到 controller（先清空已有图）。 */
  static bool load(Controller& controller, const std::filesystem::path& root,
                   std::string& out_project_name,
                   const NodePoseSetter& pose_setter = {});

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
