#pragma once

#include <string>

class HttpClient {
public:
    std::string get(
        const std::string& host,
        const std::string& target,
        const std::string& port = "443"
    ) const;
};