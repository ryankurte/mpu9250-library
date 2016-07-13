/*
 * mpu9250 driver functions
 *
 * Copyright 2016 Ryan Kurte
 */

#include "mpu9250.h"

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "mpu9250_regs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define G_TO_MS 9.80665


/***        Internal Functions          ***/

static int mpu9250_read_reg(struct mpu9250_s *device, uint8_t reg, uint8_t* val)
{
    uint8_t data_out[2] = {0xFF, 0xFF};
    uint8_t data_in[2] = {0xFF, 0xFF};
    int res;

    data_out[0] = reg | MPU9250_REG_READ_FLAG;
    data_out[1] = 0x00;

    res = device->driver->spi_transfer(device->driver_ctx, 2, data_out, data_in);

    if (res >= 0) {
        *val = data_in[1];
    }

    return res;
}

static int mpu9250_read_regs(struct mpu9250_s *device, uint8_t start, uint8_t length, uint8_t* data)
{
    uint8_t data_out[length + 1];
    uint8_t data_in[length + 1];
    int res;

    data_out[0] = start | MPU9250_REG_READ_FLAG;
    for (int i = 0; i < length; i++) {
        data_out[i + 1] = 0x00;
    }

    res = device->driver->spi_transfer(device->driver_ctx, length + 1, data_out, data_in);

    if (res >= 0) {
        for (int i = 0; i < length; i++) {
            data[i] = data_in[i + 1];
        }
    }

    return res;
}

static int mpu9250_write_reg(struct mpu9250_s *device, uint8_t reg, uint8_t val)
{
    uint8_t data_out[2] = {0xFF, 0xFF};
    uint8_t data_in[2] = {0xFF, 0xFF};
    int res;

    data_out[0] = reg | MPU9250_REG_WRITE_FLAG;
    data_out[1] = val;

    res = device->driver->spi_transfer(device->driver_ctx, 2, data_out, data_in);

    return res;
}

int mpu9250_update_reg(struct mpu9250_s *device, uint8_t reg, uint8_t val, uint8_t mask)
{
    uint8_t data = 0;
    int res;

    // Read existing config
    res = mpu9250_read_reg(device, reg, &data);
    if (res < 0) {
        return res;
    }

    // Update
    data &= ~mask;
    data |=  mask & val;

    // Write back
    return mpu9250_write_reg(device, reg, data);
}

/***        External Functions          ***/

int8_t mpu9250_init(struct mpu9250_s *device, struct mpu9250_driver_s *driver, void* driver_ctx)
{
    int res;

    // Check driver functions exist
    if (driver->spi_transfer == NULL) {
        return MPU9250_DRIVER_INVALID;
    }

    // Save driver pointers
    device->driver = driver;
    device->driver_ctx = driver_ctx;

    // TODO: init

    // Hard reset chip (nb. only works if SPI working)
    res = mpu9250_write_reg(device, REG_PWR_MGMT_1, MPU9250_PWR_MGMT_1_HRESET);
    if (res < 0) {
        printf("RESET write error: %d\r\n", res);
        return MPU9250_DRIVER_ERROR;
    }

    //TODO: do we need a wait here? Probably.
    usleep(10000);

    printf("RESET complete\r\n");

    // Check communication
    uint8_t who;
    res = mpu9250_read_reg(device, REG_WHO_AM_I, &who);
    if (res < 0) {
        printf("WHOAMI read error: %d\r\n", res);
        return MPU9250_DRIVER_ERROR;
    }
    if (who != 0x71) {
        printf("Unexpected response: %.2x\r\n", who);
        return MPU9250_COMMS_ERROR;
    }

    printf("Device identified\r\n");

    // TODO: Enable compass

    // Set default scales
    res = mpu9250_set_gyro_scale(device, MPU9250_GYRO_SCALE_2000DPS);
    if (res < 0) {
        printf("Error %d setting gyro scale\r\n", res);
        return MPU9250_DRIVER_ERROR;
    }

    res = mpu9250_set_accel_scale(device, MPU9250_ACCEL_SCALE_16G);
    if (res < 0) {
        printf("Error %d setting accel scale\r\n", res);
        return MPU9250_DRIVER_ERROR;
    }

    // Set default sampling rate & filter


    return 0;
}

int8_t mpu9250_close(struct mpu9250_s *device)
{
    // TODO: shutdown

    // Clear driver pointer
    device->driver = NULL;

    return 0;
}

