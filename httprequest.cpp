//                  *****  HTTPRequest Implementation  *****

#include "httprequest.h"
#include "txt.h"

#include <string.h>

void HTTPRequest::parseRequest(const std::string &rqst)
{
    headers_.clear();
    std::size_t i1 = 0;
    std::size_t i2 = rqst.find('\n', i1);
    while (i2 != std::string::npos && i2 != i1)
    {
        headers_.push_back(rqst.substr(i1, i2 - i1));
        i1 = i2 + 1;
        i2 = rqst.find('\n', i1);
    }
    body_ = rqst.substr(i2 + 1);
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
