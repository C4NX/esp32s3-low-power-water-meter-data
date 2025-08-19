#ifndef DL_WRAPPER_H
#define DL_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
#include <cstddef>
using std::size_t;

extern "C" {
#else
#include <stddef.h>
#endif

/**
 * Structure to hold a single detection result
 */
typedef struct {
    int category;
    float score;
    int x1, y1, x2, y2;
} detection_result_t;

/**
 * Run digit detection on a JPEG buffer.
 * @param buf      Pointer to JPEG data
 * @param len      Length of JPEG data
 * @param results  Array of detection_result_t (pre-allocated by caller)
 * @param max_results  Max number of results the array can hold
 * @return         Number of detected objects (could be 0), or -1 on error
 */
int app_run_digit_detection(uint8_t* buf, size_t len,
                       detection_result_t* results, int max_results);

#ifdef __cplusplus
}

// C++ helpers -------------------------------------------------------------

// Include (or forward declare) esp-dl types needed for the C++ wrapper.
// We only need pointer members here, so forward declarations would be enough;
// however, including the minimal headers ensures base classes are visible for
// inheritance (DetectWrapper) without relying on transitive includes in the
// .cpp file.
#include "dl_detect_base.hpp"              // dl::detect::DetectWrapper
#include "dl_detect_yolo11_postprocessor.hpp" // dl::detect::yolo11PostProcessor
#include "dl_image_preprocessor.hpp"       // dl::image::ImagePreprocessor
#include "dl_model_base.hpp"               // dl::Model

/**
 * Lightweight wrapper for loading a YOLO11 model from LittleFS and preparing
 * pre/post processors. Only construction logic has been added so far; extend
 * with inference helpers as needed.
 */

class WaterMeterDetect : public dl::detect::DetectWrapper {
public:
    /**
     * Construct the wrapper.
     * @param model_name         File name (or relative path under base) of the model (will be "/littlefs/model/<model_name>.espdl").
     */
    WaterMeterDetect(const char *model_name);
    ~WaterMeterDetect();

private:
    dl::Model *m_model {nullptr};
    dl::image::ImagePreprocessor *m_image_preprocessor {nullptr};
    dl::detect::yolo11PostProcessor *m_postprocessor {nullptr};
};
#endif

#endif // DL_WRAPPER_H