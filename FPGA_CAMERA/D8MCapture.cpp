#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <socal/hps.h>
#include <opencv2/opencv.hpp>
#include <sys/ioctl.h>
#include "D8MCapture.h"

using namespace cv;

#define HW_REGS_BASE (ALT_STM_OFST)
#define HW_REGS_SPAN (0x04000000)
#define HW_REGS_MASK (HW_REGS_SPAN - 1)

#define BUFF_SPAN (800 * 480 * 4 * 2)
#define H2F_MASK (H2F_SPAN - 1)

#define IORD(base, index)                   (*(((uint32_t *)base)+index))
#define IOWR(base, index, data)             (*(((uint32_t *)base)+index) = data)

#define REG_CONTROL					0
#define REG_STATUS					1
#define REG_MEM_ADDR				2
#define REG_FRAME_DIM				3
#define REG_DETECTED_FRAME_DIM		4

// bit mask for CONTROL
#define CONTROL_CAPTURE_BIT			0x01
#define CONTROL_DUMMY_DATA_BIT		0x02
#define CONTROL_AUTO_FRAME_DIM_BIT	0x04

// bit mask for STATUS
#define STATUS_DONE_BIT				0x01
#define STATUS_FIFO_FULL_BIT		0x02
#define STATUS_INVALID_FRAME_BIT	0x04

cv::D8MCapture::D8MCapture()
{
    opened = false;
    frame_index = 0;
}

cv::D8MCapture::D8MCapture(uint32_t capture_base, const char *capture_ram_device)
{
    opened = false;
    frame_index = 0;
    open(capture_base, capture_ram_device);
}

cv::D8MCapture::~D8MCapture()
{
    release();
}

bool cv::D8MCapture::retrieve(OutputArray image, int flag)
{
    bool bDone = false;
    int width, height;

    uint32_t value;

    value = IORD(capture_controller, REG_DETECTED_FRAME_DIM);
    width = (value >> 16) & 0xFFFF;
    height = value & 0xFFFF;
    //printf("width:%d, height:%d\r\n", width, height);

    bDone = true;
    image.create(480, 800, CV_8UC4);
    Mat src = image.getMat();
    if (frame_index == 0)
        memcpy(src.ptr(), capture_sdram1, width * height * 4);
    else
        memcpy(src.ptr(), capture_sdram2, width * height * 4);
    return bDone;
}

bool cv::D8MCapture::wait_done(int timeout_s)
{
    bool bDone = false;
    int i = timeout_s * 1000 * 1000 / 100;

    uint32_t status;

    while (!bDone && i >= 0) {
        status = IORD(capture_controller, REG_STATUS);
        if ((status & STATUS_DONE_BIT) == STATUS_DONE_BIT) {
            //printf("done, status=%xh\r\n", status);
            bDone = true;
        }
        usleep(100);
        i--;
    }
    capture_status = status;
    return bDone;
}

bool cv::D8MCapture::grab()
{
    bool bDone = false;

    bDone = wait_done(1);

    if (frame_index == 0) {
        IOWR(capture_controller, REG_MEM_ADDR, capture_sdram_base);
        frame_index = 1;
    } else {
        IOWR(capture_controller, REG_MEM_ADDR,
                capture_sdram_base + (800 * 480 * 4));
        frame_index = 0;
    }

    start_capture();

    return bDone;
}

void cv::D8MCapture::start_capture()
{
    uint32_t Command;

    Command = 0;
    Command |= CONTROL_CAPTURE_BIT | CONTROL_AUTO_FRAME_DIM_BIT;
    IOWR(capture_controller, REG_CONTROL, Command);
    Command &= ~CONTROL_CAPTURE_BIT;
    IOWR(capture_controller, REG_CONTROL, Command);
    Command |= CONTROL_CAPTURE_BIT;
    IOWR(capture_controller, REG_CONTROL, Command);
}

bool cv::D8MCapture::read(OutputArray image)
{
    bool bSuccess = true;
    if (bSuccess == true)
        bSuccess = grab();
    if (bSuccess == true)
        bSuccess = retrieve(image);
    return bSuccess;
}

bool cv::D8MCapture::isOpened()
{
    return opened;
}

bool cv::D8MCapture::open(uint32_t capture_base, const char *capture_ram_device)
{
    int fd;
    fd = ::open(capture_ram_device,O_RDWR);
    if (fd < 0) {
        printf("ERROR: could not open %s...\n", capture_ram_device);
        return false;
    }

    if (ioctl(fd, 0, &capture_sdram_base) != 0) {
        printf("ERROR: could not read buffer phy...\n");
        close(fd);
        return false;
    }
    close(fd);

    if ((mem_fd = ::open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
        printf("ERROR: could not open \"/dev/mem\"...\n");
        return (1);
    }
    h2f_lw_virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE),
            MAP_SHARED, mem_fd, HW_REGS_BASE);
    if (h2f_lw_virtual_base == MAP_FAILED) {
        printf("ERROR: mmap() failed...\n");
        close(mem_fd);
        return false;
    }
    capture_sdram1 = (uint8_t *) mmap(NULL, BUFF_SPAN, (PROT_READ | PROT_WRITE),
            MAP_SHARED, mem_fd, capture_sdram_base);
    if (capture_sdram1 == MAP_FAILED) {
        printf("ERROR: axi mmap() failed...\n");
        close(mem_fd);
        return false;
    }
    capture_sdram2 = capture_sdram1 + (800 * 480 * 4);

    capture_controller = (uint32_t*) ((uint8_t*) h2f_lw_virtual_base
            + ((ALT_LWFPGASLVS_OFST + capture_base) & HW_REGS_MASK));

    IOWR(capture_controller, REG_MEM_ADDR, capture_sdram_base);
    start_capture();

    opened = true;
    return true;
}

void cv::D8MCapture::release()
{
    if (munmap(capture_sdram1, BUFF_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
    }
    if (munmap(h2f_lw_virtual_base, HW_REGS_SPAN) != 0) {
        printf("ERROR: munmap() fggailed...\n");
    }
    close(mem_fd);

    opened = false;
}

