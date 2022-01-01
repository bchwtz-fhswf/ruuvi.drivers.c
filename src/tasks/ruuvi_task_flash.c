/**
 * @addtogroup flash_tasks
 */
/*@{*/
/**
 * @file ruuvi_task_flash.c
 * @author Otso Jousimaa <otso@ojousima.net>
 * @copyright 2019 Ruuvi Innovations Ltd, license BSD-3-Clause.
 * @date 2019-11-18 provide compile time choice incase no flash is available
 * This module has 2 sets of code:
 *  If flash is enabled:
 *    on_error: In the event of a fatal error, the error, source file and line number are stored in an error file in flash.
 *              Then calls the bootloader, failing that reset.
 *    task_flash_init: calls print_error_cause which retrieves error file from flash and logs it(requires nRF52 DK board).
 *              Then sets up on_error as the call back error handler.
 *  If no flash:
 *    on_error: In the event of a fatal error calls the bootloader, failing that reset.
 *    task_flash_init: sets up on_error as the call back error handler.
 * These are in addition to flash utility functions of load, free, is busy and gc_run(which yields until not busy)
 */

#include "ruuvi_driver_error.h"
#include "ruuvi_driver_sensor.h"
#include "ruuvi_interface_log.h"
#include "ruuvi_interface_power.h"
#include "ruuvi_interface_yield.h"
#include "ruuvi_task_flash.h"
#include "flashdb.h"
#include "ruuvi_task_flashdb.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#ifndef TASK_FLASH_LOG_LEVEL
#define TASK_FLASH_LOG_LEVEL RI_LOG_LEVEL_INFO
#endif

#if RI_LOG_ENABLED
#include <stdio.h>
#include <stdarg.h>

static inline void LOG (const char * const msg)
{
    ri_log (RI_LOG_LEVEL_INFO, msg);
}

static inline void LOGD (const char * const msg)
{
    ri_log (RI_LOG_LEVEL_DEBUG, msg);
}

static inline void LOGDf (const char * const msg, ...)
{
    char fmsg[RD_LOG_BUFFER_SIZE];
    va_list args;
    *fmsg = 0;
    va_start(args, msg);
    vsnprintf(fmsg, RD_LOG_BUFFER_SIZE, msg, args);
    va_end(args);
    ri_log (RI_LOG_LEVEL_DEBUG, fmsg);
}
#else
#define LOG(...) 
#define LOGD(...)
#define LOGDf(...)
#define snprintf(...)
#endif

/* KVDB object */
static struct fdb_kvdb kvdb = { 0 };


typedef struct
{
    rd_status_t error;
    char filename[32];
    int line;
} rt_flash_error_cause_t;

#if 0
static void on_error (const rd_status_t err,
                      const bool fatal,
                      const char * file,
                      const int line)
{
    if (!fatal)
    {
        return;
    }

    error_cause_t error = {.error = err, .line = line };
    rd_status_t err_code;
    uint32_t timeout = 0;
    strncpy (error.filename, file, sizeof (error.filename));
    // Store reason of fatal error
    err_code = rt_flash_store (APPLICATION_FLASH_ERROR_FILE,
                               APPLICATION_FLASH_ERROR_RECORD,
                               &error, sizeof (error));

    // Wait for flash store op to complete
    while (RD_SUCCESS == err_code &&
            timeout < 1000 &&
            ri_flash_is_busy())
    {
        timeout++;
        // Use microsecond wait to busyloop instead of millisecond wait to low-power sleep
        // as low-power sleep may hang on interrupt context.
        ri_delay_us (1000);
    }

    // Try to enter bootloader, if that fails reset.
    ri_power_enter_bootloader();
    ri_power_reset();
}
#endif

#ifndef CEEDLING
static
#endif
void print_error_cause (void)
{

}

rd_status_t rt_flash_init (void)
{
    rd_status_t err_code = RD_SUCCESS;

    // Init FlashDB, which causes initialization of Macronix driver and SPI bus
    fal_flash_init();

    rt_macronix_high_performance_switch(true);
    fdb_err_t result = fdb_kvdb_init(&kvdb, "environment", "env_kvdb", NULL, NULL);
    rt_macronix_high_performance_switch(false);

    if(result!=FDB_NO_ERR) {
      err_code = rt_flashdb_to_ruuvi_error(result);
    }

    fdb_kv_print(&kvdb);

    return err_code;
}

rd_status_t rt_flash_store (const char * const key,
                            const void * const message, const size_t message_length)
{
    rd_status_t err_code = RD_SUCCESS;
    struct fdb_blob blob;

    LOGDf("Save %s size %d\r\n", key, message_length);

    fdb_err_t result = fdb_kv_set_blob(&kvdb, key, fdb_blob_make(&blob, message, message_length));

    if(result!=FDB_NO_ERR) {
      err_code = rt_flashdb_to_ruuvi_error(result);
    }

    return err_code;
}

rd_status_t rt_flash_load (const char * const key,
                           void * const message, const size_t message_length)
{
    rd_status_t err_code = RD_SUCCESS;
    struct fdb_blob blob;

    LOGDf("Load %s size %d\r\n", key, message_length);

    size_t size = fdb_kv_get_blob(&kvdb, key, fdb_blob_make(&blob, message, message_length));

    if(size==0) {
      LOGDf("%s NOT FOUND\r\n", key);
      return RD_ERROR_NOT_FOUND;
    }

    if(blob.saved.len!=message_length) {
      err_code = RD_ERROR_DATA_SIZE;
    }

    return err_code;
}

rd_status_t rt_flash_free (const char * const key)
{
    rd_status_t err_code = RD_SUCCESS;
    fdb_err_t result = fdb_kv_del(&kvdb, key);

    LOGDf("Delete %s size %d\r\n", key);

    if(result!=FDB_NO_ERR) {
      err_code = rt_flashdb_to_ruuvi_error(result);
    }

    return err_code;
}

/*@}*/
