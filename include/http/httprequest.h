#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>


class HttpRequest{

    public:
        enum ParseState{
            PARSE_LINE,    // 解析请求行
            PARSE_HEADERS, // 解析头部
            PARSE_BODY,    // 解析正文（后续扩展）
            PARSE_ERROR    // 错误状态
        };
        vector<char> change(const std::vector<char> *orgin);
        bool parse(std::vector<char> *data);
        bool keep_alive() const;

        const std::string &method() const { return method_; }
        const std::string &path() const { return path_; }
        const std::string &version() const { return version_; }
        const ParseState &state() const { return state_; }

    private:
        ParseState state_ = PARSE_LINE;
        std::string method_;
        std::string path_;
        std::string version_;
        std::unordered_map<std::string, std::string> headers_;
};

#endif