package com.forgeshape.app;
public class Probe { public static void main(String[] a){
  try { new EditorUiState(); System.out.println("EditorUiState: loads and constructs on plain JVM against android.jar classes"); } catch (Throwable t) { System.out.println("EditorUiState: " + t); }
  try { new android.os.HandlerThread("forgeshape-autosave"); System.out.println("HandlerThread constructed"); } catch (Throwable t) { System.out.println("android.os.HandlerThread (needed by AutosaveController:89): " + t); }
  try { new java.io.File("x"); android.content.Context c = null; System.out.println("android.content.Context is abstract in android.jar: " + java.lang.reflect.Modifier.isAbstract(android.content.Context.class.getModifiers())); } catch (Throwable t) { System.out.println("Context: " + t); }
}}
