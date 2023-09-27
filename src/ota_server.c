#include "pico_wifi_boot/ota_server.h"

#include <stdio.h>
#include <string.h>

#include "cyw43_config.h"
#include "hardware/flash.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "pico_wifi_boot/flash.h"
#include "pico_wifi_boot/reboot.h"
#include "pico_wifi_boot/sniffer_crc32.h"

#define OTA_MAGIC_CODE "OTA\n"
#define OTA_MAGIC_CODE_LEN 4

enum OtaErrorCode {
    SUCCESS = 0,
    STORAGE_FULL = 1,
    CHECKSUM_FAILED = 2,
    REBOOTING = 3,
};

struct __attribute__((__packed__)) OtaRequest {
    uint8_t magic_code[OTA_MAGIC_CODE_LEN]; // "OTA\n"
    uint32_t payload_size;
    uint32_t checksum;
};

struct __attribute__((__packed__)) OtaResponse {
    uint8_t magic_code[OTA_MAGIC_CODE_LEN]; // "OTA\n"
    uint8_t error_code;
};

struct OtaConnectionState {
    uint32_t partial_bytes;
    struct OtaRequest request;
    bool request_filled;
    struct OtaResponse response;
    uint8_t data[FLASH_SECTOR_SIZE];
    uint32_t bytes_written;
    bool ready_to_reboot;
};

void reboot_after_disconnect() {
    printf("OTA server: disconnect triggered reboot\n");

    // Give peripherals some time to process output
    sleep_ms(100);

    if (running_in_bootloader()) {
        reboot();
    } else {
        reboot_into_bootloader();
    }
}

bool ota_process_request(struct tcp_pcb* pcb, struct OtaConnectionState* state, struct pbuf* pb) {
    cyw43_arch_lwip_check();

    if (state->partial_bytes + pb->tot_len > sizeof(state->request)) {
        printf("OTA server: too many bytes for request structure\n");
        return false;
    }

    if (pbuf_copy_partial(pb, ((uint8_t*)&state->request) + state->partial_bytes, pb->tot_len, 0) != pb->tot_len) {
        printf("OTA server: pbuf copy failed\n");
        return false;
    }
    state->partial_bytes += pb->tot_len;

    if (state->partial_bytes == sizeof(state->request)) {
        state->request_filled = true;
        state->partial_bytes = 0;

        if (memcmp(state->request.magic_code, OTA_MAGIC_CODE, OTA_MAGIC_CODE_LEN) != 0) {
            printf("OTA server: received bad header\n");
            return false;
        }

        bool is_flashable = state->request.payload_size <= USER_PROGRAM_MAX_SIZE;
        bool in_bootloader = running_in_bootloader();

        memcpy(state->response.magic_code, OTA_MAGIC_CODE, OTA_MAGIC_CODE_LEN);
        state->response.error_code = is_flashable ? (in_bootloader ? SUCCESS : REBOOTING) : STORAGE_FULL;

        if (tcp_write(pcb, &state->response, sizeof(state->response), TCP_WRITE_FLAG_COPY) != ERR_OK) {
            printf("OTA server: TCP send failed\n");
            return false;
        }

        printf(
            "OTA server: client requested %"PRIu32" bytes (%s)\n",
            state->request.payload_size,
            is_flashable ? "okay" : "insufficient storage");

        if (state->response.error_code == REBOOTING) {
            state->ready_to_reboot = true;
            printf("OTA server: waiting to reboot into bootloader\n");
        }
    }

    return true;
}

bool ota_process_payload(struct OtaConnectionState* state, struct pbuf* pb) {
    cyw43_arch_lwip_check();

    uint32_t processed = 0;
    do {
        uint32_t available = MIN(pb->tot_len - processed, FLASH_SECTOR_SIZE - state->partial_bytes);

        if (pbuf_copy_partial(pb, state->data + state->partial_bytes, available, processed) != available) {
            printf("OTA server: pbuf copy failed\n");
            return false;
        }
        processed += available;
        state->partial_bytes += available;

        if (state->bytes_written + state->partial_bytes > state->request.payload_size) {
            printf("OTA server: too many bytes received for payload\n");
            return false;
        }

        if (state->partial_bytes == FLASH_SECTOR_SIZE
            || state->bytes_written + state->partial_bytes == state->request.payload_size) {
            // Always write a full sector for simplicity, since we erase one anyway
            write_flash_sector(USER_PROGRAM_OFFSET + state->bytes_written, state->data);

            state->bytes_written += state->partial_bytes;
            state->partial_bytes = 0;
        }
    } while (processed < pb->tot_len);

    return true;
}

