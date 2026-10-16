/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    SD storage module implementation
 */

#include "sd_storage.h"
#include "mqtt_client.h"    /* 补传需要读 MQTT 状态 + 复用 CAN 发布接口 */
#include <rtdevice.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dfs_fs.h>
#include <sys/stat.h>   /* mkdir：实现在 rt-thread 的 dfs_posix.c 里 */
#include <board.h>      /* 诊断用：直接读 SDIO / DMA 寄存器，需要 CMSIS 定义 */

#define DBG_TAG "sd_storage"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include <sys/statfs.h>

/* SD卡存储状态 */
static sd_status_t sd_status = SD_STATUS_UNMOUNTED;

/* sdtest 的进度。写到 OLED 上而不是只打串口：
 * 屏幕内容不会因为死机被清掉，所以这是"死在哪个阶段"最可靠的线索。 */
char g_sdtest_stage[16] = "idle";

static void sdtest_set_stage(const char *s)
{
    rt_uint32_t i = 0;

    for (; s[i] != '\0' && i < sizeof(g_sdtest_stage) - 1u; i++)
    {
        g_sdtest_stage[i] = s[i];
    }
    g_sdtest_stage[i] = '\0';
}

/* SD卡存储线程 */
static rt_thread_t sd_storage_thread = RT_NULL;

/* 存储缓冲区 */
static sd_storage_record_t sd_buffer[SD_BUFFER_SIZE];
static rt_uint32_t sd_buffer_count = 0;

/* 缓冲区访问互斥锁 */
static rt_mutex_t sd_buffer_lock = RT_NULL;

/* 数据可用信号量 */
static rt_sem_t sd_data_sem = RT_NULL;

/* 数据文件 + 游标文件的互斥（补传线程和落盘线程共用） */
static rt_mutex_t sd_file_lock = RT_NULL;

/* 已补传的记录条数（相对数据文件开头的条数） */
static rt_uint32_t sd_read_seq = 0;

/* 补传线程 */
static rt_thread_t sd_backfill_thread = RT_NULL;

/* SD卡存储线程优先级 */
#define SD_STORAGE_THREAD_PRIORITY 14

/* SD卡存储线程堆栈大小 */
#define SD_STORAGE_THREAD_STACK_SIZE 4096

/* 文件操作超时 */
#define FILE_OP_TIMEOUT 5000

/* ---- 断网续传（补传）相关 ---- */
#define SD_CURSOR_FILE          "/sd/can_cursor.bin"    /* 已补传条数游标（4 字节） */
#define SD_BACKFILL_BATCH       32                      /* 每批补传条数 */
#define SD_BACKFILL_PACE_MS     30                      /* 每条之间的间隔（限速） */
#define SD_BACKFILL_POLL_MS     2000                    /* 无积压时的轮询周期 */

/**
 * 挂载SD卡
 */
static int mount_sd_card(void)
{
    struct statfs stat;
    int ret;

    LOG_I("Mounting SD card");

    /* 检查SD卡是否已经挂载 */
    if (statfs(SD_MOUNT_POINT, &stat) == 0)
    {
        LOG_I("SD card already mounted at %s", SD_MOUNT_POINT);
        sd_status = SD_STATUS_MOUNTED;
        return 0;
    }

    /* 这个工程没有根文件系统，"根目录下的子目录"建不出来（mkdir 会静默失败），
     * 而 dfs_mount 会检查挂载点目录是否存在——所以挂不到 /sd。
     * 解法：把卡挂到 "/"（dfs_mount 对 "/" 是特例，不检查目录，见 dfs_fs.c 的
     * dfs_mount），挂上之后再 mkdir /sd，这时目录才是真正建在卡上的。
     *
     * 另外 RT-Thread 的 mkfs 用的是 FM_ANY|FM_SFD（超级软盘格式，不带分区表），
     * 所以卡上只有整盘设备 "sd"、不会有 sd0——设备名直接用 "sd"。 */
    ret = dfs_mount("sd", "/", "elm", 0, 0);
    if (ret != 0)
    {
        LOG_E("Failed to mount SD card on / (ret=%d)", ret);
        sd_status = SD_STATUS_ERROR;
        return -1;
    }

    /* 挂载成功后建数据目录（这一步是真正写到卡上的） */
    mkdir(SD_MOUNT_POINT, 0);

    LOG_I("SD card mounted at / , dir %s ready", SD_MOUNT_POINT);
    sd_status = SD_STATUS_MOUNTED;
    return 0;
}

