#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

//Send test result.
#include "unittest_transport.h"

//Redirect UNITY functions
#define UNITY_OUTPUT_START()    unity_output_init()
#define UNITY_OUTPUT_CHAR(c)    unity_output_char(c)
#define UNITY_OUTPUT_FLUSH()    unity_output_flush()
#define UNITY_OUTPUT_COMPLETE() unity_output_complete()


#ifdef __cplusplus
}
#endif

#endif /* UNITY_CONFIG_H */
