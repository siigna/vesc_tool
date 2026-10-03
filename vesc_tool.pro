#-------------------------------------------------
#
# Project created by QtCreator 2016-08-12T21:55:19
#
#-------------------------------------------------
#
# The application target. Its sources, resources and dependencies live in
# app.pri, which the test binaries under tests/ include as well.

include($$PWD/app.pri)

SOURCES += $$PWD/main.cpp

ios | macx: {
    TARGET = "ESCargot Tool"
    CONFIG += sdk_no_version_check
}else: {
    android:{
        TARGET = "vesc_tool"
    }else:{

        TARGET = vesc_tool_$$VT_VERSION
    }
}

# The three blocks that used to stand here picked a per-ABI version code,
# and two of them were not guarded by `android:` at all, so they ran on every
# platform. They are gone with the per-ABI codes themselves: this builds one
# universal APK carrying every ABI, so there is one code, derived in app.pri.

android: {
    manifest.input = $$PWD/android/AndroidManifest.xml.in
    manifest.output = $$PWD/android/AndroidManifest.xml
    QMAKE_SUBSTITUTES += manifest
}

TEMPLATE = app

release_win {
    DESTDIR = build/win
    OBJECTS_DIR = build/win/obj
    MOC_DIR = build/win/obj
    RCC_DIR = build/win/obj
    UI_DIR = build/win/obj
}

release_lin {
    # http://micro.nicholaswilson.me.uk/post/31855915892/rules-of-static-linking-libstdc-libc-libgcc
    # http://insanecoding.blogspot.se/2012/07/creating-portable-linux-binaries.html
    QMAKE_LFLAGS += -static-libstdc++ -static-libgcc
    DESTDIR = build/lin
    OBJECTS_DIR = build/lin/obj
    MOC_DIR = build/lin/obj
    RCC_DIR = build/lin/obj
    UI_DIR = build/lin/obj
}

release_macos {
    # brew install qt
    DESTDIR = build/macos
    OBJECTS_DIR = build/macos/obj
    MOC_DIR = build/macos/obj
    RCC_DIR = build/macos/obj
    UI_DIR = build/macos/obj
}

release_android {
    DESTDIR = build/android

    # Intermediates are per-ABI, which the shared build/android/obj they used
    # to go in was not.
    #
    # A multi-ABI build runs one sub-make per architecture, and they run at the
    # same time. res_lisp.qrc is big enough that qmake builds it with rcc's
    # two-pass mode, writing qrc_res_lisp.tmp.o in pass 1 and reading it back
    # in pass 2 -- so with one shared directory the armv7 pass read the
    # arm64 pass's half-written temporary and failed with
    #
    #     No data signature found
    #
    # which names neither the resource nor the real problem. Upstream never
    # saw it because build_android only ever passed a single ABI.
    VT_ANDROID_OBJ = build/android/obj
    !isEmpty(ANDROID_TARGET_ARCH): VT_ANDROID_OBJ = build/android/$$ANDROID_TARGET_ARCH/obj

    OBJECTS_DIR = $$VT_ANDROID_OBJ
    MOC_DIR = $$VT_ANDROID_OBJ
    RCC_DIR = $$VT_ANDROID_OBJ
    UI_DIR = $$VT_ANDROID_OBJ
}

DISTFILES += \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/src/io/github/siigna/escargot/VForegroundService.java \
    android/src/io/github/siigna/escargot/Utils.java

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

macx-clang:contains(QMAKE_HOST.arch, arm.*): {
    QMAKE_APPLE_DEVICE_ARCHS=arm64
}

macx {
    ICON        =  macos/appIcon.icns
    QMAKE_INFO_PLIST = macos/Info.plist
    DISTFILES += macos/Info.plist
    QMAKE_CFLAGS_RELEASE = $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO
    QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO
    QMAKE_OBJECTIVE_CFLAGS_RELEASE = $$QMAKE_OBJECTIVE_CFLAGS_RELEASE_WITH_DEBUGINFO
    QMAKE_LFLAGS_RELEASE = $$QMAKE_LFLAGS_RELEASE_WITH_DEBUGINFO
    QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
}

ios {
    QMAKE_INFO_PLIST = ios/Info.plist
    HEADERS += ios/src/setIosParameters.h
    SOURCES += ios/src/setIosParameters.mm
    DISTFILES += ios/Info.plist \
                 ios/*.storyboard
    QMAKE_ASSET_CATALOGS = $$PWD/ios/Images.xcassets
    QMAKE_ASSET_CATALOGS_APP_ICON = "AppIcon"

    ios_artwork.files = $$files($$PWD/ios/iTunesArtwork*.png)
    QMAKE_BUNDLE_DATA += ios_artwork
    app_launch_images.files = $$files($$PWD/ios/LaunchImage*.png)
    QMAKE_BUNDLE_DATA += app_launch_images
    app_launch_screen.files = $$files($$PWD/ios/MyLaunchScreen.storyboard)
    QMAKE_BUNDLE_DATA += app_launch_screen

    #QMAKE_IOS_DEPLOYMENT_TARGET = 11.0

    disable_warning.name = GCC_WARN_64_TO_32_BIT_CONVERSION
    disable_warning.value = NO

    QMAKE_MAC_XCODE_SETTINGS += disable_warning

    # Note for devices: 1=iPhone, 2=iPad, 1,2=Universal.
    CONFIG -= warn_on
    QMAKE_APPLE_TARGETED_DEVICE_FAMILY = 1,2
}
CONFIG -= warn_on

contains(ANDROID_TARGET_ARCH,) {
    ANDROID_ABIS = \
        armeabi-v7a
}

