package com.dzh.d_player

import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.dzh.d_player.databinding.ActivityMainBinding
import java.io.File


class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private var player: DPlayer? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val permissions = arrayOf(android.Manifest.permission.WRITE_EXTERNAL_STORAGE,android.Manifest.permission.READ_EXTERNAL_STORAGE)
        if (ContextCompat.checkSelfPermission(this,android.Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED){
            init()
        }else{
            requestPermissions(permissions, 1001)
        }
        binding.btnPlay.setOnClickListener {
            if (isPrepared){
                player!!.start() // 调用 C++ 开始播放
            }
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 1001){
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // 权限被用户同意，可以做你要做的事情了。
                init()
            } else {
                // 权限被用户拒绝了，可以提示用户,关闭界面等等。
                Toast.makeText(this, "请给与存储权限", Toast.LENGTH_SHORT).show()
            }
            return
        }
    }

    private var isPrepared = false

    private fun init(){
        ///storage/emulated/0/Android/data/com.dzh.d_player/files/demo.mp4
         val path = getExternalFilesDir(null)!!.absolutePath + File.separator + "demo.mp4"
        ///storage/emulated/0/demo.mp4
        //val path = Environment.getExternalStorageDirectory().absolutePath + File.separator + "demo.mp4"
        Log.d("DZHTEST", "mp4路径$path")

        player = DPlayer()
        player!!.setSurfaceView(binding.surfaceView)
        player!!.setDataSource(File(path).absolutePath)
        // 准备成功的回调处    <----  C++ 子线程调用的
        player!!.setOnPreparedListener {
            runOnUiThread { // Toast.makeText(MainActivity.this, "准备成功，即将开始播放", Toast.LENGTH_SHORT).show();
                binding.tvState.setTextColor(Color.GREEN) // 绿色
                binding.tvState.text = "恭喜init初始化成功"
            }
            isPrepared = true
        }

        player!!.setOnErrorListener { errorInfo ->
            runOnUiThread { // Toast.makeText(MainActivity.this, "出错了，错误详情是:" + errorInfo, Toast.LENGTH_SHORT).show();
                binding.tvState.setTextColor(Color.RED) // 红色
                binding.tvState.text = "哎呀,错误啦，错误:$errorInfo"
            }
        }
    }

    // ActivityThread.java Handler
    override fun onResume() { // 我们的准备工作：触发
        super.onResume()
        player?.prepare()
    }

    override fun onStop() {
        super.onStop()
        player?.stop()
    }

    override fun onDestroy() {
        super.onDestroy()
        player?.release()
    }
}