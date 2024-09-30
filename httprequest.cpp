//                  *****  HTTPRequest Implementation  *****

#include "httprequest.h"
#include "txt.h"

#include <string.h>

bool HTTPRequest::parseRequest(std::string &rqst, bool parsePostData)
{
    clear();
    if (rqst.find("\r\n\r\n") != std::string::npos)
    {
        std::size_t i1 = 0;
        std::size_t i2 = rqst.find("\r\n", i1);
        while (i2 != std::string::npos && i2 != i1)
        {
            headers_.push_back(rqst.substr(i1, i2 - i1));
            i1 = i2 + 2;
            i2 = rqst.find("\r\n", i1);
        }

        std::size_t cl = 0;
        int index = headerIndex("Content-Length");
        if (index > 0)
        {
            std::string valstr = header(index).second;
            try
            {
                cl = std::stoul(valstr);
            }
            catch(const std::exception& e)
            {
                printf("Bad Content-Length %s\n", valstr.c_str());
            }
        }
        body_offset_ = i2 + 2;
        body_size_ = rqst.size() - body_offset_;
        if (body_size_ >= cl)
        {
            body_size_ = cl;
            body_ = &rqst[body_offset_];
            if (type() == "POST" && parsePostData)
            {
                get_post();
            }
        }
    }
    return isComplete();
}

std::string HTTPRequest::type() const
{
    if (headers_.size() > 0)
    {
        std::vector<std::string> tokens;
        TXT::split(headers_.at(0), " ", tokens);
        if (tokens.size() == 3)
        {
            return tokens.at(0);
        }
    }
    return std::string();
}

std::string HTTPRequest::url() const
{
    if (headers_.size() > 0)
    {
        std::vector<std::string> tokens;
        TXT::split(headers_.at(0), " ", tokens);
        if (tokens.size() == 3)
        {
            return tokens.at(1);
        }
    }
    return std::string();
}

std::string HTTPRequest::path() const
{
    std::string ret = url();
    std::size_t i1 = ret.find('?');
    return ret.substr(0, i1);
}

std::string HTTPRequest::root() const
{
    std::string ret = path();
    std::size_t i1 = ret.find('.');
    return ret.substr(0, i1);
}

std::string HTTPRequest::filetype() const
{
    std::string ret("html");
    std::string pat = path();
    std::size_t i1 = pat.rfind('.');
    if (i1 != std::string::npos && i1 + 1 < pat.length())
    {
        ret = pat.substr(i1 + 1);
    }
    return ret;
}

std::string HTTPRequest::query(const std::string &key) const
{
    std::string ret;
    std::string qry = uri_decode(url());
    std::size_t i1 = qry.find('?');
    if (i1 + 1 > qry.length()) i1 = qry.length();
    qry.erase(0, i1 + 1);
    std::vector<std::string> tok;
    TXT::split(qry, "&", tok);
    for (auto it = tok.begin(); it != tok.end(); ++it)
    {
        i1 = it->find('=');
        if (i1 != std::string::npos && it->substr(0, i1) == key)
        {
            if (i1 + 1 < it->length())
            {
                ret = it->substr(i1 + 1);
            }
            break;
        }
    }
    return ret;
}

int HTTPRequest::headerIndex(const std::string &name, int from) const
{
    int ret = -1;
    for (int ii = from; ii < headers_.size(); ii++)
    {
        std::size_t i1 = headers_.at(ii).find(':');
        if (i1 != std::string::npos)
        {
            if (strcasecmp(headers_.at(ii).substr(0, i1).c_str(), name.c_str()) == 0)
            {
                ret = ii;
                break;
            }
        }
    }
    return ret;
}

std::pair<std::string, std::string> HTTPRequest::header(int index) const
{
    if (index > 0 && index < headers_.size())
    {
        std::size_t i1 = headers_.at(index).find(':');
        if (i1 != std::string::npos)
        {
            std::size_t i2 = headers_.at(index).find_first_not_of(' ', i1 + 1);
            std::size_t i3 = headers_.at(index).find_first_of("\r\n", i2);
            return std::pair<std::string, std::string>(headers_.at(index).substr(0, i1), headers_.at(index).substr(i2, i3 - i2));
        }
    }
    return std::pair<std::string, std::string>();
}

