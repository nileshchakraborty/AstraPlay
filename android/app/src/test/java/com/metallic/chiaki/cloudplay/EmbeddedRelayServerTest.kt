package com.metallic.chiaki.cloudplay

import org.junit.Assert.*
import org.junit.Test

class EmbeddedRelayServerTest {

    @Test
    fun `isVirtualSubnet correctly identifies virtual and container bridges`() {
        // Waydroid subnets
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("192.168.240.1"))
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("192.168.240.54"))
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("192.168.245.1"))
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("192.168.245.89"))
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("192.168.250.12"))

        // QEMU / Android emulator NAT
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("10.0.2.15"))
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("10.0.2.2"))

        // KVM / libvirt default bridge
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("192.168.122.1"))

        // Docker bridge networks
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("172.17.0.1"))
        assertTrue(EmbeddedRelayServer.isVirtualSubnet("172.18.0.2"))

        // Standard home physical Wi-Fi / LAN IPs MUST NOT be flagged as virtual
        assertFalse(EmbeddedRelayServer.isVirtualSubnet("192.168.1.150"))
        assertFalse(EmbeddedRelayServer.isVirtualSubnet("192.168.0.25"))
        assertFalse(EmbeddedRelayServer.isVirtualSubnet("192.168.2.1"))
        assertFalse(EmbeddedRelayServer.isVirtualSubnet("10.0.0.42"))
        assertFalse(EmbeddedRelayServer.isVirtualSubnet("172.20.10.5"))
    }

    @Test
    fun `isVirtualInterface correctly identifies virtual and tunnel adapters`() {
        // Waydroid
        assertTrue(EmbeddedRelayServer.isVirtualInterface("waydroid0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("waydroid-net"))

        // Docker, VM, container bridges
        assertTrue(EmbeddedRelayServer.isVirtualInterface("docker0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("virbr0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("veth1234a"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("lxcbr0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("vboxnet0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("vmnet1"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("bridge0"))

        // VPN / tunnel adapters
        assertTrue(EmbeddedRelayServer.isVirtualInterface("tun0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("tap0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("tailscale0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("wg0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("dummy0"))
        assertTrue(EmbeddedRelayServer.isVirtualInterface("p2p0"))

        // Physical Wi-Fi and Ethernet adapters MUST NOT be flagged as virtual
        assertFalse(EmbeddedRelayServer.isVirtualInterface("wlan0"))
        assertFalse(EmbeddedRelayServer.isVirtualInterface("wlan1"))
        assertFalse(EmbeddedRelayServer.isVirtualInterface("eth0"))
        assertFalse(EmbeddedRelayServer.isVirtualInterface("en0"))
        assertFalse(EmbeddedRelayServer.isVirtualInterface("wifi0"))
        assertFalse(EmbeddedRelayServer.isVirtualInterface("lan0"))
    }
}
