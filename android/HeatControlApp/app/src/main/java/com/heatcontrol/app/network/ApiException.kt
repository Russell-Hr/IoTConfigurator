package com.heatcontrol.app.network

/** Thrown for any failed call to a device's HTTP API - network error, auth failure, or a
 *  non-2xx response with a JSON {"error"/"ok":false} body. [code] is the HTTP status, or
 *  -1 for a connection-level failure (device unreachable, timeout, DNS/mDNS failure). */
class ApiException(val code: Int, message: String) : Exception(message)