std::string HTTPRequest::header(const std::string &name) const
{
    int index = headerIndex(name);
    return header(index).second;
}

std::string HTTPRequest::cookie(const std::string &name, const std::string &defval) const
{
    std::string ret(defval);
    bool found = false;
    int index = headerIndex("Cookie");
    while (!found && index > 0)
    {
        std::pair<std::string, std::string> hdr = header(index);
        std::size_t i1 = 0;
        std::size_t i2 = hdr.second.find('=', i1);
        while (!found && i1 < hdr.second.length() && i2 != std::string::npos)
        {
            i2 += 1;
            std::size_t i3 = hdr.second.find("; ", i2);
            std::size_t ll = i3 != std::string::npos ? i3 - i2 : hdr.second.length() - i2;
            if (i2 - i1 - 1 == name.length() && strncasecmp(hdr.second.c_str() + i1, name.c_str(), i2 - i1 - 1) == 0)
            {
                ret = hdr.second.substr(i2, ll);
                found = true;
            }
            i1 = i2 + ll + 2;
            i2 = hdr.second.find('=', i1);
        }
        index = headerIndex("Cookie", index + 1);
    }

    return ret;
}

bool HTTPRequest::parsePost()
{
    if (post_data_.size() > 0)
    {
        return true;
    }
    return get_post();
}

bool HTTPRequest::get_post()
{
    bool ret = false;
    post_data_.clear();

    std::string content_type = header("Content-Type");
    if (content_type == "application/x-www-form-urlencoded")
    {
        ret = get_post_urlencoded();
    }
    else if (content_type.find("multipart/form-data") != std::string::npos)
    {
        ret = get_post_multipart(content_type);
    }
    return ret;
}

bool HTTPRequest::get_post_urlencoded()
{
    bool ret = true;
    std::string key;
    std::string value;
    char *valptr;
    char *ptr = body_;
    char *end = body_ + body_size_;
    while (ptr < end)
    {
        char *cp = ptr;
        while (cp < end && *cp != '=') cp++;
        if (cp < end)
        {
            key = uri_decode(std::string(ptr, cp - ptr));
            ptr = cp + 1;
            if (ptr < end)
            {
                cp = ptr;
                while (cp < end && *cp != '&') cp++;
                value = uri_decode(std::string(ptr, cp - ptr));
                memcpy(ptr, value.c_str(), value.length() + 1);
                valptr = ptr;
                ptr = cp + 1;
            }
            else
            {
                value.clear();
            }
            post_data_.insert(std::pair<std::string, const char *>(key, valptr));
        }
        else
        {
            ret = false;
        }
    }
    return ret;
}

