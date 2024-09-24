//                  *****  HTTPRequest Class  *****

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <map>
#include <string>
#include <utility>
#include <vector>

class HTTPRequest
{
public:
    typedef std::multimap<std::string, const char *> PostData;

private:
    std::vector<std::string>        headers_;           // Header lines
    std::size_t                     body_offset_;       // Body offset in input string
    std::size_t                     body_size_;         // Body size
    char                            *body_;             // Pointer to body string
    PostData                        post_data_;         // POST data

    bool get_post();
    bool get_post_urlencoded();
    bool get_post_multipart(std::string &content_type);

public:
    HTTPRequest() : body_offset_(0), body_size_(0), body_(nullptr) {}
    HTTPRequest(std::string &rqst): body_offset_(0), body_size_(0), body_(nullptr) { parseRequest(rqst); }
    ~HTTPRequest() {}

    bool parseRequest(std::string &rqst, bool parsePostData=true);
    bool parsePost();

    std::string type() const;
    std::string url() const;
    std::string path() const;
    std::string root() const;
    std::string filetype() const;
    std::string query(const std::string &key) const;

    int headerIndex(const std::string &name, int from=0) const;
    std::pair<std::string, std::string> header(int index) const;
    std::string header(const std::string &name) const;

    std::string cookie(const std::string &name, const std::string &defval=std::string()) const;

    const std::size_t bodyOffset() const { return body_offset_; }
    const std::size_t bodySize() const { return body_size_; }
    const char *body() const { return body_; }
    const std::size_t size() const { return body_offset_ + body_size_; }

    const PostData &postData() const { return post_data_; }
    const char *postValue(const std::string &key) const;

    static std::string uri_decode(const std::string &uri);
    static void replaceHeader(std::string &rqst, const std::string &newHeader = std::string("HTTP/1.0 200 OK\r\nContent-Type: text/html"));
    static void setHTMLLengthHeader(std::string &rqst);

    bool isComplete() const { return body_ != nullptr; }
    void clear() { headers_.clear(); body_offset_ = 0; body_size_ = 0; body_ = nullptr; post_data_.clear(); }
};

#endif
