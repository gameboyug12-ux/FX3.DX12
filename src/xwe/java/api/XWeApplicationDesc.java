package xwe.java.api;

import java.util.Objects;

/** Immutable startup configuration for one XWe application instance. */
public record XWeApplicationDesc(String title, int width, int height,
                                 boolean enableDebugLayer) {
    public XWeApplicationDesc {
        Objects.requireNonNull(title, "title");
        if (title.isBlank()) throw new IllegalArgumentException("title must not be blank");
        if (width <= 0 || height <= 0)
            throw new IllegalArgumentException("window dimensions must be positive");
    }

    public static XWeApplicationDesc defaults(String title) {
        return new XWeApplicationDesc(title, 1280, 720, true);
    }
}
