const ASTRAPLAY_API_BASE = "/astraplay";

function generateUUID() {
  if (typeof crypto !== "undefined" && crypto.randomUUID) {
    return crypto.randomUUID();
  }
  const bytes = new Uint8Array(16);
  crypto.getRandomValues(bytes);
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  const hex = Array.from(bytes).map(b => b.toString(16).padStart(2, "0")).join("");
  return [
    hex.slice(0, 8),
    hex.slice(8, 12),
    hex.slice(12, 16),
    hex.slice(16, 20),
    hex.slice(20, 32)
  ].join("-");
}

class AstraPlayAuth {
  constructor() {
    this.currentStep = 0;
    this.qrCode = null;
    const cid = generateUUID();
    this.loginUrl = "https://ca.account.sony.com/api/authz/v3/oauth/authorize?client_id=ba495a24-818c-472b-b12d-ff231c1b5745&redirect_uri=https://remoteplay.dl.playstation.net/remoteplay/redirect&scope=psn:clientapp referenceDataService:countryConfig.read pushNotification:webSocket.desktop.connect sessionManager:remotePlaySession.system.update&response_type=code&service_entity=urn:service-entity:psn&access_type=offline&smcid=remoteplay&layout_type=popup&PlatformPrivacyWs1=minimal&no_captcha=true&cid=" + encodeURIComponent(cid);
    this.npssoPageUrl = "https://ca.account.sony.com/api/v1/ssocookie";

    this.initializeElements();
    this.bindEvents();
    this.loadQRCode();

    if (document.readyState === "loading") {
      document.addEventListener("DOMContentLoaded", () => this.delayedShowInstructions());
    } else {
      this.delayedShowInstructions();
    }
  }

  initializeElements() {
    this.instructionsOverlay = document.getElementById("instructionsOverlay");
    this.startSetupBtn = document.getElementById("startSetupBtn");
    this.authInterface = document.getElementById("authInterface");
    this.progressFill = document.getElementById("progressFill");

    this.step1 = document.getElementById("step1");
    this.step2 = document.getElementById("step2");
    this.step3 = document.getElementById("step3");
    this.step4 = document.getElementById("step4");
    this.step5 = document.getElementById("step5");

    this.loginBtn = document.getElementById("loginBtn");
    this.npssoPageBtn = document.getElementById("npssoPageBtn");
    this.npssoToken = document.getElementById("npssoToken");
    this.pasteClipboardBtn = document.getElementById("pasteClipboardBtn");
    this.npssoValidationStatus = document.getElementById("npssoValidationStatus");

    this.backToLoginBtn = document.getElementById("backToLoginBtn");
    this.continueToTokenBtn = document.getElementById("continueToTokenBtn");
    this.backToNpssoPageBtn = document.getElementById("backToNpssoPageBtn");
    this.continueToConfirmBtn = document.getElementById("continueToConfirmBtn");
    this.backToUrlBtn = document.getElementById("backToUrlBtn");
    this.confirmMatchBtn = document.getElementById("confirmMatchBtn");

    this.yourQrCode = document.getElementById("yourQrCode");
    this.continueBtn = document.getElementById("continueBtn");
    this.authDetails = document.getElementById("authDetails");
    this.warningBanner = document.getElementById("warningBanner");

    this.loginConfirmModal = document.getElementById("loginConfirmModal");
    this.closeLoginConfirmBtn = document.getElementById("closeLoginConfirmBtn");
    this.understandBtn = document.getElementById("understandBtn");

    this.troubleshootBtn = document.getElementById("troubleshootBtn");
    this.troubleshootBtn2 = document.getElementById("troubleshootBtn2");
    this.troubleshootModal = document.getElementById("troubleshootModal");
    this.closeTroubleshootBtn = document.getElementById("closeTroubleshootBtn");
    this.copyTroubleshootUrlBtn = document.getElementById("copyTroubleshootUrlBtn");

    [this.step1, this.step2, this.step3, this.step4, this.step5].forEach(el => {
      if (el) {
        el.classList.remove("active");
        el.style.display = "none";
      }
    });
  }

