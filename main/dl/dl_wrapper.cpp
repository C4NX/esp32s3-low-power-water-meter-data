#include "dl_wrapper.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstdio>
#include <cstdint>

#include "dl_detect_base.hpp"
#include "dl_detect_yolo11_postprocessor.hpp"
#include "dl_image_preprocessor.hpp"

const char* TAG = "dl_wrapper";

extern "C" {
    int app_run_digit_detection(uint8_t* buf, size_t len,
                           detection_result_t* results, int max_results) 
    {

        // Load image
        dl::image::jpeg_img_t jpeg_img = {.data = (void *)buf, .data_len = (size_t)(len)};
        auto img = sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);

        // Create and run detection
        WaterMeterDetect* detect = new WaterMeterDetect("best");
        auto &detect_results = detect->run(img);

        // Log results
        for (const auto &res : detect_results) {
            ESP_LOGI(TAG,
                     "[category: %d, score: %f, x1: %d, y1: %d, x2: %d, y2: %d]",
                     res.category,
                     res.score,
                     res.box[0],
                     res.box[1],
                     res.box[2],
                     res.box[3]);
        }

        // Cleanup
        delete detect;

        return detect_results.size();
    }
}


WaterMeterDetect::WaterMeterDetect(const char *model_name)
{
    bool param_copy = true;
    if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) < 1024 * 1024 * 9) {
        param_copy = false;
    }
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/littlefs/model/%s.espdl", model_name);
    m_model = new dl::Model(full_path,
                            model_name,
                            fbs::MODEL_LOCATION_IN_FLASH_PARTITION,
                            0,
                            dl::MEMORY_MANAGER_GREEDY,
                            nullptr,
                            param_copy);
    m_model->minimize();
#if CONFIG_IDF_TARGET_ESP32P4
    m_image_preprocessor = new dl::image::ImagePreprocessor(m_model, {0, 0, 0}, {255, 255, 255});
#else
    m_image_preprocessor =
        new dl::image::ImagePreprocessor(m_model, {0, 0, 0}, {255, 255, 255}, DL_IMAGE_CAP_RGB565_BIG_ENDIAN);
#endif
    m_postprocessor =
        new dl::detect::yolo11PostProcessor(m_model, 0.25, 0.7, 10, {{8, 8, 4, 4}, {16, 16, 8, 8}, {32, 32, 16, 16}});
}

WaterMeterDetect::~WaterMeterDetect() {
    delete m_postprocessor;
    delete m_image_preprocessor;
    delete m_model;
}