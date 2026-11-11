#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace tg {

/** \brief 初始化系统日志。
 *
 *  滚动文件写到工作目录的 logs，并保留一份内存行供输出视图读取。
 *  重复调用只生效一次。
 */
void initLog();

/** \brief 写一条 info 日志。
 *
 *  尚未初始化时会先初始化。
 *
 *  \param[in] fmt 格式串。
 *  \param[in] args 格式参数。
 */
template <typename... Args>
void logInfo(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  initLog();
  spdlog::get("system")->info(fmt, std::forward<Args>(args)...);
}

/** \brief 写一条 error 日志。
 *
 *  尚未初始化时会先初始化。
 *
 *  \param[in] fmt 格式串。
 *  \param[in] args 格式参数。
 */
template <typename... Args>
void logError(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  initLog();
  spdlog::get("system")->error(fmt, std::forward<Args>(args)...);
}

/** \brief 把 cursor 之后的新日志行追加到 lines。
 *
 *  \param[in] cursor 上次读到的行数。
 *  \param[in,out] lines 界面上已有的行，新行追加在末尾。
 *  \return 当前内存日志的总行数，下次当作 cursor。
 */
std::size_t copyLogLinesSince(std::size_t cursor, std::vector<std::string>& lines);

}  // namespace tg
