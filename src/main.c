/*
 * Codeforces 用户分析器 - 输出到"result.html"
 * 依赖：libcurl, cJSON
 * 编译：gcc main.c cJSON.c -lcurl -o cf_analyzer
 * 运行：./cf_analyzer  (需要同目录下 users.txt)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>
#include "cJSON.h"

/* ======================== 等级分颜色与头衔 ======================== */
static const char* get_rating_color(int rating) {
    if (rating >= 3000) return "#FF0000";
    if (rating >= 2600) return "#FF0000";
    if (rating >= 2400) return "#FF0000";
    if (rating >= 2300) return "#FF8C00";
    if (rating >= 2100) return "#FF8C00";
    if (rating >= 1900) return "#AA00AA";
    if (rating >= 1600) return "#0000FF";
    if (rating >= 1400) return "#03A89E";
    if (rating >= 1200) return "#008000";
    if (rating >= 1)    return "#808080";
    return "#000000";
}

static const char* get_rank_name(int rating) {
    if (rating >= 3000) return "Legendary Grandmaster";
    if (rating >= 2600) return "International Grandmaster";
    if (rating >= 2400) return "Grandmaster";
    if (rating >= 2300) return "International Master";
    if (rating >= 2100) return "Master";
    if (rating >= 1900) return "Candidate Master";
    if (rating >= 1600) return "Expert";
    if (rating >= 1400) return "Specialist";
    if (rating >= 1200) return "Pupil";
    if (rating >= 1)    return "Newbie";
    return "Unrated";
}

/* ======================== HTTP 响应缓冲区 ======================== */
typedef struct {
    char *data;
    size_t size;
} ResponseBuffer;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    ResponseBuffer *buf = (ResponseBuffer *)userp;
    char *ptr = realloc(buf->data, buf->size + real_size + 1);
    if (!ptr) return 0;
    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), contents, real_size);
    buf->size += real_size;
    buf->data[buf->size] = '\0';
    return real_size;
}

/* ======================== Codeforces API 调用 ======================== */
#define API_BASE "https://codeforces.com/api/"

static int api_get(const char *url, ResponseBuffer *buf) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    buf->data = malloc(1);
    buf->size = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK ? 0 : -1;
}

static char* fetch_user_info(const char *handles) {
    char url[2048];
    snprintf(url, sizeof(url), "%suser.info?handles=%s", API_BASE, handles);
    ResponseBuffer buf;
    if (api_get(url, &buf) != 0) return NULL;
    return buf.data;
}

static char* fetch_user_rating(const char *handle) {
    char url[2048];
    snprintf(url, sizeof(url), "%suser.rating?handle=%s", API_BASE, handle);
    ResponseBuffer buf;
    if (api_get(url, &buf) != 0) return NULL;
    return buf.data;
}

static char* fetch_user_status(const char *handle, int from, int count) {
    char url[2048];
    snprintf(url, sizeof(url), "%suser.status?handle=%s&from=%d&count=%d",
             API_BASE, handle, from, count);
    ResponseBuffer buf;
    if (api_get(url, &buf) != 0) return NULL;
    return buf.data;
}

/* ======================== 数据结构 ======================== */
typedef struct {
    int contest_id;
    char name[256];
    time_t time;
    int rank;
    int old_rating, new_rating;
    char solved_problems[26][4];
    int solved_count;
    char upsolved_problems[26][4];
    int upsolved_count;
} ContestRecord;

typedef struct {
    char handle[64];
    int rating, max_rating;
    char rank[64];
    char avatar[256];
    ContestRecord *contests;
    int contest_count;
    int total_contests;
    int max_rating_ever;
    int recent_contests_180d;
    int max_rating_180d;
    int *solved_ratings;
    time_t *solved_times;
    int solved_count;
} User;

/* ======================== 辅助函数：按时间降序排序 ======================== */
static int cmp_contest_by_time_desc(const void *a, const void *b) {
    const ContestRecord *ca = (const ContestRecord *)a;
    const ContestRecord *cb = (const ContestRecord *)b;
    if (ca->time > cb->time) return -1;
    if (ca->time < cb->time) return 1;
    return 0;
}

