#include "ruuvi_driver_enabled_modules.h"
#if RT_SENSOR_ENABLED
#include "ruuvi_driver_error.h"
#include "ruuvi_driver_sensor.h"
#include "ruuvi_interface_log.h"
#include "ruuvi_task_flash.h"
#include "ruuvi_task_sensor.h"
#include "flashdb.h"
#include "macronix_flash.h"
#include "ruuvi_task_flashdb.h"


#include <string.h>
#include<stdio.h>
#include<stdlib.h>

#ifndef TASK_SENSOR_LOG_LEVEL
#define TASK_SENSOR_LOG_LEVEL RI_LOG_LEVEL_DEBUG
#endif

static inline void LOG (const char * const msg)
{
    ri_log (TASK_SENSOR_LOG_LEVEL, msg);
}

static inline void LOGD (const char * const msg)
{
    ri_log (RI_LOG_LEVEL_DEBUG, msg);
}

static inline void LOGHEX (const uint8_t * const msg, const size_t len)
{
    ri_log_hex (TASK_SENSOR_LOG_LEVEL, msg, len);
}

/* Key-Value database initialization
*
*       &kvdb: database object
*       "env": database name
*   partition: The flash partition name base on FAL. Please make sure it's in FAL partition table.
* &default_kv: The default KV nodes. It will auto add to KVDB when first initialize successfully.
*        NULL: The user data if you need, now is empty.
*/
rd_status_t init_fdb(rt_sensor_ctx_t * const sensor) {
    fdb_kvdb * kvdb = NULL;
    kvdb = get_kvdb_conn();
    struct fdb_blob blob;
    uint8_t sensor_config_fdb_enabled = 1;
    if (sensor_config_fdb_enabled == 0 || !kvdb) {
        return RD_SUCCESS;
    }
    /* default KV nodes */
    struct fdb_default_kv_node default_kv_table[] = {
        {"sensor_config_fdb_enabled", &sensor_config_fdb_enabled, sizeof(sensor_config_fdb_enabled)}};

    struct fdb_default_kv default_kv;
    default_kv.kvs = default_kv_table;

    char *partition;

    if (rt_macronix_flash_exists() == RD_SUCCESS)
    {
      partition = "fdb_kvdb2";
    }
    else
    {
      partition = "fdb_kvdb1";
    }
    fdb_err_t result = fdb_kvdb_init(kvdb, "env", partition, &default_kv, NULL);
    rt_macronix_high_performance_switch(false); //resetting high-power mode in case of factory reset
    
    if (result == FDB_NO_ERR)
    {
        fdb_kv_get_blob(kvdb, "sensor_config_fdb_enabled", fdb_blob_make(&blob, &sensor_config_fdb_enabled, sizeof(sensor_config_fdb_enabled)));
        if (blob.saved.len > 0 && sensor_config_fdb_enabled)
        {
            // activate logging
            // return app_enable_sensor_logging(NULL, false);
            return RD_SUCCESS;
        }
        // but return RD_SUCCESS
        return RD_SUCCESS;
    }
    return RD_ERROR_NOT_INITIALIZED;
}

/** @brief Initialize sensor CTX
 *
 * To initialize a sensor, initialization function, sensor bus and sensor handle must
 * be set. After initialization, sensor control structure is ready to use,
 * initial configuration is set to actual values on sensor.
 *
 * To configure the sensor, set the sensor configuration in struct and call
 * @ref rt_sensor_configure.
 *
 * @param[in] sensor Sensor to initialize.
 *
 * @retval RD_SUCCESS on success.
 * @retval RD_ERROR_NULL if sensor is NULL
 * @retval RD_ERROR_NOT_FOUND if sensor->handle is RD_HANDLE_UNUSED
 * @return error code from sensor on other error.
 */
