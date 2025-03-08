#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>

class HttpRequest
{

public:
    enum ParseState
    {
        PARSE_LINE,    // 解析请求行
        PARSE_HEADERS, // 解析头部
        PARSE_BODY,    // 解析正文（待扩展）
        PARSE_ERROR    // 错误状态
    };
    bool parse(const char *data, size_t len);
    bool parse(const std::vector<char> &data){return parse(data.data(), data.size());}
    bool keep_alive() const;

    const std::string &method() const { return method_; }
    const std::string &path() const { return path_; }
    const std::string &version() const { return version_; }
    const ParseState &state() const { return state_; }
    
    static std::vector<char> hex_decode(const std::vector<char> orgin)
    {
        std::vector<char> bin;
        for (size_t i = 0; i < orgin.size(); i += 2){
            std::string tmp = "" + orgin[i] + orgin[i + 1];
            bin.emplace_back(static_cast<char>(
                std::stoi(tmp, nullptr, 16)));
        }
        return bin;
    }
    
private:
    ParseState state_ = PARSE_LINE;
    std::string method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;

    bool parse_request_line(std::string_view line);
    void parse_headers(std::string_view line);

};

#endif