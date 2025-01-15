package com.gyso.player

import java.io.OutputStream
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.Executors
import java.util.concurrent.LinkedBlockingQueue

class SimpleH264TcpIpServer private constructor() {
    val queue = LinkedBlockingQueue<ByteArray>()
    companion object {
        const val PORT = 7878
        // 使用 lazy 实现线程安全单例
        val instance: SimpleH264TcpIpServer by lazy { SimpleH264TcpIpServer() }
    }

    // 服务端的 ServerSocket
    private var serverSocket: ServerSocket? = null

    // 最新连接的客户端 Socket
    @Volatile
    private var latestClientSocket: Socket? = null

    // 线程池用于处理客户端连接
    private val executorService = Executors.newCachedThreadPool()

    /**
     * 初始化网络监听
     */
    fun init() {
        if (serverSocket != null) {
            println("Server is already running on port $PORT.")
            return
        }

        try {
            serverSocket = ServerSocket(PORT)
            println("Server started on port $PORT.")

            // 在独立线程中处理客户端连接
            executorService.execute {
                while (!serverSocket!!.isClosed) {
                    println("Waiting for client connection...")
                    val clientSocket = serverSocket!!.accept()
                    println("Client connected: ${clientSocket.inetAddress.hostAddress}")

                    // 更新最新连接的客户端
                    synchronized(this) {
                        latestClientSocket?.close() // 关闭之前的客户端
                        latestClientSocket = clientSocket
                    }

                    // 可以扩展，处理客户端发过来的数据
                    executorService.execute {
                        handleClientInput(clientSocket)
                    }
                    // 可以扩展 向客户端发送数据
                    executorService.execute {
                        handleClientOutput(clientSocket)
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            stop()
        }
    }

    /**
     * 发送数据到最新的客户端
     */
    fun send(data: ByteArray) {
        latestClientSocket?.let {
            if (!it.isClosed){
                queue.offer(data)
            }
        }
    }

    /**
     * 停止服务器
     */
    fun stop() {
        try {
            synchronized(this) {
                latestClientSocket?.close()
                latestClientSocket = null
            }
            serverSocket?.close()
            serverSocket = null
            println("Server stopped.")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun handleClientOutput(clientSocket: Socket){
        try {
            if (!clientSocket.isClosed) {
                val outputStream: OutputStream = clientSocket.getOutputStream()
                while (!clientSocket.isClosed){
                    val data = queue.take()
                    outputStream.write(data)
                    outputStream.flush()
                }
                println("Data sent to client: ${clientSocket.inetAddress.hostAddress}")
            } else {
                println("No active client to send data.")
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    /**
     * 处理客户端发过来的数据（可扩展，当前仅打印连接信息）
     */
    private fun handleClientInput(clientSocket: Socket) {
        try {
            val inputStream = clientSocket.getInputStream()
            val buffer = ByteArray(1024)
            var bytesRead: Int
            while (inputStream.read(buffer).also { bytesRead = it } != -1) {
                val receivedData = buffer.copyOf(bytesRead)
                println("Received from client: ${String(receivedData)}")
            }
        } catch (e: Exception) {
            println("Client connection closed: ${clientSocket.inetAddress.hostAddress}")
        } finally {
            clientSocket.close()
        }
    }

}
