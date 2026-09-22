package com.heatcontrol.app.data

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.nio.charset.StandardCharsets
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** Keystore-backed AES/GCM wrapper for controller credentials stored in Room. */
class CredentialCipher(context: Context) {
    init { require(context.applicationContext != null) }
    private val alias = "heatcontrol_device_credentials_v1"
    private val transformation = "AES/GCM/NoPadding"

    private fun key(): SecretKey {
        val ks = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (ks.getKey(alias, null) as? SecretKey)?.let { return it }
        val kg = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        kg.init(KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .build())
        return kg.generateKey()
    }

    fun encrypt(value: String): String {
        if (value.startsWith("enc:v1:")) return value
        val cipher = Cipher.getInstance(transformation)
        cipher.init(Cipher.ENCRYPT_MODE, key())
        val iv = Base64.encodeToString(cipher.iv, Base64.NO_WRAP)
        val data = Base64.encodeToString(cipher.doFinal(value.toByteArray(StandardCharsets.UTF_8)), Base64.NO_WRAP)
        return "enc:v1:$iv:$data"
    }

    fun decrypt(value: String): String {
        if (!value.startsWith("enc:v1:")) return value
        return runCatching {
            val parts = value.split(':', limit = 4)
            require(parts.size == 4)
            val cipher = Cipher.getInstance(transformation)
            cipher.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(128, Base64.decode(parts[2], Base64.DEFAULT)))
            String(cipher.doFinal(Base64.decode(parts[3], Base64.DEFAULT)), StandardCharsets.UTF_8)
        }.getOrElse { throw IllegalStateException("Stored controller credential cannot be decrypted", it) }
    }
}
