#include <opencv2/opencv.hpp>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <string.h>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "D8MCapture.h"
#include "hps_0.h"

#ifndef CAPTURE_RAM_DEVICE
#define CAPTURE_RAM_DEVICE "/dev/f2h-dma-memory"
#endif /* ifndef CAPTURE_RAM_DEVICE */

// Flag that we are not in overlay mode. Overlay mode takes two images and
// overlays one on top of the other. In "standard" mode, we display a single
// "original" image and allow its brightness and contrast to be adjusted.
bool overlay_mode = false;

std::string overlay_image_path = "overlay.png";

// brightness value to use on display images on a scale from 00 to 100
unsigned char brightness = 50;
unsigned char beta = 0;

// contrast value to use on display images on a scale from 00 to 100
unsigned char contrast = 50;
double alpha = 1.0;

cv::Mat overlay_image;

// image data buffers
unsigned char overlay_image_data[2000000] = {0x00};

int main()
{
        // Address of the PC you will be running the main GUI application on
    std::string server_address = "192.168.1.2";

    // The UDP port on which to conduct communications
    int port = 9999;

    // We will send this message to the PC when the embedded side of the
    // application starts up.
    char message[] = "This is the Terasic";
    int message_length = strlen(message);

    // Create a UDP socket that we can use to communicate with the PC
    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Bind the embedded side of the application to this port
    sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr= htonl(INADDR_ANY);
    client_address.sin_port = htons(port);
    bind(sock, (struct sockaddr *)&client_address, sizeof(client_address));

    // Send the message to the PC application
    sockaddr_in destination_address;
    memset(&destination_address, 0, sizeof(destination_address));
    destination_address.sin_family = AF_INET;
    destination_address.sin_port = htons(port);
    destination_address.sin_addr.s_addr = inet_addr(server_address.c_str());
    sendto(sock, message, message_length, 0, (sockaddr *) &destination_address, sizeof(destination_address));

    // Retrieve the overlay image that will be drawn over the top of the camera
    // image
    overlay_image = cv::imread(overlay_image_path, cv::IMREAD_UNCHANGED);

    // Create the Mat for the camera image and create the capture device
    cv::Mat camera_image;
    cv::D8MCapture * cap = new cv::D8MCapture(
        TV_DECODER_TERASIC_STREAM_CAPTURE_BASE,
        CAPTURE_RAM_DEVICE
    );

    // Grab one frame from the camera so we can get the size, then resize the
    // overlay image to match it
    cap->read(camera_image);
    cv::Size size = camera_image.size();
    cv::resize(overlay_image, overlay_image, size);

    // cv::imshow does not honor alpha channels so we have to do the image
    // blending on our own. Start by splitting the 4-channel RGBA overlay image
    // into its individual channels
    std::vector<cv::Mat> individual_channels;
    cv::split(overlay_image, individual_channels);

    // Reconstitute the RGB channels into a single RGB image without the alpha
    // channel
    cv::Mat overlay_image_3_channel[3] = {
        individual_channels[0],
        individual_channels[1],
        individual_channels[2]
    };
    cv::merge(overlay_image_3_channel, 3, overlay_image);

    // Place the alpha channel into a Mat by itself that will be used to mask
    // the image copy later
    cv::Mat overlay_image_mask;
    overlay_image_mask = individual_channels[3];

    cv::namedWindow("Final Project");

    // Now wait until we receive a response from the PC application
    char response[16536] = {'\0'};
    bool application_running = true;

    while (application_running)
    {
        fd_set rfds;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1;

        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);

        int status = select(sock + 1, &rfds, NULL, NULL, &tv);

        if(status == -1)
        {
            application_running = false;
        }

        if (status > 0)
        {
            recv(sock, (void *)response, sizeof(response), 0);

            // The first byte of the received data determines the type of message
            // and how to interpret the rest of the payload
            switch(response[0])
            {
                // receive_overlay_image
                case 'V':
                {
                    // The first byte of a message after the type-byte is an
                    // indicator of whether or not this is the final chunk of an
                    // image.
                    unsigned char final_chunk = response[1];

                    // The next 4 bytes of the message header tell the offset byte
                    // within the image from where the data begins. This tells us
                    // where within our image data buffer to being copying data
                    // bytes.
                    unsigned long offset = (
                        ((unsigned char)response[5] << 24) +
                        ((unsigned char)response[4] << 16) +
                        ((unsigned char)response[3] << 8) +
                        (unsigned char)response[2]
                    );

                    // The final 4-bytes of the 10-byte message header tells us the
                    // length of the image data that follows.
                    unsigned long length = (
                        ((unsigned char)response[9] << 24) +
                        ((unsigned char)response[8] << 16) +
                        ((unsigned char)response[7] << 8) +
                        (unsigned char)response[6]
                    );

                    std::cout << "Received " << length << " bytes to be copied to " << offset << std::endl;

                    // Now that we know the offset and the length, copy the image
                    // data chunk to the buffer that holds the complete overlay
                    // image
                    //std::cout << "Received an overlay image chunk" << std::endl;
                    memcpy(&overlay_image_data[offset], &response[10], length);

                    if (final_chunk)
                    {
                        int image_length = offset + length;
                        std::cout << "Received final image chunk, image is " << image_length << " bytes long." << std::endl;
                        // since we have the complete image, write it to the file now
                        overlay_image = cv::Mat(480, 800, CV_8UC4, overlay_image_data);
                        cv::resize(overlay_image, overlay_image, size);
                        std::cout << "Assigned the data to the image" << std::endl;

                        // cv::imshow does not honor alpha channels so we have to do the image
                        // blending on our own. Start by splitting the 4-channel RGBA overlay image
                        // into its individual channels
                        cv::split(overlay_image, individual_channels);
                        std::cout << "Split the new data into channels" << std::endl;

                        // Reconstitute the RGB channels into a single RGB image without the alpha
                        // channel
                        cv::Mat overlay_image_3_channel[3] = {
                            individual_channels[0],
                            individual_channels[1],
                            individual_channels[2]
                        };
                        cv::merge(overlay_image_3_channel, 3, overlay_image);
                        std::cout << "Merged the channel data into an image with no alpha" << std::endl;

                        // Place the alpha channel into a Mat by itself that will be used to mask
                        // the image copy later
                        overlay_image_mask = individual_channels[3];
                        std::cout << "Set the mask with the new alpha data" << std::endl;
                    }
                }
                break;

                // enable_overlay
                case 'E':
                {
                    // flag that we are now in overlay mode
                    overlay_mode = true;
                }
                break;

                // disable_overlay
                case 'D':
                {
                    // flag that we are no longer in overlay mode
                    overlay_mode = false;
                }
                break;

                // adjust_brightness
                case 'B':
                {
                    // The first byte of a message after the type-byte is the
                    // brightness value
                    brightness = response[1];
                    beta = (unsigned char)(brightness / 100.0 * 50 - 25);
                }
                break;

                // adjust_contrast
                case 'C':
                {
                    // The first byte of a message after the type-byte is the
                    // contrast value
                    contrast = response[1];
                    alpha = contrast / 100.0 * 50 - 25;
                }
                break;

                // quit application
                case 'Q':
                {
                    application_running = false;
                }
                break;
            }

            // clear out the response buffer in preparation to receive the next chunk of data
            memset(&response, 0, sizeof(response));
        }

        // read one frame from the camera
        cap->read(camera_image);

        // Remove the alpha channel from the camera image
        cv::cvtColor(camera_image, camera_image, cv::COLOR_RGBA2RGB);

        if (overlay_mode)
        {
            // copy the overlay image over the camera image, using the overlay
            // image's alpha channel data as a mask to determine which data to copy
            // and which data to ignore
            overlay_image.copyTo(
                camera_image(
                    cv::Rect(0, 0, overlay_image.cols, overlay_image.rows)
                ),
                overlay_image_mask
            );
        }

        // Apply the brightness and contrast settings to the image
        camera_image.convertTo(camera_image, -1 , alpha, beta);

        // draw the final output image
        cv::imshow("Final Project", camera_image);
        cv::waitKey(10);
    }

    return 0;
}
