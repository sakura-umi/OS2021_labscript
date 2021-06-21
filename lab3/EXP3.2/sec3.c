/**
 * @file sec3.c
 * @author Sakura (yukinon@srpr.cc)
 * @brief OSLab3.3
 * @version 1.4
 * @date 2021-05-19
 *
 * @copyright Copyright (c) 2021
 *
 */

// 定义debug宏, 方便排查错误
#define debug(__T) printk("--test%d--\n", __T);
// 获取两个隐藏的函数的地址
#define FOLLOW_PAGE_ADDRESS 0xffffffffa1e8e660
#define PAGE_REFERENCED_ADDRESS 0xffffffffa1ea9840
// 通过宏定义这两个函数, 让他们能被正确调用
#define FOLLOW_PAGE_FUNC(_VMAREA_, _ADDR_, _FLAGS_) ((struct page* (*)(struct vm_area_struct*, unsigned long, unsigned int))FOLLOW_PAGE_ADDRESS)(_VMAREA_, _ADDR_, _FLAGS_)
#define PAGE_REFERENCED_FUNC(_PAGE_, _LOCKED_, _MEMCG_, _VMFLAGS_) ((int (*)(struct page*, int, struct mem_cgroup*, unsigned long*))PAGE_REFERENCED_ADDRESS)(_PAGE_, _LOCKED_, _MEMCG_, _VMFLAGS_)
// sysfs 控制启动停止的信号
#define SYSFS_TEST_RUN_STOP 0
#define SYSFS_TEST_RUN_START 1
// 字符串数组的最大长度
#define CHAR_MAX_LENTH 2048

// 头文件
// page, vma等需要
#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/errno.h>
// file system需要
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/freezer.h>
// 模块必备三个头文件
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
// pid操作引入
#include <linux/pid.h>
// 定时器操作引入
#include <linux/timer.h>
#include <linux/jiffies.h>
// 字符串操作引入
#include <linux/string.h>
// proc file system引入
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/signal.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
// 获取物理地址引入
#include <asm/page.h>

// 许可证信息
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sakura");
MODULE_DESCRIPTION("Lab3.3!");
MODULE_VERSION("1.4");

typedef struct phys_infos
{
    int length;
    char phys[CHAR_MAX_LENTH][256];
}phys_info;

// 定义结构体变量, 存储虚拟地址和物理地址的对应信息
phys_info physinfo;

// /sys/kerbel/mm/kmscan/pid
// 全局的pid, 用于获取用户向system输入的pid值
static unsigned int pid = 0;

/*
 * Procfs Module
 * Procfs模块
 * 构建Procfs内文件
 * 承载输出
 */

// 设置系统目录和初值
static struct proc_dir_entry* proc_test = NULL;
static struct proc_dir_entry* proc_test_root = NULL;
static struct proc_dir_entry* proc_temp = NULL;
// 此处用来保存一些特殊目录(如kmscan)等, 在停止后或者函数结束后进行删除等特殊操作. 防止remove函数修改proc_dir_entry指针
// proc_test_const[pid]保存指向对应pid的文件夹指针, 若文件夹没有删除重建则不再更改.
static struct proc_dir_entry *proc_test_const[32767] = {NULL};
const struct proc_dir_entry *proc_test_root_const;
// 文件夹锁, 1--存在文件夹; 0--不存在文件夹. 若存在文件夹则无法继续创建文件夹.
static int mutex[32768] = { 0 };

// 此处是proc_fs的open/show方法, 当用户使用cat查看时会调用
// func=1/2的show方法，如果使用cat指令查看则触发该函数
static int test_show(struct seq_file* m, void* v)
{
    int* pid_n = (int*)m->private;
    if (pid_n != NULL)
    {
        seq_printf(m, "%d\n", *pid_n);
    }
    return 0;
}
// func=3的show方法，如果使用cat指令查看则触发该函数, 读取physinfo结构体中的物理地址信息并打印.
static int phys_show(struct seq_file* m, void* v)
{
    int temp = 0;
    for(; temp < physinfo.length; temp++)
    {
        seq_printf(m, "%s\n", physinfo.phys[temp]);
    }
    return 0;
}
// func=1/2的open方法
static int test_open(struct inode* inode, struct file* file)
{
    return single_open(file, test_show, PDE_DATA(inode));
}
// func=3的open方法
static int phys_open(struct inode* inode, struct file* file)
{
    return single_open(file, phys_show, PDE_DATA(inode));
}

