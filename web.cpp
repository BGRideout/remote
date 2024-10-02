#include "web.h"
#include "ws.h"
#include "txt.h"

#include "stdio.h"

#include "pico/cyw43_arch.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "lwip/prot/iana.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include "mbedtls/debug.h"
#include "hardware/gpio.h"

WEB *WEB::singleton_ = nullptr;
int WEB::debug_level_ = 0;

WEB::WEB() : server_(nullptr), wifi_state_(CYW43_LINK_DOWN),
             ap_active_(0), ap_requested_(0), mdns_active_(false),
             http_callback_(nullptr), message_callback_(nullptr), notice_callback_(nullptr)
{
}

WEB *WEB::get()
{
    if (!singleton_)
    {
        singleton_ = new WEB();
    }
    return singleton_;
}

bool WEB::init()
{
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
    uint32_t pm;
    cyw43_wifi_get_pm(&cyw43_state, &pm);
    printf("Power mode: %x\n", pm);

    mdns_resp_init();

#ifdef USE_HTTPS
    u16_t port = LWIP_IANA_PORT_HTTPS;
    const char *pkey;
    uint16_t    pkeylen;
    get_file_("newkey.pem", pkey, pkeylen);
    const char pkpass[] = "webmouse";
    uint16_t pkpasslen = sizeof(pkpass);
    const char *cert;
    uint16_t    certlen;
    get_file_("newcert.pem", cert, certlen);
    
    struct altcp_tls_config * conf = altcp_tls_create_config_server_privkey_cert((const u8_t *)pkey, pkeylen + 1,
                                                                                 (const u8_t *)pkpass, pkpasslen,
                                                                                 (const u8_t *)cert, certlen + 1);
    if (!conf)
    {
        printf("TLS configuration not loaded\n");
    }
    mbedtls_debug_set_threshold(1);

    altcp_allocator_t alloc = {altcp_tls_alloc, conf};
    #else
    u16_t port = LWIP_IANA_PORT_HTTP;
    struct altcp_tls_config * conf = nullptr;
    altcp_allocator_t alloc = {altcp_tcp_alloc, conf};
    #endif

    struct altcp_pcb *pcb = altcp_new_ip_type(&alloc, IPADDR_TYPE_ANY);

    err_t err = altcp_bind(pcb, IP_ANY_TYPE, port);
    if (err)
    {
        printf("failed to bind to port %u: %d\n", port, err);
        return false;
    }

    server_ = altcp_listen_with_backlog(pcb, 1);
    if (!server_)
    {
        printf("failed to listen\n");
        if (pcb)
        {
            altcp_close(pcb);
        }
        return false;
    }

    altcp_arg(server_, this);
    altcp_accept(server_, tcp_server_accept);
    
    add_repeating_timer_ms(500, timer_callback, this, &timer_);

    return true;
}

bool WEB::connect_to_wifi(const std::string &hostname, const std::string &ssid, const std::string &password)
{
    hostname_ = hostname;
    wifi_ssid_ = ssid;
    wifi_pwd_ = password;
    printf("Host '%s' connecting to Wi-Fi on SSID '%s' ...\n", hostname_.c_str(), wifi_ssid_.c_str());
    netif_set_hostname(wifi_netif(CYW43_ITF_STA), hostname_.c_str());
    bool ret = cyw43_arch_wifi_connect_async(wifi_ssid_.c_str(), wifi_pwd_.c_str(), CYW43_AUTH_WPA2_AES_PSK) == 0;
    return ret;
}

WEB::CLIENT *WEB::addClient(struct altcp_pcb *pcb)
{
    auto it = clientPCB_.find(pcb);
    if (it == clientPCB_.end())
    {
        CLIENT *client = new CLIENT(pcb);
        clientPCB_.emplace(pcb, client->handle());
        clientHndl_.emplace(client->handle(), client);
        return client;
    }
    else
    {
        printf("A client entry for pcb %p already eists. Points to handle %d\n", pcb, it->second);
    }
    return nullptr;
}

void WEB::deleteClient(struct altcp_pcb *pcb)
{
    auto it1 = clientPCB_.find(pcb);
    if (it1 != clientPCB_.end())
    {
        auto it2 = clientHndl_.find(it1->second);
        if (it2 != clientHndl_.end())
        {
            delete it2->second;
            clientHndl_.erase(it2);
        }
        clientPCB_.erase(it1);
    }
}

