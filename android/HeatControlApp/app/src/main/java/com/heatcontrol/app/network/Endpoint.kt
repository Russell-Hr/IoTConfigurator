package com.heatcontrol.app.network

/** Builds an HTTP URL safely for IPv4, hostnames and IPv6 literals. */
fun heatControlUrl(host: String, path: String = ""): String {
    val h = host.trim().removePrefix("http://").removePrefix("https://").trimEnd('/')
    val normalized = if (h.contains(":") && !h.startsWith("[")) "[$h]" else h
    return "http://$normalized$path"
}
