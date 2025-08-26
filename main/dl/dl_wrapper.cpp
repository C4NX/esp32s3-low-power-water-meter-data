#include "dl_wrapper.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "esp_task_wdt.h"
#include "esp_timer.h"

#include "dl_detect_base.hpp"
#include "dl_detect_yolo11_postprocessor.hpp"
#include "dl_image_preprocessor.hpp"

const char* TAG = "dl_wrapper";

extern "C" void start_inference_task(void);

static void inference_task(void *arg) {
    ESP_LOGI(TAG, "Inference task started");
    int64_t t0 = esp_timer_get_time();
    int n = app_run_digit_detection_on_test_image();
    int64_t t1 = esp_timer_get_time();
    ESP_LOGI(TAG, "Inference done, results=%d, time=%.2f ms", n, (t1 - t0) / 1000.0);
    vTaskDelete(NULL);
}

extern "C" void start_inference_task(void) {
    xTaskCreatePinnedToCore(inference_task, "infer", 8192, NULL, 5, NULL, 0);
}

extern "C" {
    int app_run_digit_detection(uint8_t* buf, size_t len,
                           detection_result_t* results, int max_results) 
    {
        // Load image
        dl::image::jpeg_img_t jpeg_img = {.data = (void *)buf, .data_len = (size_t)(len)};
        ESP_LOGI(TAG, "Decoding JPEG, pointer: %p of size: %zu", jpeg_img.data, jpeg_img.data_len);
        auto img = sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
        if (!img.data) {
            ESP_LOGE(TAG, "JPEG decode failed (unsupported format or alloc failure)");
            return -1;
        }

        // Create (once) and run detection
        static WaterMeterDetect* detect = nullptr;
        if (!detect) {
            detect = new WaterMeterDetect("best");
        }

        ESP_LOGI(TAG, "Running detection...");
        auto &detect_results = detect->run(img);

        // Copy results into caller-provided array (clamped by max_results)
        int copied = 0;
        for (const auto &res : detect_results) {
            if (copied >= max_results) break;
            ESP_LOGI(TAG, "Found result %d: category=%d, score=%.2f, box=(%d,%d,%d,%d)", copied,
                     res.category, res.score, res.box[0], res.box[1], res.box[2], res.box[3]);
            results[copied].category = res.category;
            results[copied].score = res.score;
            results[copied].x1 = res.box[0];
            results[copied].y1 = res.box[1];
            results[copied].x2 = res.box[2];
            results[copied].y2 = res.box[3];
            copied++;

        }

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

        // Free decoded image buffer
        if (img.data) {
            heap_caps_free(img.data);
        }

        return copied;
    }
}


// Generated symbol (see build/best.espdl.S) is _binary_best_espdl_start (no directory prefix)
extern const uint8_t _binary_best_espdl_start[] asm("_binary_best_espdl_start");
extern const uint8_t _binary_best_espdl_end[]   asm("_binary_best_espdl_end");
static const uint8_t *g_model_aligned = nullptr; // allocated aligned copy (if needed)

WaterMeterDetect::WaterMeterDetect(const char *model_name) {
    // Force param_copy for speed (load params to PSRAM / internal RAM for faster conv)
    bool param_copy = true;

    const uint8_t *raw = _binary_best_espdl_start;
    size_t model_size = (size_t)(_binary_best_espdl_end - _binary_best_espdl_start);
    uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
    if (addr % 16 != 0) {
        // Allocate 16-byte aligned buffer and copy
        uint8_t *buf = (uint8_t *)heap_caps_aligned_alloc(16, model_size, MALLOC_CAP_DEFAULT);
        if (buf) {
            memcpy(buf, raw, model_size);
            g_model_aligned = buf;
            ESP_LOGI(TAG, "Copied model to 16-byte aligned buffer (%p -> %p, size=%u)", raw, buf, (unsigned)model_size);
            raw = g_model_aligned;
        } else {
            ESP_LOGW(TAG, "Failed to allocate aligned copy, continuing with unaligned model (may warn)" );
        }
    } else {
        g_model_aligned = raw; // already aligned
    }

    // Allocate base members
    this->m_model = new dl::Model((const char *)raw,
                                  model_name,
                                  fbs::MODEL_LOCATION_IN_FLASH_RODATA,
                                  0,
                                  dl::MEMORY_MANAGER_GREEDY,
                                  nullptr,
                                  param_copy);
    // Minimize model graph to drop unused ops and speed execution
    this->m_model->minimize();
#if CONFIG_IDF_TARGET_ESP32P4
    this->m_image_preprocessor = new dl::image::ImagePreprocessor(this->m_model, {0,0,0}, {255,255,255});
#else
    this->m_image_preprocessor = new dl::image::ImagePreprocessor(this->m_model, {0,0,0}, {255,255,255}, DL_IMAGE_CAP_RGB565_BIG_ENDIAN);
#endif
    this->m_postprocessor = new dl::detect::yolo11PostProcessor(this->m_model, 0.25, 0.7, 10, {{8,8,4,4},{16,16,8,8},{32,32,16,16}});
}

WaterMeterDetect::~WaterMeterDetect() {
    // Base class DetectImpl destructor will delete m_model, m_image_preprocessor, m_postprocessor
    // Free aligned copy if we allocated one (and it isn't pointing at flash)
    if (g_model_aligned && g_model_aligned != _binary_best_espdl_start) {
        heap_caps_free((void*)g_model_aligned);
        g_model_aligned = nullptr;
    }
}