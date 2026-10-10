// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

import Foundation
import os.log

private let coordLog = OSLog(subsystem: "com.pylux.stream", category: "DonationPrompt")

@MainActor
final class DonationPromptCoordinator: ObservableObject {
    static let shared = DonationPromptCoordinator()

    static let minStreamMs: Int64 = 3_600_000
    static let autoPromptMinIntervalMs: Int64 = 3_600_000
    static let showDelaySeconds: TimeInterval = 5.0

    @Published var showPaywall = false

    private var scheduledTask: Task<Void, Never>?
    private var connectedAt: Date?

    private init() {}

    // MARK: - Stream Auto-Prompt

    func scheduleOfferIfEligible() {
        // Automatic in-stream prompt overlay disabled to avoid intrusive prompts during use.
        return
    }

    func cancelScheduledOffer() {
        scheduledTask?.cancel()
        scheduledTask = nil
    }

    // MARK: - Settings Entry (no gates)

    func openSupportFromSettings() {
        guard !DonationStore.productIDs.isEmpty else { return }
        cancelScheduledOffer()
        scheduledTask = Task { @MainActor in
            await DonationStore.shared.loadProducts()
            guard !Task.isCancelled else { return }
            showPaywall = true
        }
    }

    // MARK: - Stream Time Tracking

    func markConnected() {
        if connectedAt == nil {
            connectedAt = Date()
        }
    }

    func flushStreamTime() {
        guard let start = connectedAt else { return }
        let delta = Int64(Date().timeIntervalSince(start) * 1000)
        if delta > 0 {
            SecureStore.shared.addTotalStreamTimeMs(delta)
        }
        connectedAt = nil
    }
}
