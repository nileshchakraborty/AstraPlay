// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#import "DiscoveryBridge.h"
#import "ChiakiSessionBridge.h"
#import "AstraPlayChiakiLog.h"
#include <chiaki/discoveryservice.h>
#include <chiaki/discovery.h>
#include <os/log.h>
#include <arpa/inet.h>

static os_log_t s_disc_log;

// ---- AstraPlayDiscoveredHost ----

@interface AstraPlayDiscoveredHost ()
- (instancetype)initWithCHost:(ChiakiDiscoveryHost *)chost;
@end

@implementation AstraPlayDiscoveredHost

- (instancetype)initWithCHost:(ChiakiDiscoveryHost *)chost {
    self = [super init];
    if (self) {
        switch (chost->state) {
            case CHIAKI_DISCOVERY_HOST_STATE_READY: _state = AstraPlayDiscoveryHostStateReady; break;
            case CHIAKI_DISCOVERY_HOST_STATE_STANDBY: _state = AstraPlayDiscoveryHostStateStandby; break;
            default: _state = AstraPlayDiscoveryHostStateUnknown; break;
        }
        _hostAddr = chost->host_addr ? [NSString stringWithUTF8String:chost->host_addr] : nil;
        _systemVersion = chost->system_version ? [NSString stringWithUTF8String:chost->system_version] : nil;
        _hostName = chost->host_name ? [NSString stringWithUTF8String:chost->host_name] : nil;
        _hostType = chost->host_type ? [NSString stringWithUTF8String:chost->host_type] : nil;
        _hostId = chost->host_id ? [NSString stringWithUTF8String:chost->host_id] : nil;
        _runningAppTitleId = chost->running_app_titleid ? [NSString stringWithUTF8String:chost->running_app_titleid] : nil;
        _runningAppName = chost->running_app_name ? [NSString stringWithUTF8String:chost->running_app_name] : nil;
        _hostRequestPort = chost->host_request_port;
        _isPS5 = chiaki_discovery_host_is_ps5(chost);
    }
    return self;
}

@end

// ---- AstraPlayDiscoveryService ----

@interface AstraPlayDiscoveryService () {
    ChiakiDiscoveryService _service;
    ChiakiLog _log;
    BOOL _running;
}
@property (nonatomic, copy) AstraPlayDiscoveryCallback callback;
@end

static void discovery_log_cb(ChiakiLogLevel level, const char *msg, void *user) {
    (void)user;
    os_log_type_t type = OS_LOG_TYPE_DEFAULT;
    if (level == CHIAKI_LOG_ERROR) type = OS_LOG_TYPE_ERROR;
    else if (level == CHIAKI_LOG_DEBUG || level == CHIAKI_LOG_VERBOSE) type = OS_LOG_TYPE_DEBUG;
    os_log_with_type(s_disc_log, type, "[Discovery] %{public}s", msg ? msg : "");
}

static void discovery_service_cb(ChiakiDiscoveryHost *hosts, size_t hosts_count, void *user) {
    AstraPlayDiscoveryService *svc = (__bridge AstraPlayDiscoveryService *)user;
    NSMutableArray<AstraPlayDiscoveredHost *> *arr = [NSMutableArray arrayWithCapacity:hosts_count];
    for (size_t i = 0; i < hosts_count; i++) {
        [arr addObject:[[AstraPlayDiscoveredHost alloc] initWithCHost:&hosts[i]]];
    }
    NSArray *snapshot = [arr copy];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (svc.callback) {
            svc.callback(snapshot);
        }
    });
}

@implementation AstraPlayDiscoveryService

+ (void)initialize {
    if (self == [AstraPlayDiscoveryService class]) {
        s_disc_log = os_log_create("com.astraplay.stream", "Discovery");
    }
}

- (instancetype)initWithCallback:(AstraPlayDiscoveryCallback)callback {
    self = [super init];
    if (self) {
        _callback = callback;
        _running = NO;
        
        chiaki_session_bridge_init();
        astraplay_chiaki_log_init(&_log, discovery_log_cb, NULL);
        
        // Setup broadcast address (255.255.255.255:987 for PS4, :9302 for PS5)
        // We'll use the generic broadcast that catches both
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(CHIAKI_DISCOVERY_PORT_PS4);
        addr.sin_addr.s_addr = INADDR_BROADCAST;
        
        ChiakiDiscoveryServiceOptions opts;
        memset(&opts, 0, sizeof(opts));
        opts.hosts_max = 16;
        opts.host_drop_pings = 3;
        opts.ping_ms = 500;
        opts.ping_initial_ms = 500;
        opts.send_addr = (struct sockaddr_storage *)&addr;
        opts.send_addr_size = sizeof(addr);
        opts.broadcast_addrs = NULL;
        opts.broadcast_num = 0;
        opts.send_host = "255.255.255.255";
        opts.cb = discovery_service_cb;
        opts.cb_user = (__bridge void *)self;
        
        ChiakiErrorCode err = chiaki_discovery_service_init(&_service, &opts, &_log);
        if (err == CHIAKI_ERR_SUCCESS) {
            _running = YES;
            os_log(s_disc_log, "Discovery service started");
        } else {
            os_log_error(s_disc_log, "Failed to start discovery service: %d", (int)err);
        }
    }
    return self;
}

- (void)shutdown {
    if (_running) {
        chiaki_discovery_service_fini(&_service);
        _running = NO;
        os_log(s_disc_log, "Discovery service stopped");
    }
}

- (void)dealloc {
    [self shutdown];
}

+ (void)wakeupHost:(NSString *)host credential:(uint64_t)credential ps5:(BOOL)ps5 {
    static ChiakiLog wakeup_log;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        wakeup_log = (ChiakiLog){0};
        astraplay_chiaki_log_init(&wakeup_log, discovery_log_cb, NULL);
    });
    chiaki_discovery_wakeup(&wakeup_log, NULL, host.UTF8String, credential, ps5);
}

@end
