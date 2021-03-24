
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

#include "mgos.h"
#include "frozen/frozen.h"
#include "mgos_espnow.h"
#include "esp_now.h"
#include "esp_wifi.h"

bool mgos_espnow_parse_colon_mac(const char * data, uint8_t *dest){
    int scanned = sscanf(data, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx%*c", dest, dest+1, dest+2, dest+3, dest+4, dest+5);
    if(scanned >= 6) return true;
    else return false;
}

int mgos_espnow_total_peers(){
    int result = 0;
    struct mgos_espnow_peer *peer;
    SLIST_FOREACH(peer, &peer_list, next){
        result++;
    }
    return result;
}

static void mgos_espnow_add_broadcast_peer(){
    LOG(LL_ERROR, ("Adding broadcast peer to ESPNOW"));
    struct esp_now_peer_info newpeer = {
        .peer_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
        .channel = mgos_sys_config_get_wifi_ap_channel(),
        .ifidx = ESP_IF_WIFI_AP,
        .encrypt = false
    };
    esp_now_add_peer(&newpeer);
}

struct mgos_espnow_peer *mgos_espnow_get_peer_by_mac(const uint8_t *mac){
    struct mgos_espnow_peer *peer;
    SLIST_FOREACH(peer, &peer_list, next){
        if(memcmp(mac, peer->mac, 6) == 0){
            return peer;
        }
    }
    return NULL;
}

struct mgos_espnow_peer *mgos_espnow_get_peer_by_name(const char *name){
    struct mgos_espnow_peer *peer;
    SLIST_FOREACH(peer, &peer_list, next){
        if(strcmp(name, peer->name) == 0){
            return peer;
        }
    }
    return NULL;
}

void mgos_espnow_remove_send_peer_cb(espnow_send_peer_cb_t cb, const char *name){
    struct espnow_send_peer_cb *cb_entry;
    SLIST_FOREACH(cb_entry, &espnow_send_peer_cb_head, next){
        if(cb == cb_entry->cb && strcmp(name, cb_entry->peer->name) == 0){
            SLIST_REMOVE(&espnow_send_peer_cb_head, cb_entry, espnow_send_peer_cb, next);
            free(cb_entry);
            return;
        }
    }
}

void mgos_espnow_remove_send_mac_cb(espnow_send_mac_cb_t cb, uint8_t *mac, enum mac_cb_type type){
    struct espnow_send_mac_cb *cb_entry;
    SLIST_FOREACH(cb_entry, &espnow_send_mac_cb_head, next){
        if(cb == cb_entry->cb){
            if(type == MAC){
                if(cb_entry->type == MAC && memcmp(cb_entry->mac, mac, 6) == 0){
                    SLIST_REMOVE(&espnow_send_mac_cb_head, cb_entry, espnow_send_mac_cb, next);
                    free(cb_entry);
                    return;
                }
            } else {
                if(type == cb_entry->type){
                    SLIST_REMOVE(&espnow_send_mac_cb_head, cb_entry, espnow_send_mac_cb, next);
                    free(cb_entry);
                    return;
                }
            }
        }
    }
}

void mgos_espnow_remove_recv_mac_cb(espnow_recv_mac_cb_t cb, uint8_t *mac, enum mac_cb_type type){
    struct espnow_recv_mac_cb *cb_entry;
    SLIST_FOREACH(cb_entry, &espnow_recv_mac_cb_head, next){
        if(cb == cb_entry->cb){
            if(type == MAC){
                if(cb_entry->type == MAC && memcmp(cb_entry->mac, mac, 6) == 0){
                    SLIST_REMOVE(&espnow_recv_mac_cb_head, cb_entry, espnow_recv_mac_cb, next);
                    free(cb_entry);
                    return;
                }
            } else {
                if(type == cb_entry->type){
                    SLIST_REMOVE(&espnow_recv_mac_cb_head, cb_entry, espnow_recv_mac_cb, next);
                    free(cb_entry);
                    return;
                }
            }
        }
    }
}
void mgos_espnow_remove_recv_peer_cb(espnow_recv_peer_cb_t cb, const char *name){
    struct espnow_recv_peer_cb *cb_entry;
    SLIST_FOREACH(cb_entry, &espnow_recv_peer_cb_head, next){
        if(cb == cb_entry->cb && strcmp(name, cb_entry->peer->name) == 0){
            SLIST_REMOVE(&espnow_recv_peer_cb_head, cb_entry, espnow_recv_peer_cb, next);
            free(cb_entry);
            return;
        }
    }
}

static void espnow_global_rx_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len){
    if(mgos_sys_config_get_espnow_debug_level() != -1){
        LOG(mgos_sys_config_get_espnow_debug_level(), 
        ("RX - MAC %.2x:%.2x:%.2x:%.2x:%.2x:%.2x Data len %d - %.*s", 
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], 
        data_len, data_len, data));
    }
    struct espnow_recv_peer_cb *p_cb;
    struct espnow_recv_mac_cb *m_cb;
    struct mgos_espnow_peer *recv_peer = mgos_espnow_get_peer_by_mac(mac_addr);
    //Peer Callbacks
    if(recv_peer != NULL){
        SLIST_FOREACH(p_cb, &espnow_recv_peer_cb_head, next){
            if(p_cb->peer == recv_peer){
                p_cb->cb(recv_peer, data, data_len, p_cb->ud);
            }
        }
    }
    //Mac Callbacks
    SLIST_FOREACH(m_cb, &espnow_recv_mac_cb_head, next){
        switch(m_cb->type){
            case ALL:
            m_cb->cb(mac_addr, data, data_len, m_cb->ud);
            break;
            case ANY_PEER:
            if(recv_peer != NULL){
                m_cb->cb(mac_addr, data, data_len, m_cb->ud);
            }
            break;
            case BCAST:
            if(recv_peer == NULL){
                m_cb->cb(mac_addr, data, data_len, m_cb->ud);
            }
            break;
            case MAC:
            if(memcmp(m_cb->mac, mac_addr, 6) == 0){
                m_cb->cb(mac_addr, data, data_len, m_cb->ud);
            }
            break;
        }
    }
}

