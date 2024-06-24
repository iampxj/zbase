/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_LIB_MSG_STORAGE_H_
#define BASEWORK_LIB_MSG_STORAGE_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_MAX_MESSAGES
# define MAX_MESSAGES  CONFIG_MAX_MESSAGES
#else
# define MAX_MESSAGES  10
#endif

#ifdef CONFIG_MAX_MESSAGE_TYPES
# define MAX_MESSAGE_TYPES CONFIG_MAX_MESSAGE_TYPES
#else
# define MAX_MESSAGE_TYPES 50
#endif

typedef void (*message_notif_cb)(int idx, void *arg);

enum notification_state {
    MSG_NOTIF_NORMAL,
    MSG_NOTIF_SILENCE,
    MSG_NOTIF_DISABLE
};

/* message file operations */
struct msg_fops {
    int (*open)(const char *name, void **fd);
    int (*close)(void *fd);
    int (*flush)(void *fd);
#ifndef CONFIG_SIMULATOR
    ssize_t (*write)(void *fd, const void *buf, size_t size, uint32_t offset);
    ssize_t (*read)(void *fd, void *buf, size_t size, uint32_t offset);
#endif
    int (*unlink)(const char *name);
};

struct msg_payload {
#define MAX_MESSAGE_SIZE 256
#define MAX_CONTACT_SIZE 32
    uint32_t timestamp;
	uint16_t id;
    uint16_t type;
    uint16_t c_len;
    char contact[MAX_CONTACT_SIZE];
    uint16_t m_len;
    char buffer[MAX_MESSAGE_SIZE];
};

struct msg_node {
	uint16_t type;  /* message type */
	uint16_t index;   /* current index */
	uint16_t n_index; /* next index of the node */
    uint16_t p_index; /* previous index of the node */
};

/*
 * Walk around messages by time order
 */
#define MSG_STORAGE_FOREACH(_id) \
    for (int _id = msg_storage_first(); \
        _id >= 0; \
        _id = msg_storage_next(_id))

/*
 * msg_storage_numbers - Get the number of messages
 *
 * return the numbers of message 
 */
size_t msg_storage_numbers(void);

/*
 * msg_storage_clean - Clean all messages
 *
 * return 0 if success
 */
int msg_storage_clean(void);

/*
 * msg_storage_first - Get the first message ID
 *
 * return the ID if success (id >= 0)
 */
int msg_storage_first(void);

/*
 * msg_storage_next - Get the first message ID
 *
 * @id: message id
 * return the next id if success (id >= 0)
 */
int msg_storage_next(int id);

/*
 * msg_storage_remove - Delete a message by message id
 *
 * @id: message id
 * return 0 if success
 */
int msg_storage_remove(int id);

/*
 * msg_storage_read - Read message content by message id
 *
 * @id: message id
 * @playload: message content
 * return 0 if success
 */
int msg_storage_read(int id, struct msg_payload *payload);

/*
 * msg_storage_read - Write message content to database
 *
 * @playload: message content
 * return 0 if success
 */
int msg_storage_write(const struct msg_payload *payload);

/*
 * msg_storage_flush - Flush message data to disk
 *
 * return 0 if success
 */
int msg_storage_flush(void);

/*
 * msg_storage_get_newidx - Get the index of message that just received
 *
 * return >= 0 if success
 */
int msg_storage_get_newidx(void);

/*
 * msg_storage_get_node_newidx - Get the new index of message linked list
 *
 * return new index of linked list
 */
int msg_storage_get_node_newidx(void);

/*
 * msg_storage_init - Initialize message database
 *
 * return 0 if success
 */
int msg_storage_init(void);

/*
 * msg_storage_register - Register file operation for message database
 *
 * return 0 if success
 */
int msg_storage_register(const struct msg_fops *fops);

/*
 * msg_storage_deinit - Close message database and free resource
 *
 * return 0 if success
 */
int msg_storage_deinit(void);

/*
 * msg_storage_set_notification - Set notification state of message by type
 *
 * @type: message type
 * @state: notification state
 *
 * return 0 if success
 */
int msg_storage_set_notification(int type, enum notification_state state);

/*
 * msg_storage_get_notification - Get notification state of message by type
 *
 * @type: message type
 * @state: notification state pointer
 *
 * return 0 if success
 */
int msg_storage_get_notification(int type, enum notification_state *sta);

/*
 * msg_storage_set_bitvec - Set bit-vector for user
 *
 * @setmsk: set mask
 * @clrmsk: clear mask
 * return 0 if success
 */
int msg_storage_set_bitvec(uint32_t setmsk, uint32_t clrmsk);

/*
 * msg_storage_get_bitvect - Get bit-vector
 *
 * return bit-vector
 */
uint32_t msg_storage_get_bitvect(void);

/*
 * msg_storage_set_notif_switch - Set the message notification switch
 *
 * @enable: message type
 * return 0 if success
 */
int msg_storage_set_notif_switch(bool enable);

/*
 * msg_storage_get_notif_switch - Get the message notification switch status
 *
 * return notification status
 */
bool msg_storage_get_notif_switch(void);

/*
 * msg_storage_register_notifier - Reigister notification callback for message
 *
 * @cb: callback pointer
 * @arg: user data 
 *
 * return 0 if success
 */
int msg_storage_register_notifier(message_notif_cb cb, void *arg);

/*
 * msg_partition_backend_init - Register partition backend for message
 *
 * return 0 if success
 */
int msg_partition_backend_init(void);

/*
 * msg_file_backend_init - Register file backend for message
 *
 * return 0 if success
 */
int msg_file_backend_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_MSG_STORAGE_H_ */
