#ifndef  RUUVI_TASK_FLASHDB_H
#define  RUUVI_TASK_FLASHDB_H

#include "ruuvi_driver_error.h"
#include "flashdb.h"

typedef struct fdb_kvdb fdb_kvdb;
typedef struct fb_tsdb fdb_tsdb;

/** @brief Initialize sensor db
 *
 * To interact with the flashdb provided by the sensor, a connection needs to be fetched.
 * This function holds the global connection and acts as a singleton.
 *
 * @return fdb_kvdb * as singleton
 */
fdb_kvdb * get_kvdb_conn();

/** @brief Initialize sensor db
 *
 * To interact with the flashdb provided by the sensor, a connection needs to be fetched.
 * This function holds the global connection and acts as a singleton.
 *
 * @return fdb_kvdb * as singleton
 */
fdb_tsdb * get_tsdb_conn();

/*
 *  This function converts an error code of FlashDB to an Ruuvi error code.
 *
 *  @param[in] err_code Error code of FlashDB.
 *  @return Ruuvi error code which represents the state of FlashDB.
 */
rd_status_t rt_flashdb_to_ruuvi_error(fdb_err_t fdb_err);

/*
 *  This function checks if Macronix Flash Chip is available.
 *
 *  @param[in] err_code Error code of FlashDB.
 *  @return RD_SUCCESS | If Macronix Flash is available.
 *  @return RD_ERROR_NOT_FOUND | If Macronix Flash is not available.
 *  @return RD_ERROR_NOT_INITIALIZED | If the module is currently not initialzed.
 */
rd_status_t rt_macronix_flash_exists(void);

/*
 * This functions sends the Chip Erase command to the macronix flash chip if 
 * the chip is available. This function is called during factory reset.
 */
rd_status_t rt_reset_macronix_flash(void);

/*
 *  This function checks if Macronix Flash Chip is available.
 *
 *  @param[in] err_code Error code of FlashDB.
 */
void rt_macronix_high_performance_switch(const bool enable);

#endif