/**
 * 卸载SD卡
 */
static int unmount_sd_card(void)
{
    int ret;

    LOG_I("Unmounting SD card");

    ret = dfs_unmount("/");   /* 卡挂在根目录上，卸载点也是 / */
    if (ret == 0)
    {
        LOG_I("SD card unmounted successfully");
        sd_status = SD_STATUS_UNMOUNTED;
        return 0;
    }
    else
    {
        LOG_W("Failed to unmount SD card: %d", ret);
        return -1;
    }
}

/**
 * 初始化SD卡文件
 */
static int init_sd_files(void)
{
    int fd;

    /* 创建数据文件 */
    fd = open(SD_DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        LOG_E("Failed to create/open data file: %s", SD_DATA_FILE);
        return -1;
    }

    close(fd);
    LOG_D("Data file initialized: %s", SD_DATA_FILE);

    return 0;
}

/**
 * 数据文件里的记录条数
 */
static rt_uint32_t sd_file_records(void)
{
    struct stat st;

    if (stat(SD_DATA_FILE, &st) != 0)
    {
        return 0;
    }
    return (rt_uint32_t)(st.st_size / sizeof(sd_storage_record_t));
}

/**
 * 读补传游标（没有 / 读失败都当 0）
 */
static rt_uint32_t sd_cursor_load(void)
{
    int fd;
    rt_uint32_t v = 0;
    rt_size_t rsize;

    fd = open(SD_CURSOR_FILE, O_RDONLY, 0644);
    if (fd >= 0)
    {
        rsize = read(fd, &v, sizeof(v));
        if (rsize != sizeof(v))
        {
            v = 0;
        }
        close(fd);
    }
    return v;
}

/**
 * 写补传游标（失败只提示，不致命）
 */
static void sd_cursor_save(rt_uint32_t v)
{
    int fd;
    rt_size_t wsize;

    fd = open(SD_CURSOR_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        LOG_W("open cursor file failed");
        return;
    }
    wsize = write(fd, &v, sizeof(v));
    if (wsize != sizeof(v))
    {
        LOG_W("write cursor failed");
    }
    close(fd);
}

/**
 * 挂载完成后的收尾：建数据文件 + 加载补传游标
 */
static void sd_after_mount(void)
{
    init_sd_files();

    /* 游标和文件对不上（比如文件被清过）就从头来：宁可重发，不丢 */
    sd_read_seq = sd_cursor_load();
    if (sd_read_seq > sd_file_records())
    {
        sd_read_seq = 0;
    }
    LOG_I("SD ready, backfill cursor %u/%u",
          (unsigned)sd_read_seq, (unsigned)sd_file_records());
}

/**
 * 将缓冲区数据写入SD卡
 */
static int flush_buffer_to_sd(void)
{
    int fd;
    rt_size_t written;
    rt_uint32_t i;

    if (sd_buffer_count == 0)
    {
        return 0;
    }

    if (sd_status != SD_STATUS_MOUNTED)
    {
        LOG_W("SD card not mounted, cannot flush buffer");
        return -1;
    }

    /* 锁顺序固定：先文件锁、再缓冲区锁（补传线程只拿文件锁，避免死锁） */
    rt_mutex_take(sd_file_lock, RT_WAITING_FOREVER);

    /* 获取互斥锁 */
    rt_mutex_take(sd_buffer_lock, RT_WAITING_FOREVER);

    /* 打开文件进行写操作 */
    fd = open(SD_DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        LOG_E("Failed to open data file for writing");
        rt_mutex_release(sd_buffer_lock);
        rt_mutex_release(sd_file_lock);
        return -1;
    }

    /* 写入所有缓冲区数据 */
    for (i = 0; i < sd_buffer_count; i++)
    {
        written = write(fd, &sd_buffer[i], sizeof(sd_storage_record_t));
        if (written != sizeof(sd_storage_record_t))
        {
            LOG_W("Failed to write record %d to SD card", i);
            break;
        }
    }

    close(fd);

    LOG_I("Flushed %d records to SD card", i);

    /* 清空缓冲区 */
    sd_buffer_count = 0;

    rt_mutex_release(sd_buffer_lock);
    rt_mutex_release(sd_file_lock);

    return i;
}

/**
 * SD卡存储线程
 */
