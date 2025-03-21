# Webserver 学习项目
**开发笔记** | 最后更新：2024.3

## 待完成
### 文件服务器模块
+ 实现零拷贝传输（sendfile/mmap）
+ 支持HTTP文件范围请求（Range header）
+ 文件缓存策略优化

### 数据库模块
+ SQL查询用户登录验证
+ 连接池设计（最大连接数限制）

### 性能优化
+ 实现内存池管理（固定块分配）
+ 压测工具对比（wrk/ab）

## 已完成
### 基础架构
+ epoll ET边缘触发实现
+ Reactor事件处理模型
+ 非阻塞socket配置

### 线程管理
+ 无锁线程池
+ 异步日志系统
+ 静态资源管理器
