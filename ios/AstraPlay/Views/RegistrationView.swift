// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Console registration matching Android's RegistActivity + RegistExecuteActivity

import SwiftUI

enum ConsoleVersion: String, CaseIterable, Identifiable {
    case ps5 = "PS5"
    case ps4GE8 = "PS4 >= 8.0"
    case ps4GE7 = "PS4 >= 7.0, < 8"
    case ps4LT7 = "PS4 < 7.0"

    var id: String { rawValue }

    var isPS5: Bool { self == .ps5 }

    var registTarget: AstraPlayRegistTarget {
        switch self {
        case .ps5: return .PS5
        case .ps4GE8: return .PS4_GE8
        case .ps4GE7: return .PS4_GE7
        case .ps4LT7: return .PS4_LT7
        }
    }

    var chiakiTarget: Int {
        Int(registTarget.rawValue)
    }
}

struct RegistrationView: View {
    @ObservedObject var hostStore: HostStore
    @Environment(\.dismiss) private var dismiss

    @State private var host = "255.255.255.255"
    @State private var broadcast = true
    @State private var consoleVersion: ConsoleVersion = .ps5
    @State private var psnId = ""
    @State private var pin = ""
    @State private var isExecuting = false
    @State private var logText = ""
    @State private var resultMessage: String?
    @State private var registSuccess = false
    @State private var registService: AstraPlayRegistService?
    @State private var showingLoginSheet = false
    @State private var showingManualNpssoSheet = false
    @State private var manualNpssoInput = ""
    @State private var manualNpssoError: String?

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // Icon + title (matches Android's iconImageView + titleTextView)
                Image(systemName: "gamecontroller.fill")
                    .font(.system(size: 48))
                    .foregroundColor(.accentColor)
                    .padding(.top, 16)

                Text("Register Console")
                    .font(.system(size: 32, weight: .bold))

                // Host field
                VStack(alignment: .leading, spacing: 4) {
                    Text("Host").font(.caption).foregroundColor(.secondary)
                    TextField("Host (IP or broadcast)", text: $host)
                        .textFieldStyle(.roundedBorder)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                }

                // Broadcast toggle
                Toggle("Broadcast", isOn: $broadcast)

                // Console version (matches Android's ps4VersionRadioGroup)
                VStack(alignment: .leading, spacing: 8) {
                    ForEach(ConsoleVersion.allCases) { version in
                        Button {
                            consoleVersion = version
                        } label: {
                            HStack {
                                Image(systemName: consoleVersion == version ? "circle.inset.filled" : "circle")
                                    .foregroundColor(.accentColor)
                                Text(version.rawValue)
                                    .foregroundColor(.primary)
                            }
                        }
                    }
                }