/* ======================== JSON 解析 ======================== */
static User parse_user_info_json(const char *json) {
    User user;
    memset(&user, 0, sizeof(user));
    cJSON *root = cJSON_Parse(json);
    if (!root) return user;
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || strcmp(status->valuestring, "OK") != 0) {
        cJSON_Delete(root);
        return user;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsArray(result) || cJSON_GetArraySize(result) == 0) {
        cJSON_Delete(root);
        return user;
    }
    cJSON *user_obj = cJSON_GetArrayItem(result, 0);
    cJSON *handle = cJSON_GetObjectItem(user_obj, "handle");
    cJSON *rating = cJSON_GetObjectItem(user_obj, "rating");
    cJSON *max_rating = cJSON_GetObjectItem(user_obj, "maxRating");
    cJSON *rank = cJSON_GetObjectItem(user_obj, "rank");
    cJSON *avatar = cJSON_GetObjectItem(user_obj, "titlePhoto");

    if (handle) strncpy(user.handle, handle->valuestring, 63);
    user.rating = rating ? rating->valueint : 0;
    user.max_rating = max_rating ? max_rating->valueint : 0;
    if (rank) strncpy(user.rank, rank->valuestring, 63);
    else if (user.rating == 0) strcpy(user.rank, "Unrated");
    else strcpy(user.rank, get_rank_name(user.rating));
    if (avatar) strncpy(user.avatar, avatar->valuestring, 255);
    cJSON_Delete(root);
    return user;
}

static int parse_rating_json(const char *json, ContestRecord **contests_out, int *count_out) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || strcmp(status->valuestring, "OK") != 0) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    int n = cJSON_GetArraySize(result);
    ContestRecord *contests = malloc(sizeof(ContestRecord) * n);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(result, i);
        memset(&contests[i], 0, sizeof(ContestRecord));
        cJSON *cid = cJSON_GetObjectItem(item, "contestId");
        cJSON *cname = cJSON_GetObjectItem(item, "contestName");
        cJSON *rank = cJSON_GetObjectItem(item, "rank");
        cJSON *old_r = cJSON_GetObjectItem(item, "oldRating");
        cJSON *new_r = cJSON_GetObjectItem(item, "newRating");
        cJSON *utime = cJSON_GetObjectItem(item, "ratingUpdateTimeSeconds");
        if (cid) contests[i].contest_id = cid->valueint;
        if (cname) strncpy(contests[i].name, cname->valuestring, 255);
        if (rank) contests[i].rank = rank->valueint;
        if (old_r) contests[i].old_rating = old_r->valueint;
        if (new_r) contests[i].new_rating = new_r->valueint;
        if (utime) contests[i].time = (time_t)utime->valueint;
    }
    *contests_out = contests;
    *count_out = n;
    cJSON_Delete(root);
    return 0;
}

/* 存储一次 AC 提交的简要信息 */
typedef struct {
    int contest_id;
    char problem_index[4];
    int is_upsolved;
} AcSubmission;

static void parse_status_full(const char *json,
                              int **ratings, time_t **times, int *ac_count,
                              AcSubmission **ac_subs, int *sub_count) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsArray(result)) { cJSON_Delete(root); return; }

    int n = cJSON_GetArraySize(result);
    int cap = 100000;
    int *r = malloc(sizeof(int) * cap);
    time_t *t = malloc(sizeof(time_t) * cap);
    AcSubmission *subs = malloc(sizeof(AcSubmission) * cap);
    int cnt = 0, sub_cnt = 0;

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(result, i);
        cJSON *verdict = cJSON_GetObjectItem(item, "verdict");
        if (!verdict || strcmp(verdict->valuestring, "OK") != 0) continue;

        cJSON *problem = cJSON_GetObjectItem(item, "problem");
        if (!problem) continue;
        cJSON *rating = cJSON_GetObjectItem(problem, "rating");
        cJSON *creation = cJSON_GetObjectItem(item, "creationTimeSeconds");
        cJSON *cid = cJSON_GetObjectItem(problem, "contestId");
        cJSON *index = cJSON_GetObjectItem(problem, "index");
        cJSON *author = cJSON_GetObjectItem(item, "author");
        int is_upsolved = 0;
        if (author) {
            cJSON *ptype = cJSON_GetObjectItem(author, "participantType");
            if (ptype && strcmp(ptype->valuestring, "PRACTICE") == 0)
                is_upsolved = 1;
        }

        if (creation && cJSON_IsNumber(creation) && cnt < cap) {
            t[cnt] = (time_t)creation->valueint;
            r[cnt] = rating ? rating->valueint : 0;
            cnt++;
        }

        if (cid && index && cJSON_IsNumber(cid) && cJSON_IsString(index) && sub_cnt < cap) {
            subs[sub_cnt].contest_id = cid->valueint;
            strncpy(subs[sub_cnt].problem_index, index->valuestring, 3);
            subs[sub_cnt].problem_index[3] = '\0';
            subs[sub_cnt].is_upsolved = is_upsolved;
            sub_cnt++;
        }
    }

    *ratings = r;
    *times = t;
    *ac_count = cnt;
    *ac_subs = subs;
    *sub_count = sub_cnt;
    cJSON_Delete(root);
}