struct proc_ops test_ops = {
    .proc_open = test_open,
    .proc_read = seq_read,
    .proc_release = single_release,
}; //此处也可以增加其他方法
// 增加func=3时专用的方法，传入物理地址信息
struct proc_ops phys_ops = {
    .proc_open = phys_open,
    .proc_read = seq_read,
    .proc_release = single_release,
};

/*
 * Sysfs Module
 * Sysfs模块
 * 获取输入信息
 * 执行函数功能
 */
// /sys/kerbel/mm/sysfs_test/func
static unsigned int sysfs_test_func = 0;
//  /sys/kernel/mm/sysfs_test/sysfs_test_run
static unsigned int sysfs_test_run = SYSFS_TEST_RUN_STOP;
//  /sys/kernel/mm/sysfs_test/sleep_millisecs
static unsigned int sysfs_test_thread_sleep_millisecs = 20000;

static struct task_struct* sysfs_test_thread;

static DECLARE_WAIT_QUEUE_HEAD(sysfs_test_thread_wait);

static DEFINE_MUTEX(sysfs_test_thread_mutex);

static int sysfs_testd_should_run(void)
{
    return (sysfs_test_run & SYSFS_TEST_RUN_START);
}

// 三个func对应的函数
// lab3.1 统计vma数量
static void print_vma_count(void)
{
    // 此处控制后续文件删除的操作, 指向目标文件.
    static struct proc_dir_entry* de = NULL;
	// 实验要求的vma计数
    int vma_count = 0;
    // pid转char型的中间变量
	char pid_char[7] = {0};
	sprintf(pid_char, "%d", pid);
    // 创建task_struct, vma等变量
	struct task_struct* target_task;
	struct vm_area_struct* vma_ptr;

    // 如果对应pid的文件夹锁 = 0, 说明该pid对应的文件夹不存在, 创建文件夹, 存在文件夹则跳过(针对多次运行该函数而言)
    if(mutex[pid] == 0)//de == NULL && proc_test == NULL
    {
		proc_temp = (struct proc_dir_entry *)proc_test_root_const;
        proc_test = proc_mkdir(pid_char, proc_temp);
		// 存储指向对应pid文件夹指针, 后续除非删除重建该文件夹则不再更改.
        proc_test_const[pid] = proc_test;
        mutex[pid] = 1;// 将该pid对应的文件夹锁置1
    }
	// 寻找该pid的task_struct.
    target_task = pid_task(find_vpid(pid), PIDTYPE_PID);
	vma_ptr = target_task->mm->mmap;

	// 如果vma_ptr为NULL, 跳出循环.
	while(vma_ptr)
	{
		vma_count++;
		vma_ptr = vma_ptr->vm_next;
	}

	// 文件指针标记不为空则删除, 并在后面重新创建, 达到编辑该文件值的目的.
    if(de != NULL);
    {
        proc_remove(de);
    }
	proc_temp = (struct proc_dir_entry *)proc_test_const[pid];
    de = proc_create_data("vma_count", 0664, proc_temp, &test_ops, &vma_count);
}
// lab3.2 统计文件/匿名页, 和其中活跃的页个数.
static void print_active_inactive_page_count(void)
{
	// 定义基础变量
	int file = 0, active_file = 0, anon = 0, active_anon = 0;
    unsigned long addr;
    unsigned long vmflags;
	struct task_struct* target_task;
	struct vm_area_struct* vma_ptr;
    struct page* pages;
	char pid_char[7] = {0};
	sprintf(pid_char, "%d", pid);

	// 判断对应pid的文件夹锁是否为1, 如果是1则删除该文件夹并在后面重新创建.
	// 如果存在文件夹, 则从数组中找到指向文件夹的指针, 删除它后重新创建, 并将数组对应的指针指向新的文件夹位置.
    if(mutex[pid] == 1)//de != NULL || mutex == 1
    {
		proc_temp = (struct proc_dir_entry *)proc_test_const[pid];
        proc_remove(proc_temp);
        mutex[pid] = 0;
    }
	proc_temp = (struct proc_dir_entry *)proc_test_root_const;
    proc_test = proc_mkdir(pid_char, proc_temp);
    proc_test_const[pid] = proc_test;
	// 创建完文件夹后, 重新上锁
    mutex[pid] = 1;

	// 查找目标进程的task_struct
	target_task = pid_task(find_vpid(pid), PIDTYPE_PID);
	vma_ptr = target_task->mm->mmap;
	// 直到vma_ptr为空时停止循环
	while(vma_ptr)
	{
        addr = 0;
        do{
            pages = FOLLOW_PAGE_FUNC(vma_ptr, vma_ptr->vm_start + addr, FOLL_GET | FOLL_TOUCH);
			/* TIPS:
			 * foll_flags参数
			 * FOLL_WRITE	检查pte是不是可写的.
			 * FOLL_TOUCH	把page标记为accessed.
			 * FOLL_GET 	执行get_page(pages).
			 * FOLL_DUMP 	如果是0则抛出异常.
			 * FOLL_FORCE	代表后续get_user_pages函数中read/write的权限.
			 * 此处选择 FOLL_TOUCH , FOLL_GET 或者 FOLL_TOUCH | FOLL_GET 都可以完成此功能.
			 */
            addr += PAGE_SIZE;
            if(pages != NULL)
            {
                if(PageAnon(pages) == 1)
                {
					// PageAnon函数返回1则说明该page为匿名页, 匿名页计数+1.
                    anon++;
                    int temp = 0;
                    temp = PAGE_REFERENCED_FUNC(pages, 1, pages->mem_cgroup, &vmflags);
                    if(temp != 0)
                    {
						// page_referenced函数返回非零值则说明该页活跃, 活跃匿名页计数+1.
                        active_anon++;
                    }
                }
                else
                {
					// PageAnon函数返回0则说明该page为文件页, 文件页计数+1.
                    file++;
                    int temp = 0;
                    temp = PAGE_REFERENCED_FUNC(pages, 1, pages->mem_cgroup, &vmflags);
                    if(temp != 0)
                    {
						// page_referenced函数返回非零值则说明该页活跃, 活跃文件页计数+1.
                        active_file++;
                    }

                }
            }
        }while(addr < (vma_ptr->vm_end - vma_ptr->vm_start));
        vma_ptr = vma_ptr->vm_next;
	}
    printk("file is %d, actfile is %d, anon is %d, actanon is %d\n", file, active_file, anon, active_anon);

	// 输出到文件中
	proc_temp = (struct proc_dir_entry *)proc_test_const[pid];
	proc_create_data("file", 0664, proc_temp, &test_ops, &file);
    proc_create_data("active_file", 0664, proc_temp, &test_ops, &active_file);
	proc_create_data("anon", 0664, proc_temp, &test_ops, &anon);
	proc_create_data("active_anon", 0664, proc_temp, &test_ops, &active_anon);
}

