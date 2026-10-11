# AstraPlay Commercial & Paid Store Distribution Guide

This document outlines the operational and policy requirements for distributing AstraPlay as a paid app ($0.99 / nominal fee) across **Google Play**, **Apple App Store**, and **Steam**.

---

## 1. Open Source License Compliance (AGPLv3)

AstraPlay is licensed under the **GNU Affero General Public License v3 (AGPL-3.0)**. 

### Why Selling is Permitted:
The Free Software Foundation explicitly guarantees the right to charge for copies:
> *"Our General Public Licenses are designed to make sure that you have the freedom to distribute copies of free software (and charge for them if you wish)..."*

### Mandatory Compliance Obligations:
1. **Source Code Access**: 
   - A direct link to the full, unmodified source code repository must remain prominent in:
     - The in-app Settings ("About" > "Source Code (AGPLv3)")
     - In-app License Dialog (`agpl_license.txt` & `disclaimer.txt`)
     - The store listing description.
2. **Redistribution Rights**: 
   - Customers who purchase the app have full rights under AGPLv3 to build, modify, or redistribute the source code without paying further royalties.
3. **No Proprietary DRM Locks**:
   - The app must not implement proprietary paywall checks or DRM locks that obstruct the user from running the open-source code build.

---

## 2. Platform Setup Checklists

### A. Google Play Store (Android & Android TV)

1. **Merchant Account**:
   - Google Play Console requires linking a **Google Payments Merchant Profile** (Settings > Developer account > Payment settings).
   - Verify bank account, business address, and tax information (W-8BEN / W-9).
2. **App Pricing Setup**:
   - In Google Play Console, go to **Monetize with Play** > **App pricing**.
   - Set price to **$0.99 USD** (Google will automatically calculate localized prices worldwide).
   - *Note: Once an app is published as Paid, you can discount it or run sales promotions, but you cannot permanently change it to Free without creating a new package.*
3. **Privacy Policy**:
   - Under **Policy and programs** > **App content**, provide the Privacy Policy URL:
     `https://nileshchakraborty.github.io/AstraPlay/privacy`
4. **App Content Declarations**:
   - **Target age group**: 13+ (or 18+)
   - **Ads**: No ads.
   - **Data Safety**:
     - Does the app collect or share user data? **No** (all credentials stay local on device).
     - Does the app transmit data? Only direct local network and HTTPS to official Sony PSN endpoints.

---

### B. Apple App Store (iOS, iPadOS, macOS)

1. **Paid Applications Agreement**:
   - In **App Store Connect** > **Business**, accept the **Paid Applications Schedule 2 Agreement**.
   - Set up tax forms and bank account for international payouts.
2. **App Pricing**:
   - In App Store Connect > **Price and Availability**, select **Price Tier 1 ($0.99 USD)**.
3. **Privacy Nutrition Labels**:
   - "Data Not Collected" (app operates with local storage and direct peer-to-peer / official API connections).
4. **License Terms**:
   - Apple's standard EULA contains usage restrictions. The repository's AGPL-3.0 license and source code link are declared in the in-app settings and App Store metadata notes.

---

### C. Steam (Windows & Linux / Steam Deck)

1. **Steamworks Onboarding**:
   - Pay the $100 Steam Direct fee.
   - Complete digital tax and banking interviews.
2. **App Pricing**:
   - Select the $0.99 tier in the Steamworks pricing tool.
3. **Store Page**:
   - State clearly in the store page description that the software is open-source and that purchasing on Steam supports ongoing development and offers automatic Steam Deck / Steam Cloud integration.

---

## 3. Store Listing Assets Ready in Repo

- **Short description**: `android/fastlane/metadata/android/en-US/short_description.txt`
- **Full description**: `android/fastlane/metadata/android/en-US/full_description.txt`
- **Privacy Policy**: `docs/privacy.md` (published to `https://nileshchakraborty.github.io/AstraPlay/privacy`)
- **In-App About / Legal**: Configured in `android/app/src/main/res/xml/preferences.xml` and `SettingsFragment.kt`.
