#define LINUX


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/cdev.h>

#include "mp3_given.h"
#include "mp3.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_09");
MODULE_DESCRIPTION("CS-423 MP3");

#define DEBUG 1
#define GLOBAL_RW_PERM 0666
#define DIR_NAME "mp3"
#define FILE_NAME "status"

#define NPAGES 128
#define SAMPLE_SIZE 16

/* The proc directory entry */
static struct proc_dir_entry *pdir_mp3;

/* A list holding the RMS parameters for each task */
static struct mp3_pcb mp3_pcb;

/* Spinlock to protect the linked list */
static spinlock_t list_lock;
static unsigned long lock_flag;

/* Cache for mp3_task_struct slab allocation */
static struct kmem_cache *task_cache;

/* Work task to perform job monitoring */
static struct delayed_work *monitor_work;

/* Keeps track of number of tasks in list */
static int num_jobs;

/* Keeps track of the number of samples */
static int num_samples;

/* A buffer allocated for profiler data */
static char *prof_buffer;

/* The character driver */
static dev_t node_number;
static struct cdev node_cdev;


/* Helper function to delete the linked list */
void delete_mp3_pcb(void) {
   struct list_head *head, *next;
   struct mp3_pcb *tmp;

   list_for_each_safe(head, next, &mp3_pcb.list) {
      tmp = list_entry(head, struct mp3_pcb, list);
      list_del(head);
      kmem_cache_free(task_cache, tmp);
   }
}

/* Helper function to create the directory entries for /proc */
void create_mp3_proc_files(void) {
   pdir_mp3 = proc_mkdir(DIR_NAME, NULL);
   proc_create(FILE_NAME, GLOBAL_RW_PERM, pdir_mp3, &proc_fops);
}

/* Helper function to delete the directory entries for /proc */
void delete_mp3_proc_files(void) {
   remove_proc_entry(FILE_NAME, pdir_mp3);
   remove_proc_entry(DIR_NAME, NULL);
}

/* Occurs when a user runs cat /proc/mp3/status
 * Returns a list of which applications are registered
 */
ssize_t read_proc(struct file *filp, char *user, size_t count, loff_t *offset)
{
   struct list_head *head;
   struct mp3_pcb *tmp;
   int len;
   int pos = 0;
   char *buf = (char *)kmalloc(count, GFP_KERNEL);

   #ifdef DEBUG
   printk("/proc/mp3/status read\n");
   #endif

   if((int)*offset > 0) {
      kfree((void *)buf);
      return 0;
   }

   list_for_each(head, &mp3_pcb.list) {
      tmp = list_entry(head, struct mp3_pcb, list);
      len = sprintf(buf + pos, "PID: %lu\n", tmp->pid);
      pos += len;
   }

   copy_to_user(user, buf, pos);
   kfree((void *)buf);

   *offset +=pos;
   return pos;
}

/* Occurs when a user runs ./process > /proc/mp3/status
   Input format should either be: "R PID"
                                  "U PID"
*/
ssize_t write_proc(struct file *filp, const char *user, size_t count, loff_t *offset)
{
   char *buf = (char *)kmalloc(count, GFP_KERNEL);
   char type;
   unsigned long pid;
   copy_from_user(buf, user, count);

   type = buf[0];
   sscanf(&buf[1], "%lu", &pid);

   switch(type)
   {
      case 'R':
         register_handler(pid);
         break;
      case 'U':
         unregister_handler(pid);
         break;
   }

   kfree((void *)buf);
   return count;
}

/* Helper function to register a task */
void register_handler(unsigned long pid) {
   struct mp3_pcb *temp_task;

   temp_task = kmem_cache_alloc(task_cache, GFP_KERNEL);
   temp_task->pid = pid;
   temp_task->linux_task = find_task_by_pid(pid);

   /* Add a task entry */
   spin_lock_irqsave(&list_lock, lock_flag);
   list_add(&(temp_task->list), &(mp3_pcb.list));
   num_jobs++;
   spin_unlock_irqrestore(&list_lock, lock_flag);

   if(num_jobs == 1) {
      monitor_work = (struct delayed_work*)kmalloc(sizeof(struct delayed_work), GFP_KERNEL);
      INIT_DELAYED_WORK(monitor_work, monitor_wq_function);
      schedule_delayed_work(monitor_work, msecs_to_jiffies(0));
   }

   #ifndef DEBUG
   printk("PROCESS REGISTERED: %lu\n", temp_task->pid);
   #endif
}