bool HTTPRequest::get_post_multipart(std::string &content_type)
{
    bool ret = false;
    std::string boundary;
    std::size_t i1 = content_type.find("boundary=");
    std::size_t i2;
    if (i1 != std::string::npos)
    {
        i1 += 9;
        i2 = content_type.find_first_of("; \r\n", i1);
        boundary = content_type.substr(i1, i2 - i1);
    }

    if (!boundary.empty())
    {
        ret = true;
        boundary.insert(0, "--");
        char *ptr = body_;
        char *end = body_ + body_size_;
        char *cp;
        std::string key;
        std::string value;
        std::string line;
        while (ptr < end && !line.empty())
        {
            cp = ptr;
            while (cp < end && *cp != '\r') cp++;
            line = std::string(ptr, cp - ptr);
            ptr = cp + 1 < end ? cp + 2 : end;
        }
        cp = ptr;
        while (cp < end && *cp != '\r') cp++;
        line = std::string(ptr, cp - ptr);
        ptr = cp + 1 < end ? cp + 2 : end;

        while (ptr < end && line == boundary)
        {
            key.clear();
            cp = ptr;
            while (cp < end && *cp != '\r') cp++;
            line = std::string(ptr, cp - ptr);
            char *lp = ptr;
            ptr = cp + 1 < end ? cp + 2 : end;
            while (ptr < end && !line.empty())
            {
                if (strncasecmp(line.c_str(), "Content-Disposition:", 20) == 0)
                {
                    i1 = line.find("name=\"");
                    if (i1 != std::string::npos)
                    {
                        i1 += 6;
                        i2 = line.find('"', i1);
                        if (i2 != std::string::npos)
                        {
                            key = line.substr(i1, i2 - i1);
                        }
                    }
                    i1 = line.find("filename=\"");
                    if (i1 != std::string::npos)
                    {
                        i1 += 10;
                        i2 = line.find('"', i1);
                        if (i2 != std::string::npos)
                        {
                            lp[i2] = '\0';
                            post_data_.insert(std::pair<std::string, const char *>(key + ".filename", lp + i1));
                        }
                    }
                }
                cp = ptr;
                while (cp < end && *cp != '\r') cp++;
                line = std::string(ptr, cp - ptr);
                lp = ptr;
                ptr = cp + 1 < end ? cp + 2 : end;
            }

            char *value = ptr;
            while (ptr < end)
            {
                cp = ptr;
                while (cp < end && *cp != '\r' && *(cp + 1) == '\n') cp++;
                ptr = cp < end ? cp + 1 : end;
                if (strncmp(ptr, boundary.c_str(), boundary.length()) == 0)
                {
                    *(ptr - 2) = '\0';
                    post_data_.insert(std::pair<std::string, const char *>(key, value));
                    cp = ptr;
                    while (cp < end && *cp != '\r') cp++;
                    line = std::string(ptr, cp - ptr);
                    ptr = cp + 1 < end ? cp + 2 : end;
                    break;
                }
            }
        }
    }
    return ret;
}

const char *HTTPRequest::postValue(const std::string &key) const
{
    const char *ret = nullptr;
    auto it = post_data_.equal_range(key);
    if (it.first != it.second)
    {
        ret = it.first->second;
    }
    return ret;
}

int HTTPRequest::postArray(const std::string &key, std::vector<const char *> &array) const
{
    array.clear();
    auto range = post_data_.equal_range(key);
    for (auto it = range.first; it != range.second; ++it)
    {
        array.push_back(it->second);
    }
    return array.size();
}

void HTTPRequest::printPostData() const
{
    printf("Post data for %s:\n", url().c_str());
    for (auto it = post_data_.cbegin(); it != post_data_.cend(); ++it)
    {
        printf("  '%s' : '%s'\n", it->first.c_str(), it->second);
    }
}

std::string HTTPRequest::uri_decode(const std::string &uri)
{
    std::string ret;
    char ch;
    int i, ii;
    for (i=0; i<uri.length(); i++)
    {
        if (uri[i] == '%') {
            sscanf(uri. substr(i+1,2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        }
        else if (uri[i] == '+')
        {
            ret += ' ';
        }
        else
        {
            ret+=uri[i];
        }
    }
    return (ret);
}

void HTTPRequest::replaceHeader(std::string &rqst, const std::string &newHeader)
{
    std::size_t i1 = rqst.find("\r\n\r\n");
    if (i1 != std::string::npos)
    {
        std::size_t i2 = newHeader.find("\r\n\r\n");
        rqst.replace(0, i1, newHeader, 0, i2);
    }
}

void HTTPRequest::setHTMLLengthHeader(std::string &rqst)
{
    std::size_t i1 = rqst.find("\r\n\r\n");
    if (i1 != std::string::npos)
    {
        std::string newHeader("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ");
        int cl = rqst.length() - i1 - 4;
        newHeader += std::to_string(cl) + "\r\nConnection: keep-alive\r\nCache-Control: no-store\r\n\r\n";
        replaceHeader(rqst, newHeader);
    }
}
