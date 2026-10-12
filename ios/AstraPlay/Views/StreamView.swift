// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Stream screen matching Android's StreamActivity

import SwiftUI
import UIKit
import os.log

private let streamViewLog = OSLog(subsystem: "com.astraplay.stream", category: "StreamView")

private func chiakiQuitReasonDescription(_ reason: Int32) -> String {
    switch reason {
    case 0: return "Stream stopped normally."
    case 1: return "Stopped."
    case 0x01000001: return "Session request failed (unknown reason)."
    case 0x01000002: return "Connection refused by console. It may be in use or not ready."
    case 0x01000003: return "Streaming is already in use on the console."
    case 0x01000004: return "The console ended streaming unexpectedly. Please wait and try again."
    case 0x02000001: return "Control connection failed (unknown)."
    case 0x02000002: return "Control connection refused. Check network settings."
    case 0x04000001: return "Stream connection timed out."
    default:
        return String(format: "Stream ended (code: 0x%08x).", UInt32(bitPattern: reason))
    }
}

enum DisplayMode: String, CaseIterable {
    case fit = "Fit"
    case zoom = "Zoom"
    case stretch = "Stretch"
    case ratio16_10 = "16:10"
    case ratio20_9 = "20:9"
}

struct StreamView: View {
    let connectInfo: StreamConnectInfo
    @Environment(\.dismiss) private var dismiss
    @StateObject private var session: StreamSession
    @State private var showOverlay = true
    @State private var displayMode: DisplayMode = .fit
    @State private var onScreenControls: Bool
    @State private var touchpadOnly: Bool
    @State private var touchpadEmulation: Bool
    @State private var showStats: Bool
    @State private var micEnabled: Bool = true
    @State private var statsText: String = ""
    @State private var isTouchpadPillPressed = false
    /// Previous cumulative dropped-frame total so the overlay can show drops *this tick*
    /// (per second) instead of a lifetime total. -1 = uninitialized.
    @State private var lastDroppedFrames: Int64 = -1
    @State private var showQuitAlert = false
    @State private var showErrorAlert = false
    @State private var errorMessage = ""
    @State private var showPinAlert = false
    @State private var pinIncorrect = false
    @State private var pinEntry = ""
    @State private var videoHostView: StreamVideoUIView?
    @State private var hideOverlayTask: Task<Void, Never>?
    @State private var pipClosedByUser = false
    @State private var pipIsPossible = false
    @State private var connectedAt: Date? = nil
    @State private var touchOpacity: Double = 0.65
    @State private var touchScale: CGFloat = 1.0
    @State private var isEditingLayout = false

    init(connectInfo: StreamConnectInfo) {
        self.connectInfo = connectInfo
        let prefs = StreamPreferences.load()
        let tpOnly = prefs.touchpadOnlyEnabled
        let fullOn = prefs.onScreenControlsEnabled
        _onScreenControls = State(initialValue: tpOnly ? false : fullOn)
        _touchpadOnly = State(initialValue: tpOnly)
        _touchpadEmulation = State(initialValue: prefs.touchpadEmulationEnabled)
        _showStats = State(initialValue: prefs.streamStatsOverlayEnabled)
        let initialMode: DisplayMode
        switch prefs.aspectRatio {
        case "zoom": initialMode = .zoom
        case "stretch": initialMode = .stretch
        case "16_10": initialMode = .ratio16_10
        case "20_9": initialMode = .ratio20_9
        default: initialMode = .fit
        }
        _displayMode = State(initialValue: initialMode)
        _touchOpacity = State(initialValue: Double(prefs.touchControlsOpacity) / 100.0)
        _touchScale = State(initialValue: CGFloat(prefs.touchControlsScale) / 100.0)
        _session = StateObject(wrappedValue: StreamSession(connectInfo: connectInfo, input: StreamInput()))
    }

    private func persistStreamOverlayPreferences() {
        var p = StreamPreferences.load()
        p.onScreenControlsEnabled = onScreenControls
        p.touchpadOnlyEnabled = touchpadOnly
        p.streamStatsOverlayEnabled = showStats
        p.save()
    }

    private func flushStreamTime() {
        guard let start = connectedAt else { return }
        let delta = Int64(Date().timeIntervalSince(start) * 1000)
        if delta > 0 {
            SecureStore.shared.addTotalStreamTimeMs(delta)
        }
        connectedAt = nil
    }

    private var isConnected: Bool {
        if case .connected = session.state { return true }
        return false
    }

