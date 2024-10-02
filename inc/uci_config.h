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
