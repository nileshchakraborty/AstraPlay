// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// SwiftUI bridge for the touch overlay.

import SwiftUI

/// SwiftUI entry for on-screen controls (`StreamTouchControlsContainerView`).
struct StreamTouchControlsOverlayRepresentable: UIViewRepresentable {
    var mode: StreamTouchControlsMode
    var streamInput: StreamInput
    var isEditingLayout: Bool = false
    /// PS tap: show bottom bar (Disconnect) while still sending PS to the console.
    var onPSButtonPressed: (() -> Void)? = nil

    func makeCoordinator() -> Coordinator {
        Coordinator()
    }

    func makeUIView(context: Context) -> StreamTouchControlsContainerView {
        let v = StreamTouchControlsContainerView(streamInput: streamInput)
        v.onPSButtonPressed = onPSButtonPressed
        v.setEditMode(isEditingLayout)
        return v
    }

    func updateUIView(_ uiView: StreamTouchControlsContainerView, context: Context) {
        uiView.onPSButtonPressed = onPSButtonPressed
        uiView.apply(mode: mode, streamInput: streamInput)
        uiView.setEditMode(isEditingLayout)
    }

    final class Coordinator {}
}