    /// Single compact top row with short labels, e.g.
    /// "4.7 Mbps • PL 1.1% • DF/s 0 • 60 FPS • 90 ms • 1280×720".
    private func updateStats() {
        // No native read unless the overlay is actually showing — zero cost when off.
        guard showStats, isConnected, let m = session.metrics() else { return }
        let drops: Int64 = lastDroppedFrames < 0 ? 0 : max(0, Int64(m.droppedFrames) - lastDroppedFrames)
        lastDroppedFrames = Int64(m.droppedFrames)
        var parts: [String] = []
        parts.append(String(format: "%.1f Mbps", m.bitrateMbps))
        parts.append(String(format: "PL %.1f%%", m.packetLoss * 100.0))
        parts.append("DF/s \(drops)")
        parts.append(String(format: "%.0f FPS", m.fps))
        if m.rttMs > 0 { parts.append(String(format: "%.0f ms", m.rttMs)) }
        parts.append("\(m.width)×\(m.height)")
        if let bat = session.input.controllerBattery {
            parts.append("Bat \(bat)")
        }
        statsText = parts.joined(separator: "   •   ")
    }

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            // Video bounds with aspect ratio preset framing (matches Android AspectRatioFrameLayout)
            GeometryReader { geo in
                let targetSize = calculateVideoSize(mode: displayMode, containerSize: geo.size)

                StreamVideoView(
                    aspectRatio: CGFloat(connectInfo.videoWidth) / CGFloat(max(connectInfo.videoHeight, 1)),
                    displayMode: displayMode
                ) { view in
                    videoHostView = view
                    session.attachToView(view)
                }
                .frame(width: targetSize.width, height: targetSize.height)
                .position(x: geo.size.width / 2, y: geo.size.height / 2)
            }
            .ignoresSafeArea()
            .clipped()

            // Always show at least PS when full/touchpad controls are off (no tap-to-show overlay anymore).
            StreamTouchControlsOverlayRepresentable(
                mode: touchpadOnly ? .touchpadOnly : (onScreenControls ? .full : .psOnly),
                streamInput: session.input,
                isEditingLayout: isEditingLayout,
                onPSButtonPressed: {
                    showOverlay = true
                    scheduleHideOverlay()
                }
            )
            .opacity(touchOpacity)
            .scaleEffect(touchScale)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .ignoresSafeArea()
            .allowsHitTesting(true)

            // Performance stats overlay: single top-centered row. Stays visible while
            // toggled on and connected, independent of the auto-hiding control bar.
            if showStats, isConnected {
                VStack {
                    Text(statsText)
                        .font(.system(size: 12, weight: .regular, design: .monospaced))
                        .foregroundColor(.white)
                        .lineLimit(1)
                        .fixedSize()
                        .padding(.horizontal, 12)
                        .padding(.vertical, 5)
                        .background(Color.black.opacity(0.4))
                        .cornerRadius(6)
                        .padding(.top, 1)
                        .allowsHitTesting(false)
                    Spacer()
                }
                .transition(.opacity)
            }

            // Floating DualSense Touchpad Pill (for mobile telescopic grips when on-screen controls are off)
            if touchpadEmulation && !onScreenControls && !touchpadOnly {
                VStack {
                    HStack {
                        Spacer()
                        HStack(spacing: 6) {
                            Image(systemName: "hand.tap.fill")
                                .font(.system(size: 11, weight: .semibold))
                            Text("TOUCHPAD")
                                .font(.system(size: 11, weight: .bold, design: .rounded))
                                .tracking(0.5)
                        }
                        .foregroundColor(.white.opacity(isTouchpadPillPressed ? 0.95 : 0.65))
                        .padding(.horizontal, 14)
                        .padding(.vertical, 7)
                        .background(
                            RoundedRectangle(cornerRadius: 14)
                                .fill(Color.black.opacity(isTouchpadPillPressed ? 0.65 : 0.35))
                                .overlay(
                                    RoundedRectangle(cornerRadius: 14)
                                        .stroke(Color.white.opacity(isTouchpadPillPressed ? 0.45 : 0.15), lineWidth: 1)
                                )
                        )
                        .scaleEffect(isTouchpadPillPressed ? 1.05 : 1.0)
                        .contentShape(Rectangle())
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { value in
                                    if !isTouchpadPillPressed {
                                        isTouchpadPillPressed = true
                                        UIImpactFeedbackGenerator(style: .medium).impactOccurred()
                                        session.input.sendEmulatedTouchpadClick(down: true)
                                        session.input.sendEmulatedTouchpadTouch(phase: 0, xNorm: 0.5, yNorm: 0.5)
                                    } else {
                                        let normX = Float(min(max(value.location.x / 110.0, 0.0), 1.0))
                                        let normY = Float(min(max(value.location.y / 28.0, 0.0), 1.0))
                                        session.input.sendEmulatedTouchpadTouch(phase: 1, xNorm: normX, yNorm: normY)
                                    }
                                }
                                .onEnded { _ in
                                    isTouchpadPillPressed = false
                                    session.input.sendEmulatedTouchpadClick(down: false)
                                    session.input.sendEmulatedTouchpadTouch(phase: 2, xNorm: 0.5, yNorm: 0.5)
                                }
                        )
                        .padding(.top, 14)
                        .padding(.trailing, 24)
                    }
                    Spacer()
                }
                .transition(.opacity)

