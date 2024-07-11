/*
 * Copyright 2024 wtcat
 */
#include <errno.h>

#include <init.h>
#include <device.h>
#include <drivers/i2c.h>
#include <drivers/spi.h>

#include "basework/os/osapi_bus.h"

static void spi_bus_transfer(struct iobus_sqe *sqe) {
    struct spi_param *ip = &sqe->spi;
    const struct device *spi_bus = sqe->dd;

    switch (sqe->op) {
    case IOBUS_SPI_TRANSFER:
        sqe->result = spi_transceive(spi_bus, ip->param, 
            ip->tx_param, ip->rx_param);
        break;
    case IOBUS_SPI_WRITE:
        sqe->result = spi_write(spi_bus, ip->param, 
            ip->tx_param);
        break;
    case IOBUS_SPI_READ:
        sqe->result = spi_read(spi_bus, ip->param, 
            ip->rx_param);
        break;
    default:
        pr_err("Error***: iobus-spi invalid opcode(%d)\n", sqe->op);
        sqe->result = -EINVAL;
        break;
    }

    if (sqe->done)
        sqe->done(sqe);
}

static void i2c_bus_transfer(struct iobus_sqe *sqe) {
    struct i2c_param *ip = &sqe->i2c;
    const struct device *i2c_bus = sqe->dd;

    switch (sqe->op) {
    case IOBUS_I2C_WRITEREAD:
        sqe->result = i2c_write_read(i2c_bus, ip->addr, ip->txbuf, 
            ip->txlen, ip->rxbuf, ip->rxlen);
        break;
    case IOBUS_I2C_WRITE:
        sqe->result = i2c_write(i2c_bus, ip->txbuf, 
            ip->txlen, ip->addr);
        break;
    case IOBUS_I2C_READ:
        sqe->result = i2c_read(i2c_bus, ip->rxbuf, 
            ip->rxlen, ip->addr);
        break;
    default:
        pr_err("Error***: iobus-i2c invalid opcode(%d)\n", sqe->op);
        sqe->result = -EINVAL;
        break;
    }

    if (sqe->done)
        sqe->done(sqe);
}

static int device_open(const char *name, void **devfd) {
    const struct device *dev;

    dev = device_get_binding(name);
    if (dev == NULL)
        return -ENODEV;

    if (devfd)
        *devfd = (void *)dev;
    return 0;
}

static struct iobus_llops iobus[] = {
    {
        .type     = K_IOBUS_I2C,
        .transfer = i2c_bus_transfer,
        .devopen  = device_open
    }, {
        .type     = K_IOBUS_SPI,
        .transfer = spi_bus_transfer,
        .devopen  = device_open
    }
};

static int iobus_device_init(const struct device *dev __unused) {
    iobus_init();
    for (size_t i = 0; i < ARRAY_SIZE(iobus); i++)
        iobus_register_llops(&iobus[i]);
    return 0;
}

SYS_INIT(iobus_device_init, POST_KERNEL, 0);
