/*************************************************************************
    > File Name: nx_log.c
    > Author: bidutech
    > Mail: bidutech
    > Created Time: 2018年04月11日 星期三 14时47分01秒
 ************************************************************************/

#include <stdio.h>
#include "nx_log.h"

ENUM_LEVEL_INI_LOG_NAME();
static int  gfd = -1;
static struct nx_logger nx_logger_instance;

static void _nx_write_log(int level,char * buf,int len);
static int nx_logmsg_produce(int level,char * msg);
static int _nx_vscnprintf(char *buf, size_t size, const char *fmt, va_list args);

static int nx_log_file_exist(char * filename){
    if(filename == NULL)
        return 0;
    struct stat info;
    if(0 != stat(filename, &info)){
        if(errno == ENOENT){
            return 0;
        }
    }
    return 1;
}

static int nx_log_loggable(struct nx_logger *l, int level){
    if(l == NULL ||level >= NX_LEVEL_OFF || level <= NX_LEVEL_NULL){
        return 0;
    }
    if(l->inited !=1)
        return 0;

    if (level < l->level) {
        return 0;
    }

    return 1;
}

static int nx_lock_log_file(struct nx_logger *l,int fd,int level){
    if(l == NULL || fd <0)
        return 0;
    int ret = 0;
    if(l->use_plock == 1){
        //lock
        if(fd >=0){
            ret = flock(fd/*l->log_files[level].fd*/, LOCK_EX);
        }
    }
    if(l->use_tlock == 1){
        pthread_mutex_lock(&(l->log_files[level].mutex)/*&(l->mutex)*/);
    }
    return ret;
}

static int nx_unlock_log_file(struct nx_logger *l,int fd,int level){
    if(l == NULL || fd <0)
        return 0;

    int ret = 0;

    if(l->use_plock == 1){
        //unlock
        if(fd >=0){
            ret = flock(fd/*l->log_files[level].fd*/, LOCK_UN);
        }
    }

    if(l->use_tlock == 1){
        pthread_mutex_unlock(&(l->log_files[level].mutex)/*&(l->mutex)*/);
    }
    return ret;
}

