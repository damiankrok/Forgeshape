package com.forgeshape.app;
/** CR-05 sandbox FAKE of the product Diagnostics (which needs Android):
 *  same three static signatures (Diagnostics.java:62-70), prints the token. */
final class Diagnostics {
    private Diagnostics() {}
    static void info(String category, String token, String detail) { System.out.println("  [diag info ] " + category + " " + token + (detail == null ? "" : " " + detail)); }
    static void warn(String category, String token, String detail) { System.out.println("  [diag warn ] " + category + " " + token + (detail == null ? "" : " " + detail)); }
    static void error(String category, String token, String detail) { System.out.println("  [diag error] " + category + " " + token + (detail == null ? "" : " " + detail)); }
}
