#include <HttpHeader.hpp>
#include <StrUtils.hpp>
#include <iostream>
#include <json/json.hpp>

HttpHeader::HttpHeader(const char *read_buffer, const ssize_t &length) : __type(judge_request_type(read_buffer, length))
{
    std::vector<std::string> requestHeaders = StrUtils::split(read_buffer, "\n");
    if (requestHeaders.size() == 0)
    {
        return;
    }
    std::vector<std::string> requestLine = StrUtils::split(requestHeaders.at(0), " ");
    std::vector<std::string> requestHeaderLines;
    std::copy_if(requestHeaders.begin() + 1, requestHeaders.end(), std::back_inserter(requestHeaderLines), [](const std::string &item)
                 { return StrUtils::trim(item).length() >= 1; });
    // parse params
    if (requestLine.size() >= 2)
    {
        std::vector<std::string> requestContent = StrUtils::split(requestLine.at(1), "?");
        Path = requestContent.at(0);
        if (requestContent.size() >= 2)
        {
            for (std::string param : StrUtils::split(requestContent.at(1), "&"))
            {
                try
                {
                    std::vector p = StrUtils::split(param, "=");
                    Params[p[0]] = StrUtils::trim(p[1]);
                }
                catch (std::exception e)
                {
                    std::cerr << "[ERROR]: " << e.what() << std::endl;
                }
            }
        }
    }

    // parse headers
    for (std::string r_param : requestHeaderLines)
    {
        std::vector<std::string> p = StrUtils::split(r_param, ":");
        if (p.size() < 2)
        {
            continue;
        }
        if (p.at(0) == "Connection")
        {
            Connection = StrUtils::trim(p.at(1));
        }
        else if (p.at(0) == "Host")
        {
            Host = StrUtils::trim(p.at(1));
        }
        else if (p.at(0) == "Upgrade")
        {
            Upgrade = StrUtils::trim(p.at(1));
        }
        else if (p.at(0) == "Sec-WebSocket-Key")
        {
            SecWebSocketKey = StrUtils::trim(p.at(1));
        }
    }
}

request_type HttpHeader::type(void) const
{
    if (__type == request_type::GET && Upgrade != "" && SecWebSocketKey != "")
    {
        return request_type::UPGRADE;
    }
    return __type;
}

request_type HttpHeader::judge_request_type(const char *read_buffer, const ssize_t &length)
{
    std::vector<std::string> requestHeaders = StrUtils::split(read_buffer, "\n");
    if (requestHeaders.size() == 0)
    {
        return request_type::UNKNOWN;
    }
    std::vector<std::string> requestLine = StrUtils::split(requestHeaders.at(0), " ");
    if (requestLine.at(0) == "GET")
    {
        return request_type::GET;
    }
    if (requestLine.at(0) == "POST")
    {
        return request_type::POST;
    }
    if (requestLine.at(0) == "PATCH")
    {
        return request_type::PATCH;
    }
    if (requestLine.at(0) == "PUT")
    {
        return request_type::PUT;
    }
    if (requestLine.at(0) == "DELETE")
    {
        return request_type::DELETE;
    }

    return request_type::UNKNOWN;
}

nlohmann::json &operator<<(nlohmann::json &out, const HttpHeader &header)
{
    if(header.type() == request_type::UNKNOWN) {
        out["type"] = "UNKNOWN";
    }
    else if(header.type() == request_type::GET) {
        out["type"] = "GET";
    }
    else if(header.type() == request_type::POST) {
        out["type"] = "POST";
    }
    else if(header.type() == request_type::PATCH) {
        out["type"] = "PATCH";
    }
    else if(header.type() == request_type::PUT) {
        out["type"] = "PUT";
    }
    else if(header.type() == request_type::DELETE) {
        out["type"] = "DELETE";
    }
    else if(header.type() == request_type::UPGRADE) {
        out["type"] = "UPGRADE";
    }

    out["Path"]= header.Path;
    out["Conneciton"] = header.Connection;
    out["Host"] = header.Host;
    out["Upgrade"] = header.Upgrade;
    out["Sec-Web-Socket-Key"] = header.SecWebSocketKey;
    return out;
}

std::ostream &operator<<(std::ostream &out, const HttpHeader &header)
{
    nlohmann::json __json;
    __json << header;
    out << __json.dump(4);
    return out;
}