void espnow_global_tx_cb(const uint8_t *mac_addr, esp_now_send_status_t status){
    uint8_t bcast_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    if(mgos_sys_config_get_espnow_debug_level() != -1){
        LOG(mgos_sys_config_get_espnow_debug_level(), 
        ("TX - MAC %.2x:%.2x:%.2x:%.2x:%.2x:%.2x Result: %s", 
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], 
        status == ESP_NOW_SEND_SUCCESS ? "Success": "Failure"));
    }
    struct espnow_send_peer_cb *p_cb;
    struct espnow_send_mac_cb *m_cb;
    struct mgos_espnow_peer *send_peer = mgos_espnow_get_peer_by_mac(mac_addr);
    //Peer Callbacks
    if(send_peer != NULL){
        SLIST_FOREACH(p_cb, &espnow_send_peer_cb_head, next){
            p_cb->cb(send_peer, status == ESP_NOW_SEND_SUCCESS, p_cb->ud);
        }
    }
    //Mac Callbacks
    SLIST_FOREACH(m_cb, &espnow_send_mac_cb_head, next){
        switch(m_cb->type){
            case ALL:
            m_cb->cb(mac_addr, status == ESP_NOW_SEND_SUCCESS, m_cb->ud);
            break;
            case BCAST:
            if(memcmp(bcast_addr, mac_addr, 6) == 0){
                m_cb->cb(mac_addr, status == ESP_NOW_SEND_SUCCESS, m_cb->ud);
            }
            break;
            case ANY_PEER:
            if(send_peer != NULL){
                m_cb->cb(mac_addr, status == ESP_NOW_SEND_SUCCESS, m_cb->ud);
            }
            break;
            case MAC:
            if(memcmp(m_cb->mac, mac_addr, 6) == 0){
                m_cb->cb(mac_addr, status == ESP_NOW_SEND_SUCCESS, m_cb->ud);
            }
            break;
        }
    }
}

