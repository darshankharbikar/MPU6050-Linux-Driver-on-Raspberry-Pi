#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define DRIVER_NAME         "mpu6050_demo"
#define CLASS_NAME          "mpu6050_class"
#define DEVICE_NAME         "mpu6050_demo"

/* MPU6050 Register Map */
#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_WHO_AM_I        0x75
#define MPU6050_WHO_AM_I_VAL        0x68

/* Sensor data container passed to user space */
struct mpu6050_sensor_data {
    s16 accel_x;
    s16 accel_y;
    s16 accel_z;
    s16 gyro_x;
    s16 gyro_y;
    s16 gyro_z;
};

struct mpu6050_device {
    struct i2c_client *client;
    struct cdev cdev;
    dev_t devt;
    struct class *class;
    struct device *device;
};

static struct mpu6050_device *mpu_dev_global;

/* Helper: Read 16-bit big-endian register pair */
static int mpu6050_read_word_be(struct i2c_client *client, u8 reg, s16 *val)
{
    int ret = i2c_smbus_read_word_data(client, reg);
    if (ret < 0)
        return ret;

    *val = (s16)swab16((u16)ret);
    return 0;
}

/* File operations: read() reads fresh 6-axis raw values */
static ssize_t mpu6050_dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    struct mpu6050_device *mpu = file->private_data;
    struct mpu6050_sensor_data data;

    if (len < sizeof(struct mpu6050_sensor_data))
        return -EINVAL;

    /* Read Accelerometer Registers (0x3B to 0x40) */
    if (mpu6050_read_word_be(mpu->client, 0x3B, &data.accel_x) < 0 ||
        mpu6050_read_word_be(mpu->client, 0x3D, &data.accel_y) < 0 ||
        mpu6050_read_word_be(mpu->client, 0x3F, &data.accel_z) < 0) {
        dev_err(&mpu->client->dev, "[MPU6050-DEMO] Error reading accelerometer data\n");
        return -EIO;
    }

    /* Read Gyroscope Registers (0x43 to 0x48) */
    if (mpu6050_read_word_be(mpu->client, 0x43, &data.gyro_x) < 0 ||
        mpu6050_read_word_be(mpu->client, 0x45, &data.gyro_y) < 0 ||
        mpu6050_read_word_be(mpu->client, 0x47, &data.gyro_z) < 0) {
        dev_err(&mpu->client->dev, "[MPU6050-DEMO] Error reading gyroscope data\n");
        return -EIO;
    }

    if (copy_to_user(buf, &data, sizeof(data)))
        return -EFAULT;

    return sizeof(data);
}

static int mpu6050_dev_open(struct inode *inode, struct file *file)
{
    file->private_data = mpu_dev_global;
    return 0;
}

static int mpu6050_dev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations mpu6050_fops = {
    .owner   = THIS_MODULE,
    .open    = mpu6050_dev_open,
    .read    = mpu6050_dev_read,
    .release = mpu6050_dev_release,
};

static int mpu6050_probe(struct i2c_client *client)
{
    int ret;
    struct mpu6050_device *mpu;

    dev_info(&client->dev, "[MPU6050-DEMO] Entering probe function (addr: 0x%02x)\n", client->addr);

    mpu = devm_kzalloc(&client->dev, sizeof(struct mpu6050_device), GFP_KERNEL);
    if (!mpu)
        return -ENOMEM;

    mpu->client = client;
    mpu_dev_global = mpu;
    i2c_set_clientdata(client, mpu);

    /* 1. Verify WHO_AM_I register */
    ret = i2c_smbus_read_byte_data(client, MPU6050_REG_WHO_AM_I);
    if (ret < 0) {
        dev_err(&client->dev, "[MPU6050-DEMO] Failed reading WHO_AM_I: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "[MPU6050-DEMO] WHO_AM_I: 0x%02x (expected: 0x%02x)\n", ret, MPU6050_WHO_AM_I_VAL);

    /* 2. Wake up sensor: clear SLEEP bit */
    ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret < 0) {
        dev_err(&client->dev, "[MPU6050-DEMO] Failed to clear sleep mode: %d\n", ret);
        return ret;
    }
    msleep(50);

    /* 3. Basic Configuration: 1kHz sample rate, +/-250dps, +/-2g */
    i2c_smbus_write_byte_data(client, MPU6050_REG_SMPLRT_DIV, 0x07);
    i2c_smbus_write_byte_data(client, MPU6050_REG_CONFIG, 0x06);
    i2c_smbus_write_byte_data(client, MPU6050_REG_GYRO_CONFIG, 0x00);
    i2c_smbus_write_byte_data(client, MPU6050_REG_ACCEL_CONFIG, 0x00);

    /* 4. Allocate dynamic Character Device region */
    ret = alloc_chrdev_region(&mpu->devt, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        dev_err(&client->dev, "[MPU6050-DEMO] alloc_chrdev_region failed\n");
        return ret;
    }

    cdev_init(&mpu->cdev, &mpu6050_fops);
    mpu->cdev.owner = THIS_MODULE;
    ret = cdev_add(&mpu->cdev, mpu->devt, 1);
    if (ret < 0) {
        dev_err(&client->dev, "[MPU6050-DEMO] cdev_add failed\n");
        goto unregister_chrdev;
    }

    /* 5. Create device node under /dev */
    mpu->class = class_create(CLASS_NAME);
    if (IS_ERR(mpu->class)) {
        ret = PTR_ERR(mpu->class);
        dev_err(&client->dev, "[MPU6050-DEMO] class_create failed\n");
        goto delete_cdev;
    }

    mpu->device = device_create(mpu->class, NULL, mpu->devt, NULL, DEVICE_NAME);
    if (IS_ERR(mpu->device)) {
        ret = PTR_ERR(mpu->device);
        dev_err(&client->dev, "[MPU6050-DEMO] device_create failed\n");
        goto destroy_class;
    }

    dev_info(&client->dev, "[MPU6050-DEMO] Device node /dev/%s created successfully!\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(mpu->class);
delete_cdev:
    cdev_del(&mpu->cdev);
unregister_chrdev:
    unregister_chrdev_region(mpu->devt, 1);
    return ret;
}

static void mpu6050_remove(struct i2c_client *client)
{
    struct mpu6050_device *mpu = i2c_get_clientdata(client);

    /* Put sensor into sleep mode */
    i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1, 0x40);

    /* Destroy device and class nodes */
    if (mpu) {
        device_destroy(mpu->class, mpu->devt);
        class_destroy(mpu->class);
        cdev_del(&mpu->cdev);
        unregister_chrdev_region(mpu->devt, 1);
    }

    dev_info(&client->dev, "[MPU6050-DEMO] Driver removed and /dev/%s destroyed\n", DEVICE_NAME);
}

/* Device Tree Match Table */
static const struct of_device_id mpu6050_custom_of_match[] = {
    { .compatible = "custom,mpu6050" },
    { .compatible = "invensense,mpu6050" },
    { }
};
MODULE_DEVICE_TABLE(of, mpu6050_custom_of_match);

/* Legacy I2C ID Table */
static const struct i2c_device_id mpu6050_id[] = {
    { "mpu6050_demo", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_id);

static struct i2c_driver mpu6050_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = mpu6050_custom_of_match,
    },
    .probe = mpu6050_probe,
    .remove = mpu6050_remove,
    .id_table = mpu6050_id,
};

module_i2c_driver(mpu6050_driver);

MODULE_AUTHOR("Embedded Demo");
MODULE_DESCRIPTION("MPU6050 Character Device I2C Driver");
MODULE_LICENSE("GPL");
