#ifndef  RUUVI_TASK_FLASH_H
#define  RUUVI_TASK_FLASH_H

/**
 * @defgroup peripheral_tasks Peripheral tasks
 */
/** @{ */
/**
 * @defgroup flash_tasks Flash tasks
 * @brief Non-volatile storage functions.
 *
 */
/** @} */
/**
 * @addtogroup flash_tasks
 */
/** @{ */
/**
 * @file ruuvi_task_flash.h
 * @author Otso Jousimaa <otso@ojousima.net>
 * @date 2020-02-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 *
 * Store and load data to/from persistent storage.
 * Typical usage:
 *
 * @code{.c}
 *  rd_status_t err_code = RD_SUCCESS;
 *  err_code = rt_flash_init();
 *  RD_ERROR_CHECK(err_code, RD_SUCCESS;
 *  char data[] = "Hello Flash!"
 *  err_code = rt_flash_store(1, 1, data, sizeof(data));
 *  RD_ERROR_CHECK(err_code, RD_SUCCESS;
 *  while(rt_flash_busy());
 *  char load[20];
 *  err_code = rt_flash_load(1, 1, load, sizeof(data));
 *  RD_ERROR_CHECK(err_code, RD_SUCCESS;
 *  while(rt_flash_busy());
 *  err_code = rt_flash_free(1, 1);
 *  RD_ERROR_CHECK(err_code, RD_SUCCESS;
 *  err_code = rt_flash_gc_run();
 *  RD_ERROR_CHECK(err_code, RD_SUCCESS;
 * @endcode
 */

#include "ruuvi_driver_error.h"
#include "ruuvi_driver_sensor.h"
#include "ruuvi_interface_log.h"

/**
 * @brief Initialize flash storage.
 *
 * If flash initialization fails, flash is purged and device tries to enter bootloader.
 *
 * @retval RD_SUCCESS on success
 * @retval RD_ERROR_INVALID_STATE if flash is already initialized
 * @warning Erases entire flash storage and reboots on failure.
 */
rd_status_t rt_flash_init (void);

/**
 * @brief Store data to flash.
 *
 * @param[in] key to store
 * @param[in] message Data to store. Must be aligned to a 4-byte boundary.
 * @param[in] message_length Length of stored data. Maximum 4000 bytes per record on nRF52.
 *
 * @retval RD_SUCCESS on success.
 * @retval RD_ERROR_NULL if data pointer is NULL.
 * @retval RD_ERROR_INVALID_STATE if flash is not initialized.
 * @retval RD_ERROR_BUSY if another operation was ongoing.
 * @retval RD_ERROR_NO_MEM if there was no space for the record in flash.
 * @retval RD_ERROR_DATA_SIZE if record exceeds maximum size.
 *
 * @warning triggers garbage collection if there is no space available, which leads to
 *          long processing time.
 */
rd_status_t rt_flash_store (const char * const key,
                            const void * const message, const size_t message_length);

/**
 * @brief Load data from flash.
 *
 * @param[in] key to load
 * @param[in] message Data to load. Must be aligned to a 4-byte boundary.
 * @param[in] message_length Length of loaded data. Maximum 4000 bytes per record on nRF52.
 *
 * @retval RD_SUCCESS on success.
 * @retval RD_ERROR_NULL if data pointer is NULL.
 * @retval RD_ERROR_INVALID_STATE if flash is not initialized.
 * @retval RD_ERROR_BUSY if another operation was ongoing.
 * @retval RD_ERROR_NOT_FOUND if given record was not found.
 * @retval RD_ERROR_DATA_SIZE if record exceeds maximum size.
 *
 * @warning Triggers garbage collection if there is no space available,
 *          which leads to long processing time.
 */
rd_status_t rt_flash_load (const char * const key,
                           void * const message, const size_t message_length);

/**
 * @brief Free data from flash.
 *
 * @param[in] key to delete
 *
 * @retval RD_SUCCESS on success.
 * @retval RD_ERROR_INVALID_STATE if flash is not initialized.
 * @retval RD_ERROR_BUSY if another operation was ongoing.
 * @retval RD_ERROR_NOT_FOUND if given record was not found.
 *
 * @warning Triggers garbage collection if there is no space available, which leads to
 *          long processing time.
 */
rd_status_t rt_flash_free (const char * const key);

#ifdef CEEDLING
// Give Ceedling access to internal functions.
void print_error_cause (void);
#endif


/** @} */
#endif
