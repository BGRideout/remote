//                  *****  HTTPRequest Class  *****

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <string>
#include <utility>
#include <vector>

class HTTPRequest
{
private:
    std::vector<std::string>        headers_;           // Header lines
    std::string                     body_;              // Body text

public:
    HTTPRequest() {}
    HTTPRequest(const std::string &rqst) { parseRequest(rqst); }
    ~HTTPRequest() {}

    void parseRequest(const std::string &rqst);

    std::string type() const;
    std::string url() const;
    std::string path() const;
    int headerIndex(const std::string &name, int from=0) const;
    std::pair<std::string, std::string> header(int index) const;
    std::string header(const std::string &name) const;
};

#endif
