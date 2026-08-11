/**
 * @file service_i18n.c
 * @brief 轻量级国际化服务实现
 */

#include "service_i18n.h"
#include "service_i18n_generated.h"
#include "service_nvs.h"

#include <string.h>
#include <stdlib.h>

static i18n_language_t s_current_language = I18N_LANG_ZH_CN;

bool service_i18n_init(void)
{
    char saved[SERVICE_I18N_LANG_ID_MAX_LEN] = {0};

    if (service_nvs_get_language(saved, sizeof(saved)) == ESP_OK && saved[0] != '\0') {
        (void)service_i18n_set_language_by_id(saved);
    } else {
        s_current_language = I18N_LANG_ZH_CN;
    }

    return true;
}

bool service_i18n_set_language_by_id(const char *lang_id)
{
    if (lang_id == NULL) {
        return false;
    }

    for (int i = 0; i < I18N_LANG_COUNT; i++) {
        if (strcmp(g_i18n_language_ids[i], lang_id) == 0) {
            s_current_language = (i18n_language_t)i;
            return true;
        }
    }

    return false;
}

void service_i18n_set_language(i18n_language_t lang)
{
    if (lang >= 0 && lang < I18N_LANG_COUNT) {
        s_current_language = lang;
    }
}

i18n_language_t service_i18n_get_language(void)
{
    return s_current_language;
}

const char *service_i18n_get_language_id(void)
{
    return g_i18n_language_ids[s_current_language];
}

static int i18n_entry_cmp(const void *a, const void *b)
{
    const i18n_entry_t *ea = (const i18n_entry_t *)a;
    const i18n_entry_t *eb = (const i18n_entry_t *)b;
    return strcmp(ea->key, eb->key);
}

const char *service_i18n_translate(const char *key)
{
    if (key == NULL) {
        return "";
    }

    i18n_entry_t target = { .key = key };
    const i18n_entry_t *found = bsearch(
        &target,
        g_i18n_entries,
        g_i18n_entry_count,
        sizeof(i18n_entry_t),
        i18n_entry_cmp
    );

    if (found == NULL) {
        return key;
    }

    const char *tr = found->translations[s_current_language];
    return (tr != NULL && tr[0] != '\0') ? tr : key;
}
