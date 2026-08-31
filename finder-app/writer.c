#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    // 初始化 syslog
    openlog("writer", LOG_PID, LOG_USER);

    // 检查参数个数（需接收 writefile 和 writestr）
    if (argc != 3) {
        syslog(LOG_ERR, "Error: Expected 2 arguments (file path and string), received %d", argc - 1);
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // 记录 LOG_DEBUG 级别的写入日志
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    // 打开文件进行写入
    FILE *fp = fopen(writefile, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file %s: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    // 写入内容并检查是否成功
    if (fputs(writestr, fp) == EOF) {
        syslog(LOG_ERR, "Error writing to file %s: %s", writefile, strerror(errno));
        fclose(fp);
        closelog();
        return 1;
    }

    fclose(fp);
    closelog();
    return 0;
}