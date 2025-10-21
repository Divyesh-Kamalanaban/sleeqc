#pragma once
#include "model_data.h"
#include "esp_log.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
// #include "tensorflow/lite/version.h"

#define TAG "TFLITE"
constexpr int kTensorArenaSize = 20 * 1024;
static uint8_t tensor_arena[kTensorArenaSize];

class TFLiteRunner {
public:
    TFLiteRunner() : interpreter(nullptr), input(nullptr), output(nullptr) {}

    void init() {
        model = tflite::GetModel(adaptive_pqc_model_lite_tflite);

        // Register only the ops your model uses
        static tflite::MicroMutableOpResolver<10> resolver; // 10 = max ops
        resolver.AddFullyConnected();
        resolver.AddRelu();
        resolver.AddReshape();
        resolver.AddQuantize();
        resolver.AddDequantize();
        resolver.AddLogistic();
        // add more ops if your model needs them

        static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, sizeof(tensor_arena));
        interpreter = &static_interpreter;
        interpreter->AllocateTensors();

        input = interpreter->input(0);
        output = interpreter->output(0);

        ESP_LOGI(TAG, "TFLite model loaded. Input dims: %d, Output dims: %d",
                 input->dims->size, output->dims->size);
    }

    int predict(float free_heap_kb, float sign_time_ms, float stack_hwm) {
        input->data.f[0] = free_heap_kb;
        input->data.f[1] = sign_time_ms;
        input->data.f[2] = stack_hwm;

        if (interpreter->Invoke() != kTfLiteOk) {
            ESP_LOGE(TAG, "Invoke failed");
            return 0;  // default to Dilithium2
        }

        float val = output->data.f[0];
        int decision = (val > 0.5f) ? 1 : 0; // 1 = Dilithium5, 0 = Dilithium2
        ESP_LOGI(TAG, "ML predicted: %.2f → Using Dilithium%d", val, decision ? 5 : 2);
        return decision;
    }

private:
    const tflite::Model* model;
    tflite::MicroInterpreter* interpreter;
    TfLiteTensor* input;
    TfLiteTensor* output;
};
