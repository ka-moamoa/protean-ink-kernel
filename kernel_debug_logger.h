#ifndef KERNEL_DEBUG_LOGGER_H_
#define KERNEL_DEBUG_LOGGER_H_

#if KERNEL_DEBUG_PRINT
#define KERNEL_LOG_DEBUG(...) \
{ \
    do { \
        printf("[KERNEL_DEBUG] --> %s:%d\t| ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n");} \
    while (0); \
}

#else
#define KERNEL_LOG_DEBUG(...)
#endif

#if KERNEL_INFO_PRINT
#define KERNEL_LOG_INFO(...) \
{ \
    do { \
        printf("[KERNEL_INFO] --> %s:%d\t| ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n");} \
    while (0); \
}

#else
#define KERNEL_LOG_INFO(...)
#endif

#if KERNEL_WARN_PRINT
#define KERNEL_LOG_WARNING(...) \
{ \
    do { \
        printf("[KERNEL_WARNING] --> %s:%d\t| ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n");} \
    while (0); \
}

#else
#define KERNEL_LOG_WARNING(...)
#endif

#if KERNEL_ERROR_PRINT
#define KERNEL_LOG_ERROR(...) \
{ \
    do { \
        printf("[KERNEL_ERROR] --> %s:%d\t| ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("!!!\n");} \
    while (0); \
}

#else
#define KERNEL_LOG_ERROR(...)
#endif

#endif // KERNEL_DEBUG_LOGGER_H_