/* Helper function to unregister a task */
void unregister_handler(unsigned long pid) {
   struct list_head *head, *next;
   struct mp3_pcb *tmp;

   spin_lock_irqsave(&list_lock, lock_flag);
   list_for_each_safe(head, next, &mp3_pcb.list) {
      tmp = list_entry(head, struct mp3_pcb, list);
      if(tmp->pid == pid) {
         list_del(head);
         num_jobs--;
         kmem_cache_free(task_cache, tmp);
         break;
      }
   }
   spin_unlock_irqrestore(&list_lock, lock_flag);

   if(num_jobs == 0) {
      cancel_delayed_work(monitor_work);
      kfree(monitor_work);
   }

   #ifdef DEBUG
   printk("PROCESS DEREGISTERED: %lu\n", pid);
   #endif
}

/* Callback for the work function to monitor jobs */
void monitor_wq_function(struct work_struct *work) {
   struct list_head *head;
   struct mp3_pcb *tmp;
   unsigned long min_flt, maj_flt, utime, stime, util;
   unsigned long t_min_flt = 0, t_maj_flt = 0, t_util = 0;

   list_for_each(head, &mp3_pcb.list) {
      tmp = list_entry(head, struct mp3_pcb, list);
      if(get_cpu_use(tmp->pid, &min_flt, &maj_flt, &utime, &stime) != 0) {
         util = (utime + stime) / 2;
         t_min_flt += min_flt;
         t_maj_flt += maj_flt;
         t_util += util;
      }
   }

   num_samples = (num_samples + 1) % 1200;
   prof_buffer[num_samples * SAMPLE_SIZE + 0] = jiffies;
   prof_buffer[num_samples * SAMPLE_SIZE + 4] = t_min_flt;
   prof_buffer[num_samples * SAMPLE_SIZE + 8] = t_maj_flt;
   prof_buffer[num_samples * SAMPLE_SIZE + 12] = t_util;

   schedule_delayed_work(monitor_work, msecs_to_jiffies(1000 / 20));

   /* Implement */
   #ifdef DEBUG
   printk("WORKQUEUE FUNCTION CALLED\n");
   #endif
}

/* Drive open op */
int open_drive(struct inode *i, struct file *f){
   return 0;
}

/* Drive close op */
int release_drive(struct inode *i, struct file *f){
   return 0;
}


/* Called when module is loaded */
int __init mp3_init(void)
{
   num_jobs = 0;

   prof_buffer = vmalloc(NPAGES * PAGE_SIZE);

   INIT_LIST_HEAD(&mp3_pcb.list);
   create_mp3_proc_files();
   task_cache = kmem_cache_create("mp3_task_cache",
                                  sizeof(struct mp3_pcb),
                                  0, SLAB_HWCACHE_ALIGN,
                                  NULL);

   spin_lock_init(&list_lock);

   alloc_chrdev_region(&node_number, 0, 1, "node");
   cdev_init(&node_cdev, &drive_fops);
   cdev_add(&node_cdev, node_number, 1);

   printk(KERN_ALERT "MP3 MODULE LOADED\n");
   return 0;
}

/* Called when module is unloaded */
void __exit mp3_exit(void)
{
   // Cleans up the file entries in /proc and the data structures
   if(num_jobs > 0) {
      cancel_delayed_work(monitor_work);
   }

   kmem_cache_destroy(task_cache);
   delete_mp3_proc_files();
   delete_mp3_pcb();
   vfree(prof_buffer);

   cdev_del(&node_cdev);
   unregister_chrdev_region(node_number, 1);

   printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
}


// Register init and exit funtions
module_init(mp3_init);
module_exit(mp3_exit);