                // Invisible double-tap gesture recognizer on top 20% of screen for quick touchpad click
                GeometryReader { geo in
                    VStack {
                        Color.clear
                            .frame(width: geo.size.width, height: geo.size.height * 0.20)
                            .contentShape(Rectangle())
                            .onTapGesture(count: 2) {
                                UIImpactFeedbackGenerator(style: .heavy).impactOccurred()
                                session.input.sendEmulatedTouchpadClick(down: true)
                                DispatchQueue.main.asyncAfter(deadline: .now() + 0.15) {
                                    session.input.sendEmulatedTouchpadClick(down: false)
                                }
                            }
                        Spacer()
                    }
                }
            }

            // Bottom overlay bar (matches Android's stream overlay)
            if showOverlay {
                VStack {
                    Spacer()
                    overlayBar
                }
                .transition(.move(edge: .bottom))
            }

            // Full-screen connecting overlay (above touch controls so STUN/holepunch is always visible)
            if case .connecting = session.state {
                ZStack {
                    Color.black.opacity(0.62)
                        .ignoresSafeArea()
                    VStack(spacing: 18) {
                        ProgressView()
                            .progressViewStyle(CircularProgressViewStyle(tint: .white))
                            .scaleEffect(1.35)
                        Text(session.connectionPhase.isEmpty ? "Connecting…" : session.connectionPhase)
                            .font(.headline)
                            .foregroundColor(.white)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 28)
                        Text("This can take a while on some networks.")
                            .font(.subheadline)
                            .foregroundColor(.white.opacity(0.72))
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 36)
                    }
                }
                .allowsHitTesting(true)
            }

            // PIN entry in-stream (not .alert TextField, not fullScreenCover): those paths use separate UIKit
            // controllers that can force portrait and fight AppOrientationLock, then crash when rotating back.
            if showPinAlert {
                LoginPinEntryView(
                    pinIncorrect: pinIncorrect,
                    pinEntry: $pinEntry,
                    onSubmit: {
                        let pin = pinEntry
                        pinEntry = ""
                        showPinAlert = false
                        session.sendLoginPin(pin)
                    },
                    onCancel: {
                        pinEntry = ""
                        showPinAlert = false
                        session.pause()
                        dismiss()
                    }
                )
                .zIndex(20_000)
                .allowsHitTesting(true)
            }
        }
        // NavigationStack / fullScreenCover still propose safe-area-inset size to children; expand to physical edges
        // so zoom (resizeAspectFill) can use full width (Android SurfaceView is match_parent).
        .ignoresSafeArea()
        .statusBar(hidden: true)
        .onAppear {
            // Match Android StreamActivity userLandscape (async so presentation from parent is committed).
            DispatchQueue.main.async { AppOrientationLock.lockLandscapeForStream() }
            session.pipManager.pipClosedByUser = { [weak session] in
                session?.pause()
                pipClosedByUser = true
            }
            session.resume()
            scheduleHideOverlay()
        }
        .onChange(of: onScreenControls) { on in
            if on { touchpadOnly = false }
            if !on && !touchpadOnly { session.input.clearTouchOverlayState() }
            persistStreamOverlayPreferences()
        }
        .onChange(of: touchpadOnly) { on in
            if on { onScreenControls = false }
            if !on && !onScreenControls { session.input.clearTouchOverlayState() }
            persistStreamOverlayPreferences()
        }
        .onChange(of: showStats) { on in
            if on { lastDroppedFrames = -1 } // first tick reads 0, not the lifetime total
            persistStreamOverlayPreferences()
        }
        // Refresh the overlay once per second to match libchiaki's CONNECTIONQUALITY
        // cadence. The closure no-ops (no native read) unless the overlay is showing.
        .onReceive(Timer.publish(every: 1, on: .main, in: .common).autoconnect()) { _ in
            updateStats()
        }
        .onDisappear {
            session.input.clearTouchOverlayState()
            AppOrientationLock.unlockAfterStream()
            flushStreamTime()
            if !session.pipManager.isActive {
                session.pause()
            }
        }
        .onChange(of: session.menuRequestToken) { _ in
            // OPTIONS+SHARE chord fired -> surface the in-stream menu (auto-hides), same as a tap.
            showOverlay = true
            scheduleHideOverlay()
        }
        .onChange(of: session.state) { newState in
            switch newState {
            case .connected:
                if let view = videoHostView {
                    session.attachToView(view)
                }
                lastDroppedFrames = -1 // reset stats baseline for the new session
                if connectedAt == nil {
                    connectedAt = Date()
                }
            case .quit(_, _):
                flushStreamTime()
                showQuitAlert = true
            case .createError(let code, let message):
                flushStreamTime()
                if let m = message, !m.isEmpty {
                    errorMessage = "\(m) (code \(code))"
                } else {
                    errorMessage = "Connection failed (code \(code))"
                }
                showErrorAlert = true
            case .loginPinRequest(let incorrect):
                flushStreamTime()
                pinIncorrect = incorrect
                pinEntry = ""
                showPinAlert = true
            default:
                break
            }
        }
        .alert("Stream Ended", isPresented: $showQuitAlert) {
            Button("OK") { dismiss() }
        } message: {
            if case .quit(let reason, let reasonStr) = session.state {
                let msg = reasonStr ?? chiakiQuitReasonDescription(reason)
                Text(msg)
            } else {
                Text("The stream has ended.")
            }
        }
        .alert("Connection Error", isPresented: $showErrorAlert) {
            Button("OK") { dismiss() }
        } message: {
            Text(errorMessage)
        }
        .onChange(of: showPinAlert) { show in
            if show {
                AppOrientationLock.lockLandscapeForStream()
            }
        }
        .onChange(of: pipClosedByUser) { closed in
            if closed { dismiss() }
        }
        .onReceive(session.pipManager.$isPossible) { possible in
            pipIsPossible = possible
        }
    }

    // MARK: - Overlay bar (matches Android's stream overlay: controls, display mode, disconnect)

    private var overlayBar: some View {
        HStack(spacing: 12) {
            // On-Screen Controls toggle (matches Android's onScreenControlsSwitch)
            Toggle("On-Screen Controls", isOn: $onScreenControls)
                .toggleStyle(.switch)
                .font(.system(size: 13))
                .foregroundColor(.white)
                .fixedSize()

            // Touchpad Only toggle (matches Android's touchpadOnlySwitch)
            Toggle("Touchpad only", isOn: $touchpadOnly)
                .toggleStyle(.switch)
                .font(.system(size: 13))
                .foregroundColor(.white)
                .fixedSize()

            // Performance stats overlay toggle (matches Android's statsSwitch)
            Toggle("Stats", isOn: $showStats)
                .toggleStyle(.switch)
                .font(.system(size: 13))
                .foregroundColor(.white)
                .fixedSize()

            // Microphone mute toggle (matches Desktop and Android's micSwitch)
            Toggle("Mic", isOn: $micEnabled)
                .toggleStyle(.switch)
                .font(.system(size: 13))
                .foregroundColor(.white)
                .fixedSize()
                .onChange(of: micEnabled) { enabled in
                    session.toggleMicrophone(muted: !enabled)
                    scheduleHideOverlay()
                }

            if let battery = session.input.controllerBattery {
                HStack(spacing: 4) {
                    Image(systemName: "gamecontroller")
                        .font(.system(size: 12))
                    Text(battery)
                        .font(.system(size: 12, weight: .medium))
                }
                .foregroundColor(.white.opacity(0.9))
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(Color.white.opacity(0.15))
                .cornerRadius(6)
            }

            // Edit Layout button (matches Android's editLayoutButton)
            Button {
                isEditingLayout.toggle()
                if !isEditingLayout {
                    scheduleHideOverlay()
                }
            } label: {
                Text(isEditingLayout ? "Done" : "Edit Layout")
                    .lineLimit(1)
            }
            .font(.system(size: 13, weight: .medium))
            .foregroundColor(isEditingLayout ? .yellow : .white)
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .overlay(
                RoundedRectangle(cornerRadius: 8)
                    .stroke(isEditingLayout ? Color.yellow : Color.white.opacity(0.5), lineWidth: 1)
            )

            if isEditingLayout {
                Button {
                    var prefs = StreamPreferences.load()
                    prefs.touchControlsLayoutPreset = "standard"
                    prefs.touchControlsCustomOffsets = ""
                    prefs.save()
                    isEditingLayout = false
                    scheduleHideOverlay()
                } label: {
                    Text("Reset")
                        .lineLimit(1)
                }
                .font(.system(size: 13, weight: .medium))
                .foregroundColor(.red)
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .overlay(
                    RoundedRectangle(cornerRadius: 8)
                        .stroke(Color.red, lineWidth: 1)
                )
            }

            Spacer()

            // Display mode toggle group (matches Android's displayModeToggle)
            HStack(spacing: 0) {
                displayModeButton(.fit, icon: "rectangle.arrowtriangle.2.inward")
                zoomModeButton()
                displayModeButton(.stretch, icon: "arrow.left.and.right")
            }
            .background(
                RoundedRectangle(cornerRadius: 8)
                    .stroke(Color.white.opacity(0.5), lineWidth: 1)
            )

            // Picture in Picture button
            if pipIsPossible {
                Button {
                    session.pipManager.startIfPossible()
                } label: {
                    Image(systemName: "pip.enter")
                        .font(.system(size: 14))
                        .foregroundColor(.white.opacity(0.9))
                        .frame(width: 48, height: 36)
                }
                .overlay(
                    RoundedRectangle(cornerRadius: 8)
                        .stroke(Color.white.opacity(0.5), lineWidth: 1)
                )
            }

            // Disconnect button (matches Android's disconnectButton). The bar is
            // crowded (toggles + display modes + PiP), so keep the label on one
            // line and let it scale down instead of wrapping ("Disconn/ect").
            Button {
                session.pause()
                dismiss()
            } label: {
                Text("Disconnect")
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
            }
            .font(.system(size: 13, weight: .medium))
            .foregroundColor(.white)
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .overlay(
                RoundedRectangle(cornerRadius: 8)
                    .stroke(Color.white.opacity(0.5), lineWidth: 1)
            )
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 8)
        .background(Color.black.opacity(0.6))
    }

    private var zoomOrPresetMode: DisplayMode {
        let pref = StreamPreferences.load().aspectRatio
        switch pref {
        case "16_10": return .ratio16_10
        case "20_9": return .ratio20_9
        default: return .zoom
        }
    }

    private var isZoomOrPresetActive: Bool {
        displayMode == .zoom || displayMode == .ratio16_10 || displayMode == .ratio20_9
    }

    private var zoomModeIcon: String {
        switch displayMode {
        case .ratio16_10: return "aspectratio"
        case .ratio20_9: return "rectangle.ratio.16.to.9"
        case .zoom: return "arrow.up.left.and.arrow.down.right"
        default:
            switch zoomOrPresetMode {
            case .ratio16_10: return "aspectratio"
            case .ratio20_9: return "rectangle.ratio.16.to.9"
            default: return "arrow.up.left.and.arrow.down.right"
            }
        }
    }

    private func zoomModeButton() -> some View {
        Menu {
            Button("Zoom (Fill / Crop)") { displayMode = .zoom }
            Button("16:10 (iPad / Steam Deck)") { displayMode = .ratio16_10 }
            Button("20:9 (Ultrawide)") { displayMode = .ratio20_9 }
        } label: {
            Image(systemName: zoomModeIcon)
                .font(.system(size: 14))
                .foregroundColor(isZoomOrPresetActive ? .accentColor : .white.opacity(0.7))
                .frame(width: 48, height: 36)
        } primaryAction: {
            displayMode = zoomOrPresetMode
        }
    }

    private func displayModeButton(_ mode: DisplayMode, icon: String) -> some View {
        Button {
            displayMode = mode
        } label: {
            Image(systemName: icon)
                .font(.system(size: 14))
                .foregroundColor(displayMode == mode ? .accentColor : .white.opacity(0.7))
                .frame(width: 48, height: 36)
        }
    }

    private func calculateVideoSize(mode: DisplayMode, containerSize: CGSize) -> CGSize {
        guard containerSize.width > 0 && containerSize.height > 0 else {
            return containerSize
        }
        let containerAspect = containerSize.width / containerSize.height
        let targetAspect: CGFloat
        switch mode {
        case .fit, .zoom, .stretch:
            return containerSize
        case .ratio16_10:
            targetAspect = 16.0 / 10.0
        case .ratio20_9:
            targetAspect = 20.0 / 9.0
        }

        var width = containerSize.width
        var height = containerSize.height

        if containerAspect < targetAspect {
            width = containerSize.height * targetAspect
        } else {
            height = containerSize.width / targetAspect
        }

        return CGSize(width: width, height: height)
    }

    private func scheduleHideOverlay() {
        guard !isEditingLayout else { return }
        hideOverlayTask?.cancel()
        hideOverlayTask = Task {
            try? await Task.sleep(nanoseconds: 4_000_000_000) // 4 seconds, matches Android
            if !Task.isCancelled {
                withAnimation { showOverlay = false }
            }
        }
    }
}

