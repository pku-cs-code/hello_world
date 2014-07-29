#include <utils/utils.h>
#include <curl/curl.h>
#include "config.h"
#include "report.h"
#include "topology.h"
#include "sw_scaner.h"

#define WORK_DIR          "./"
#define CONFIG_FILE WORK_DIR"sw_scaner.conf"

/* 静态函数声明区 */
static int parse_args(int argc, char **argv);
static void show_help(const char *program);
static void show_version(const char *program);
static int create_daemon_process();
static int set_daemon_process();
static int  install_signals(void);
static void signal_handle(int signum);
static pid_t create_work_process(topology_t *topology);

/* 全局变量申明区 */
char            *_config_file   = CONFIG_FILE; 
config_t        *_config        = NULL;
topologies_t    *_topologies    = NULL;
sw_scaner_t     *_sw_scaner     = NULL;
int              _main_continue = 1;

int main(int argc, char ** argv) {
    int i = 0;
    pid_t *pids = NULL;

    if (!parse_args(argc, argv)) {
        return -1;
    }

    init_snmp("sw_scaner");

    _config = config_load(_config_file);
    if (NULL == _config) {
        return -1;
    }
    log_set_level(_config->log_level);
    
    if (LOG_DST_FILE == _config->log_dst) {
        if (!log_set_file(_config->log_file)) {
            log_error("log_set_file failed.");
            return -1;
        }
    }

    config_dump(_config);

    if (_config->daemon) {
        log_info("create daemon process.");
        if (!create_daemon_process()) {
            log_error("create daemon process failed.");
            return -1;
        }
    }

    install_signals();

    if (curl_global_init(CURL_GLOBAL_ALL)) {
        log_error("curl_global_init failed.");
        return -1;
    }

    /* 屏蔽#注释字符，community中有可能有#字符 */
    property_set_comment_char(0);
    _topologies = topologies_load(_config->idc_topology_file);
    if (NULL == _topologies) {
        log_error("load idc topology data failed.");
        return -1;
    }

    pids = (pid_t *)malloc(sizeof(pid_t) * _topologies->count);
    if (NULL == pids) {
        log_error("malloc for pids failed.");
        return -1;
    }

    for (i = 0; i < _topologies->count; i++) {
        pids[i] = create_work_process(&_topologies->topologies[i]);
        if (pids[i] == 0) {
            break; 
        }
        else if (pids[i] == -1) {
            log_error("create_work_process failed.");
            return -1;
        }
        else {
            log_info("create work process: %d.", pids[i]);
        }
    }

    while (_main_continue) {
        usleep(5000);
    }

    config_free(_config);
    curl_global_cleanup();
    sw_scaner_destroy(_sw_scaner); 
    topologies_free(_topologies);

    return 0;
}

static pid_t create_work_process(topology_t *topology)
{
    pid_t pid = 0;
    char buffer[256] = {0};
    struct timeval last_time;
    
    pid = fork();
    if (pid < 0) {
        log_error("create work_process failed.");
        return -1;
    }

    if (pid > 0) {
        return pid;
    }

    set_daemon_process();

    snprintf(buffer, sizeof(buffer) - 1, _config->log_file, getpid());
    log_set_file(buffer);

    _sw_scaner = sw_scaner_create(topology);
    if (NULL == _sw_scaner) {
        log_error("create sw_scaner failed.");
        return -1;
    }

    bzero(&last_time, sizeof(struct timeval));
    while (_main_continue) {
        struct timeval cur_time;
        unsigned int delta = 0;

        gettimeofday(&cur_time, NULL);
        delta = (cur_time.tv_sec  - last_time.tv_sec) * 1000 + 
                (cur_time.tv_usec - last_time.tv_usec)/1000;

        if (delta >= 2000) {
            struct timeval t1, t2;

            memcpy(&last_time, &cur_time, sizeof(struct timeval));

            log_info("start scan all sw ......");
            gettimeofday(&t1, NULL);
            if (!sw_scaner_scan(_sw_scaner)) {
                log_error("sw_scaner_scan failed.");
                return 0;
            }
            gettimeofday(&t2, NULL);
            delta = (t2.tv_sec  - t1.tv_sec) * 1000 +
                    (t2.tv_usec - t1.tv_usec)/1000;
            log_info("scan delta: %ums.", delta);

            gettimeofday(&t1, NULL);
            report_do(topology, _sw_scaner, _config->http_timeout);
            gettimeofday(&t2, NULL);
            delta = (t2.tv_sec  - t1.tv_sec) * 1000 +
                    (t2.tv_usec - t1.tv_usec)/1000;
            log_info("report delta: %ums.", delta);
        }
    } 

    return 0;
}

