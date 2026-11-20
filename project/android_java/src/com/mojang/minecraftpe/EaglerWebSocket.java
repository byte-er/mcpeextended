package com.mojang.minecraftpe;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.URI;
import java.security.SecureRandom;
import java.util.concurrent.ConcurrentLinkedQueue;

import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

/**
 * Minimal RFC 6455 WebSocket client for Eaglercraft servers (binary
 * messages only). Android has no built-in WebSocket client and the APK is
 * built without extra libraries, so this does the HTTP upgrade and framing
 * itself. Received messages are queued for the native game loop to poll.
 */
public class EaglerWebSocket {
    public static final int CONNECTING = 0, OPEN = 1, CLOSING = 2, CLOSED = 3;

    private final ConcurrentLinkedQueue<byte[]> inbox = new ConcurrentLinkedQueue<byte[]>();
    private final SecureRandom random = new SecureRandom();
    private volatile int state = CONNECTING;
    private volatile String error = "";
    private Socket socket;
    private OutputStream out;

    public EaglerWebSocket(final String url) {
        Thread t = new Thread(new Runnable() {
            public void run() { runConnection(url); }
        }, "EaglerWebSocket");
        t.setDaemon(true);
        t.start();
    }

    public int getState() { return state; }
    public String getError() { return error; }
    public byte[] poll() { return inbox.poll(); }

    private void fail(String msg) {
        if (error.length() == 0) error = msg;
        state = CLOSED;
        try { if (socket != null) socket.close(); } catch (IOException e) { }
    }

    private void runConnection(String url) {
        try {
            URI uri = new URI(url.trim());
            String scheme = uri.getScheme() == null ? "wss" : uri.getScheme().toLowerCase();
            boolean secure = scheme.equals("wss") || scheme.equals("https");
            String host = uri.getHost();
            if (host == null) { fail("Invalid server address"); return; }
            int port = uri.getPort() > 0 ? uri.getPort() : (secure ? 443 : 80);
            String path = uri.getRawPath();
            if (path == null || path.length() == 0) path = "/";
            if (uri.getRawQuery() != null) path += "?" + uri.getRawQuery();

            Socket s;
            if (secure) {
                Socket plain = new Socket();
                plain.connect(new InetSocketAddress(host, port), 10000);
                SSLSocket ssl = (SSLSocket) ((SSLSocketFactory) SSLSocketFactory.getDefault())
                        .createSocket(plain, host, port, true);
                ssl.startHandshake();
                s = ssl;
            } else {
                s = new Socket();
                s.connect(new InetSocketAddress(host, port), 10000);
            }
            s.setTcpNoDelay(true);
            socket = s;
            out = s.getOutputStream();
            DataInputStream in = new DataInputStream(s.getInputStream());

            byte[] keyBytes = new byte[16];
            random.nextBytes(keyBytes);
            String key = android.util.Base64.encodeToString(keyBytes, android.util.Base64.NO_WRAP);
            String hostHeader = (port == 443 || port == 80) ? host : host + ":" + port;
            String req = "GET " + path + " HTTP/1.1\r\n"
                    + "Host: " + hostHeader + "\r\n"
                    + "Upgrade: websocket\r\n"
                    + "Connection: Upgrade\r\n"
                    + "Sec-WebSocket-Key: " + key + "\r\n"
                    + "Sec-WebSocket-Version: 13\r\n"
                    + "Origin: " + (secure ? "https://" : "http://") + hostHeader + "\r\n"
                    + "User-Agent: MCPEExtended\r\n\r\n";
            synchronized (this) { out.write(req.getBytes("US-ASCII")); out.flush(); }

            String status = readLine(in);
            if (status == null || !status.contains(" 101")) {
                fail("Server refused the connection" + (status != null ? " (" + status.trim() + ")" : ""));
                return;
            }
            String line;
            while ((line = readLine(in)) != null && line.length() > 0) { }
            state = OPEN;

            ByteArrayOutputStream fragments = null;
            while (state == OPEN || state == CLOSING) {
                int b0 = in.readUnsignedByte();
                int b1 = in.readUnsignedByte();
                boolean fin = (b0 & 0x80) != 0;
                int op = b0 & 0x0F;
                long len = b1 & 0x7F;
                if (len == 126) len = in.readUnsignedShort();
                else if (len == 127) len = in.readLong();
                byte[] mask = null;
                if ((b1 & 0x80) != 0) { mask = new byte[4]; in.readFully(mask); }
                if (len > 16 * 1024 * 1024) { fail("Message too large"); return; }
                byte[] payload = new byte[(int) len];
                in.readFully(payload);
                if (mask != null) for (int i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];

                if (op == 0x8) { // close
                    if (payload.length > 2 && error.length() == 0)
                        error = new String(payload, 2, payload.length - 2, "UTF-8");
                    sendFrame(0x8, new byte[0]);
                    fail(error.length() > 0 ? error : "Connection closed");
                    return;
                } else if (op == 0x9) { // ping
                    sendFrame(0xA, payload);
                } else if (op == 0x2 || op == 0x0) {
                    if (!fin || fragments != null) {
                        if (fragments == null) fragments = new ByteArrayOutputStream();
                        fragments.write(payload);
                        if (fin) { inbox.add(fragments.toByteArray()); fragments = null; }
                    } else {
                        inbox.add(payload);
                    }
                }
                // text frames (op 1) are not used by the game protocol
            }
        } catch (Exception e) {
            fail(state == CONNECTING ? "Could not connect: " + e.getMessage() : "Connection lost");
        }
    }

