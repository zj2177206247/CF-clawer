# Codeforces 用户比赛分析器

基于 C 语言的 Codeforces 用户数据分析工具。通过 Codeforces API 拉取用户比赛记录与提交数据，生成包含等级分变化直方图和比赛明细的可视化 HTML 报告。

## 功能特性

- 批量查询多名 Codeforces 用户数据
- 用户总览表：等级分、头衔、比赛次数等汇总
- 每位用户详情页：
  - 等级分变化直方图（全部 / 近一年 / 近180天 / 近30天）
  - 完整比赛记录表（赛前分 → 赛后分、排名、通过/补做题目）
- 使用 ECharts 渲染交互式图表

## 项目结构

```
Codeforces_Clawer/
├── src/                          # 源代码
│   ├── main.c                    # 主程序
│   ├── cJSON.c                   # JSON 解析库
│   └── cJSON.h                   # JSON 解析库头文件
├── CMakeLists.txt                # CMake 构建配置
├── dist/                         # 可执行文件分发目录（不上传 GitHub）
├── .gitignore
└── README.md
```

## 依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| [libcurl](https://curl.se/download.html) | 8.20.0 | HTTP 请求 |
| [cJSON](https://github.com/DaveGamble/cJSON) | 1.7.x | JSON 解析 |

> cJSON 源码已包含在 `src/` 中，无需额外下载。

## 编译

### 1. 下载 curl 开发包

从 [curl 官方下载页](https://curl.se/windows/) 下载 **curl for Windows (MinGW64)**，解压到 `third_party/` 目录：

```
third_party/
└── curl-8.20.0_2-win64-mingw/
    ├── bin/
    ├── include/
    └── lib/
```

### 2. 构建

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## 使用

### 1. 准备用户列表

在可执行文件同目录下创建 `users.txt`，每行一个 Codeforces 用户名：

```
tourist
Petr
```

### 2. 运行

```bash
./CodeforcesViewer.exe
```

程序运行后在同目录生成 `result.html`，用浏览器打开即可。

## 注意事项

- 调用 Codeforces API 时每次请求间隔 2 秒，用户越多等待越长
- `result.html` 使用在线 ECharts 库，查看时需要联网
- 请合理使用，避免对 Codeforces API 造成压力

## 许可

MIT License
