/**
 * @file service_i18n.h
 * @brief 轻量级国际化服务
 *
 * 为 C 代码与 EEZ 生成的 translated-literal 文本提供统一的翻译接口。
 * 翻译数据由 translations.tsv 经 tools/gen_i18n.py 生成静态 C 数组。
 */

#ifndef SERVICE_I18N_H
#define SERVICE_I18N_H

#include <stdbool.h>
#include <stddef.h>
#include "service_i18n_generated_enum.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 语言 ID 最大长度（含结尾 '\0'） */
#define SERVICE_I18N_LANG_ID_MAX_LEN 8

/**
 * @brief 初始化国际化服务
 *
 * 从 NVS 恢复上次保存的语言；未配置时默认中文。
 *
 * @return true 成功
 */
bool service_i18n_init(void);

/**
 * @brief 按语言 ID 切换语言
 *
 * @param[in] lang_id 语言 ID，如 "zh-CN" 或 "en"
 * @return true 成功
 */
bool service_i18n_set_language_by_id(const char *lang_id);

/**
 * @brief 按枚举值切换语言
 *
 * @param[in] lang 语言枚举值
 */
void service_i18n_set_language(i18n_language_t lang);

/**
 * @brief 获取当前语言枚举值
 */
i18n_language_t service_i18n_get_language(void);

/**
 * @brief 获取当前语言 ID 字符串
 */
const char *service_i18n_get_language_id(void);

/**
 * @brief 翻译指定 key
 *
 * @param[in] key 翻译 key；未找到时回退返回 key 本身
 * @return 当前语言对应的译文
 */
const char *service_i18n_translate(const char *key);

/* EEZ Studio translated-literal 生成代码使用 gettext 风格的 _(...) 宏 */
#define _(s) service_i18n_translate(s)

/* 标记需要提取但此处不翻译的字符串，例如含格式占位符的模板 */
#define N_(s) (s)

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_I18N_H */
