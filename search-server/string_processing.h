#pragma once
#include <iostream>
#include <set>
#include "document.h"

using namespace std;

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;
    for (const std::string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

std::ostream& operator << (std::ostream& out, const Document search);
