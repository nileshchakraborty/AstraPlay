// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// AstraPlay relay login service
// Mirrors Android's PsnLoginActivity.kt exactly

import Foundation
import os.log

private let loginLog = OSLog(subsystem: "com.astraplay.stream", category: "AstraPlayLogin")

class AstraPlayLoginService {
    static let shared = AstraPlayLoginService()
    
    /// Effective relay server URL for PSN login.
    /// Priority order:
    /// 1. User-configured custom relay URL in SecureStore (Keychain)
    /// 2. ASTRAPLAY_RELAY_URL environment variable
    var astraplayURL: String {
        let custom = SecureStore.shared.relayServerUrl.trimmingCharacters(in: CharacterSet(charactersIn: "/ "))
        if !custom.isEmpty { return custom }
        return (ProcessInfo.processInfo.environment["ASTRAPLAY_RELAY_URL"] ?? "")
            .trimmingCharacters(in: CharacterSet(charactersIn: "/ "))
    }
    
    /// Check whether a relay URL is configured (either in SecureStore or environment)
    var isRelayConfigured: Bool {
        return !astraplayURL.isEmpty
    }
    
    /// Generate UUID v4 for OAuth cid parameter
    func generateUUID() -> String {
        return UUID().uuidString.lowercased()
    }
    
    /// Build OAuth v3 authorization URL
    /// Mirrors desktop app's OAuth flow (gui/src/qmlbackend.cpp)
    func buildOAuthURL() -> URL? {
        let cid = generateUUID()
        let clientId = "ba495a24-818c-472b-b12d-ff231c1b5745"
        let redirectUri = "https://remoteplay.dl.playstation.net/remoteplay/redirect"
        let scope = "psn:clientapp referenceDataService:countryConfig.read pushNotification:webSocket.desktop.connect sessionManager:remotePlaySession.system.update"
        
        var components = URLComponents(string: "https://ca.account.sony.com/api/authz/v3/oauth/authorize")
        components?.queryItems = [
            URLQueryItem(name: "client_id", value: clientId),
            URLQueryItem(name: "redirect_uri", value: redirectUri),
            URLQueryItem(name: "scope", value: scope),
            URLQueryItem(name: "response_type", value: "code"),
            URLQueryItem(name: "service_entity", value: "urn:service-entity:psn"),
            URLQueryItem(name: "access_type", value: "offline"),
            URLQueryItem(name: "smcid", value: "remoteplay"),
            URLQueryItem(name: "layout_type", value: "popup"),
            URLQueryItem(name: "PlatformPrivacyWs1", value: "minimal"),
            URLQueryItem(name: "no_captcha", value: "true"),
            URLQueryItem(name: "cid", value: cid)
        ]
        
        return components?.url
    }
    
    /// Generate random 6-character alphanumeric code (excluding similar looking chars)
    func generateLoginCode() -> String {
        let chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789" // Exclude I,1,O,0
        return String((0..<6).map { _ in chars.randomElement()! })
    }
    
    /// Create code on the relay server
    /// Reference: gui/src/qmlbackend.cpp lines 3363-3440
    func createCode(_ code: String) async -> Bool {
        guard isRelayConfigured else {
            os_log(.error, log: loginLog, "Relay login failed: No relay server URL configured")
            return false
        }
        guard let url = URL(string: "\(astraplayURL)/psstream/create-code") else {
            os_log(.error, log: loginLog, "Invalid create-code URL: %{public}s", astraplayURL)
            return false
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        let payload: [String: String] = ["code": code]
        guard let jsonData = try? JSONSerialization.data(withJSONObject: payload) else {
            os_log(.error, log: loginLog, "Failed to encode JSON payload")
            return false
        }
        request.httpBody = jsonData
        
        do {
            let (data, response) = try await URLSession.shared.data(for: request)
            
            guard let httpResponse = response as? HTTPURLResponse else {
                os_log(.error, log: loginLog, "Invalid response type")
                return false
            }
            
            guard httpResponse.statusCode == 200 else {
                os_log(.error, log: loginLog, "HTTP error creating code: %d", httpResponse.statusCode)
                return false
            }
            
            guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let result = json["result"] as? String,
                  result == "success" else {
                let errorMsg = (try? JSONSerialization.jsonObject(with: data) as? [String: Any])?["error"] as? String ?? "Unknown error"
                os_log(.error, log: loginLog, "Server error: %{public}s", errorMsg)
                return false
            }
            
            os_log(.info, log: loginLog, "Code created successfully on relay server")
            return true
            
        } catch {
            os_log(.error, log: loginLog, "Exception creating code: %{public}s", error.localizedDescription)
            return false
        }
    }
    
    /// Check token status on the relay server
    /// Reference: gui/src/qmlbackend.cpp lines 3442-3525
    func checkTokenStatus(_ code: String) async -> String? {
        guard isRelayConfigured else {
            os_log(.error, log: loginLog, "Check tokens failed: No relay server URL configured")
            return nil
        }
        guard let url = URL(string: "\(astraplayURL)/psstream/get-tokens") else {
            os_log(.error, log: loginLog, "Invalid get-tokens URL: %{public}s", astraplayURL)
            return nil
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        let payload: [String: String] = ["code": code]
        guard let jsonData = try? JSONSerialization.data(withJSONObject: payload) else {
            os_log(.error, log: loginLog, "Failed to encode JSON payload")
            return nil
        }
        request.httpBody = jsonData
        
        do {
            let (data, response) = try await URLSession.shared.data(for: request)
            
            guard let httpResponse = response as? HTTPURLResponse else {
                os_log(.error, log: loginLog, "Invalid response type")
                return nil
            }
            
            guard httpResponse.statusCode == 200 else {
                os_log(.default, log: loginLog, "HTTP response code: %d", httpResponse.statusCode)
                return nil
            }
            
            guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let result = json["result"] as? String else {
                os_log(.error, log: loginLog, "Invalid JSON response")
                return nil
            }
            
            switch result {
            case "success":
                if let npsso = json["npsso"] as? String, !npsso.isEmpty {
                    os_log(.info, log: loginLog, "Received NPSSO token (length: %d)", npsso.count)
                    return npsso
                } else {
                    os_log(.default, log: loginLog, "Success result but empty npsso field")
                    return nil
                }
                
            case "pending", "":
                os_log(.default, log: loginLog, "Token status: pending or not found")
                return nil
                
            case "error":
                let errorMsg = json["error"] as? String ?? "Unknown error"
                os_log(.error, log: loginLog, "Server error: %{public}s", errorMsg)
                return nil
                
            default:
                os_log(.default, log: loginLog, "Unknown result value: %{public}s", result)
                return nil
            }
            
        } catch {
            os_log(.error, log: loginLog, "Exception checking status: %{public}s", error.localizedDescription)
            return nil
        }
    }
    
    /// Get the browser URL for user login
    func getLoginURL(code: String) -> URL? {
        guard isRelayConfigured else { return nil }
        return URL(string: "\(astraplayURL)/psstream/?psstream_code=\(code)")
    }
}