bool ota_process_staged(struct tcp_pcb* pcb, struct OtaConnectionState* state) {
    cyw43_arch_lwip_check();

    if (state->request.payload_size != state->bytes_written) {
        return false;
    }

    printf("OTA server: payload received, verifying..\n");

    bool checksum_ok =
        sniffer_crc32((uint8_t*)XIP_BASE + USER_PROGRAM_OFFSET, state->request.payload_size) == state->request.checksum;

    struct OtaResponse response;
    memcpy(response.magic_code, OTA_MAGIC_CODE, OTA_MAGIC_CODE_LEN);
    response.error_code = checksum_ok ? SUCCESS : CHECKSUM_FAILED;

    if (tcp_write(pcb, &response, sizeof(response), TCP_WRITE_FLAG_COPY) != ERR_OK) {
        printf("OTA server: failed to write response\n");
        return false;
    }

    if (checksum_ok) {
        state->ready_to_reboot = true;
        printf("OTA server: flashing succeeded, waiting to reboot\n");
    } else {
        state->bytes_written = 0;
        printf("OTA server: checksum failed! Client may retry\n");
    }

    return true;
}

bool ota_process(struct tcp_pcb* pcb, struct OtaConnectionState* state, struct pbuf* pb) {
    if (!state->request_filled) {
        return ota_process_request(pcb, state, pb);
    }

    // If a non-success response was sent, the client should have disconnected
    if (state->response.error_code != SUCCESS) {
        return false;
    }

    if (!ota_process_payload(state, pb)) {
        return false;
    }

    if (state->bytes_written == state->request.payload_size) {
        if (!ota_process_staged(pcb, state)) {
            return false;
        }
    }

    return true;
}

err_t on_ota_recv(void* arg, struct tcp_pcb* pcb, struct pbuf* pb, err_t err) {
    struct OtaConnectionState* state = arg;

    cyw43_arch_lwip_check();

    if (!arg) {
        printf("OTA server: missing arg in callback\n");
        if (pb) {
            pbuf_free(pb);
        }
        tcp_abort(pcb);
        return ERR_ABRT;
    }

    bool keep_connection = true;

    if (pb) {
        if (pb->tot_len) {
            keep_connection = ota_process(pcb, state, pb);
            tcp_recved(pcb, pb->tot_len);
        }

        pbuf_free(pb);
    } else {
        printf("OTA server: connection closed by client\n");

        if (state->ready_to_reboot) {
            reboot_after_disconnect();
        }

        keep_connection = false;
    }

    if (!keep_connection) {
        tcp_arg(pcb, NULL);
        free(state);

        tcp_abort(pcb);
        return ERR_ABRT;
    }

    return ERR_OK;
}

void on_ota_error(void* arg, err_t err) {
    struct OtaConnectionState* state = arg;

    printf("OTA server: connection closed with error %d\n", (int)err);

    if (state) {
        if (state->ready_to_reboot) {
            reboot_after_disconnect();
        } else {
            free(state);
        }
    }
}

err_t on_ota_connect(void* arg, struct tcp_pcb* new_pcb, err_t err) {
    cyw43_arch_lwip_check();

    // TODO: don't allow concurrent connections

    if (new_pcb == NULL) {
        printf("OTA server: error accepting connection\n");
        return ERR_ARG;
    }

    if (err == ERR_OK) {
        printf("OTA server: client connected\n");
    } else {
        printf("OTA server: connect error %d, proceeding anyway\n", (int)err);
    }

    // TODO: clean up inactive connections on tcp_poll
    tcp_arg(new_pcb, calloc(1, sizeof(struct OtaConnectionState)));
    tcp_err(new_pcb, on_ota_error);
    tcp_recv(new_pcb, on_ota_recv);

    return ERR_OK;
}

struct tcp_pcb* init_listen_pcb(uint16_t port) {
    cyw43_arch_lwip_check();

    struct tcp_pcb* temp_pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!temp_pcb) {
        return NULL;
    }

    if (tcp_bind(temp_pcb, IP_ADDR_ANY, port) != ERR_OK) {
        tcp_abort(temp_pcb);
        return NULL;
    }

    struct tcp_pcb* listen_pcb = tcp_listen(temp_pcb);
    if (!listen_pcb) {
        tcp_abort(temp_pcb);
    }

    // temp_pcb has already been freed
    return listen_pcb;
}

struct tcp_pcb* ota_init(uint16_t port) {
    cyw43_thread_enter();

    struct tcp_pcb* listen_pcb = init_listen_pcb(port);
    if (listen_pcb) {
        tcp_accept(listen_pcb, on_ota_connect);

        printf("OTA server listening on port %d\n", port);
    } else {
        printf("OTA server failed to listen on port %d\n", port);
    }

    cyw43_thread_exit();

    return listen_pcb;
}