rd_status_t rt_sensor_initialize (rt_sensor_ctx_t * const sensor)
{
    rd_status_t err_code = RD_SUCCESS;

    if ( (NULL == sensor) || (NULL == sensor->init))
    {
        err_code |= RD_ERROR_NULL;
    }
    else if (RD_HANDLE_UNUSED != sensor->handle)
    {
        err_code = sensor->init (& (sensor->sensor), sensor->bus, sensor->handle);
    }
    else
    {
        err_code |= RD_ERROR_NOT_FOUND;
    }
    err_code |= init_fdb(sensor);

    return err_code;
}

/** @brief Store the sensor state to NVM.
 *
 * @param[in] sensor Sensor to store.
 *
 * @return RD_SUCCESS on success.
 * @return RD_ERROR_NULL if sensor is NULL.
 * @return error code from sensor on other error.
 */
rd_status_t rt_sensor_store (rt_sensor_ctx_t * const sensor)
{
    rd_status_t err_code = RD_SUCCESS;

    if (NULL == sensor)
    {
        err_code |= RD_ERROR_NULL;
    }
    else if (rt_flash_busy())
    {
        err_code |= RD_ERROR_BUSY;
    }
    else
    {
        // err_code |= rt_flash_store (sensor->nvm_file, sensor->nvm_record,
                                    // & (sensor->configuration),
                                    // sizeof (sensor->configuration));
        err_code |= rt_sensor_store_to_fdb(get_kvdb_conn(), sensor);
    }

    return err_code;
}

/** @brief Load the sensor state from NVM.
 *
 * @param[in] sensor Sensor to store.
 *
 * @return RD_SUCCESS on success.
 * @return RD_ERROR_NULL if sensor is NULL.
 * @return error code from sensor on other error.
 */
rd_status_t rt_sensor_load (rt_sensor_ctx_t * const sensor)
{
    rd_status_t err_code = RD_SUCCESS;

    if (NULL == sensor)
    {
        err_code |= RD_ERROR_NULL;
    }
    else if (rt_flash_busy())
    {
        err_code |= RD_ERROR_BUSY;
    }
    else
    {
        // TODO: have to set fdb config before this point
        // err_code |= rt_flash_load (sensor->nvm_file, sensor->nvm_record,
                                //    & (sensor->configuration),
                                //    sizeof (sensor->configuration));
        err_code |= rt_sensor_get_from_fdb(get_kvdb_conn(), sensor);
        err_code |= rt_sensor_get_all_from_fdb(get_kvdb_conn(), sensor);

    }

    return err_code;
}

/** @brief Configure a sensor with given settings.
 *
 * @param[in,out] sensor In: Sensor to configure.
                         Out: Sensor->configuration will be set to actual configuration.
 *
 * @return RD_SUCCESS on success.
 * @return RD_ERROR_NULL if sensor is NULL.
 * @return error code from sensor on other error.
 */
rd_status_t rt_sensor_configure (rt_sensor_ctx_t * const ctx)
{
    rd_status_t err_code = RD_SUCCESS;

    if (NULL == ctx)
    {
        err_code |= RD_ERROR_NULL;
    }
    else if (!rd_sensor_is_init (& (ctx->sensor)))
    {
        err_code |= RD_ERROR_INVALID_STATE;
    }
    else
    {
        LOG ("\r\nAttempting to configure ");
        LOG (ctx->sensor.name);
        LOG (" with:\r\n");
        ri_log_sensor_configuration (TASK_SENSOR_LOG_LEVEL,
                                     & (ctx->configuration), "");
        err_code |= ctx->sensor.configuration_set (& (ctx->sensor),
                    & (ctx->configuration));
        LOG ("Actual configuration:\r\n");
        ri_log_sensor_configuration (TASK_SENSOR_LOG_LEVEL,
                                     & (ctx->configuration), "");
    }

    return err_code;
}

/**
 * @brief Read sensors and encode to given buffer in Ruuvi DF5.
 *
 * @param[in] buffer uint8_t array with length of 24 bytes.
 * @return RD_SUCCESS if data was encoded
 */
//ruuvi_endpoint_status_t task_sensor_encode_to_5 (uint8_t * const buffer);

