/*
MIT License

Copyright (c) 2020 Juan Molero

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef CS_ESPNOW_H
#define CS_ESPNOW_H

#include "mgos.h"
#include "esp_now.h"

#include <queue.h>

#define MGOS_ESPNOW_MAX_LEN ESP_NOW_MAX_DATA_LEN 
typedef enum {
    ESPNOW_OK,
    ESPNOW_SEND_FAILED,
    ESPNOW_NOT_INIT, //Lib not init
    ESPNOW_MAX_PEERS, //ESPNOW cannot register any more peers
    ESPNOW_PEER_NOT_FOUND, //Peer not found
    ESPNOW_PAYLOAD_LEN_ERR, //Payload length exceeds maximum. Macro is ESP_NOW_MAX_DATA_LEN 
    ESPNOW_NO_MEM
} mgos_espnow_result_t;

enum mac_cb_type {
    MAC,
    BCAST,
    ANY_PEER,
    ALL
};

struct mgos_espnow_peer {
    uint8_t mac[6];
    bool softap;
    int channel;
    char *name;
    
    SLIST_ENTRY(mgos_espnow_peer) next;
};

//RX Callbacks
typedef void(*espnow_recv_peer_cb_t)(struct mgos_espnow_peer *peer, const uint8_t *data, int len, void *ud);
typedef void(*espnow_recv_mac_cb_t)(const uint8_t *mac, const uint8_t *data, int len, void *ud);

//TX Callbacks.
//Status tells if the message was sent properly. Messages sent to a registered peer will only succeed if the peer is ready to receive messages
typedef void(*espnow_send_peer_cb_t)(struct mgos_espnow_peer *peer, bool success, void *ud);
typedef void(*espnow_send_mac_cb_t)(const uint8_t *mac, bool success, void *ud);


struct espnow_recv_peer_cb {
    struct mgos_espnow_peer *peer;
    
    espnow_recv_peer_cb_t cb;
    void *ud;
    
    SLIST_ENTRY(espnow_recv_peer_cb) next;
};

struct espnow_recv_mac_cb {
    uint8_t mac[6];
    enum mac_cb_type type;
    
    espnow_recv_mac_cb_t cb;
    void *ud;
    
    SLIST_ENTRY(espnow_recv_mac_cb) next;
};

struct espnow_send_peer_cb {
    struct mgos_espnow_peer *peer;
    
    espnow_send_peer_cb_t cb;
    void *ud;
    
    bool any;
    
    SLIST_ENTRY(espnow_send_peer_cb) next;
};

struct espnow_send_mac_cb {
    uint8_t mac[6];
    enum mac_cb_type type;
    
    espnow_send_mac_cb_t cb;
    void *ud;
    
    SLIST_ENTRY(espnow_send_mac_cb) next;
};

SLIST_HEAD(peer_list, mgos_espnow_peer) peer_list;
SLIST_HEAD(espnow_recv_peer_cb_head, espnow_recv_peer_cb) espnow_recv_peer_cb_head;
SLIST_HEAD(espnow_recv_mac_cb_head, espnow_recv_mac_cb) espnow_recv_mac_cb_head;
SLIST_HEAD(espnow_send_peer_cb_head, espnow_send_peer_cb) espnow_send_peer_cb_head;
SLIST_HEAD(espnow_send_mac_cb_head, espnow_send_mac_cb) espnow_send_mac_cb_head;

#ifdef __cplusplus
extern "C"{
#endif

    //Lib Init
    bool mgos_espnow_init();
    //Send to a peer loaded. NULL to send to all loaded peers.
    mgos_espnow_result_t mgos_espnow_send(const char *name, const uint8_t *data, int len);
    //Send a broadcast message
    mgos_espnow_result_t mgos_espnow_broadcast(const uint8_t *data, int len);
    
    
    //Register for messages received by peers
    mgos_espnow_result_t mgos_espnow_register_recv_peer_cb(const char *name, espnow_recv_peer_cb_t cb, void *ud);
    
    
    //Register for messages received from a mac address according to type:
    //  MAC - For received messages from a specific MAC. Will match peers and broadcast messages if enabled
    //  BCAST - For messages received from UNREGISTERED peers. Will only work if broadcast is enabled of course.
    //  ANY_PEER - Messages from any peer registered
    //  ANY - Any message received
    mgos_espnow_result_t mgos_espnow_register_recv_mac_cb(const uint8_t *mac, enum mac_cb_type type, espnow_recv_mac_cb_t cb, void *ud);
    
    
    //Remove receive callback.
    void mgos_espnow_remove_recv_mac_cb(espnow_recv_mac_cb_t cb, uint8_t *mac, enum mac_cb_type type);
    void mgos_espnow_remove_recv_peer_cb(espnow_recv_peer_cb_t cb, const char *name);


    //Register a callback to be called after a message is sent to a peer
    mgos_espnow_result_t mgos_espnow_register_send_peer_cb(const char *name, espnow_send_peer_cb_t cb, void *ud);
    //Register for messages sent to a mac address
    mgos_espnow_result_t mgos_espnow_register_send_mac_cb(const uint8_t *mac, enum mac_cb_type type, espnow_send_mac_cb_t cb, void *ud);
    //Remove send callback from list.
    void mgos_espnow_remove_send_mac_cb(espnow_send_mac_cb_t cb, uint8_t *mac, enum mac_cb_type type);
    void mgos_espnow_remove_send_peer_cb(espnow_send_peer_cb_t cb, const char *name);


    //Total registered peers loaded from file
    int mgos_espnow_total_peers();


    //Add peer to memory. Optionally save it to json.
    mgos_espnow_result_t mgos_espnow_add_peer(const char *name, const uint8_t *mac, bool softap, int channel, bool save);
    void mgos_espnow_remove_peer(const char *name, bool save);
    struct mgos_espnow_peer *mgos_espnow_get_peer_by_name(const char *peer);
    struct mgos_espnow_peer *mgos_espnow_get_peer_by_mac(const uint8_t *mac);
    //Parse colon separated mac address to target pointer. Return true on success.
    bool mgos_espnow_parse_colon_mac(const char *mac, uint8_t *tmac);

#ifdef __cplusplus
}
#endif

#endif