// lab3.3 将页表对应的物理地址打印到proc文件系统中
static void print_phys_num(void) // 待完成
{
	//定义变量
    unsigned long addr;
    struct task_struct* target_task;
	struct vm_area_struct* vma_ptr;
    struct page* pages;
	// 行数计数
    int num_count = 0;
    //内存分页部分
    pgd_t *pgd;
    p4d_t *p4d;
    pmd_t *pmd;
    pud_t *pud;
    pte_t *pte;
    unsigned long phys_addr; // 物理地址
    //寻找目标进程的task_struct
	target_task = pid_task(find_vpid(pid), PIDTYPE_PID);
	vma_ptr = target_task->mm->mmap;
	// vma为空的时候, 结束循环
	while(vma_ptr)
	{
        for(addr = vma_ptr->vm_start; addr < vma_ptr->vm_end; addr += PAGE_SIZE)
        {
            pages = FOLLOW_PAGE_FUNC(vma_ptr, addr, FOLL_GET | FOLL_TOUCH);
			/* TIPS:
			 * foll_flags参数
			 * FOLL_WRITE	检查pte是不是可写的.
			 * FOLL_TOUCH	把page标记为accessed.
			 * FOLL_GET 	执行get_page(pages).
			 * FOLL_DUMP 	如果是0则抛出异常.
			 * FOLL_FORCE	代表后续get_user_pages函数中read/write的权限.
			 * 此处选择 FOLL_TOUCH , FOLL_GET 或者 FOLL_TOUCH | FOLL_GET 都可以完成此功能.
			 */
            if(pages != NULL)
            {
				// TIPS:
				// 可以直接使用pmd_off()获取pmd值, 省去前四步
				// pmd = pmd_off(target_task->mm, addr);
                pgd = pgd_offset(target_task->mm, addr);
                p4d = p4d_offset(pgd, addr); // 此处p4d和pgd值相等 (由于使用5级分页结构但是linux5.9.0还没有启用5级分页).
                pud = pud_offset(p4d, addr); // 一步一步寻找偏移量, 也可以用上面的方法直达pmd
                pmd = pmd_offset(pud, addr);
                pte = pte_offset_kernel(pmd, addr);
                phys_addr = (pte_val(*pte) & PAGE_MASK) | (addr & ~PAGE_MASK); // 根据pte找出物理地址
                // printk("Virtual Address 0x%lx corresponds to Physical Address 0x%.16lx.", addr, phys_addr);
				sprintf(physinfo.phys[num_count], "Virtual Address 0x%lx corresponds to Physical Address 0x%.16lx.", addr, phys_addr);
				num_count++;
                // 此处如果不使用printk输出到内核日志, 可能可以直接输出到/proc下.
            }
			// 不输出不存在物理地址映射的页面的虚拟地址
			/*
            else
            {
                printk("Virtual Address 0x%lx corresponds to No Physical Address because of on-demand allocation.\n", addr);
				//由于按需分配 存在虚拟页面未映射物理地址
            }*/
        }
        vma_ptr = vma_ptr->vm_next;
    }
    physinfo.length = num_count;
	// 输出到/proc的文件中(咕咕咕)
    static struct proc_dir_entry* phys_ptr = NULL;
    if(phys_ptr)
        proc_remove(phys_ptr);
    proc_temp = (struct proc_dir_entry *)proc_test_root_const;
    // 使用为func=3定制的方法创建文件
	phys_ptr = proc_create_data("phys_addr", 0664, proc_temp, &phys_ops, NULL);
}

