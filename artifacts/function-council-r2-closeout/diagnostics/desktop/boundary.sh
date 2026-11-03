#!/usr/bin/env bash
# Boundary counts for the desktop scope review. Read-only greps over the product tree.
cd /home/user/Forgeshape
C=app/src/main/cpp; J=app/src/main/java/com/forgeshape/app
echo "B1 JNI exports (JNIEXPORT ... Java_com_forgeshape_app_NativeViewport_*): $(grep -c 'Java_com_forgeshape_app_NativeViewport_' $C/forgeshape_jni.cpp)"
echo "B1b distinct export names: $(grep -o 'Java_com_forgeshape_app_NativeViewport_[A-Za-z0-9_]*' $C/forgeshape_jni.cpp | sort -u | wc -l)"
echo "B1c JNIEXPORT lines in jni.cpp: $(grep -c 'JNIEXPORT' $C/forgeshape_jni.cpp)"
echo "B1d Java_ exports outside NativeViewport in any .cpp: $(grep -ho 'Java_com_forgeshape_app_[A-Za-z]*_' $C/*.cpp | grep -vc NativeViewport_)"
echo "B2 'static native' declarations in NativeViewport.java: $(grep -c 'static native' $J/NativeViewport.java)"
echo "B2b Java files declaring a native method (regex '^\s*(public |private |protected )?(static )?native '): $(grep -lE '^\s*(public |private |protected )?(static )?native ' $J/*.java | xargs -n1 basename | tr '\n' ' ')"
echo "B1e names defined twice (preprocessor variants): $(grep -o 'Java_com_forgeshape_app_NativeViewport_[A-Za-z0-9_]*' $C/forgeshape_jni.cpp | sort | uniq -d | tr '\n' ' ')"
echo "B1f export-name set equals Java native-name set: $(diff <(grep -o 'Java_com_forgeshape_app_NativeViewport_[A-Za-z0-9_]*' $C/forgeshape_jni.cpp | sed 's/.*NativeViewport_//' | sort -u) <(grep -oE 'static native [A-Za-z0-9_<>]+(\[\])? [A-Za-z0-9_]+\(' $J/NativeViewport.java | sed -E 's/.* ([A-Za-z0-9_]+)\($/\1/' | sort -u) >/dev/null && echo yes || echo NO)"
echo "B3 JNI upcalls in jni.cpp (FindClass|GetMethodID|GetStaticMethodID|Call*Method|RegisterNatives|AttachCurrentThread): $(grep -cE 'FindClass|GetMethodID|GetStaticMethodID|Call[A-Za-z]*Method|RegisterNatives|AttachCurrentThread' $C/forgeshape_jni.cpp)"
echo "B4 files under cpp/ (forgeshape_*) including an <android/...> header:"; grep -l '#include <android/' $C/forgeshape_* | xargs -n1 basename
echo "B4b #include <jni.h> files:"; grep -l '#include <jni.h>' $C/forgeshape_* | xargs -n1 basename
echo "B5 ANativeWindow occurrences per file:"; grep -c 'ANativeWindow' $C/forgeshape_*.{h,cpp} | grep -v ':0$'
echo "B5b ANativeWindow_* call sites (renderer.cpp):"; grep -n 'ANativeWindow_[a-zA-Z]*(' $C/forgeshape_renderer.cpp
echo "B5c ANativeWindow_* call sites (jni.cpp):"; grep -n 'ANativeWindow_[a-zA-Z]*(' $C/forgeshape_jni.cpp
echo "B6 Android-only Vulkan symbols in renderer.{h,cpp}:"; grep -nE 'VK_USE_PLATFORM_ANDROID_KHR|VK_KHR_ANDROID_SURFACE|vkCreateAndroidSurfaceKHR|VkAndroidSurfaceCreateInfoKHR|VK_STRUCTURE_TYPE_ANDROID_SURFACE|AHardwareBuffer|VK_ANDROID' $C/forgeshape_renderer.h $C/forgeshape_renderer.cpp
echo "B6b all instance/device extension names requested by renderer.cpp:"; grep -noE 'VK_[A-Z0-9_]*_EXTENSION_NAME' $C/forgeshape_renderer.cpp
echo "B7 __android_log_print / FS_LOG sites: jni.cpp=$(grep -cE '__android_log_print|FS_LOG[IEW]' $C/forgeshape_jni.cpp) renderer.cpp=$(grep -cE '__android_log_print|FS_LOG[IEW]' $C/forgeshape_renderer.cpp) other=$(grep -lE '__android_log_print' $C/forgeshape_*.cpp | grep -v -e jni -e renderer | wc -l)"
echo "B8 process-scoped accessor callers in renderer.*: $(grep -cE 'constructionScene\(\)|constructionHistory\(\)|sketchSession\(\)|sculptSession\(\)|gizmoSession\(\)|freeformEditSession\(\)|meshStore\(\)|displaySettings\(\)' $C/forgeshape_renderer.cpp $C/forgeshape_renderer.h | awk -F: '{s+=$2}END{print s}')"
echo "B9 translateAction + touchEvent locations in jni.cpp:"; grep -nE 'translateAction|NativeViewport_touchEvent' $C/forgeshape_jni.cpp | head -5
echo "B10 Java file counts: total=$(ls $J/*.java | wc -l); importing android.*=$(grep -l '^import android\.' $J/*.java | wc -l); not importing android.*=$(grep -L '^import android\.' $J/*.java | wc -l)"
echo "B10b java without android imports:"; grep -L '^import android\.' $J/*.java | xargs -n1 basename | tr '\n' ' '; echo
echo "B11 lines: jni.cpp=$(wc -l < $C/forgeshape_jni.cpp) renderer=$(cat $C/forgeshape_renderer.{h,cpp} | wc -l) java_total=$(cat $J/*.java | wc -l)"
echo "B12 MSVC-sensitive grep over neutral sources (product+selftests, excl jni/renderer/third_party):"
N=$(ls $C/forgeshape_*.{h,cpp} | grep -v -e forgeshape_jni.cpp -e forgeshape_renderer)
for p in '__builtin_' '__attribute__' 'M_PI\b' '#pragma GCC' '__int128' 'typeof\(' '\bVLA' 'alloca\(' '__restrict' '\bssize_t\b' '<unistd.h>' '<pthread.h>' '<sys/' 'std::aligned_alloc' ; do
  printf '  %-22s %s\n' "$p" "$(echo "$N" | xargs grep -lE "$p" 2>/dev/null | wc -l) files"; done
echo "B12b designated initializers '{ .x =' / '{.x=' (C++20 under MSVC /std:c++17 is an error):"; echo "$N" | xargs grep -nE '\{ *\.[a-zA-Z_]+ *=' | head -5 | sed 's|.*/||'
echo "B12c 'long' as a type in code (not comments/strings/long long):"; echo "$N" | xargs grep -nwE 'long' | grep -vE '//.*\blong\b|\* .*\blong\b' | grep -vE 'long long|"[^"]*long' | sed 's|^app/src/main/cpp/||'
echo "B13 manifold include sites: $(grep -l 'manifold/' $C/forgeshape_* | xargs -n1 basename | tr '\n' ' ')"
echo "B14 manifest:"; grep -nE 'uses-feature|uses-permission|configChanges|vulkan' app/src/main/AndroidManifest.xml
