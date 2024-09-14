//                  *****  HTTPRequest Class  *****

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <map>
#include <string>
#include <utility>
#include <vector>

class HTTPRequest
{
private:
    std::vector<std::string>        headers_;           // Header lines
    std::size_t                     body_offset_;       // Body offset in input string
    std::size_t                     body_size_;         // Body size
    const char                      *body_;             // Pointer to body string

    bool get_post_urlencoded(std::multimap<std::string, std::string> &data) const;
    bool get_post_multipart(std::string &content_type, std::multimap<std::string, std::string> &data) const;

public:
    HTTPRequest() : body_offset_(0), body_size_(0), body_(nullptr) {}
    HTTPRequest(const std::string &rqst): body_offset_(0), body_size_(0), body_(nullptr) { parseRequest(rqst); }
    ~HTTPRequest() {}

    bool parseRequest(const std::string &rqst);

    std::string type() const;
    std::string url() const;
    std::string path() const;
    std::string query(const std::string &key) const;

    int headerIndex(const std::string &name, int from=0) const;
    std::pair<std::string, std::string> header(int index) const;
    std::string header(const std::string &name) const;

    const std::size_t bodyOffset() const { return body_offset_; }
    const std::size_t bodySize() const { return body_size_; }
    const char *body() const { return body_; }
    const std::size_t size() const { return body_offset_ + body_size_; }

    bool postData(std::multimap<std::string, std::string> &data) const;

    static std::string uri_decode(const std::string &uri);

    bool isComplete() const { return body_ != nullptr; }
    void clear() { headers_.clear(); body_offset_ = 0; body_size_ = 0; body_ = nullptr; }
};

#endif
