#pragma once

#include <stddef.h>

#if defined(_WIN32)
#define ANN_API __declspec(dllexport)
#else
#define ANN_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

ANN_API int init_dataset(const char* path);
ANN_API void free_dataset(void);
ANN_API size_t dataset_count(void);
ANN_API int dataset_dim(void);
ANN_API unsigned int dataset_label(int index);
ANN_API void ann_api(const float* query, int k, int* indices, float* distances);

#ifdef __cplusplus
}
#endif
