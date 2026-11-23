package xwe.java.api;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import static java.lang.foreign.ValueLayout.*;

/** Package-private Java 22+ Panama bindings for the stable XWe C ABI. */
final class XWeNative {
    private static final Arena LIBRARY_ARENA = Arena.ofShared();
    private static final Linker LINKER = Linker.nativeLinker();
    private static final SymbolLookup SYMBOLS;

    static final StructLayout APPLICATION_DESC = MemoryLayout.structLayout(
        JAVA_INT.withName("struct_size"),
        MemoryLayout.paddingLayout(4),
        ADDRESS.withName("window_title"),
        JAVA_INT.withName("window_width"),
        JAVA_INT.withName("window_height"),
        JAVA_INT.withName("enable_debug_layer")
    ).withByteAlignment(8);

    static final MethodHandle CREATE;
    static final MethodHandle RUN;
    static final MethodHandle DESTROY;
    static final MethodHandle VERSION;

    static {
        try {
            if (!System.getProperty("os.name", "").startsWith("Windows")
                    || ADDRESS.byteSize() != 8 || APPLICATION_DESC.byteSize() != 32) {
                throw new XWeException("XWe Panama bindings require the Windows x64 ABI.");
            }
            String configured = System.getProperty("xwe.library");
            Path library = configured == null || configured.isBlank()
                ? Path.of("xwe_engine.dll").toAbsolutePath()
                : Path.of(configured).toAbsolutePath();
            SYMBOLS = SymbolLookup.libraryLookup(library, LIBRARY_ARENA);
            CREATE = downcall("XWeCreateApplication",
                FunctionDescriptor.of(ADDRESS, ADDRESS));
            RUN = downcall("XWeRunApplication",
                FunctionDescriptor.of(JAVA_INT, ADDRESS));
            DESTROY = downcall("XWeDestroyApplication",
                FunctionDescriptor.ofVoid(ADDRESS));
            VERSION = downcall("XWeGetVersion",
                FunctionDescriptor.of(ADDRESS));
        } catch (Throwable error) {
            throw new ExceptionInInitializerError(new XWeException(
                "Unable to load XWe native library. Set -Dxwe.library=<absolute path to xwe_engine.dll>.", error));
        }
    }

    private XWeNative() {}

    private static MethodHandle downcall(String name, FunctionDescriptor descriptor) {
        MemorySegment symbol = SYMBOLS.find(name)
            .orElseThrow(() -> new XWeException("Missing native XWe export: " + name));
        return LINKER.downcallHandle(symbol, descriptor);
    }

    static MemorySegment create(XWeApplicationDesc desc, Arena arena) {
        MemorySegment title = arena.allocateArray(JAVA_CHAR,
            (desc.title() + "\0").toCharArray());
        MemorySegment nativeDesc = arena.allocate(APPLICATION_DESC);
        nativeDesc.set(JAVA_INT, 0, (int) APPLICATION_DESC.byteSize());
        nativeDesc.set(ADDRESS, 8, title);
        nativeDesc.set(JAVA_INT, 16, desc.width());
        nativeDesc.set(JAVA_INT, 20, desc.height());
        nativeDesc.set(JAVA_INT, 24, desc.enableDebugLayer() ? 1 : 0);
        try {
            MemorySegment handle = (MemorySegment) CREATE.invokeExact(nativeDesc);
            if (handle.equals(MemorySegment.NULL))
                throw new XWeException("XWeCreateApplication returned a null handle.");
            return handle;
        } catch (XWeException error) {
            throw error;
        } catch (Throwable error) {
            throw new XWeException("XWeCreateApplication downcall failed.", error);
        }
    }

    static int run(MemorySegment handle) {
        try {
            return (int) RUN.invokeExact(handle);
        } catch (Throwable error) {
            throw new XWeException("XWeRunApplication downcall failed.", error);
        }
    }

    static void destroy(MemorySegment handle) {
        try {
            DESTROY.invokeExact(handle);
        } catch (Throwable error) {
            throw new XWeException("XWeDestroyApplication downcall failed.", error);
        }
    }

    static String version() {
        try {
            MemorySegment pointer = (MemorySegment) VERSION.invokeExact();
            if (pointer.equals(MemorySegment.NULL)) return "unknown";
            return pointer.reinterpret(4096).getString(0, StandardCharsets.UTF_16LE);
        } catch (Throwable error) {
            throw new XWeException("XWeGetVersion downcall failed.", error);
        }
    }
}