static void sysfs_test_to_do(void)
{
    if (sysfs_test_func == 1)
        print_vma_count();
    else if (sysfs_test_func == 2)
        print_active_inactive_page_count();
	else if (sysfs_test_func == 3)
		print_phys_num();
}

static int sysfs_testd_thread(void* nothing)
{
    set_freezable();
    set_user_nice(current, 5);
    while (!kthread_should_stop())
    {
        mutex_lock(&sysfs_test_thread_mutex);
        if (sysfs_testd_should_run())
            sysfs_test_to_do();
        mutex_unlock(&sysfs_test_thread_mutex);
        try_to_freeze();
        if (sysfs_testd_should_run())
        {
            schedule_timeout_interruptible(
                msecs_to_jiffies(sysfs_test_thread_sleep_millisecs));
        }
        else
        {
            wait_event_freezable(sysfs_test_thread_wait,
                sysfs_testd_should_run() || kthread_should_stop());
        }
    }
    return 0;
}


#ifdef CONFIG_SYSFS

/*
 * This all compiles without CONFIG_SYSFS, but is a waste of space.
 */

#define SYSFS_TEST_ATTR_RO(_name) \
        static struct kobj_attribute _name##_attr = __ATTR_RO(_name)

#define SYSFS_TEST_ATTR(_name)                         \
        static struct kobj_attribute _name##_attr = \
                __ATTR(_name, 0644, _name##_show, _name##_store)

static ssize_t sleep_millisecs_show(struct kobject* kobj,
    struct kobj_attribute* attr, char* buf)
{
    return sprintf(buf, "%u\n", sysfs_test_thread_sleep_millisecs);
}

static ssize_t sleep_millisecs_store(struct kobject* kobj,
    struct kobj_attribute* attr,
    const char* buf, size_t count)
{
    unsigned long msecs;
    int err;

    err = kstrtoul(buf, 10, &msecs);
    if (err || msecs > UINT_MAX)
        return -EINVAL;

    sysfs_test_thread_sleep_millisecs = msecs;

    return count;
}
SYSFS_TEST_ATTR(sleep_millisecs);

static ssize_t pid_show(struct kobject* kobj,
    struct kobj_attribute* attr, char* buf)
{
    return sprintf(buf, "%u\n", pid);
}

static ssize_t pid_store(struct kobject* kobj,
    struct kobj_attribute* attr,
    const char* buf, size_t count)
{
    unsigned long tmp;
    int err;

    err = kstrtoul(buf, 10, &tmp);
    if (err || tmp > UINT_MAX)
        return -EINVAL;

    pid = tmp;

    return count;
}
SYSFS_TEST_ATTR(pid);


static ssize_t func_show(struct kobject* kobj,
    struct kobj_attribute* attr, char* buf)
{
    return sprintf(buf, "%u\n", sysfs_test_func);
}

static ssize_t func_store(struct kobject* kobj,
    struct kobj_attribute* attr,
    const char* buf, size_t count)
{
    unsigned long tmp;
    int err;

    err = kstrtoul(buf, 10, &tmp);
    if (err || tmp > UINT_MAX)
        return -EINVAL;

    sysfs_test_func = tmp;

    return count;
}
SYSFS_TEST_ATTR(func);

static ssize_t run_show(struct kobject* kobj, struct kobj_attribute* attr,
    char* buf)
{
    return sprintf(buf, "%u\n", sysfs_test_run);
}

static ssize_t run_store(struct kobject* kobj, struct kobj_attribute* attr,
    const char* buf, size_t count)
{
    int err;
    unsigned long flags;
    err = kstrtoul(buf, 10, &flags);
    if (err || flags > UINT_MAX)
        return -EINVAL;
    if (flags > SYSFS_TEST_RUN_START)
        return -EINVAL;
    mutex_lock(&sysfs_test_thread_mutex);
    if (sysfs_test_run != flags)
    {
        sysfs_test_run = flags;
    }
    mutex_unlock(&sysfs_test_thread_mutex);

    if (flags & SYSFS_TEST_RUN_START)
        wake_up_interruptible(&sysfs_test_thread_wait);
    return count;
}
SYSFS_TEST_ATTR(run);



static struct attribute* sysfs_test_attrs[] = {
    // 扫描进程的扫描间隔 默认为20秒
    &sleep_millisecs_attr.attr,
    &pid_attr.attr,
    &func_attr.attr,
    &run_attr.attr,
    NULL,
};


static struct attribute_group sysfs_test_attr_group = {
    .attrs = sysfs_test_attrs,
    .name = "kmscan",
};
#endif /* CONFIG_SYSFS */

static int sysfs_test_init(void)
{
    //procfs part
    // 在 proc 根目录创建 test 文件夹
    proc_test_root = proc_mkdir("kmscan", NULL);
    proc_test_root_const = proc_test_root;
    if (proc_test_root_const == NULL) {
        printk("%s proc create %s failed\n", __func__, "test");
        return -EINVAL;
    }

    int err;
    sysfs_test_thread = kthread_run(sysfs_testd_thread, NULL, "sysfs_test");
    if (IS_ERR(sysfs_test_thread))
    {
        pr_err("sysfs_test: creating kthread failed\n");
        err = PTR_ERR(sysfs_test_thread);
        goto out;
    }

#ifdef CONFIG_SYSFS
    err = sysfs_create_group(mm_kobj, &sysfs_test_attr_group);
    if (err)
    {
        pr_err("sysfs_test: register sysfs failed\n");
        kthread_stop(sysfs_test_thread);
        goto out;
    }
#else
    sysfs_test_run = KSCAN_RUN_STOP;
#endif  /* CONFIG_SYSFS */

out:
    return err;
}

static void sysfs_test_exit(void)
{
// sysfs part
    if (sysfs_test_thread)
    {
        kthread_stop(sysfs_test_thread);
        sysfs_test_thread = NULL;
    }

#ifdef CONFIG_SYSFS

    sysfs_remove_group(mm_kobj, &sysfs_test_attr_group);

#endif

    printk("sysfs exit successfully!\n");
// procfs part
	proc_temp = (struct proc_dir_entry *)proc_test_root_const;
    proc_remove(proc_temp);
    printk("procfs exit successfully!\n");
}



/* --- 随内核启动  ---  */
// subsys_initcall(kscan_init);
module_init(sysfs_test_init);
module_exit(sysfs_test_exit);