// MARK: - Login PIN entry (in-stream overlay; avoids UIAlertController and fullScreenCover)

private struct LoginPinEntryView: View {
    let pinIncorrect: Bool
    @Binding var pinEntry: String
    let onSubmit: () -> Void
    let onCancel: () -> Void
    @State private var keyboardBottomInset: CGFloat = 0

    var body: some View {
        ZStack {
            Color.black.opacity(0.92)
                .ignoresSafeArea()
            VStack(spacing: 18) {
                Text(pinIncorrect ? "Incorrect PIN" : "Login PIN required")
                    .font(.title2.bold())
                    .foregroundStyle(.white)
                    .multilineTextAlignment(.center)
                Text(
                    pinIncorrect
                        ? "The PIN was incorrect. Enter the PIN shown on your console."
                        : "Enter the PIN displayed on your console."
                )
                .font(.body)
                .foregroundStyle(.white.opacity(0.88))
                .multilineTextAlignment(.center)
                TextField("PIN", text: $pinEntry)
                    .keyboardType(.numberPad)
                    .textContentType(.oneTimeCode)
                    .multilineTextAlignment(.center)
                    .font(.system(size: 26, weight: .medium, design: .monospaced))
                    .padding(.vertical, 12)
                    .padding(.horizontal, 16)
                    .background(RoundedRectangle(cornerRadius: 12).fill(Color.white.opacity(0.15)))
                    .foregroundStyle(.white)
                    .frame(maxWidth: 280)
                    .onChange(of: pinEntry) { newValue in
                        let digits = String(newValue.filter(\.isNumber).prefix(4))
                        if digits != newValue {
                            pinEntry = digits
                        }
                    }
                HStack(spacing: 24) {
                    Button("Cancel", role: .cancel, action: onCancel)
                        .foregroundStyle(.white)
                    Button("Submit", action: onSubmit)
                        .buttonStyle(.borderedProminent)
                        .disabled(pinEntry.count != 4)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            .padding(.horizontal, 28)
            .padding(.top, 52)
            .padding(.bottom, 20 + keyboardBottomInset)
        }
        .onReceive(NotificationCenter.default.publisher(for: UIResponder.keyboardWillChangeFrameNotification)) { note in
            guard let frame = note.userInfo?[UIResponder.keyboardFrameEndUserInfoKey] as? CGRect else { return }
            let duration = (note.userInfo?[UIResponder.keyboardAnimationDurationUserInfoKey] as? Double) ?? 0.25
            let inset = keyboardBottomOverlap(keyboardFrameScreen: frame)
            withAnimation(.easeOut(duration: duration)) {
                keyboardBottomInset = inset
            }
        }
        .onReceive(NotificationCenter.default.publisher(for: UIResponder.keyboardWillHideNotification)) { note in
            let duration = (note.userInfo?[UIResponder.keyboardAnimationDurationUserInfoKey] as? Double) ?? 0.25
            withAnimation(.easeOut(duration: duration)) {
                keyboardBottomInset = 0
            }
        }
    }

    private func keyboardBottomOverlap(keyboardFrameScreen: CGRect) -> CGFloat {
        guard let window = UIApplication.shared.connectedScenes
            .compactMap({ $0 as? UIWindowScene })
            .flatMap({ $0.windows })
            .first(where: { $0.isKeyWindow })
        else {
            return max(0, UIScreen.main.bounds.height - keyboardFrameScreen.minY)
        }
        let kbInWindow = window.convert(keyboardFrameScreen, from: nil)
        return max(0, window.bounds.maxY - kbInWindow.minY)
    }
}