static void sd_storage_thread_entry(void *param)
{
    rt_err_t err;
    rt_uint32_t flush_interval = 10000;  /* 10秒 */

    LOG_I("SD storage thread started");

    /* 挂载SD卡 */
    if (mount_sd_card() != 0)
    {
        LOG_E("Failed to mount SD card at startup");
        /* 可选：继续运行，等待SD卡插入 */
    }

    /* 初始化SD文件 + 加载补传游标 */
    if (sd_status == SD_STATUS_MOUNTED)
    {
        sd_after_mount();
    }

    /* 主循环 */
    while (1)
    {
        /* 等待数据可用或超时 */
        err = rt_sem_take(sd_data_sem, rt_tick_from_millisecond(flush_interval));

        /* 周期性或有数据时刷新缓冲区 */
        if (sd_status == SD_STATUS_MOUNTED && sd_buffer_count > 0)
        {
            flush_buffer_to_sd();
        }
        else if (sd_status == SD_STATUS_ERROR || sd_status == SD_STATUS_UNMOUNTED)
        {
            /* 尝试重新挂载SD卡 */
            if (mount_sd_card() == 0)
            {
                sd_after_mount();
                flush_buffer_to_sd();
            }
        }
    }
}

/**
 * 获取积压（已备份到 SD 卡、还没补传上云）的记录条数
 */
rt_uint32_t sd_storage_get_backlog(void)
{
    rt_uint32_t total, backlog;

    if (sd_status != SD_STATUS_MOUNTED)
    {
        return 0;
    }

    rt_mutex_take(sd_file_lock, RT_WAITING_FOREVER);
    total = sd_file_records();
    backlog = (total > sd_read_seq) ? (total - sd_read_seq) : 0;
    rt_mutex_release(sd_file_lock);

    return backlog;
}

/**
 * 把积压记录补传上云，返回本轮补传的条数。
 * 规则：发一条、游标推进一条；中途断开就停下（下次继续）；
 *       全部补完后把数据文件清掉、游标归零。
 */
static rt_uint32_t sd_backfill_drain(void)
{
    static sd_storage_record_t batch[SD_BACKFILL_BATCH];
    rt_uint32_t sent = 0;

    while (mqtt_client_get_status() == MQTT_STATUS_CONNECTED)
    {
        rt_uint32_t total, n, i;
        int fd;

        rt_mutex_take(sd_file_lock, RT_WAITING_FOREVER);
        total = sd_file_records();

        if (sd_read_seq >= total)
        {
            /* 全部补完：清数据文件、游标归零（在锁里做，防止和落盘线程抢） */
            if (total > 0)
            {
                unlink(SD_DATA_FILE);
                sd_read_seq = 0;
                sd_cursor_save(0);
                LOG_I("backlog drained, data file reset");
            }
            rt_mutex_release(sd_file_lock);
            break;
        }

        /* 从游标位置取一批 */
        fd = open(SD_DATA_FILE, O_RDONLY, 0644);
        if (fd < 0)
        {
            rt_mutex_release(sd_file_lock);
            LOG_W("backfill: open data file failed");
            break;
        }
        (void)lseek(fd, (off_t)(sd_read_seq * (rt_uint32_t)sizeof(sd_storage_record_t)), SEEK_SET);

        n = total - sd_read_seq;
        if (n > SD_BACKFILL_BATCH)
        {
            n = SD_BACKFILL_BATCH;
        }
        n = (rt_uint32_t)read(fd, batch, n * sizeof(sd_storage_record_t)) / sizeof(sd_storage_record_t);
        close(fd);
        rt_mutex_release(sd_file_lock);

        if (n == 0)
        {
            break;
        }

        /* 锁外逐条发布（限速，别和实时转发抢 AT 通道） */
        for (i = 0; i < n; i++)
        {
            if (mqtt_client_get_status() != MQTT_STATUS_CONNECTED)
            {
                break;
            }
            if (mqtt_client_publish_can_data(batch[i].can_id, batch[i].data, batch[i].dlc) != 0)
            {
                break;
            }

            rt_mutex_take(sd_file_lock, RT_WAITING_FOREVER);
            sd_read_seq++;
            rt_mutex_release(sd_file_lock);
            sent++;

            rt_thread_mdelay(SD_BACKFILL_PACE_MS);
        }

        /* 每批推进一次游标文件（断电最多重发一批） */
        rt_mutex_take(sd_file_lock, RT_WAITING_FOREVER);
        sd_cursor_save(sd_read_seq);
        rt_mutex_release(sd_file_lock);

        if (i < n)
        {
            break;      /* 中途断开，等下次连接再继续 */
        }
    }

    return sent;
}

