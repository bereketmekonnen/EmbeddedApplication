#ifndef D8MCAPTURE_H_
#define D8MCAPTURE_H_

#include <stdint.h>
#include <opencv2/opencv.hpp>

namespace cv {

class D8MCapture {
    public:
    D8MCapture();
    D8MCapture(uint32_t capture_base, const char *capture_ram_device);
    bool grab();
    bool isOpened();
    bool open(uint32_t capture_base, const char *capture_ram_device);
    bool read(OutputArray image);
    bool retrieve(OutputArray image, int flag = 0);
    void release();
    virtual ~D8MCapture();
    //D8MCapture & operator>>(Mat &image);

    private:
    bool opened;
    int mem_fd;
    void *h2f_lw_virtual_base;
    uint32_t *capture_controller = NULL;
    int frame_index;
    uint8_t *capture_sdram1 = NULL;
    uint8_t *capture_sdram2 = NULL;
    uint32_t capture_sdram_base;
    uint32_t capture_status;
    bool wait_done(int timeout_s);
    void start_capture();
};

} /* namespace cv */

#endif /* D8MCAPTURE_H_ */
