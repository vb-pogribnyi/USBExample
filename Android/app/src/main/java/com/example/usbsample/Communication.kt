package com.example.usbsample

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbManager
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.max

const val TAG = "USBSample_comm"
private const val ACTION_USB_PERMISSION = "com.example.usbsample.USB_PERMISSION"

fun ByteArray.toHexString(): String = joinToString(" ") {"%02X".format(it)}
class Communication {
    private val usbReceiver = object : BroadcastReceiver() {

        override fun onReceive(context: Context, intent: Intent) {
            if (ACTION_USB_PERMISSION == intent.action) {
                synchronized(this) {
                    val device: UsbDevice? = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)

                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        device?.apply {
                            status.value = "Device available"
                        }
                    } else {
                        Log.d(TAG, "permission denied for device $device")
                        status.value = "Permission not granted"
                    }
                }
            }
        }
    }

    companion object {
        var anotherStatus = mutableStateOf("AnotherStatus")
        var status = mutableStateOf("Status")
        var usbManager: UsbManager? = null
        var activeDevice: UsbDevice? = null
        var bulkEndpoint: UsbEndpoint? = null

        fun claimDevice() {
            if (usbManager != null) {
                if (activeDevice == null) {
                    val deviceList = usbManager?.deviceList
                    if (deviceList != null && deviceList.isEmpty()) {
                        status.value = "No devices connected"
                    } else {
                        val device = deviceList?.values?.first()
                        status.value = "Requesting permission for device $device"
                        if (device != null) {
                            activeDevice = device

                            activeDevice!!.getInterface(0).also { intf ->
                                for (i in 0 until intf.endpointCount) {
                                    val ep = intf.getEndpoint(i)
                                    if (ep.type == UsbConstants.USB_ENDPOINT_XFER_BULK &&
                                        ep.direction == UsbConstants.USB_DIR_IN) {
                                        bulkEndpoint = ep
                                        anotherStatus.value = "Using endpoint ${ep.endpointNumber} as bulk."
                                        break
                                    }
                                }
                                if (bulkEndpoint == null) {
                                    anotherStatus.value =
                                        "Did not find bulk endpoint of ${intf.endpointCount} endpoints!"
                                }
                            }
                        }
                    }
                }
            } else {
                status.value = "No USB Manager!"
            }
        }

        private val USB_PACKET_SIZE = 64

        private val MY_CUSTOM_IN_REQUEST_TYPE = 0xCC
        private val MY_CUSTOM_OUT_REQUEST_TYPE = 0x4C
        private val MY_CUSTOM_REQUEST = 0x25

        fun toggleLED() {
            if (activeDevice == null) {
                claimDevice()
            }
            if (activeDevice == null) {
                status.value = "Cant claim device"
            }
            var bytes : ByteArray = byteArrayOf(0x00, 0x01, 0x03, 0x02)
            usbManager?.openDevice(activeDevice)?.apply {
                status.value = "Sending TurnOFF..."
                controlTransfer(MY_CUSTOM_OUT_REQUEST_TYPE, MY_CUSTOM_REQUEST, 0x57, 0x58, bytes, 0, 1000)
            }
        }
        fun readBulk() {
            if (activeDevice == null) {
                claimDevice()
            }
            if (activeDevice == null) {
                status.value = "Cant claim device"
            }
            val bytesToRead = 256
            var bytes = ByteArray(bytesToRead)
            usbManager?.openDevice(activeDevice)?.apply {
                status.value = "Starting read..."
                controlTransfer(MY_CUSTOM_IN_REQUEST_TYPE, MY_CUSTOM_REQUEST, bytesToRead, 0, bytes, 0, 1000)
                val buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
                val sizeAvailable = buffer.short.toInt() and 0xFFFF
                val isOverrun = bytes[2]
                status.value = "Available $sizeAvailable bytes. Overrun status: $isOverrun"

                val transferResult = bulkTransfer(bulkEndpoint, bytes, max(sizeAvailable, bytesToRead), 100)
                anotherStatus.value = transferResult.toString()
//                status.value = bytes.toHexString()
            }
        }
    }
}