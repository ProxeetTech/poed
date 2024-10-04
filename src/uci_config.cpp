/*
 * © 2024 Proxeet Limited
 *
 * The poed packet is licensed under the GNU Lesser General Public License version 3,
 * published by the Free Software Foundation.
 *
 * You may use, distribute, and modify this code under the terms of the LGPLv3.
 *
 * The project is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License version 3 for more details.
 *
 * Author: Aleksey Vasilenko (a.vasilenko@proxeet.com)
 */

#include "uci_config.h"
#include "utils.h"
#include "logs.h"
#include <iostream>

UciConfig::UciConfig(string confname) {
    this->name = std::move(confname);
    this->ctx = nullptr;
    this->pkg = nullptr;
}

bool UciConfig::import() {
    ctx = uci_alloc_context();
    if (!ctx) {
        syslog(LOG_ERR, "Failed to allocate UCI context");
        return false;
    }

    if (uci_load(ctx, "poed", &pkg) != UCI_OK) {
        uci_free_context(ctx);
        return false;
    }

    struct uci_element *e;
    uci_foreach_element(&pkg->sections, e) {
        struct uci_section *s = uci_to_section(e);
        const char* section_type = s->type;

        syslog(LOG_DEBUG, "Found section: type=%s", section_type);

        struct UciSection section;
        struct uci_element *option_element;
        uci_foreach_element(&s->options, option_element) {
            struct uci_option *option = uci_to_option(option_element);
            if (option->type == UCI_TYPE_STRING) {
                syslog(LOG_DEBUG, "  Option: %s = %s\n", option_element->name, option->v.string);
                section.options[option_element->name] = option->v.string;
            } else if (option->type == UCI_TYPE_LIST) {
                syslog(LOG_DEBUG, "  Option: %s = NON-STRING TYPE\n", option_element->name);
            }
        }
        sections[section_type].push_back(section);
        section.options.clear();
    }

    uci_unload(ctx, pkg);
    uci_free_context(ctx);

    return true;
}

const map<string, vector<UciSection>>& UciConfig::getSections() const {
    return sections;
}

void UciConfig::dump() {
    for (const auto& section: sections) {
        size_t sec_cnt = section.second.size();
        for (size_t i = 0; i < sec_cnt; i++) {
            cout << section.first << "[" << i << "]:\n";
            for (const auto& option: section.second.at(i).options) {
                cout << "    " << option.first << "=" << option.second << "\n";
            }
        }
    }
}

/* Method to validate the presence of a section and its options,
* size_t occurrences == 0 means minimum 1 occurrence
*/
bool UciConfig::validateSection(const std::string& sectionName, size_t occurrences,
                     const std::vector<std::string>& requiredOptions) const {
    auto it = sections.find(sectionName);

    /* Check if the section is present */
    if (it == sections.end()) {
        if (occurrences == 0) {
            syslog(LOG_ERR, "Section '%s' is required but not found", sectionName.c_str());
            return false;
        } else {
            return true; // If occurrences > 0, absence of this section is not an error
        }
    }

    /* Check the number of occurrences */
    size_t section_count = it->second.size();
    if (occurrences > 0 && section_count > occurrences) {
        syslog(LOG_ERR, "Section '%s' has too many occurrences. Expected at most %zu, found %zu",
               sectionName.c_str(), occurrences, section_count);
        return false;
    }

    /* Check required options */
    for (const auto& section : it->second) {
        for (const auto& option : requiredOptions) {
            auto option_iter = section.options.find(option);
            if (option_iter == section.options.end() || option_iter->second.empty()) {
                syslog(LOG_ERR, "Missing or empty option '%s' in section '%s'",
                       option.c_str(), sectionName.c_str());
                return false;
            }
        }
    }

    return true;
}

bool UciConfig::exists() {
    struct uci_context *local_ctx = uci_alloc_context();
    if (!local_ctx) {
        syslog(LOG_ERR, "Failed to allocate UCI context\n");
        return false;
    }

    struct uci_package *local_pkg = nullptr;
    if (uci_load(local_ctx, name.c_str(), &local_pkg) != UCI_OK) {
        uci_free_context(local_ctx);
        return false;
    }

    uci_unload(local_ctx, local_pkg);
    uci_free_context(local_ctx);
    return true;
}