static int parse_args(int argc, char **argv)
{
    int opt_char = 0;
    int parsed   = 0;

    if (argc == 1) {
        parsed = 1;
    }

    while(-1 != (opt_char = getopt(argc, argv, "c:vh"))){
        parsed = 1; 

        switch(opt_char){
            case 'c':
                _config_file = optarg;
                break;
            case 'v':
                show_version(argv[0]);
                exit(0);
            case 'h':
                show_help(argv[0]);
                exit(0);
            case '?':
            case ':':
            default:
                log_error("unknown arg: %c.", opt_char);
                show_help(argv[0]);
                return 0;
        }
    }

    if (parsed) {
        return 1;
    }
    else {
        log_error("unknown arg.");
        show_help(argv[0]);
        return 0;
    }
}

static void show_help(const char *program)
{
    printf("Usage: %s [-c config_file]\n", program); 
    printf("       %s -v\n", program); 
    printf("       %s -h\n", program); 
    printf("       -c: 指定配置文件，默认./sw_scaner.conf。\n"); 
    printf("       -v: 显示版本号。\n"); 
    printf("       -h: 显示命令帮助。\n\n"); 
}

static void show_version(const char *program)
{
    printf("%s, Version 1.0.0\n\n", program);
}

static int set_daemon_process()
{
    int   fd  = -1;

    /* 脱离原始会话 */ 
    if (setsid() == -1) {
        log_error("setsid failed.");
        return 0;
    }

    /* 修改工作目录 */
    chdir("/");

    /* 重设掩码 */
    umask(0);

    fd = open("/dev/null", O_RDWR); 
    if (fd == -1) {
        log_error("open /dev/null failed.");
        return 0;
    }

    /* 重定向子进程的标准输入到null设备 */
    if (dup2(fd, STDIN_FILENO) == -1) {  
        log_error("dup2 STDIN to fd failed.");
        return 0;
    }

    /* 重定向子进程的标准输出到null设备 */
    if (dup2(fd, STDOUT_FILENO) == -1) {
        log_error("dup2 STDOUT to fd failed.");
        return 0;
    }

    /* 重定向子进程的标准错误到null设备 */
    if (dup2(fd, STDERR_FILENO) == -1) {
        log_error("dup2 STDERR to fd failed.");
        return 0;
    }

    return 1;
}

static int create_daemon_process()
{
    pid_t pid = 0;
    int   fd  = -1;

    pid = fork();
    /* 创建进程错误 */
    if (pid < 0) {
        return 0;
    }
    /* 父进程 */
    else if (pid > 0) {
        config_free(_config);
        exit(0);
    }
    /* 子进程 */
    else {
        /* 脱离原始会话 */ 
        if (setsid() == -1) {
            log_error("setsid failed.");
            return 0;
        }

        /* 修改工作目录 */
        chdir("/");

        /* 重设掩码 */
        umask(0);

        fd = open("/dev/null", O_RDWR); 
        if (fd == -1) {
            log_error("open /dev/null failed.");
            return 0;
        }

        /* 重定向子进程的标准输入到null设备 */
        if (dup2(fd, STDIN_FILENO) == -1) {  
            log_error("dup2 STDIN to fd failed.");
            return 0;
        }

        /* 重定向子进程的标准输出到null设备 */
        if (dup2(fd, STDOUT_FILENO) == -1) {
            log_error("dup2 STDOUT to fd failed.");
            return 0;
        }

        /* 重定向子进程的标准错误到null设备 */
        if (dup2(fd, STDERR_FILENO) == -1) {
            log_error("dup2 STDERR to fd failed.");
            return 0;
        }
    }

    return 1;
}

static int install_signals(void){
    if(SIG_ERR == signal(SIGINT, signal_handle)){
        log_error("Install SIGINT fails.");
        return 0;
    }   
    if(SIG_ERR == signal(SIGTERM, signal_handle)){
        log_error("Install SIGTERM fails.");
        return 0;
    }   
    if(SIG_ERR == signal(SIGSEGV, signal_handle)){
        log_error("Install SIGSEGV fails.");
        return 0;
    }   
    if(SIG_ERR == signal(SIGBUS, signal_handle)){
        log_error("Install SIGBUS fails.");
        return 0;
    }   
    if(SIG_ERR == signal(SIGQUIT, signal_handle)){
        log_error("Install SIGQUIT fails.");
        return 0;
    } 
    if(SIG_ERR == signal(SIGCHLD, signal_handle)){
        log_error("Install SIGCHLD fails.");
        return 0;
    }

    return 1;
}

static void signal_handle(int signum){
    if(SIGTERM == signum){
        log_info("recv kill signal, glance will exit normally.");
        _main_continue = 0;
    }
    else if(SIGINT == signum){
        log_info("recv CTRL-C signal, glance will exit normally.");
        _main_continue = 0;
    }
    else if(SIGCHLD == signum){
        log_debug("recv SIGCHLD signal[%d].", signum);
    }
    else{
        log_info("receive signal: %d", signum);
        exit(0);
    }
}

