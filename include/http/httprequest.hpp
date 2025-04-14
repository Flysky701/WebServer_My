#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <random>

#include "log.h"

class HttpRequest
{
public:
    enum ParseState
    {
        PARSE_LINE,     // 解析请求行
        PARSE_HEADERS,  // 解析头部
        PARSE_BODY,     // 解析正文
        PARSE_COMPLETE, // 解析完成
        PARSE_ERROR     // 解析错误
    };
    bool parse(const char *data, size_t len);
    bool parse(const std::vector<char> &data) { return parse(data.data(), data.size()); }
    bool keep_alive() const;

    const std::string &method() const { return method_; }
    const std::string &path() const { return path_; }
    const std::string &version() const { return version_; }
    const std::string &body() const { return body_; }
    const ParseState &state() const { return state_; }


    struct FileInfo
    {
        std::string mime_type;
        std::string file_extension;
    };
    struct UploadFile{
        std::string filename;
        std::string temp_path;
        size_t size;
    };

    std::string get_token() const;
    static std::string get_file_extension(const std::string &path);
    static std::string get_mime_type(const std::string &path);

    FileInfo get_file_info() const
    {
        FileInfo result = {get_mime_type(path_), get_file_extension(path_)};
        return result;
    }

    const std::unordered_map<std::string, std::string> &query_params() const { return query_params_; };
    const std::unordered_map<std::string, std::string> &form_params() const { return form_params_; };
    const std::unordered_map<std::string, UploadFile> &uploaded_files() const { return uploaded_files_; };

    void set_context(const std::string& key, const std::string& value){
        context_[key] = value;
    }
    std::string get_context(const std::string &key) const{
        auto it = context_.find(key);
        return it != context_.end() ? it->second : "";
    }

private:
    // state
    ParseState state_ = PARSE_LINE;

    std::string method_;
    std::string path_;
    std::string version_;
    std::string body_;

    std::string current_field_name_;
    std::string current_filename_;

    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> context_;
    std::unordered_map<std::string, UploadFile> uploaded_files_;
    static const std::unordered_map<std::string, std::string> MIME_TYPES;
    
    std::unordered_map<std::string, std::string> query_params_;
    std::unordered_map<std::string, std::string> form_params_;

    bool parse_request_line(std::string_view line);
    void parse_headers(std::string_view line);
    void parse_body(std::string_view line);
    void parse_multipart_form(const std::string &boundary);
    void process_part_headers(std::string_view headers);
    void handle_part_content(size_t start, size_t length);

    static std::string url_decode(std::string_view str);

    void parse_key_value(std::string_view &str, std::unordered_map<std::string, std::string> &param_)
    {
        size_t param_start = 0;
        while (param_start < str.size())
        {
            size_t param_end = str.find('&', param_start);
            if (param_end == std::string_view::npos)
                param_end = str.size();
            std::string_view param = str.substr(param_start, param_end - param_start);
            size_t eq_pos = param.find('=');

            if (eq_pos != std::string_view::npos)
            {
                std::string key = url_decode(param.substr(0, eq_pos));
                std::string value = url_decode(param.substr(eq_pos + 1));
                param_.emplace(key, value);
            }
            param_start = param_end + 1;
        }
    }

    std::string generate_temp_path()const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::string chars =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789";
        static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

        std::string filename(16, '\0');
        for (auto &c : filename) c = chars[dis(gen)];
        return "/tmp/upload_" + filename;
    }
};

using std::string;
using std::string_view;
using std::vector;

const std::unordered_map<std::string, std::string> HttpRequest::MIME_TYPES = {
    {"html", "text/html"},
    {"css", "text/css"},
    {"js", "application/javascript"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"gif", "image/gif"},
    {"mp4", "video/mp4"},
    {"webm", "video/webm"},
    {"pdf", "application/pdf"}};

string HttpRequest::get_file_extension(const string &path)
{
    size_t dot_pos = path.find('.');
    string file_extension = "";
    if (dot_pos != string::npos)
        file_extension = path.substr(dot_pos + 1);
    return file_extension;
}

