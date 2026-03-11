/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    size_t cumulative_size = 0;
    uint8_t index =0;

    //check if buffer is empty
    if(buffer->in_offs == buffer->out_offs && !buffer->full) {
        return NULL; // buffer is empty
    }

    for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        
        index =(buffer->out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        cumulative_size += buffer->entry[index].size;
        if (cumulative_size > char_offset) {
            
            *entry_offset_byte_rtn = (char_offset -(cumulative_size - buffer->entry[index].size));
            
            return &buffer->entry[index];
        }

    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
* @return pointer to the location of the value that was overwritten in the buffer, or NULL if no value was overwritten
*/
const char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{   
    const char* ret_ptr = NULL;
    //validate input parameters
    if (buffer == NULL || add_entry == NULL) {
        return NULL; // invalid input
    }
    // check if buffer is full, if so move out_offs to next entry
    if (buffer->full) {
        ret_ptr = buffer->entry[buffer->in_offs].buffptr;
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    // add new entry to buffer at in_offs
    buffer->entry[buffer->in_offs] = *add_entry;
    //update in_offs to next entry
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    //if in_offs has wrapped around to out_offs, set full flag
    if (buffer->in_offs == buffer->out_offs && buffer->entry[buffer->in_offs].size != 0) {
        buffer->full = true; 
    }
    

    return ret_ptr;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