mgos_espnow_result_t mgos_espnow_register_recv_mac_cb(const uint8_t *mac, enum mac_cb_type type, espnow_recv_mac_cb_t cb, void *ud){
    struct espnow_recv_mac_cb *cb_entry = (struct espnow_recv_mac_cb *)calloc(1, sizeof(*cb_entry));
    if(cb_entry == NULL){
        LOG(LL_ERROR, ("Failed to allocate rx cb struct"));
        return ESPNOW_NO_MEM;
    }
    cb_entry->type = type;
    if(type == MAC){
        memcpy(cb_entry->mac, mac, 6);
    }
    cb_entry->cb = cb;
    cb_entry->ud = ud;
    
    SLIST_INSERT_HEAD(&espnow_recv_mac_cb_head, cb_entry, next);
    return ESPNOW_OK;
}

mgos_espnow_result_t mgos_espnow_register_send_mac_cb(const uint8_t *mac, enum mac_cb_type type, espnow_send_mac_cb_t cb, void *ud){
    struct espnow_send_mac_cb *cb_entry = (struct espnow_send_mac_cb *)calloc(1, sizeof(*cb_entry));
    if(cb_entry == NULL){
        LOG(LL_ERROR, ("Failed to allocate tx cb struct"));
        return ESPNOW_NO_MEM;
    }
    cb_entry->type = type;
    if(type == MAC){
        memcpy(cb_entry->mac, mac, 6);
    }
    cb_entry->cb = cb;
    cb_entry->ud = ud;
    
    SLIST_INSERT_HEAD(&espnow_send_mac_cb_head, cb_entry, next);
    return ESPNOW_OK;
}

mgos_espnow_result_t mgos_espnow_register_recv_peer_cb(const char *name, espnow_recv_peer_cb_t cb, void *ud){
    struct espnow_recv_peer_cb *cb_entry = (struct espnow_recv_peer_cb *)calloc(1, sizeof(*cb_entry));
    if(cb_entry == NULL){
        LOG(LL_ERROR, ("Failed to allocate rx cb struct"));
        return ESPNOW_NO_MEM;
    }
    struct mgos_espnow_peer *peer = mgos_espnow_get_peer_by_name(name);
    if(peer == NULL){
        return ESPNOW_PEER_NOT_FOUND;
    }
    cb_entry->peer = peer;
    cb_entry->cb = cb;
    cb_entry->ud = ud;
    
    SLIST_INSERT_HEAD(&espnow_recv_peer_cb_head, cb_entry, next);
    return ESPNOW_OK;
}

mgos_espnow_result_t mgos_espnow_register_send_peer_cb(const char *name, espnow_send_peer_cb_t cb, void *ud){
    struct espnow_send_peer_cb *cb_entry = (struct espnow_send_peer_cb *)calloc(1, sizeof(*cb_entry));
    if(cb_entry == NULL){
        LOG(LL_ERROR, ("Failed to allocate tx cb struct"));
        return ESPNOW_NO_MEM;
    }
    struct mgos_espnow_peer *peer = mgos_espnow_get_peer_by_name(name);
    if(peer == NULL){
        return ESPNOW_PEER_NOT_FOUND;
    }
    cb_entry->peer = peer;
    cb_entry->cb = cb;
    cb_entry->ud = ud;
    
    SLIST_INSERT_HEAD(&espnow_send_peer_cb_head, cb_entry, next);
    return ESPNOW_OK;
}