/**
 * @brief Search for requested sensor backend in given list of sensors.
 *
 * @param[in] sensor_list Array of sensors to search the backend from.
 * @param[in] count Number of sensor backends in the list.
 * @param[in] name NULL-terminated, max 9-byte (including trailing NULL) string
 *                 representation of sensor.
 * @return pointer to requested sensor CTX if found
 * @return NULL if requested sensor was not found
 */
rt_sensor_ctx_t * rt_sensor_find_backend (rt_sensor_ctx_t * const sensor_list,
        const size_t count, const char * const name)
{
    rt_sensor_ctx_t * p_sensor = NULL;

    for (size_t ii = 0; (count > ii) && (NULL == p_sensor); ii++)
    {
        if (0 == strcmp (sensor_list[ii].sensor.name, name))
        {
            p_sensor = & (sensor_list[ii]);
        }
    }

    return p_sensor;
}

/**
 * @brief Search for a sensor which can provide requested values
 *
 * @param[in] sensor_list Array of sensors to search the backend from.
 * @param[in] count Number of sensor backends in the list.
 * @param[in] values Fields which sensor must provide.
 * @return Pointer to requested sensor if found. If there are many candidates, first is
 *         returned
 * @return NULL if requested sensor was not found.
 */
rt_sensor_ctx_t * rt_sensor_find_provider (rt_sensor_ctx_t * const
        sensor_list, const size_t count, rd_sensor_data_fields_t values)
{
    rt_sensor_ctx_t * p_sensor = NULL;

    for (size_t ii = 0; (count > ii) && (NULL == p_sensor); ii++)
    {
        if ( (values.bitfield & sensor_list[ii].sensor.provides.bitfield) == values.bitfield)
        {
            p_sensor = & (sensor_list[ii]);
        }
    }

    return p_sensor;
}

rd_status_t rt_sensor_store_to_fdb(fdb_kvdb_t kvdb, rt_sensor_ctx_t *sensor)
{
  struct fdb_blob blob;

// set the new created_at date
  sensor->configuration.created_at = rd_sensor_timestamp_get();
  { /* GET the KV value */
    /* get the "boot_count" KV value */
    fdb_kv_set_blob(kvdb, strcat(sensor->sensor.name, "_config"), fdb_blob_make(&blob, &sensor->configuration, sizeof(sensor->configuration)));
  }
  return RD_SUCCESS;
}

rd_status_t rt_sensor_get_from_fdb(fdb_kvdb_t kvdb, rt_sensor_ctx_t *sensor)
{

  struct fdb_blob blob;

    fdb_kv_get_blob(kvdb, strcat(sensor->sensor.name, "_config"), fdb_blob_make(&blob, &sensor->configuration, sizeof(sensor->configuration)));
  if (blob.saved.len > 0)
  {
    // printf("get the '' value is %d\n", boot_count);
    return RD_SUCCESS;
  }
  else
  {
    // make sure initializing the values will work - do not try this at home ;-)
    rt_sensor_store_to_fdb(kvdb, sensor);
    printf("get of some values of the sensor failed\n");
    return RD_ERROR_NOT_FOUND;
  }
}


rd_status_t rt_sensor_get_all_from_fdb(fdb_kvdb_t kvdb, rt_sensor_ctx_t *sensor)
{

    struct fdb_kv_iterator iterator;
    fdb_kv_t cur_kv;
    struct fdb_blob blob;
    size_t data_size;
    fdb_kv_iterator_init(&iterator);
    int i = 0;
    do {
        rd_sensor_configuration_t *configuration;
        // configuration = malloc(sizeof(rd_sensor_configuration_t));
        // memset(configuration, 0, sizeof(rd_sensor_configuration_t));
        cur_kv = &(iterator.curr_kv);
        data_size = (size_t) cur_kv->value_len;
        fdb_blob_read((fdb_db_t) kvdb, fdb_kv_to_blob(cur_kv, fdb_blob_make(&blob, configuration, data_size)));
        /*
         * balabala do what ever you like with blob...
         */
        // free(data_buf);
        sensor->historical_configurations[i] = configuration;
        i++;
    } while (fdb_kv_iterate(kvdb, &iterator));
    return RD_SUCCESS;
}
#endif