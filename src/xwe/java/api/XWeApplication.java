package xwe.java.api;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Owns a native XWe application. The Arena (including UTF-16 title storage)
 * remains alive until after XWeDestroyApplication because the engine retains
 * the title pointer until its Win32 window is created.
 */
public final class XWeApplication implements AutoCloseable {
    private final Arena arena;
    private final MemorySegment handle;
    private final AtomicBoolean open = new AtomicBoolean(true);
    private volatile boolean running;

    private XWeApplication(XWeApplicationDesc desc) {
        arena = Arena.ofShared();
        try {
            handle = XWeNative.create(desc, arena);
        } catch (Throwable error) {
            arena.close();
            throw error;
        }
    }

    public static XWeApplication create(XWeApplicationDesc desc) {
        if (desc == null) throw new NullPointerException("desc");
        return new XWeApplication(desc);
    }

    /** Runs the native Win32/D3D12 event loop on the calling thread. */
    public int run() {
        if (!open.get()) throw new IllegalStateException("XWe application is closed.");
        synchronized (this) {
            if (running) throw new IllegalStateException("XWe application is already running.");
            running = true;
        }
        try {
            return XWeNative.run(handle);
        } finally {
            synchronized (this) { running = false; }
        }
    }

    public boolean isOpen() { return open.get(); }

    @Override
    public void close() {
        synchronized (this) {
            if (!open.get()) return;
            if (running) throw new IllegalStateException("Cannot close XWe while run() is active.");
            if (open.compareAndSet(true, false)) {
                try {
                    XWeNative.destroy(handle);
                } finally {
                    arena.close();
                }
            }
        }
    }
}
