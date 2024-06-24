**常用功能模块接口说明**

1> log使用 (#include "basework/log.h")
  见 basework/tests/log_test.c

  a> 设置当前模块的log等级（#define CONFIG_LOGLEVEL LOGLEVEL_NOTICE）
  b> 设置当前模块打印前缀 （#define pr_fmt(fmt) "log_test: " fmt）
  
  上述为可选配置, 若未定义则使用全局配置（注意： a> b> 必须定义在 basework/log.h 之前）,共有6个打印等级如下：

    pr_dbg("This is a dbg-level message!");
    pr_info("This is a info-level message!");
    pr_notice("This is a notice-level message!");
    pr_warn("This is a warn-level message!");
    pr_err("This is a err-level message!");
    pr_emerg("This is a fatal-level message!");

    注意： 在PC端使用时需用初始化log, 如下将log 输出重定向到stdout
        static struct printer std_out;
        printf_format_init(&std_out);
        pr_log_init(&std_out);


2> 异步队列 (#include "basework/rq.h") 
    见 basework/tests/rq_test.c

    回调参数以指针形式传递
    rq_submit(                 @ 返回0表示成功
        void (*exec)(void *),  @ 用户回调
        void *arg,             @ 回调参数
    )


    回调参数会被复制到队列项中，以值形式传递
    rq_submit_cp(              @ 返回0表示成功
        void (*exec)(void *),  @ 用户回调
        void *data,            @ 回调参数
        size_t size            @ 参数大小
    )
    

    a> rq_submit: 提交用户操作到队列尾部 （普通事件）
    b> rq_submit_urgent: 提交用户操作到队列头 （紧急事件）
    c> rq_reset: 清空队列（所有未处理的操作都将丢弃）


3> 系统定时器 (#include "basework/os/osapi_timer.h") 
    见basework/tests/rq_test.c

    os_timer_create(                            @ 返回0表示成功
        os_timer_t *timer,                      @ 定时器handle 指针
        void (*expired_fn)(os_timer_t, void *), @ 用户超时回调
        void *arg,                              @ 回调参数
        bool isr_context                        @ 表示回调是否需要在中断上下文执行（一般情况下设为 false）
    );

    os_timer_mod: 修改定时器超时时间并重新增加到定时器队列
    os_timer_add：增加定时器到队列（需用户确保定时器不在队列中）
    os_timer_del: 停止定时器
    os_timer_destroy: 停止并销毁定时器

    1s 周期定时器用例()：
        static os_timer_t timer;
        static void timer_cb(os_timer_t timer, void *arg) {
            // 用户事件处理
            //..
            os_timer_mod(timer, 1000);
        }

        os_timer_create(&timer, timer_cb, NULL, false); //创建定时器
        os_timer_add(timer, 1000); //启动定时器



4> 系统异常 (#include "basework/os/osapi.h")

    os_panic: 主动触发系统异常


5> 内存分配接口 (#include "basework/malloc.h")
    general_malloc 等价于 malloc
    general_calloc 等价于 calloc
    general_free   等价于 free

    内存大小配置宏： CONFIG_BASEWORK_MEM_SIZE 默认为16 kb


6> 文件接口 (#include "basework/os/osapi_fs.h")

7> 环境变量

    fm-update   (yes/no)                固件是否更新   
    why-reboot  (normal/exception_xxx)  系统重启原因
    runtime     ()                      系统运行时间
    mac         (xx:xx:xx:xx:xx:xx)     MAC地址

8> 用户分区表(用户自己添加分区)

    basework/dev/partitions_cfg.c  

9> 块设备（#include "basework/dev/blkdev.h")

    测试例程： basework/tests/blkdev_test.cc
    块设备是flash设备的高级读写接口模块，自带读写缓存策略以及擦除操作，用户使用时只需关注读写数据。
    blkdev_read:  块设备读
    blkdev_write: 块设备写

10> 分区字节读写（#include "basework/dev/partition.h)

    使用blkdev读写，不需要考虑擦除和缓存
    测试例程： basework/tests/blkdev_test.cc
    lgpt_read:  逻辑分区读数据
    lgpt_write: 逻辑分区写数据

11> disklog格式化打印, disk表示重定向目标为flash

    测试例程： basework/tests/blkdev_test.cc
    npr_dbg(disk, "aaaaa")
    npr_info(disk, "aaaaa")
    npr_notice(disk, "aaaaa")
    npr_warn(disk, "aaaaa")
    npr_err(disk, "aaaaa")

12> disklog直接读取和写入

    测试例程： basework/tests/blkdev_test.cc
    disklog_input: flashlog 写入
    disklog_output: flashlog 读取（可多次读取，不会丢失)。 

13> 用户分区文件

    头文件： #include "basework/dev/partition_file.h"
    测试例程： basework/tests/partition_test.cc (非完全测试)
    当前二级分区表文件定义文件： basework/dev/paritions_usr_cfg.c
    usr_sfile_open: 打开一个用户分区文件（分区文件名规则： ${parent_parition_name}/${child_partition_name})。
    usr_sfile_close： 关闭文件（不关闭文件会导致文件错误及内存泄漏，具体是文件元数据丢失）
    usr_sfile_invalid: 清除文件内容
    usr_sfile_setoffset: 设置文件指针偏移
    usr_sfile_write: 写文件
    usr_sfile_read： 读文件
    usr_sfile_size： 获取文件大小
    注意： 分区文件的大小是固定的，由二级分区表定义，当读写超过文件固定的最大大小会直接返回错误。 
        不要同时打开多个同一分区文件（当前支持不完善），分区文件是带校验和的无需手动校验数据的完整性。


14> 天时分秒通知接口

    头文件： #include "basework/dev/clock_timer.h"
    测试例程： basework/dev/clock_timer_test.cc (非完全测试)
    超时注册接口：
    clock_callout_add(
        struct clock_callout *cc,  //超时回调实列
        void (*callout)(void),     //超时回调
        unsigned int seconds       //超时时间（单位：秒） 典型值：CLK_SEC_TIMEOUT， CLK_MIN_TIMEOUT，CLK_HOUR_TIMEOUT， CLK_DAY_TIMEOUT
    )
    clock_callout_del: 停止回调
    





  
