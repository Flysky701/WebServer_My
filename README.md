# Fileserver 项目
**开发笔记** | 最后更新：2024.3.25

## 待完成
### 用户登录模块
+ 实现登录状态
+ 实现dashboard

### 文件服务器模块
+ 实现零拷贝传输（sendfile）
+ 实现上传下载

### 性能优化
+ 实现内存池
+ 压测工具对比（wrk/ab）

## 已完成
### 基础架构
+ epoll ET边缘触发实现
+ Reactor事件处理模型
+ 非阻塞socket配置

### 数据库模块
+ SQL查询用户验证
+ SQL 文件 CRUD
+ 连接池设计（最大连接数限制）

### 线程管理
+ 无锁线程池
+ 异步日志系统
+ 静态资源管理器
