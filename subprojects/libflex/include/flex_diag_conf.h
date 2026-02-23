/// \file flex_diag_conf.h
// Copyright (c) 2025, Myriota Pty Ltd, All Rights Reserved
// SPDX-License-Identifier: BSD-3-Clause-Attribution
//
// This file is licensed under the BSD with attribution  (the "License"); you
// may not use these files except in compliance with the License.
//
// You may obtain a copy of the License here:
// LICENSE-BSD-3-Clause-Attribution.txt and at
// https://spdx.org/licenses/BSD-3-Clause-Attribution.html
//
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FLEX_DIAG_CONF_H
#define FLEX_DIAG_CONF_H

#include <stdbool.h>
#include <stdint.h>

/// @defgroup DiagConf Diagnostics and Configuration
/// @brief User defined Diagnostics and Configuration fields
///@{

// clang-format off
/// Begins recording the diagnostic and configuration table
/// \note The collective length of all item names, and string values including
/// terminators is 512 bytes and 256 bytes respectively.
#define FLEX_DIAG_CONF_TABLE_BEGIN() \
  FLEX_DiagConfTableItem *diag_conf_table_get_(void) { \
    static FLEX_DiagConfTableItem table[] = {

/// Ends recording the diagnostic and configuration table
#define FLEX_DIAG_CONF_TABLE_END() \
    {0}}; \
    return table; \
  }
// clang-format on

/// Adds a boolean to the diagnostic and configuration table
/// \param id The id of diagnostic or configuration of type FLEX_DiagConfID.
/// \param name The name of diagnostic or configuration.
/// \param default_value The default_value of the diagnostic or configuration.
/// \param type If the table item is a configuration, diagnostic, or presistent diagnostic. See
/// FLEX_DiagConfType.
#define FLEX_DIAG_CONF_TABLE_BOOL_ADD(id, name, default_value, type) \
  { id, FLEX_DIAG_CONF_VALUE_TAG_B8, type, 0, name, (void *)default_value }

/// Adds a float to the diagnostic and configuration table
/// \param id The id of diagnostic or configuration of type FLEX_DiagConfID.
/// \param name The name of diagnostic or configuration.
/// \param default_value The default_value of the diagnostic or configuration.
/// \param type If the table item is a configuration, diagnostic, or presistent diagnostic. See
/// FLEX_DiagConfType.
#define FLEX_DIAG_CONF_TABLE_FLOAT_ADD(id, name, default_value, type) \
  { id, FLEX_DIAG_CONF_VALUE_TAG_F32, type, 0, name, (void *)default_value }

/// Adds a signed integer to the diagnostic and configuration table
/// \param id The id of diagnostic or configuration of type FLEX_DiagConfID.
/// \param name The name of diagnostic or configuration.
/// \param default_value The default_value of the diagnostic or configuration.
/// \param type If the table item is a configuration, diagnostic, or presistent diagnostic. See
/// FLEX_DiagConfType.
#define FLEX_DIAG_CONF_TABLE_I32_ADD(id, name, default_value, type) \
  { id, FLEX_DIAG_CONF_VALUE_TAG_I32, type, 0, name, (void *)default_value }

/// Adds a string to the diagnostic and configuration table
/// \param id The id of diagnostic or configuration of type FLEX_DiagConfID.
/// \param name The name of diagnostic or configuration.
/// \param default_value The default_value of the diagnostic or configuration.
/// \param max_len If the maximum length of the string that can be stored.
/// \param type If the table item is a configuration, diagnostic, or presistent diagnostic. See
/// FLEX_DiagConfType.
#define FLEX_DIAG_CONF_TABLE_STR_ADD(id, name, default_value, max_len, type) \
  { id, FLEX_DIAG_CONF_VALUE_TAG_STR, type, max_len, name, (void *)default_value }

/// Adds a timestamp to the diagnostic and configuration table
/// \param id The id of diagnostic or configuration of type FLEX_DiagConfID.
/// \param name The name of diagnostic or configuration.
/// \param default_value The default_value of the diagnostic or configuration.
/// \param type If the table item is a configuration, diagnostic, or presistent diagnostic. See
/// FLEX_DiagConfType.
#define FLEX_DIAG_CONF_TABLE_TIME_ADD(id, name, default_value, type) \
  { id, FLEX_DIAG_CONF_VALUE_TAG_T32, type, 0, name, (void *)default_value }

/// Adds a unsigned integer to the diagnostic and configuration table
/// \param id The id of diagnostic or configuration of type FLEX_DiagConfID.
/// \param name The name of diagnostic or configuration.
/// \param default_value The default_value of the diagnostic or configuration.
/// \param type If the table item is a configuration, diagnostic, or presistent diagnostic. See
/// FLEX_DiagConfType.
#define FLEX_DIAG_CONF_TABLE_U32_ADD(id, name, default_value, type) \
  { id, FLEX_DIAG_CONF_VALUE_TAG_U32, type, 0, name, (void *)default_value }