/**
 * 补传线程：MQTT 恢复连接后，把积压的数据批量补上云（断网续传）
 */
static void sd_backfill_thread_entry(void *param)
{
    LOG_I("SD backfill thread started");

    while (1)
    {
        if (sd_status == SD_STATUS_MOUNTED &&
            mqtt_client_get_status() == MQTT_STATUS_CONNECTED &&
            sd_storage_get_backlog() > 0)
        {
            rt_uint32_t sent = sd_backfill_drain();

            if (sent > 0)
            {
                LOG_I("backfill round done: %u records", (unsigned)sent);
            }
        }

        rt_thread_mdelay(SD_BACKFILL_POLL_MS);
    }
}

/**
 * 初始化SD卡存储模块
 */
int sd_storage_init(void)
{
    rt_err_t err;

    LOG_I("Initializing SD storage module");

    /* 创建互斥锁 */
    if (sd_buffer_lock == RT_NULL)
    {
        sd_buffer_lock = rt_mutex_create("sd_buffer_lock", RT_IPC_FLAG_FIFO);
        if (sd_buffer_lock == RT_NULL)
        {
            LOG_E("Failed to create buffer lock");
            return -1;
        }
    }

    /* 创建文件互斥锁（数据文件 + 游标文件共用） */
    if (sd_file_lock == RT_NULL)
    {
        sd_file_lock = rt_mutex_create("sd_file_lock", RT_IPC_FLAG_FIFO);
        if (sd_file_lock == RT_NULL)
        {
            LOG_E("Failed to create file lock");
            return -1;
        }
    }

    /* 创建信号量 */
    if (sd_data_sem == RT_NULL)
    {
        sd_data_sem = rt_sem_create("sd_data_sem", 0, RT_IPC_FLAG_FIFO);
        if (sd_data_sem == RT_NULL)
        {
            LOG_E("Failed to create data semaphore");
            return -1;
        }
    }

    /* 初始化缓冲区 */
    sd_buffer_count = 0;
    rt_memset(sd_buffer, 0, sizeof(sd_buffer));

    LOG_I("SD storage module initialized successfully");
    return 0;
}

/**
 * 启动SD卡存储线程
 */
int sd_storage_start(void)
{
    rt_err_t err;

    if (sd_storage_thread != RT_NULL)
    {
        LOG_W("SD storage thread already started");
        return 0;
    }

    /* 创建存储线程 */
    sd_storage_thread = rt_thread_create(
        "sd_storage",
        sd_storage_thread_entry,
        RT_NULL,
        SD_STORAGE_THREAD_STACK_SIZE,
        SD_STORAGE_THREAD_PRIORITY,
        10
    );

    if (sd_storage_thread == RT_NULL)
    {
        LOG_E("Failed to create SD storage thread");
        return -1;
    }

    err = rt_thread_startup(sd_storage_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start SD storage thread: %d", err);
        return -1;
    }

    /* 创建补传线程（断网续传；优先级略低于落盘线程） */
    if (sd_backfill_thread == RT_NULL)
    {
        sd_backfill_thread = rt_thread_create("sd_backfill",
                                              sd_backfill_thread_entry,
                                              RT_NULL,
                                              3072,
                                              SD_STORAGE_THREAD_PRIORITY + 1,
                                              10);
        if (sd_backfill_thread == RT_NULL)
        {
            LOG_W("Failed to create SD backfill thread");
        }
        else
        {
            err = rt_thread_startup(sd_backfill_thread);
            if (err != RT_EOK)
            {
                LOG_W("Failed to start SD backfill thread: %d", err);
            }
        }
    }

    LOG_I("SD storage thread started successfully");
    return 0;
}

/**
 * 保存CAN数据到SD卡
 */
