#ifndef PERCEPTRON_H
#define PERCEPTRON_H

#include <cstdint>
#include <cstring>

template <int N_FEAT, int W_MAX = 127, int W_MIN = -128>
struct Perceptron {
    int16_t  weights[N_FEAT];
    int32_t  prev_quant[N_FEAT];
    int32_t  prev_sum;
    double   prev_label_metric;
    uint32_t call_count;
    bool     inited;

    void init() {
        memset(weights, 0, sizeof(weights));
        memset(prev_quant, 0, sizeof(prev_quant));
        prev_sum = 0;
        prev_label_metric = 0.0;
        call_count = 0;
        inited = true;
    }

    // Quantize features [0,1] → [0,127], compute dot product
    int32_t infer(const double feat[N_FEAT], int32_t quant_out[N_FEAT]) const {
        int32_t sum = 0;
        for (int i = 0; i < N_FEAT; i++) {
            double c = feat[i];
            if (c < 0.0) c = 0.0;
            if (c > 1.0) c = 1.0;
            quant_out[i] = (int32_t)(c * 127.0);
            sum += weights[i] * quant_out[i];
        }
        return sum;
    }

    // Signed quantize: feat*128-64 (model 700 style)
    int32_t infer_signed(const double feat[N_FEAT], int32_t quant_out[N_FEAT]) const {
        int32_t sum = 0;
        for (int i = 0; i < N_FEAT; i++) {
            quant_out[i] = (int32_t)(feat[i] * 128.0 - 64.0);
            sum += weights[i] * quant_out[i];
        }
        return sum;
    }

    // POPET-style train: label ∈ {+1,-1}, theta = confidence threshold
    void train(int8_t label, int32_t theta) {
        int32_t abs_sum = (prev_sum < 0) ? -prev_sum : prev_sum;
        int8_t pred_sign = (prev_sum > 0) ? +1 : -1;
        if (pred_sign != label || abs_sum < theta) {
            for (int i = 0; i < N_FEAT; i++) {
                int32_t x = prev_quant[i];
                int32_t update = label * ((x > 32) ? 2 : 1);
                int32_t nw = weights[i] + update;
                if (nw > W_MAX) nw = W_MAX;
                if (nw < W_MIN) nw = W_MIN;
                weights[i] = (int16_t)nw;
            }
        }
    }

    // Simple train: label ∈ {+1,-1}, sign-only update (model 700 style)
    void train_sign(int8_t label, int32_t confidence_floor) {
        int32_t abs_sum = (prev_sum < 0) ? -prev_sum : prev_sum;
        int8_t pred_sign = (prev_sum > 0) ? +1 : -1;
        if (pred_sign != label || abs_sum < confidence_floor) {
            for (int i = 0; i < N_FEAT; i++) {
                int32_t x = prev_quant[i];
                int32_t update = label * (x > 0 ? 1 : -1);
                int32_t nw = weights[i] + update;
                if (nw > W_MAX) nw = W_MAX;
                if (nw < W_MIN) nw = W_MIN;
                weights[i] = (int16_t)nw;
            }
        }
    }

    void snapshot(int32_t sum, const int32_t quant[N_FEAT], double metric) {
        prev_sum = sum;
        memcpy(prev_quant, quant, sizeof(int32_t) * N_FEAT);
        prev_label_metric = metric;
    }
};

#endif
