package com.example.usbsample

import android.app.Application
import android.content.Context
import android.content.pm.ActivityInfo
import android.hardware.usb.UsbManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.LocalActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.usbsample.ui.theme.USBSampleTheme

class App : Application() {
    override fun onCreate() {
        super.onCreate()
        instance = this
    }

    companion object {
        private lateinit var instance: App
        val context: Context get() = instance.applicationContext
    }
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        Communication.usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            USBSampleTheme {
                Column {
                    Spacer(Modifier.padding(20.dp))
                    Greeting()
                }
            }
        }
    }
}


@Composable
fun Greeting(modifier: Modifier = Modifier) {
    val activity = LocalActivity.current
    var status by Communication.status
    var anotherStatus by Communication.anotherStatus
    activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
    Row(modifier=Modifier.fillMaxSize()) {
        Box(modifier = Modifier.weight(1f).fillMaxHeight()) {
            Column() {
                Button(onClick = {
                    Communication.toggleLED()
                }) { Text("Toggle LED") }
                Button(onClick = {
                    Communication.readBulk()
                }) { Text("Read Bulk") }

                Text(anotherStatus)
                Text(status)
            }
        }


        Spacer(modifier = Modifier.weight(0.4f))
    }
}


@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    USBSampleTheme {
        Greeting()
    }
}