WEB::CLIENT *WEB::findClient(ClientHandle handle)
{
    auto it = clientHndl_.find(handle);
    if (it != clientHndl_.end())
    {
        return it->second;
    }
    return nullptr;
}

WEB::CLIENT *WEB::findClient(struct altcp_pcb *pcb)
{
    auto it = clientPCB_.find(pcb);
    if (it != clientPCB_.end())
    {
        return findClient(it->second);
    }
    return nullptr;
}

bool WEB::update_wifi(const std::string &hostname, const std::string &ssid, const std::string &password)
{
    bool ret = true;
    if (hostname != hostname_ || ssid != wifi_ssid_ || password != wifi_pwd_)
    {
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        ret = connect_to_wifi(hostname, ssid, password);
        mdns_resp_rename_netif(wifi_netif(CYW43_ITF_STA), hostname_.c_str());
    }
    return ret;
}

err_t WEB::tcp_server_accept(void *arg, struct altcp_pcb *client_pcb, err_t err)
{
    WEB *web = get();
    if (err != ERR_OK || client_pcb == NULL) {
        printf("Failure in accept %d\n", err);
        return ERR_VAL;
    }
    CLIENT *client = web->addClient(client_pcb);
    if (isDebug(1)) printf("Client connected %p (handle %d) (%d clients)\n", client_pcb, client->handle(), web->clientPCB_.size());
    if (isDebug(3))
    {
        for (auto it = web->clientHndl_.cbegin(); it != web->clientHndl_.cend(); ++it)
        {
            printf("  %c-%p (%d)", it->second->isWebSocket() ? 'w' : 'h', it->first, it->second->handle());
        }
        if (web->clientHndl_.size() > 0) printf("\n");
    }

    altcp_arg(client_pcb, client_pcb);
    altcp_sent(client_pcb, tcp_server_sent);
    altcp_recv(client_pcb, tcp_server_recv);
    altcp_poll(client_pcb, tcp_server_poll, 1 * 2);
    altcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

err_t WEB::tcp_server_recv(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    WEB *web = get();
    if (!p)
    {
        web->close_client(tpcb);
        return ERR_OK;
    }

    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0)
    {
        // Receive the buffer
        bool ready = false;
        CLIENT *client = web->findClient(tpcb);
        if (client)
        {
            char buf[64];
            u16_t ll = 0;
            while (ll < p->tot_len)
            {
                u16_t l = pbuf_copy_partial(p, buf, sizeof(buf), ll);
                ll += l;
                client->addToRqst(buf, l);
            }
        }

        altcp_recved(tpcb, p->tot_len);

        if (client)
        {
            while (client->rqstIsReady())
            {
                if (!client->isWebSocket())
                {
                    web->process_rqst(*client);
                }
                else
                {
                    web->process_websocket(*client);
                }

                //  Look up again in case client was closed
                web->findClient(tpcb);
                if (client)
                {
                    client->resetRqst();
                }
            }
        }
    }
    else
    {
        printf("Zero length receive from %p\n", tpcb);
    }
    pbuf_free(p);

    return ERR_OK;
}

err_t WEB::tcp_server_sent(void *arg, struct altcp_pcb *tpcb, u16_t len)
{
    WEB *web = get();
    web->write_next(tpcb);
    return ERR_OK;
}

err_t WEB::tcp_server_poll(void *arg, struct altcp_pcb *tpcb)
{
    WEB *web = get();
    CLIENT *client = web->findClient(tpcb);
    if (client)
    {
        //  Test for match on PCB to avoid race condition on close
        if (client->pcb() == tpcb)
        {
            if (client->more_to_send())
            {
                if (isDebug(1)) printf("Sending to %d on poll (%d clients)\n", client->handle(), web->clientPCB_.size());
                web->write_next(client->pcb());
            }
        }
        else
        {
            printf("Poll on closed pcb %p (client %d)\n", tpcb, client->handle());
        }
    }
    return ERR_OK;
}

