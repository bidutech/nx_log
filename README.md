# nx_log
<pre><code>

this is a High efficiency log 
process and thread safe c log  lib

sample:

void test()
{
    char * logpath ="/data1/home/test/log";
    nx_logger_init(1,NX_LEVEL_NULL,logpath,10);

    char *msg = logpath;

    while(1){
        NX_ASYN_LOG_INFO("%s",msg);
        NX_ASYN_LOG_ERROR("%s",msg);
        NX_ASYN_LOG_DEBUG("%s",msg);
        sleep(1);
    }
}
</code></pre>
