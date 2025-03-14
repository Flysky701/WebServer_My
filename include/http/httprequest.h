#pragma once

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
            PARSE_BODY,    // 解析正文
            PARSE_COMPLETE,
            PARSE_ERROR    // 错误状态
        };
        bool parse(const char *data, size_t len);
        bool parse(const std::vector<char> &data){return parse(data.data(), data.size());}
        bool keep_alive() const;
    
        const std::string &method() const { return method_; }
        const std::string &path() const { return path_; }
        const std::string &version() const { return version_; }
        const ParseState &state() const { return state_; }


        struct FileInfo{
            std::string mime_type;
            std::string file_extension;
        };

        FileInfo get_file_info()const{
            FileInfo result = {get_mime_type(path_), get_file_extension(path_)};
            return result;
        }

        // 动态路由
        // const std::unordered_map<std::string, std::string> &query_params() const { return query_params_; };
        // const std::unordered_map<std::string, std::string> &path_params() const { return path_params_; };
        // const std::unordered_map<std::string, std::string> &form_params() const { return form_params_; };

    private:
        // state
        ParseState state_ = PARSE_LINE;

        std::string method_;
        std::string path_;
        std::string version_;

        std::unordered_map<std::string, std::string> headers_;
        static const std::unordered_map<std::string, std::string> MIME_TYPES;

        bool parse_request_line(std::string_view line);
        void parse_headers(std::string_view line);
        static std::string get_file_extension(const std::string &path);
        static std::string get_mime_type(const std::string &path);

        // 动态路由，未完成
        // std::unordered_map<std::string, std::string> query_params_;
        // std::unordered_map<std::string, std::string> path_params_;
        // std::unordered_map<std::string, std::string> form_params_;
};