# 简易 C++ HTTP Server 项目（基于 cpp-httplib）

项目包含：
- 基于 `cpp-httplib` 的 HTTP server 示例
- 支持 GET/POST JSON 接口
- 异步处理示例（使用 `std::async`）
- 可选 MySQL (libmysqlclient) 与 hiredis (Redis) 支持（通过 CMake 检测）
- 配置文件加载（`config/config.json`）
- WebSocket 示例（注释在代码中，需使用支持 websocket 的 `httplib.h`）

快速开始：

1. 下载依赖头文件（脚本会把头文件放到 `external`）：

```bash
cd /Users/mac/cplusplusprj/httpserver
./scripts/fetch_deps.sh
```

2. 创建 build 目录并构建：

```bash
mkdir -p build && cd build
cmake ..
make -j
```

3. 运行服务（可选传入自定义配置文件路径）：

```bash
./httpserver config/config.json
```

端点示例：
- `GET /ping` -> 返回文本 `pong`
- `GET /config` -> 返回加载的 JSON 配置
- `POST /json` -> 接受 JSON 请求体，返回相同 JSON，并在后台异步处理
- `POST /echo` -> 回显原始 body

MySQL/Redis：
- 若系统安装了 `libmysqlclient`、`hiredis`，CMake 会自动检测并启用支持。
- 在代码中有简单的连接示例（参见 `src/main.cpp`）。

WebSocket：
- `cpp-httplib` 的 websocket 支持并非在所有 header 版本中都存在；若要启用，请确保下载的是带 websocket 支持的 `httplib.h`，然后解除 `src/main.cpp` 中的 websocket 示例注释并调整 API 细节（README 中提供了注释示例）。
