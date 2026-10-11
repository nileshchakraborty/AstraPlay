// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// ObjC bridge to Chiaki holepunch API for PSN Remote Play connections

#ifndef HolepunchBridge_h
#define HolepunchBridge_h

#import <Foundation/Foundation.h>
#include <stdbool.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

/// Console type for holepunch
typedef NS_ENUM(NSInteger, AstraPlayHolepunchConsoleType) {
    AstraPlayHolepunchConsoleTypePS4 = 0,
    AstraPlayHolepunchConsoleTypePS5 = 1,
};

/// Port type for holepunch
typedef NS_ENUM(NSInteger, AstraPlayHolepunchPortType) {
    AstraPlayHolepunchPortTypeCTRL = 0,
    AstraPlayHolepunchPortTypeDATA = 1,
};

/// Device info from PSN discovery
@interface AstraPlayHolepunchDevice : NSObject
@property (nonatomic, assign) AstraPlayHolepunchConsoleType consoleType;
@property (nonatomic, strong) NSString *deviceName;
@property (nonatomic, strong) NSData *deviceUid;  // 32 bytes
@property (nonatomic, assign) BOOL remoteplayEnabled;
/// Hex string of device UID
@property (nonatomic, readonly) NSString *duidHex;
@end

/// Opaque holepunch session handle
@interface AstraPlayHolepunchSession : NSObject

/// Initialize a holepunch session with a valid PSN OAuth2 token.
- (nullable instancetype)initWithToken:(NSString *)psnOAuth2Token;

/// List devices associated with PSN account.
/// @param token PSN OAuth2 access token
/// @param consoleType PS4 or PS5
/// @return Array of AstraPlayHolepunchDevice, or nil on error
+ (nullable NSArray<AstraPlayHolepunchDevice *> *)listDevicesWithToken:(NSString *)token
                                                       consoleType:(AstraPlayHolepunchConsoleType)consoleType;

/// Discover UPnP (non-fatal if fails).
- (int)upnpDiscover;

/// Create session on PSN server.
- (int)createSession;

/// Create offer for CTRL connection.
- (int)createOffer;

/// Start session for a specific console.
- (int)startWithDuid:(NSData *)duid consoleType:(AstraPlayHolepunchConsoleType)consoleType;

/// Human-readable detail from the last failed `startWithDuid` (or empty). For UI / debugging.
- (NSString *)lastStartErrorMessage;

/// Punch hole for the specified port type.
- (int)punchHole:(AstraPlayHolepunchPortType)portType;

/// Get the native session pointer (for passing to ChiakiSessionBridge).
- (uintptr_t)nativePtr;

/// Cancel holepunch operations.
- (void)cancel;

/// Mark the session as consumed by a native chiaki session (which takes ownership).
/// Prevents double-free: the native session will call chiaki_holepunch_session_fini itself.
- (void)markConsumed;

/// Finalize and free the session. Call after streaming ends.
- (void)fini;

@end

NS_ASSUME_NONNULL_END

#endif /* HolepunchBridge_h */