/* 计算近180天统计 */
static void calc_recent_180d(User *u) {
    time_t now = time(NULL);
    time_t cutoff = now - 180 * 24 * 3600;
    u->recent_contests_180d = 0;
    u->max_rating_180d = 0;
    for (int i = 0; i < u->contest_count; i++) {
        if (u->contests[i].time >= cutoff) {
            u->recent_contests_180d++;
            if (u->contests[i].new_rating > u->max_rating_180d)
                u->max_rating_180d = u->contests[i].new_rating;
        }
    }
}

/* ======================== HTML 输出 ======================== */
static void print_histogram_chart(User *u) {
    int bin_starts[50], bin_count = 0;
    for (int r = 800; r <= 3500; r += 100)
        bin_starts[bin_count++] = r;

    time_t now = time(NULL);
    time_t year_ago = now - 365*24*3600;
    time_t half_year = now - 180*24*3600;
    time_t month_ago = now - 30*24*3600;

    int all[50] = {0}, y1[50] = {0}, y180[50] = {0}, y30[50] = {0};
    for (int i = 0; i < u->solved_count; i++) {
        int rating = u->solved_ratings[i];
        time_t t = u->solved_times[i];
        int idx = (rating - 800) / 100;
        if (idx < 0 || idx >= bin_count) continue;
        all[idx]++;
        if (t >= year_ago) y1[idx]++;
        if (t >= half_year) y180[idx]++;
        if (t >= month_ago) y30[idx]++;
    }

    printf("<div id='histogram_%s' style='width:100%%; height:500px; margin:30px 0;'></div>\n", u->handle);
    printf("<script>\n");
    printf("(function() {\n");
    printf("  var chart = echarts.init(document.getElementById('histogram_%s'));\n", u->handle);
    printf("  var bins = [");
    for (int i = 0; i < bin_count; i++)
        printf("'%d-%d'%s", bin_starts[i], bin_starts[i]+99, i<bin_count-1?",":"");
    printf("];\n");
    printf("  var option = {\n");
    printf("    title: { text: '%s 通过题目等级分分布' },\n", u->handle);
    printf("    tooltip: {},\n");
    printf("    legend: { data: ['全部','近一年','近180天','近30天'] },\n");
    printf("    xAxis: { data: bins },\n");
    printf("    yAxis: { name: '数量' },\n");
    printf("    series: [\n");
    printf("      { name: '全部', type: 'bar', data: [");
    for (int i=0;i<bin_count;i++) printf("%d%s",all[i],i<bin_count-1?",":"");
    printf("] },\n");
    printf("      { name: '近一年', type: 'bar', data: [");
    for (int i=0;i<bin_count;i++) printf("%d%s",y1[i],i<bin_count-1?",":"");
    printf("] },\n");
    printf("      { name: '近180天', type: 'bar', data: [");
    for (int i=0;i<bin_count;i++) printf("%d%s",y180[i],i<bin_count-1?",":"");
    printf("] },\n");
    printf("      { name: '近30天', type: 'bar', data: [");
    for (int i=0;i<bin_count;i++) printf("%d%s",y30[i],i<bin_count-1?",":"");
    printf("] }\n");
    printf("    ]\n");
    printf("  };\n");
    printf("  chart.setOption(option);\n");
    printf("  window.addEventListener('resize', function() { chart.resize(); });\n");
    printf("})();\n");
    printf("</script>\n");
}