int mpu9250_set_gyro_scale(struct mpu9250_s *device, mpu9250_gyro_scale_e scale)
{
    switch (scale) {
    case MPU9250_GYRO_SCALE_250DPS:
        device->gyro_scale = 250.0 / 180 * M_PI / (float)GYRO_SCALE_BASE;
        break;
    case MPU9250_GYRO_SCALE_500DPS:
        device->gyro_scale = 500.0 / 180 * M_PI / (float)GYRO_SCALE_BASE;
        break;
    case MPU9250_GYRO_SCALE_1000DPS:
        device->gyro_scale = 1000.0 / 180 * M_PI / (float)GYRO_SCALE_BASE;
        break;
    case MPU9250_GYRO_SCALE_2000DPS:
        device->gyro_scale = 2000.0 / 180 * M_PI / (float)GYRO_SCALE_BASE;
        break;
    default:
        return -1;
    }

    return mpu9250_update_reg(device,
                              REG_GYRO_CONFIG,
                              scale << MPU9250_GYRO_CONFIG_SCALE_SHIFT,
                              MPU9250_GYRO_CONFIG_SCALE_MASK);
}

int mpu9250_set_accel_scale(struct mpu9250_s *device, mpu9250_accel_scale_e scale)
{
    switch (scale) {
    case MPU9250_ACCEL_SCALE_2G:
        device->accel_scale = 2.0 / (float)ACCEL_SCALE_BASE;
        break;
    case MPU9250_ACCEL_SCALE_4G:
        device->accel_scale = 4.0 / (float)ACCEL_SCALE_BASE;
        break;
    case MPU9250_ACCEL_SCALE_8G:
        device->accel_scale = 8.0 / (float)ACCEL_SCALE_BASE;
        break;
    case MPU9250_ACCEL_SCALE_16G:
        device->accel_scale = 16.0 / (float)ACCEL_SCALE_BASE;
        break;
    default:
        return -1;
    }

    return mpu9250_update_reg(device,
                              REG_ACCEL_CONFIG_1,
                              scale << MPU9250_ACCEL_CONFIG_1_SCALE_SHIFT,
                              MPU9250_ACCEL_CONFIG_1_SCALE_MASK);
}


int mpu9250_read_gyro_raw(struct mpu9250_s *device, int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t data_in[6];
    int res;

    res = mpu9250_read_regs(device, REG_GYRO_XOUT_H, 6, data_in);
    if (res >= 0) {
        *x = (int16_t)data_in[0] << 8 | data_in[1];
        *y = (int16_t)data_in[2] << 8 | data_in[3];
        *z = (int16_t)data_in[4] << 8 | data_in[5];
    }
    return res;

}

int mpu9250_read_gyro(struct mpu9250_s *device, float *x, float *y, float *z)
{
    int16_t raw_x, raw_y, raw_z;
    int res;

    res = mpu9250_read_gyro_raw(device, &raw_x, &raw_y, &raw_z);
    if (res >= 0) {
        *x = raw_x * device->gyro_scale;
        *y = raw_y * device->gyro_scale;
        *z = raw_z * device->gyro_scale;
    }

    return res;
}

int mpu9250_read_accel_raw(struct mpu9250_s *device, int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t data_in[6];
    int res;

    res = mpu9250_read_regs(device, REG_ACCEL_XOUT_H, 6, data_in);
    if (res >= 0) {
        *x = (int16_t)data_in[0] << 8 | data_in[1];
        *y = (int16_t)data_in[2] << 8 | data_in[3];
        *z = (int16_t)data_in[4] << 8 | data_in[5];
    }
    return res;

}

int mpu9250_read_accel(struct mpu9250_s *device, float *x, float *y, float *z)
{
    int16_t raw_x, raw_y, raw_z;
    int res;

    res = mpu9250_read_accel_raw(device, &raw_x, &raw_y, &raw_z);
    if (res >= 0) {
        *x = raw_x * device->accel_scale;
        *y = raw_y * device->accel_scale;
        *z = raw_z * device->accel_scale;
    }

    return res;
}

int mpu9250_read_temp_raw(struct mpu9250_s *device, int16_t *temp)
{
    uint8_t data_in[2];
    int res;

    res = mpu9250_read_regs(device, REG_TEMP_OUT_H, 2, data_in);
    if (res >= 0) {
        *temp = data_in[0] << 8 | data_in[1];
    }
    return res;
}

int mpu9250_read_temp(struct mpu9250_s *device, float* temp)
{

    int16_t raw_temp;
    int res;

    res = mpu9250_read_temp_raw(device, &raw_temp);
    if (res >= 0) {
        //TODO: no datasheet source for these factors
        // Find and refactor out to constants.
        *temp = ((float)raw_temp / 340) + 36.53;
    }

    return res;
}


