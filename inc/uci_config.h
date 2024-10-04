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

#ifndef POED_UCI_CONFIG_H
#define POED_UCI_CONFIG_H

#include <map>
#include <vector>
#include <string>

using namespace std;

struct UciSection {
    map<string, string> options;
};

class UciConfig {
    string name;
    map<string, vector<UciSection>> sections;

    struct uci_context *ctx;
    struct uci_package *pkg;

public:
    explicit UciConfig(string confname);
    ~UciConfig() = default;

    bool import();
    const map<string, vector<UciSection>> & getSections() const;
    void dump();

    /* Method to validate the presence of a section and its options,
     * size_t occurrences == 0 means minimum 1 occurrence
     * */
    bool validateSection(const std::string& sectionName, size_t occurrences,
                         const std::vector<std::string>& requiredOptions) const;
    bool exists();
};

#endif //POED_UCI_CONFIG_H