int sd_storage_save_record(sd_storage_record_t *record)
{
    rt_err_t err;

    if (record == RT_NULL)
    {
        return -1;
    }

    /* 获取互斥锁 */
    err = rt_mutex_take(sd_buffer_lock, RT_WAITING_FOREVER);
    if (err != RT_EOK)
    {
        return -1;
    }

    /* 检查缓冲区是否满 */
    if (sd_buffer_count >= SD_BUFFER_SIZE)
    {
        LOG_W("SD buffer full, data will be lost");
        rt_mutex_release(sd_buffer_lock);
        return -1;
    }

    /* 添加到缓冲区 */
    rt_memcpy(&sd_buffer[sd_buffer_count], record, sizeof(sd_storage_record_t));
    sd_buffer_count++;

    rt_mutex_release(sd_buffer_lock);

    /* 释放信号量，通知存储线程 */
    if (sd_buffer_count >= SD_BUFFER_SIZE / 2)  /* 缓冲区半满时 */
    {
        rt_sem_release(sd_data_sem);
    }

    return 0;
}

/**
 * 保存多条CAN数据到SD卡
 */
int sd_storage_save_batch(sd_storage_record_t *records, rt_uint32_t count)
{
    rt_uint32_t i;
    int ret;

    if (records == RT_NULL || count == 0)
    {
        return 0;
    }

    /* 逐条保存记录 */
    for (i = 0; i < count; i++)
    {
        ret = sd_storage_save_record(&records[i]);
        if (ret != 0)
        {
            LOG_W("Failed to save record %d", i);
            break;
        }
    }

    return i;
}

/**
 * 读取SD卡中的数据
 */
int sd_storage_read_records(sd_storage_record_t *records, rt_uint32_t max_count)
{
    int fd;
    rt_size_t read_size;
    rt_uint32_t count = 0;

    if (records == RT_NULL || max_count == 0)
    {
        return 0;
    }

    if (sd_status != SD_STATUS_MOUNTED)
    {
        LOG_W("SD card not mounted");
        return 0;
    }

    /* 打开数据文件 */
    fd = open(SD_DATA_FILE, O_RDONLY, 0644);
    if (fd < 0)
    {
        LOG_W("Failed to open data file for reading");
        return 0;
    }

    /* 读取记录 */
    while (count < max_count)
    {
        read_size = read(fd, &records[count], sizeof(sd_storage_record_t));
        if (read_size != sizeof(sd_storage_record_t))
        {
            break;
        }
        count++;
    }

    close(fd);

    LOG_I("Read %d records from SD card", count);

    return count;
}

/**
 * 获取SD卡状态
 */
sd_status_t sd_storage_get_status(void)
{
    return sd_status;
}

/**
 * 检查SD卡是否已挂载
 */
rt_bool_t sd_storage_is_mounted(void)
{
    return (sd_status == SD_STATUS_MOUNTED) ? RT_TRUE : RT_FALSE;
}

/**
 * 获取缓冲区中待保存的记录数
 */
rt_uint32_t sd_storage_get_buffer_count(void)
{
    rt_uint32_t count;

    rt_mutex_take(sd_buffer_lock, RT_WAITING_FOREVER);
    count = sd_buffer_count;
    rt_mutex_release(sd_buffer_lock);

    return count;
}

/**
 * 停止SD卡存储模块
 */
int sd_storage_stop(void)
{
    /* 刷新所有缓冲区数据到SD卡 */
    if (sd_status == SD_STATUS_MOUNTED)
    {
        flush_buffer_to_sd();
    }

    /* 卸载SD卡 */
    unmount_sd_card();

    LOG_I("SD storage module stopped");
    return 0;
}

#ifdef FINSH_USING_MSH
/**
 * sdtest —— SD 卡诊断：读 0 号扇区并打印关键字段
 *
 * 用来区分三种情况：
 *   1) 签名 55AA 存在、偏移 0x52 处是 "FAT32" → 整卡一个 FAT 卷，文件系统没问题
 *   2) 偏移 0x1BE 有合法分区表项（type != 0）→ 卡有 MBR，必须挂 sd0 而不是 sd
 *   3) 全 0 / 全 FF / 两次读结果不一致 → SDIO 的读数据通路有问题
 *
 * 特意读两次：一次用 32 字节对齐的缓冲，一次故意错开 1 字节。驱动对非对齐缓冲会走
 * 内部 cache_buf 中转（rthw_sdio_transfer_by_dma），而 FatFs 自己的扇区缓冲不保证
 * 32 字节对齐——如果只有非对齐那次出错，就正好解释了"卡认得到、但挂不上"。
 */
/* 诊断工作线程：单独线程里做，这样即使读扇区卡住，msh 仍然可用，
 * 可以用 list thread 看到这个线程停在什么状态（suspend=在等 IPC，ready=在空转） */