static void _nx_write_log(int level,char * buf,int len) {

    if( buf == NULL || len <=0 ){
        return ;
    }

    int        r = -1;/*stat call. errno*/
    int        newfd = -1;
    char       rotate[MAX_FILE_NAME] = {0};
    ssize_t    n;
    struct stat info;
    struct stat *old;
    struct timeval  tv;
    static struct nx_logger *l = &nx_logger_instance;

    if(!nx_log_loggable(l,level))
        return;


    if(l->log_files[level].fd < 0){
        l->log_files[level].fd = open(l->log_files[level].name,
                                      O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (l->log_files[level].fd < 0) {
            nx_log_stderr("opening log file '%s' failed: %s",
                          l->log_files[level].name, strerror(errno));
            return ;
        }
    }

    if(l->plock_fd < 0){
        l->plock_fd = open(G_NX_LOG_PROCESS_LOCK_FILE_NAME, O_CREAT|O_RDWR);
        if(l->plock_fd <0){
            nx_log_stderr("open file plock_fd error");
        }
    }

    old = &(l->log_files[level].stat_info);

    nx_lock_log_file(l,l->plock_fd,level);
    /*write buf. multi proces if buflen < kernel buflen no need lock*/
    n = nx_write(l->log_files[level].fd, buf, len);
    if (n < 0) {
        l->nerror++;
        nx_log_stderr("write log file '%s' failed: %s",
                      l->log_files[level].name, strerror(errno));
    }
    nx_unlock_log_file(l,l->plock_fd,level);

    if (0 !=stat(l->log_files[level].name, &info)){
        r = errno;
        //info.st_ino = -1;
        nx_log_stderr("stat log file '%s' failed: %s",
                      l->log_files[level].name, strerror(errno));
    }

    if(r == ENOENT){
        //file not find
        goto  rotate;
    }
    if(info.st_ino == old->st_ino){
        if (info.st_size + n < l->file_max_size*DEFAULT_SIZE){
            return;
        }
    }

rotate:
    //lock
    // if use lock l->log_files[level].name must exist
    r = -1;
    nx_lock_log_file(l,l->plock_fd,level);
    gettimeofday(&tv, NULL);
    char tailformat[1024] = {0};
    nx_strftime(tailformat, 1024, l->rotate_format, localtime(&tv.tv_sec));
    l->log_files[level].count = (l->log_files[level].count) % 9;
    snprintf(rotate,sizeof(rotate),"%s/%s_%s%d",l->log_path,
             nx_log_name[level], tailformat, l->log_files[level].count);
    l->log_files[level].count++;

    if (0 !=stat(l->log_files[level].name, &info)){
        r = errno;
        //info.st_ino = -1;
        nx_log_stderr("stat log file '%s' failed: %s",
                      l->log_files[level].name, strerror(errno));
    }

    if(r != ENOENT  && info.st_ino == old->st_ino){
        rename(l->log_files[level].name,rotate);
    }
    newfd = open(l->log_files[level].name,
                 O_WRONLY | O_APPEND | O_CREAT, 0644);
    if(newfd <0){
        rename(rotate,l->log_files[level].name);
        nx_log_stderr("file rotate error:%d-%s",errno,strerror(errno));
        //unlock
        nx_unlock_log_file(l,l->plock_fd,level);
        return;
    }
    l->log_files[level].fd = dup2(newfd,l->log_files[level].fd);
    close(newfd);

    if (0 !=stat(l->log_files[level].name,  &(l->log_files[level].stat_info))){
        nx_log_stderr("stat log file '%s' failed: %s",
                      l->log_files[level].name, strerror(errno));
    }

    old = &(l->log_files[level].stat_info);
    //unlock
    nx_unlock_log_file(l,l->plock_fd,level);
}

static nx_log_level nx_log_2level(const char* level)
{
    if (!level)
    {
        return NX_LEVEL_NULL;
    }

    if (strcmp("DEBUG", level) == 0)
    {
        return NX_LEVEL_DEBUG;
    }

    if (strcmp("INFO", level) == 0)
    {
        return NX_LEVEL_INFO;
    }

    //     if (strcmp("WARN", level) == 0)
    //     {
    //        return NX_LEVEL_WARN;
    //     }

    if (strcmp("ERROR", level) == 0)
    {
        return NX_LEVEL_ERROR;
    }

    if (strcmp("OFF", level) == 0)
    {
        return NX_LEVEL_OFF;
    }

    return NX_LEVEL_DEBUG;
}

static char* nx_log_level_2_str( nx_log_level level)
{
    switch (level)
    {
    case NX_LEVEL_DEBUG:
        return (char*)"DEBUG";
    case NX_LEVEL_INFO:
        return (char*)"INFO";
        //    case NX_LEVEL_WARN:
        //        return (char*)"WARN";
    case NX_LEVEL_ERROR:
        return (char*)"ERROR";
    case NX_LEVEL_OFF:
        return (char*)"OFF";
    default:
        return (char*)"UNKNOW";
    }
    return (char*)"UNKNOW";
}


static void nx_logger_set_w_threads(struct nx_logger *l,int level,int thread_num){
    if(!nx_log_loggable(l,level)){
        nx_log_stderr("error log level:%d",level);
        return;
    }

    l->log_files[level].w_threads = thread_num;
    return;
}

static int _nx_vscnprintf(char *buf, size_t size, const char *fmt, va_list args){

    int n;

    n = vsnprintf(buf, size, fmt, args);

    /*
     * The return value is the number of characters which would be written
     * into buf not including the trailing '\0'. If size is == 0 the
     * function returns 0.
     *
     * On error, the function also returns 0. This is to allow idiom such
     * as len += _vscnprintf(...)
     *
     * See: http://lwn.net/Articles/69419/
     */
    if (n <= 0) {
        return 0;
    }

    if (n < (int) size) {
        return n;
    }

    return (int)(size - 1);
}

static int nx_logmsg_produce(int level,char * msg)
{
    static struct nx_logger *l = &nx_logger_instance;
    if(msg == NULL)
        return -1;
    if(!nx_log_loggable(l,level))
        return -1;

    struct timeval now;
    struct timespec outtime;

    pthread_mutex_lock(&l->msgq_mutex);
    while(l->msgq_size >= l->msgq_max_size){
        /***default asynchronous will lose some msg*****/
        /******synchronous will block the request******/
        if(0 == l->asyn){
            //gettimeofday(&now, NULL);
            //outtime.tv_sec = now.tv_sec;
            //outtime.tv_nsec= (now.tv_usec + 3) * 1000;/*3 microseconds */
            //pthread_cond_timedwait(&l->msgq_not_full,&l->msgq_mutex,&outtime);
            pthread_cond_wait(&l->msgq_not_full, &l->msgq_mutex);
        }
        if(l->msgq_size >= l->msgq_max_size){
            pthread_mutex_unlock(&l->msgq_mutex);
            printf("Discard the msg\n");
            return -1;
        }
    }

    if(l->tail <= l->msgq_max_size){
        struct nx_log_msg *pmsg = NULL;
        pmsg = nx_array_get(&l->msgq,l->tail);
        if(pmsg != NULL){
            pmsg->level = level;
            memset(pmsg->msg,0,sizeof(pmsg->msg));
            snprintf(pmsg->msg,sizeof(pmsg->msg),"%s",msg);
        }
    }

done:
    l->tail = (l->tail +1) % l->msgq_max_size;
    l->msgq_size++;
    if(l->msgq_size > 0){
        pthread_cond_broadcast(&l->msgq_not_empty);
    }
    pthread_mutex_unlock(&l->msgq_mutex);
    return 0;
}


static void * nx_log_consume(void *arg){

    static struct nx_logger *l = (struct nx_logger *)&nx_logger_instance;

    if(l->inited != 1){
        sleep(1);
    }

    while(1){

        pthread_mutex_lock(&l->msgq_mutex);
        while(l->msgq_size <= 0){
            pthread_cond_wait(&l->msgq_not_empty, &l->msgq_mutex);
        }

        if(l->front <= l->msgq_max_size){
            struct nx_log_msg *pmsg= NULL;
            pmsg = nx_array_get(&l->msgq,l->front);
            _nx_write_log(pmsg->level,pmsg->msg,strlen(pmsg->msg));
            pmsg->level = 0;
            memset(pmsg->msg,0,sizeof(pmsg->msg));
        }

done:
        l->front = (l->front +1)  % l->msgq_max_size;
        l->msgq_size--;
        if(l->msgq_size <= l->msgq_max_size){
            pthread_cond_broadcast(&l->msgq_not_full);
        }
        pthread_mutex_unlock(&l->msgq_mutex);
    }
}

void _nx_log_stderr(const char *fmt, ...){
    int len, size, errno_save;
    char buf[2 * NX_LOG_MAX_LEN];
    va_list args;

    errno_save = errno;
    len = 0;                /* length of output buffer */
    size = 2 * NX_LOG_MAX_LEN; /* size of output buffer */

    va_start(args, fmt);
    len += nx_vscnprintf(buf, size, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    nx_write(STDERR_FILENO, buf, len);
    errno = errno_save;
}

struct nx_logger * nx_logger_init(int asyn,int df_level,char * log_path, int log_file_max_size/*M*/){

    static struct nx_logger *l = (struct nx_logger *)&nx_logger_instance;
    int i =0;
    int status = NX_ERROR;
    l->inited = 0;

    snprintf(l->log_path,sizeof(l->log_path),log_path);
    snprintf(l->rotate_format, sizeof(l->rotate_format),"%s","%Y%m%d%H%M%S");
    l->level = df_level;
    l->file_max_size = log_file_max_size;
    l->use_plock =1;
    l->use_tlock =1;


    for(i = 0; i < MAX_LOG_LEVEL_FILE; i++ ){
        if(i == NX_LEVEL_NULL)
            continue;
        if(l->inited != 1){
            snprintf(l->log_files[i].name,sizeof(l->log_files[i].name),"%s/%s",l->log_path,nx_log_name[i]);
            l->log_files[i].fd = open(l->log_files[i].name, O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (l->log_files[i].fd < 0) {
                for(i = 0; i < MAX_LOG_LEVEL_FILE; i++ ){
                    if(l->log_files[i].fd >= 0){
                        close(l->log_files[i].fd);
                    }
                }
                nx_log_stderr("opening log file '%s' failed: %s", l->log_files[i].name, strerror(errno));
                return NULL;
            }
        }

        if (0 !=stat(l->log_files[i].name, &(l->log_files[i].stat_info))){
            nx_log_stderr("stat log file '%s' failed: %s", l->log_files[i].name, strerror(errno));
        }
        l->log_files[i].max_size = l->file_max_size * DEFAULT_SIZE;
        l->log_files[i].logger = l;
        l->log_files[i].w_threads = l->w_threads;
        l->log_files[i].count = 0;
        pthread_mutex_init(&(l->log_files[i].mutex),NULL);
    }
    if(l->use_plock !=0)
        l->use_plock =1;
    if(l->use_tlock !=1)
        l->use_tlock =0;

    l->plock_fd = -1;

    status = nx_array_init(&(l->msgq), NX_LOG_MAX_MSG_QUEUE+1, sizeof(struct nx_log_msg));
    if (status != NX_OK) {
        return status;
    }
    l->msgq.nelem = NX_LOG_MAX_MSG_QUEUE+1;

    l->tail = l->front =0;
    l->msgq_max_size = NX_LOG_MAX_MSG_QUEUE ;
    l->msgq_size = 0;
    if(asyn) {
        l->asyn = NX_LOG_ASYN_MODEL;
    }else{
        l->asyn = 0;
    }
    pthread_mutex_init(&l->msgq_mutex,NULL);
    pthread_cond_init(&l->msgq_not_empty, NULL);
    pthread_cond_init(&l->msgq_not_empty, NULL);

    pthread_t thrd[NX_LOG_THREAD_NUM]={0};

    //for(i=0;i < NX_LOG_THREAD_NUM; i++){
    i =0;
    pthread_create(&thrd[i],NULL,nx_log_consume,NULL);
    //}


    l->inited = 1;
    return l;
}

int nx_logger_deinit(struct nx_logger *l){

    if(l == NULL)
        return 0;
    int i =0;

    for(i = 0; i < MAX_LOG_LEVEL_FILE; i++ ){
        if(i == NX_LEVEL_NULL)
            continue;
        close(l->log_files[i].fd);
    }

    return 1;
}

void nx_asyn_write_log(int  level,  const char* file,  const char* function,
                       int  line,   const char* format, ...)
{

    int   temlen = 0;
    int   len = 0;
    int   size = NX_LOG_MAX_LEN;
    char  msg[NX_LOG_MAX_LEN];
    char  timestamp[NX_LOG_MAX_LEN];
    char  full_msg[NX_LOG_MAX_LEN] ={0};
    char  *f = NULL;
    struct timeval tv;
    static struct nx_logger *l = &nx_logger_instance;

    if(!nx_log_loggable(l,level))
        return;

    gettimeofday(&tv, NULL);
    temlen += nx_strftime(timestamp + temlen, size - temlen, "\n[%Y-%m-%d %H:%M:%S."
                          , localtime(&tv.tv_sec));
    temlen += snprintf(timestamp + temlen, size - temlen, "%03ld]", tv.tv_usec/1000);

    va_list args;
    va_start(args, format);
    len += nx_vscnprintf(msg, NX_LOG_MAX_LEN, format, args);
    va_end(args);

    f= strrchr(file,'/');
    f +=1;
//    sprintf(full_msg, "%s %s %s %s:%d %s", timestamp,
//            nx_log_level_2_str(level), f,function,line, msg);

    sprintf(full_msg, "\n%s", msg);

    nx_logmsg_produce(level,full_msg);

}

void nx_syn_write_log(int  level,  const char* file,  const char* function,
                      int  line,   const char* format, ...){

    int   temlen = 0;
    int   len = 0;
    int   size = NX_LOG_MAX_LEN;
    char  msg[NX_LOG_MAX_LEN];
    char  timestamp[NX_LOG_MAX_LEN];
    char  full_msg[NX_LOG_MAX_LEN] ={0};
    char  *f = NULL;
    struct timeval tv;
    static struct nx_logger *l = &nx_logger_instance;
    if(!nx_log_loggable(l,level))
        return;

    gettimeofday(&tv, NULL);
    temlen += nx_strftime(timestamp + temlen, size - temlen,
                          "\n[%Y-%m-%d %H:%M:%S.", localtime(&tv.tv_sec));
    temlen += snprintf(timestamp + temlen, size - temlen,
                       "%03ld]", tv.tv_usec/1000);

    va_list args;
    va_start(args, format);
    len += nx_vscnprintf(msg, NX_LOG_MAX_LEN, format, args);
    va_end(args);

    f= strrchr(file,'/');
    f +=1;
//    sprintf(full_msg, "%s %s %s %s:%d %s", timestamp,
//            nx_log_level_2_str(level), f,function,line, msg);
    sprintf(full_msg, "\n%s", msg);

    _nx_write_log(level,full_msg,strlen(full_msg));

}


int test_nx_log(){

    char * logpath ="/data1/home/test/log/";
    nx_logger_init(1,NX_LEVEL_NULL,logpath,10);

    char *msg = logpath;

    while(1){
        NX_ASYN_LOG_INFO("%s",msg);
        NX_ASYN_LOG_ERROR("%s",msg);
        NX_ASYN_LOG_DEBUG("%s",msg);
        sleep(1);
    }


    return 0;

    static struct nx_logger *l = &nx_logger_instance;

    /********************************Test code**********************************/
    struct timeval tv;
    gettimeofday(&tv, NULL);
    char tempbuf[1024] = {0};
    nx_strftime(tempbuf, 1024, "%Y%m%d%H%M%S", localtime(&tv.tv_sec));
    printf("time fomate:%s\n",tempbuf);
    nx_log_file_exist("nouse");
    snprintf(l->rotate_format, sizeof(l->rotate_format),"%s","%Y%m%d%H%M%S");
    /**********************************************************************/

    nx_logger_init(0,NX_LEVEL_INFO,logpath,10);
    //    l = &nx_logger_instance;

    char buf[1024] ={0};
    memset(buf,'x',1022);
    buf[1022]='\n';

    if(0){
        int tfd = -1,tfd1 = -1;
        int ret1 =-1;
        tfd =open(G_NX_LOG_PROCESS_LOCK_FILE_NAME, O_CREAT|O_RDWR);
        ret1 = flock(tfd, LOCK_EX);
        printf("flock fd:%d,%d\n",tfd,ret1);
        ret1 = flock(tfd,LOCK_EX);// not lock
        printf("flock fd:%d,%d\n",tfd,ret1);

        tfd1 =open(G_NX_LOG_PROCESS_LOCK_FILE_NAME, O_CREAT|O_RDWR);
        ret1 = flock(tfd1, LOCK_EX);//locked
        printf("flock fd:%d,%d\n",tfd,ret1);
        ret1 = flock(tfd1, LOCK_EX);
        printf("flock fd:%d,%d\n",tfd,ret1);
    }

    int i = 0;

    for(i =0;i < 10; i++){
        //_nx_log_info(l,buf,strlen(buf));
        //nx_syn_write_log(NX_LEVEL_INFO,buf,strlen(buf));
        //_nx_log_info(buf,strlen(buf));
        NX_SYN_LOG_INFO("%s",buf);
        //nx_write_log(l,NX_LEVEL_ERROR,buf,strlen(buf));
        //nx_write_log(l,NX_LEVEL_DEBUG,buf,strlen(buf));
        sleep(1);
    }


    pid_t gpid = getpid();


    pid_t fpid; //fpid表示fork函数返回的值
    int count=0;
    fpid=fork();
    if (fpid < 0)
        printf("error in fork!");
    else if (fpid == 0) {
        printf("i am the child process, my process id is %d,ppid:%d\n",getpid(),getppid());
        count++;
        char buf[2048] ={0};
        int i =0;
        while(1){
            for(i=0;i < 1 ; i++){
                memset(buf,'X',2048);
                sprintf(buf,"ppid:%d,child pid:%d-%d", getppid(),getpid(),i);
                // nx_write_log(NX_LEVEL_INFO,buf,2048);
                // _nx_log_info(buf,strlen(buf));
                NX_ASYN_LOG_INFO("%s",buf);
                sleep(1);
            }
        }
    } else {
        sleep(1);
        printf("i am the parent process, my process id is %d-%d\n",getpid(),getppid());
        char buf[2048] ={0};
        int i =0;
        while(1){
            for(i=1;i < 10 ; i++){
                memset(buf,'Y',2048);
                sprintf(buf,"ppid:%d,child pid:%d-%d", getppid(),getpid(),i);
                //buf[2047]='\n';
                //nx_write_log(NX_LEVEL_INFO,buf,2048);
                //_nx_log_info(buf,strlen(buf));
                NX_ASYN_LOG_INFO("%s",buf);
                sleep(1);
            }
            //count++;
        }
    }

    return 0;



    int j =0;
    for(j =0; j <1 ;j++){//多进程测试
        pid_t fpid; //fpid表示fork函数返回的值
        int count=0;
        fpid=fork();
        if(fpid == 0 || fpid <0){
            pid_t ppid = getppid();
            if(ppid != gpid){
                break;
            }

        }
        if (fpid < 0)
            printf("error in fork!");
        else if (fpid == 0) {
            printf("i am the child process, my process id is %d,ppid:%d/n",getpid());
            count++;
            char buf[1024] ={0};
            int i =0;
            sleep(1);
            for(i=0;i < 1 ; i++){
                memset(buf,'1',1024);
                sprintf(buf,"ppid:%d,child pid:%d-%d", getppid(),getpid(),i);
                //buf[1023]='\n';
                /*nx_write_log*/NX_SYN_LOG_INFO("%s",buf);
                exit(0);
                usleep(10);
            }
        } else {
            printf("i am the parent process, my process id is %d/n",getpid());
            char buf[1024] ={0};
            int i =0;
            for(i=1;i < 2 ; i++){
                memset(buf,'1',1024);
                sprintf(buf,"pareent pid:%d-%d", getpid(),i);
                /*nx_write_log*/NX_SYN_LOG_INFO("%s",buf);
                usleep(1);
            }
            count++;
        }
    }

    nx_logger_deinit(l);
}


char* test_get_thread_name()
{
    //    int = 0;
    //    pthread_t tid = pthread_self();
    //    for (; i<g_thread_num; i++)
    //    {
    //        if (tid == g_thread_info[i].tid)
    //        {
    //            return g_thread_info[i].name;
    //        }
    //    }

    //    char threadId[256] = {0};
    ////    char* threadName = get_thread_name();
    ////    if (!threadName)
    ////    {
    ////        sprintf(threadId, "%s/thread.%lld.log", forder, pthread_self());
    ////    } else {
    ////        sprintf(threadId, "%s/thread.%s.log", forder, threadName);
    ////    }
    ///

    return NULL;
}