void WEB::tcp_server_err(void *arg, err_t err)
{
    WEB *web = get();
    altcp_pcb *client_pcb = (altcp_pcb *)arg;
    printf("Error %d on client %p\n", err, client_pcb);
    CLIENT *client = web->findClient(client_pcb);
    if (client)
    {
        web->close_client(client_pcb, true);
    }
}

err_t WEB::send_buffer(struct altcp_pcb *client_pcb, void *buffer, u16_t buflen, bool allocate)
{
    CLIENT *client = get()->findClient(client_pcb);
    if (client)
    {
        client->queue_send(buffer, buflen, allocate);
        write_next(client_pcb);
    }
    return ERR_OK;
}

err_t WEB::write_next(altcp_pcb *client_pcb)
{
    err_t err = ERR_OK;
    CLIENT *client = get()->findClient(client_pcb);
    if (client)
    {
        u16_t nn = altcp_sndbuf(client_pcb);
        if (nn > TCP_MSS)
        {
            nn = TCP_MSS;
        }
        void *buffer;
        u16_t buflen;
        if (client->get_next(nn, &buffer, &buflen))
        {
            cyw43_arch_lwip_begin();
            cyw43_arch_lwip_check();
            err = altcp_write(client_pcb, buffer, buflen, 0);
            altcp_output(client_pcb);
            cyw43_arch_lwip_end();
            if (err != ERR_OK)
            {
                printf("Failed to write %d bytes of data %d to %p (%d)\n", buflen, err, client_pcb, client->handle());
                client->requeue(buffer, buflen);
            }
        }

        if (client->isClosed() && !client->more_to_send())
        {
            close_client(client_pcb);
        }
    }
    else
    {
        printf("Unknown client %p for write\n", client_pcb);
    }
    return err;    
}

void WEB::process_rqst(CLIENT &client)
{
    bool ok = false;
    bool close = true;
    if (isDebug(2)) printf("Request from %p (%d):\n%s\n", client.pcb(), client.handle(), client.rqst().c_str());
    if (!client.isWebSocket())
    {
        ok = true;
        std::string url = client.http().url();
        if (url == "/ws/")
        {
            open_websocket(client);
        }
        else
        {
            if (client.http().type() == "POST")
            {
                client.http().parseRequest(client.rqst(), true);
            }
            process_http_rqst(client, close);
        }
    }

    if (!ok)
    {
        send_buffer(client.pcb(), (void *)"HTTP/1.0 500 Internal Server Error\r\n\r\n", 38);
    }

    if (!client.isWebSocket() && close)
    {
        close_client(client.pcb());
    }
}

void WEB::process_http_rqst(CLIENT &client, bool &close)
{
    const char *data;
    u16_t datalen = 0;
    bool is_static = false;
    if (http_callback_ && http_callback_(this, client.handle(), client.http(), close))
    {
        ;
    }
    else
    {
        send_buffer(client.pcb(), (void *)"HTTP/1.0 404 NOT_FOUND\r\n\r\n", 26);
    }
}

bool WEB::send_data(ClientHandle client, const char *data, u16_t datalen, bool allocate)
{
    CLIENT *clptr = findClient(client);
    CLIENT *clpcb = findClient(clptr->pcb());
    if (clptr && clpcb == clptr)
    {
        send_buffer(clptr->pcb(), (void *)data, datalen, allocate);
    }
    else
    {
        printf("send_data to non-existent client handle %d (pcb: %p)\n", client, clpcb);
    }
    return clptr != nullptr;
}

void WEB::open_websocket(CLIENT &client)
{
    if (isDebug(1)) printf("Accepting websocket connection on %p (handle %d)\n", client.pcb(), client.handle());
    std::string host = client.http().header("Host");
    std::string key = client.http().header("Sec-WebSocket-Key");
    
    bool hasConnection = client.http().header("Connection").find("Upgrade") != std::string::npos;
    bool hasUpgrade = client.http().header("Upgrade") == "websocket";
    bool hasOrigin = client.http().headerIndex("Origin") > 0;
    bool hasVersion = client.http().header("Sec-WebSocket-Version") == "13";

    if (hasConnection && hasOrigin && hasUpgrade && hasVersion && host.length() > 0)
    {
        key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        unsigned char sha1[20];
        mbedtls_sha1((const unsigned char *)key.c_str(), key.length(), sha1);
        unsigned char b64[64];
        size_t b64ll;
        mbedtls_base64_encode(b64, sizeof(b64), &b64ll, sha1, sizeof(sha1));

        static char resp[]=
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: ";

        static char crlfcrlf[] = "\r\n\r\n";

        send_buffer(client.pcb(), resp, strlen(resp), false);
        send_buffer(client.pcb(), b64, b64ll);
        send_buffer(client.pcb(), crlfcrlf, 4);

        client.setWebSocket();
        client.clearRqst();
    }
    else
    {
        printf("Bad websocket request from %p\n", client.pcb());
    }
}