    private static String readLine(InputStream in) throws IOException {
        StringBuilder sb = new StringBuilder();
        int c;
        while ((c = in.read()) != -1) {
            if (c == '\n') return sb.toString().replace("\r", "");
            sb.append((char) c);
        }
        return sb.length() > 0 ? sb.toString() : null;
    }

    private synchronized void sendFrame(int op, byte[] data) throws IOException {
        if (out == null) return;
        int len = data.length;
        ByteArrayOutputStream f = new ByteArrayOutputStream(len + 14);
        f.write(0x80 | op);
        if (len < 126) f.write(0x80 | len);
        else if (len < 65536) { f.write(0x80 | 126); f.write(len >> 8); f.write(len); }
        else { f.write(0x80 | 127); for (int i = 7; i >= 0; i--) f.write((int) (((long) len) >> (8 * i))); }
        byte[] mask = new byte[4];
        random.nextBytes(mask);
        f.write(mask);
        byte[] masked = new byte[len];
        for (int i = 0; i < len; i++) masked[i] = (byte) (data[i] ^ mask[i & 3]);
        f.write(masked);
        out.write(f.toByteArray());
        out.flush();
    }

    /** Called from the game thread; queued so network I/O never blocks rendering. */
    public void send(final byte[] data) {
        if (state != OPEN) return;
        sendQueue.add(data);
        synchronized (sendLock) {
            if (sender == null) {
                sender = new Thread(new Runnable() {
                    public void run() { senderLoop(); }
                }, "EaglerWebSocketSend");
                sender.setDaemon(true);
                sender.start();
            }
            sendLock.notify();
        }
    }

    private final ConcurrentLinkedQueue<byte[]> sendQueue = new ConcurrentLinkedQueue<byte[]>();
    private final Object sendLock = new Object();
    private Thread sender;

    private void senderLoop() {
        try {
            while (state == OPEN) {
                byte[] d;
                while ((d = sendQueue.poll()) != null) sendFrame(0x2, d);
                synchronized (sendLock) {
                    if (sendQueue.isEmpty()) sendLock.wait(1000);
                }
            }
        } catch (Exception e) {
            fail("Connection lost");
        }
    }

    public void close() {
        if (state == CLOSED) return;
        state = CLOSING;
        try { sendFrame(0x8, new byte[] { 0x03, (byte) 0xE8 }); } catch (Exception e) { }
        fail("");
    }
}
