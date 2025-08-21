// file: ble_min.c
#include <gattlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // write
#define NUS_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // notify

#define BLE_SCAN_TIMEOUT 10

// Declaration of thread condition variable
static pthread_cond_t m_connection_terminated = PTHREAD_COND_INITIALIZER;

// declaring mutex
static pthread_mutex_t m_connection_terminated_lock = PTHREAD_MUTEX_INITIALIZER;

static gattlib_connection_t *m_connection;
static char target_addr[18] = {0};
static int device_found = 0;

static void notification_handler(const uuid_t *uuid, const uint8_t *data,
                                 size_t len, void *user_data) {
  (void)uuid;
  (void)user_data;
  fprintf(stdout, "[notify] %zu bytes: ", len);
  for (size_t i = 0; i < len; i++)
    fprintf(stdout, "%c", (data[i] >= 32 && data[i] < 127) ? data[i] : '.');
  fprintf(stdout, "  (hex: ");
  for (size_t i = 0; i < len; i++)
    fprintf(stdout, "%02X ", data[i]);
  fprintf(stdout, ")\n");
}

static void on_device_connect(gattlib_adapter_t *, const char *dst,
                              gattlib_connection_t *connection, int error,
                              void *) {
  int ret;
  uuid_t tx_uuid, rx_uuid;

  fprintf(stdout, "[conn] on_device_connect called with dst=%s, error=%d\n",
          dst, error);
  if (error) {
    fprintf(stderr, "[conn] connect failed to %s (error code: %d)\n", dst,
            error);
    goto EXIT;
  }

  fprintf(stdout, "[conn] connected: %s\n", dst);
  m_connection = connection;

  // Convert characteristics to their respective UUIDs
  ret = gattlib_string_to_uuid(NUS_TX_UUID, strlen(NUS_TX_UUID) + 1, &tx_uuid);
  if (ret) {
    fprintf(stderr, "Fail to convert TX characteristic to UUID.\n");
    goto EXIT;
  }

  ret = gattlib_string_to_uuid(NUS_RX_UUID, strlen(NUS_RX_UUID) + 1, &rx_uuid);
  if (ret) {
    fprintf(stderr, "Fail to convert RX characteristic to UUID.\n");
    goto EXIT;
  }

  // Register notification handler
  ret = gattlib_register_notification(connection, notification_handler, NULL);
  if (ret) {
    fprintf(stderr, "register_notification failed: %d\n", ret);
    goto EXIT;
  }

  // Start notification
  ret = gattlib_notification_start(connection, &tx_uuid);
  if (ret) {
    fprintf(stderr, "notification_start failed: %d\n", ret);
    goto EXIT;
  }

  fprintf(stdout, "[gatt] subscribed TX notifications.\n");

  // Write to RX
  const uint8_t payload[] = "hello";
  ret = gattlib_write_char_by_uuid(connection, &rx_uuid, payload,
                                   sizeof(payload) - 1);
  if (ret) {
    fprintf(stderr, "write RX failed: %d\n", ret);
    goto EXIT;
  }
  fprintf(stdout, "[gatt] wrote %zu bytes to RX.\n", sizeof(payload) - 1);

  // Wait for notifications (15 seconds)
  fprintf(stdout,
          "[wait] send something from phone app to see notifications...\n");
  sleep(15);

  // Stop notification
  gattlib_notification_stop(connection, &tx_uuid);

EXIT:
  gattlib_disconnect(connection, false /* wait_disconnection */);
  pthread_mutex_lock(&m_connection_terminated_lock);
  pthread_cond_signal(&m_connection_terminated);
  pthread_mutex_unlock(&m_connection_terminated_lock);
}

static void ble_discovered_device(gattlib_adapter_t *adapter, const char *addr,
                                  const char *name, void *) {
  gattlib_advertisement_data_t *advertisement_data = NULL;
  gattlib_manufacturer_data_t *manufacturer_data = NULL;
  size_t advertisement_data_count = 0;
  size_t manufacturer_data_count = 0;
  int ret;

  fprintf(stdout, "[scan] ble_discovered_device called with addr=%s, name=%s\n",
          addr, name ? name : "NULL");
  // If we already found a device, skip
  if (device_found) {
    fprintf(stdout, "[scan] device already found, skipping\n");
    return;
  }

  // Get advertisement data
  ret = gattlib_get_advertisement_data_from_mac(
      adapter, addr, &advertisement_data, &advertisement_data_count,
      &manufacturer_data, &manufacturer_data_count);
  if (ret != 0) {
    fprintf(stderr, "[scan] failed to get advertisement data for %s (ret=%d)\n",
            addr, ret);
    return;
  }
  fprintf(stdout, "[scan] got advertisement data for %s, count=%zu\n", addr,
          advertisement_data_count);

  // Check if the device advertises the NUS service
  for (size_t i = 0; i < advertisement_data_count; i++) {
    char uuid_str[37]; // 36 characters + null terminator

    // Convert the UUID to string
    gattlib_uuid_to_string(&advertisement_data[i].uuid, uuid_str,
                           sizeof(uuid_str));
    printf("[scan] found device %s with UUID %s\n", addr, uuid_str);
    if (strcasecmp(uuid_str, NUS_SERVICE_UUID) == 0) {
      strncpy(target_addr, addr, sizeof(target_addr));
      device_found = 1;
      fprintf(stdout, "[scan] found NUS device: %s\n", target_addr);

      // Connect to the device
      fprintf(stdout, "[scan] attempting to connect to %s\n", addr);
      ret = gattlib_connect(adapter, addr, GATTLIB_CONNECTION_OPTIONS_NONE,
                            on_device_connect, NULL);
      if (ret != GATTLIB_SUCCESS) {
        fprintf(stderr, "[conn] connect failed to %s (ret=%d)\n", addr, ret);
      }
      break; // 找到后退出循环
    }
  }

  // Free the advertisement data
  if (advertisement_data != NULL) {
    free(advertisement_data);
  }
}

static void *ble_task(void *) {
  gattlib_adapter_t *adapter;
  int ret;

  fprintf(stdout, "[task] ble_task started\n");
  ret = gattlib_adapter_open(NULL, &adapter);
  if (ret) {
    fprintf(stderr, "adapter_open failed: %d\n", ret);
    return NULL;
  }
  fprintf(stdout, "[task] adapter opened successfully\n");

  fprintf(stdout, "[scan] start (looking for NUS)...\n");
  ret = gattlib_adapter_scan_enable(adapter, ble_discovered_device,
                                    BLE_SCAN_TIMEOUT, NULL);
  if (ret) {
    fprintf(stderr, "scan_enable failed: %d\n", ret);
    gattlib_adapter_close(adapter);
    return NULL;
  }
  fprintf(stdout, "[scan] scan enabled successfully\n");

  // Wait for the device to be connected
  pthread_mutex_lock(&m_connection_terminated_lock);
  pthread_cond_wait(&m_connection_terminated, &m_connection_terminated_lock);
  pthread_mutex_unlock(&m_connection_terminated_lock);

  gattlib_adapter_scan_disable(adapter);
  gattlib_adapter_close(adapter);

  return NULL;
}

int main() {
  int ret;

  ret = gattlib_mainloop(ble_task, NULL);
  if (ret != GATTLIB_SUCCESS) {
    fprintf(stderr, "Failed to create gattlib mainloop\n");
    return 1;
  }

  return 0;
}