#ifndef __MP3__
#define __MP3__

/* The linked list structure */
struct mp3_pcb {
   struct list_head list; /* Kernel's list structure */
   struct task_struct* linux_task;
   unsigned long pid;
   unsigned long utilization;
   unsigned long major;
   unsigned long minor;
};

/* Helper function to get a pcb entry from a pid */
struct mp3_pcb* get_pcb_from_pid(unsigned int pid);

/* Helper function to delete the pid RMS list */
void delete_mp3_pcb(void);

/* Helper function to create the directory entries for /proc */
void create_mp3_proc_files(void);

/* Helper function to delete the entries for /proc */
void delete_mp3_proc_files(void);

/* /proc file read op */
ssize_t read_proc(struct file *filp, char *user, size_t count, loff_t *offset);

/* /proc file write op */
ssize_t write_proc(struct file *filp, const char *user, size_t count, loff_t *offset);

/* Helper function to register a task */
void register_handler(unsigned long pid);

/* Helper function to deregister a task */
void unregister_handler(unsigned long pid);

/* Callback for the work function to monitor jobs */
void monitor_wq_function(struct work_struct *work);

/* Called when module is loaded */
int __init mp3_init(void);

/* Called when module is unloaded */
void __exit mp3_exit(void);

/* Drive open op */
int open_drive(struct inode *i, struct file *f);

/* Drive close op */
int close_drive(struct inode *i, struct file *f);

/* Available file operations for mp3/status */
struct file_operations proc_fops = {
   read: read_proc,
   write: write_proc
};

struct file_operations drive_fops = {
	open: open_drive,
	close: close_drive
	// mmap: mmap_drive
};

#endif