  bindEvents() {
    this.startSetupBtn.addEventListener("click", () => this.startSetup());
    this.loginBtn.addEventListener("click", () => this.openLoginUrl());
    this.npssoPageBtn.addEventListener("click", () => this.openNpssoPage());
    this.npssoToken.addEventListener("input", () => this.handleNpssoInput());

    if (this.pasteClipboardBtn) {
      this.pasteClipboardBtn.addEventListener("click", async () => {
        try {
          if (navigator.clipboard && navigator.clipboard.readText) {
            const clip = await navigator.clipboard.readText();
            if (clip) {
              this.npssoToken.value = clip;
              this.handleNpssoInput();
            } else {
              this.showError("Clipboard is empty.");
            }
          } else {
            this.showError("Clipboard access not supported. Please paste into the box.");
          }
        } catch (err) {
          console.warn("Clipboard access denied:", err);
          this.showError("Please paste your code directly into the box.");
        }
      });
    }

    this.continueToTokenBtn.addEventListener("click", () => this.goToStep(3));
    this.backToLoginBtn.addEventListener("click", () => this.goToStep(1));
    this.continueToConfirmBtn.addEventListener("click", () => this.goToStep(4));
    this.backToNpssoPageBtn.addEventListener("click", () => this.goToStep(2));
    if (this.backToUrlBtn) {
      this.backToUrlBtn.addEventListener("click", () => this.goToStep(3));
    }

    this.confirmMatchBtn.addEventListener("click", () => this.handleConnect());
    this.continueBtn.addEventListener("click", () => this.handleContinue());

    if (this.closeLoginConfirmBtn) {
      this.closeLoginConfirmBtn.addEventListener("click", () => this.closeLoginConfirmModal());
    }
    if (this.understandBtn) {
      this.understandBtn.addEventListener("click", () => this.proceedWithLogin());
    }
    if (this.loginConfirmModal) {
      this.loginConfirmModal.addEventListener("click", e => {
        if (e.target === this.loginConfirmModal) this.closeLoginConfirmModal();
      });
    }

    if (this.troubleshootBtn) {
      this.troubleshootBtn.addEventListener("click", () => this.openTroubleshootModal());
    }
    if (this.troubleshootBtn2) {
      this.troubleshootBtn2.addEventListener("click", () => this.openTroubleshootModal());
    }
    if (this.closeTroubleshootBtn) {
      this.closeTroubleshootBtn.addEventListener("click", () => this.closeTroubleshootModal());
    }
    if (this.copyTroubleshootUrlBtn) {
      this.copyTroubleshootUrlBtn.addEventListener("click", () => this.copyTroubleshootUrl());
    }
    if (this.troubleshootModal) {
      this.troubleshootModal.addEventListener("click", e => {
        if (e.target === this.troubleshootModal) this.closeTroubleshootModal();
      });
    }

    document.addEventListener("keydown", e => {
      if (e.key === "Escape") {
        if (this.troubleshootModal && this.troubleshootModal.classList.contains("active")) {
          this.closeTroubleshootModal();
        }
        if (this.loginConfirmModal && this.loginConfirmModal.classList.contains("active")) {
          this.closeLoginConfirmModal();
        }
      }
    });
  }