                // PSN Account ID help & auto-fill
                if consoleVersion != .ps4LT7 {
                    VStack(spacing: 8) {
                        Button {
                            showingLoginSheet = true
                        } label: {
                            HStack {
                                Image(systemName: "person.crop.circle.badge.checkmark")
                                Text(psnId.isEmpty ? "Sign in to PlayStation to Auto-Fill" : "Signed in to PlayStation (Auto-Filled)")
                            }
                            .font(.subheadline)
                            .fontWeight(.medium)
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 8)
                        }
                        .buttonStyle(.bordered)
                        .tint(Color(red: 0.96, green: 0.62, blue: 0.04))

                        Button {
                            showingManualNpssoSheet = true
                        } label: {
                            HStack {
                                Image(systemName: "safari")
                                Text("Trouble signing in? Use 3-Step Browser Sign-In")
                            }
                            .font(.caption)
                        }
                        .foregroundColor(.accentColor)

                        VStack(spacing: 2) {
                            Text("Or enter your 8-byte Account ID manually")
                                .font(.caption)
                                .foregroundColor(.secondary)
                            Link("About obtaining your Account ID",
                                 destination: URL(string: "https://github.com/nileshchakraborty/AstraPlay#obtaining-your-psn-accountid")!)
                                .font(.caption2)
                        }
                    }
                }

                // PSN ID field
                VStack(alignment: .leading, spacing: 4) {
                    Text(consoleVersion == .ps4LT7
                         ? "Online ID (username, case-sensitive)"
                         : "Account ID (8 bytes, base64)")
                        .font(.caption).foregroundColor(.secondary)
                    TextField(consoleVersion == .ps4LT7 ? "Online ID" : "Account ID (base64)", text: $psnId)
                        .textFieldStyle(.roundedBorder)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                }

                // PIN instructions (matches Android's pinHelp views)
                VStack(spacing: 4) {
                    Text("On your console, navigate to")
                        .font(.callout)
                        .foregroundColor(.secondary)
                    Text(consoleVersion.isPS5
                         ? "Settings → System → Remote Play → Link Device"
                         : "Settings → Remote Play Connection Settings → Add Device")
                        .font(.callout)
                        .fontWeight(.bold)
                    Text("to obtain the PIN")
                        .font(.callout)
                        .foregroundColor(.secondary)
                }

                // PIN field
                VStack(alignment: .leading, spacing: 4) {
                    Text("PIN").font(.caption).foregroundColor(.secondary)
                    TextField("8-digit PIN", text: $pin)
                        .textFieldStyle(.roundedBorder)
                        .keyboardType(.numberPad)
                }

                // Register button
                if !isExecuting {
                    Button("Register") {
                        startRegistration()
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(!isFormValid)
                    .frame(maxWidth: .infinity)
                    .padding(.top, 16)
                }

                // Execution state (matches Android's RegistExecuteActivity)
                if isExecuting {
                    ProgressView()
                        .padding()
                }

                if !logText.isEmpty {
                    GroupBox("Log") {
                        ScrollView {
                            Text(logText)
                                .font(.system(size: 11, design: .monospaced))
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                        .frame(maxHeight: 200)
                    }
                }

                if let msg = resultMessage {
                    Text(msg)
                        .font(.callout)
                        .foregroundColor(registSuccess ? .green : .red)
                        .fontWeight(.bold)
                }

                if registSuccess {
                    Button("Done") { dismiss() }
                        .buttonStyle(.borderedProminent)
                }
            }
            .padding()
        }
        .navigationTitle("Register Console")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") {
                    registService?.stop()
                    dismiss()
                }
            }
        }
        .onAppear {
            let savedId = SecureStore.shared.accountId
            if !savedId.isEmpty && psnId.isEmpty {
                psnId = savedId
            }
        }
        .sheet(isPresented: $showingLoginSheet) {
            if let url = AstraPlayLoginService.shared.buildOAuthURL() {
                LoginWebViewContainer(url: url) { npsso in
                    showingLoginSheet = false
                    Task.detached {
                        let success = PsnTokenManager.shared.exchangeNpssoForTokens(npsso)
                        await MainActor.run {
                            if success {
                                let savedId = SecureStore.shared.accountId
                                if !savedId.isEmpty {
                                    psnId = savedId
                                }
                            } else {
                                resultMessage = "PSN login error: Failed to retrieve account ID"
                            }
                        }
                    }
                }
            }
        }
        .sheet(isPresented: $showingManualNpssoSheet) {
            manualNpssoSheet
        }
    }

    private var manualNpssoSheet: some View {
        NavigationStack {
            Form {
                Section {
                    Text("Follow these 3 quick steps in your web browser:")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                } header: {
                    Text("How to Connect")
                }

                Section {
                    Text("Tap below to log into your PlayStation account in Safari. Once signed in, switch back to this app.")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                    Button {
                        if let url = URL(string: "https://ca.account.sony.com/api/authz/v3/oauth/authorize?client_id=ba495a24-818c-472b-b12d-ff231c1b5745&redirect_uri=https://remoteplay.dl.playstation.net/remoteplay/redirect&response_type=code&scope=psn:clientapp") {
                            UIApplication.shared.open(url)
                        } else if let fallback = URL(string: "https://my.playstation.com") {
                            UIApplication.shared.open(fallback)
                        }
                    } label: {
                        HStack {
                            Image(systemName: "person.crop.circle.badge.checkmark")
                            Text("1. Open PlayStation Sign-In")
                                .fontWeight(.medium)
                        }
                    }
                } header: {
                    Text("Step 1: Sign in to PlayStation")
                }

                Section {
                    Text("After signing in, tap below to view your account code. Safari will show a page with: {\"npsso\":\"...\"}. Copy that code (or select and copy everything on the page)!")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                    Button {
                        if let url = URL(string: "https://ca.account.sony.com/api/v1/ssocookie") {
                            UIApplication.shared.open(url)
                        }
                    } label: {
                        HStack {
                            Image(systemName: "key.fill")
                            Text("2. Get My Code")
                                .fontWeight(.medium)
                        }
                    }
                } header: {
                    Text("Step 2: Get Your Code")
                }

                Section {
                    Text("Paste what you copied into the box below. We will automatically find and verify your code.")
                        .font(.footnote)
                        .foregroundColor(.secondary)

                    TextField("Paste code or {\"npsso\":\"...\"} here", text: $manualNpssoInput, axis: .vertical)
                        .lineLimit(2...4)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()

                    Button {
                        if let clip = UIPasteboard.general.string {
                            manualNpssoInput = clip
                        }
                    } label: {
                        HStack {
                            Image(systemName: "doc.on.clipboard")
                            Text("Paste from Clipboard")
                                .font(.footnote)
                        }
                    }

                    if let err = manualNpssoError {
                        Text(err)
                            .font(.caption)
                            .foregroundColor(.red)
                    }

                    Button {
                        submitManualNpsso()
                    } label: {
                        HStack {
                            Spacer()
                            Image(systemName: "checkmark.circle.fill")
                            Text("Connect & Auto-Fill Account ID")
                                .fontWeight(.semibold)
                            Spacer()
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(manualNpssoInput.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                } header: {
                    Text("Step 3: Paste Your Code")
                } footer: {
                    let cleaned = sanitizeNpsso(manualNpssoInput)
                    if !cleaned.isEmpty {
                        Text(cleaned.count == 64 ? "✓ Valid 64-character code detected!" : "Code detected: \(cleaned.count)/64 characters")
                            .font(.caption)
                            .foregroundColor(cleaned.count == 64 ? .green : .orange)
                    }
                }
            }
            .navigationTitle("Connect PlayStation Account")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        showingManualNpssoSheet = false
                    }
                }
            }
        }
    }

    private func sanitizeNpsso(_ raw: String) -> String {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if let range = trimmed.range(of: #"[a-zA-Z0-9_-]{64}"#, options: .regularExpression) {
            return String(trimmed[range])
        }
        if let range = trimmed.range(of: #""npsso"\s*:\s*"([^"]+)""#, options: .regularExpression) {
            let match = String(trimmed[range])
            let parts = match.components(separatedBy: "\"")
            if parts.count >= 4 {
                return parts[3]
            }
        }
        return trimmed.trimmingCharacters(in: CharacterSet(charactersIn: " \"'{}"))
    }

    private func submitManualNpsso() {
        let token = sanitizeNpsso(manualNpssoInput)
        guard !token.isEmpty else {
            manualNpssoError = "Please enter or paste your code."
            return
        }
        guard token.count == 64 else {
            manualNpssoError = "We found \(token.count) characters, but PlayStation codes must be 64 characters. Make sure you completed Step 1 (Sign-In) and copied the code from Step 2."
            return
        }
        manualNpssoError = nil
        showingManualNpssoSheet = false
        Task.detached {
            let success = PsnTokenManager.shared.exchangeNpssoForTokens(token)
            await MainActor.run {
                if success {
                    let savedId = SecureStore.shared.accountId
                    if !savedId.isEmpty {
                        psnId = savedId
                    }
                } else {
                    resultMessage = "PSN login error: Failed to retrieve account ID"
                }
            }
        }
    }

    private var isFormValid: Bool {
        !host.isEmpty && pin.count == 8 && !psnId.isEmpty
    }

    private func startRegistration() {
        isExecuting = true
        logText = ""
        resultMessage = nil

        let info = AstraPlayRegistInfo()
        info.target = consoleVersion.registTarget
        info.host = host
        info.broadcast = broadcast
        info.pin = UInt32(pin) ?? 0

        if consoleVersion == .ps4LT7 {
            info.psnOnlineId = psnId
        } else {
            if let decoded = Data(base64Encoded: psnId), decoded.count == 8 {
                info.psnAccountId = decoded
            }
        }

        registService = AstraPlayRegistService(info: info) { [self] result, hostData, log in
            isExecuting = false
            logText = log ?? ""

            switch result {
            case .success:
                registSuccess = true
                resultMessage = "Registration successful!"
                if let hd = hostData {
                    let registered = RegisteredHost(
                        target: Int(hd.target),
                        apSsid: hd.apSsid,
                        apBssid: hd.apBssid,
                        apKey: hd.apKey,
                        apName: hd.apName,
                        serverMac: hd.serverMac,
                        serverNickname: hd.serverNickname,
                        rpRegistKey: hd.rpRegistKey,
                        rpKeyType: Int(hd.rpKeyType),
                        rpKey: hd.rpKey
                    )
                    hostStore.addRegisteredHost(registered)
                }
            case .failed:
                resultMessage = "Registration failed."
            case .canceled:
                resultMessage = "Registration canceled."
            @unknown default:
                break
            }
        }
    }
}
