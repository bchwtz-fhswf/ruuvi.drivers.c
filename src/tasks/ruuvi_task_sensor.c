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

static fdb_kvdb *kvdb = NULL;

/* Singleton for global kvdb conn */
fdb_kvdb * get_kvdb_conn() {
    if (!kvdb)
        kvdb = malloc(sizeof(struct fdb_kvdb));
    return kvdb;
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
    struct fdb_blob blob;
    uint8_t sensor_config_fdb_enabled = 0;

    /* default KV nodes */
    struct fdb_default_kv_node default_kv_table[] = {
        {"sensor_config_fdb_enabled", &sensor_config_fdb_enabled, sizeof(sensor_config_fdb_enabled)}};

    struct fdb_default_kv default_kv;
    default_kv.kvs = default_kv_table;

    char* partition_name;
    partition_name = malloc(strlen("sensor_")+strlen(sensor->sensor.name)+strlen("_config"));
    strcpy(partition_name, "sensor_");
    strcat(partition_name, sensor->sensor.name);
    strcat(partition_name, "_config");
    fdb_err_t result = fdb_kvdb_init(&kvdb, "env", partition_name, &default_kv, NULL);
    rt_macronix_high_performance_switch(false); //resetting high-power mode in case of factory reset
    free(partition_name);
    if (result == FDB_NO_ERR)
    {

        fdb_kv_get_blob(&kvdb, "sensor_config_fdb_enabled", fdb_blob_make(&blob, &sensor_config_fdb_enabled, sizeof(sensor_config_fdb_enabled)));
        if (blob.saved.len > 0 && sensor_config_fdb_enabled)
        {
            // activate logging
            // return app_enable_sensor_logging(NULL, false);
            return RD_SUCCESS;
        }
        else
        {
        // but return RD_SUCCESS
        return RD_SUCCESS;
        }
    }
    return RD_ERROR_FATAL;
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
        err_code |= rt_flash_store (sensor->nvm_file, sensor->nvm_record,
                                    & (sensor->configuration),
                                    sizeof (sensor->configuration));
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
        err_code |= rt_flash_load (sensor->nvm_file, sensor->nvm_record,
                                   & (sensor->configuration),
                                   sizeof (sensor->configuration));
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
#endif