  async loadQRCode() {
    const params = new URLSearchParams(window.location.search);
    this.qrCode = params.get("astraplay_code") || params.get("psstream_code");
    if (this.qrCode) {
      this.yourQrCode.textContent = this.qrCode;
      try {
        const res = await fetch(ASTRAPLAY_API_BASE + "/validate-code", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ code: this.qrCode })
        });
        const data = await res.json();
        if (!res.ok || data.result !== "valid") {
          this.showError("Invalid or expired code. Please generate a new QR code from your device.");
        }
      } catch (err) {
        console.error("Error validating QR code:", err);
        this.showError("Unable to validate login code. Please check your network connection.");
      }
    } else {
      this.showError("Missing astraplay_code in URL. Please scan the QR code from the AstraPlay app.");
    }
  }

  delayedShowInstructions() {
    requestAnimationFrame(() => {
      requestAnimationFrame(() => this.showInstructions());
    });
  }

  showInstructions() {
    this.instructionsOverlay.classList.add("active");
    setTimeout(() => {
      document.querySelectorAll(".fade-in-step").forEach((el, index) => {
        setTimeout(() => el.classList.add("visible"), 200 * index);
      });
    }, 300);
  }

  startSetup() {
    this.instructionsOverlay.classList.remove("active");
    setTimeout(() => {
      this.authInterface.classList.add("visible");
      this.goToStep(1);
    }, 300);
  }

  goToStep(step) {
    [this.step1, this.step2, this.step3, this.step4, this.step5].forEach(el => {
      if (el) {
        el.classList.remove("active");
        el.style.display = "none";
      }
    });

    let targetEl;
    if (step === 1) targetEl = this.step1;
    else if (step === 2) targetEl = this.step2;
    else if (step === 3) targetEl = this.step3;
    else if (step === 4) targetEl = this.step4;
    else if (step === 5) targetEl = this.step5;
    else { targetEl = this.step1; step = 1; }

    setTimeout(() => {
      if (targetEl) {
        targetEl.style.display = "block";
        targetEl.classList.add("active");
      }
    }, 100);

    this.updateWarningBanners(step);
    this.updateProgress(step === 5 ? 4 : step);
    this.currentStep = step;
  }

  updateWarningBanners(step) {
    if (this.warningBanner) {
      this.warningBanner.style.display = step === 1 ? "flex" : "none";
    }
  }

  updateProgress(step) {
    this.progressFill.style.width = (step / 4) * 100 + "%";
  }

  openLoginUrl() {
    this.openLoginConfirmModal();
  }

  openLoginConfirmModal() {
    if (this.loginConfirmModal) {
      this.loginConfirmModal.classList.add("active");
      document.body.style.overflow = "hidden";
    }
  }

  closeLoginConfirmModal() {
    if (this.loginConfirmModal) {
      this.loginConfirmModal.classList.remove("active");
      document.body.style.overflow = "";
    }
  }

  proceedWithLogin() {
    this.closeLoginConfirmModal();
    const popup = window.open(this.loginUrl, "_blank");
    this.loginBtn.innerHTML = '<span class="button-icon">✓</span> Sign-In Page Opened';
    this.loginBtn.style.background = "linear-gradient(135deg, #4CAF50, #45a049)";
    setTimeout(() => this.goToStep(2), 1500);
    if (popup) popup.focus();
  }

  openNpssoPage() {
    const popup = window.open(this.npssoPageUrl, "_blank");
    this.npssoPageBtn.innerHTML = '<span class="button-icon">✓</span> Code Page Opened';
    this.npssoPageBtn.style.background = "linear-gradient(135deg, #4CAF50, #45a049)";
    if (popup) popup.focus();
  }

  handleNpssoInput() {
    const rawVal = this.npssoToken.value.trim();
    this.validateNpssoToken(rawVal);
  }

  validateNpssoToken(input) {
    const statusIcon = this.npssoValidationStatus.querySelector(".status-icon");
    const statusText = this.npssoValidationStatus.querySelector(".status-text");

    if (!input) {
      this.npssoValidationStatus.className = "validation-status";
      statusIcon.textContent = "⏳";
      statusText.textContent = "Waiting for code...";
      this.continueToConfirmBtn.disabled = true;
      return null;
    }

    const trimmed = input.trim();
    // Regex auto-extraction for standard 64-char token (e.g. from JSON {"npsso":"<token>"})
    const tokenMatch = trimmed.match(/[a-zA-Z0-9_-]{64}/);
    if (tokenMatch) {
      const token = tokenMatch[0];
      if (this.npssoToken.value !== token) {
        this.npssoToken.value = token;
      }
      this.npssoValidationStatus.className = "validation-status valid";
      statusIcon.textContent = "✅";
      statusText.textContent = "Valid 64-character code detected!";
      this.continueToConfirmBtn.disabled = false;
      return token;
    }

    // Fallback JSON parsing
    if (trimmed.startsWith("{") && trimmed.includes("npsso")) {
      try {
        const json = JSON.parse(trimmed);
        if (json.npsso && json.npsso.length === 64) {
          this.npssoToken.value = json.npsso;
          this.npssoValidationStatus.className = "validation-status valid";
          statusIcon.textContent = "✅";
          statusText.textContent = "Valid 64-character code detected!";
          this.continueToConfirmBtn.disabled = false;
          return json.npsso;
        }
      } catch (_) {}
    }

    this.npssoValidationStatus.className = "validation-status invalid";
    statusIcon.textContent = "❌";
    statusText.textContent = `Found ${trimmed.length} characters (PlayStation codes must be 64 characters). Complete Step 1 and Step 2.`;
    this.continueToConfirmBtn.disabled = true;
    return null;
  }

  async handleConnect() {
    const rawInput = this.npssoToken.value.trim();
    const match = rawInput.match(/[a-zA-Z0-9_-]{64}/);
    const token = match ? match[0] : rawInput;

    if (this.qrCode && token) {
      this.confirmMatchBtn.innerHTML = '<span class="button-icon">⏳</span>Connecting...';
      this.confirmMatchBtn.disabled = true;

      try {
        const res = await fetch(ASTRAPLAY_API_BASE + "/connect", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ code: this.qrCode, npsso: token })
        });
        const data = await res.json();

        if (res.ok && data.result === "success") {
          this.completeAuthentication();
        } else {
          this.showError(data.error || "Failed to save authentication data");
          this.confirmMatchBtn.innerHTML = '<span class="button-icon">✓</span>Yes, Connect Account';
          this.confirmMatchBtn.disabled = false;
        }
      } catch (err) {
        console.error("Error connecting:", err);
        this.showError("Connection failed. Please check your network and try again.");
        this.confirmMatchBtn.innerHTML = '<span class="button-icon">✓</span>Yes, Connect Account';
        this.confirmMatchBtn.disabled = false;
      }
    } else {
      this.showError("Please enter your 64-character code before connecting.");
    }
  }

  completeAuthentication() {
    this.authDetails.textContent = "";
    const strong = document.createElement("strong");
    strong.textContent = "Device Code: ";
    this.authDetails.appendChild(strong);
    this.authDetails.appendChild(document.createTextNode(this.qrCode || ""));
    this.goToStep(5);
  }

  handleContinue() {
    alert('Please return to AstraPlay on your device and press "Check Status" to finish connecting.');
  }

  showError(message) {
    const el = document.createElement("div");
    el.style.cssText = `
      position: fixed;
      top: 20px;
      right: 20px;
      background: #f44336;
      color: white;
      padding: 14px 18px;
      border-radius: 8px;
      box-shadow: 0 4px 12px rgba(244, 67, 54, 0.3);
      z-index: 10000;
      animation: slideInRight 0.3s ease;
      font-size: 0.95rem;
      max-width: 90vw;
    `;
    el.textContent = message;
    document.body.appendChild(el);
    setTimeout(() => el.remove(), 5000);
  }

  openTroubleshootModal() {
    if (this.troubleshootModal) {
      this.troubleshootModal.classList.add("active");
      document.body.style.overflow = "hidden";
    }
  }

  closeTroubleshootModal() {
    if (this.troubleshootModal) {
      this.troubleshootModal.classList.remove("active");
      document.body.style.overflow = "";
    }
  }

  copyTroubleshootUrl() {
    const url = "https://ca.account.sony.com/api/v1/ssocookie";
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(url).then(() => {
        this.showCopySuccess();
      }).catch(() => {
        this.fallbackCopyUrl(url);
      });
    } else {
      this.fallbackCopyUrl(url);
    }
  }

  fallbackCopyUrl(url) {
    const el = document.createElement("textarea");
    el.value = url;
    el.style.position = "fixed";
    el.style.opacity = "0";
    document.body.appendChild(el);
    el.select();
    try {
      document.execCommand("copy");
      this.showCopySuccess();
    } catch (_) {
      this.showError("Failed to copy. Please copy manually: " + url);
    }
    document.body.removeChild(el);
  }

  showCopySuccess() {
    const orig = this.copyTroubleshootUrlBtn.innerHTML;
    this.copyTroubleshootUrlBtn.innerHTML = "✓ Copied!";
    this.copyTroubleshootUrlBtn.style.background = "#4CAF50";
    setTimeout(() => {
      this.copyTroubleshootUrlBtn.innerHTML = orig;
      this.copyTroubleshootUrlBtn.style.background = "";
    }, 2000);
  }
}

document.addEventListener("DOMContentLoaded", () => {
  new AstraPlayAuth();
});

const style = document.createElement("style");
style.textContent = `
  @keyframes slideInRight {
    from { transform: translateX(100%); opacity: 0; }
    to { transform: translateX(0); opacity: 1; }
  }
`;
document.head.appendChild(style);
