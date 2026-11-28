"""Prepare the display-only Android port on a pinned CubePilot checkout."""

import argparse
import shutil
import subprocess
from pathlib import Path

REVISION = "eff16bdf347b8aba6bd7cdbb2e25a306adf96ac7"
VERSION_TAG = "v5.0.8.5-herelink"
LIBEVENTS_REVISION = "840a88ea226d4eb0fd4c391ce860317422756435"
REPOSITORY = "https://github.com/CubePilot/qgroundcontrol-herelink.git"
OVERLAY = Path(__file__).resolve().parent
ROOT = OVERLAY.parent.parent


def git(source: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(source), *args], encoding="utf-8").strip()


def original(source: Path, name: str) -> str:
    return git(source, "show", f"{REVISION}:{name}") + "\n"


def write(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def copy(source: Path, destination: Path) -> None:
    if destination.exists() and destination.read_bytes() == source.read_bytes():
        return
    shutil.copyfile(source, destination)


def prepare(source: Path) -> None:
    if source == ROOT or (
        ROOT in source.parents and ".cache" not in source.relative_to(ROOT).parts
    ):
        raise RuntimeError("Use a separate CubePilot checkout, never the Windows source tree")
    if not source.exists():
        subprocess.run(["git", "init", "--quiet", str(source)], check=True)
        git(source, "remote", "add", "origin", REPOSITORY)
        git(source, "fetch", "--depth", "1", "origin", REVISION)
        git(source, "checkout", "--detach", REVISION)
        git(source, "fetch", "--depth", "1", "origin", "tag", VERSION_TAG)
    if Path(git(source, "rev-parse", "--show-toplevel")).resolve() != source:
        raise RuntimeError("The Android source must be the root of a dedicated checkout")
    if git(source, "rev-parse", "HEAD") != REVISION:
        raise RuntimeError(
            f"Expected CubePilot revision {REVISION}; refusing to modify another version"
        )

    target = source / "custom/fatum"
    target.mkdir(parents=True, exist_ok=True)
    for name in ("OsdPanel.qml", "OsdStick.qml", "CameraTiltIndicator.qml", "AttitudeArrow3D.qml"):
        copy(ROOT / "custom/qml" / name, target / name)
    for name in ("fatum.jpg", "reticle.png", "roll.svg", "pitch.svg"):
        copy(ROOT / "custom/resources" / name, target / name)
    for name in ("battery-full.svg", "bolt.svg"):
        icon = (ROOT / "resources/InstrumentValueIcons" / name).read_text(encoding="utf-8")
        write(target / name, icon.replace("<path ", '<path fill="white" '))
    copy(ROOT / "resources/Antenna_T.svg", target / "antenna.svg")

    # Keep the desktop artwork, replacing APIs introduced after Qt 6.6.
    attitude = (ROOT / "custom/qml/DroneAttitude3D.qml").read_text(encoding="utf-8")
    attitude = attitude.replace("import QtQuick3D.Helpers\n", "")
    attitude = (
        "\n".join(line for line in attitude.splitlines() if "explicitTexture" not in line) + "\n"
    )
    start = attitude.index("                Model {\n                    materials: [hornMaterial")
    end = attitude.index("\n            }\n\n            Repeater3D", start)
    attitude = (
        attitude[:start]
        + """                Model {
                    source: "#Cone"
                    z: -8
                    eulerRotation.x: -90
                    scale: Qt.vector3d(0.40, 0.42, 0.25)
                    materials: hornMaterial
                }"""
        + attitude[end:]
    )
    write(target / "DroneAttitude3D.qml", attitude)

    view = (ROOT / "custom/qml/HerelinkOsdView.qml").read_text(encoding="utf-8")
    view = view.replace("import QGroundControl\n", "").replace(
        "import QGroundControl.Controls\n", ""
    )
    view = view.replace("property var backend: herelinkOsd", "required property var backend")
    view = view.replace("component ReadoutIcon: QGCColoredImage", "component ReadoutIcon: Image")
    view = view.replace(
        'source: "qrc:/qmlimages/Signal100.svg"', 'source: "qrc:/Herelink/antenna.svg"'
    )
    view = view.replace(
        'source: "qrc:/InstrumentValueIcons/battery-full.svg"',
        'source: "qrc:/Herelink/battery-full.svg"',
    )
    view = view.replace(
        'source: "qrc:/InstrumentValueIcons/bolt.svg"', 'source: "qrc:/Herelink/bolt.svg"'
    )
    view = view.replace(
        "QGroundControl.settingsManager.appSettings.virtualJoystick.rawValue",
        "root.backend.virtualJoystick",
    )
    # The Android display has no camera-routing settings or handlers.
    start = view.index("        Accessible.description:")
    end = view.index("        checkable:", start)
    view = view[:start] + view[end:]
    start = view.index("        onPressAndHold:")
    view = view[:start] + "    }\n}\n"
    view = view.replace(
        "width: Math.max(32, 38 * root.unit); height: Math.max(16, 18 * root.unit)",
        "width: Math.max(64, 76 * root.unit); height: Math.max(40, 40 * root.unit)",
    )
    write(target / "HerelinkOsdView.qml", view)
    for name in (
        "NativeOsdModel.qml",
        "FlyViewCustomLayer.qml",
        "DisplayResources.h",
        "NativeTelemetry.h",
        "NativeTelemetry.cc",
    ):
        copy(OVERLAY / name, target / name)

    # The interceptor is owned by the engine. All upstream control methods stay intact.
    header_name = "custom/src/HerelinkCorePlugin.h"
    header = original(source, header_name).replace(
        "    // Overrides from QGCCorePlugin",
        "    QQmlApplicationEngine *createQmlApplicationEngine(QObject *parent) override;\n\n"
        "    // Overrides from QGCCorePlugin",
    )
    write(source / header_name, header)
    cpp_name = "custom/src/HerelinkCorePlugin.cc"
    cpp = original(source, cpp_name)
    cpp += """
#include "../fatum/DisplayResources.h"
#include "../fatum/NativeTelemetry.h"

QQmlApplicationEngine *HerelinkCorePlugin::createQmlApplicationEngine(QObject *parent)
{
    static const bool registered = [] {
        qmlRegisterTypesAndRevisions<NativeTelemetry>("Fatum.Display", 1);
        return true;
    }();
    Q_UNUSED(registered);
    auto *engine = QGCCorePlugin::createQmlApplicationEngine(parent);
    engine->addUrlInterceptor(new DisplayResources(engine));
    return engine;
}
"""
    write(source / cpp_name, cpp)
    cmake = original(source, "custom/CMakeLists.txt")
    cmake += """
set(CUSTOM_SOURCES ${CUSTOM_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/fatum/NativeTelemetry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fatum/NativeTelemetry.cc
    CACHE INTERNAL "" FORCE)
set(CUSTOM_INCLUDE_DIRECTORIES ${CUSTOM_INCLUDE_DIRECTORIES}
    ${CMAKE_CURRENT_SOURCE_DIR}/fatum
    CACHE INTERNAL "" FORCE)
"""
    write(source / "custom/CMakeLists.txt", cmake)
    # Current libevents main requires newer 32-bit system-ID MAVLink helpers.
    # Keep the dependency that existed when this CubePilot base was released.
    events_cmake = original(source, "src/MAVLink/LibEvents/CMakeLists.txt").replace(
        "GIT_TAG main", f"GIT_TAG {LIBEVENTS_REVISION}"
    )
    write(source / "src/MAVLink/LibEvents/CMakeLists.txt", events_cmake)
    # The Android SDK ships libpng16.pc, while its OpenGL plugin requests libpng.pc.
    # Without that alias the finder silently falls back to an archive without dependencies.
    gst_cmake = original(source, "cmake/find-modules/FindGStreamer.cmake").replace(
        '    set(ENV{PKG_CONFIG_PATH} "")\n    if(CMAKE_HOST_WIN32)',
        """    if(NOT EXISTS "${GSTREAMER_LIB_PATH}/pkgconfig/libpng.pc"
       AND EXISTS "${GSTREAMER_LIB_PATH}/pkgconfig/libpng16.pc")
        configure_file("${GSTREAMER_LIB_PATH}/pkgconfig/libpng16.pc"
                       "${GSTREAMER_LIB_PATH}/pkgconfig/libpng.pc" COPYONLY)
    endif()
    set(ENV{PKG_CONFIG_PATH} "")
    if(CMAKE_HOST_WIN32)""",
    )
    write(source / "cmake/find-modules/FindGStreamer.cmake", gst_cmake)
    # The pinned Herelink release tag is lightweight; default git describe skips it.
    version_cmake = original(source, "cmake/Git.cmake").replace(
        "describe --always --abbrev=0", "describe --always --tags --abbrev=0"
    )
    write(source / "cmake/Git.cmake", version_cmake)
    # Android may request Quit after QML startup fails, before a root window exists.
    application = original(source, "src/QGCApplication.cc").replace(
        "    if (e->type() == QEvent::Quit) {",
        "    if (e->type() == QEvent::Quit && _mainRootWindow) {",
    )
    write(source / "src/QGCApplication.cc", application)
    # Replace duplicate readouts in video view; the map retains native instruments.
    instruments_path = "src/FlightDisplay/FlyViewBottomRightRowLayout.qml"
    instruments = (
        original(source, instruments_path)
        .replace(
            "RowLayout {\n",
            "RowLayout {\n"
            "    readonly property bool videoMain: !!_mapControl\n"
            "        && _mapControl.pipState.state !== _mapControl.pipState.fullState\n\n",
            1,
        )
        .replace(
            "    TelemetryValuesBar {\n",
            "    TelemetryValuesBar {\n        visible: !parent.videoMain\n",
            1,
        )
        .replace(
            "visible:            QGroundControl.corePlugin.options.flyView.showInstrumentPanel",
            "visible:            !parent.videoMain && QGroundControl.corePlugin.options.flyView.showInstrumentPanel",
        )
    )
    write(source / instruments_path, instruments)
    # Leave space for the edge-anchored mode badge without removing native actions.
    widgets_path = "src/FlightDisplay/FlyViewWidgetLayer.qml"
    widgets = original(source, widgets_path).replace(
        "anchors.topMargin:      _toolsMargin + parentToolInsets.topEdgeLeftInset",
        "anchors.topMargin:      _toolsMargin + parentToolInsets.topEdgeLeftInset\n"
        "            + (mapControl && mapControl.pipState.state !== mapControl.pipState.fullState\n"
        "               ? parent.height * 0.09 : 0)",
    )
    write(source / widgets_path, widgets)
    # Keep the native PiP/map toggle clear of the lower-left OSD controls.
    fly_path = "src/FlightDisplay/FlyView.qml"
    fly = original(source, fly_path).replace(
        "            item1IsFullSettingsKey:",
        "            anchors.bottomMargin: _toolsMargin + (_mainWindowIsMap ? 0 : mapHolder.height * 0.30)\n"
        "            item1IsFullSettingsKey:",
        1,
    )
    write(source / fly_path, fly)
    resources = [p.name for p in target.iterdir() if p.suffix in (".qml", ".jpg", ".png", ".svg")]
    resources.sort()
    qrc = '<RCC>\n    <qresource prefix="/Herelink">\n'
    qrc += "".join(f'        <file alias="{name}">fatum/{name}</file>\n' for name in resources)
    write(source / "custom/custom.qrc", qrc + "    </qresource>\n</RCC>\n")

    # Give the APK its own identity so installation can coexist with stock QGC.
    overrides = original(source, "custom/cmake/CustomOverrides.cmake")
    overrides += """
set(QGC_APP_NAME "FatumHerelink")
set(QGC_APP_DESCRIPTION "Fatum Herelink GCS")
set(QGC_ORG_NAME "FatumHerelink")
set(QGC_ORG_DOMAIN "org.fatum.herelink")
set(QGC_ANDROID_PACKAGE_NAME "org.fatum.herelink.gcs" CACHE STRING "Android Package Name" FORCE)
"""
    write(source / "custom/cmake/CustomOverrides.cmake", overrides)
    # Qt 6.6 androiddeployqt also reads the package directly from the manifest.
    for directory in ("android", "custom/android"):
        manifest = original(source, f"{directory}/AndroidManifest.xml").replace(
            'package="org.cubepilot.qgroundcontrol_herelink"', 'package="org.fatum.herelink.gcs"'
        )
        write(source / directory / "AndroidManifest.xml", manifest)
        gradle = original(source, f"{directory}/build.gradle").replace(
            'namespace "org.cubepilot.qgroundcontrol_herelink"',
            'namespace "org.fatum.herelink.gcs"\n        applicationId "org.fatum.herelink.gcs"',
        )
        write(source / directory / "build.gradle", gradle)
    for icon in (OVERLAY / "icons").glob("drawable-*/icon.png"):
        destination = source / "custom/android/res" / icon.relative_to(OVERLAY / "icons")
        destination.parent.mkdir(parents=True, exist_ok=True)
        copy(icon, destination)
    print(f"Prepared {source} at {REVISION}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / ".cache/herelink-android-source")
    args = parser.parse_args()
    prepare(args.source.resolve())


if __name__ == "__main__":
    main()