string HttpRequest::get_mime_type(const string &path)
{
    string extension = get_file_extension(path);
    auto it = MIME_TYPES.find(extension);
    if (it != MIME_TYPES.end())
        return it->second;
    return "application/octet-stream";
}

string HttpRequest::get_token() const{
    // Auth头
    auto auth_it = headers_.find("authorization");
    if (auth_it != headers_.end())
    {
        std::string auth_headers = auth_it->second;
        if (auth_headers.find("bearer ") == 0)
        {
            LOG_INFO("展示解析token {}", auth_headers.substr(7));
            return auth_headers.substr(7);
        }
    }

    // cookie
    auto cookie_it = headers_.find("cookie");
    if (cookie_it != headers_.end())
    {
        std::string cookies = cookie_it->second;
        size_t token_start = cookies.find("token=");
        if (token_start != std::string::npos)
        {
            token_start += 6;
            size_t token_end = cookies.find(';', token_start);
            if (token_end == std::string::npos)
                token_end = cookies.length();
            return cookies.substr(token_start, token_end - token_start);
        }
    }
    return "";
}

bool HttpRequest::parse(const char *data, size_t len)
{
    std::string_view input(data, len);
    size_t pos = 0;

    // LOG_DEBUG("展示接受数据: \n {}", input);

    while (pos < input.size() && state_ != PARSE_ERROR && state_ != PARSE_COMPLETE){
        string_view line;
        if (state_ == PARSE_BODY){
            line = input.substr(pos);
            LOG_DEBUG("show:{}", line);
        }
        else{
            size_t line_end = input.find("\r\n", pos);
            if (line_end == string::npos)
                break;

            line = input.substr(pos, line_end - pos);
            pos = line_end + 2;
        }

        switch (state_)
        {
        case PARSE_LINE:
            if (parse_request_line(line) == false)
            {
                state_ = PARSE_ERROR;
                return false;
            }
            else
                state_ = PARSE_HEADERS;
            break;
        case PARSE_HEADERS:
            if (line.empty())
            {
                if (method_ == "POST" || method_ == "PUT"){
                    state_ = PARSE_BODY;
                }
                else
                    state_ = PARSE_COMPLETE;
            }
            else
                parse_headers(line);
            break;
        case PARSE_BODY:
        {
            size_t content_length = 0;
            content_length = std::stoul(headers_.at("content-length"));

            // 完整读取 body 数据
            size_t needed = content_length - body_.size();
            size_t take = std::min(needed, input.size() - pos);
            body_.append(input.substr(pos, take));
            pos += take;

            if (body_.size() >= content_length)
            {
                parse_body(body_);
                state_ = PARSE_COMPLETE;
            }
            break;
        }
        default:
            return false;
        }
        // LOG_DEBUG("state_为{}", state_);
    }
    // LOG_DEBUG("state_为{}", state_);

    return state_ == PARSE_COMPLETE;
}

bool HttpRequest::parse_request_line(string_view line)
{
    size_t method_end = line.find(' ');
    if (method_end == string_view::npos)
        return false;

    method_ = line.substr(0, method_end);

    size_t path_start = method_end + 1;
    size_t path_end = line.find(' ', path_start);
    if (path_end == string_view::npos)
        return false;

    string_view full_path = line.substr(path_start, path_end - path_start);
    size_t query_start = full_path.find('?');

    if (query_start != string_view::npos)
    {
        path_ = full_path.substr(0, query_start);
        string_view query_str = full_path.substr(query_start + 1);
        parse_key_value(query_str, query_params_);
    }
    else
    {
        path_ = full_path;
    }

    size_t version_start = path_end + 1;
    version_ = line.substr(version_start);

    return method_.size() && path_.size() && version_.size();
}

void HttpRequest::parse_headers(string_view line)
{
    // LOG_DEBUG("parse_headers函数");
    size_t colon = line.find(':');
    if (colon == string_view::npos)
        return;

    string key(line.substr(0, colon));
    transform(key.begin(), key.end(), key.begin(), ::tolower); // 转小写

    string_view value = line.substr(colon + 1);

    value.remove_prefix(std::min(value.find_first_not_of(" "), value.size()));
    value.remove_suffix(value.size() - std::min(value.find_last_not_of(" \r") + 1, value.size()));

    headers_[key] = value;
}