void WEB::process_websocket(CLIENT &client)
{
    std::string func;
    uint8_t opc = client.wshdr().meta.bits.OPCODE;
    std::string payload = client.rqst().substr(client.wshdr().start, client.wshdr().length);
    switch (opc)
    {
    case WEBSOCKET_OPCODE_TEXT:
        if (payload.substr(0, 5) == "func=")
        {
            std::size_t ii = payload.find_first_of(" ");
            if (ii == std::string::npos)
            {
                func = func = payload.substr(5);
            }
            else
            {
                func = payload.substr(5, ii - 5);
            }
        }

        if (message_callback_)
        {
            message_callback_(this, client.handle(), payload);
        }
        break;

    case WEBSOCKET_OPCODE_PING:
        send_websocket(client.pcb(), WEBSOCKET_OPCODE_PONG, payload);
        break;

    case WEBSOCKET_OPCODE_CLOSE:
        send_websocket(client.pcb(), WEBSOCKET_OPCODE_CLOSE, payload);
        close_client(client.pcb());
        break;

    default:
        printf("Unhandled websocket opcode %d from %p\n", opc, client.pcb());
    }
}

bool WEB::send_message(ClientHandle client, const std::string &message)
{
    CLIENT *clptr = findClient(client);
    CLIENT *clpcb = findClient(clptr->pcb());
    if (clptr && clpcb == clptr)
    {
        if (isDebug(2)) printf("%p (%d) message: %s\n", clptr->pcb(), clptr->handle(), message.c_str());
        send_websocket(clptr->pcb(), WEBSOCKET_OPCODE_TEXT, message);
    }
    else
    {
        printf("send_message to non-existent client handle %d (pcb: %p)\n", client, clpcb);
    }
    return clptr != nullptr;
}

void WEB::send_websocket(struct altcp_pcb *client_pcb, enum WebSocketOpCode opc, const std::string &payload, bool mask)
{
    std::string msg;
    WS::BuildPacket(opc, payload, msg, mask);
    send_buffer(client_pcb, (void *)msg.c_str(), msg.length());
}

void WEB::broadcast_websocket(const std::string &txt)
{
    for (auto it = clientHndl_.begin(); it != clientHndl_.end(); ++it)
    {
        if (it->second->isWebSocket())
        {
            send_websocket(it->second->pcb(), WEBSOCKET_OPCODE_TEXT, txt);
        }
    }
}

void WEB::close_client(struct altcp_pcb *client_pcb, bool isClosed)
{
    CLIENT *client = findClient(client_pcb);
    if (client)
    {
        if (!isClosed)
        {
            client->setClosed();
            if (!client->more_to_send())
            {
                altcp_close(client_pcb);
                if (isDebug(1)) printf("Closed %s %p (%d). client count = %d\n",
                                    (client->isWebSocket() ? "ws" : "http"), client_pcb, client->handle(), clientPCB_.size() - 1);
                deleteClient(client_pcb);
                if (isDebug(3))
                {
                    for (auto it = clientHndl_.cbegin(); it != clientHndl_.cend(); ++it)
                    {
                        printf(" %c-%p (%d)", client->isWebSocket() ? 'w' : 'h', client->pcb(), client->handle());
                    }
                    if (clientHndl_.size() > 0) printf("\n");
                }
            }
            else
            {
                if (isDebug(1)) printf("Waiting to close %s %p (%d)\n", (client->isWebSocket() ? "ws" : "http"), client_pcb, client->handle());
            }
        }
        else
        {
            printf("Closing %s %p (%d)for error\n", (client->isWebSocket() ? "ws" : "http"), client_pcb, client->handle());
            client->setClosed();
            deleteClient(client_pcb);
        }
    }
    else
    {
        if (!isClosed)
        {
            altcp_close(client_pcb);
        }   
    }
}

