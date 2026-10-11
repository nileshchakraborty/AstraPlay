// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// ObjC bridge to Chiaki discovery service for iOS

#ifndef DiscoveryBridge_h
#define DiscoveryBridge_h

#import <Foundation/Foundation.h>
#include <stdbool.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

/// Discovered host state
typedef NS_ENUM(NSInteger, AstraPlayDiscoveryHostState) {
    AstraPlayDiscoveryHostStateUnknown = 0,
    AstraPlayDiscoveryHostStateReady = 1,
    AstraPlayDiscoveryHostStateStandby = 2,
};

/// Discovered host info (immutable snapshot)
@interface AstraPlayDiscoveredHost : NSObject
@property (nonatomic, readonly) AstraPlayDiscoveryHostState state;
@property (nonatomic, readonly, nullable) NSString *hostAddr;
@property (nonatomic, readonly, nullable) NSString *systemVersion;
@property (nonatomic, readonly, nullable) NSString *hostName;
@property (nonatomic, readonly, nullable) NSString *hostType;
@property (nonatomic, readonly, nullable) NSString *hostId;
@property (nonatomic, readonly, nullable) NSString *runningAppTitleId;
@property (nonatomic, readonly, nullable) NSString *runningAppName;
@property (nonatomic, readonly) uint16_t hostRequestPort;
@property (nonatomic, readonly) BOOL isPS5;
@end

/// Callback type for discovery updates
typedef void (^AstraPlayDiscoveryCallback)(NSArray<AstraPlayDiscoveredHost *> *hosts);

/// Discovery service - wraps ChiakiDiscoveryService
@interface AstraPlayDiscoveryService : NSObject

/// Create and start the discovery service.
/// @param callback Called on main thread when host list changes.
- (instancetype)initWithCallback:(AstraPlayDiscoveryCallback)callback;

/// Stop and destroy the discovery service.
- (void)shutdown;

/// Send a wakeup packet to a host.
+ (void)wakeupHost:(NSString *)host credential:(uint64_t)credential ps5:(BOOL)ps5;

@end

NS_ASSUME_NONNULL_END

#endif /* DiscoveryBridge_h */
