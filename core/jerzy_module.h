#ifndef JERZY_MODULE_H
#define JERZY_MODULE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define JA_ABI 2u
#define JA_MAX_PARAMETERS 1024u
typedef struct {
    const char *id, *label, *unit, *choices;
    float minimum, maximum, initial;
    uint32_t integer;
} JAParameter;
typedef struct {
    uint32_t frame, size;
    uint8_t bytes[3];
} JAMidi;
typedef struct {
    uint32_t size, frames;
    double sample_rate, bpm, beat;
    uint32_t playing;
    const float *input[2];
    float *output[2];
    const float *parameters;
    const JAMidi *midi;
    uint32_t midi_count;
    const float *return_input[2];
    const float *control_parameters;
} JAProcess;
typedef struct {
    uint32_t size, abi;
    const char *id, *name, *version;
    uint32_t instrument, parameter_count;
    const JAParameter *parameters;
    void *(*create)(double sample_rate, uint32_t max_frames, const char *asset);
    void (*destroy)(void *);
    void (*process)(void *, const JAProcess *);
    uint32_t (*latency)(void *);
} JAModule;
typedef const JAModule *(*JAEntry)(void);
/* Export ja_module. Only create/destroy may allocate or access files.
   process must never throw, allocate, block, log or perform IO. */
#if defined(_WIN32)
#define JA_EXPORT __declspec(dllexport)
#else
#define JA_EXPORT __attribute__((visibility("default")))
#endif
JA_EXPORT const JAModule *ja_module(void);
#ifdef __cplusplus
}
#endif
#endif
