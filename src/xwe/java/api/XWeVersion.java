package xwe.java.api;

/** Native engine version information. */
public final class XWeVersion {
    private XWeVersion() {}
    public static String get() { return XWeNative.version(); }
}