static void print_user_detail(User *u) {
    printf("<div id='user_%s' class='user-detail' style='display:none; margin-top:30px; padding:25px; background:#fff; border-radius:10px; box-shadow:0 4px 15px rgba(0,0,0,0.1);'>\n", u->handle);
    printf("<a href='javascript:void(0)' onclick='showList()' style='float:right; text-decoration:none; color:#3498db; font-weight:bold;'>&larr; 返回总览</a>\n");
    printf("<h2 style='color:%s; margin-top:0; font-size:1.8em;'>%s</h2>\n", get_rating_color(u->rating), u->handle);

    printf("<div style='display:flex; align-items:center; gap:20px; margin-bottom:25px; flex-wrap:wrap;'>\n");
    if (strlen(u->avatar) > 0)
        printf("<img src='%s' style='width:90px; height:90px; border-radius:50%%; border:2px solid %s;' alt='avatar'>\n", u->avatar, get_rating_color(u->rating));
    printf("<div style='flex:1; min-width:250px;'>\n");
    printf("<p style='margin:5px 0;'><b>等级分：</b><span style='color:%s; font-size:1.4em; font-weight:bold;'>%d</span> &nbsp; "
           "<b>头衔：</b>%s &nbsp; <b>最高等级分：</b>%d</p>\n",
           get_rating_color(u->rating), u->rating, u->rank, u->max_rating_ever);
    printf("<p style='margin:5px 0;'><b>比赛总次数：</b>%d &nbsp; <b>近180天比赛：</b>%d &nbsp; "
           "<b>近180天最高分：</b>%d</p>\n",
           u->contest_count, u->recent_contests_180d, u->max_rating_180d);
    printf("</div></div>\n");

    print_histogram_chart(u);

    printf("<h3 style='margin-top:20px;'>比赛记录（按时间由近到远）</h3>\n");
    printf("<div style='overflow-x:auto;'>\n");
    printf("<table style='width:100%%; border-collapse:collapse; min-width:800px;'>\n");
    printf("<tr style='background:#3498db; color:#fff;'>"
           "<th style='padding:12px 8px;'>赛事名称</th><th style='padding:12px 8px;'>时间</th>"
           "<th style='padding:12px 8px;'>赛前分</th><th style='padding:12px 8px;'>赛后分</th>"
           "<th style='padding:12px 8px;'>变化分</th>"          // 新增列标题
           "<th style='padding:12px 8px;'>排名</th><th style='padding:12px 8px;'>通过题目</th>"
           "<th style='padding:12px 8px;'>赛后补题</th></tr>\n");

    for (int i = 0; i < u->contest_count; i++) {
        ContestRecord *c = &u->contests[i];
        int delta = c->new_rating - c->old_rating;        // 计算变化分
        const char *delta_color = (delta > 0) ? "#27ae60" : ((delta < 0) ? "#e74c3c" : "#000");
        char delta_sign = (delta > 0) ? '+' : ' ';
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&c->time));

        printf("<tr style='border-bottom:1px solid #ddd; text-align:center;'>");
        printf("<td style='padding:10px 6px;'>%s</td>", c->name);
        printf("<td style='padding:10px 6px;'>%s</td>", time_str);
        printf("<td style='padding:10px 6px; color:%s;'>%d</td>", get_rating_color(c->old_rating), c->old_rating);
        printf("<td style='padding:10px 6px; color:%s;'>%d</td>", get_rating_color(c->new_rating), c->new_rating);
        // 变化分列，带符号和颜色
        printf("<td style='padding:10px 6px; color:%s; font-weight:bold;'>%c%d</td>",
               delta_color, delta_sign, delta);
        printf("<td style='padding:10px 6px;'>%d</td>", c->rank);
        printf("<td style='padding:10px 6px;'>");
        if (c->solved_count == 0) {
            printf("-");
        } else {
            for (int j = 0; j < c->solved_count; j++) {
                printf("%s%s", c->solved_problems[j], (j < c->solved_count - 1) ? ", " : "");
            }
        }
        printf("</td>");
        printf("<td style='padding:10px 6px;'>");
        if (c->upsolved_count == 0) {
            printf("-");
        } else {
            for (int j = 0; j < c->upsolved_count; j++) {
                printf("%s%s", c->upsolved_problems[j], (j < c->upsolved_count - 1) ? ", " : "");
            }
        }
        printf("</td>");
        printf("</tr>\n");
    }
    printf("</table>\n");
    printf("</div>\n");
    printf("</div>\n");
}