mgos_espnow_result_t mgos_espnow_send(const char *name, const uint8_t *data, int len){
    struct mgos_espnow_peer *peer;
    char *error[50];
    SLIST_FOREACH(peer, &peer_list, next){
        if(strcmp(name, peer->name) == 0){
            esp_err_t error = esp_now_send(peer->mac, data, len);
            if(error != ESP_OK){
                LOG(LL_ERROR, ("ESPNOW ERROR %d: %s", error, esp_err_to_name(error)));
            }
            return ESPNOW_OK;
        }
    }
    return ESPNOW_PEER_NOT_FOUND;
}

mgos_espnow_result_t mgos_espnow_broadcast(const uint8_t *data, int len){
    uint8_t bcast_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    esp_now_send(bcast_addr, data, len);
    return ESPNOW_OK;
}

static void mgos_espnow_dump_peers_json(){
    LOG(LL_ERROR, ("Dumping current peer list to file"));
    FILE *out_json = fopen(mgos_sys_config_get_espnow_peers_filename(), "w");
    if(out_json == NULL){
        LOG(LL_ERROR, ("Failed to open file %s for writing!", mgos_sys_config_get_espnow_peers_filename()));
        return;
    }
    struct json_out file_output = JSON_OUT_FILE(out_json);
    int total_peers = mgos_espnow_total_peers();
    json_printf(&file_output, "[");
    int idx = 0;
    struct mgos_espnow_peer *peer;
	SLIST_FOREACH(peer, &peer_list, next){
        json_printf(&file_output, "{name: %Q, mac: \"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\", softap: %B, channel: %d}", peer->name, peer->mac[0], peer->mac[1], peer->mac[2], peer->mac[3], peer->mac[4], peer->mac[5],
        peer->softap, peer->channel);
        if(idx < total_peers-1){
            json_printf(&file_output, ",");
        }
        idx++;
    }
    json_printf(&file_output, "]");
    fclose(out_json);
}

static esp_err_t mgos_espnow_internal_add_peer(struct mgos_espnow_peer *peer){
    struct esp_now_peer_info newpeer;
    bool modify = false;
    if(esp_now_get_peer(peer->mac, &newpeer) == ESP_OK){
        modify = true;
    }
    memcpy(newpeer.peer_addr, peer->mac, 6);
    newpeer.channel = peer->channel;
    if(peer->softap) newpeer.ifidx = ESP_IF_WIFI_AP;
    else newpeer.ifidx = ESP_IF_WIFI_STA;
    newpeer.encrypt = false;
    if(!modify) return esp_now_add_peer(&newpeer);
    else return esp_now_mod_peer(&newpeer);
}

static void mgos_espnow_internal_remove_peer(struct mgos_espnow_peer *peer){
    esp_now_del_peer(peer->mac);
}

mgos_espnow_result_t mgos_espnow_add_peer(const char *name, const uint8_t *mac, bool softap, int channel, bool save){
    struct mgos_espnow_peer *peer, *mnewpeer;
    SLIST_FOREACH(peer, &peer_list, next){
        if(strcmp(name, peer->name) == 0 || memcmp(peer->mac, mac, 6) == 0){
            LOG(LL_ERROR, ("Adding new peer. Removing %s, MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x", peer->name, peer->mac[0], peer->mac[1], peer->mac[2], peer->mac[3], peer->mac[4], peer->mac[5]));
            SLIST_REMOVE(&peer_list, peer, mgos_espnow_peer, next);
            free(peer->name);
            free(peer);
        }
    }
    mnewpeer = (struct mgos_espnow_peer *)calloc(1, sizeof(*mnewpeer));
    mnewpeer->name = strdup(name);
    memcpy(mnewpeer->mac, mac, 6);
    mnewpeer->softap = softap;
    if(channel == -1) mnewpeer->channel = mgos_sys_config_get_wifi_ap_channel();
    else mnewpeer->channel = channel;
    esp_err_t result = mgos_espnow_internal_add_peer(mnewpeer);
    if(result != ESP_OK){
        free(mnewpeer->name);
        free(mnewpeer);
        if(result == ESP_ERR_ESPNOW_FULL){
            return ESPNOW_MAX_PEERS;
        } else if(result == ESP_ERR_ESPNOW_NOT_INIT) {
            return ESPNOW_NOT_INIT;
        } else {
            return ESPNOW_NO_MEM;
        }
    } else {
        SLIST_INSERT_HEAD(&peer_list, mnewpeer, next);
        if(save) mgos_espnow_dump_peers_json();
        return ESPNOW_OK;
    }
}

