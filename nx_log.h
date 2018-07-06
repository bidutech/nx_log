/*************************************************************************
    > File Name: nx_log.h
    > Author: bidutech
    > Mail: bidutech
    > Created Time: 2018年04月11日 星期三 14时47分09秒
    this is process and thread safe logger use flock and mutex
    suport multi process single thread multi process multi thread
    single process multi thread ;(all safe  if init the right lock)sss
 ************************************************************************/

#ifndef _NX_LOGNX_H_
#define _NX_LOGNX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/file.h>
#include <pthread.h>
#include "nx_array.h"


#define nx_log_stderr(...) do {    \
    _nx_log_stderr(__VA_ARGS__);   \
} while (0)

#define nx_write(_d, _b, _n)    \
    write(_d, _b, (size_t)(_n))
#define nx_vscnprintf(_s, _n, _f, _a)   \
    _nx_vscnprintf((char *)(_s), (size_t)(_n), _f, _a)

#define nx_strftime(_s, _n, fmt, tm)        \
    (int)strftime((char *)(_s), (size_t)(_n), fmt, tm)

typedef enum {
    NX_LEVEL_ON,/* open all log*/
    NX_LEVEL_DEBUG,
    NX_LEVEL_INFO,
    //NX_LEVEL_NOTICE,
    //NX_LEVEL_WARN,
    NX_LEVEL_ERROR,
    //NX_LEVEL_FATAL,
    NX_LEVEL_OFF
} nx_log_level;


#define MAX_LOG_LEVEL_FILE NX_LEVEL_OFF
#define ENUM_LEVEL_INI_LOG_NAME() static char \
    nx_log_name[MAX_LOG_LEVEL_FILE][MAX_FILE_NAME] = {\
    "null.log", \
    "debug.log",\
    "info.log",\
    "error.log",\
}

#define G_NX_LOG_PROCESS_LOCK_FILE_NAME  "/tmp/.nx_plock.lock"
#define G_NX_LOG_PROCESS_MSGQ_LOCK_FILE_NAME  "/tmp/.nx_p_msgq_lock.lock"

#define NX_LOG_ASYN_MODEL   1
#define NX_LOG_MAX_LEN 512
#define NX_LOG_MAX_MSG_QUEUE   1000

#define NX_LOG_THREAD_NUM  3
#define MAX_FILE_NAME   1024

#define ASYN_LOG    1
#define SYN_LOG     0


// 1M
#define DEFAULT_SIZE    1024*1024

struct nx_logger;

struct nx_log_msg{
    int level;
    char msg[NX_LOG_MAX_LEN];
};

struct _file{
    struct stat     stat_info;
    int             fd;
    uint64_t        size;
    uint64_t        max_size;
    int             count;
    char            name[MAX_FILE_NAME];
    int             w_threads;           /* write thread num nouse */
    int             asyn_period;  /* log num in msgq > asyn_period  call fsync*/
    pthread_mutex_t mutex;
    struct nx_logger *logger;
};

/********************************************************************
****所以可以看出fflush和fsync的调用顺序应该是：
****c库缓冲-----fflush---------〉内核缓冲--------fsync-----〉磁盘
********************************************************************/

struct nx_logger{
    uint32_t        file_max_size;                  /* M*/
    char            log_path[MAX_FILE_NAME];        /* log store path*/
    char            rotate_format[MAX_FILE_NAME];   /* same c printf*/
    struct _file    log_files[MAX_LOG_LEVEL_FILE];  /* log file descriptor */
    char            prefilename[MAX_FILE_NAME];
    nx_log_level    level;                          /* log level */
    int             w_threads;
    int             async;
    int             nerror;
    int             nx_errno;                   /* # log error */
    char            errno_str[MAX_FILE_NAME];   /* error info string*/
    int             inited;                     /* 1 inited*/
    int             use_plock;
    /**********************use_plock***************************
     * process write file use flock default 1 use flock
     *********************************************************/
    int             use_tlock;
    /***********************use_tlock*************************************
     * thread mutex default 0 not use mutex
     * single process multi threads  open file use APPEND ,write (not lseek）
     * can not lock ,because multi thread share the same file table
    ********************************************************************/
    int   plock_fd;
    /******************plock_fd***************************************
     *  process lock fd used when rotate and write file
     *****************************************************************/

    int             pid;
    int             ppid;

    int  asyn;
    struct nx_array msgq;
    int front;
    int tail;
    int msgq_max_size;
    int msgq_size;
    pthread_mutex_t msgq_mutex;
    pthread_cond_t msgq_not_empty;
    pthread_cond_t msgq_not_full;

};


/***********************同步log *********************/
#define NX_SYN_LOG_DEBUG(format, ...)\
    nx_syn_write_log(NX_LEVEL_DEBUG, __FILE__, \
    __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#define NX_SYN_LOG_ERROR(format, ...) \
    nx_syn_write_log(NX_LEVEL_ERROR, __FILE__, \
    __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#define NX_SYN_LOG_INFO(format, ...)  \
    nx_syn_write_log(NX_LEVEL_INFO, __FILE__, \
    __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

/***********************异步log*********************/
#define NX_ASYN_LOG_DEBUG(format, ...)\
    nx_asyn_write_log(NX_LEVEL_DEBUG, __FILE__, \
    __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#define NX_ASYN_LOG_ERROR(format, ...) \
    nx_asyn_write_log(NX_LEVEL_ERROR, __FILE__,\
    __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#define NX_ASYN_LOG_INFO(format, ...)  \
    nx_asyn_write_log(NX_LEVEL_INFO, __FILE__,\
    __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

//#define LOG_WARN(format, ...)  write_log(WARN, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

/***************syn interface and asyn interface*********************************
name:nx_logger_init
param:
 asyn: 0 syn; 1 asyn will lose log
 df_level: default log level
 log_path: log path
 log_file_max_size: log file max size will rotate
****************************************************/
struct nx_logger * nx_logger_init(int asyn,int df_level,char * log_path,
                                  char *prefilename,
                                  int log_file_max_size/*M*/);

int  nx_logger_deinit(struct nx_logger *l);

void _nx_log_stderr(const char *fmt, ...);

void nx_asyn_write_log(int  level,  const char* file,  const char* function,
                 int  line,   const char* format, ...);

void nx_syn_write_log(int  level,  const char* file,  const char* function,
                 int  line,   const char* format, ...);


int test_nx_log();

#ifdef __cplusplus
}
#endif

#endif