static void print_user_list(User *users, int count) {
    printf("<div id='list-container'>\n");
    printf("<h1 style='text-align:center; color:#2c3e50; margin-bottom:30px;'>Codeforces 用户比赛分析</h1>\n");
    printf("<div style='overflow-x:auto;'>\n");
    printf("<table style='border-collapse:collapse; width:100%%; min-width:800px; background:#fff; box-shadow:0 2px 10px rgba(0,0,0,0.1);'>\n");
    printf("<tr style='background:#2c3e50; color:#fff;'>"
           "<th style='padding:14px 10px;'>用户 ID</th><th style='padding:14px 10px;'>等级分</th>"
           "<th style='padding:14px 10px;'>头衔</th><th style='padding:14px 10px;'>比赛次数</th>"
           "<th style='padding:14px 10px;'>最高等级分</th><th style='padding:14px 10px;'>近180天比赛</th>"
           "<th style='padding:14px 10px;'>近180天最高分</th></tr>\n");
    for (int i = 0; i < count; i++) {
        const char *color = get_rating_color(users[i].rating);
        printf("<tr style='border-bottom:1px solid #ddd; text-align:center;'>");
        printf("<td style='padding:12px 8px;'><a href='javascript:void(0)' onclick='showUser(\"user_%s\")' style='text-decoration:none; font-weight:bold; color:%s;'>%s</a></td>",
               users[i].handle, color, users[i].handle);
        printf("<td style='padding:12px 8px; color:%s;'><b>%d</b></td>", color, users[i].rating);
        printf("<td style='padding:12px 8px;'>%s</td>", users[i].rank);
        printf("<td style='padding:12px 8px;'>%d</td>", users[i].total_contests);
        printf("<td style='padding:12px 8px;'>%d</td>", users[i].max_rating_ever);
        printf("<td style='padding:12px 8px;'>%d</td>", users[i].recent_contests_180d);
        printf("<td style='padding:12px 8px;'>%d</td>", users[i].max_rating_180d);
        printf("</tr>\n");
    }
    printf("</table>\n");
    printf("</div>\n");
    printf("</div>\n");
}

static void print_page_script(void) {
    printf("<script>\n");
    printf("function showUser(id) {\n");
    printf("  document.getElementById('list-container').style.display = 'none';\n");
    printf("  var details = document.getElementsByClassName('user-detail');\n");
    printf("  for (var i = 0; i < details.length; i++) {\n");
    printf("    details[i].style.display = 'none';\n");
    printf("  }\n");
    printf("  var userDiv = document.getElementById(id);\n");
    printf("  if (userDiv) {\n");
    printf("    userDiv.style.display = 'block';\n");
    printf("    var charts = userDiv.querySelectorAll('[id^=\"histogram_\"]');\n");
    printf("    for (var j = 0; j < charts.length; j++) {\n");
    printf("      var instance = echarts.getInstanceByDom(charts[j]);\n");
    printf("      if (instance) instance.resize();\n");
    printf("    }\n");
    printf("  }\n");
    printf("  window.scrollTo(0, 0);\n");
    printf("}\n");
    printf("function showList() {\n");
    printf("  document.getElementById('list-container').style.display = 'block';\n");
    printf("  var details = document.getElementsByClassName('user-detail');\n");
    printf("  for (var i = 0; i < details.length; i++) {\n");
    printf("    details[i].style.display = 'none';\n");
    printf("  }\n");
    printf("  window.scrollTo(0, 0);\n");
    printf("}\n");
    printf("</script>\n");
}