void mgos_espnow_remove_peer(const char *name, bool save){
    struct mgos_espnow_peer *peer = mgos_espnow_get_peer_by_name(name);
    if(peer == NULL) return;
    SLIST_REMOVE(&peer_list, peer, mgos_espnow_peer, next);
    mgos_espnow_internal_remove_peer(peer);
    free(peer->name);
    free(peer);
}

static void mgos_espnow_load_peers_file(){
    char *peers_json;
    int peers_json_len, channel = -1;
    struct json_token item;
    char *name = NULL, *mac = NULL;
    char scanned_softap = 0;
    peers_json = json_fread(mgos_sys_config_get_espnow_peers_filename());
    if(peers_json == NULL) return;
    peers_json_len = strlen(peers_json);
    for(int i = 0; json_scanf_array_elem(peers_json, peers_json_len, "", i, &item) > 0; i++) {
        name = NULL;
        mac = NULL;
        scanned_softap = 1;
        if(json_scanf(item.ptr, item.len, "{ name: %Q, mac: %Q }", &name, &mac) == 2){
            struct mgos_espnow_peer *peer = (struct mgos_espnow_peer*) calloc(1, sizeof(*peer));
            if(mgos_espnow_parse_colon_mac(mac, peer->mac)){
                LOG(LL_ERROR, ("New Peer Name: %s  MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x", name, peer->mac[0], peer->mac[1], peer->mac[2], peer->mac[3], peer->mac[4], peer->mac[5]));
                peer->name = name;
                json_scanf(item.ptr, item.len, "{ softap: %B }", &scanned_softap);
                peer->softap = (bool)scanned_softap;
                channel = -1;
                json_scanf(item.ptr, item.len, "{ channel: %d }", &channel);
                if(channel == -1) peer->channel = mgos_sys_config_get_wifi_ap_channel();
                else peer->channel = channel;
                SLIST_INSERT_HEAD(&peer_list, peer, next);
                mgos_espnow_internal_add_peer(peer);
            } else {
                LOG(LL_ERROR, ("PEER#%d: %s HAS INVALID MAC %s", i, name, mac));
                free(peer);
            }
            free(mac);
        }
    }
}

bool mgos_espnow_init(){
    if(!mgos_sys_config_get_espnow_enable()) return true;
    if(!mgos_sys_config_get_wifi_sta_enable() && !mgos_sys_config_get_wifi_ap_enable()){
        LOG(LL_ERROR, ("No wifi interfaces enabled! ESPNOW will not work."));
        return true;
    }
    SLIST_INIT(&peer_list);  
    SLIST_INIT(&espnow_recv_peer_cb_head);
    SLIST_INIT(&espnow_recv_mac_cb_head);
    SLIST_INIT(&espnow_send_peer_cb_head);
    SLIST_INIT(&espnow_send_mac_cb_head);
    esp_now_init();
    if(mgos_sys_config_get_espnow_enable_broadcast()){
        mgos_espnow_add_broadcast_peer();
    }
    mgos_espnow_load_peers_file();
    esp_now_register_recv_cb(espnow_global_rx_cb);
    esp_now_register_send_cb(espnow_global_tx_cb);
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    LOG(LL_ERROR, ("AP MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    LOG(LL_ERROR, ("STA MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
    return true;
}
