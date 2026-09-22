package com.heatcontrol.app.network

import org.junit.Assert.assertEquals
import org.junit.Test

class EndpointTest {
    @Test fun ipv4HostIsBuiltCorrectly() {
        assertEquals("http://192.168.4.1/api/status", heatControlUrl("192.168.4.1", "/api/status"))
    }

    @Test fun ipv6LiteralIsBracketed() {
        assertEquals("http://[fe80::1234]/api/status", heatControlUrl("fe80::1234", "/api/status"))
    }

    @Test fun existingSchemeAndBracketsAreNormalized() {
        assertEquals("http://[fe80::1234]/api/status", heatControlUrl("http://[fe80::1234]/", "/api/status"))
    }
}
