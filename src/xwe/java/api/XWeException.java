package xwe.java.api;

/** Runtime failure reported by the XWe native bridge. */
public final class XWeException extends RuntimeException {
    public XWeException(String message) { super(message); }
    public XWeException(String message, Throwable cause) { super(message, cause); }
}
