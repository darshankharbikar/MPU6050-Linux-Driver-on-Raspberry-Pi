#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

/* Must match the struct layout in mpu6050_demo.c */
struct mpu6050_sensor_data {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
};

static volatile int keep_running = 1;

void sig_handler(int sig) {
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    const char *device_path = "/dev/mpu6050_demo";
    struct mpu6050_sensor_data raw;
    int fd;

    /* Catch Ctrl+C cleanly */
    signal(SIGINT, sig_handler);

    printf("===================================================\n");
    printf("   MPU-6050 Custom Driver User-Space Test App      \n");
    printf("===================================================\n");
    printf("[*] Opening device node: %s\n", device_path);

    fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("[-] Failed to open device node (run with sudo?)");
        return EXIT_FAILURE;
    }

    printf("[+] Device opened successfully! (fd: %d)\n", fd);
    printf("[*] Streaming live data... (Press Ctrl+C to stop)\n\n");
    printf("%-24s | %-24s\n", "  ACCELERATION (g)", "  GYROSCOPE (deg/s)");
    printf("---------------------------------------------------\n");

    while (keep_running) {
        ssize_t bytes_read = read(fd, &raw, sizeof(raw));

        if (bytes_read != sizeof(raw)) {
            perror("\n[-] Error reading from character device");
            break;
        }

        /* 
         * Scale factors for default sensor configuration:
         * Accel (+/- 2g range): 16384.0 LSB/g
         * Gyro  (+/- 250 deg/s range): 131.0 LSB/(deg/s)
         */
        float ax = raw.accel_x / 16384.0f;
        float ay = raw.accel_y / 16384.0f;
        float az = raw.accel_z / 16384.0f;

        float gx = raw.gyro_x / 131.0f;
        float gy = raw.gyro_y / 131.0f;
        float gz = raw.gyro_z / 131.0f;

        /* Print real-time updating single-line output */
        printf("\rX:%+5.2f Y:%+5.2f Z:%+5.2f | X:%+6.1f Y:%+6.1f Z:%+6.1f",
               ax, ay, az, gx, gy, gz);
        fflush(stdout);

        usleep(100000); /* 100 ms refresh interval (10 Hz) */
    }

    printf("\n\n[*] Closing device node...\n");
    close(fd);
    printf("[+] Done.\n");

    return EXIT_SUCCESS;
}
