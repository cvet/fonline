# ProGuard rules for Android packaging template
# Keep SDL native methods
-keep class org.libsdl.app.** { *; }
-keep class $PACKAGE$.** { *; }