void HttpRequest::parse_body(string_view line)
{
    // LOG_DEBUG("parse_body函数");
    body_ = line;
    if (headers_.count("content-type")){
        auto &content_type = headers_.at("content-type");
        // 表单
        if (content_type.find("x-www-form-urlencoded") != string::npos){
            parse_key_value(line, form_params_);
        }
        // 文件
        if (content_type.find("multipart/form-data") != string::npos){
            size_t boundary_pos = content_type.find("boundary=");
            string boundary = content_type.substr(boundary_pos + 9);
            parse_multipart_form(boundary);
        }
    }
}

void HttpRequest::parse_multipart_form(const std::string &boundary)
{
    LOG_INFO("parse_multipart_form");
    const std::string delimiter = "--" + boundary;
    const std::string end_delimiter = delimiter + "--";
    size_t pos = 0;

    // 遍历所有数据块
    while (pos < body_.size())
    {
        // 查找分块起始位置
        size_t part_start = body_.find(delimiter, pos);
        if (part_start == std::string::npos)
            break;
        part_start += delimiter.size() + 2; // 跳过 \r\n

        // 查找分块头结束位置
        size_t header_end = body_.find("\r\n\r\n", part_start);
        if (header_end == std::string::npos)
            break;

        std::string_view headers(body_.data() + part_start, header_end - part_start);
        process_part_headers(headers);

        size_t content_start = header_end + 4; // 跳过 \r\n\r\n
        size_t content_end = body_.find("\r\n" + delimiter, content_start);

        // 处理内容部分
        if (content_end != std::string::npos)
        {
            handle_part_content(content_start, content_end - content_start);
            pos = content_end;
        }
        else
            pos = body_.size();
        
    }
}

void HttpRequest::process_part_headers(std::string_view headers)
{
    size_t name_pos = headers.find("name=\"");
    if (name_pos != std::string_view::npos)
    {
        name_pos += 6; // len("name=\") = 6;
        size_t name_end = headers.find('"', name_pos);
        current_field_name_ = headers.substr(name_pos, name_end - name_pos);
    }

    size_t filename_pos = headers.find("filename=\"");
    if (filename_pos != std::string_view::npos)
    {
        filename_pos += 10; // len("filename=") = 10
        size_t filename_end = headers.find('"', filename_pos);
        current_filename_ = headers.substr(filename_pos, filename_end - filename_pos);
    }
}

// 处理内容部分（支持大文件）
void HttpRequest::handle_part_content(size_t start, size_t length)
{
    // 如果是文件上传
    if (!current_filename_.empty()){
        
        // 生成安全临时文件名
        LOG_INFO("写入文件");
        std::string temp_path = generate_temp_path();

        // 流式写入文件
        std::ofstream outfile(temp_path, std::ios::binary);
        outfile.write(body_.data() + start, length);
        uploaded_files_[current_field_name_] = {
            .filename = current_filename_,
            .temp_path = temp_path,
            .size = length};
    }
    // 普通表单字段
    else{
        std::string_view content(body_.data() + start, length);
        form_params_[current_field_name_] = content;
    }

    // 重置临时变量
    current_field_name_.clear();
    current_filename_.clear();
}

string HttpRequest::url_decode(string_view str)
{
    string res;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '%' && i + 2 < str.size()){
            int hex_val;
            if (sscanf(str.substr(i + 1, 2).data(), "%02x", &hex_val) == 1){
                res += static_cast<char>(hex_val);
                i += 2;
            }
        }
        else if (str[i] == '+')
            res += ' ';
        else
            res += str[i];
    }
    return res;
}

bool HttpRequest::keep_alive() const
{
    auto it = headers_.find("connection");
    if (it != headers_.end())
    {
        return it->second == "keep-alive" || (version_ == "HTTP/1.1" && it->second != "close");
    }
    return version_ == "HTTP/1.1";
}
