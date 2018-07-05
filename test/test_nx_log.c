/*************************************************************************
	> File Name: test_nx_log.c
	> Author: shanhai
	> Mail: shanhai
	> Created Time: 2018年07月05日 星期四 10时36分25秒
 ************************************************************************/

#include <stdio.h>
#include <nx_log.h>

int test_nx_log1();

int main(){

   test_nx_log1();
    printf("hello nx_log");
    return 0;
}


int test_nx_log1(){

    char * logpath ="/data1/home/mysql/twe/ofo_test/3001/log/";
    nx_logger_init(1,NX_LEVEL_NULL,logpath,10);

    char *msg = logpath;

    while(1){
        NX_ASYN_LOG_INFO("%s",msg);
        NX_ASYN_LOG_ERROR("%s",msg);
        NX_ASYN_LOG_DEBUG("%s",msg);
        sleep(1);
    }


    return 0;

    struct nx_logger *l = nx_logger_init(0,NX_LEVEL_INFO,logpath,10);

    /**********************Test code*******************/


    //    struct timeval tv;
    //    gettimeofday(&tv, NULL);
    //    char tempbuf[1024] = {0};
    //    nx_strftime(tempbuf, 1024, "%Y%m%d%H%M%S", localtime(&tv.tv_sec));
    //    printf("time fomate:%s\n",tempbuf);




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

