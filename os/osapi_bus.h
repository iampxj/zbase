/*
 * Copyright 2024 wtcat
 */
#ifndef OSAPI_BUS_H_
#define OSAPI_BUS_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <stdint.h>
#include <stddef.h>

#include "basework/generic.h"
#include "basework/assert.h"
#include "basework/container/queue.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifndef IOBUS_DEVICE_USER_EXTENSION
#define IOBUS_DEVICE_USER_EXTENSION
#endif

/*
 * I/O bus type
 */
enum iobus_type {
    K_IOBUS_I2C  = 0,
    K_IOBUS_SPI
};


struct iobus_sq;
struct iobus_device;

struct i2c_param {
    const void *txbuf;
    void *rxbuf;
    uint16_t txlen;
    uint16_t rxlen;
    uint16_t addr;
};

struct spi_param {
    void *tx_param;
    void *rx_param;
    void *param;
};

struct iobus_device {
    char name[32];
    struct iobus_sq *sq;
    void *dev;
    void (*notify)(const struct iobus_device *iod, int dstate);
#define IOBUS_DEVSTATE_SUSPEND 0
#define IOBUS_DEVSTATE_RESUME  1
    IOBUS_DEVICE_USER_EXTENSION
};

struct iobus_sqe {
    /* Bus operation code */
    uint32_t  op;
#define IOBUS_I2C_WRITEREAD  0
#define IOBUS_I2C_WRITE      1
#define IOBUS_I2C_READ       2
#define IOBUS_SPI_TRANSFER   3
#define IOBUS_SPI_WRITE      4
#define IOBUS_SPI_READ       5

    union {
        struct i2c_param i2c;
        struct spi_param spi;
    };
    union {
        void   *dd;
        int     fd;
    };

    int result;
    void *arg;
    void (*done)(struct iobus_sqe *qe);
};

struct iobus_llops {
    enum iobus_type type;
    void (*transfer)(struct iobus_sqe *sqe);
    int  (*devopen)(const char *name, void **devfd);
    STAILQ_ENTRY(iobus_llops) next;
};

#define IOBUS_I2C(_op, _addr, _txbuf, _txlen, _rxbuf, _rxlen) \
    { \
        .op = _op, \
        .i2c = { \
            .txbuf = _txbuf, \
            .rxbuf = _rxbuf, \
            .txlen = _txlen, \
            .rxlen = _rxlen, \
            .addr  = _addr, \
        }, \
        .result = -1, \
        .arg = NULL, \
        .done = NULL \
    }

int iobus_init(void);
int iobus_create(const char *buses[], enum iobus_type type, int qsize, 
    int prio, void *stack, int stacksize);
int iobus_destroy(struct iobus_sq *sq);
struct iobus_device *iobus_request(const char *bus);
int iobus_release(struct iobus_device *iod);
int __iobus_burst_submit(struct iobus_sq *sq, struct iobus_sqe **sqes, 
    size_t n);
int iobus_burst_submit_wait(struct iobus_device *iodev, 
    struct iobus_sqe **sqes, size_t n);
int iobus_register_llops(struct iobus_llops *ops);
void iobus_dump(void);

static __rte_always_inline int 
iobus_submit(struct iobus_device *iodev, struct iobus_sqe sqes[], 
    size_t n) {
    struct iobus_sqe *seqs_tbl[8];
#ifdef CONFIG_IOBUS_PARAM_CHECKER
    rte_assert(iodev != NULL);
    rte_assert(sqes != NULL);
    rte_assert(n > 0 && n < 8);
#endif
    for (size_t i = 0; i < n; i++) {
        sqes[i].dd  = iodev->dev;
        seqs_tbl[i] = &sqes[i];
    }
    return iobus_burst_submit_wait(iodev, seqs_tbl, n);
}

static __rte_always_inline int
iobus_submit_async(struct iobus_device *iodev, struct iobus_sqe *sqe, 
    void (*done)(struct iobus_sqe *qe)) {
#ifdef CONFIG_IOBUS_PARAM_CHECKER
    rte_assert(iodev != NULL);
    rte_assert(sqe != NULL);
#endif
    sqe->dd = iodev->dev;
    sqe->arg  = sqe;
    sqe->done = done;
    return __iobus_burst_submit(iodev->sq, (struct iobus_sqe **)&sqe->arg, 1);
}

#ifdef __cplusplus
}
#endif
#endif /* OSAPI_BUS_H_ */
