/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Tiago Neto"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
     filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    
     //validate inputs
    if(!filp || !buf || !f_pos || *f_pos < 0 || count == 0) {
        return -EINVAL;
    }
    struct aesd_dev *driver = filp->private_data;
    // try to aquire the mutex lock before accessing the circular buffer
    if(mutex_lock_interruptible(&driver->buffer_lock)) {
        return -ERESTARTSYS;
    }

    // find the buffer entry corresponding to the current file position
    struct aesd_buffer_entry *entry;
    ssize_t entry_offset = 0;
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&driver->c_buffer, *f_pos, &entry_offset);
    if(entry == NULL) {
        PDEBUG("No entry found for offset %lld", *f_pos);
        retval = 0; // EOF
        goto out;
    }

    // calculate the number of bytes to copy from the entry to the user buffer
    ssize_t bytes_to_copy = entry->size - entry_offset;
    // adjust bytes_to_copy if it exceeds the requested count
    if(bytes_to_copy > count) {
        bytes_to_copy = count;
    }

    // copy data from the entry to the user buffer
    retval = copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy);
    if(retval != 0) {
        PDEBUG("Failed to copy data to user buffer");
        retval = -EFAULT;
        goto out;
    }

    // update the file position
    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

    out:
        mutex_unlock(&driver->buffer_lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *driver = filp->private_data;
    bool newline_found = false;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    //validate inputs
    if(!filp || !buf || !f_pos || *f_pos < 0 || count == 0) {
        return -EINVAL;
    }
    // try to aquire the mutex lock before accessing the circular buffer
    if(mutex_lock_interruptible(&driver->buffer_lock)) {
        return -ERESTARTSYS;
    }

    // allocate memory for the new buffer entry with size equal to count
    char *kbuf = kmalloc(count, GFP_KERNEL);
    if(kbuf == NULL) {
        PDEBUG("Failed to allocate memory for buffer entry");
        retval = -ENOMEM;
        goto free_mutex;
    }

    // copy data from the user buffer to the kernel buffer
    retval = copy_from_user(kbuf, buf, count);
    if(retval != 0) {
        PDEBUG("Failed to copy data from user buffer");
        retval = -EFAULT;
        goto free_kbuf;
    }
    
    // check for newline in buffer and determine the size of the current command and the next command
    char *newline_pos = memchr(kbuf, '\n', count);
    size_t current_cmd_bytes = 0;
    size_t next_cmd_bytes = 0;
    if(newline_pos != NULL) {
        current_cmd_bytes = newline_pos - kbuf + 1;
        next_cmd_bytes = count - current_cmd_bytes;
        newline_found = true;
    } 
    else {
        current_cmd_bytes = count;
    }

    //appended previous entry data to current buffer
    //allocate extra memory
    driver->current_entry.buffptr = krealloc(driver->current_entry.buffptr, driver->current_entry.size + current_cmd_bytes, GFP_KERNEL);
    if(driver->current_entry.buffptr == NULL) {
        PDEBUG("Failed to allocate memory for current entry");
        retval = -ENOMEM;
        goto free_mutex;
    }
    // copy current command data to current entry buffer
    memcpy(driver->current_entry.buffptr + driver->current_entry.size, kbuf, current_cmd_bytes);
    // update current entry size
    driver->current_entry.size += current_cmd_bytes;
    retval = current_cmd_bytes; // return the number of bytes written for the current command
    // if newline was found push command to circular buffer as a new entry
    if(newline_found){
        const char* overwritten_buffptr = aesd_circular_buffer_add_entry(&driver->c_buffer, &driver->current_entry);
        // if an entry was overwritten, free the memory allocated for that entry
        if(overwritten_buffptr != NULL) {
            kfree(overwritten_buffptr); 
        }
        retval = current_cmd_bytes; // return the number of bytes written for the current command
        newline_found = false;
        // reset current entry buffer and size for next command
        driver->current_entry.buffptr = NULL;
        driver->current_entry.size = 0;
        // if there is data remaining in the buffer after the newline, copy it to the current entry buffer for the next command
        if(next_cmd_bytes > 0) {
            driver->current_entry.buffptr = kmalloc(next_cmd_bytes, GFP_KERNEL);
            if(driver->current_entry.buffptr == NULL) {
                PDEBUG("Failed to allocate memory for current entry");
                goto free_mutex;
            }
            memcpy(driver->current_entry.buffptr, kbuf + current_cmd_bytes, next_cmd_bytes);
            driver->current_entry.size = next_cmd_bytes;
            retval = count; // return the total number of bytes written from the user buffer

        }
    }
    
    free_mutex:
        mutex_unlock(&driver->buffer_lock);

    free_kbuf:
        kfree(kbuf);
    
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    //intialize the mutex
    mutex_init(&aesd_device.buffer_lock);
    //initialize the circular buffer
    aesd_circular_buffer_init(&aesd_device.c_buffer);
    

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    
    // free any memory allocated for the circular buffer entries
    struct aesd_buffer_entry *entry;
    uint8_t index;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.c_buffer, index) {
        if(entry->buffptr != NULL) {
            kfree(entry->buffptr);
        }
    }
    // destroy the mutex
    mutex_destroy(&aesd_device.buffer_lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
