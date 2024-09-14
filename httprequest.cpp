//                  *****  HTTPRequest Implementation  *****

#include "httprequest.h"
#include "txt.h"

#include <string.h>

bool HTTPRequest::parseRequest(const std::string &rqst)
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
            cl = std::stoi(valstr);
        }
        body_offset_ = i2 + 2;
        body_size_ = rqst.size() - body_offset_;
        if (body_size_ >= cl)
        {
            body_size_ = cl;
            body_ = rqst.c_str() + body_offset_;
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

std::string HTTPRequest::query(const std::string &key) const
{
    return std::string();
}

int HTTPRequest::headerIndex(const std::string &name, int from) const
{
    int ret = -1;
    for (int ii = 1; ii < headers_.size(); ii++)
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

bool HTTPRequest::postData(std::multimap<std::string, std::string> &data) const
{
    bool ret = false;
    data.clear();

    std::string content_type = header("Content-Type");
    if (content_type == "application/x-www-form-urlencoded")
    {
        ret = get_post_urlencoded(data);
    }
    else if (content_type.find("multipart/form-data") != std::string::npos)
    {
        ret = get_post_multipart(content_type, data);
    }
    return ret;
}

bool HTTPRequest::get_post_urlencoded(std::multimap<std::string, std::string> &data) const
{
    bool ret = true;
    std::string key;
    std::string value;
    const char *ptr = body_;
    const char *end = body_ + body_size_;
    while (ptr < end)
    {
        const char *cp = ptr;
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
                ptr = cp + 1;
            }
            else
            {
                value.clear();
            }
            data.insert(std::pair<std::string, std::string>(key, value));
        }
        else
        {
            ret = false;
        }
    }
    return ret;
}

bool HTTPRequest::get_post_multipart(std::string &content_type, std::multimap<std::string, std::string> &data) const
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
        const char *ptr = body_;
        const char *end = body_ + body_size_;
        const char *cp;
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
                            std::string filename = line.substr(i1, i2 - i1);
                            data.insert(std::pair<std::string, std::string>(key + ".filename", filename));
                        }
                    }
                }
                cp = ptr;
                while (cp < end && *cp != '\r') cp++;
                line = std::string(ptr, cp - ptr);
                ptr = cp + 1 < end ? cp + 2 : end;
            }

            value.clear();
            while (ptr < end)
            {
                cp = ptr;
                while (cp < end && *cp != '\r' && *(cp + 1) == '\n') cp++;
                value += std::string(ptr, cp - ptr + 1);
                ptr = cp < end ? cp + 1 : end;
                if (strncmp(ptr, boundary.c_str(), boundary.length()) == 0)
                {
                    value.pop_back();
                    value.pop_back();
                    data.insert(std::pair<std::string, std::string>(key, value));
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

std::string HTTPRequest::uri_decode(const std::string &uri)
{
    std::string ret;
    char ch;
    int i, ii;
    for (i=0; i<uri.length(); i++)
    {
        if (uri[i]=='%') {
            sscanf(uri.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
            ret+=ch;
            i=i+2;
        } else {
            ret+=uri[i];
        }
    }
    return (ret);
}