/// Diagnostics and Configurations IDs
typedef enum {
  FLEX_DIAG_CONF_ID_USER_0,    ///< Diagnostics or Configuration ID 0
  FLEX_DIAG_CONF_ID_USER_1,    ///< Diagnostics or Configuration ID 1
  FLEX_DIAG_CONF_ID_USER_2,    ///< Diagnostics or Configuration ID 2
  FLEX_DIAG_CONF_ID_USER_3,    ///< Diagnostics or Configuration ID 3
  FLEX_DIAG_CONF_ID_USER_4,    ///< Diagnostics or Configuration ID 4
  FLEX_DIAG_CONF_ID_USER_5,    ///< Diagnostics or Configuration ID 5
  FLEX_DIAG_CONF_ID_USER_6,    ///< Diagnostics or Configuration ID 6
  FLEX_DIAG_CONF_ID_USER_7,    ///< Diagnostics or Configuration ID 7
  FLEX_DIAG_CONF_ID_USER_8,    ///< Diagnostics or Configuration ID 8
  FLEX_DIAG_CONF_ID_USER_9,    ///< Diagnostics or Configuration ID 9
  FLEX_DIAG_CONF_ID_USER_10,   ///< Diagnostics or Configuration ID 10
  FLEX_DIAG_CONF_ID_USER_11,   ///< Diagnostics or Configuration ID 11
  FLEX_DIAG_CONF_ID_USER_12,   ///< Diagnostics or Configuration ID 12
  FLEX_DIAG_CONF_ID_USER_13,   ///< Diagnostics or Configuration ID 13
  FLEX_DIAG_CONF_ID_USER_14,   ///< Diagnostics or Configuration ID 14
  FLEX_DIAG_CONF_ID_USER_15,   ///< Diagnostics or Configuration ID 15
  FLEX_DIAG_CONF_ID_USER_MAX,  ///< Diagnostics or Configuration MAX ID
} FLEX_DiagConfID;

/// The type of diagnostic or configuration item
typedef enum {
  FLEX_DIAG_CONF_TYPE_CONF,  ///< Configuration Type - Read/Write value while the application is
                             ///< running
  FLEX_DIAG_CONF_TYPE_DIAG,  ///< Diagnostic Type - Read only value that is cleared on reset.
  FLEX_DIAG_CONF_TYPE_PERSIST_DIAG,  ///< Persistent Diagnostic Type - Read only value that is not
                                     ///< cleared on reset.
} FLEX_DiagConfType;

/// Diagnostics or Configuration Notify Function Pointer Declaration.
/// \param value The new value of the diagnostic or configuration.
typedef void (*FLEX_DiagConfValueNotifyHandler)(const void *const value);

/// \cond INTERNAL_HIDDEN

typedef enum {
  FLEX_DIAG_CONF_VALUE_TAG_B8,
  FLEX_DIAG_CONF_VALUE_TAG_F32,
  FLEX_DIAG_CONF_VALUE_TAG_I32,
  FLEX_DIAG_CONF_VALUE_TAG_T32,
  FLEX_DIAG_CONF_VALUE_TAG_U32,
  FLEX_DIAG_CONF_VALUE_TAG_STR,
} FLEX_DiagConfValueTag;

typedef struct {
  const uint8_t id;
  const uint8_t tag;
  const uint8_t flags;
  const uint8_t max_len;
  const char *const name;
  void *value;
} FLEX_DiagConfTableItem;

/// \endcond

/// Writes the diagnostic or configuration value.
/// \param id The id of the diagnostic or configuration.
/// \param value The diagnostic or configuration value to write.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_ENOTRECOVERABLE: BLE module comms in a unrecoverable state.
/// \retval -FLEX_ERROR_EPROTO: BLE module comms protocol error most likely a version miss match.
/// \retval -FLEX_ERROR_ECOMM: Failed to communicate with the BLE module.
/// \retval -FLEX_ERROR_EINVAL: Supplied a id that is out of range.
/// \retval -FLEX_ERROR_ENXIO: Supplied a id with no corresponding table entry.
int FLEX_DiagConfValueWrite(const FLEX_DiagConfID id, const void *const value);

/// Reads the diagnostic or configuration value.
/// \param id The id of the diagnostic or configuration.
/// \param value The diagnostic or configuration value to read.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_ENOTRECOVERABLE: BLE module comms in a unrecoverable state.
/// \retval -FLEX_ERROR_EPROTO: BLE module comms protocol error most likely a version miss match.
/// \retval -FLEX_ERROR_ECOMM: Failed to communicate with the BLE module.
/// \retval -FLEX_ERROR_EINVAL: Supplied a id that is out of range.
/// \retval -FLEX_ERROR_ENXIO: Supplied a id with no corresponding table entry.
int FLEX_DiagConfValueRead(const FLEX_DiagConfID id, void *const value);

/// Adds an event handler for when the given diagnostic or configuration
/// value changes.
/// \note There can only be one handler subscribed to the event at a time,
/// so passing in a new handler will remove current one.
/// \param id The id of the diagnostic or configuration.
/// \param handler the event handler to be called on diagnostic or configuration change.
/// \return FLEX_SUCCESS (0) if succeeded and < 0 if failed.
/// \retval -FLEX_ERROR_EINVAL: Supplied a id that is out of range.
/// \retval -FLEX_ERROR_ENXIO: Supplied a id with no corresponding table entry.
int FLEX_DiagConfValueNotifyHandlerSet(const FLEX_DiagConfID id,
  const FLEX_DiagConfValueNotifyHandler handler);

///@}

#endif /* FLEX_DIAG_CONF_H */