void WEB::check_wifi()
{
    netif *ni = wifi_netif(CYW43_ITF_STA);
    int sts = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (sts != wifi_state_ || !ip_addr_eq(&ni->ip_addr, &wifi_addr_))
    {
        wifi_state_ = sts;
        wifi_addr_ = ni->ip_addr;

        switch (wifi_state_)
        {
        case CYW43_LINK_JOIN:
        case CYW43_LINK_NOIP:
            // Progress - no action needed
            send_notice(STA_INITIALIZING);
            break;

        case CYW43_LINK_UP:
            //  Connected
            if (mdns_active_)
            {
                mdns_resp_remove_netif(ni);
            }
            mdns_resp_add_netif(ni, hostname_.c_str());
            mdns_resp_announce(ni);
            mdns_active_ = true;
            printf("Connected to WiFi with IP address %s\n", ip4addr_ntoa(netif_ip4_addr(ni)));
            send_notice(STA_CONNECTED);
            break;

        case CYW43_LINK_DOWN:
        case CYW43_LINK_FAIL:
        case CYW43_LINK_NONET:
            //  Not connected
            printf("WiFi disconnected. status = %s\n", (wifi_state_ == CYW43_LINK_DOWN) ? "link down" :
                                                       (wifi_state_ == CYW43_LINK_FAIL) ? "link failed" :
                                                       "No network found");
            send_notice(STA_DISCONNECTED);
            break;

        case CYW43_LINK_BADAUTH:
            //  Need intervention to connect
            printf("WiFi authentication failed\n");
            send_notice(STA_DISCONNECTED);
            break;
        }
    }

    if (ap_active_ > 0)
    {
        ap_active_ -= 1;
        if (ap_active_ == 0)
        {
            stop_ap();
        }
    }
    if (ap_requested_ > 0)
    {
        start_ap();
    }
}

void WEB::scan_wifi(ClientHandle client, WiFiScan_cb callback, void *user_data)
{
    if (!cyw43_wifi_scan_active(&cyw43_state))
    {
        cyw43_wifi_scan_options_t opts = {0};
        int sts = cyw43_wifi_scan(&cyw43_state, &opts, this, scan_cb);
        scans_.clear();
    }
    ScanRqst rqst = {.client = client, .cb = callback, .user_data = user_data };
    scans_.push_back(rqst);
}

int WEB::scan_cb(void *arg, const cyw43_ev_scan_result_t *rslt)
{
    if (rslt)
    {
        std::string ssid;
        ssid.append((char *)rslt->ssid, rslt->ssid_len);
        get()->ssids_[ssid] = rslt->rssi;
    }
    return 0;
}

void WEB::check_scan_finished()
{
    if (scans_.size() > 0 && !cyw43_wifi_scan_active(&cyw43_state))
    {
        for (auto it = scans_.cbegin(); it != scans_.cend(); ++it)
        {
            if (it->cb != nullptr)
            {
                it->cb(this, it->client, ssids_, it->user_data);
            }
        }

        ssids_.clear();
        scans_.clear();
    }
}

bool WEB::timer_callback(repeating_timer_t *rt)
{
    get()->check_wifi();
    get()->check_scan_finished();
    return true;
}

void WEB::start_ap()
{
    if (ap_active_ == 0)
    {
        printf("Starting AP %s\n", ap_name_.c_str());
        cyw43_arch_enable_ap_mode(ap_name_.c_str(), "12345678", CYW43_AUTH_WPA2_AES_PSK);
        netif_set_hostname(wifi_netif(CYW43_ITF_AP), ap_name_.c_str());
    
        // Start the dhcp server
        ip4_addr_t addr;
        ip4_addr_t mask;
        IP4_ADDR(ip_2_ip4(&addr), 192, 168, 4, 1);
        IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);
        dhcp_server_init(&dhcp_, &addr, &mask);
    }
    else
    {
        printf("AP is already active. Timer reset.\n");
    }
    ap_active_ = ap_requested_ * 60 * 2;
    ap_requested_ = 0;
    send_notice(AP_ACTIVE);
}