static void sdtest_worker(void *param)
{
    static rt_uint8_t pool_a[512 + 64];
    static rt_uint8_t pool_b[512 + 64];
    rt_uint8_t *buf_a = (rt_uint8_t *)(((rt_ubase_t)pool_a + 31u) & ~(rt_ubase_t)31u);
    rt_uint8_t *buf_b = (rt_uint8_t *)(((rt_ubase_t)pool_b + 31u) & ~(rt_ubase_t)31u) + 1;
    struct rt_device_blk_geometry geo;
    rt_device_t dev;
    rt_size_t n_a;
    rt_size_t n_b;
    int i;
    int diff = 0;

    RT_UNUSED(param);

    dev = rt_device_find("sd");
    if (dev == RT_NULL)
    {
        rt_kprintf("sdtest: device 'sd' not found\n");
        sdtest_set_stage("no-device");
        return;
    }

    if (rt_device_open(dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("sdtest: open failed\n");
        sdtest_set_stage("open-fail");
        return;
    }

    rt_memset(&geo, 0, sizeof(geo));
    sdtest_set_stage("open-ok");

    if (rt_device_control(dev, RT_DEVICE_CTRL_BLK_GETGEOME, &geo) == RT_EOK)
    {
        rt_kprintf("sdtest: %u bytes/sector, %u sectors (~%u MB)\n",
                   (unsigned)geo.bytes_per_sector,
                   (unsigned)geo.sector_count,
                   (unsigned)(geo.sector_count / 2048u));
        sdtest_set_stage("geo-ok");
    }

    rt_memset(buf_a, 0, 512);
    rt_memset(buf_b, 0, 512);

    /* 哨兵：如果 DMA 越界写（传输长度不对），buf_a 后面这 64 字节会被改掉 */
    for (i = 0; i < 64; i++)
    {
        buf_a[512 + i] = 0x5A;
    }

    rt_kprintf("[sdtest] pre : SDIO STA=%08X DCOUNT=%08X DCTRL=%08X\n",
               (unsigned)SDIO->STA, (unsigned)SDIO->DCOUNT, (unsigned)SDIO->DCTRL);
    /* F4 的每个流只有 CR/NDTR/PAR/M0AR/FCR，中断标志在 DMA2 的 LISR/HISR 里
     * （Stream3 属于低 4 个流 → LISR，bit 对应 TEIF3/TCIF3 等） */
    rt_kprintf("[sdtest] pre : DMA2_S3 CR=%08X NDTR=%08X DMA2 LISR=%08X HISR=%08X\n",
               (unsigned)DMA2_Stream3->CR, (unsigned)DMA2_Stream3->NDTR,
               (unsigned)DMA2->LISR, (unsigned)DMA2->HISR);

    rt_kprintf("[sdtest] step1: read 1 block (32-byte aligned buffer) ...\n");
    sdtest_set_stage("rd1");
    /* 给 OLED 线程留时间把这行刷到屏幕上（它 500ms 一轮）。万一读扇区把系统搞死，
     * 屏幕上就留得住"死在 rd1"这条线索——否则阶段名还没显示就卡住了。 */
    rt_thread_mdelay(1200);
    /* 注意：块设备的 size 单位是"块"，不是字节！
     * rt_mmcsd_read() 内部是 rd_ptr + (req_size << 9)，也就是按块数×512 字节算的。
     * 之前这里误传 512（以为是 512 字节），实际变成 512 块 = 256KB，
     * DMA 直接灌爆 512 字节的缓冲区，把内存踩烂导致整机死机。 */
    n_a = rt_device_read(dev, 0, buf_a, 1);
    sdtest_set_stage("rd1-ok");

    {
        int bad = 0;

        for (i = 0; i < 64; i++)
        {
            if (buf_a[512 + i] != 0x5A)
            {
                bad = 1;
                break;
            }
        }
        sdtest_set_stage(bad ? "canary-BAD" : "canary-ok");
        rt_kprintf("sdtest: canary %s\n", bad ? "BAD - DMA overran the buffer!" : "ok");
        rt_thread_mdelay(800);
    }

    rt_kprintf("[sdtest] step2: read 1 block (deliberately unaligned buffer) ...\n");
    sdtest_set_stage("rd2");
    rt_thread_mdelay(1200);
    n_b = rt_device_read(dev, 0, buf_b, 1);   /* 1 块 = 512 字节 */
    sdtest_set_stage("rd2-ok");

    rt_kprintf("sdtest: aligned=%d blocks, unaligned=%d blocks (expect 1 and 1)\n",
               (int)n_a, (int)n_b);

    for (i = 0; i < 512; i++)
    {
        if (buf_a[i] != buf_b[i])
        {
            diff++;
        }
    }
    rt_kprintf("sdtest: diff between the two reads = %d\n", diff);

    rt_kprintf("sdtest: [0..15] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
               buf_a[0], buf_a[1], buf_a[2], buf_a[3], buf_a[4], buf_a[5], buf_a[6], buf_a[7],
               buf_a[8], buf_a[9], buf_a[10], buf_a[11], buf_a[12], buf_a[13], buf_a[14], buf_a[15]);

    rt_kprintf("sdtest: sig[510,511] = %02X %02X %s\n",
               buf_a[510], buf_a[511],
               (buf_a[510] == 0x55 && buf_a[511] == 0xAA) ? "(55AA ok)" : "(NO 55AA!)");
    sdtest_set_stage((buf_a[510] == 0x55 && buf_a[511] == 0xAA) ? "sig-55AA" : "sig-none");

    rt_kprintf("sdtest: @0x52 = %c%c%c%c%c%c%c%c (should be FAT32)\n",
               buf_a[0x52], buf_a[0x53], buf_a[0x54], buf_a[0x55],
               buf_a[0x56], buf_a[0x57], buf_a[0x58], buf_a[0x59]);

    rt_kprintf("sdtest: MBR[0x1BE] flag=%02X type=%02X lba=%u size=%u\n",
               buf_a[0x1BE], buf_a[0x1BE + 4],
               (unsigned)(buf_a[0x1BE + 8] | (buf_a[0x1BE + 9] << 8) |
                          (buf_a[0x1BE + 10] << 16) | ((rt_uint32_t)buf_a[0x1BE + 11] << 24)),
               (unsigned)(buf_a[0x1BE + 12] | (buf_a[0x1BE + 13] << 8) |
                          (buf_a[0x1BE + 14] << 16) | ((rt_uint32_t)buf_a[0x1BE + 15] << 24)));

    rt_kprintf("sdtest: BPB bytes/sector=%u sectors/cluster=%u\n",
               (unsigned)(buf_a[11] | (buf_a[12] << 8)), (unsigned)buf_a[13]);

    rt_device_close(dev);
    sdtest_set_stage("done");
}

/**
 * sdtest —— 启动工作线程做 SD 读诊断（命令本身立刻返回，不占着 msh）
 */
static int sdtest(int argc, char **argv)
{
    rt_thread_t tid;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    tid = rt_thread_create("sdtest", sdtest_worker, RT_NULL, 4096, 22, 10);
    if (tid == RT_NULL)
    {
        rt_kprintf("sdtest: create worker thread failed\n");
        return -1;
    }
    rt_thread_startup(tid);

    rt_kprintf("sdtest v2: worker started (prio 22, stack 4096)\n");
    rt_kprintf("sdtest v2: watch the OLED first line for the stage (SD:xxx)\n");
    return 0;
}
/**
 * sdstat —— 看 SD 备份/补传状态：积压多少条、游标在哪
 */
static int sdstat(int argc, char **argv)
{
    rt_uint32_t total, backlog;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_mutex_take(sd_file_lock, RT_WAITING_FOREVER);
    total = sd_file_records();
    backlog = (total > sd_read_seq) ? (total - sd_read_seq) : 0;
    rt_mutex_release(sd_file_lock);

    rt_kprintf("sd: %s\n",
               sd_status == SD_STATUS_MOUNTED ? "mounted" :
               (sd_status == SD_STATUS_ERROR ? "error" : "unmounted"));
    rt_kprintf("buffer %u/%u, data file %u records, cursor %u, backlog %u\n",
               (unsigned)sd_buffer_count, (unsigned)SD_BUFFER_SIZE,
               (unsigned)total, (unsigned)sd_read_seq, (unsigned)backlog);
    return 0;
}
MSH_CMD_EXPORT(sdstat, show SD backup and backfill status);

MSH_CMD_EXPORT(sdtest, dump sd sector 0 to diagnose mount failure);
#endif /* FINSH_USING_MSH */
