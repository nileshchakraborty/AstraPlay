// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// ObjC bridge to Chiaki registration for iOS

#ifndef RegistBridge_h
#define RegistBridge_h

#import <Foundation/Foundation.h>
#include <stdbool.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

/// Console target version for registration
typedef NS_ENUM(NSInteger, AstraPlayRegistTarget) {
    AstraPlayRegistTargetPS4_LT7 = 800,
    AstraPlayRegistTargetPS4_GE7 = 900,
    AstraPlayRegistTargetPS4_GE8 = 1000,
    AstraPlayRegistTargetPS5 = 1000100,
};

/// Registration result event
typedef NS_ENUM(NSInteger, AstraPlayRegistResult) {
    AstraPlayRegistResultCanceled = 0,
    AstraPlayRegistResultFailed = 1,
    AstraPlayRegistResultSuccess = 2,
};

/// Registered host data returned on success
@interface AstraPlayRegisteredHostData : NSObject
@property (nonatomic, assign) NSInteger target;
@property (nonatomic, strong) NSString *apSsid;
@property (nonatomic, strong) NSString *apBssid;
@property (nonatomic, strong) NSString *apKey;
@property (nonatomic, strong) NSString *apName;
@property (nonatomic, strong) NSData *serverMac; // 6 bytes
@property (nonatomic, strong, nullable) NSString *serverNickname;
@property (nonatomic, strong) NSData *rpRegistKey; // CHIAKI_SESSION_AUTH_SIZE (16) bytes
@property (nonatomic, assign) uint32_t rpKeyType;
@property (nonatomic, strong) NSData *rpKey; // 16 bytes
@end

/// Registration info to start registration
@interface AstraPlayRegistInfo : NSObject
@property (nonatomic, assign) AstraPlayRegistTarget target;
@property (nonatomic, strong) NSString *host;
@property (nonatomic, assign) BOOL broadcast;
@property (nonatomic, strong, nullable) NSString *psnOnlineId;
@property (nonatomic, strong, nullable) NSData *psnAccountId; // 8 bytes
@property (nonatomic, assign) uint32_t pin;
@end

/// Callback for registration events
typedef void (^AstraPlayRegistCallback)(AstraPlayRegistResult result, AstraPlayRegisteredHostData * _Nullable host, NSString * _Nullable logText);

/// Registration service
@interface AstraPlayRegistService : NSObject

- (instancetype)initWithInfo:(AstraPlayRegistInfo *)info callback:(AstraPlayRegistCallback)callback;
- (void)stop;

@end

NS_ASSUME_NONNULL_END

#endif /* RegistBridge_h */