void WEB::stop_ap()
{
    dhcp_server_deinit(&dhcp_);
    cyw43_arch_disable_ap_mode();
    printf("AP deactivated\n");
    send_notice(AP_INACTIVE);
}




WEB::CLIENT::~CLIENT()
{
    while (sendbuf_.size() > 0)
    {
        delete sendbuf_.front();
        sendbuf_.pop_front();
    }
}

void WEB::CLIENT::addToRqst(const char *str, u16_t ll)
{
    rqst_.append(str, ll);
}

bool WEB::CLIENT::rqstIsReady()
{
    bool ret = false;
    if (!isWebSocket())
    {
        ret = http_.parseRequest(rqst_, false);
    }
    else
    {
        ret = WS::ParsePacket(&wshdr_, rqst_) == WEBSOCKET_SUCCESS;
    }
    return ret;
}

void WEB::CLIENT::queue_send(void *buffer, u16_t buflen, bool allocate)
{
    WEB::SENDBUF *sbuf = new WEB::SENDBUF(buffer, buflen, allocate);
    sendbuf_.push_back(sbuf);
}

bool WEB::CLIENT::get_next(u16_t count, void **buffer, u16_t *buflen)
{
    bool ret = false;
    *buffer = nullptr;
    *buflen = 0;
    if (count > 0)
    {
        while (*buflen == 0 && sendbuf_.size() > 0)
        {
            if (sendbuf_.front()->to_send() > 0)
            {
                ret = sendbuf_.front()->get_next(count, buffer, buflen);
            }
            else
            {
                delete sendbuf_.front();
                sendbuf_.pop_front();
            }
        }
    }

    return ret;
}

void WEB::CLIENT::requeue(void *buffer, u16_t buflen)
{
    if (sendbuf_.size() > 0)
    {
        sendbuf_.front()->requeue(buffer, buflen);
    }
}

void WEB::CLIENT::resetRqst()
{
    if (!isWebSocket())
    {
        std::size_t ii = rqst_.find("\r\n\r\n");
        if (http_.isComplete())
        {
            rqst_.erase(0, http_.size());
        }
        else
        {
            rqst_.clear();
        }
        http_.clear();
    }
    else
    {
        std::size_t ii = wshdr_.start + wshdr_.length;
        if (ii < rqst_.length())
        {
            rqst_.erase(0, ii);
        }
        else
        {
            rqst_.clear();
        }
    }
}

ClientHandle WEB::CLIENT::next_handle_ = 999;

ClientHandle WEB::CLIENT::nextHandle()
{
    ClientHandle start = next_handle_;
    next_handle_ += 1;
    if (next_handle_ == 0) next_handle_ = 1000;
    while (WEB::get()->findClient(next_handle_))
    {
        next_handle_ += 1;
        if (next_handle_ == 0) next_handle_ = 1000;
        if (next_handle_ == start) assert("No client handles");
    }
    return next_handle_;
}

WEB::SENDBUF::SENDBUF(void *buf, uint32_t size, bool alloc) : buffer_((uint8_t *)buf), size_(size), sent_(0), allocated_(alloc)
{
    if (allocated_)
    {
        buffer_ = new uint8_t[size];
        memcpy(buffer_, buf, size);
    }
}

WEB::SENDBUF::~SENDBUF()
{
    if (allocated_)
    {
        delete [] buffer_;
    }
}

bool WEB::SENDBUF::get_next(u16_t count, void **buffer, u16_t *buflen)
{
    bool ret = false;
    *buffer = nullptr;
    uint32_t nn = to_send();
    if (count < nn)
    {
        nn = count;
    }
    if (nn > 0)
    {
        *buffer = &buffer_[sent_];
        ret = true;
    }
    *buflen = nn;
    sent_ += nn;

    return ret;
}

void WEB::SENDBUF::requeue(void *buffer, u16_t buflen)
{
    int32_t nn = sent_ - buflen;
    if (nn >= 0)
    {
        if (memcmp(&buffer_[nn], buffer, buflen) == 0)
        {
            sent_ = nn;
            printf("%d bytes requeued\n", buflen);
        }
        else
        {
            printf("Buffer mismatch! %d bytes not requeued\n", buflen);
        }
    }
    else
    {
        printf("%d bytes to requeue exceeds %d sent\n", buflen, sent_);
    }
}