/* ======================== 主函数 ======================== */
int main(void) {
    freopen("result.html", "w", stdout);

    curl_global_init(CURL_GLOBAL_ALL);

    char handles[100][64];
    int n_handles = 0;
    FILE *fp = fopen("users.txt", "r");
    if (!fp) {
        fprintf(stderr, "Error: users.txt not found\n");
        printf("<html><body><h1>Error: users.txt not found</h1></body></html>\n");
        curl_global_cleanup();
        return 1;
    }
    char line[64];
    while (fgets(line, sizeof(line), fp) && n_handles < 100) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) > 0)
            strncpy(handles[n_handles++], line, 63);
    }
    fclose(fp);

    if (n_handles == 0) {
        fprintf(stderr, "No users in users.txt\n");
        printf("<html><body><h1>No users in users.txt</h1></body></html>\n");
        curl_global_cleanup();
        return 1;
    }

    fprintf(stderr, "Fetching info for %d users...\n", n_handles);

    printf("<!DOCTYPE html>\n<html lang='zh-CN'>\n<head>\n");
    printf("<meta charset='UTF-8'>\n");
    printf("<title>Codeforces 用户比赛分析</title>\n");
    printf("<style>\n");
    printf("body { font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 20px; background: #f0f2f5; }\n");
    printf(".container { max-width: 1200px; margin: 0 auto; }\n");
    printf("a { color: #3498db; }\n");
    printf(".user-detail { display: none; }\n");
    printf("</style>\n");
    printf("<script src='https://cdn.jsdelivr.net/npm/echarts@5.5.0/dist/echarts.min.js'></script>\n");
    printf("</head>\n<body>\n");
    printf("<div class='container'>\n");

    char handle_list[4096] = "";
    for (int i = 0; i < n_handles; i++) {
        if (i > 0) strcat(handle_list, ";");
        strcat(handle_list, handles[i]);
    }

    User users[100];
    int user_cnt = 0;
    fprintf(stderr, "Fetching basic user info...\n");
    char *info_json = fetch_user_info(handle_list);
    if (info_json) {
        cJSON *root = cJSON_Parse(info_json);
        if (root) {
            cJSON *result = cJSON_GetObjectItem(root, "result");
            if (cJSON_IsArray(result)) {
                int n = cJSON_GetArraySize(result);
                for (int i = 0; i < n && i < 100; i++) {
                    cJSON *user_obj = cJSON_GetArrayItem(result, i);
                    char *json_str = cJSON_PrintUnformatted(user_obj);
                    char wrapped[4096];
                    snprintf(wrapped, sizeof(wrapped), "{\"status\":\"OK\",\"result\":[%s]}", json_str);
                    users[user_cnt] = parse_user_info_json(wrapped);
                    free(json_str);
                    users[user_cnt].contests = NULL;
                    users[user_cnt].contest_count = 0;
                    users[user_cnt].total_contests = 0;
                    users[user_cnt].max_rating_ever = users[user_cnt].max_rating;
                    users[user_cnt].solved_ratings = NULL;
                    users[user_cnt].solved_times = NULL;
                    users[user_cnt].solved_count = 0;
                    user_cnt++;
                }
            }
            cJSON_Delete(root);
        }
        free(info_json);
    }

    fprintf(stderr, "Basic info fetched: %d users.\n", user_cnt);

    for (int i = 0; i < user_cnt; i++) {
        fprintf(stderr, "[%d/%d] Processing %s...\n", i+1, user_cnt, users[i].handle);
        sleep(2);

        fprintf(stderr, "  Fetching contest history...\n");
        char *rating_json = fetch_user_rating(users[i].handle);
        if (rating_json) {
            ContestRecord *contests = NULL;
            int cnt = 0;
            if (parse_rating_json(rating_json, &contests, &cnt) == 0) {
                users[i].contests = contests;
                users[i].contest_count = cnt;
                users[i].total_contests = cnt;
                for (int j = 0; j < cnt; j++) {
                    if (contests[j].new_rating > users[i].max_rating_ever)
                        users[i].max_rating_ever = contests[j].new_rating;
                }
                calc_recent_180d(&users[i]);
            }
            free(rating_json);
        }

        fprintf(stderr, "  Fetching submissions...\n");
        int *all_ratings = NULL;
        time_t *all_times = NULL;
        AcSubmission *all_subs = NULL;
        int ac_cnt = 0, sub_cnt = 0;
        int from = 1;
        int page = 1;
        while (1) {
            sleep(2);
            char *status_json = fetch_user_status(users[i].handle, from, 1000);
            if (!status_json) break;

            cJSON *root = cJSON_Parse(status_json);
            if (!root) { free(status_json); break; }
            cJSON *res = cJSON_GetObjectItem(root, "result");
            int batch_size = cJSON_GetArraySize(res);
            cJSON_Delete(root);

            int *tmp_r; time_t *tmp_t; AcSubmission *tmp_subs;
            int tmp_ac, tmp_sub;
            parse_status_full(status_json, &tmp_r, &tmp_t, &tmp_ac, &tmp_subs, &tmp_sub);
            free(status_json);

            all_ratings = realloc(all_ratings, (ac_cnt + tmp_ac) * sizeof(int));
            all_times = realloc(all_times, (ac_cnt + tmp_ac) * sizeof(time_t));
            memcpy(all_ratings + ac_cnt, tmp_r, tmp_ac * sizeof(int));
            memcpy(all_times + ac_cnt, tmp_t, tmp_ac * sizeof(time_t));
            ac_cnt += tmp_ac;

            all_subs = realloc(all_subs, (sub_cnt + tmp_sub) * sizeof(AcSubmission));
            memcpy(all_subs + sub_cnt, tmp_subs, tmp_sub * sizeof(AcSubmission));
            sub_cnt += tmp_sub;

            free(tmp_r); free(tmp_t); free(tmp_subs);

            if (batch_size == 0) break;
            fprintf(stderr, "    Fetched page %d of submissions...\n", page);
            page++;
            from += 1000;
        }
        users[i].solved_ratings = all_ratings;
        users[i].solved_times = all_times;
        users[i].solved_count = ac_cnt;

        fprintf(stderr, "  Counting solved and upsolved problems...\n");
        for (int j = 0; j < users[i].contest_count; j++) {
            int cid = users[i].contests[j].contest_id;
            int sc = 0, uc = 0;
            for (int k = 0; k < sub_cnt; k++) {
                if (all_subs[k].contest_id == cid) {
                    if (all_subs[k].is_upsolved) {
                        int dup = 0;
                        for (int m = 0; m < uc; m++) {
                            if (strcmp(users[i].contests[j].upsolved_problems[m],
                                       all_subs[k].problem_index) == 0) {
                                dup = 1;
                                break;
                            }
                        }
                        if (!dup && uc < 26) {
                            strcpy(users[i].contests[j].upsolved_problems[uc],
                                   all_subs[k].problem_index);
                            uc++;
                        }
                    } else {
                        int dup = 0;
                        for (int m = 0; m < sc; m++) {
                            if (strcmp(users[i].contests[j].solved_problems[m],
                                       all_subs[k].problem_index) == 0) {
                                dup = 1;
                                break;
                            }
                        }
                        if (!dup && sc < 26) {
                            strcpy(users[i].contests[j].solved_problems[sc],
                                   all_subs[k].problem_index);
                            sc++;
                        }
                    }
                }
            }
            users[i].contests[j].solved_count = sc;
            users[i].contests[j].upsolved_count = uc;
        }

        if (users[i].contest_count > 0) {
            qsort(users[i].contests, users[i].contest_count,
                  sizeof(ContestRecord), cmp_contest_by_time_desc);
        }

        free(all_subs);
        fprintf(stderr, "  Done %s\n", users[i].handle);
    }

    fprintf(stderr, "Data fetched, generating HTML...\n");

    print_user_list(users, user_cnt);

    for (int i = 0; i < user_cnt; i++) {
        print_user_detail(&users[i]);
    }

    print_page_script();

    printf("</div>\n");

    for (int i = 0; i < user_cnt; i++) {
        free(users[i].contests);
        free(users[i].solved_ratings);
        free(users[i].solved_times);
    }

    printf("</body>\n</html>\n");
    curl_global_cleanup();
    fclose(stdout);

    fprintf(stderr, "result.html generated!\n");
    return 0;
}
